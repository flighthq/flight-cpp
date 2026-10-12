# A null test on an element of a nullable-element array tests the wrong optional layer

This is a silent miscompilation: the output compiles and returns wrong answers.

`(T | null)[]` lowers to `flight::Array<std::optional<T>>`, and an index read lowers to `Array::get(i)`,
which returns `std::optional<std::optional<T>>`. The outer layer answers "is the index in range" (JavaScript
`undefined`) and the inner layer answers "is the element null". Every comparison of an indexed element
against `null` or `undefined` is emitted as `get(i).has_value()`, the outer layer only. So a `null` element
compares as present, and an out-of-range read compares as null.

## Minimal TypeScript

```ts
export function live(slots: (number | null)[], i: number): boolean {
  return slots[i] !== null;
}
```

At flight-compiler `2b687ea` with all 38 of this repository's compiler source patches applied:

```cpp
inline bool live(flight::Array<std::optional<double>> slots, double i) {
  return slots.get(i).has_value();
}
```

Run against `[null, 1]`:

| expression | JavaScript | native |
| --- | --- | --- |
| `slots[0] !== null` | `false` | `true` |
| `slots[1] !== null` | `true` | `true` |
| `slots[5] !== null` | `true` | `false` |

## Pinned boundary

All of these emit the same `slots.get(i).has_value()`, or its negation:

| source | correct C++ meaning |
| --- | --- |
| `slots[i] !== null` | out of range, or in range and the element present |
| `slots[i] === null` | in range and the element null |
| `slots[i] != null` | in range and the element present |
| `slots[i] !== undefined` | in range (the only one the current emission gets right) |
| `const slot = slots[i]; slot !== null` | same as `slots[i] !== null`; the bound local is typed `std::optional<std::optional<double>>` and tested with one `has_value()` |

The element type does not matter: `((() => void) | null)[]` emits identically.

## Required C++ shape

Test both layers as the source comparison requires, for example for `slots[i] !== null`:

```cpp
const auto element = slots.get(i);
return !element.has_value() || element->has_value();
```

## Affected Flight modules

`@flighthq/signals` at flight `a627849` uses a null slot as a tombstone while a dispatch is running:
- `compactSignalData` skips tombstones with `data.slots[read] === null`
  (`generated/include/flight/signals/slot.hpp:71`, and the copy in `safe.hpp:50`);
- `countLiveSlots`, behind `hasSignalSlots`, counts `data.slots[i] !== null` (`slot.hpp:167`).

Natively a tombstone therefore counts as a live slot and is never compacted. After a slot disconnects
during a dispatch:
- `hasSignalSlots` keeps reporting it;
- the arrays keep the dead cell;
- a signal whose last slot disconnected does not return to `data === null`.

Three upstream cases observe this. They are ported in `tests/generated_signals_test.cpp` and reported as
known defects rather than asserted. With the two tests corrected to the shape above in a scratch copy of
`slot.hpp`, all three pass and nothing else regresses.

Other SDK modules that null-test indexed elements of nullable-element arrays are expected to be affected
the same way. They were not enumerated here.

## Workaround carried

[`compiler-indexed-element-presence-test.patch`](../../source-patches/compiler-indexed-element-presence-test.patch)
adds the two-layer lane to `emitCppPresenceTestCpp`. It covers an element read through a runtime array,
written directly or retained by an immutable array-element binding. The outer layer is tested as the index
miss (`undefined`) and, when the element type stores a sentinel as optional absence, the inner layer as that
sentinel. An element type with no absence of its own has only the outer layer, so strict `null` is
type-decided. RegExpExecArray captures and typed arrays keep their own lanes.

A binding's declaration owns its storage, and two emission repairs flatten a declared single-absence binding
to one layer after emission (`signals-flatten-generic-slot-read`,
`animation-state-machine-flatten-channel-index-read`). So a binding's test dispatches at compile time on the
carrier it actually has, inside a generic lambda so that the branch not taken is never instantiated.

Verified three ways:
- against node on 90 cells: every operator above, crossed with direct reads, bound locals and null,
  undefined, callable and plain element arrays, crossed with in-range null, in-range present and
  out-of-range indices;
- by `cppIndexedElementPresence.test.ts` (11 cases, all failing without the patch);
- by the unchanged set of pre-existing backend failures.

`@flighthq/signals` is regenerated with the patch, and its three tombstone cases are asserted in
`tests/generated_signals_test.cpp`. Reverting either regenerated comparison fails them.
