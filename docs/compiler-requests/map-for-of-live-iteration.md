# A `for...of` over a Map must iterate it live, because its body may delete from it

JavaScript defines what happens when a `for...of` over a `Map` deletes from that map: an entry deleted
before the loop reaches it is skipped, the entry being visited may delete itself, and the walk continues
with the next entry in insertion order. The emitter lowers the loop to a C++ range-for over the map, which
walks `flight::Map`'s record vector directly; deleting from the map erases from that vector and invalidates
the loop's iterator. The emitted module compiles and is not refused. It fails at runtime, on the first
delete, with undefined behavior.

Measured at flight-compiler `2b687ea` with all 44 of this repository's compiler source patches applied, one
module per case, the emitted header then compiled with clang 22 and run.

## Minimal TypeScript (measured)

```ts
export function prune(map: Map<string, number[]>): number {
  let visited = 0;
  for (const [key, list] of map) {
    visited++;
    if (list.length === 0) map.delete(key);
  }
  return visited;
}
```

```cpp
inline double prune(flight::Map<flight::String, flight::Array<double>> map) {
  double visited = 0.0;
  for (auto array_pattern_value : map) {
    const flight::String key = std::get<0>(array_pattern_value);
    flight::Array<double> list = std::get<1>(array_pattern_value);
    visited++;
    if ((static_cast<double>(list.size()) == 0.0)) {
      map.erase(key);
    }
  }
  return visited;
}
```

Called with `a: [], b: [1], c: [], d: [2]`, JavaScript visits four entries and leaves `b` and `d`. The
emitted C++ aborts on the iteration after the first delete:

```text
panic: member call on null pointer of type 'std::vector<double>'
include/flight/array.hpp:535:67 in size
```

## Required C++ shape

The runtime already has the iteration JavaScript specifies: `Map::entries()` (and `keys()`, `values()`)
returns a live cursor keyed by insertion identity, which skips entries deleted before it reaches them,
survives the deletion of the entry it is on, and visits entries added before it is exhausted. Lowering the
loop onto it is the whole fix:

```cpp
for (auto array_pattern_value : map.entries()) {
```

With that one change the case above visits four entries and leaves two, as JavaScript does. A loop whose
body provably never mutates the map may keep the range-for; one that calls `delete`, `set` or `clear` on
the iterated map, or calls anything that might, needs the cursor.

## Affected Flight modules

`@flighthq/tween` `updateTweens.ts` `updateTweens`: the manager walks `manager.tweens` and deletes a
target's entry once its tween list empties. Any update pass that finishes the last tween of a target that
is not the final entry crashes the next iteration.

The same lowering is emitted for every `for...of` over a Map in the generated tree; this request does not
audit which of the other bodies mutate their map.

## Carrying workaround

The `tween-update-erased-target-writes` override walks `manager->tweens.entries()`. It is tested by
`generated_tween_test` ("deleting an emptied target mid-pass does not skip or repeat the targets after it",
and the repeating-timer case that crashed before the change).
