// `flight::indexed_member_t<T, RowKey<"key">>` is `T['key']` for a type parameter, resolved at instantiation.
// The compiler emits it where TypeScript writes `failure: Result['outcome']` (@flighthq/tray invokeRead and
// invokeUpdate) and `kind: T['kind']` (@flighthq/materials getMaterialOfKind). A type argument for an object type
// is a Ref, a structural row, or a variant of those, so the member is read through the reference, the row's
// schema, or every alternative alike. Against the committed generated member table.
#include <flight/runtime.hpp>

#include <flight/types/app_window.hpp>
#include <flight/types/blinn_phong_material.hpp>
#include <flight/types/material.hpp>
#include <flight/types/tray.hpp>

#include <concepts>
#include <functional>
#include <iostream>
#include <optional>
#include <type_traits>

namespace {

using namespace flight::types;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Type, typename Key>
using Unchecked = typename flight::detail::indexed_member<Type, Key>::type;

using Outcome = flight::RowKey<"outcome">;
using Kind = flight::RowKey<"kind">;

// A variant whose alternatives all store `outcome` as a String: the result unions of @flighthq/tray.
static_assert(std::same_as<flight::indexed_member_t<TrayImageUpdateResult, Outcome>, flight::String>);
// A Ref to a nominal type, and a Ref to its derived interface.
static_assert(std::same_as<flight::indexed_member_t<flight::Ref<Material>, Kind>, flight::String>);
static_assert(std::same_as<flight::indexed_member_t<flight::Ref<BlinnPhongMaterial>, Kind>, flight::String>);
// A structural row reads the member through its schema.
static_assert(std::same_as<
              flight::indexed_member_t<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Material>>>>, Kind>,
              flight::String>);
// An optional member keeps its declared storage.
static_assert(std::same_as<flight::indexed_member_t<flight::Ref<HostWindowGeometryCapability>, flight::RowKey<"center">>,
                           std::optional<std::function<void(flight::Ref<AppWindow>)>>>);

// Negatives resolve to nothing, which the checked alias turns into a compile-time error at instantiation.
struct NumberOutcome final : public flight::ReferenceEnabled {
  double outcome = 0;
};
struct StringOutcome final : public flight::ReferenceEnabled {
  flight::String outcome;
};
struct NoOutcome final : public flight::ReferenceEnabled {
  flight::String kind;
};
// Alternatives that store the member as different C++ types: TypeScript's answer is a union, not either one.
static_assert(std::is_void_v<Unchecked<std::variant<flight::Ref<StringOutcome>, flight::Ref<NumberOutcome>>, Outcome>>);
// An alternative without the member.
static_assert(std::is_void_v<Unchecked<std::variant<flight::Ref<StringOutcome>, flight::Ref<NoOutcome>>, Outcome>>);
// A key the type does not declare, and a type that is not an object.
static_assert(std::is_void_v<Unchecked<flight::Ref<Material>, Outcome>>);
static_assert(std::is_void_v<Unchecked<double, Kind>>);
// An optional member in one alternative and a required one in another is a union with undefined, not one type.
struct OptionalOutcome final : public flight::ReferenceEnabled {
  std::optional<flight::String> outcome;
};
static_assert(std::is_void_v<Unchecked<std::variant<flight::Ref<StringOutcome>, flight::Ref<OptionalOutcome>>, Outcome>>);

// The emitted shape: a function template whose parameter is `Result['outcome']`, called with explicit arguments.
template <typename Result>
flight::indexed_member_t<Result, Outcome> echo_outcome(flight::indexed_member_t<Result, Outcome> failure) {
  return failure;
}

template <typename T>
bool is_kind(const flight::Ref<Material>& material, flight::indexed_member_t<T, Kind> kind) {
  return material->kind == kind;
}

} // namespace

int main() {
  check(echo_outcome<TrayImageUpdateResult>(flight::String("image-update-failed")) == flight::String("image-update-failed"),
        "a variant's agreed member type passes the value through unchanged");
  auto material = flight::make_ref<Material>();
  material->kind = flight::String("blinn-phong");
  check(is_kind<flight::Ref<BlinnPhongMaterial>>(material, flight::String("blinn-phong")), "a Ref argument's member type");
  check(!is_kind<flight::Ref<BlinnPhongMaterial>>(material, flight::String("pbr")), "and compares by value");
  if (failures == 0) std::cout << "indexed member tests passed\n";
  return failures == 0 ? 0 : 1;
}
