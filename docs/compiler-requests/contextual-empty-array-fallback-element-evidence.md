# A contextual empty-array fallback must inherit the iterable element type

An optional typed array followed by `?? []` has one possible element type at a `for...of` site: the
declared element type of the present arm. The empty arm contributes no values. The emitter instead
plans the literal as an untyped `never[]`-like container and refuses the enclosing function because it
cannot choose native element storage.

Measured at Flight `a627849` / flight-compiler `2b687ea` in
`@flighthq/spritesheet-formats` `applySpritesheetImportOptions`:

```ts
interface Descriptor { kind: string }
interface Options { formats?: readonly Descriptor[] }
declare function bind(kind: string): void;

export function apply(options: Readonly<Options>): void {
  for (const format of options.formats ?? []) bind(format.kind);
}
```

The complete module is refused with:

```text
an empty array has no concrete element type; add an explicit T[] annotation or T[] assertion at the
construction site
```

The same source shape previously appeared in
`@flighthq/textureatlas-formats` `applyTextureAtlasImportOptions` as
`cpp-empty-array-element-type-unproven`.

## Required lowering

The present arm already supplies `readonly Descriptor[]`. The fallback can therefore use that element
carrier without inventing a value:

```cpp
auto formats = options.formats;
if (formats.has_value()) {
  for (const auto& format : formats.value()) bind(format->kind);
}
```

This is contextual typing, not a request for a universal native representation of `never[]`. An empty
array with no typed sibling or destination may still need an explicit source annotation.

## Carrying workaround

`source-patches/spritesheet-formats-explicit-formats.patch` binds the optional array, returns when it is
absent, and iterates the identical array when present. Absence still performs zero calls; presence keeps
the same references, order, and last-write behavior. The analogous texture-atlas patch carries the same
rewrite. Both expire when the empty fallback inherits its sibling's contextual element type.

## Separate snapshot boundary: an erased empty array

`@flighthq/snapshot` `ensureSnapshotContainer(existing, isArray): object` returns
`isArray ? [] : {}`. This is not the optional-fallback case above: the declared result deliberately
erases the concrete container, and later code narrows it with `Array.isArray` before treating it as
`unknown[]`. At the same pin the function is refused with the same empty-array diagnostic.

The required carrier is therefore the identity-preserving erased array view described in
`erased-array-identity-preserving-view.md`, not an element type borrowed from the object arm. Choosing
`Array<Any>` by fiat is only correct if that is the array's actual storage; copying another typed
array into it would change identity and break the restore/interpolation writes.

## G2 a `Map` present arm: DOM render-option merging (patch carried)

At Flight `a627849` / compiler `806d257`, `@flighthq/scene2d-dom` `mergeDomRenderOptions` spreads
`merged.X ?? []` into `new Map([...])` for four `ReadonlyMap<Kind, V>` fields. Here the present arm is a
`Map`, not an array. The emitter types the coalesce as the Map carrier but builds the `[]` arm as
`Array<tuple<String, V>>`, so `merge_dom_render_options.hpp` failed at lines 65, 68, 74 and 92 (GCC 15.2).

The source patch `scene2d-dom-merge-options-empty-map-fallback` respells each fallback as
`new Map<Kind, V>()` of the field's declared types. Each fallback is a fresh empty iterable consumed only by
the spread, so the resulting Map's keys, values and insertion order are unchanged.

Normal exact-pin `pkg:generate @flighthq/scene2d-dom` with the patch:
- the header now compiles (1 attempted, 1 pass, 0 fail), and an unused-header TU links against the runtime
  library;
- the scoped run's different private member order for `scene_2d_dom_pipeline.hpp` (the scope-dependent order in
  `docs/generation-state.md`) is not committed; that header still fails at its recorded, unrelated diagnostic.

The patch expires when the emitter lowers an empty fallback as its present arm's iterable kind.
