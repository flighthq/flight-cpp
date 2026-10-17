# A readonly structural sequence stored as an owning array must keep its identity

A parameter typed as a readonly array of readonly rows is stored where an owning array is expected. In
TypeScript the stored value is the same array object. The emitter refuses: each element is a `RowOf`
view of an exact nominal referent, the view keeps the outer owner only behind opaque identity, and
recovering an owning `flight::Array<Ref<T>>` would copy the elements into a different array object.

## Minimal TypeScript

```ts
interface Entry { depth: number }
interface Source { frames: (readonly Entry[])[] }

export function createSource(frames: readonly (readonly Readonly<Entry>[])[]): Source {
  return { frames: frames as (readonly Entry[])[] };
}
```

`createSource(x).frames === x` holds in TypeScript.

## Required C++ shape

An identity-preserving projected-sequence carrier: the stored member keeps the caller's array owner and
exposes element views over it, rather than a copied owning array.

## Affected modules

- `createSwfTimelineSource` in `@flighthq/swf` (`swfDocument.ts`): refused, not supplied.
- `cffCharstring.ts` in `@flighthq/font-formats`: refused, supplied by override
  `font-formats-cff-charstring-structural-subroutines`.
- `expandRequirements` in `@flighthq/requirement-codegen` (`requirementCodegen.ts`): refused, supplied by the source
  patch `requirement-codegen-expand-requirements-nominal-copy`, which builds the accumulator from fresh objects
  copied field by field instead of aliasing the caller's row sequence and spreading a catalog row's sequence into
  it. The equivalence rests on the helper being private and its result being consumed once, by a caller that reads
  only `facet` and `key` and mints its own objects -- asserted in `tests/generated_requirement_codegen_test.cpp`,
  whose additive-expansion cases fail when the originals stop being carried through.
- `initializeRiveArtboardImportContext` in `@flighthq/scene2d-formats` (`riveImportRegistry.ts`): refused
  at the `readonly Readonly<RiveCoreObject>[]` store into the context's owning `objects` member; its
  retained `createRiveArtboardImportContext` caller keeps `rive_import_registry.hpp` and the three
  headers that include it failing on the omitted callee. Declared only by
  `scene2d-formats-declare-refused-rive-artboard-import-context`, so the callers compile and any call
  fails to link: the import context stays unbuilt rather than substituted, and the projected-sequence
  carrier is still owed here.
