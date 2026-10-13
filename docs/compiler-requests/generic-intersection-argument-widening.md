# A generic argument that widens into an intersection parameter needs its conversion and template argument

Inside a generic function, passing a `NodeOf<Traits>` to another generic whose parameter is
`TransformNode<Traits> = NodeOf<Traits> & HasTransform` is ordinary TypeScript when `Traits extends HasTransform`:
the argument already carries every member the intersection names. The emitter writes the call bare -- no
template argument and no structural conversion -- and C++ cannot deduce `Traits` from the alias, so every
instantiation of the caller fails. The header is not refused, and the compile gate does not instantiate
templates, so it passes.

Measured at flight-compiler `2b687ea` with all 44 of this repository's compiler source patches applied, one
module per case, then explicitly instantiated with clang 22.

## Minimal TypeScript (measured)

```ts
export interface Node<Traits extends object = object> { kind: string; children: Node<Traits>[] }
export type NodeOf<Traits extends object> = Node<Traits> & Traits;
export interface HasTransform { x: number }
export type TransformNode<Traits extends object> = NodeOf<Traits> & HasTransform;

export function ensure<Traits extends object>(target: TransformNode<Traits>): void { target.x = target.x + 0; }
export function visit<Traits extends HasTransform>(node: NodeOf<Traits>): void { ensure(node); }
```

```cpp
template <typename Traits>
inline void visit(NodeOf<Traits> node) {
  ensure(node);
}
```

```text
template void visit<flight::Ref<HasTransform>>(NodeOf<flight::Ref<HasTransform>>);
traversal.hpp:40:3: error: no matching function for call to 'ensure'
```

## Required C++ shape

The callee's own type argument, and the conversion its parameter declares:

```cpp
ensure<Traits>(flight::structural_ref_cast<TransformNode<Traits>>(node));
```

## Affected Flight modules

`@flighthq/velocity` `transformVelocity.ts` `visitTransformVelocity`, which calls `ensureNodeWorldMatrix(node)`
and `getNodeWorldMatrix(node)` with a `NodeOf<Traits>` (`Traits extends HasTransform2D`). The shape is the one the
`velocity-transform-trait-traversal` source patch writes to remove the refused readonly-to-writable assertion.
Related, not the same: [heritage argument deduction](heritage-argument-generic-deduction.md) is about a caller
whose declared owner differs from the parameter's by name, where this caller is itself generic.

## Carrying workaround

None. `contributeTransformVelocity` is also blocked inside `@flighthq/node`
([node-transform-runtime-owner-argument](node-transform-runtime-owner-argument.md)), so a velocity-side override
could not be exercised; none is carried.
