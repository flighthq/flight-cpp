# Strict equality between a nullable, optional value and a present value must not lower to a raw `==`

`bones[i].name === name` compares `Bone2D.name` (`name?: string | null`) with a `string`. The emitter keeps
the bone name in its three-state carrier, `std::variant<flight::String, flight::Null, flight::Undefined>`,
and writes the comparison as a plain C++ `==` between that variant and a `flight::String`. Nothing defines
that operator, so the header does not compile. The emitted function itself is not refused; the defect only
shows at C++ compile time.

Measured at flight-compiler `2b687ea` with this repository's compiler patches, in the emitted
`@flighthq/skeleton2d` at flight `a627849`.

## Observed

`generated/include/flight/skeleton2d/skeleton2d.hpp:181`, in `get_skeleton2_dbone_index_by_name`:

```cpp
if ((([&]() -> std::variant<flight::String, flight::Null, flight::Undefined> { /* re-wrap bones.element(i)->name */ }()) == name)) {
```

```text
error: invalid operands to binary expression ('std::variant<flight::String, flight::Null, flight::Undefined>' and 'flight::String')
```

A two-state `string | null` member does not hit this, because it is carried as `std::optional<flight::String>`,
whose `==` against a value is already defined.

## Required C++ shape

`===` against a present value is true exactly when the carrier holds that alternative and it compares equal:

```cpp
if (std::holds_alternative<flight::String>(bone_name) && std::get<flight::String>(bone_name) == name) {
```

Either the emitter writes this, or the runtime defines the comparison once for every presence carrier: a
`template <typename T> bool operator==(const std::variant<T, flight::Null, flight::Undefined>&, const T&)` in
namespace `flight`, found by argument-dependent lookup through `flight::Null`. Because the right side is a
present value, strict and loose equality agree here, so one operator serves both. A runtime operator would
need its own direct tests of the null, undefined, equal and unequal cases.

## Affected Flight modules

`@flighthq/skeleton2d` `skeleton2d.ts` `getSkeleton2DBoneIndexByName`. It fails `skeleton2d.hpp`, the core
module header, and `ik_constraint2_d.hpp`, which includes it.

## Carrying workaround

[`skeleton2d-bone-name-present-comparison`](../../source-patches/skeleton2d-bone-name-present-comparison.patch)
narrows the bone name to a present local before comparing.

## Answered in the runtime

`flight/presence.hpp` now defines the comparison: `operator==(const std::variant<Value, Null, Undefined>&, const
Value&)`, found by argument-dependent lookup through `flight::Null`, with direct tests in `tests/runtime_test.cpp`
(`test_nullish_variant_strict_equality`) for the present, null, undefined, reversed, `!=` and NaN cases. Emitted
`===` against a present value now compiles as written. Two packages are affected:
- `@flighthq/particles-formats` `particleDesignerSerialize.ts` (`blendModeToSrc`, `blendModeToDst`) compiles with
  no patch.
- `skeleton2d-bone-name-present-comparison` is no longer needed for its compile error. It stays until skeleton2d
  is next regenerated without it, which is outside this change.
