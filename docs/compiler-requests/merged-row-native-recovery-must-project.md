# Native reference recovery from a merged row must project the row first

`@flighthq/node` `disposeNode<Traits>(target: Node<Traits>)` reads the target's entity runtime. The
emitter's generic body takes a merged `Node<Traits> & Traits` row and recovers the native node from it
directly with `flight::structural_ref_cast<std::shared_ptr<Node<Traits>>>(target)`. A merged row has no
single nominal object type, so the `structural_ref_cast` object branch trips the runtime's deliberate
assertion:

```
include/flight/structural_ref.hpp:1683: static assertion failed due to requirement '!std::is_void_v<void>':
  a merged structural row must be projected before native reference recovery
```

The assertion is correct: a merge describes an object without naming its C++ type, so recovery must go
through a projection that keeps the same owner.

## Minimal TypeScript

```ts
interface Node<Traits extends object> { kind: string }
type NodeOf<Traits extends object> = Node<Traits> & Traits;

declare function entityRuntime(target: object): NodeRuntime<Traits>;

export function disposeNode<Traits extends object>(target: Node<Traits>): void {
  const runtime = entityRuntime(target);
  runtime.parent = null;
}
```

## Observed C++

`@flighthq/node` `node.ts` emits the generic helper (`node.hpp:76-77`):

```cpp
template <typename Traits>
inline void dispose_node(flight::StructuralRef<flight::RowReadonly<flight::RowMerge<
    flight::RowOf<std::shared_ptr<flight::types::Node<Traits>>>, flight::RowOf<Traits>>>> target) {
  auto runtime = get_mutable_node_runtime<Traits>(
      flight::structural_ref_cast<std::shared_ptr<flight::types::Node<Traits>>>(target));
```

and `@flighthq/scene3d` `disposeNode3D` instantiates it at `scene_node_dispose.hpp:22` with
`std::shared_ptr<flight::types::NodeTraits>`. The instantiation trace names the chain exactly:

```
structural_ref.hpp:1683: a merged structural row must be projected before native reference recovery
  node.hpp:77:       structural_ref_cast<std::shared_ptr<Node<>>, RowMerge<RowOf<Node<...>>, ...>>
  scene_node_dispose.hpp:22: dispose_node<std::shared_ptr<NodeTraits>>
```

## Required C++ shape

Project the merged row to the nominal `Node<Traits>` row over the **same retained owner**, then recover:

```cpp
auto node_row = flight::structural_ref_cast<
    flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Node<Traits>>>>>>(target);
auto runtime = get_mutable_node_runtime<Traits>(
    flight::structural_ref_cast<std::shared_ptr<flight::types::Node<Traits>>>(node_row));
```

or obtain the native object from the merge's shared owner. Either way the one object and its control
block stay the same; no relaxed readonly, unchecked runtime recovery, copy, or narrowed domain is
wanted.

## Affected modules

- `@flighthq/scene3d` `sceneNodeDispose.ts` via `@flighthq/node` `disposeNode` -- one header,
  `scene3d/scene_node_dispose.hpp`, cluster `static-assertion-18059635`. Measured at `-j1`.

## Second, independent half

The caller substitutes the **default** `NodeTraits` instead of the inferred `Node3DTraits`
(`Node3D = Node<Node3DTraits> & Node3DTraits`), so even a projected recovery would not find the exact
owner. That substitution is
[heritage argument generic deduction](heritage-argument-generic-deduction.md); both halves are needed
before the header passes.

One follow-on family is also visible in the same instantiation chain: the hierarchy override's
`emit_signal` call does not match at `overrides/include/flight/node/hierarchy.hpp:227`, and it stays
after the recovery is fixed.
