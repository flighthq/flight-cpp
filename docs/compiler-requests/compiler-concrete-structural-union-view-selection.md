# Concrete owner entering a structural-view union

A concrete nominal reference is refused when passed to a union of readonly structural views, even
when exactly one view is proven from that concrete owner.

## Minimal TypeScript

```ts
interface Texture2D { readonly dimension: '2d'; version: number; runtime: object }
interface TextureCube { readonly dimension: 'cube'; version: number; runtime: object }
type Texture2DView = Omit<Texture2D, 'runtime'>;
type TextureCubeView = Omit<TextureCube, 'runtime'>;
type TextureView = Readonly<Texture2DView> | Readonly<TextureCubeView>;

function resolve(_value: TextureView): void {}
export function draw(texture: Texture2D): void { resolve(texture); }
```

## Expected C++

The exact structural proof selects the 2D arm and retains the source owner:

```cpp
resolve(TextureView{
    std::in_place_type<flight::StructuralRef<
        flight::RowReadonly<flight::RowOf<flight::Ref<Texture2DView>>>>>,
    flight::structural_ref_cast</* Texture2DView row */>(texture)});
```

No copied object, `Any`, or side storage is permitted.

## Affected modules

`glParticleEmitter3D`, `glMeshProgram`, `glShadowMap`, PBR material registration,
`renderGlScene3D`, and `scene3DGlPipeline` in `@flighthq/scene3d-gl`.

## Carrying workaround

[`compiler-concrete-structural-union-view-selection.patch`](../../source-patches/compiler-concrete-structural-union-view-selection.patch)
uses the existing exact row-widening proof to choose one structural destination arm. It expires when
the upstream compiler applies the same proof at concrete-to-view-union boundaries.
