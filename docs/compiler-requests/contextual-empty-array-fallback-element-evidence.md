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
