# Named union shapes through `Partial<T>`

The emitter refuses `Partial<T>` when `T` is a named object union, even though semantic type information
contains a closed object shape for every concrete union arm.

## Minimal TypeScript

```ts
interface Texture2D { readonly dimension: '2d'; source: object | null; version: number }
interface TextureCube { readonly dimension: 'cube'; sources: readonly (object | null)[]; version: number }
type TextureLike = Texture2D | TextureCube;

declare function createTexture(opts: Partial<TextureLike> & { dimension: '2d' }): Texture2D;
export function createVideoTexture(opts?: Readonly<Partial<TextureLike>>): Texture2D {
  return createTexture({ ...opts, dimension: '2d' });
}
```

## Expected C++

`Partial<TextureLike>` must be planned from the named union's concrete arms, with every member optional,
and the object spread must visit the active arm before the explicit `dimension` field replaces it:

```cpp
using TextureLikePartial = std::variant<
    flight::StructuralRef</* readonly partial Texture2D row */>,
    flight::StructuralRef</* readonly partial TextureCube row */>>;
```

Every supplied option and its source reference must survive the spread. The compiler must not erase the
options to `Any`, discard arm-specific fields, or invent defaults before the explicit overriding fields
are evaluated.

## Affected modules

`createVideoTexture` in `@flighthq/texture` is the current package-level
`cpp-partial-shape-unresolvable` refusal. The same mechanism also appears in generic configuration paths
such as particle validation.

At Flight `a627849`, `@flighthq/spritesheet-formats` measures the same refusal on two public serializer
parameters: `Partial<AsepriteDocument>` and `Partial<TexturePackerDocument>`, where each document alias
is the union of its array and hash schema arms. The refusal omits both complete serializer declarations.

## Carrying workaround

There is no general compiler patch in this repository. Some individual modules replace a partial mapped
type with a hand-written closed options interface when that interface is already their true API, but
`createVideoTexture` intentionally accepts the public `TextureLike` surface. Its generated module remains
refused until named mapped unions resolve to their per-arm object shapes.

The two spritesheet serializers carry a narrower source workaround:
`source-patches/spritesheet-formats-serializer-existing-shapes.patch` spells the already-equivalent
top-level partial union as one optional row whose `frames` field is the same array-or-record union and
whose `meta` field keeps the same owner. Internal helpers receive only that shared metadata field, which
is all they previously read. It changes no runtime value or accepted structural caller and expires when
the named union form plans those same rows directly.


## Bounded video constructor supply

The partial `texture-video-variant-and-construction` override now supplies the constructor
for readonly host rows and the closed four-arm union of readonly `RowPartial<RowOf<...>>`
option rows. Its typed snapshot preserves the declared fields and retained source owners,
reads represented own string fields before the entity symbol, then applies the forced
`dimension`, `source` and `version` fields. Host width/height callbacks run before the spread.
The native function-value test uses that exact readonly/partial-union signature; the pinned
TypeScript oracle executes the original constructor and every exercised helper declaration.

This is a constructor boundary, not a general mapped-union compiler implementation. Open
dynamic own-property spreads beyond those four declared shapes are not supplied. The raw
module remains refused, and canonical readonly carriers for the other video helper APIs
remain explicitly listed in the partial override metadata. Passing the native convenience
Texture tests does not promote those missing callable surfaces to complete.
