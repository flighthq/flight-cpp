#pragma once

#include <algorithm>
#include <any>
#include <cstddef>
#include <iterator>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include <flight/symbol.hpp>

// Symbol-keyed properties attached to an object, and the registry that gives one object exactly one
// set of them.
//
// This is the storage behind three views that must agree: `flight::AttachedProperties`, a
// `StructuralRef`'s computed-symbol accessors, and a `flight::Record<flight::Symbol, T>` projected
// from an erased `flight::Ref<void>`. They agree because they are the same store, resolved by
// object identity rather than by static type -- not because they are kept in sync, which would be a
// divergence waiting to happen.
//
// The registry holds each attachment strongly and its object weakly, so an attachment lives as long
// as the object rather than as long as the transient view that wrote it, and an object's death
// releases it. Expiry is checked on every lookup, so an address reused by a later object never
// inherits the earlier object's properties.
namespace flight {

class SymbolAttachment final {
 public:
  struct Entry final {
    Symbol key;
    std::any value;
  };

  [[nodiscard]] const std::any* find(const Symbol& key) const noexcept {
    const auto entry = locate(key);
    return entry == entries_.end() ? nullptr : &entry->value;
  }

  [[nodiscard]] bool contains(const Symbol& key) const noexcept { return find(key) != nullptr; }

  // Insertion order is retained, and reassigning a key does not move it.
  void assign(const Symbol& key, std::any value) {
    const auto entry = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& candidate) {
      return candidate.key.identity() == key.identity();
    });
    if (entry != entries_.end()) {
      entry->value = std::move(value);
      return;
    }
    entries_.push_back(Entry{key, std::move(value)});
  }

  bool remove(const Symbol& key) {
    const auto entry = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& candidate) {
      return candidate.key.identity() == key.identity();
    });
    if (entry == entries_.end()) return false;
    entries_.erase(entry);
    return true;
  }

  [[nodiscard]] const std::vector<Entry>& entries() const noexcept { return entries_; }
  [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
  [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }

 private:
  [[nodiscard]] std::vector<Entry>::const_iterator locate(const Symbol& key) const noexcept {
    return std::find_if(entries_.begin(), entries_.end(), [&](const Entry& candidate) {
      return candidate.key.identity() == key.identity();
    });
  }

  std::vector<Entry> entries_;
};

namespace detail {

struct AttachmentRegistryEntry final {
  std::shared_ptr<SymbolAttachment> attachment;
  std::weak_ptr<void> object;
};

struct AttachmentRegistry final {
  std::unordered_map<const void*, AttachmentRegistryEntry> entries;
  std::mutex mutex;
  std::size_t sweep_threshold{64};
};

[[nodiscard]] inline AttachmentRegistry& attachment_registry() {
  static AttachmentRegistry registry;
  return registry;
}

// Drops entries whose object has been destroyed. Expiry is already checked on every lookup, so this
// only keeps the table from growing without bound.
inline void sweep_attachment_registry(AttachmentRegistry& registry) {
  if (registry.entries.size() < registry.sweep_threshold) return;
  for (auto entry = registry.entries.begin(); entry != registry.entries.end();) {
    entry = entry->second.object.expired() ? registry.entries.erase(entry) : std::next(entry);
  }
  registry.sweep_threshold = registry.entries.size() * 2 + 64;
}

} // namespace detail

[[nodiscard]] inline std::shared_ptr<SymbolAttachment> attachment_for(
    const std::shared_ptr<void>& object) {
  if (!object) return {};
  auto& registry = detail::attachment_registry();
  const std::lock_guard lock(registry.mutex);
  const auto key = object.get();
  if (const auto found = registry.entries.find(key); found != registry.entries.end()) {
    if (!found->second.object.expired()) return found->second.attachment;
    registry.entries.erase(found);
  }
  detail::sweep_attachment_registry(registry);
  auto attachment = std::make_shared<SymbolAttachment>();
  registry.entries.emplace(key, detail::AttachmentRegistryEntry{attachment, object});
  return attachment;
}

} // namespace flight
