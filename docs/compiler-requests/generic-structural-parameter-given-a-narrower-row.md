# A generic structural parameter is passed an owner whose emitted row is narrower than the parameter's

**Family.** Structural view composition at a generic call site. **Sites:** one measured
(`@flighthq/effects` capture geometry); the shape is generic and likely to recur wherever TypeScript
passes a `Node2D`-like owner to a `<Traits>`-parameterised structural parameter.

## The call, and what it does at the pin

`packages/effects/src/effectCaptureGeometry.ts:17,20`:

```ts
source: Node2D,
...
computeNodeRootLocalBoundsRectangle(_bounds, source);
```

`packages/node/src/boundsRectangle.ts:73-76`:

```ts
export function computeNodeRootLocalBoundsRectangle<Traits extends object>(
  out: RectangleLike,
  root: Spatial2DNode<Traits>,
): void
```

The emitted call is

```cpp
flight::node::compute_node_root_local_bounds_rectangle(bounds, source);
```

with `source` emitted as `flight::types::Node2D`, and the callee emitted as

```cpp
template <typename Traits>
inline void compute_node_root_local_bounds_rectangle(flight::types::RectangleLike out,
                                                      flight::types::Spatial2DNode<Traits> root)
```

**Measured failure, at the pinned revisions:**

```
error: no matching function for call to 'compute_node_root_local_bounds_rectangle'
  @ flight/effects/effect_capture_geometry.hpp:65
```

It fails in three headers (its own, plus `contract.hpp` and `_internal_index.hpp` which include it).

## Why TypeScript accepts the argument

The projection is explicit in the source, and this is the expected identity-preserving behaviour:

```ts
// packages/types/src/HasBoundsRectangle.ts:26
export type Spatial2DNode<Traits extends object = NodeTraits> =
  NodeOf<Traits> & HasBoundsRectangle & HasTransform2D;

// packages/types/src/Node2D.ts:9,13-14
export type Node2D = Node<Node2DTraits> & Node2DTraits;
export interface Node2DTraits extends NodeTraits, HasAppearance, HasBlendMode,
  HasBoundsRectangle, HasClip, HasMaterial, HasTransform2D { ... }
```

`Node2D` supplies every constituent of `Spatial2DNode<Node2DTraits>`, `Traits` infers to `Node2DTraits`, and
overload resolution succeeds. Nothing is inferred that the source did not state.

## The two emitted views, measured

```cpp
// flight/types/node2_d.hpp:42 -- TWO components
using Node2D = flight::StructuralRef<flight::RowMerge<
    flight::RowOf<std::shared_ptr<flight::types::Node<flight::Ref<Node2DTraits>>>>,
    flight::RowOf<flight::Ref<Node2DTraits>>>>;

// flight/types/has_bounds_rectangle.hpp:152 -- FIVE components
template <typename Traits> using Spatial2DNode = flight::StructuralRef<flight::RowMerge<
    flight::RowMerge<flight::RowOf<std::shared_ptr<Node<Traits>>>, flight::RowOf<Traits>>,
    flight::RowOf<flight::Ref<HasBoundsRectangle>>,
    flight::RowOf<flight::Ref<flight::types::HasTransform2D>>,
    flight::RowOf<flight::Ref<entity_runtime_key_c3fe878ec1e8f9f2<Traits>>>>>;
```

The argument's row is missing three of the callee's: `HasBoundsRectangle`, `HasTransform2D`, and the
Traits-keyed `entity_runtime_key_<Traits>`. No implicit conversion exists between the two, which is why no
overload matches.

## What is measured and what is proposed -- these are different claims

**Measured:** the emitted call does not match any overload; the two emitted views have two and five
components; the argument's row lacks the three named components.

**Measured, and it corrects an earlier claim of mine:** the runtime's own projection already spans the gap
at the type level. This compiles against the emitted tree:

```cpp
const auto source = flight::types::Node2D{};
const auto projected = flight::structural_ref_cast<flight::types::Spatial2DNode<flight::types::Node2DTraits>>(source);
```

So it is **not** true that the differing row decomposition makes a cell projection impossible -- I asserted
that before testing it, and the test refutes me. A cell-wise projection is available; what the emission
lacks is the projection, not the ability to express one.

**Proposed, and needing compiler validation (not asserted here):** that the emitter carry an
identity-preserving structural projection at such a call site, or that `Node2D` be emitted as the
intersection it is (`Spatial2DNode<Node2DTraits>`, or a representation carrying the same cells). Which of
those is right for the emitter is a question for the compiler, and neither is proven from the generated
tree alone. What the generated tree does settle is the shape of the gap.

## What we do meanwhile

Nothing. The one measured site is left failing and recorded; no repair, no override, and specifically no
runtime cast fabricated to paper over it -- the runtime is not missing a cast, the emission is missing a
projection.
