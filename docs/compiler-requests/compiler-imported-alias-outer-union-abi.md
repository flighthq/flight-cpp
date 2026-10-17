# Exact imported alias inside an outer union

An imported alias used as a nullable callable result is recursively flattened even when its defining
header and external bindings publish one exact native carrier. Distinct browser source types then
collide because the host has deliberately mapped them to the same handle.

## Minimal TypeScript

```ts
// provider.ts
export type HostImageSource = CanvasImageSource;
export type Resolver = () => HostImageSource | null;

// consumer.ts
import type { HostImageSource, Resolver } from './provider.js';
export function resolve(fn: Resolver): HostImageSource | null { return fn(); }
```

## Expected C++

```cpp
std::optional<provider::HostImageSource> resolve(provider::Resolver fn) {
  return fn();
}
```

The outer optional is preserved, while `HostImageSource` remains the exact carrier selected by its
declaring module. It must not reopen into browser alternatives.

## Affected modules

The Canvas texture resolver callback and cache result shared by `@flighthq/types` and
`@flighthq/scene2d-canvas`.

## Carrying workaround

[`compiler-imported-alias-outer-union-abi.patch`](../../source-patches/compiler-imported-alias-outer-union-abi.patch)
keeps an imported alias intact only when resolution reaches an exact external carrier or a target
collision. It expires when upstream union planning preserves exact imported ABI carriers.

## G2 DOM image callback: exact alias recovery must preserve source-member reads

At Flight `a627849` / compiler `806d257`, `scene2d-dom/domImageTextureResolver.ts:11`
retains this private callback's entire `Readonly<Texture>` input:

```ts
function resolveDomImageTexture(_state: DomRenderState, texture: Readonly<Texture>): CanvasImageSource | null {
  return (getTextureSource(texture) as Readonly<ImageResource> | null)?.source ?? null;
}
```

The current refusal is `cpp-contextual-union-inequivalent`: target `optionalSingle`
`flight::host_sdl::ImageSource` versus binary source `optionalSingle`
`flight::types::HostImageSource`. The latter is already an exact alias of the former;
its source alias is the entire `CanvasImageSource`, not an image-only subset. Preserve that
exact carrier and the outer null result rather than reconstructing browser variants.
The existing shared SDL handle retains identity and seven provider-kind values. This callback
borrows a supplied handle; it does not require a new URL decoder or DOM compositor.

A bounded candidate changed only the callback's type import/result annotation to `HostImageSource`.
The whole original/patched module emitted byte-identical JavaScript. Normal exact-pin generation
then emitted a body which selected `std::get<Ref<ImageResource>>` from the result of
`get_texture_source`, before making a readonly row and reading `source`. Both candidate headers
compiled, and real native execution preserved image/null results and the same handle identity
across all seven existing provider kinds. **The candidate was rejected:** invoking the installed
callback with a represented `Texture2D` whose source is `Bitmap` throws `bad_variant_access`.
The actual pinned original callback returns null for that source's absent `source` property.
Its TypeScript assertion does not introduce an owner check or change optional-member semantics.

The callback parameter and the published resolver type admit the full Texture domain; registering
under the image key does not narrow that callable signature. Source queries support 2D/3D and
first bound array/cube sources, including the existing open TextureSource owner and concrete
VoxelGrid/ExternalTexture/Bitmap/CompressedImageResource/RenderTarget/ImageResource arms.
Do not replace these inputs with ImageResource-only textures or treat an absent member as a
failed exact-owner extraction. Preserve the selected source and borrowed handle, source/query/member
read order, nullish fallback and getter/exception behavior. A faithful implementation needs a
represented source-member read across that declared domain, or an explicit refusal where the
representation cannot supply it; the rejected annotation/body is not carried.

The independent, carried `scene2d-dom-texture-table-carrier` annotation recovers only
`registerDomTextureResolver`'s persistent Map operation. It supplies no image callback or generic
`resolveDomTexture` body. The latter's distinct browser-domain refusal remains covered by
[image-source discriminator evidence](image-source-union-domain-discriminator.md).
