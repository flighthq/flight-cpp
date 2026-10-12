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

## Diagnosed in part: every isolated shape elects the weak container, this module does not -- and it stays open

Nothing about an isolated fixture reproduces this. Measured with the pinned compiler, one fixture per
shape, each a module-level `const registry = new WeakMap<Subject, V>();`:

| fixture | elected container |
| --- | --- |
| `Ref<Subject>` key, `() => void` value (the clipboard shape) | `flight::WeakMap<..., std::function<void()>>` |
| `Ref<Subject>` key, `(() => void)[]` value (the connectivity shape) | `flight::WeakMap<..., flight::Array<std::function<void()>>>` |
| `Ref<Subject>` key, `object` value | `flight::WeakMap<..., flight::ErasedRef>` |
| `Ref<Subject>` key, `number[]` value | `flight::WeakMap<..., flight::Array<double>>` |
| `Ref<Subject>` key, `string` value | `flight::WeakMap<..., flight::String>` |
| an IMPORTED interface as the key, function value | `flight::WeakMap<...>` |
| an imported key allocated through `allocateEntity`/`finishEntity`, `set` from a subscribe call, and a `detach` that reads, deletes and calls through | `flight::WeakMap<...>` |
| the exact clipboard sequence -- `detach` (read, `delete`, invoke) then `set` in `attach` | `flight::WeakMap<...>` |

Those are the focused control for this request: a reference key with a callable value, with a collection
value, with an object value, with an array value, with a scalar value, an imported key, an allocated entity
key, and the module's own delete-then-set sequence. **Every one of them elects `flight::WeakMap`.**

What is ruled out as the cause, then:

- the **value shape** -- five value types, from a bare callable to a scalar, all elect the weak container;
- the **key shape** -- a local interface, an imported interface, and a key allocated as an entity are the same;
- the **usage** -- `set` from a returned closure, a `detach` that reads/deletes/invokes, and delete-before-set
  are all the same;
- the **binding profile set** -- `@flighthq/clipboard` was regenerated onto the current set `070fadeb2a07`
  during this unit and its declaration is unchanged.

So the axis is inside this module and is *not* any of those four, and this request stays **open** with the
measurements above attached: an isolated fixture and the real module disagree under one pin, one profile set
and one compiler revision, and that pair is the reproduction a fix has to start from. Naming a smaller cause
than "something in this module" would mean guessing, and the guess would be wrong in one of those four
directions.

**The observability boundary, precisely.** A C++ module cannot be tested for collection: there is no
collector to run, `flight::WeakMap` stores its keys weakly as the runtime's own contract, and every accessor
(`get`, `delete_key`) needs a strong key to look one up with, so no test can ask "did the dropped key go
away". What *is* observable, and what the control asserts, is the container the emitter elected: for every
fixture above it is `flight::WeakMap`, which is the source's `WeakMap` and not a strong `Map` -- and for this
module it is `flight::Map`. Nothing here invents GC behaviour to demonstrate more.
