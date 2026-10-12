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

and `packages/layout/src/gridLayout.ts` at its own `as Readonly<GridLayoutContainerStyle> | null`.

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
