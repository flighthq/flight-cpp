#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <type_traits>
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

template <typename Row>
struct schema_source<RowPartial<Row>> : schema_source<Row> {};

template <typename Row>
struct schema_source<RowReadonly<Row>> : schema_source<Row> {};

template <typename Row>
struct schema_source<RowWritable<Row>> : schema_source<Row> {};

template <typename Schema>
using schema_object_t = unwrap_ref_t<typename schema_source<Schema>::type>;

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
} // namespace flight::detail
#endif

namespace flight {

template <typename Schema>
class StructuralRef {
 public:
  using object_type = detail::schema_object_t<Schema>;

  StructuralRef() = default;

  explicit StructuralRef(std::shared_ptr<object_type> object) : object_(std::move(object)) {}

  template <typename Type>
  StructuralRef(std::shared_ptr<Type> object)
      : object_(detail::flatten_ref<object_type>(std::move(object))) {}

  template <typename OtherSchema>
    requires std::same_as<object_type, typename StructuralRef<OtherSchema>::object_type>
  StructuralRef(const StructuralRef<OtherSchema>& other) : object_(other.shared_object()) {}

  [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(object_); }
  [[nodiscard]] object_type* operator->() const noexcept { return object_.get(); }
  [[nodiscard]] object_type& operator*() const noexcept { return *object_; }
  [[nodiscard]] const std::shared_ptr<object_type>& shared_object() const noexcept { return object_; }

  template <typename OtherSchema>
  [[nodiscard]] bool operator==(const StructuralRef<OtherSchema>& other) const noexcept {
    return object_.get() == other.shared_object().get();
  }

 private:
  std::shared_ptr<object_type> object_;
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
  return detail::generated_row_member<Key>(*source);
}

template <typename Key, typename Schema, typename Value>
void row_set(const StructuralRef<Schema>& target, Value&& value) {
  detail::generated_row_member<Key>(*target) = std::forward<Value>(value);
}

template <typename Value, typename Schema>
[[nodiscard]] Value row_get(const StructuralRef<Schema>& source, const Symbol& key) {
  if (key.key() != String("EntityRuntime")) {
    throw std::out_of_range("computed structural symbol has no generated field binding");
  }
  if constexpr (requires { Value{source->entity_runtime_key}; }) {
    return Value{source->entity_runtime_key};
  } else {
    static_assert(detail::dependent_false<Schema>, "structural symbol read has no generated field binding");
  }
}

template <typename Schema, typename Value>
void row_set(const StructuralRef<Schema>& target, const Symbol& key, Value&& value) {
  if (key.key() != String("EntityRuntime")) {
    throw std::out_of_range("computed structural symbol has no generated field binding");
  }
  if constexpr (requires { target->entity_runtime_key = std::forward<Value>(value); }) {
    target->entity_runtime_key = std::forward<Value>(value);
  } else {
    static_assert(detail::dependent_false<Schema>, "structural symbol write has no generated field binding");
  }
}

template <typename Schema, typename... Fields>
[[nodiscard]] StructuralRef<Schema> make_structural_ref(Fields&&... fields) {
  using Object = typename StructuralRef<Schema>::object_type;
  StructuralRef<Schema> result(std::make_shared<Object>());
  (row_set<typename std::remove_cvref_t<Fields>::key_type>(
       result, std::forward<Fields>(fields).value),
   ...);
  return result;
}

template <typename Target, typename Schema>
[[nodiscard]] Target structural_ref_cast(const StructuralRef<Schema>& source) {
  if constexpr (requires { typename Target::object_type; }) {
    return Target(source);
  } else {
    return detail::wrap_ref<Target>(source.shared_object());
  }
}

} // namespace flight
