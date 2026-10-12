# A reference-keyed WeakMap is emitted as a strong Map

The emitter lowers `new WeakMap<Subject, V>()` to `flight::WeakMap<Ref<Subject>, V>` for some modules and to
`flight::Map<Ref<Subject>, V>` for others. The second form retains every key it has ever held, so a subject
the source expects to be collected while the map still exists is kept alive by the module instead. The
runtime has both containers, so this is a lowering choice rather than a missing capability.

Measured at flight `a627849` / flight-compiler `2b687ea`, from two generated headers in the same inventory:

```cpp
// flight/connectivity/connectivity.hpp -- the WeakMap the source asked for
inline flight::WeakMap<flight::Ref<flight::types::Connectivity>, std::function<void()>> subscriptions = ...

// flight/clipboard/clipboard.hpp -- a strong Map for the same source shape
inline flight::Map<flight::Ref<flight::types::ClipboardWatch>, std::function<void()>> watch_subscriptions = ...
```

Both sources declare `new WeakMap<Subject, V>()` for a reference-shaped subject and a callable value:
`connectivity.ts`'s `_subscriptions` and `clipboard.ts`'s `_watchSubscriptions`. The difference is not the
value type, not the key's shape, and not the presence of an `erase` -- it is which lane each module's
declaration took, and nothing in either refusal ledger records it, because both forms compile.

## What it costs

Only lifetime. Every *observable* clipboard or connectivity behaviour is identical: routing is keyed by
reference identity either way, a detach erases the entry it holds, and a re-attach replaces it. What differs
is that a `ClipboardWatch` the caller has dropped, or a `Connectivity` entity it has released, stays alive
for as long as the module's table does, where the source's `WeakMap` would have let it go. A host that
creates and drops watches in a loop accumulates them.

## Required C++

`flight::WeakMap<Ref<Subject>, V>` -- one container, chosen by the `WeakMap` the source wrote, wherever the
key is a reference shape. The runtime's `WeakMap` already keys references weakly and already carries a
`std::function` value (the connectivity site above), so the repair is in the election, not the contract.

## Carrying workaround

None carried. `@flighthq/clipboard`'s behaviour is unaffected and its audit test says so in the bounds
section, distinguishing this from a missing capability; nothing in the repository asserts against it, so a
fix would not need those tests to move. Filed from a bounded evidence audit rather than from a refusal,
because there is no refusal here to point at.

## Distinguish from the requests already filed

[`compiler-structural-row-weak-keys.md`](compiler-structural-row-weak-keys.md) is about which keys may be
weak at all; [`nullable-reference-weak-map-value.md`](nullable-reference-weak-map-value.md) and
[`media-weakmap-value-representation.md`](media-weakmap-value-representation.md) are about the value side.
This is the container election for a key that is already a reference.
