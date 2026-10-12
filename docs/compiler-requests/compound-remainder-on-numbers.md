# Compound `%=` on a number must lower to `fmod`

A binary `a % b` on numbers already lowers to `std::fmod(a, b)`. The compound assignment `a %= b` on a
captured numeric local does not: it is emitted verbatim inside the binding cell's update, and C++ has no
`%=` for `double`, so the header does not compile.

## Minimal TypeScript

```ts
export function accumulate(period: number): (delta: number) => number {
  let elapsed = 0;
  return (delta) => {
    elapsed += delta;
    elapsed %= period;
    return elapsed;
  };
}
```

## Expected C++

```cpp
elapsed_capture.update_binding([&](auto& value) { value = std::fmod(value, period); return value; });
```

`std::fmod` has the JavaScript remainder's semantics on numbers: truncation toward zero and the
dividend's sign.

## Affected modules

`connectSignalAtFrameRate` in `@flighthq/signals` (`throttle.ts`): `flight/signals/throttle.hpp`, and
through it `contract.hpp` and `_internal_index.hpp`, fail with "invalid operands to binary expression
('double' and 'const double')".

## Carrying workaround

Override [`signals-throttle-typed-argument-buffers`](../../overrides/manifest.json) writes the
remainder as `std::fmod`. The request closes when compound `%=` uses the same lowering as binary `%`.
