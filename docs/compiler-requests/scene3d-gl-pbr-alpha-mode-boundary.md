# PBR define-key construction needs only the alpha-mode scalar

`@flighthq/scene3d-gl`'s standard-block define-key helper accepts a whole
`Readonly<Material3D> | null` only to read `alphaMode`. Its three concrete callers hold
`StandardPbrMaterial`, `SpecularGlossinessPbrMaterial` or `ExtendedPbrMaterial`. TypeScript widens each derived
interface to `Material3D`; flight-compiler emits independent flattened reference owners, so after the material
value path retains those concrete owners there is still no identity-preserving C++ derived-to-base conversion
for this call.

This is the same missing heritage principle described by
[interface-heritage widening](interface-heritage-widening.md), but the narrow package fix does not need or
pretend to supply global C++ inheritance. The callee needs the closed `MaterialAlphaMode` value, not material
identity, so the exact local boundary is a scalar.

## Exact current-pin replacement targets

The stale patch names pre-`a4f7bffd6` renderer files. At Flight `a627849` the four targets are:

| current source | exact replacement |
| --- | --- |
| `packages/scene3d-gl/src/glPbrStandardBlock.ts` | retain `buildGlPbrStandardDefineKey(state, standard, material)` for existing callers; add `buildGlPbrStandardDefineKeyForAlphaMode(state, standard, alphaMode: MaterialAlphaMode)`; route both through one private implementation |
| `glStandardPbrMeshMaterialRenderer.ts` | pass `pbr?.alphaMode ?? 'opaque'` to the scalar entry point |
| `glSpecularGlossinessPbrMeshMaterialRenderer.ts` | pass `specGloss?.alphaMode ?? 'opaque'` with the already-converted standard block |
| `glExtendedPbrMeshMaterialRenderer.ts` | pass `extended?.alphaMode ?? 'opaque'` with `extended?.standard ?? null` and unchanged extension contributions |

The private implementation receives the already-resolved `baseColorMap` and alpha mode, then constructs the
same `GlPbrDefineKey`. It must remain the one owner of `alphaMaskEnabled`, the opaque alpha-map gate, all five
standard texture-readiness flags and the UV-transform gate. There must not be three copied define-key bodies.

The original object-taking entry point remains public and continues to read `baseColorMap` before
`material?.alphaMode`; existing callers and their source evaluation order do not change. The three scalar
callers read only plain material data. Those objects are Flight entities with data fields, not accessor-backed
host objects, so moving the side-effect-free alpha-field read to argument construction introduces no observable
getter order.

## Equivalence

Each renderer reads the same `alphaMode` at the same synchronous bind boundary and applies the same `'opaque'`
fallback for null. The shared implementation receives the identical standard block, base-color map and alpha
mode, calls `isGlTextureReady` on the same texture slots in the same order, and returns identical booleans.

Consequently the program-cache key, shader defines, shader source, texture bindings, UV transform, alpha-map
sampling, uniform values, extension contributions, cull state and draw calls are unchanged. No material is
copied or re-owned, and the scalar carries no reference identity. The standard, specular-glossiness and
extension-composed paths still select one program per identical feature set.

## Focused tests

1. **Shared helper equivalence.** In `glPbrStandardBlock.test.ts`, evaluate both public entry points for
   `'opaque'`, `'mask'` and `'blend'`, with a ready and unavailable alpha map. Assert complete key equality,
   not only `alphaMaskEnabled`; opaque must suppress `hasAlphaMap`, while mask and blend follow readiness.
2. **Three concrete callers.** Extend each renamed renderer test with a non-default alpha mode. Assert its
   selected `pbr:` cache key matches the object-taking helper's key and that null still chooses opaque. The
   ExtendedPbr case must retain its extension contribution segment, and SpecularGlossiness must retain its
   converted standard block.
3. **Focused generation.** With the concrete material-value predecessor applied, generate
   `glPbrStandardBlock` and all three renderer roots. Assert there is no concrete-PBR to `Material3D` row
   conversion and that all four declarations emit. Then compile the four generated headers before the wider
   Scene3D-GL gate.
4. **Order/control.** Instrument texture readiness in the standard-block test and assert each map is queried
   once in the existing order. This catches an implementation that duplicates the key body or resolves the
   alpha map on both sides of the new boundary.

## Carrying workaround and status

`source-patches/scene3d-gl-pbr-alpha-mode-boundary.patch` carries the same scalar-boundary design against
Flight `7e2fc7d`, but its three renderer paths were renamed and the patch has a GL `pendingRebase` marker at
`a627849`. The source-patch lane must regenerate those hunks after the material-value predecessor. This request
changes neither source patches nor generated inventory. It expires when emitted interface heritage supports
the identity-preserving derived-to-base conversion, or when Flight adopts the scalar helper upstream.
