# A contextual object return must use its declared nominal owner

A function with a declared interface return type returns an object literal containing exactly that
interface's fields. The emitter creates a module-local anonymous owner for the literal and returns it
from a function declared as `Ref<Nominal>`, producing invalid C++ without a compiler diagnostic for the
helper.

Measured at Flight `a627849` / flight-compiler `2b687ea` in both serializer metadata helpers in
`@flighthq/spritesheet-formats`:

```ts
interface Meta { app: string; image: string; size: { h: number; w: number } }

function dataToMeta(data: Data): Meta {
  return { app: 'tool', image: data.imageFile, size: { h: data.height, w: data.width } };
}
```

## Observed C++

The generated Aseprite helper has the equivalent shape:

```cpp
inline flight::Ref<flight::types::AsepriteMeta> data_to_meta(...) {
  return flight::make_ref<app_format_frame_tags_image_layers_scale_size_slices_version_...>(...);
}
```

Clang rejects the anonymous `Ref` as not convertible to `Ref<AsepriteMeta>`. TexturePacker emits the
same defect with its own anonymous metadata owner. The headers contain no diagnostic for these helpers;
their public serializer declarations are independently omitted by the named-union `Partial<T>` refusal.

## Required lowering

The function's contextual return type must select the declared nominal owner at the construction site:

```cpp
return flight::make_ref<flight::types::Meta>(flight::types::Meta{...});
```

All properties retain source evaluation order, and the single fresh object returned by JavaScript must
remain the single owner returned by C++.

## Carrying workaround

`source-patches/spritesheet-formats-serializer-existing-shapes.patch` binds each literal to its declared
metadata type immediately before returning it. The annotation erases in JavaScript and the local returns
the identical fresh object. The same patch removes a second anonymous-owner site by letting the internal
helper accept `undefined` directly instead of synthesizing `{}` at its call sites. It expires when
contextual return and call construction use their declared nominal owners directly.
