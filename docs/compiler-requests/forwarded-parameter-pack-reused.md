# A forwarded parameter pack is consumed more than once

This is a silent miscompilation: the output compiles, and every use after the first receives moved-from
values.

A rest parameter spread into a call (`slot(...args)`) is emitted as `std::forward<ArgsPack>(args)...` at
every use. Forwarding an rvalue moves it, so when the same pack is spread more than once -- in a loop, or in
two calls -- every use after the first receives moved-from values. JavaScript passes the same argument
values to every call.

## Minimal TypeScript

```ts
export function emitAll<T extends (...args: any[]) => void>(slots: T[], ...args: Parameters<T>): void {
  for (let i = 0; i < slots.length; i++) slots[i](...args);
}
```

At flight-compiler `2b687ea` with this repository's compiler source patches applied:

```cpp
slots.element(i)(std::forward<ArgsPack>(args)...);
```

Two sequential calls emit the same: `a(std::forward<ArgsPack>(args)...); b(std::forward<ArgsPack>(args)...);`.
A single use is correct.

## Observed

`@flighthq/signals` at flight `a627849` dispatches with exactly this loop in `makeDispatch` (`slot.hpp`) and
`emitSignalSafe` (`safe.hpp`). Two connected slots, called as `emitSignal(signal, flight::String("x"))`,
receive `[a:x] [b:]`: the second slot sees an empty string. The same call with an lvalue argument is
correct, because forwarding an lvalue copies. A `Ref` payload passed as an rvalue would arrive at later
slots as a null pointer.

The upstream signals tests pass numbers, which copy, so only one upstream case observes this. That case is
"emitSignalSafe gives a nested safe emission its own snapshot and arguments", with `String` arguments. It is
ported in `tests/generated_signals_test.cpp` and reported as a known defect.

## Required C++ shape

Forward only at a use that is provably the last one, and otherwise pass the pack's elements as lvalues:

```cpp
slots.element(i)(args...);
```

## Workaround carried

[`compiler-dependent-callable-pack-reuse.patch`](../../source-patches/compiler-dependent-callable-pack-reuse.patch).
The emit context records every dependent callable pack that its declaring function spreads more than once,
or inside a loop: `for`, `for-of`, `for-in`, `while` and `do`, including a loop's condition and increment.
Such a pack's spreads pass `args...` as lvalues, and a single use outside any loop still forwards.

Verified four ways:
- **Emission tests:** `cppDependentCallablePackReuse.test.ts` has 7 cases. The 5 reuse cases (two calls,
  and one spread in each loop form) fail without the patch, and the 2 single-use controls pass both ways.
- **Existing expectation:** the dispatch expectation in `cppCompilerBackend.test.ts` that pinned the
  forwarded spelling now expects lvalues. The backend suite's pre-existing failures are otherwise
  unchanged.
- **Native emitted code:** the loop, two-call and one-call functions were run with `double`, `String` and
  `Ref` arguments, as rvalues and as lvalues. All 18 expectations hold. Without the patch, exactly the four
  non-trivial rvalue reuse cells fail.
- **Signals:** `@flighthq/signals` is regenerated with the patch. The nested safe `String` case and direct
  two-slot temporary `String` and `Ref` delivery, through both `emitSignal` and `emitSignalSafe`, are
  asserted in `tests/generated_signals_test.cpp`, and all five fail against the pre-patch headers.
