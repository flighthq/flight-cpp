#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <flight/any.hpp>
#include <flight/array.hpp>
#include <flight/dom_exception.hpp>
#include <flight/map.hpp>
#include <flight/record.hpp>
#include <flight/set.hpp>
#include <flight/string.hpp>

// `structuredClone`. The snapshot package is the reason it exists downstream: capture, restore, and
// interpolation all deep-copy a caller's value and rely on the copy sharing nothing with the
// original except the sharing the original had with itself.
//
// Two properties are deliberate:
//
//  * Shared references and cycles survive. Two members that pointed at one object still point at
//    one object after the clone, and a reference graph that contains a cycle clones without
//    recursing forever, because each reference is recorded before its contents are copied.
//  * A value the runtime cannot clone is refused, not shallow-copied. Types with no clone
//    definition fail to compile with a message naming the customization point; values that
//    JavaScript itself refuses -- symbols and functions -- throw `DataCloneError` at runtime, which
//    is what `structuredClone` does.
//
// A generated aggregate opts in by specializing `structured_clone_traits`. That is a deliberate
// requirement rather than an oversight: C++20 cannot enumerate an aggregate's members, so a
// default that copied the aggregate would produce a shallow copy wearing a deep copy's name.
namespace flight {

class DataCloneError final : public std::runtime_error {
 public:
  explicit DataCloneError(String message)
      : std::runtime_error(message.to_utf8()),
        exception(std::move(message), String("DataCloneError")) {}

  DOMException exception;
};

namespace detail {

// Records each reference already cloned, so one object reached twice is cloned once and a cycle
// terminates.
class StructuredCloneMemo final {
 public:
  template <typename Value>
  [[nodiscard]] std::shared_ptr<Value> find(const std::shared_ptr<Value>& source) const {
    const auto found = entries_.find(source.get());
    if (found == entries_.end()) return {};
    return std::static_pointer_cast<Value>(found->second);
  }

  template <typename Value>
  void record(const std::shared_ptr<Value>& source, const std::shared_ptr<Value>& clone) {
    entries_.emplace(source.get(), std::static_pointer_cast<void>(clone));
  }

 private:
  std::unordered_map<const void*, std::shared_ptr<void>> entries_;
};

template <typename>
inline constexpr bool structured_clone_unsupported = false;

} // namespace detail

// The customization point. A specialization supplies
// `static Value clone(const Value&, flight::detail::StructuredCloneMemo&)`.
template <typename Value, typename = void>
struct structured_clone_traits {
  static_assert(detail::structured_clone_unsupported<Value>,
                "flight::structured_clone has no definition for this type; specialize "
                "flight::structured_clone_traits rather than accepting a shallow copy");
};

template <typename Value>
[[nodiscard]] Value structured_clone(const Value& value, detail::StructuredCloneMemo& memo) {
  return structured_clone_traits<Value>::clone(value, memo);
}

template <typename Value>
[[nodiscard]] Value structured_clone(const Value& value) {
  detail::StructuredCloneMemo memo;
  return structured_clone(value, memo);
}

// Values that carry no interior structure are cloned by copying them.
template <typename Value>
  requires(std::is_arithmetic_v<Value> || std::same_as<Value, String> ||
           std::same_as<Value, Undefined> || std::same_as<Value, Null>)
struct structured_clone_traits<Value> {
  [[nodiscard]] static Value clone(const Value& value, detail::StructuredCloneMemo&) {
    return value;
  }
};

// JavaScript refuses to clone a symbol, and so does this.
template <>
struct structured_clone_traits<Symbol> {
  [[nodiscard]] static Symbol clone(const Symbol&, detail::StructuredCloneMemo&) {
    throw DataCloneError(String("a symbol cannot be structurally cloned"));
  }
};

template <typename Value>
struct structured_clone_traits<std::optional<Value>> {
  [[nodiscard]] static std::optional<Value> clone(const std::optional<Value>& value,
                                                  detail::StructuredCloneMemo& memo) {
    if (!value) return std::nullopt;
    return structured_clone(*value, memo);
  }
};

template <typename Value>
struct structured_clone_traits<std::vector<Value>> {
  [[nodiscard]] static std::vector<Value> clone(const std::vector<Value>& value,
                                                detail::StructuredCloneMemo& memo) {
    std::vector<Value> result;
    result.reserve(value.size());
    for (const auto& element : value) result.push_back(structured_clone(element, memo));
    return result;
  }
};

template <typename First, typename Second>
struct structured_clone_traits<std::pair<First, Second>> {
  [[nodiscard]] static std::pair<First, Second> clone(const std::pair<First, Second>& value,
                                                      detail::StructuredCloneMemo& memo) {
    return {structured_clone(value.first, memo), structured_clone(value.second, memo)};
  }
};

template <typename... Elements>
struct structured_clone_traits<std::tuple<Elements...>> {
  [[nodiscard]] static std::tuple<Elements...> clone(const std::tuple<Elements...>& value,
                                                     detail::StructuredCloneMemo& memo) {
    return std::apply(
        [&memo](const Elements&... elements) {
          return std::tuple<Elements...>(structured_clone(elements, memo)...);
        },
        value);
  }
};

template <typename... Alternatives>
struct structured_clone_traits<std::variant<Alternatives...>> {
  [[nodiscard]] static std::variant<Alternatives...> clone(
      const std::variant<Alternatives...>& value, detail::StructuredCloneMemo& memo) {
    return std::visit(
        [&memo](const auto& held) {
          return std::variant<Alternatives...>(structured_clone(held, memo));
        },
        value);
  }
};

template <typename Value>
struct structured_clone_traits<Array<Value>> {
  [[nodiscard]] static Array<Value> clone(const Array<Value>& value,
                                          detail::StructuredCloneMemo& memo) {
    Array<Value> result;
    for (const auto& element : value) result.push(structured_clone(element, memo));
    return result;
  }
};

template <typename Key, typename Value>
struct structured_clone_traits<Record<Key, Value>> {
  [[nodiscard]] static Record<Key, Value> clone(const Record<Key, Value>& value,
                                                detail::StructuredCloneMemo& memo) {
    Record<Key, Value> result;
    for (const auto& [key, element] : value) {
      result.set(structured_clone(key, memo), structured_clone(element, memo));
    }
    return result;
  }
};

template <typename Key, typename Value>
struct structured_clone_traits<Map<Key, Value>> {
  [[nodiscard]] static Map<Key, Value> clone(const Map<Key, Value>& value,
                                             detail::StructuredCloneMemo& memo) {
    Map<Key, Value> result;
    for (const auto& [key, element] : value) {
      result.set(structured_clone(key, memo), structured_clone(element, memo));
    }
    return result;
  }
};

template <typename Value>
struct structured_clone_traits<Set<Value>> {
  [[nodiscard]] static Set<Value> clone(const Set<Value>& value,
                                        detail::StructuredCloneMemo& memo) {
    Set<Value> result;
    for (const auto& element : value) result.add(structured_clone(element, memo));
    return result;
  }
};

// A reference is cloned once. The clone is created and recorded before its contents are copied, so
// a cycle reaches the recorded clone instead of recursing, and two members that shared one object
// still share one object afterwards.
template <typename Value>
  requires std::default_initializable<Value>
struct structured_clone_traits<std::shared_ptr<Value>> {
  [[nodiscard]] static std::shared_ptr<Value> clone(const std::shared_ptr<Value>& value,
                                                    detail::StructuredCloneMemo& memo) {
    if (!value) return {};
    if (auto existing = memo.find(value)) return existing;
    auto result = std::make_shared<Value>();
    memo.record(value, result);
    *result = structured_clone(*value, memo);
    return result;
  }
};

// An erased value clones its primitive alternatives exactly. An object, a host value, or a callable
// is refused: the runtime holds it behind a type-erased handle with no clone definition attached,
// and copying the handle would share the object while claiming to have cloned it. A host that needs
// one of its own types cloned specializes `structured_clone_traits` and stores the clone itself.
template <>
struct structured_clone_traits<Any> {
  [[nodiscard]] static Any clone(const Any& value, detail::StructuredCloneMemo&) {
    switch (value.kind()) {
      case AnyKind::undefined:
      case AnyKind::null:
      case AnyKind::boolean:
      case AnyKind::number:
      case AnyKind::string:
        return value;
      case AnyKind::symbol:
        throw DataCloneError(String("a symbol cannot be structurally cloned"));
      case AnyKind::function:
        throw DataCloneError(String("a function cannot be structurally cloned"));
      case AnyKind::object:
      case AnyKind::external:
        break;
    }
    throw DataCloneError(
        String("an erased object has no structured-clone definition; the host must supply one"));
  }
};

} // namespace flight
