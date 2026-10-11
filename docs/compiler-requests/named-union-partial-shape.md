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

## Carrying workaround

There is no general compiler patch in this repository. Some individual modules replace a partial mapped
type with a hand-written closed options interface when that interface is already their true API, but
`createVideoTexture` intentionally accepts the public `TextureLike` surface. Its generated module remains
refused until named mapped unions resolve to their per-arm object shapes.
