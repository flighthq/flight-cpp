// Derived from @flighthq/snapshot/packages/snapshot/src/captureSnapshot.ts at
// a62784923f3be814463286c9fb28edfe6ab15789. The generated module is refused at
// Object.isFrozen and at the imported generic Snapshot<T> alias.
#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
              "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

namespace flight::snapshot {

// The source API is generic over plain JavaScript data. C++ cannot enumerate an arbitrary aggregate,
// and pretending it can would turn a shallow copy into a snapshot. This is the exact represented
// native domain instead: primitives plus insertion-ordered objects and dense arrays. Containers have
// reference identity, so cloning can preserve cycles and shared subtrees, and freezing is observable
// through every alias rather than being discarded by the backend's Object.freeze erasure.
class SnapshotValue final {
 public:
  enum class Kind { undefined, null, boolean, number, string, array, object, non_plain };

  struct Node;
  using ObjectEntry = std::pair<flight::String, SnapshotValue>;

  SnapshotValue() = default;
  SnapshotValue(flight::Undefined) noexcept {}
  SnapshotValue(flight::Null) noexcept : value_(flight::Null{}) {}
  SnapshotValue(std::nullptr_t) noexcept : value_(flight::Null{}) {}
  SnapshotValue(bool value) noexcept : value_(value) {}

  template <typename Number>
    requires(std::is_arithmetic_v<Number> && !std::same_as<std::remove_cvref_t<Number>, bool>)
  SnapshotValue(Number value) : value_(static_cast<double>(value)) {}

  SnapshotValue(flight::String value) : value_(std::move(value)) {}
  SnapshotValue(const char* value) : value_(flight::String(value)) {}

  [[nodiscard]] static SnapshotValue array(
      std::initializer_list<SnapshotValue> values = {});
  [[nodiscard]] static SnapshotValue object(
      std::initializer_list<ObjectEntry> entries = {});

  // Map, Set, Date, RegExp, ArrayBuffer and typed-array views are structured-cloneable in the
  // source but outside the package's plain-data contract. The guard names them; they remain opaque
  // leaves for strict identity comparison. An uncloneable sentinel models functions/class instances
  // and exercises structuredClone's DataCloneError boundary.
  [[nodiscard]] static SnapshotValue non_plain(flight::String kind, bool cloneable = true);

  [[nodiscard]] Kind kind() const noexcept;
  [[nodiscard]] bool is_container() const noexcept;
  [[nodiscard]] bool is_array() const noexcept { return kind() == Kind::array; }
  [[nodiscard]] bool is_object() const noexcept { return kind() == Kind::object; }
  [[nodiscard]] bool is_non_plain() const noexcept { return kind() == Kind::non_plain; }
  [[nodiscard]] bool is_frozen() const noexcept;
  [[nodiscard]] bool is_cloneable() const noexcept;

  [[nodiscard]] bool as_boolean() const;
  [[nodiscard]] double as_number() const;
  [[nodiscard]] const flight::String& as_string() const;
  [[nodiscard]] const flight::String& non_plain_kind() const;

  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] const SnapshotValue& array_at(std::size_t index) const;
  [[nodiscard]] SnapshotValue& array_at(std::size_t index);
  void array_push(SnapshotValue value);
  void array_resize(std::size_t size);

  [[nodiscard]] const SnapshotValue* object_get(const flight::String& key) const;
  [[nodiscard]] SnapshotValue* object_get(const flight::String& key);
  void object_set(flight::String key, SnapshotValue value);
  [[nodiscard]] const std::vector<ObjectEntry>& object_entries() const;

  [[nodiscard]] const void* identity() const noexcept;
  [[nodiscard]] bool same_identity(const SnapshotValue& other) const noexcept;

  // Public for the package's sibling override headers. The node is still mutated only through the
  // checked SnapshotValue operations except while the clone is being assembled privately below.
  [[nodiscard]] const std::shared_ptr<Node>& node() const noexcept { return node_value(); }

  // Internal graph construction spelling. Public only because the recursive clone helpers are
  // header-only free functions; callers should use array/object/non_plain.
  explicit SnapshotValue(std::shared_ptr<Node> node) : value_(std::move(node)) {}

 private:
  [[nodiscard]] const std::shared_ptr<Node>& node_value() const noexcept;
  [[nodiscard]] std::shared_ptr<Node>& node_value() noexcept;
  void require_kind(Kind expected) const;
  void require_mutable(Kind expected) const;

  using Storage = std::variant<flight::Undefined, flight::Null, bool, double, flight::String,
                               std::shared_ptr<Node>>;
  Storage value_;

  friend SnapshotValue clone_snapshot_graph(const SnapshotValue&, bool);
};

struct SnapshotValue::Node final {
  Kind kind{Kind::object};
  bool frozen{false};
  bool cloneable{true};
  flight::String non_plain_kind;
  std::vector<SnapshotValue> array;
  std::vector<ObjectEntry> object;
};

inline SnapshotValue SnapshotValue::array(std::initializer_list<SnapshotValue> values) {
  auto node = std::make_shared<Node>();
  node->kind = Kind::array;
  node->array.assign(values.begin(), values.end());
  return SnapshotValue(std::move(node));
}

inline SnapshotValue SnapshotValue::object(std::initializer_list<ObjectEntry> entries) {
  auto value = SnapshotValue(std::make_shared<Node>());
  value.node_value()->kind = Kind::object;
  for (const auto& [key, child] : entries) value.object_set(key, child);
  return value;
}

inline SnapshotValue SnapshotValue::non_plain(flight::String kind, bool cloneable) {
  auto node = std::make_shared<Node>();
  node->kind = Kind::non_plain;
  node->non_plain_kind = std::move(kind);
  node->cloneable = cloneable;
  return SnapshotValue(std::move(node));
}

inline SnapshotValue::Kind SnapshotValue::kind() const noexcept {
  switch (value_.index()) {
    case 0: return Kind::undefined;
    case 1: return Kind::null;
    case 2: return Kind::boolean;
    case 3: return Kind::number;
    case 4: return Kind::string;
    default: return node_value()->kind;
  }
}

inline bool SnapshotValue::is_container() const noexcept {
  return kind() == Kind::array || kind() == Kind::object;
}

inline bool SnapshotValue::is_frozen() const noexcept {
  return value_.index() != 5 || node_value()->frozen;
}

inline bool SnapshotValue::is_cloneable() const noexcept {
  return !is_non_plain() || node_value()->cloneable;
}

inline bool SnapshotValue::as_boolean() const {
  require_kind(Kind::boolean);
  return std::get<bool>(value_);
}

inline double SnapshotValue::as_number() const {
  require_kind(Kind::number);
  return std::get<double>(value_);
}

inline const flight::String& SnapshotValue::as_string() const {
  require_kind(Kind::string);
  return std::get<flight::String>(value_);
}

inline const flight::String& SnapshotValue::non_plain_kind() const {
  require_kind(Kind::non_plain);
  return node_value()->non_plain_kind;
}

inline std::size_t SnapshotValue::size() const {
  if (is_array()) return node_value()->array.size();
  if (is_object()) return node_value()->object.size();
  throw std::logic_error("snapshot value is not a container");
}

inline const SnapshotValue& SnapshotValue::array_at(std::size_t index) const {
  require_kind(Kind::array);
  return node_value()->array.at(index);
}

inline SnapshotValue& SnapshotValue::array_at(std::size_t index) {
  require_mutable(Kind::array);
  return node_value()->array.at(index);
}

inline void SnapshotValue::array_push(SnapshotValue value) {
  require_mutable(Kind::array);
  node_value()->array.push_back(std::move(value));
}

inline void SnapshotValue::array_resize(std::size_t size) {
  require_mutable(Kind::array);
  node_value()->array.resize(size);
}

inline const SnapshotValue* SnapshotValue::object_get(const flight::String& key) const {
  require_kind(Kind::object);
  const auto& entries = node_value()->object;
  const auto found = std::find_if(entries.begin(), entries.end(),
                                  [&](const ObjectEntry& entry) { return entry.first == key; });
  return found == entries.end() ? nullptr : &found->second;
}

inline SnapshotValue* SnapshotValue::object_get(const flight::String& key) {
  require_mutable(Kind::object);
  auto& entries = node_value()->object;
  const auto found = std::find_if(entries.begin(), entries.end(),
                                  [&](const ObjectEntry& entry) { return entry.first == key; });
  return found == entries.end() ? nullptr : &found->second;
}

inline void SnapshotValue::object_set(flight::String key, SnapshotValue value) {
  require_mutable(Kind::object);
  auto& entries = node_value()->object;
  const auto found = std::find_if(entries.begin(), entries.end(),
                                  [&](const ObjectEntry& entry) { return entry.first == key; });
  if (found == entries.end()) entries.emplace_back(std::move(key), std::move(value));
  else found->second = std::move(value);
}

inline const std::vector<SnapshotValue::ObjectEntry>& SnapshotValue::object_entries() const {
  require_kind(Kind::object);
  return node_value()->object;
}

inline const void* SnapshotValue::identity() const noexcept {
  return value_.index() == 5 ? node_value().get() : nullptr;
}

inline bool SnapshotValue::same_identity(const SnapshotValue& other) const noexcept {
  return identity() != nullptr && identity() == other.identity();
}

inline const std::shared_ptr<SnapshotValue::Node>& SnapshotValue::node_value() const noexcept {
  return std::get<std::shared_ptr<Node>>(value_);
}

inline std::shared_ptr<SnapshotValue::Node>& SnapshotValue::node_value() noexcept {
  return std::get<std::shared_ptr<Node>>(value_);
}

inline void SnapshotValue::require_kind(Kind expected) const {
  if (kind() != expected) throw std::logic_error("snapshot value kind mismatch");
}

inline void SnapshotValue::require_mutable(Kind expected) const {
  require_kind(expected);
  if (node_value()->frozen) throw std::logic_error("snapshot value is frozen");
}

namespace detail {

using SnapshotCloneMemo = std::unordered_map<const SnapshotValue::Node*, std::shared_ptr<SnapshotValue::Node>>;

inline SnapshotValue clone_snapshot_graph_impl(const SnapshotValue& value, SnapshotCloneMemo& memo) {
  switch (value.kind()) {
    case SnapshotValue::Kind::undefined: return SnapshotValue(flight::undefined);
    case SnapshotValue::Kind::null: return SnapshotValue(flight::null);
    case SnapshotValue::Kind::boolean: return SnapshotValue(value.as_boolean());
    case SnapshotValue::Kind::number: return SnapshotValue(value.as_number());
    case SnapshotValue::Kind::string: return SnapshotValue(value.as_string());
    case SnapshotValue::Kind::array:
    case SnapshotValue::Kind::object:
    case SnapshotValue::Kind::non_plain: break;
  }

  const auto source = value.node();
  if (!source->cloneable) {
    throw flight::DataCloneError(flight::String("snapshot source contains an uncloneable ") +
                                 source->non_plain_kind);
  }
  if (const auto found = memo.find(source.get()); found != memo.end()) {
    return SnapshotValue(found->second);
  }

  auto clone = std::make_shared<SnapshotValue::Node>();
  clone->kind = source->kind;
  clone->cloneable = source->cloneable;
  clone->non_plain_kind = source->non_plain_kind;
  memo.emplace(source.get(), clone);
  if (source->kind == SnapshotValue::Kind::array) {
    clone->array.reserve(source->array.size());
    for (const auto& child : source->array) clone->array.push_back(clone_snapshot_graph_impl(child, memo));
  } else if (source->kind == SnapshotValue::Kind::object) {
    clone->object.reserve(source->object.size());
    for (const auto& [key, child] : source->object) {
      clone->object.emplace_back(key, clone_snapshot_graph_impl(child, memo));
    }
  }
  return SnapshotValue(std::move(clone));
}

inline void freeze_snapshot_graph_impl(
    const SnapshotValue& value, std::unordered_set<const SnapshotValue::Node*>& seen) {
  if (value.identity() == nullptr || !seen.insert(value.node().get()).second) return;
  value.node()->frozen = true;
  if (value.is_array()) {
    for (const auto& child : value.node()->array) freeze_snapshot_graph_impl(child, seen);
  } else if (value.is_object()) {
    for (const auto& entry : value.node()->object) freeze_snapshot_graph_impl(entry.second, seen);
  }
}

}  // namespace detail

inline SnapshotValue clone_snapshot_graph(const SnapshotValue& value, bool freeze) {
  detail::SnapshotCloneMemo memo;
  auto clone = detail::clone_snapshot_graph_impl(value, memo);
  if (freeze) {
    std::unordered_set<const SnapshotValue::Node*> seen;
    detail::freeze_snapshot_graph_impl(clone, seen);
  }
  return clone;
}

using SnapshotCaptureGuard = std::function<void(const SnapshotValue&)>;

inline std::optional<SnapshotCaptureGuard> snapshot_capture_guard;

inline void set_snapshot_capture_guard(std::optional<SnapshotCaptureGuard> guard) {
  snapshot_capture_guard = std::move(guard);
}

inline SnapshotValue capture_snapshot(const SnapshotValue& source) {
  if (snapshot_capture_guard.has_value()) (*snapshot_capture_guard)(source);
  return clone_snapshot_graph(source, true);
}

}  // namespace flight::snapshot
