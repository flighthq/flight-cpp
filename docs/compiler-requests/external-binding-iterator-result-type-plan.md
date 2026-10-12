# Missing iterator-result type plan

An expression whose type is `IteratorResult<T>` cannot cross into C++. The emitter's runtime external
symbol binding plan has no type for `IteratorResult` or for `IteratorYieldResult`, so a direct
`map.keys().next()` fails the module at the binding stage before any expression is lowered:

```
cpp emission failed for .../glyphAtlasEntry.ts: runtime external symbol binding plan is incomplete
  (missing: IteratorResult[type], IteratorYieldResult[type])
```

The runtime already represents the value: `flight::IteratorResult<T>` in
`include/flight/iterator.hpp`, produced by `flight::MapIterator::next()` and
`flight::ArrayIterator::next()` (both of which are already bound — see `bindings/runtime.json`).
What is missing is the *plan*, not the carrier.

The two names must be decided separately, because they are not the same shape:

- `IteratorResult<T>` is a nominal contract type and maps cleanly onto the runtime struct
  `flight::IteratorResult<T>` (`std::optional<T> value; bool done;`). A `space: "type"`,
  `targetName: "flight::IteratorResult"` binding over `flight/iterator.hpp` is the whole of it.
- `IteratorYieldResult<T>` is an arm of the standard-library `IteratorResult` union, not a Flight
  type. It has no runtime counterpart today. Declaring it means either adding a runtime struct (an
  `IteratorYieldResult<T>` carrying the yielded value, with `IteratorReturnResult` for the done arm)
  or deciding that a yielded arm is expressed as `flight::IteratorResult<T>` and refusing the name.

Until that decision is made, no source spelling of "call `next()` and inspect the result" lowers,
whatever the receiver.

## Minimal TypeScript

```ts
export function firstKey(lru: Readonly<Map<number, boolean>>): number | null {
  const oldest = lru.keys().next();
  if (oldest.done === true) return null;
  return oldest.value;
}
```

Annotating the result (`const oldest: IteratorResult<number> = lru.keys().next();`) does not help: the
annotation is precisely what asks for the missing `IteratorResult[type]` binding. Leaving it inferred
is the earlier failure — the emitter re-derives an anonymous structural row that is incompatible with
the runtime carrier.

## Expected C++

```cpp
auto oldest = lru->keys().next();          // flight::IteratorResult<flight::Map<...>::Value>
if (oldest.done) return std::nullopt;
return oldest.value;
```

## Affected modules

`@flighthq/glyphatlas`'s `glyphAtlasEntry.ts` was the module that surfaced it (its LRU eviction reads
the first key), but the plan is shared: any emitted module that calls `next()` on a Map or Array
iterator is refused the same way, so the reach is every package that iterates a collection.

## Carrying workaround

No workaround is carried for the binding itself — the binding profile set is a whole-SDK decision and
was deliberately not changed package-scoped. Instead
[`glyphatlas-iterator-result-binding.patch`](../../source-patches/glyphatlas-iterator-result-binding.patch)
rewrites the one call site into target-neutral source that the current runtime already represents
(iterate `keys()` and return from the first iteration), which clears the refusal without a binding.

That patch expires as soon as this plan exists, and the direct `next()` form should be preferred
again then.
