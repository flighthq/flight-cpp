# A generic node row passed to an `object` parameter must pass its object

A `NodeOf<Traits>` is a merged structural row, `Node<Traits> & Traits`. Passing one to a parameter typed `object`
-- a WeakMap key, a registry source -- should pass the node itself. The emitter writes `node.shared_object()`,
which a merged row does not offer (its object type is `void`, because no single nominal type owns both halves),
so every instantiation of the caller fails. The header is not refused and passes the uninstantiated compile gate.

Measured at flight-compiler `2b687ea` with all 44 compiler source patches applied, then explicitly instantiated
with clang 22.

## Minimal TypeScript (measured)

```ts
const seen = new WeakMap<object, number>();
export function remember(source: object): void { seen.set(source, 1); }
export function rememberNode<Traits extends HasTransform>(node: NodeOf<Traits>): void { remember(node); }
```

```cpp
template <typename Traits>
inline void remember_node(NodeOf<Traits> node) {
  remember(node.shared_object());
}
```

```text
traversal.hpp:51:17: error: invalid reference to function 'shared_object': constraints not satisfied
```

## Required C++ shape

The row's owner holds the one object every view of the node shares; that object is the identity `object`
means:

```cpp
remember(node.shared_owner()->native_object());
```

## Affected Flight modules

`@flighthq/velocity` `transformVelocity.ts` `visitTransformVelocity`: `ensureVelocitySample(field, node)` keys
the field's samples by the node. Related: [structural-row-erasure-to-unknown](structural-row-erasure-to-unknown.md)
covers the same row entering `unknown`, which is refused rather than mis-emitted.

## Carrying workaround

None, for the reason given in [generic-intersection-argument-widening](generic-intersection-argument-widening.md).
