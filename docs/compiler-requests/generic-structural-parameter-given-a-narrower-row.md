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

**Measured: the projection COMPILES but does not produce the parameter's row.** This corrects an earlier
claim of mine in both directions, and the second half matters more than the first.

```cpp
const auto source = make_node2d();   // the emitted construction path the repository's own tests use:
                                     // make_structural_ref<flight::types::Node2D::schema_type>(row_field<...>...)
const types::Spatial2DNode<types::Node2DTraits> projected =
    flight::structural_ref_cast<types::Spatial2DNode<types::Node2DTraits>>(source);
```

  - I earlier asserted a call-site projection "cannot rescue it", WITHOUT TESTING IT. That was wrong: the
    expression above compiles.
  - But the correction is not that it works. The compiler then reports, at the call,
      `candidate function template not viable: no known conversion from
       'flight::StructuralRef<flight::RowReadonly<flight::Row...'`
    -- the cast's RESULT is a read-only `StructuralRef<RowReadonly<...>>`, while the parameter is the
    five-component `RowMerge`. So the runtime's projection helper does NOT yield the parameter's row shape,
    and it is not a working workaround. Spelling the projection with the alias rather than `auto`, and
    passing the template argument explicitly, both change nothing: this is a row-shape gap, not a
    deduction gap.

**The cells are not there to project.** Built the emitted way, the owner carries the traits data cells
only. The three components the callee's row names are not data:
  - `RowOf<flight::Ref<HasBoundsRectangle>>` and `RowOf<flight::Ref<HasTransform2D>>` are TAG components:
    `HasBoundsRectangle extends Entity {}` is an EMPTY marker interface, and the bounds cell lives on
    `HasBoundsRectangleRuntime.boundsRectangle`.
  - `RowOf<flight::Ref<entity_runtime_key_c3fe878ec1e8f9f2<Traits>>>` is a Traits-KEYED runtime cell,
    while the emitted traits struct carries the ERASED `std::optional<flight::Ref<flight::types::EntityRuntime>>
    entity_runtime_key`.
This is the irreducible blocker for a workaround inside this repository: a lane fix would have to
synthesize three components the owner does not carry, which is fabricating cells rather than projecting
them. The fix is emitter-side, where the projection can be carried or `Node2D` emitted as the intersection.

**Proposed, and needing compiler validation (not asserted here):** that the emitter carry an
identity-preserving structural projection at such a call site, or that `Node2D` be emitted as the
intersection it is (`Spatial2DNode<Node2DTraits>`, or a representation carrying the same cells). Which of
those is right for the emitter is a question for the compiler, and neither is proven from the generated
tree alone. What the generated tree does settle is the shape of the gap.

## What we do meanwhile

Nothing. The one measured site is left failing and recorded; no repair, no override, and specifically no
runtime cast fabricated to paper over it -- the runtime is not missing a cast, the emission is missing a
projection.

## Re-measured at flight-compiler `806d257` (G2)

Unchanged apart from the line: the call is now `effect_capture_geometry.hpp:68`. Clang's candidate note
names the cause directly, `could not match 'flight::RowMerge' against 'flight::RowOf'`. The
`Spatial2DNode<Traits>` parameter is a `RowMerge` row and `Node2D` is a `RowOf` row, so `Traits` is not
deducible and the call has no viable overload.

It is G2 cluster `call-has-no-matching-56e47749`: `effect_capture_geometry.hpp`, plus `contract.hpp` and
`_internal_index.hpp`, which include it. There is no faithful local fix. A cast, a copied owner, or
substituting another bounds computation would each break the identity-preserving projection this
request asks for.
