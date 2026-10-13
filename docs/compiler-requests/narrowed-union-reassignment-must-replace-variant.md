# Reassigning a narrowed union must replace the whole variant

A local narrowed to one union arm is later assigned a new value of the original union type. The C++
emitter keeps the earlier narrowing as an lvalue projection and assigns the whole new variant into
that alternative, which does not compile.

Measured with flight-compiler `2b687ea` and Flight `a627849`.

## Minimal TypeScript

```ts
const FAILURE = Symbol('failure');
type Result = number | typeof FAILURE;

declare function next(previous: number): Result;

export function advance(seed: Result): Result {
  let value = seed;
  if (value === FAILURE) return FAILURE;
  value = next(value);
  if (value === FAILURE) return FAILURE;
  return value;
}
```

## Observed C++ shape

```cpp
std::variant<double, flight::Symbol> value = seed;
if (value.index() == 1) return failure;
std::get<0>(value) = next(std::get<0>(value));
```

`next` returns the full `std::variant<double, flight::Symbol>`, which cannot be assigned to the
projected `double` alternative.

## Required behavior

An assignment of the declared union type must replace the whole variant and invalidate the previous
flow narrowing. Only an assignment already proved to be the narrowed arm may target
`std::get<arm>(value)`.

## Affected module

`@flighthq/scene-document`, `sceneDocumentYamlSubset.ts`: `parseSequenceMapping` checks the first
`addMappingEntry` result, then reassigns `count` from another call inside its continuation loop. The
emitter assigns that result variant into `std::get<0>(count)`, preventing the YAML parser and every
header including it from compiling.

## Carrying workaround

The package binds the first call to `initialCount`, returns on its failure arm, and then initializes a
separate numeric `count`. Each later call is bound to `nextCount`, checked, and only its proven number
is assigned to `count`. The parser already returned immediately whenever either result was failure, so
no failure value was observably stored between operations; call count, order, mapping mutation,
diagnostics, and every numeric update are unchanged.
