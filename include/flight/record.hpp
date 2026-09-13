#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <flight/array.hpp>
#include <flight/string.hpp>
#include <flight/symbol.hpp>

namespace flight {

using PropertyKey = std::variant<String, double, Symbol>;

namespace detail {

template <typename>
inline constexpr bool dependent_false_record = false;

template <typename Value>
struct is_variant : std::false_type {};

template <typename... Values>
struct is_variant<std::variant<Values...>> : std::true_type {};

struct CanonicalRecordKey {
  std::variant<String, Symbol> value;
  std::optional<std::uint32_t> array_index;

  [[nodiscard]] bool is_symbol() const noexcept { return value.index() == 1; }
};

inline std::optional<std::uint32_t> record_array_index(const String& key) {
  const auto encoded = key.to_utf8();
  if (encoded.empty()) return std::nullopt;
  if (encoded == "0") return 0;
  if (encoded.front() == '0') return std::nullopt;
  std::uint64_t value = 0;
  for (const auto character : encoded) {
    if (character < '0' || character > '9') return std::nullopt;
    const auto digit = static_cast<std::uint64_t>(character - '0');
    if (value > (4294967294ULL - digit) / 10ULL) return std::nullopt;
    value = value * 10ULL + digit;
  }
  return static_cast<std::uint32_t>(value);
}

template <typename Key>
[[nodiscard]] CanonicalRecordKey canonical_record_key(const Key& key) {
  using ExactKey = std::remove_cvref_t<Key>;
  if constexpr (std::same_as<ExactKey, String>) {
    return CanonicalRecordKey{key, record_array_index(key)};
  } else if constexpr (std::same_as<ExactKey, Symbol>) {
    return CanonicalRecordKey{key, std::nullopt};
  } else if constexpr (std::is_arithmetic_v<ExactKey> && !std::same_as<ExactKey, bool>) {
    const auto string = String::from_number(static_cast<double>(key));
    return CanonicalRecordKey{string, record_array_index(string)};
  } else if constexpr (is_variant<ExactKey>::value) {
    return std::visit([](const auto& value) { return canonical_record_key(value); }, key);
  } else {
    static_assert(dependent_false_record<ExactKey>, "Flight Record key is not a PropertyKey domain");
  }
}

inline bool same_record_key(const CanonicalRecordKey& left, const CanonicalRecordKey& right) noexcept {
  if (left.value.index() != right.value.index()) return false;
  if (left.is_symbol()) return std::get<Symbol>(left.value) == std::get<Symbol>(right.value);
  return std::get<String>(left.value) == std::get<String>(right.value);
}

} // namespace detail

template <typename Key, typename Value>
class Record {
 public:
  using Entry = std::pair<Key, Value>;

 private:
  struct StoredEntry {
    Entry entry;
    detail::CanonicalRecordKey canonical_key;
  };

  struct Storage {
    std::vector<StoredEntry> entries;
  };

 public:
  class const_iterator {
   public:
    using difference_type = typename std::vector<StoredEntry>::difference_type;
    using iterator_category = std::forward_iterator_tag;
    using pointer = const Entry*;
    using reference = const Entry&;
    using value_type = Entry;

    const_iterator() = default;

    [[nodiscard]] reference operator*() const noexcept { return iterator_->entry; }
    [[nodiscard]] pointer operator->() const noexcept { return &iterator_->entry; }

    const_iterator& operator++() noexcept {
      ++iterator_;
      return *this;
    }

    const_iterator operator++(int) noexcept {
      auto previous = *this;
      ++*this;
      return previous;
    }

    [[nodiscard]] friend bool operator==(const const_iterator&, const const_iterator&) noexcept = default;

   private:
    friend class Record;
    using StoredIterator = typename std::vector<StoredEntry>::const_iterator;

    explicit const_iterator(StoredIterator iterator) noexcept : iterator_(iterator) {}

    StoredIterator iterator_{};
  };

  using size_type = typename std::vector<StoredEntry>::size_type;

  Record() : storage_(std::make_shared<Storage>()) {}

  Record(std::initializer_list<Entry> entries) : Record() {
    for (const auto& [key, value] : entries) set(key, value);
  }

  template <typename Range>
    requires requires(const Range& range) {
      std::begin(range);
      std::end(range);
    }
  explicit Record(const Range& entries) : Record() {
    for (const auto& entry : entries) set(std::get<0>(entry), std::get<1>(entry));
  }

  [[nodiscard]] const_iterator begin() const noexcept {
    return const_iterator(storage_->entries.cbegin());
  }

  [[nodiscard]] const_iterator end() const noexcept {
    return const_iterator(storage_->entries.cend());
  }

  [[nodiscard]] Record clone() const {
    Record result;
    *result.storage_ = *storage_;
    return result;
  }

  [[nodiscard]] bool empty() const noexcept { return storage_->entries.empty(); }
  [[nodiscard]] const void* identity() const noexcept { return storage_.get(); }
  [[nodiscard]] size_type size() const noexcept { return storage_->entries.size(); }

  template <typename LookupKey>
  [[nodiscard]] std::optional<Value> get(const LookupKey& key) const {
    const auto entry = find(detail::canonical_record_key(key));
    if (entry == storage_->entries.cend()) return std::nullopt;
    return entry->entry.second;
  }

  template <typename LookupKey>
  [[nodiscard]] bool has(const LookupKey& key) const {
    return find(detail::canonical_record_key(key)) != storage_->entries.cend();
  }

  template <typename LookupKey>
  bool erase(const LookupKey& key) {
    const auto entry = find(detail::canonical_record_key(key));
    if (entry == storage_->entries.end()) return false;
    storage_->entries.erase(entry);
    return true;
  }

  Record& set(Key key, Value value) {
    auto canonical = detail::canonical_record_key(key);
    const auto existing = find(canonical);
    if (existing != storage_->entries.end()) {
      existing->entry.second = std::move(value);
      return *this;
    }

    const auto position = insertion_position(canonical);
    storage_->entries.insert(position, StoredEntry{Entry(std::move(key), std::move(value)), std::move(canonical)});
    return *this;
  }

  [[nodiscard]] Array<String> enumerable_keys() const {
    Array<String> result;
    for (const auto& entry : storage_->entries) {
      if (!entry.canonical_key.is_symbol()) result.push(std::get<String>(entry.canonical_key.value));
    }
    return result;
  }

  [[nodiscard]] Array<std::tuple<String, Value>> enumerable_entries() const {
    Array<std::tuple<String, Value>> result;
    for (const auto& entry : storage_->entries) {
      if (!entry.canonical_key.is_symbol()) {
        result.push(std::tuple<String, Value>(std::get<String>(entry.canonical_key.value), entry.entry.second));
      }
    }
    return result;
  }

  [[nodiscard]] friend bool operator==(const Record& left, const Record& right) noexcept {
    return left.storage_ == right.storage_;
  }

 private:
  [[nodiscard]] auto find(const detail::CanonicalRecordKey& key) {
    return std::find_if(storage_->entries.begin(), storage_->entries.end(), [&](const StoredEntry& entry) {
      return detail::same_record_key(entry.canonical_key, key);
    });
  }

  [[nodiscard]] auto find(const detail::CanonicalRecordKey& key) const {
    return std::find_if(storage_->entries.cbegin(), storage_->entries.cend(), [&](const StoredEntry& entry) {
      return detail::same_record_key(entry.canonical_key, key);
    });
  }

  [[nodiscard]] auto insertion_position(const detail::CanonicalRecordKey& key) {
    if (key.array_index) {
      return std::find_if(storage_->entries.begin(), storage_->entries.end(), [&](const StoredEntry& entry) {
        return !entry.canonical_key.array_index || *entry.canonical_key.array_index > *key.array_index;
      });
    }
    if (!key.is_symbol()) {
      return std::find_if(storage_->entries.begin(), storage_->entries.end(), [](const StoredEntry& entry) {
        return entry.canonical_key.is_symbol();
      });
    }
    return storage_->entries.end();
  }

  std::shared_ptr<Storage> storage_;
};

} // namespace flight
