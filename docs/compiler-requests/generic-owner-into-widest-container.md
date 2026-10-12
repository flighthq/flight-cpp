# A generic owner entering a widest-signature container must keep its identity

A generic handle is pushed into a container typed for the widest instantiation of the same generic,
through an `as unknown as` cast. In TypeScript that stores the same object: the container element and
the caller's handle are one reference. The emitter refuses (`cpp-generic-owner-argument-assertion-unproven`),
correctly: `SignalConnection<T>` and `SignalConnection<(...args: any[]) => void>` are distinct C++
structs, so no conversion can produce the same object, and a copy would split identity and state.

## Minimal TypeScript

```ts
interface Handle<T extends (...args: any[]) => void> { slot: T; connected: boolean }
interface Bag { handles: Handle<(...args: any[]) => void>[] }

export function track<T extends (...args: any[]) => void>(bag: Bag, slot: T): Handle<T> {
  const handle: Handle<T> = { connected: true, slot };
  bag.handles.push(handle as unknown as Handle<(...args: any[]) => void>);
  return handle;
}
```

Afterwards `bag.handles[0] === handle`, and `handle.connected = false` is visible through the bag.

## Required C++ shape

The widest instantiation has to be an identity-preserving view rather than a separate struct: one
retained owner, reachable through both spellings. Two shapes would work:

- the container stores an erased reference (`flight::ErasedRef` or equivalent) to the exact
  `Ref<Handle<T>>`, with checked typed recovery and shared field access; or
- the widest instantiation of a function-parameterized generic is a view type that forwards to a
  retained exact owner, as `flight::StructuralRef` does for rows.

Either way, writes through one spelling are reads through the other, and reference equality holds.

## Affected modules

`connectSignalTracked` in `@flighthq/signals` (`connection.ts`) pushes into
`SignalScope.connections`. It is refused, and `scope.ts`, `contract.ts` and `index.ts` are
dependency-incomplete with it. Their headers compile; the refused function is absent.

## Why there is no workaround

Supplying `connectSignalTracked` by hand would have to store a different object in the scope, which
changes reference identity and lets the two copies' `connected`/`paused` state diverge. That is a change
to the semantics this runtime exists to preserve, so it is left refused until the representation exists.
Changing `SignalScope`'s element type is a `@flighthq/types` representation change, not a signals one.

## Investigation at the current base

Measured with all 44 compiler source patches applied (flight-compiler `2b687ea`, flight `a627849`).

**The scope push is the only blocker.** The rest of `connectSignalTracked` emits and behaves:
- `((...args: Parameters<T>) => …) as unknown as T` lowers to a `bind_callable_v1<T>` wrapper;
- natively that wrapper skips a paused connection and forwards otherwise.

The minimal reduction above still refuses with the same rule. Without the push it emits.

**Candidate fixes inside `@flighthq/signals` and the compiler alone each fail:**
- **A companion `SignalConnection<erased>` stored in the scope.** It splits `connected`/`paused` between two
  objects and breaks `scope.connections[0] === connection`, which upstream's scope tests assert with
  `toBe`.
- **`SignalConnection<T>` deriving from `SignalConnection<erased>`.** The field types differ (`T slot`,
  `Signal<T>` vs the erased carriers), so the base would carry a second copy of the state.
- **An emitter-only respelling.** No spelling of the push denotes the same object, because the two
  instantiations are unrelated C++ types.

**What would work, and where it lives:**
1. **A runtime identity-preserving erased owner view.** The widest instantiation would forward field
   access to one retained exact owner, and a generic function instantiated at the erased argument
   (`disconnect_signal_connection<erased>`) would dispatch back to the exact `T`. That is a flight-cpp
   runtime representation plus compiler support for generic owners, not a signals change.
2. **A different `SignalScope.connections` element type.** An erased handle with checked typed recovery,
   or one teardown callable per member, would let the scope keep the exact handle. That is a
   `@flighthq/types` representation change, or an upstream Flight API change.

Until one of those is chosen, `connectSignalTracked` stays refused and no override is carried. The
eleven connection cases and twelve member-registering scope cases in
`tests/generated_signals_test.cpp` remain excluded on it.
