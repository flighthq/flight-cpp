#pragma once

#include <algorithm>
#include <any>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <variant>
#include <vector>

#include <flight/reference.hpp>

namespace flight {

class BadErasedValueCast final : public std::runtime_error {
 public:
  BadErasedValueCast() : std::runtime_error("flight::ErasedValue type mismatch") {}
};

class ErasedValue {
 public:
  ErasedValue() = default;

  template <typename Value>
  ErasedValue(Value value) : value_(std::move(value)) {}

  template <typename Value>
  [[nodiscard]] Value get() const {
    const auto* value = std::any_cast<Value>(&value_);
    if (!value) throw BadErasedValueCast();
    return *value;
  }

  [[nodiscard]] bool has_value() const noexcept { return value_.has_value(); }
  [[nodiscard]] std::type_index type() const noexcept { return value_.has_value() ? value_.type() : typeid(void); }

 private:
  std::any value_;
};

template <typename Key>
struct DefaultWeakKeyPolicy;

template <typename Type>
struct DefaultWeakKeyPolicy<std::shared_ptr<Type>> {
  using key_type = std::shared_ptr<Type>;
  using weak_type = std::weak_ptr<Type>;
  using identity_type = const void*;

  [[nodiscard]] static weak_type weaken(const key_type& key) noexcept { return key; }
  [[nodiscard]] static std::optional<key_type> lock(const weak_type& key) noexcept {
    auto value = key.lock();
    return value ? std::optional<key_type>(std::move(value)) : std::nullopt;
  }
  [[nodiscard]] static identity_type identity(const key_type& key) noexcept { return key.get(); }
  [[nodiscard]] static std::size_t hash(identity_type identity) noexcept {
    return std::hash<const void*>{}(identity);
  }
  [[nodiscard]] static bool equal(identity_type left, identity_type right) noexcept { return left == right; }
};

template <typename... Types>
struct DefaultWeakKeyPolicy<std::variant<std::shared_ptr<Types>...>> {
  using key_type = std::variant<std::shared_ptr<Types>...>;
  using weak_type = std::variant<std::weak_ptr<Types>...>;
  struct identity_type {
    std::size_t alternative{};
    const void* address{};
  };

  [[nodiscard]] static weak_type weaken(const key_type& key) {
    return std::visit([](const auto& value) -> weak_type { return value; }, key);
  }

  [[nodiscard]] static std::optional<key_type> lock(const weak_type& key) {
    return std::visit(
        [](const auto& value) -> std::optional<key_type> {
          auto locked = value.lock();
          return locked ? std::optional<key_type>(key_type{std::move(locked)}) : std::nullopt;
        },
        key);
  }

  [[nodiscard]] static identity_type identity(const key_type& key) noexcept {
    return std::visit(
        [&](const auto& value) { return identity_type{key.index(), value.get()}; },
        key);
  }

  [[nodiscard]] static std::size_t hash(const identity_type& identity) noexcept {
    return std::hash<const void*>{}(identity.address) ^ (identity.alternative + 0x9e3779b9U);
  }

  [[nodiscard]] static bool equal(const identity_type& left, const identity_type& right) noexcept {
    return left.alternative == right.alternative && left.address == right.address;
  }
};

template <typename Key>
using default_weak_key_policy_t = DefaultWeakKeyPolicy<Key>;

template <typename Key, typename Value, typename Policy = default_weak_key_policy_t<Key>>
class WeakMap {
 public:
  using key_type = Key;
  using value_type = Value;
  using policy_type = Policy;

  WeakMap() : entries_(std::make_shared<std::vector<Entry>>()) {}

  [[nodiscard]] std::optional<Value> get(const Key& key) const {
    const auto found = find(key);
    return found == entries_->end() ? std::nullopt : std::optional<Value>(found->value);
  }

  [[nodiscard]] bool has(const Key& key) const { return find(key) != entries_->end(); }

  WeakMap& set(const Key& key, Value value) {
    remove_expired();
    const auto identity = Policy::identity(key);
    const auto hash = Policy::hash(identity);
    const auto found = std::find_if(entries_->begin(), entries_->end(), [&](const Entry& entry) {
      return entry.hash == hash && Policy::equal(entry.identity, identity);
    });
    if (found == entries_->end()) {
      entries_->push_back(Entry{Policy::weaken(key), std::move(identity), hash, std::move(value)});
    } else {
      found->value = std::move(value);
    }
    return *this;
  }

  [[nodiscard]] bool erase(const Key& key) {
    const auto identity = Policy::identity(key);
    const auto hash = Policy::hash(identity);
    bool removed = false;
    std::erase_if(*entries_, [&](const Entry& entry) {
      const bool matches = entry.hash == hash && Policy::equal(entry.identity, identity);
      removed = removed || matches;
      return !Policy::lock(entry.key).has_value() || matches;
    });
    return removed;
  }

  [[nodiscard]] bool delete_key(const Key& key) { return erase(key); }

 private:
  struct Entry {
    typename Policy::weak_type key;
    typename Policy::identity_type identity;
    std::size_t hash;
    Value value;
  };

  using iterator = typename std::vector<Entry>::iterator;
  using const_iterator = typename std::vector<Entry>::const_iterator;

  [[nodiscard]] const_iterator find(const Key& key) const {
    remove_expired();
    const auto identity = Policy::identity(key);
    const auto hash = Policy::hash(identity);
    return std::find_if(entries_->cbegin(), entries_->cend(), [&](const Entry& entry) {
      return entry.hash == hash && Policy::equal(entry.identity, identity);
    });
  }

  void remove_expired() const {
    std::erase_if(*entries_, [&](const Entry& entry) { return !Policy::lock(entry.key).has_value(); });
  }

  std::shared_ptr<std::vector<Entry>> entries_;
};

template <typename Key, typename Value>
class WeakMapView {
 public:
  explicit WeakMapView(WeakMap<Ref<void>, ErasedValue>& source) : source_(&source) {}

  [[nodiscard]] std::optional<Value> get(const Key& key) const {
    const auto value = source_->get(std::static_pointer_cast<void>(key));
    return value ? std::optional<Value>(value->template get<Value>()) : std::nullopt;
  }

  [[nodiscard]] bool has(const Key& key) const { return source_->has(std::static_pointer_cast<void>(key)); }

  WeakMapView& set(const Key& key, Value value) {
    source_->set(std::static_pointer_cast<void>(key), ErasedValue(std::move(value)));
    return *this;
  }

  [[nodiscard]] bool erase(const Key& key) { return source_->erase(std::static_pointer_cast<void>(key)); }

 private:
  WeakMap<Ref<void>, ErasedValue>* source_;
};

template <typename Key, typename Value>
[[nodiscard]] WeakMapView<Key, Value> checked_weak_map_view(WeakMap<Ref<void>, ErasedValue>& source) {
  return WeakMapView<Key, Value>(source);
}

} // namespace flight
