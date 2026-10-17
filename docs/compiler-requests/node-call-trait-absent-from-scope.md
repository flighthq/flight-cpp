# A node-transform call needs a traits argument its enclosing function never declares

`@flighthq/render`'s `renderTarget.ts` and `renderTransform2d.ts` each emit a node-transform call whose
`Traits` type argument the enclosing C++ function never declares. One cause, two faces:

- **Face A, deduction failure.** The emitter drops the `structural_ref_cast<...Node<Traits>>(...)` wrapper
  it emits elsewhere, so an unnarrowed `Node2D` row is passed to a parameter declared
  `Transform2DNode<Traits>` — a `RowMerge` over `RowOf` — and `Traits` cannot be deduced.
- **Face B, undeclared identifier.** Here the emitter *does* emit the cast, but writes the bare name
  `Traits` inside a function that is **not a template**, so it refers to nothing. This face is **two
  symbols, not one** — `getNodeLocalMatrix` *and* `getNodeLocalTransformRevision`. Measured by builder7 on a
  scratch emission: once the matrix call is given its trait, line 67 remains a bare-`Traits` error on the
  revision call. A repair that fixes only the first symbol leaves the header failing.

Both were introduced by the same regeneration: `d1fdb0bb` (Migrate the registry renderer group, 11 rows).
This is the newest emission, not a stale one.

## Upstream shape

`packages/render/src/renderTarget.ts`:

```ts
export function computeScene2DRenderTargetTransform(
  outRenderTransform: MatrixLike,
  source: Node2D,                        // unnarrowed
  bounds: Readonly<RectangleLike>,
  contentX: number = 0,
  contentY: number = 0,
): void {
  const localTransform = getNodeLocalMatrix(source);
  inverseMatrix(_tempInvLocal, localTransform);
  // ...
}
```

`packages/render/src/renderTransform2d.ts`:

```ts
// updateRenderProxy2DTransform -- emits to render_transform2d.hpp:67
const localTransformId = getNodeLocalTransformRevision(data.source as Node);

// recalculateRenderTransform2D -- emits to render_transform2d.hpp:55
function recalculateRenderTransform2D(state: RenderState, data: RenderProxy2D, parentData?: RenderProxy2D): void {
  const transform2D = getNodeLocalMatrix(data.source as Node & HasTransform2D);
  // ...
}
```

Note the two calls take *different* TS argument types — `Node` for the revision and `Node & HasTransform2D`
for the matrix — and the emitter writes the bare name `Traits` for both.

TypeScript infers the trait in both cases — from `Node2D` in the first, from the `Node & HasTransform2D`
intersection in the second — and the emitter has no C++ name to write for what TypeScript inferred.

## Emitted (flight-compiler `2b687ea` with the carried patches, flight `a627849`)

**Face A** — `generated/include/flight/render/render_target.hpp:96`, inside
`compute_scene2_drender_target_transform(..., flight::types::Node2D source, ...)`, a plain function:

```cpp
flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Matrix>>>> local_transform = flight::node::get_node_local_matrix(source);
```

```
render_target.hpp:96:115: error: no matching function for call to 'get_node_local_matrix'
node_transform2d.hpp:173:102: note: candidate template ignored:
    could not match 'flight::RowMerge' against 'flight::RowOf'
```

**Face B** — `generated/include/flight/render/render_transform2d.hpp`, TWO symbols across `:55` and `:67`,
inside plain functions (`recalculate_render_transform2_d` and `update_render_proxy2_dtransform`):

```cpp
// line 55 -- getNodeLocalMatrix
flight::node::get_node_local_matrix(flight::structural_ref_cast<flight::types::Transform2DNode<Traits>>(flight::StructuralRef<flight::RowWritable<...>>(...)))

// line 67 -- getNodeLocalTransformRevision
flight::node::get_node_local_transform_revision(flight::structural_ref_cast<flight::types::Transform2DNode<Traits>>(flight::StructuralRef<flight::RowWritable<...>>(...)))
```

```
render_transform2d.hpp:55:207: error: use of undeclared identifier 'Traits'
render_transform2d.hpp:67:222: error: use of undeclared identifier 'Traits'
```

Note that the second face is the one that rules out a runtime-only remedy: the defect is an identifier in
emitted text, and no overload can declare it.

## The control, which makes this a defect and not a limitation

`generated/include/flight/node/bounds_rectangle.hpp:745` emits the **identical** call correctly, and that
header compiles clean:

```cpp
flight::StructuralRef<...Matrix> child_local = flight::node::get_node_local_matrix(flight::structural_ref_cast<flight::types::Spatial2DNode<Traits>>(child));
```

Its enclosing `merge_root_local_bounds` **is** `template <typename Traits>` with an `Spatial2DNode<Traits>`
parameter. So the emitter knows the shape; it applies it only where `Traits` is already in scope. Measured:
`c++ -std=c++20 -fsyntax-only -Ioverrides/include -Igenerated/include -Iinclude` on
`node/bounds_rectangle.hpp` exits 0, while the two `@flighthq/render` headers exits 1.

## How this differs from `node-transform-runtime-owner-argument.md`

They are a family — both concern a node-row or trait not surviving into an emitted node call — but they are
not the same defect, and the difference decides what can be done:

| | `node-transform-runtime-owner-argument.md` | this request |
| --- | --- | --- |
| what fails | the **callee's parameter** is a nominal `Transform2DRuntimeOwner<Traits>` that no object ever is, so `structural_ref_cast` can never produce one | the callee resolves; the **caller's scope** is wrong — the emitter writes a `Traits` the function never declares, or omits the cast so deduction fails |
| small source repair | the sibling request records there is **none**, measured 2026-10-13: retyping the helper refuses the `@flighthq/node` closure | **there is one**, and it is already demonstrated in this repository (below) |
| remedy | helper retyping or a runtime contract | an explicit trait at the source, or an emitter fix |

So this must not be folded into that request: a reader who tried its "no small source repair" conclusion
here would stop at a wall that is not there.

## Requested source workaround (explicit trait, following the carried precedent)

`particleemitter-node-matrix-direct-emitter.patch` (added by `29f905a3`, "Give particleemitter node calls
inferred traits") carries exactly this repair for the 3D/2D particleemitter calls: it rewrites

```ts
invalidateNodeLocalBounds(emitter);
```
to
```ts
invalidateNodeLocalBounds<Node2DTraits>(emitter);
```

— that is, it names explicitly the trait TypeScript had been inferring. The same shape applies to **all three
sites**, two files and two symbols:

```ts
// renderTarget.ts:58 -- getNodeLocalMatrix, unnarrowed argument (Face A)
const localTransform = getNodeLocalMatrix<Node2DTraits>(source);

// renderTransform2d.ts:30 -- getNodeLocalMatrix (Face B, matrix)
const transform2D = getNodeLocalMatrix<Node2DTraits>(data.source as Node & HasTransform2D);

// renderTransform2d.ts:12 -- getNodeLocalTransformRevision (Face B, revision)
const localTransformId = getNodeLocalTransformRevision<Node2DTraits>(data.source as Node);
```

Three sites total: one in `renderTarget.ts`, two in `renderTransform2d.ts`. The second Face B site is the
one an earlier draft of this document omitted, and it is not optional — builder7 measured that giving only
the matrix call its trait leaves line 67 failing on the revision call.

**This workaround is now carried, and it matches the three sites above exactly.**
`source-patches/render-node-call-explicit-traits.patch` (builder7, `baseCommit a627849`, `pendingRebase:
none` — so it applies at the live base) rewrites precisely these three call sites, adding the `Node2DTraits`
import. Earlier text here said no patch was carried; that was true when written and is not now.

Whether the emitter then writes the concrete trait rather than the bare name is confirmed by regenerating
the `@flighthq/render` closure — which the regeneration runs, not this document. I did not run one
(docs-only). The precedent's own evidence is that the rewrite is what made the particleemitter calls
instantiate.

### Equivalence

The rewrite only makes explicit a type argument TypeScript was already inferring, so it cannot change
observable behaviour: the same node is passed to the same helper with the same traits, the helper reads the
same runtime cell, and absence, reference identity, equality, ordering, exception shape and task settlement
are untouched. It adds no call, removes none, and reorders nothing. What it removes is the emitter's
freedom to guess — which is the only reason the emitted C++ differs at all. A version of this rewrite that
changed *which* trait is named would not be equivalent, which is why the specification names
`Node2DTraits` at all three sites rather than leaving the choice open.

## Focused reproductions

Both are single-header standalone compiles; neither needs the package built.

```sh
# Face A -- unnarrowed RowOf (Node2D) against a RowMerge parameter (Transform2DNode<Traits>)
printf '#include <flight/render/render_target.hpp>\nint main(){return 0;}\n' > /tmp/a.cpp
c++ -std=c++20 -fsyntax-only -w $CXXFLAGS -Ioverrides/include -Igenerated/include -Iinclude /tmp/a.cpp
#   render_target.hpp:96:115: no matching function for call to 'get_node_local_matrix'
#   note: could not match 'flight::RowMerge' against 'flight::RowOf'

# Face B -- `Traits` named in a non-template function
printf '#include <flight/render/render_transform2d.hpp>\nint main(){return 0;}\n' > /tmp/b.cpp
c++ -std=c++20 -fsyntax-only -w $CXXFLAGS -Ioverrides/include -Igenerated/include -Iinclude /tmp/b.cpp
#   render_transform2d.hpp:55:207 and :67:222: use of undeclared identifier 'Traits'

# Control -- the same call where the enclosing function declares Traits: exits 0
printf '#include <flight/node/bounds_rectangle.hpp>\nint main(){return 0;}\n' > /tmp/c.cpp
c++ -std=c++20 -fsyntax-only -w $CXXFLAGS -Ioverrides/include -Igenerated/include -Iinclude /tmp/c.cpp
```

**A compiler-side fixture is NOT the verification path here, and that is measured rather than assumed.**
The fixture harness cannot faithfully model this call, because `getNodeLocalTransformRevision` and
`getNodeLocalMatrix` are real `@flighthq/node` external bindings: both the implicit synthetic shape and the
explicit one stop at `cpp-runtime-external-symbol-binding-incomplete`. A fixture built either way therefore
fails for a reason of its own and would tell us nothing about the emission defect.

The verification is **actual scratch-emitted C++ plus the package tests**: emit the `@flighthq/render`
closure, compile the two headers standalone, and confirm that `get_node_local_matrix` /
`get_node_local_transform_revision` take a concrete trait and that `Traits` no longer appears at all. That is
also how builder7 found the second Face B symbol — by fixing the first and re-emitting, not by reasoning
about the shape.

If a fixture is ever wanted, it must assert the diagnostic and not merely that emission failed — Face A must
name `get_node_local_matrix` and the `RowMerge`/`RowOf` mismatch, Face B must name `Traits` as undeclared —
and the control must assert the same call emits **without** a diagnostic where `Traits` is a declared
template parameter. Until the external binding can be modelled, the scratch emission is the evidence and the
fixture is not.

## Gate obligation

`@flighthq/render` is `partial` with **26 of its 26 headers unattempted** and is **not deferred**
(`deferred-packages.json` has zero entries), so the header-compile gate never compiled these headers and
could not have reported either face. Whatever else changes, the fix is only durable with a
`npm run pkg:compile -- @flighthq/render` obligation: `pkg:tier` must move those headers from *unattempted*
to *independently compiled*, or the same regression can land again silently. This is a coverage gap in the
gate, not a property of the defect.

## Required C++ shape

The call must name a trait the emitted scope actually has. Either the emitter derives the concrete trait
from the source's inferred type and writes it (`Transform2DNode<flight::Ref<flight::types::Node2DTraits>>`,
`get_node_local_matrix<flight::Ref<flight::types::Node2DTraits>>(...)`), or it refuses the site instead of
emitting a call it cannot instantiate. Emitting a bare `Traits` in a non-template scope, or an unnarrowed
row against a `RowMerge` parameter, satisfies neither.

## G2 command: `getNodeChildCount(Readonly<NodeAny>)` (Face A)

At Flight `a627849` / compiler `806d257`, `command/command_binding.hpp:155` (`resolveInsertIndex`) emits
`flight::node::get_node_child_count(parent)`, where `parent` is
`StructuralRef<RowReadonly<RowOf<NodeAny>>>`. The template parameter is
`StructuralRef<RowReadonly<RowMerge<RowOf<Ref<Node<Traits>>>, RowOf<Traits>>>>`, so `Traits` cannot be
deduced (GCC 15.2: `no matching function for call to 'get_node_child_count(...)'`).

TypeScript infers `Traits` from `Readonly<NodeAny>`. The emitter neither writes that argument nor narrows
the row the way it does at other `Node<Traits>` call sites. This is the Face A shape above, and the required
fix is the same: write the inferred trait, or refuse the site. Measured on `command_binding.hpp`. Five more
command headers reach it through their includes: `command_history`, `command_transaction`,
`explain_command_dispatch`, `contract` and `_internal_index`. Their own first errors were not recompiled.
Nothing is carried, and the call is not narrowed or cast.
