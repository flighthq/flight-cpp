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
};

template <typename Value>
class TypedRowCell : public RowCell {
 public:
  [[nodiscard]] std::type_index value_type() const noexcept final { return typeid(Value); }
  [[nodiscard]] virtual Value& get() = 0;
  virtual void set(Value value) = 0;
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

  [[nodiscard]] virtual const std::any* dynamic_value(const Symbol& key) const {
    const auto found = dynamic_values_.find(key.identity());
    return found == dynamic_values_.end() ? nullptr : &found->second;
  }

  virtual void set_dynamic_value(const Symbol& key, std::any value) {
    dynamic_values_.insert_or_assign(key.identity(), std::move(value));
  }

  [[nodiscard]] virtual bool has_dynamic_value(const Symbol& key) const {
    return dynamic_values_.contains(key.identity());
  }

 private:
  std::unordered_map<std::string, std::shared_ptr<RowCell>> named_cells_;
  std::unordered_map<const void*, std::any> dynamic_values_;
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

template <typename Object>
class NativeRowOwner final : public RowOwner {
 public:
  explicit NativeRowOwner(std::shared_ptr<Object> object) : object_(std::move(object)) {}

  [[nodiscard]] std::shared_ptr<void> native_object() const noexcept override {
    return std::static_pointer_cast<void>(object_);
  }

  [[nodiscard]] std::type_index native_type() const noexcept override { return typeid(Object); }

 private:
  std::shared_ptr<Object> object_;
};

template <typename Object>
[[nodiscard]] std::shared_ptr<RowOwner> owner_for(const std::shared_ptr<Object>& object) {
  static std::mutex mutex;
  static std::unordered_map<const Object*, std::weak_ptr<RowOwner>> owners;
  const std::lock_guard lock(mutex);
  if (const auto found = owners.find(object.get()); found != owners.end()) {
    if (auto owner = found->second.lock()) return owner;
    owners.erase(found);
  }
  auto owner = std::make_shared<NativeRowOwner<Object>>(object);
  bind_generated_row_members(*owner, object);
  owners.emplace(object.get(), owner);
  return owner;
}

class ProxyRowOwner final : public RowOwner {
 public:
  ProxyRowOwner(std::shared_ptr<RowOwner> target, Symbol intercepted_key, std::function<void()> before_write)
      : target_(std::move(target)),
        intercepted_key_(std::move(intercepted_key)),
        before_write_(std::move(before_write)) {}

  void before_write(const Symbol& key) const override {
    if (key == intercepted_key_) before_write_();
    target_->before_write(key);
  }

  void before_named_write(std::string_view key) const override {
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
  Symbol intercepted_key_;
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
    if (flattened) owner_ = detail::owner_for(flattened);
  }

  template <typename OtherSchema>
  StructuralRef(const StructuralRef<OtherSchema>& other) : owner_(other.shared_owner()) {}

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
    return std::static_pointer_cast<object_type>(owner_->native_object());
  }

  [[nodiscard]] const std::shared_ptr<RowOwner>& shared_owner() const noexcept { return owner_; }

  template <typename OtherSchema>
  [[nodiscard]] bool operator==(const StructuralRef<OtherSchema>& other) const noexcept {
    return owner_.get() == other.shared_owner().get();
  }

 private:
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
    if (auto* exact = source.shared_owner()->template named_value<Value>(Key::name.view())) return Value(*exact);
    using Present = detail::optional_value_t<Value>;
    if (auto* present = source.shared_owner()->template named_value<Present>(Key::name.view())) {
      return Value(*present);
    }
    return Value{};
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
  return source.shared_owner() && source.shared_owner()->has_named_cell(Key::name.view());
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

template <typename Target, typename Schema>
[[nodiscard]] Target structural_ref_cast(const StructuralRef<Schema>& source) {
  if constexpr (requires { typename Target::schema_type; }) {
    return Target(source);
  } else {
    using Object = typename StructuralRef<Schema>::object_type;
    static_assert(!std::is_void_v<Object>, "a merged structural row must be projected before native reference recovery");
    return detail::wrap_ref<Target>(source.shared_object());
  }
}

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

} // namespace flight
