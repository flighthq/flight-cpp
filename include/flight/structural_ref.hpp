#pragma once

#include <any>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>

#include <flight/any.hpp>
#include <flight/attachment.hpp>
#include <flight/reference.hpp>
#include <flight/symbol.hpp>

namespace flight {

template <std::size_t Size>
struct FixedString {
  char value[Size]{};

  consteval FixedString(const char (&source)[Size]) {
    for (std::size_t index = 0; index < Size; ++index) value[index] = source[index];
  }

  [[nodiscard]] constexpr std::string_view view() const noexcept {
    return std::string_view(value, Size - 1);
  }
};

template <FixedString Name>
struct RowKey {
  static constexpr auto name = Name;
};

template <typename Type>
struct RowOf {
  using source_type = Type;
};

template <typename... Rows>
struct RowMerge {};

template <typename Row>
struct RowPartial {
  using row_type = Row;
};

// The dual of RowPartial: every member the row names is present. TypeScript's `Required<T>` is not
// the absence of `Partial<T>` but its inverse, and a row that only knows how to make members
// optional cannot express it -- which is why `Required<Pick<Host, 'subscribe' | 'unsubscribe'>>`
// had nothing to lower onto, and a caller ended up invoking an `std::optional` as if it were the
// callable inside it.
//
// The subject's storage is unchanged: a member declared optional stays an `std::optional` in the
// object. What this states is that reading it through this row yields the value rather than the
// optional, and that a row which does not in fact hold one is a contract violation the reader is
// told about rather than a silently empty optional it might call.
template <typename Row>
struct RowRequired {
  using row_type = Row;
};

template <typename Row>
struct RowReadonly {
  using row_type = Row;
};

template <typename Row>
struct RowWritable {
  using row_type = Row;
};

// A StructuralRef compares this owner identity. A write proxy owns a distinct instance while
// delegating storage to its target, matching JavaScript Proxy identity and forwarding behavior.
class RowCell {
 public:
  RowCell() = default;
  RowCell(const RowCell&) = delete;
  RowCell& operator=(const RowCell&) = delete;
  virtual ~RowCell() = default;

  [[nodiscard]] virtual std::type_index value_type() const noexcept = 0;

  // Copies another cell's value into this one when both hold the same type, and reports whether it
  // did. The materializing cast uses it to move what a construction bag held onto the member the
  // finished object answers for itself; a cell whose type does not match is left alone rather than
  // reinterpreted.
  virtual bool assign_from(RowCell& other) = 0;
};

template <typename Value>
class TypedRowCell : public RowCell {
 public:
  [[nodiscard]] std::type_index value_type() const noexcept final { return typeid(Value); }
  [[nodiscard]] virtual Value& get() = 0;
  virtual void set(Value value) = 0;

  bool assign_from(RowCell& other) final {
    auto* typed = dynamic_cast<TypedRowCell<Value>*>(&other);
    if (!typed) return false;
    set(typed->get());
    return true;
  }
};

template <typename Value>
class OwnedRowCell final : public TypedRowCell<Value> {
 public:
  explicit OwnedRowCell(Value value) : value_(std::move(value)) {}

  [[nodiscard]] Value& get() override { return value_; }
  void set(Value value) override { value_ = std::move(value); }

 private:
  Value value_;
};

template <typename Value, typename Getter>
class NativeRowCell final : public TypedRowCell<Value> {
 public:
  explicit NativeRowCell(Getter getter) : getter_(std::move(getter)) {}

  [[nodiscard]] Value& get() override { return std::invoke(getter_); }
  void set(Value value) override { std::invoke(getter_) = std::move(value); }

 private:
  Getter getter_;
};

class RowOwner {
 public:
  RowOwner() = default;
  RowOwner(const RowOwner&) = delete;
  RowOwner& operator=(const RowOwner&) = delete;
  virtual ~RowOwner() = default;

  virtual void before_named_write(std::string_view) const {}
  virtual void before_write(const Symbol&) const {}

  [[nodiscard]] virtual std::shared_ptr<void> native_object() const noexcept { return {}; }
  [[nodiscard]] virtual std::type_index native_type() const noexcept { return typeid(void); }

  template <typename Getter>
  void bind_named(std::string_view key, Getter getter) {
    using Reference = std::invoke_result_t<Getter&>;
    static_assert(std::is_lvalue_reference_v<Reference>);
    using Value = std::remove_reference_t<Reference>;
    if (named_cell(key)) return;
    set_named_cell(
        key,
        std::make_shared<NativeRowCell<Value, Getter>>(std::move(getter)));
  }

  template <typename Value>
  [[nodiscard]] Value* named_value(std::string_view key) const {
    const auto cell = named_cell(key);
    if (!cell) return nullptr;
    const auto typed = std::dynamic_pointer_cast<TypedRowCell<Value>>(cell);
    if (!typed) throw std::bad_cast();
    return &typed->get();
  }

  // Reports absence for a cell held under a different type rather than throwing, so a reader that
  // accepts more than one storage shape can ask about each in turn. `named_value` keeps throwing:
  // asking for one shape and finding another is an error there, and only a reader that has a second
  // shape to try should be tolerating it.
  template <typename Value>
  [[nodiscard]] Value* named_value_if(std::string_view key) const {
    const auto cell = named_cell(key);
    if (!cell) return nullptr;
    const auto typed = std::dynamic_pointer_cast<TypedRowCell<Value>>(cell);
    return typed ? &typed->get() : nullptr;
  }

  template <typename Value>
  void set_named_value(std::string_view key, Value value) {
    using Stored = std::remove_cvref_t<Value>;
    if (auto* target = named_value<Stored>(key)) {
      *target = std::move(value);
      return;
    }
    set_named_cell(key, std::make_shared<OwnedRowCell<Stored>>(std::move(value)));
  }

  [[nodiscard]] virtual std::shared_ptr<RowCell> named_cell(std::string_view key) const {
    const auto found = named_cells_.find(std::string(key));
    return found == named_cells_.end() ? nullptr : found->second;
  }

  virtual void set_named_cell(std::string_view key, std::shared_ptr<RowCell> cell) {
    named_cells_.insert_or_assign(std::string(key), std::move(cell));
  }

  [[nodiscard]] virtual bool has_named_cell(std::string_view key) const {
    return named_cells_.contains(std::string(key));
  }

  // Symbol-keyed properties live in the object's one attachment rather than in this owner, so a
  // computed-symbol write through a row and a `Record<Symbol, T>` view of the same object are the
  // same entry rather than two entries that happen to agree.
  [[nodiscard]] virtual const std::any* dynamic_value(const Symbol& key) const {
    return attachment_->find(key);
  }

  virtual void set_dynamic_value(const Symbol& key, std::any value) {
    attachment_->assign(key, std::move(value));
  }

  [[nodiscard]] virtual bool has_dynamic_value(const Symbol& key) const {
    return attachment_->contains(key);
  }

  [[nodiscard]] const std::shared_ptr<SymbolAttachment>& attachment() const noexcept {
    return attachment_;
  }

  // This owner's own cell storage, for the one caller that has to move a row's contents onto a
  // different object: the materializing cast below.
  [[nodiscard]] const std::unordered_map<std::string, std::shared_ptr<RowCell>>& named_cells()
      const noexcept {
    return named_cells_;
  }

  // Adopts the attachment an object identity resolves to. A row with no object keeps the private
  // one it was constructed with.
  void adopt_attachment(std::shared_ptr<SymbolAttachment> attachment) {
    if (attachment) attachment_ = std::move(attachment);
  }

 private:
  std::shared_ptr<SymbolAttachment> attachment_{std::make_shared<SymbolAttachment>()};
  std::unordered_map<std::string, std::shared_ptr<RowCell>> named_cells_;
};

namespace detail {

template <typename>
inline constexpr bool dependent_false = false;

template <typename Type>
struct unwrap_ref {
  using type = Type;
};

template <typename Type>
struct unwrap_ref<std::shared_ptr<Type>> : unwrap_ref<Type> {};

template <typename Type>
using unwrap_ref_t = typename unwrap_ref<Type>::type;

template <typename Schema>
struct schema_source;

template <typename Type>
struct schema_source<RowOf<Type>> {
  using type = Type;
};

template <typename... Rows>
struct schema_source<RowMerge<Rows...>> {
  using type = void;
};

template <typename Row>
struct schema_source<RowPartial<Row>> : schema_source<Row> {};

template <typename Row>
struct schema_source<RowRequired<Row>> : schema_source<Row> {};

template <typename Row>
struct schema_source<RowReadonly<Row>> : schema_source<Row> {};

template <typename Row>
struct schema_source<RowWritable<Row>> : schema_source<Row> {};

template <typename Schema>
using schema_object_t = unwrap_ref_t<typename schema_source<Schema>::type>;

template <typename Object>
void bind_generated_row_members(RowOwner&, const std::shared_ptr<Object>&);

template <typename Object, typename Type>
[[nodiscard]] std::shared_ptr<Object> flatten_ref(std::shared_ptr<Type> source) {
  if constexpr (std::same_as<Type, Object>) {
    return std::move(source);
  } else if constexpr (requires(Type& value) { *value; }) {
    return source ? flatten_ref<Object>(*source) : std::shared_ptr<Object>{};
  } else if constexpr (std::derived_from<Type, Object>) {
    return std::static_pointer_cast<Object>(std::move(source));
  } else {
    static_assert(dependent_false<Type>, "structural reference source has an incompatible object type");
  }
}

template <typename Target, typename Object>
[[nodiscard]] Target wrap_ref(const std::shared_ptr<Object>& source) {
  if constexpr (std::same_as<Target, std::shared_ptr<Object>>) {
    return source;
  } else if constexpr (requires { typename Target::element_type; }) {
    using Element = typename Target::element_type;
    if constexpr (std::same_as<Element, Object>) {
      return source;
    } else if constexpr (std::derived_from<Object, Element>) {
      return std::static_pointer_cast<Element>(source);
    } else if constexpr (requires { typename Element::element_type; }) {
      return std::make_shared<Element>(wrap_ref<Element>(source));
    } else {
      static_assert(dependent_false<Target>, "structural reference target has an incompatible object type");
    }
  } else {
    static_assert(dependent_false<Target>, "structural reference target must be a Flight reference");
  }
}

template <typename Field>
[[nodiscard]] Symbol row_field_symbol() {
  using Key = typename std::remove_cvref_t<Field>::key_type;
  return Symbol::for_key(String(Key::name.view()));
}

// Holds its object weakly. The attachment registry below keeps owners alive for as long as their
// object lives, so an owner that also held the object strongly would keep it alive forever. The
// reference that keeps the object alive is the caller's, and the StructuralRef that names it.
template <typename Object>
class NativeRowOwner final : public RowOwner {
 public:
  explicit NativeRowOwner(const std::shared_ptr<Object>& object) : object_(object) {}

  [[nodiscard]] std::shared_ptr<void> native_object() const noexcept override {
    return std::static_pointer_cast<void>(object_.lock());
  }

  [[nodiscard]] std::type_index native_type() const noexcept override { return typeid(Object); }

 private:
  std::weak_ptr<Object> object_;
};

// One owner per object identity, for the lifetime of the object.
//
// Both halves matter and both were wrong before. The registry is keyed on the erased address rather
// than on a per-type table, so a typed projection, a structural projection and an erased
// `Ref<void>` view of one object all reach the same owner and therefore the same attached symbol
// properties. And the registry holds the owner strongly while holding the object weakly, so an
// attachment outlives the transient view that created it and dies with the object rather than with
// the last reference to a row.
struct OwnerRegistryEntry final {
  std::weak_ptr<void> object;
  std::shared_ptr<RowOwner> owner;
};

struct OwnerRegistry final {
  std::mutex mutex;
  std::unordered_map<const void*, OwnerRegistryEntry> entries;
  std::size_t sweep_threshold{64};
};

[[nodiscard]] inline OwnerRegistry& owner_registry() {
  static OwnerRegistry registry;
  return registry;
}

// Drops entries whose object has been destroyed. An address can be reused by a later object, so a
// stale entry is never reused: expiry is checked on every lookup, and this sweep only keeps the
// table from growing without bound.
inline void sweep_owner_registry(OwnerRegistry& registry) {
  if (registry.entries.size() < registry.sweep_threshold) return;
  for (auto entry = registry.entries.begin(); entry != registry.entries.end();) {
    entry = entry->second.object.expired() ? registry.entries.erase(entry) : std::next(entry);
  }
  registry.sweep_threshold = registry.entries.size() * 2 + 64;
}

template <typename Object>
[[nodiscard]] std::shared_ptr<RowOwner> owner_for(const std::shared_ptr<Object>& object) {
  auto& registry = owner_registry();
  const std::lock_guard lock(registry.mutex);
  const auto key = static_cast<const void*>(object.get());
  if (const auto found = registry.entries.find(key); found != registry.entries.end()) {
    if (!found->second.object.expired()) return found->second.owner;
    registry.entries.erase(found);
  }
  sweep_owner_registry(registry);
  auto owner = std::make_shared<NativeRowOwner<Object>>(object);
  owner->adopt_attachment(attachment_for(std::static_pointer_cast<void>(object)));
  bind_generated_row_members(*owner, object);
  registry.entries.emplace(key, OwnerRegistryEntry{std::static_pointer_cast<void>(object), owner});
  return owner;
}

// A JavaScript `set` trap over one row. It intercepts exactly one property -- a computed symbol
// key or a plain named key, never both -- and forwards everything else to the proxied owner
// untouched. Both key spaces are represented because TypeScript writes both: the Entity runtime
// slot is a `Symbol.for` key, while a guard over a declared field such as `EntityRuntime.binding`
// names it directly.
class ProxyRowOwner final : public RowOwner {
 public:
  ProxyRowOwner(std::shared_ptr<RowOwner> target, Symbol intercepted_key, std::function<void()> before_write)
      : target_(std::move(target)),
        intercepted_key_(std::move(intercepted_key)),
        before_write_(std::move(before_write)) {}

  ProxyRowOwner(std::shared_ptr<RowOwner> target, std::string intercepted_name, std::function<void()> before_write)
      : target_(std::move(target)),
        intercepted_name_(std::move(intercepted_name)),
        before_write_(std::move(before_write)) {}

  void before_write(const Symbol& key) const override {
    if (intercepted_key_ && key == *intercepted_key_) before_write_();
    target_->before_write(key);
  }

  void before_named_write(std::string_view key) const override {
    if (intercepted_name_ && key == *intercepted_name_) before_write_();
    target_->before_named_write(key);
  }

  [[nodiscard]] std::shared_ptr<void> native_object() const noexcept override {
    return target_->native_object();
  }

  [[nodiscard]] std::type_index native_type() const noexcept override { return target_->native_type(); }

  [[nodiscard]] std::shared_ptr<RowCell> named_cell(std::string_view key) const override {
    return target_->named_cell(key);
  }

  void set_named_cell(std::string_view key, std::shared_ptr<RowCell> cell) override {
    target_->set_named_cell(key, std::move(cell));
  }

  [[nodiscard]] bool has_named_cell(std::string_view key) const override {
    return target_->has_named_cell(key);
  }

  [[nodiscard]] const std::any* dynamic_value(const Symbol& key) const override {
    return target_->dynamic_value(key);
  }

  void set_dynamic_value(const Symbol& key, std::any value) override {
    target_->set_dynamic_value(key, std::move(value));
  }

  [[nodiscard]] bool has_dynamic_value(const Symbol& key) const override {
    return target_->has_dynamic_value(key);
  }

 private:
  std::shared_ptr<RowOwner> target_;
  std::optional<Symbol> intercepted_key_;
  std::optional<std::string> intercepted_name_;
  std::function<void()> before_write_;
};

} // namespace detail

} // namespace flight

#if __has_include(<flight/sdk/structural_members.hpp>)
#include <flight/sdk/structural_members.hpp>
#else
namespace flight::detail {
template <typename Key, typename Object>
decltype(auto) generated_row_member(Object&) {
  static_assert(dependent_false<Key>, "no generated Flight SDK structural member table is available");
}

template <typename Key, typename Object>
using generated_row_member_t = void;

template <typename Object>
void bind_generated_row_members(RowOwner&, const std::shared_ptr<Object>&) {}

// With no generated member table there is nothing to prove a widening against, so none is
// provable. Failing closed is the point: an unproven conversion is rejected rather than allowed.
template <typename Base, typename Derived>
consteval bool generated_row_widening_proven() {
  return false;
}
} // namespace flight::detail
#endif

namespace flight {

namespace detail {

template <typename Value>
struct optional_traits {
  using value_type = Value;
  static constexpr bool optional = false;
};

template <typename Value>
struct optional_traits<std::optional<Value>> {
  using value_type = Value;
  static constexpr bool optional = true;
};

template <typename Value>
using optional_value_t = typename optional_traits<Value>::value_type;

template <typename Value>
using optionalize_t = std::conditional_t<optional_traits<Value>::optional, Value, std::optional<Value>>;

template <typename Value>
struct partial_member {
  using type = optionalize_t<Value>;
};

template <>
struct partial_member<void> {
  using type = void;
};

template <typename Value>
struct required_member {
  using type = optional_value_t<Value>;
};

template <>
struct required_member<void> {
  using type = void;
};

template <typename Key, typename Schema>
struct schema_member;

template <typename Key, typename Type>
struct schema_member<Key, RowOf<Type>> {
  using type = generated_row_member_t<Key, unwrap_ref_t<Type>>;
};

template <typename Key, typename Row>
struct schema_member<Key, RowReadonly<Row>> : schema_member<Key, Row> {};

template <typename Key, typename Row>
struct schema_member<Key, RowWritable<Row>> : schema_member<Key, Row> {};

template <typename Key, typename Row>
struct schema_member<Key, RowPartial<Row>> {
  using source_type = typename schema_member<Key, Row>::type;
  using type = typename partial_member<source_type>::type;
};

template <typename Key, typename Row>
struct schema_member<Key, RowRequired<Row>> {
  using source_type = typename schema_member<Key, Row>::type;
  using type = typename required_member<source_type>::type;
};

template <typename Left, typename Right>
struct merged_member {
  static constexpr bool left_absent = std::is_void_v<Left>;
  static constexpr bool right_absent = std::is_void_v<Right>;
  static constexpr bool compatible =
      left_absent || right_absent || std::same_as<optional_value_t<Left>, optional_value_t<Right>>;
  static_assert(compatible, "RowMerge contains incompatible property cell types");
  using type = std::conditional_t<
      left_absent,
      Right,
      std::conditional_t<
          right_absent,
          Left,
          std::conditional_t<optional_traits<Left>::optional && !optional_traits<Right>::optional, Right, Left>>>;
};

template <typename Key, typename... Rows>
struct merged_schema_member;

template <typename Key>
struct merged_schema_member<Key> {
  using type = void;
};

template <typename Key, typename Row, typename... Rows>
struct merged_schema_member<Key, Row, Rows...> {
  using type = typename merged_member<
      typename schema_member<Key, Row>::type,
      typename merged_schema_member<Key, Rows...>::type>::type;
};

template <typename Key, typename... Rows>
struct schema_member<Key, RowMerge<Rows...>> : merged_schema_member<Key, Rows...> {};

template <typename Key, typename Schema>
using schema_member_t = typename schema_member<Key, Schema>::type;

template <typename Schema>
inline constexpr bool schema_partial = false;

template <typename Row>
inline constexpr bool schema_partial<RowPartial<Row>> = true;

// Required is the inverse rather than the absence of partial, so it overrides an inner RowPartial
// instead of inheriting from it: `Required<Partial<T>>` names every member of T as present.
template <typename Row>
inline constexpr bool schema_partial<RowRequired<Row>> = false;

template <typename Row>
inline constexpr bool schema_partial<RowReadonly<Row>> = schema_partial<Row>;

template <typename Row>
inline constexpr bool schema_partial<RowWritable<Row>> = schema_partial<Row>;

template <typename... Rows>
inline constexpr bool schema_partial<RowMerge<Rows...>> = (schema_partial<Rows> || ...);

template <typename Schema>
inline constexpr bool schema_readonly = false;

template <typename Row>
inline constexpr bool schema_readonly<RowReadonly<Row>> = true;

template <typename Row>
inline constexpr bool schema_readonly<RowWritable<Row>> = false;

template <typename Row>
inline constexpr bool schema_readonly<RowPartial<Row>> = schema_readonly<Row>;

template <typename Row>
inline constexpr bool schema_readonly<RowRequired<Row>> = schema_readonly<Row>;

template <typename Schema>
inline constexpr bool schema_required = false;

template <typename Row>
inline constexpr bool schema_required<RowRequired<Row>> = true;

template <typename Row>
inline constexpr bool schema_required<RowReadonly<Row>> = schema_required<Row>;

template <typename Row>
inline constexpr bool schema_required<RowWritable<Row>> = schema_required<Row>;

// When one row may be read as another.
//
// Three cases are allowed and everything else is rejected:
//
//  * the same subject, which is every projection the compiler already emits -- writable to
//    readonly, whole to partial, a row to a merge that has no single subject of its own;
//  * a row with no subject at all on either side, where there is no object relationship to prove;
//  * a proven structural widening: the source's subject declares every row key the target's
//    subject declares, at the same type, so nothing the target row can ask for is missing.
//
// Direction is preserved in both dimensions. A readonly row never becomes writable, because the
// source said its subject must not be mutated through it and a conversion is not a place to
// change that answer. And widening goes one way only: a base row may be satisfied by a derived
// subject, never the reverse, because the derived row can ask for keys the base does not have.
template <typename From, typename To>
concept row_objects_convertible =
    std::is_void_v<schema_object_t<From>> || std::is_void_v<schema_object_t<To>> ||
    std::same_as<schema_object_t<From>, schema_object_t<To>> ||
    generated_row_widening_proven<schema_object_t<To>, schema_object_t<From>>();

template <typename From, typename To>
concept row_convertible_to = row_objects_convertible<From, To> &&
                             (!schema_readonly<From> || schema_readonly<To>);

} // namespace detail

template <typename Schema>
class StructuralRef {
 public:
  using object_type = detail::schema_object_t<Schema>;
  using schema_type = Schema;

  StructuralRef() = default;

  template <typename Type>
    requires(!std::is_void_v<object_type>)
  StructuralRef(std::shared_ptr<Type> object) {
    auto flattened = detail::flatten_ref<object_type>(std::move(object));
    if (flattened) {
      owner_ = detail::owner_for(flattened);
      object_ = std::static_pointer_cast<void>(flattened);
    }
  }

  // A row may be read as another row when the two describe the same subject, or when the source's
  // subject provably answers everything this schema names -- `Readonly<GlTextureRenderTarget>` read
  // as `Readonly<GlRenderTarget>`. The conversion keeps the SOURCE object and the source's one row
  // owner, so a widened read reaches the real derived object's members rather than a copy or a
  // reinterpretation of them; there is no second object and no second identity.
  //
  // It is deliberately not a free conversion. Before this constraint any row converted to any
  // other, so two unrelated rows compiled and then threw at the first read, and a readonly row
  // converted to a writable one and granted mutation the source had refused.
  template <typename OtherSchema>
    requires detail::row_convertible_to<OtherSchema, Schema>
  StructuralRef(const StructuralRef<OtherSchema>& other)
      : object_(other.shared_native_object()), owner_(other.shared_owner()) {}

  [[nodiscard]] static StructuralRef from_owner(std::shared_ptr<RowOwner> owner) {
    StructuralRef result;
    result.owner_ = std::move(owner);
    return result;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(owner_); }

  [[nodiscard]] object_type* operator->() const noexcept
    requires(!std::is_void_v<object_type>)
  {
    return shared_object().get();
  }

  template <typename Object = object_type>
  [[nodiscard]] Object& operator*() const
    requires(!std::is_void_v<Object>)
  {
    return *shared_object();
  }

  [[nodiscard]] std::shared_ptr<object_type> shared_object() const noexcept
    requires(!std::is_void_v<object_type>)
  {
    if (!owner_ || owner_->native_type() != typeid(object_type)) return {};
    if (object_) return std::static_pointer_cast<object_type>(object_);
    return std::static_pointer_cast<object_type>(owner_->native_object());
  }

  [[nodiscard]] const std::shared_ptr<RowOwner>& shared_owner() const noexcept { return owner_; }

  // The retained object, erased. A row that owns one keeps it alive; a proxy or a schema-only row
  // has none.
  [[nodiscard]] std::shared_ptr<void> shared_native_object() const noexcept {
    if (object_) return object_;
    return owner_ ? owner_->native_object() : nullptr;
  }

  template <typename OtherSchema>
  [[nodiscard]] bool operator==(const StructuralRef<OtherSchema>& other) const noexcept {
    return owner_.get() == other.shared_owner().get();
  }

 private:
  std::shared_ptr<void> object_;
  std::shared_ptr<RowOwner> owner_;
};

template <typename Key, typename Value>
struct RowField {
  using key_type = Key;
  Value value;
};

template <typename Key, typename Value>
[[nodiscard]] auto row_field(Value&& value) {
  return RowField<Key, std::remove_cvref_t<Value>>{std::forward<Value>(value)};
}

template <typename Key, typename Schema>
decltype(auto) row_get(const StructuralRef<Schema>& source) {
  using Value = detail::schema_member_t<Key, Schema>;
  static_assert(!std::is_void_v<Value>, "structural row schema does not contain the requested property");
  if (!source.shared_owner()) throw std::bad_weak_ptr();
  if constexpr (detail::schema_partial<Schema>) {
    if (auto* exact = source.shared_owner()->template named_value_if<Value>(Key::name.view())) {
      return Value(*exact);
    }
    using Present = detail::optional_value_t<Value>;
    if (auto* present = source.shared_owner()->template named_value_if<Present>(Key::name.view())) {
      return Value(*present);
    }
    return Value{};
  } else if constexpr (detail::schema_required<Schema>) {
    // The member is read as its value. Storage may hold either the bare value or the optional the
    // subject declared, and an optional that holds nothing is the case this row exists to catch:
    // the reader is told, rather than handed an empty optional to invoke.
    if (auto* exact = source.shared_owner()->template named_value_if<Value>(Key::name.view())) {
      if constexpr (detail::schema_readonly<Schema>) return std::as_const(*exact);
      else return *exact;
    }
    auto* stored = source.shared_owner()->template named_value_if<std::optional<Value>>(Key::name.view());
    if (!stored) throw std::out_of_range("required structural row property is absent");
    if (!stored->has_value()) {
      throw std::out_of_range("structural row property is required by its schema but holds no value");
    }
    if constexpr (detail::schema_readonly<Schema>) return std::as_const(**stored);
    else return **stored;
  } else {
    auto* value = source.shared_owner()->template named_value<Value>(Key::name.view());
    if (!value) throw std::out_of_range("required structural row property is absent");
    if constexpr (detail::schema_readonly<Schema>) return std::as_const(*value);
    else return *value;
  }
}

template <typename Key, typename Schema, typename Value>
void row_set(const StructuralRef<Schema>& target, Value&& value) {
  static_assert(!detail::schema_readonly<Schema>, "a readonly structural row cannot be written");
  using Expected = detail::schema_member_t<Key, Schema>;
  static_assert(!std::is_void_v<Expected>, "structural row schema does not contain the requested property");
  static_assert(std::constructible_from<Expected, Value&&>, "structural row write has an incompatible value type");
  if (!target.shared_owner()) throw std::bad_weak_ptr();
  target.shared_owner()->before_named_write(Key::name.view());
  if constexpr (detail::schema_required<Schema>) {
    // Writing through a required row keeps the subject's own storage shape: a member the subject
    // declared optional stays an optional, now engaged.
    if (auto* stored =
            target.shared_owner()->template named_value_if<std::optional<Expected>>(Key::name.view())) {
      *stored = Expected(std::forward<Value>(value));
      return;
    }
  }
  target.shared_owner()->set_named_value(Key::name.view(), Expected(std::forward<Value>(value)));
}

template <typename Value, typename Schema>
[[nodiscard]] Value row_get(const StructuralRef<Schema>& source, const Symbol& key) {
  using Object = typename StructuralRef<Schema>::object_type;
  if constexpr (!std::is_void_v<Object>) {
    if (auto object = source.shared_object()) {
      if constexpr (requires { Value{object->entity_runtime_key}; }) {
        if (key.key() == String("EntityRuntime")) return Value{object->entity_runtime_key};
      }
    }
  }
  if (source.shared_owner()) {
    if (const auto* value = source.shared_owner()->dynamic_value(key)) {
      if (const auto* result = std::any_cast<Value>(value)) return *result;
    }
  }
  if constexpr (std::default_initializable<Value>) return Value{};
  throw std::out_of_range("computed structural symbol is absent");
}

template <typename Schema, typename Value>
void row_set(const StructuralRef<Schema>& target, const Symbol& key, Value&& value) {
  static_assert(!detail::schema_readonly<Schema>, "a readonly structural row cannot be written");
  if (!target.shared_owner()) throw std::bad_weak_ptr();
  target.shared_owner()->before_write(key);
  using Object = typename StructuralRef<Schema>::object_type;
  if constexpr (!std::is_void_v<Object>) {
    if (auto object = target.shared_object()) {
      if constexpr (requires { object->entity_runtime_key = std::forward<Value>(value); }) {
        if (key.key() == String("EntityRuntime")) {
          object->entity_runtime_key = std::forward<Value>(value);
          return;
        }
      }
    }
  }
  target.shared_owner()->set_dynamic_value(key, std::remove_cvref_t<Value>(std::forward<Value>(value)));
}

template <typename Schema>
[[nodiscard]] bool row_has(const StructuralRef<Schema>& source, const Symbol& key) {
  using Object = typename StructuralRef<Schema>::object_type;
  if constexpr (!std::is_void_v<Object>) {
    if (auto object = source.shared_object()) {
      if constexpr (requires { object->entity_runtime_key.has_value(); }) {
        if (key.key() == String("EntityRuntime")) return object->entity_runtime_key.has_value();
      }
    }
  }
  return source.shared_owner() && source.shared_owner()->has_dynamic_value(key);
}

template <typename Key, typename Schema>
[[nodiscard]] bool row_has(const StructuralRef<Schema>& source) {
  if (!source.shared_owner()) return false;
  if constexpr (detail::schema_required<Schema>) {
    using Value = detail::schema_member_t<Key, Schema>;
    if (source.shared_owner()->template named_value_if<Value>(Key::name.view())) return true;
    const auto* stored =
        source.shared_owner()->template named_value_if<std::optional<Value>>(Key::name.view());
    return stored != nullptr && stored->has_value();
  }
  return source.shared_owner()->has_named_cell(Key::name.view());
}

namespace detail {

template <typename Schema, typename Field>
void initialize_row_field(const StructuralRef<Schema>& target, Field&& field) {
  using Key = typename std::remove_cvref_t<Field>::key_type;
  using Expected = schema_member_t<Key, Schema>;
  using Value = decltype(std::forward<Field>(field).value);
  static_assert(!std::is_void_v<Expected>, "structural row construction contains an unknown property");
  static_assert(std::constructible_from<Expected, Value>, "structural row construction has an incompatible value type");
  target.shared_owner()->set_named_value(
      Key::name.view(), Expected(std::forward<Field>(field).value));
}

} // namespace detail

template <typename Schema, typename... Fields>
[[nodiscard]] StructuralRef<Schema> make_structural_ref(Fields&&... fields) {
  using Object = typename StructuralRef<Schema>::object_type;
  if constexpr (std::is_void_v<Object>) {
    auto result = StructuralRef<Schema>::from_owner(std::make_shared<RowOwner>());
    (detail::initialize_row_field<Schema>(result, std::forward<Fields>(fields)), ...);
    return result;
  } else {
    StructuralRef<Schema> result(std::make_shared<Object>());
    (detail::initialize_row_field<Schema>(result, std::forward<Fields>(fields)), ...);
    return result;
  }
}

namespace detail {

// TypeScript builds an entity as `const out = {} as EntityConstruction<T>`: one empty object literal
// that is filled field by field and then handed back as the `T` it became. There is one object in
// that story, not two.
//
// C++ cannot start from the same place. The compiler emits the empty literal as an empty struct, so
// the row has no members to write into and the field writes land in cell storage instead; the cast
// that is supposed to hand back the finished `T` then finds no `T` behind the row and yields a null
// reference, which the caller dereferences. Minting the `T` at the cast is what restores the source
// semantics: the bag the row stood for becomes the object it was always going to be, carrying what
// it already holds, and every later read and write goes to that object's own members.
//
// The rule is narrow on purpose. It applies only when the source row's object is an EMPTY type --
// a bag with no state of its own to lose -- so it can never silently replace an object that holds
// data. A row over a populated object is a different question, and structural widening answers it.
template <typename To, typename From>
concept row_materializes_from = !std::is_void_v<To> && !std::is_void_v<From> &&
                                !std::same_as<To, From> && std::is_empty_v<From> &&
                                std::default_initializable<To>;

template <typename To, typename Schema>
[[nodiscard]] StructuralRef<RowWritable<RowOf<Ref<To>>>> materialize_row(
    const StructuralRef<Schema>& source) {
  auto object = std::make_shared<To>();
  auto owner = detail::owner_for(object);
  if (const auto& from = source.shared_owner()) {
    // A member the new object answers for itself wins the CELL -- it reads and writes the real
    // member, where the bag's cell was only standing in for one -- but not the VALUE: whatever the
    // bag already held is moved onto that member. A key the object does not declare keeps the bag's
    // cell, because the row still has to answer for it.
    for (const auto& [key, cell] : from->named_cells()) {
      const auto existing = owner->named_cell(key);
      if (!existing) {
        owner->set_named_cell(key, cell);
        continue;
      }
      if (cell) existing->assign_from(*cell);
    }
    if (const auto& attachment = from->attachment()) {
      for (const auto& entry : attachment->entries()) owner->set_dynamic_value(entry.key, entry.value);
    }
  }
  return StructuralRef<RowWritable<RowOf<Ref<To>>>>::from_owner(std::move(owner));
}

} // namespace detail

template <typename Target, typename Schema>
[[nodiscard]] Target structural_ref_cast(const StructuralRef<Schema>& source) {
  if constexpr (requires { typename Target::schema_type; }) {
    using To = typename Target::object_type;
    using From = typename StructuralRef<Schema>::object_type;
    if constexpr (detail::row_materializes_from<To, From>) {
      return Target(detail::materialize_row<To>(source));
    } else {
      return Target(source);
    }
  } else {
    using Object = typename StructuralRef<Schema>::object_type;
    static_assert(!std::is_void_v<Object>, "a merged structural row must be projected before native reference recovery");
    using To = detail::unwrap_ref_t<Target>;
    if constexpr (detail::row_materializes_from<To, Object>) {
      return detail::wrap_ref<Target>(detail::materialize_row<To>(source).shared_object());
    } else {
      return detail::wrap_ref<Target>(source.shared_object());
    }
  }
}

// Symbol-keyed properties attached to an object, reachable without knowing the object's type.
//
// This is the view `Record<symbol, T | undefined>` over an erased object needs. It attaches nothing
// to the object's own storage and copies neither the object nor its properties: the entries live in
// the one owner that object identity resolves to, so a typed projection, a structural projection
// and this erased view all read and write the same entries. The attachment lasts as long as the
// object does, not as long as the view that made it.
//
// A missing entry and an entry whose value is `undefined` stay distinct: `get` returns an empty
// `AnySlot` for a key that was never written, and a present `flight::Any()` for one written with
// `undefined`. `has` answers the same question without reading the value.
class AttachedProperties final {
 public:
  AttachedProperties() = default;

  AttachedProperties(std::shared_ptr<void> object, std::shared_ptr<RowOwner> owner)
      : object_(std::move(object)), owner_(std::move(owner)) {}

  [[nodiscard]] explicit operator bool() const noexcept { return owner_ != nullptr; }

  [[nodiscard]] AnySlot get(const Symbol& key) const {
    if (!owner_) return std::nullopt;
    const auto* stored = owner_->dynamic_value(key);
    if (!stored) return std::nullopt;
    const auto* value = std::any_cast<Any>(stored);
    if (!value) throw BadAnyAccess();
    return *value;
  }

  [[nodiscard]] bool has(const Symbol& key) const {
    return owner_ != nullptr && owner_->has_dynamic_value(key);
  }

  void set(const Symbol& key, Any value) const {
    if (!owner_) throw std::bad_weak_ptr();
    owner_->before_write(key);
    owner_->set_dynamic_value(key, std::any(std::move(value)));
  }

  // The object these properties are attached to, which is what keeps them reachable.
  [[nodiscard]] const std::shared_ptr<void>& object() const noexcept { return object_; }

  [[nodiscard]] const void* identity() const noexcept { return owner_.get(); }

  [[nodiscard]] friend bool operator==(const AttachedProperties& left,
                                       const AttachedProperties& right) noexcept {
    return left.owner_ == right.owner_;
  }

 private:
  std::shared_ptr<void> object_;
  std::shared_ptr<RowOwner> owner_;
};

// Resolves the one attached-property owner for an object, creating it on first use.
template <typename Object>
[[nodiscard]] AttachedProperties attached_properties(const std::shared_ptr<Object>& object) {
  if (!object) return {};
  return AttachedProperties(std::static_pointer_cast<void>(object), detail::owner_for(object));
}

// The same view over a row that already resolved an owner, so a structural projection and an
// erased reference reach one set of entries.
template <typename Schema>
[[nodiscard]] AttachedProperties attached_properties(const StructuralRef<Schema>& source) {
  return AttachedProperties(source.shared_native_object(), source.shared_owner());
}

// The declared type of a `new Proxy(target, handler)` trap table.
//
// This runtime has no prototype chain and no general property-access interception, so it does not
// implement `Proxy`. What it implements is the one proxy shape Flight actually writes: a handler
// whose single `set` trap reports a write to one known key and then forwards the write unchanged.
// The compiler pattern-matches that exact shape and lowers it to `make_structural_write_proxy`
// below, so the handler object is consumed at emission and never reaches C++ as a value.
//
// `ProxyHandler` therefore exists to give that argument position a real declared type, and it is
// deliberately not constructible. A handler the compiler cannot lower is refused at emission rather
// than silently becoming an object that accepts traps this runtime would never run, and C++ code
// that tries to assemble a trap table by hand fails to compile instead of building one that is
// quietly ignored. The type parameter is the proxied target, mirroring TypeScript's
// `ProxyHandler<T>`; it is carried so a handler position stays type-checked, not stored.
template <typename Target>
class ProxyHandler final {
 public:
  using target_type = Target;

  ProxyHandler() = delete;
  ProxyHandler(const ProxyHandler&) = delete;
  ProxyHandler(ProxyHandler&&) = delete;
  ProxyHandler& operator=(const ProxyHandler&) = delete;
  ProxyHandler& operator=(ProxyHandler&&) = delete;
  ~ProxyHandler() = delete;
};

// `new Proxy(target, { set(t, key, value) { if (key === K) report(); t[key] = value; return true; } })`
// for a computed symbol key K. The result is a distinct reference -- JavaScript's proxy is its own
// object -- over the same row storage, so the proxied object gains no second copy of its state.
template <typename Schema, typename BeforeWrite>
  requires std::invocable<BeforeWrite&>
[[nodiscard]] StructuralRef<Schema> make_structural_write_proxy(
    StructuralRef<Schema> target,
    Symbol intercepted_key,
    BeforeWrite&& before_write) {
  if (!target.shared_owner()) return {};
  auto callback = std::function<void()>(std::forward<BeforeWrite>(before_write));
  return StructuralRef<Schema>::from_owner(std::make_shared<detail::ProxyRowOwner>(
      target.shared_owner(), std::move(intercepted_key), std::move(callback)));
}

// The same trap for a plain named key, which is what a guard over a declared field compares
// against -- `prop === 'binding'` rather than `prop === EntityRuntimeKey`. The two key spaces stay
// separate: a named interception never fires for a symbol of the same spelling, and vice versa,
// because ECMAScript property keys are not interchangeable across those spaces.
template <typename Schema, typename BeforeWrite>
  requires std::invocable<BeforeWrite&>
[[nodiscard]] StructuralRef<Schema> make_structural_write_proxy(
    StructuralRef<Schema> target,
    std::string intercepted_name,
    BeforeWrite&& before_write) {
  if (!target.shared_owner()) return {};
  auto callback = std::function<void()>(std::forward<BeforeWrite>(before_write));
  return StructuralRef<Schema>::from_owner(std::make_shared<detail::ProxyRowOwner>(
      target.shared_owner(), std::move(intercepted_name), std::move(callback)));
}

} // namespace flight
