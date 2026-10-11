# Nominal union to readonly-view union

A represented union of nominal references cannot be passed to the corresponding union of readonly
structural views, although every source arm has one exact or independently proven destination row.

## Minimal TypeScript

```ts
interface Base { kind: string }
interface First extends Base { first: number }
interface Second extends Base { second: number }
type Value = Base | First | Second;
type ReadonlyValue = Readonly<Base> | Readonly<First> | Readonly<Second>;
function read(value: ReadonlyValue): string { return value.kind; }
export function readMutable(value: Value): string { return read(value); }
```

## Expected C++

```cpp
return read(std::visit([](const auto& value) -> ReadonlyValue {
  using Value = std::decay_t<decltype(value)>;
  if constexpr (std::is_same_v<Value, flight::Ref<First>>) {
    return ReadonlyValue{std::in_place_type<flight::StructuralRef</* First row */>>,
        flight::structural_ref_cast</* First row */>(value)};
  }
  // Equivalent exact branches for Base and Second.
}, value));
```

## Affected modules

The ShadedMaterial compiler/binder and PBR extension paths that pass mutable modifier or texture arms
to readonly callbacks.

## Carrying workaround

[`compiler-readonly-union-view-conversion.patch`](../../source-patches/compiler-readonly-union-view-conversion.patch)
emits an exhaustive visit after proving one row destination per source owner. It expires when upstream
supports checked nominal-union to readonly-view-union conversion.
