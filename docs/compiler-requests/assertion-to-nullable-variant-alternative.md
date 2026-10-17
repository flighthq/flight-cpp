# An assertion to `T | null` must name which alternative it means

Two modules refuse the same construct: a value of union type asserted to a declared shape that includes
`| null`, where the emitter has to choose exactly one C++ variant alternative and the target names more
than one.

Measured at `2b687ea`, flight `a627849`, from the package's refusal ledger.

## Minimal TypeScript

`@flighthq/layout`, `packages/layout/src/flexLayout.ts:27`:

```ts
const container = containerValue as Readonly<FlexLayoutContainerStyle> | null;
```

and `packages/layout/src/gridLayout.ts` at `as Readonly<GridLayoutItemStyle> | null`. Its separate
container assertion is non-nullable `Readonly<GridLayoutContainerStyle>`.

## Observed

```
cpp emission failed for @flighthq/layout/packages/layout/src/flexLayout.ts: type assertion target must
identify exactly one C++ variant alternative: target std::optional<flight::StructuralRef<flight::RowReadonly<
flight::RowOf<flight::Ref<flight::types::FlexLay…
```

The message is the assertion path's own counterexample to itself: the target is `std::optional<…>`, which
*is* one alternative — the optional's absent state — and the present state is the structural view the
source named. So the count that comes out greater than one is not the target's alternatives but the
emitter's reading of a nullable structural target as several.

## Expected behavior

`X | null` over a structural view lowers to `std::optional<StructuralRef<RowReadonly<RowOf<Ref<X>>>>>`, one
alternative with an absent state, and the assertion targets it. That is what the message's own `target`
spelling says; the refusal is about the selection, not about the type.

## Affected modules

`@flighthq/layout` — `flexLayout.ts` and `gridLayout.ts`, two of its three refused modules. The construct
(a nullable assertion of a layout style) is ordinary in this domain, so the reach is likely wider than
one package.

## Carrying workaround

None. `@flighthq/layout` is `partial`; the `layout-node-reference-bindings` patch it does carry answers a
different defect.

## G2 flex header boundary; nullable style owners remain refused

At Flight `a627849` / compiler `806d257`, normal scoped `flexLayout.ts` emission still refuses
`getFlexBase`, `getFlexShrinkScale` and the `flexLayoutResolver` binding at the source assertions to
`Readonly<FlexLayoutItemStyle> | null` or `Readonly<FlexLayoutContainerStyle> | null`. The emitted
`LayoutNode<>` stores optional `ErasedRef` styles; an assertion does not create an object of the named
nominal owner. No permissive cast, copied style owner or fake layout resolver is supplied.

The independent [basis validation comparison](mixed-value-union-literal-comparison.md) is now carried by
equivalent source guards. The existing `layout-declare-refused-flex-resolver` repair adds only
`extern flight::types::LayoutResolver flex_layout_resolver` for the actual explicit registration call.
The real unused-header program compiles, links and runs; explicit registration compiles to an object,
then fails to link on exactly that missing binding. The original one-header serial gate is **1/1 pass**,
but registration and layout resolution remain unavailable. The module is still partial and its refusal,
package counts and other module statuses are unchanged.

The guard-domain native check uses the exact anonymous style owner emitted by the validator; it does
not establish recovery of arbitrary source objects as that owner. Faithful nullable recovery must retain
the actual object and null state. The refused resolver must still validate container and item styles,
select row/column and reverse/wrap modes, preserve padding/gaps/justification/alignment, collect only
siblings, freeze and rescale shrink weights, and write the target child's four output cells in source
order. Failure sentinels and rejected-input writes must follow the original source; an empty resolver,
constant rectangle or fallback success would not close this request.

## G2 grid resolver keeps its own item and container contracts

At Flight `a627849` / compiler `806d257`, the grid resolver binding and `getGridColumn`, `getGridRow`
and `getGridAutoTrackSize` remain refused at `cpp-type-assertion-unidentified`. The actual nullable
assertions target `Readonly<GridLayoutItemStyle> | null` from the optional erased `LayoutNode<>.itemStyle`;
the resolver's container assertion separately targets non-null `Readonly<GridLayoutContainerStyle>`.
`isGridLayoutContainerStyle(null)` rejects, whereas the flex container validator accepts null. A flex
resolver body or copied style owner cannot stand in for these grid semantics.

The declared item contract allows null, requires explicit row and column to be jointly present or
absent, validates nonnegative integer positions and positive integer spans, and defaults spans to one.
Automatic placement counts only preceding siblings and selects row-major positions. The resolver checks
span bounds before reading layout dimensions, then preserves padding/gap order, fixed/auto/fraction
track sizing and the final x/y/width/height writes. Auto tracks subtract spanned fixed sizes and gaps
from the intrinsic contribution; fraction tracks share the nonnegative remaining space by weight.
All of these missing behaviors remain source obligations, not new runtime fallbacks.

`layout-declare-refused-grid-resolver` adds only the exact external `LayoutResolver` callback binding
for explicit registration. The [two independent retained boundaries](retained-declaration-calls-omitted-callee.md#g2-grid-track-guard-and-resolver-retain-two-distinct-omissions)
also include the independently refused track-list validator. No nullable owner recovery, resolver body,
style copy, narrowed tracks or successful layout result is supplied by either declaration.
