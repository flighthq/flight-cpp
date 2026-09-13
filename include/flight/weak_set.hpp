#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include <flight/weak_map.hpp>

namespace flight {

template <typename Key, typename Policy = default_weak_key_policy_t<Key>>
class WeakSet {
 public:
  using key_type = Key;
  using policy_type = Policy;

  WeakSet() : entries_(std::make_shared<std::vector<Entry>>()) {}

  WeakSet& add(const Key& key) {
    remove_expired();
    const auto identity = Policy::identity(key);
    const auto hash = Policy::hash(identity);
    const auto found = std::find_if(entries_->begin(), entries_->end(), [&](const Entry& entry) {
      return entry.hash == hash && Policy::equal(entry.identity, identity);
    });
    if (found == entries_->end()) {
      entries_->push_back(Entry{Policy::weaken(key), std::move(identity), hash});
    }
    return *this;
  }

  [[nodiscard]] bool has(const Key& key) const {
    remove_expired();
    const auto identity = Policy::identity(key);
    const auto hash = Policy::hash(identity);
    return std::find_if(entries_->cbegin(), entries_->cend(), [&](const Entry& entry) {
             return entry.hash == hash && Policy::equal(entry.identity, identity);
           }) != entries_->cend();
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
  [[nodiscard]] bool delete_(const Key& key) { return erase(key); }

 private:
  struct Entry {
    typename Policy::weak_type key;
    typename Policy::identity_type identity;
    std::size_t hash;
  };

  void remove_expired() const {
    std::erase_if(*entries_, [&](const Entry& entry) { return !Policy::lock(entry.key).has_value(); });
  }

  std::shared_ptr<std::vector<Entry>> entries_;
};

} // namespace flight
