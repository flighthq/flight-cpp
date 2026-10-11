# Structural rows as WeakMap keys

The compiler permits only `flightReference` keys for native `WeakMap`. A structural row is also a
reference carrier: the runtime weak-key policy weakens its retained object and restores the same
identity.

## Minimal TypeScript

```ts
interface Node<T> { runtime: T }
type Node2D = Node<{ x: number }> & { x: number };
type Node3D = Node<{ z: number }> & { z: number };
interface Cache { revision: number }
type Renderable = Node2D | Node3D | Cache;
export interface State { caches: WeakMap<Renderable, Cache> }
```

## Expected C++

```cpp
flight::WeakMap<Renderable, flight::Ref<Cache>> caches;
```

Each structural key arm must use the runtime's `DefaultWeakKeyPolicy<StructuralRef<Row>>`; arrays,
maps, tasks, and unrelated external handles remain refused without their own policy.

## Affected modules

`RenderState`'s `WeakMap<Renderable, ...>`, `renderProxy`, and the Scene2D/Scene3D render traversals.

## Carrying workaround

[`compiler-structural-row-weak-keys.patch`](../../source-patches/compiler-structural-row-weak-keys.patch)
accepts only the planner's proven `structuralRow` reference category. It expires when upstream elects
the existing runtime weak-key policy for structural rows.
