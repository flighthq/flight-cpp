# A read through a closed key, and `Number.isFinite` of a union, lower to C++ that does not compile

A property read whose key is a closed union of declared keys -- an element of a `keyof` tuple, or a
`string & keyof T` parameter -- is emitted as `flight::row_get<T>(view, key)` with the key erased to
`flight::String`. The runtime has no string-keyed row read, and should not grow one: a struct's named
bindings are generated only for keys the emitted tree reads statically (`flight/sdk/structural_members.hpp`),
so a key named only inside a string table has no binding to find. `gravityZ` lost its binding at a627849 for
exactly this reason. Separately, `Number.isFinite(value)` where `value` is a union containing non-numbers
is emitted as `std::isfinite(value)` over `std::optional<std::variant<...>>`, which has no overload. In
JavaScript it is `false` for every non-number.

Measured at flight-compiler `2b687ea` with all 44 compiler source patches applied: 1 module emitted, 0 refused,
and 3 compile errors against this runtime.

## Minimal TypeScript (measured)

```ts
export interface Shape {
  a: number;
  b: number;
  name: string;
  list: number[] | null;
}

const KEYS = ['a', 'b'] as const satisfies readonly (keyof Shape)[];

export function nonFiniteKeys(shape: Readonly<Shape>): string[] {
  const out: string[] = [];
  for (const key of KEYS) {
    const value = shape[key];
    if (!Number.isFinite(value)) out.push(key);
  }
  return out;
}

export function isPositive(shape: Readonly<Shape>, key: string & keyof Shape): boolean {
  const value = shape[key];
  return Number.isFinite(value) && (value as number) > 0;
}
```

## Observed

```cpp
inline flight::Array<flight::String> keys = flight::Array<flight::String>{flight::String("a"), flight::String("b")};
...
    const double value = flight::row_get<double>(shape, key);           // no matching function for call to 'row_get'
...
  std::optional<std::variant<flight::Array<double>, double, flight::String>> value =
      flight::row_get<std::optional<std::variant<...>>>(shape, key);   // no matching function for call to 'row_get'
  return (std::isfinite(value) && ...);                                // no matching function for call to 'isfinite'
```

## Expected behavior

- The key's type is a closed union of declared members, so the read can be a switch over those members.
  Each arm reads its member directly, which needs no runtime key lookup and no named binding. Where the
  arms' types differ, the result is the union the source declares.
- `Number.isFinite(x)` of a union is `x` holds the number alternative and that number is finite. Every
  other alternative, and absence, is `false`. It must not be `std::isfinite` of the carrier.

## Affected modules

`@flighthq/particles` `validateParticleEmitterConfig.ts`:
- `validateParticleEmitterConfig` (two `NUMERIC_FIELDS` / `NON_NEGATIVE_FIELDS` loops);
- `reportInvertedRange` and `reportUnitRange` (`string & keyof ParticleEmitterConfig` parameters).

That is 5 `row_get` and 3 `isfinite` diagnostics. Before a627849 these functions were refused outright
(`cpp-intersection-member-shapeless`). They now emit, but do not compile.

`normalizeParticleEmitterConfig` in the same module is refused, correctly, as
`cpp-erased-record-assertion-unrepresented`: it writes through `out as unknown as Record<string, number>`.

## Carrying workaround

The `particles-validate-particle-emitter-config` override supplies these functions. Each closed-key read
goes through a dispatcher over the 45 numeric `ParticleEmitterConfig` members. The `string & keyof`
readers do not go through it: they read the named member and apply `Number.isFinite` to the number it holds.
