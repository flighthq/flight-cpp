# An open 3D material path must retain each concrete material owner

Flight's 3D material family is intentionally open: vendor materials implement `Material3D`, while the sixteen
built-in interfaces extend it with their own maps, scalars and literal kinds. At Flight `a627849` and
flight-compiler `2b687ea`, the native path stores those values as the one flattened `Material3D` or `Material`
owner. A built-in GL renderer later asserts the value back to its concrete interface, but the base carrier no
longer proves which native owner was stored.

The current generated inventory records the direct result in every renamed built-in renderer. For example,
`packages/scene3d-gl/src/glStandardPbrMeshMaterialRenderer.ts:33` refuses an assertion to
`Readonly<StandardPbrMaterial> | null` against the sole retained `Readonly<Material>` alternative. The same
diagnostic names BlinnPhong, CustomShader, Depth, Emissive, ExtendedPbr, Lambert, Matcap, Normal, Phong,
Shaded, SpecularGlossinessPbr, Toon, Unlit, VertexColor and Wireframe. `glMeshProgram.ts:295` and
`renderGlScene3D.ts:378` independently refuse the base `Material` to `Material3D` assertion.

This is not the exact-union assertion defect by itself. Once a source union names the open base and concrete
arms, [the carried concrete-owner union rule](compiler-concrete-owner-union-row-assertions.md) can select the
one exact arm. The missing contract here is retaining those arms through the heterogeneous scene path instead
of collapsing them to one base owner first.

## Required native shape

The path needs one represented union whose open arm keeps vendor values accepted and whose redundant built-in
arms retain their native owners:

```ts
export type Material3DValue =
  | Material3D
  | BlinnPhongMaterial
  | CustomShaderMaterial
  | DepthMaterial
  | EmissiveMaterial
  | ExtendedPbrMaterial
  | LambertMaterial
  | MatcapMaterial
  | NormalMaterial
  | PhongMaterial
  | ShadedMaterial
  | SpecularGlossinessPbrMaterial
  | StandardPbrMaterial
  | ToonMaterial
  | UnlitMaterial
  | VertexColorMaterial
  | WireframeMaterial;
```

The open `Material3D` arm is required. Removing it would close Flight's vendor-extension contract.
`StandardMaterial` must **not** be substituted for it: at this pin `StandardMaterial extends Material2D`, with
the incompatible `MaterialDimensionKey` phantom. The null-subset fallback in `renderGlScene3D.ts` remains the
existing full-trailer `Readonly<Material3D>` value and enters the open arm.

## Exact current-pin replacement targets

The old patch named `MaterialValue` and pre-rename renderer files. Its current replacement is:

| current source | exact replacement |
| --- | --- |
| `packages/types/src/Material3DValue.ts` (new), `contract.ts`, `index.ts` | declare and export the open union above |
| `Mesh.ts`, `InstancedMesh.ts`, `Billboard.ts` | `(Material3DValue | null)[]` for the positional material arrays |
| `GlMeshMaterialRenderer.ts` | `bind` receives `Readonly<Material3DValue> | null`, not base `Material` |
| `Scene3DRenderProxy.ts` | retain `Readonly<Material3DValue>` in the per-draw proxy |
| `GlScene3DRuntime.ts` | type the pooled draw entry's material as `Readonly<Material3DValue>` and replace its `object` renderer, mesh, subset, matrix and color placeholders with their already-declared exact types |
| `packages/scene3d/src/{mesh,instancedMesh,billboard}.ts` | accept the same material-value arrays without copying them |
| `glMeshMaterialRegistry.ts`, `glMeshProgram.ts` | resolve and inspect the union directly; remove the `Material` to `Material3D` assertion |
| `renderGlScene3D.ts` | carry the union through subset resolution, pooled entries, bind runs, proxies and identity sort keys; keep the default value in the open `Material3D` arm |
| renamed GL renderers | change the bind boundary from base `Material` to `Material3DValue` so each existing concrete assertion selects its named arm |

The renderer set is exactly
`glBlinnPhongMeshMaterialRenderer.ts`, `glCustomShaderMeshMaterialRenderer.ts`,
`glDepthMeshMaterialRenderer.ts`, `glEmissiveMeshMaterialRenderer.ts`,
`glExtendedPbrMeshMaterialRenderer.ts`, `glLambertMeshMaterialRenderer.ts`,
`glMatcapMeshMaterialRenderer.ts`, `glNormalMeshMaterialRenderer.ts`,
`glPhongMeshMaterialRenderer.ts`, `glShadedMeshMaterialRenderer.ts`,
`glSpecularGlossinessPbrMeshMaterialRenderer.ts`, `glStandardPbrMeshMaterialRenderer.ts`,
`glToonMeshMaterialRenderer.ts`, `glUnlitMeshMaterialRenderer.ts`,
`glVertexColorMeshMaterialRenderer.ts` and `glWireframeMeshMaterialRenderer.ts`.

Unlike the stale patch, this replacement does not add optional `alphaMode` or `blendMode` capabilities to
base `Material`: current `Material3D` already requires `alphaCutoff`, `alphaMode`, `blendMode` and
`doubleSided`, and the 2D branch must stay distinct.

## Equivalence

Every redundant arm already extends `Material3D`, and the original open arm remains, so the accepted
TypeScript values are unchanged. All propagated changes are type annotations and erase in JavaScript. The
same material array and each material object keep their identities; constructors store the caller's array by
reference; pooled draw entries and proxies keep the same objects; registry lookup still uses the same `kind`;
sort keys still use the same renderer and material identities; bind and draw ordering is unchanged.

The fallback remains one module-level object with the same kind and complete 3D trailer. No 2D material is
admitted to a 3D mesh, no vendor kind is rejected, and no material is copied, cast, reallocated or moved to
side storage. Alpha classification and blend-mode selection read the same required `Material3D` fields.

## Focused tests

The source rebase must prove all three boundaries, not merely make one renderer emit:

1. **Storage identity.** Extend `mesh.test.ts`, `instancedMesh.test.ts` and `billboard.test.ts` with two
   different concrete built-ins plus a vendor `Material3D`; assert the material array and every entry retain
   reference identity. Null slots must still select the one shared full-trailer fallback.
2. **Exact owner selection.** Generate the three type headers, `glMeshMaterialRegistry`, `glMeshProgram`,
   `renderGlScene3D` and all sixteen renamed renderer roots. Assert each concrete binder selects its named
   `Material3DValue` alternative, while a vendor material remains in the open arm. No `Material` to
   `Material3D` structural assertion may remain.
3. **Observable GL behavior.** Run the existing registry, renderer and `renderGlScene3D` suites. Pin one bind
   for a shared material run, opaque-before-blended ordering, authored blend mode, object-alpha routing,
   pooled-entry reuse and exact proxy material identity. Those are the observable claims of the rewrite.

## Carrying workaround and status

`source-patches/material-concrete-value-path.patch` carries the older form against Flight `7e2fc7d`. It has a
`pendingRebase` marker for the GL lane and is not applied at `a627849`. The source-patch lane must regenerate it
from the targets above; this request deliberately changes neither the patch nor generated inventory. It expires
when heterogeneous base-interface storage preserves checked concrete native ownership, or when Flight adopts
the explicit open value union.
