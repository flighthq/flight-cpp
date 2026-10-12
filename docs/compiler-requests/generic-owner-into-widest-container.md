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
