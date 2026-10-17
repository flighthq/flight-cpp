# A readonly interface assertion should project from the retained owner

A callback can receive a readonly base interface and assert it to a readonly interface whose fields
are selected by a discriminator. When the C++ value still retains its structural owner, the assertion
should produce a checked readonly partial projection of that owner. It must not require native class
heritage or manufacture writable authority.

## Minimal TypeScript

```ts
interface Modifier {
  readonly kind: string;
}

interface MaskedModifier extends Modifier {
  readonly mask?: unknown;
}

export function signature(modifier: Readonly<Modifier>): string {
  const masked = modifier as Readonly<MaskedModifier>;
  return masked.mask !== undefined ? 'm' : '';
}
```

## Expected C++

The assertion should retain the input owner and expose only a readonly partial view of the asserted
row:

```cpp
using MaskedView = flight::StructuralRef<flight::RowReadonly<
    flight::RowPartial<flight::RowOf<flight::Ref<MaskedModifier>>>>>;
const auto masked = flight::structural_ref_cast<MaskedView>(modifier);
return flight::row_get<flight::RowKey<"mask">>(masked).has_value() ? "m" : "";
```

The projection neither changes object identity nor claims that the base C++ owner has nominal
heritage. A missing cell reads as absent, matching the unchecked TypeScript assertion followed by an
optional-property read. A projection must still refuse a write or an incompatible represented cell.

## Affected modules

`registerBuiltInModifiers.ts` in `@flighthq/shading` uses this pattern for animated-normal,
emissive, fog, and vertex-displacement signatures. Its dissolve signature asserts an anonymous
optional-map shape through the same readonly callback boundary. The emitter refuses all five
definitions, so the partial header and its two aggregate consumers do not compile without the
override.

## Carrying workaround

The `shading-register-built-in-modifiers-structural-assertions` override keeps each callback's
original owner and uses readonly partial projections for the specific interfaces. Its anonymous-map
probe reads the named cell without imposing a value carrier, preserving the distinction between
missing/undefined and a present texture variant. Remove the override when the compiler can emit these
checked readonly projections directly.

## G2 Lottie shape transform view and complete group walk

At Flight `a627849` / compiler `806d257`, `lottieShapeLayer.ts:33` refuses its group walker
at `transform as Readonly<LottieTransform>` with `cpp-type-assertion-unidentified`.
The readonly shape-item alternatives include LottieTransformShapeItem and the open Record arm,
but not LottieTransform itself. Source LottieTransformShapeItem extends the shape base and transform;
native LottieTransform and LottieTransformShapeItem independently derive ReferenceEnabled and repeat
the transform cells. A faithful readonly view must keep the original owner and missing/optional cells,
without native reinterpretation, copying a transform or granting writable authority.

The open LottieUnknownShapeItem has string ty plus arbitrary fields and is represented by Record;
it may carry ty='tr' and valid transform fields. The schema admits this arm as well as all thirteen
named item arms. An assertion only to the concrete transform arm or a guard selecting just native
LottieTransformShapeItem narrows that source domain. A replacement item array also loses the source
array identity rather than recovering its existing storage. The refusal's
suggestion to select the actual concrete alternative does not establish that the Record arm is absent.
No such source rewrite is carried. This is the existing retained-owner readonly projection obligation,
with the [JSON/dynamic member request](parsed-json-object-shape-reads.md) still relevant to record reads.

The complete original group walk and rendering contract remains required:

- The layer passes its original container/import context and layer.shapes, using a fresh empty array
  only for nullish shapes. Even an empty item list creates and appends a new named-or-null group.
  Find the first ty='tr' before the hidden-item filter and apply its original transform to the group;
  a hidden first transform can still be selected here. Do not select only the first visible transform.
- Each recursive group allocates one Shape, a fresh claims Set and fresh modifier/painter/path arrays.
  Its rerender closure captures those same live arrays and Shape. Handlers receive the same claim set,
  original item/import context and all these owners, so later animation updates/rerenders retain them.
  Do not replace the arrays, clone item/shape owners, move claims to global state or leak them across
  groups/documents. Existing native Shape factory/command/runtime headers supply real owner operations;
  the declaration repair supplies no new factory, runtime, callback or draw path.
- Walk items in source order; skip exactly hd===true. Recurse into a visible ty='gr' using its it and
  nm??null, then continue. A visible transform item reports expressions and continues. All other kinds
  look up the original registry's string-keyed handler, including custom/open kinds. Call a found
  handler with the shared context; otherwise report lottie.unsupported-shape-item / appendLottieShapeItems
  with {shapeType:item.ty}. Then report expressions for that non-group item. Do not add expression
  reports to hidden/group branches that continue, or silently drop unknown kinds to ease native reads.
- After the entire walk, run every actually pushed modifier in push order over that same paths array,
  including paths declared later than the modifier item. The layer does not impose a one-modifier cap.
  The current trim handler separately claims the group's first trim before checking animation, so its
  second trim is ignored; do not interpret the layer's older two-trim comment as overriding that policy.
  Preserve custom handler claims, mutation order and exceptions rather than composing suppressed trims.
- Render clears the same Shape's commands first. With no paths it returns; with paths and no painters,
  append all paths with null winding override (each path's own winding). Otherwise invoke every painter
  in push order with the same Shape/all current paths. The later rerender closure calls this render
  function without rerunning modifiers. Attach Shape to group only when initial paths.length>0, then
  always attach group to parent. Preserve recursion and child order, including empty groups.

The source explicitly documents that each local paint restates every local path. General Lottie
preceding-shape style scope, nested paint scope and reverse-style render order need a different stack;
this unit does not claim or introduce that behavior. Pinned source tests cover shape registration,
nested names, multiple paints and document paint order, path winding and per-group first-trim claims.
Those are requirements, not native Lottie shape/parser/drawing results here.

The [retained-callee evidence](retained-declaration-calls-omitted-callee.md#g2-lottie-shape-layer-retains-its-refused-group-walker)
qualifies a declaration only: unused include compile/link/run0, actual handler object compile0 then
exact absent-helper link1. Normal generation retains the original refusal/body; the owned header
compiles1/1 while the helper and caller remain unlinkable. No fake shape/parser/draw, copied owner,
empty-result stand-in, narrowed kind/input domain or native shape behavior is supplied.
