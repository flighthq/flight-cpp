#pragma once

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace flight {

// Marks generated object and class shapes while preserving aggregate initialization.
struct ReferenceEnabled {};

namespace detail {

template <typename Value, typename = void>
struct reference_type {
  using type = std::shared_ptr<Value>;
};

template <typename Value>
struct reference_type<Value, std::void_t<decltype(sizeof(Value))>> {
  using type = std::conditional_t<std::derived_from<Value, ReferenceEnabled>, std::shared_ptr<Value>, Value>;
};

template <typename Value>
struct reference_type<std::shared_ptr<Value>, void> {
  using type = std::shared_ptr<Value>;
};

template <typename Value>
inline constexpr bool is_shared_ptr = false;

template <typename Value>
inline constexpr bool is_shared_ptr<std::shared_ptr<Value>> = true;

} // namespace detail

// Interface projections may apply Ref to a representation that is already reference-backed.
// Preserve the original owner instead of manufacturing nested shared_ptr layers.
template <typename Value>
using Ref = typename detail::reference_type<Value>::type;

template <typename Value, typename... Arguments>
[[nodiscard]] Ref<Value> make_ref(Arguments&&... arguments) {
  if constexpr (detail::is_shared_ptr<Value>) {
    return Value(std::forward<Arguments>(arguments)...);
  } else if constexpr (std::same_as<Ref<Value>, Value>) {
    return Value(std::forward<Arguments>(arguments)...);
  } else {
    return std::make_shared<Value>(std::forward<Arguments>(arguments)...);
  }
}

template <typename Value>
class BindingCell {
 public:
  explicit BindingCell(Value value) : value_(std::make_shared<Value>(std::move(value))) {}

  [[nodiscard]] Value read_binding() const { return *value_; }

  void rebind(Value value) const { *value_ = std::move(value); }

  template <typename Update>
  decltype(auto) update_binding(Update&& update) const {
    return std::forward<Update>(update)(*value_);
  }

 private:
  std::shared_ptr<Value> value_;
};

template <typename Value>
[[nodiscard]] BindingCell<Value> make_binding_cell(Value value) {
  return BindingCell<Value>(std::move(value));
}

} // namespace flight
