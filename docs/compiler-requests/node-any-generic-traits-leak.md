# An inferred `any` type argument leaks its parameter name into a readonly conversion

When a generic function takes `Readonly<Node<Traits>>` and is called with a `Node<any>` value, the
emitter infers `Traits = any`. It then omits the template argument at the call, and spells the
structural conversion of each argument with the unsubstituted parameter name `Traits`. That name is
undeclared in the non-generic caller, so the header does not compile.

## Minimal TypeScript

```ts
export interface Node<Traits extends object = object> {
  kind: string;
  traits: Traits;
}
export type NodeAny = Node<any>;

export function childIndex<Traits extends object>(
  source: Readonly<Node<Traits>>,
  child: Readonly<Node<Traits>>,
): number {
  return source === child ? 0 : -1;
}

export function capture(parent: NodeAny, child: NodeAny): number {
  return childIndex(parent, child);
}
```

At flight-compiler `2b687ea` with all 38 of this repository's compiler source patches applied (re-measured
after `compiler-readonly-view-argument-substitution` landed), `capture` emits:

```cpp
inline double capture(NodeAny parent, NodeAny child) {
  return child_index(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<
      flight::RowOf<flight::Ref<Node<Traits>>>>>>(
      flight::StructuralRef<flight::RowWritable<flight::RowOf<NodeAny>>>(parent)), /* child likewise */);
}
```

The required shape substitutes the inferred argument, exactly as the explicit form below already
emits: `child_index<flight::Any>(...)`, converting to `Node<flight::Any>`.

## Pinned boundary

| call | result |
| --- | --- |
| `childIndex(parent, child)` with `NodeAny` arguments, readonly parameters | leaks `Node<Traits>`; no template argument |
| the same with `NodeOf<Traits> = Node<Traits> & Traits` parameters | leaks both `Node<Traits>` and `RowOf<Traits>` |
| `childIndex<any>(parent, child)` | emits `child_index<flight::Any>` with `Node<flight::Any>` |
| `Node<Sprite>` arguments, inferred | emits `child_index<flight::Ref<Sprite>>` with the substituted row |
| `NodeAny` arguments, writable parameters (no conversion) | emits `child_index(parent, child)` |

The controls isolate the defect to the conjunction of an inferred `any` argument and a readonly
structural conversion. A concrete inferred argument is substituted. An explicit `any` is substituted.
A call that needs no conversion does not spell the parameter at all.

A concrete inferred argument is not always safe, though: when every argument reaches `Traits` only
through interface heritage (`Node2D`, `Sprite`), a checker-inferred concrete `Traits` also leaks, in the
readonly and writable forms alike. That trigger, with its own measured controls, is recorded in
[heritage-argument-generic-deduction](heritage-argument-generic-deduction.md); a fix should be checked
against both reductions.

## Affected Flight modules

`@flighthq/command` `command.ts` at flight `a627849`: `initializeRemoveNodeChildCommand` and
`initializeReorderNodeChildCommand` call `getNodeChildIndex(parent, child)` with `NodeAny` arguments.
The emitted `command.hpp` fails with `use of undeclared identifier 'Traits'` at both calls.

## Workaround carried

The `command-undeclared-node-helpers` override in `overrides/manifest.json` spells the two conversions
with `flight::Any`. That is the substitution the explicit `<any>` call already emits, and it makes no
other change to the calls. A source patch adding `<any>` at the two call sites would be equivalent. It
was not used because the override already exists to supply the refused `readNodeProperty`, and the
two edits expire together with it.

## Related leaks and how they differ

Three requests record a type-parameter name spelled where it is undeclared. They share the emission step
named as the residual boundary in
[compiler-generic-signal-row-projection](compiler-generic-signal-row-projection.md): when deduction leaves
a parameter undetermined, the conversion target is written with the parameter's name instead of being
refused. They do not share a cause. Each is a different reason deduction leaves the parameter undetermined,
so each needs its own deduction fix, and they are kept as separate requests.

Measured together in one scratch compiler at `2b687ea` with all 38 compiler source patches, against the same
`Node<Traits>` fixture (`interface Node2D extends Node<Node2DTraits>`, `interface Sprite extends Node2D`):

| argument evidence | call | result | request |
| --- | --- | --- | --- |
| `Readonly<Signal<T>>` parameter, `Signal<() => void>` argument | inferred | substituted (fixed by `compiler-readonly-view-argument-substitution`) | [signal row projection](compiler-generic-signal-row-projection.md) |
| `NodeAny` (`Node<any>`) into `Readonly<Node<Traits>>` | inferred | **leaks `Node<Traits>`** | this request |
| `NodeAny` into `Readonly<Node<Traits>>` | `<any>` | `child_index<flight::Any>` | control |
| `Node<Node2DTraits>` into `Readonly<Node<Traits>>` | inferred | `child_index<flight::Ref<Node2DTraits>>` | control |
| `Node2D`, `Sprite` (interfaces extending `Node<Node2DTraits>`) into `Readonly<Node<Traits>>` | inferred | **leaks `Node<Traits>`** | [heritage argument deduction](heritage-argument-generic-deduction.md) |
| `Node2D`, `Sprite` into `Readonly<Node<Traits>>` | `<Node2DTraits>` | `child_index<flight::Ref<Node2DTraits>>`, compiles | control |
| `Node2D`, `Sprite` into `NodeOf<Traits>` (`Node<Traits> & Traits`, writable) | inferred | **leaks `NodeOf<Traits>`**, the movieclip shape | [heritage argument deduction](heritage-argument-generic-deduction.md) |
| an unrelated owner (`Emitter<T>`) into `Readonly<Signal<T>>` | inferred | leaks, correctly undetermined | residual in [signal row projection](compiler-generic-signal-row-projection.md) |

The `any` row and the heritage row are distinguished by their controls. An explicit `<any>` fixes only the
first, and a literal `Node<Node2DTraits>` argument works while an interface extending it does not. So:
- teaching deduction to bind `any` would not fix the heritage leak;
- walking interface heritage would not fix the `any` leak;
- the residual change (refuse an undetermined parameter at emission) would turn both into refusals but
  make neither compile.

## G2: an omitted inferred `any` selects the default node runtime and breaks queue storage

At compiler `806d257` / Flight `a627849`, `renderQueue.ts:39` calls
`getNodeRuntime(current).children`, where `current` comes from the module scratch `NodeAny[]`.
The emitted `build_render_queue` instead explicitly selects the default trait owner:

```cpp
std::optional<flight::Array<flight::types::NodeOf<flight::Ref<flight::types::NodeTraits>>>> children =
    flight::row_get<flight::RowKey<"children">>(
        flight::node::get_node_runtime<flight::Ref<flight::types::NodeTraits>>(
            /* current projected as Node<Ref<NodeTraits>>, despite NodeAny storage */));
// The destination is flight::Array<flight::types::NodeAny>.
stack.element(stack_length++) = children.value().element(i);
```

Clang 22.1.8 reports `no viable overloaded '='` at
`flight/render/render_queue.hpp:131`. The child is a merged structural
`NodeOf<Ref<NodeTraits>>` row, while the stack element is the nominal
`shared_ptr<Node<Any>>` alias. These are not interchangeable C++ owners. The declaration of
`NodeRuntime<Traits>::children` is `optional<Array<NodeOf<Traits>>>`; the wrong runtime
instantiation has reached both the read and the local storage. This is the inferred-`any`
boundary above surfacing as a default-owner selection rather than an undeclared `Traits` token.

The source passes the existing child object into the scratch stack. Lowering must retain that same
node owner through the correctly substituted runtime/children carrier and the stack, including its
WeakMap lookup identity. It must preserve scratch-array reuse, reverse child pushes, preorder output,
and pruning of missing/invisible proxies. A permissive assignment between all structural rows and
nominal references, a copied `Node<Any>`, or a trait-owner reinterpretation would hide the mismatch.
The broader row/object boundary is related to
[generic-node-row-into-object-parameter.md](generic-node-row-into-object-parameter.md), but that
request's erased native-object projection is not a conversion to this nominal generic node type.

A focused TypeScript checker control using the pinned `Node.ts` generic declarations, with unrelated
non-generic imported names stubbed, resolves this call as `Readonly<NodeRuntime<any>>` and the child
array as `any[] | null`. This confirms source-side any inference; it is a type-only control, not a
new SDK emission or native fix.

The earlier explicit `<any>` control proves substitution for its reduced helper call, not that the
complete render-queue body and its child-to-stack boundary are repaired. No new compiler generation
or successful source-patch experiment is claimed here. No runtime assignment fallback or generated
body edit is carried for this row. It remains a compiler failure and links this existing request.

In the completed five-row G2 check, four other headers compile after their native storage/paint
repairs, but this render-queue row still fails at `:131`: **0/1 pass for this cause**. The row
and its new cause link this request; G1 sweep totals and all unowned rows remain unchanged.

## G2 Interaction spatial collection selects the default runtime owner

At compiler `806d257` / Flight `a627849`, `interaction/interaction_spatial_index.hpp:74` retains
`no matching function for call to 'collect_spatial_candidates'` in the original one-header cluster
`call-has-no-matching-8d9de718`. This docs-only audit retains **0/1**, the exact diagnostic and all
counts; no native or generation rerun is claimed. No override supplies this module.

Source `packages/interaction/src/interactionSpatialIndex.ts:66–82` takes a `NodeAny` and a shared
`NodeAny[]` output. Its recursive step reads `getNodeRuntime(node).children` and passes each existing
child back to the same collector, last child first. The native carriers expose the same default-runtime
selection already recorded for render-queue storage above:

```cpp
// types/node.hpp
using NodeAny = std::shared_ptr<Node<flight::Any>>;
template <typename Traits>
using NodeOf = flight::StructuralRef<flight::RowMerge<
    flight::RowOf<std::shared_ptr<Node<Traits>>>, flight::RowOf<Traits>>>;

// interaction_spatial_index.hpp:59,69,74 (abridged)
inline void collect_spatial_candidates(NodeAny node, flight::Array<NodeAny> out);
// get_node_runtime<flight::Ref<NodeTraits>>(...) reads:
std::optional<flight::Array<NodeOf<flight::Ref<NodeTraits>>>> children;
collect_spatial_candidates(children.value().element(i), out);
```

The recursive argument is a merged structural child row, while the parameter and output array use
nominal `Node<Any>` references. The generic source runtime accessor returns
`Readonly<NodeRuntime<Traits>>`, and its children are `NodeOf<Traits>[] | null`; selecting NodeTraits
from NodeAny changes both runtime and child carriers. Correctly substitute the source's inferred any
and preserve the original child through the recursive parameter, output array and manager cache.
The earlier explicit `<any>` reduction does not qualify this complete child-row-to-nominal path.
A permissive row-to-reference overload, a copied Node<Any>, an unchecked runtime cast, or restricting
the collector to Node2D would hide or narrow the actual owner boundary. No such fix is carried.

Read the original node's enabled cell and the same runtime's interactionState. A disabled node prunes
its subtree. Only `hitTestEnabled === true` opts the node in; an enabled candidate with a non-null
hitArea is appended once and its children are not visited. Otherwise visit the current children array
in reverse order, then append the parent if opted in. An ineligible parent still permits descendant
collection. The collector reads neither childrenHitTestEnabled nor clip/visible; this compiler
disposition does not add gates from the separate linear walk or claim full equivalence with it.

`refreshInteractionSpatialIndex<N extends NodeAny>` is separately NOT GENERATED at source line47,
`cpp-structural-assertion-owner-unproven`, on `nodes[rank] as Node2D` for world bounds. The existing
[Interaction graph-bounds owner request](assertion-to-undeclared-row-member.md#g2-interaction-graph-bounds-need-the-original-nodes-2d-capability)
applies: neither a corrected trait argument nor an erased cache proves the original node's transform
and bounds-runtime cells. Current native bounds access also keeps its explicit unavailable checked
HasBoundsRectangleRuntime boundary. No narrower manager API, replacement graph/runtime, fake bounds
or no-op refresh is supplied, and clearing line74 alone would not supply the refresh body.

Preserve the same `InteractionManager<N>` with root N and its optional SpatialIndex2D/runtime/backend.
A null index returns before collection or mutation. Otherwise allocate the fresh candidate array,
cache it under that manager's identity in `WeakMap<object, NodeAny[]>`, then clear the existing index.
For each ascending rank, read that node's current world rectangle, set the shared scratch AABB to
`(x,y,x+width,y+height)`, and insert the numeric rank. The backend copies bounds, so scratch reuse is
intentional. Declined insertion does not renumber the remaining candidates; cache replacement, clear,
bounds-provider effects and partial inserts before an exception retain source order.

Query returns null for no index or missing/empty cached candidates. Otherwise the same backend clears
and fills the shared query-output array. Of the returned ranks, only an existing node whose current
`hitTestNodeRegion(node,x,y,precise)` succeeds becomes best; later ranks greater than or equal to best
are skipped. Return the original N at the smallest successful rank, independent of backend output
order, or null. Preserve the precise default false, live hitArea/kind-provider dispatch and actual
2D coordinates; broadphase overlap alone is not a confirmed hit. Manager dispatch still checks its
enabled state and pointer capture before choosing this spatial path.

Inspected `interactionSpatialIndex.test.ts` has six source cases: hit/miss selection, topmost overlap,
null before refresh, opt-in changes, atomic hitArea and transform-change refresh. The native
`generated_interaction_test.cpp` tests the separate rectangle/circle `spatial_query.hpp` disabled-root
and explicit bounds-owner failure paths; it does not call this collector, refresh or ranked query.
No current native index/picking success is inferred from those fixtures. This unit updates only this
request, its original failed row/cluster and worklist103, preserving all C++/runtime/source/patch/repair/
override/manifest/pin inputs and foreign records.
