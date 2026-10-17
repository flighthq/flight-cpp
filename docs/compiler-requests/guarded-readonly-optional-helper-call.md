# Guarded readonly optional rows must stay readonly across helper calls

After an `undefined` guard proves an optional readonly structural row present, passing that row to a
helper with the same readonly parameter type must preserve its const owner. The emitter currently
reconstructs the payload through a writable row first, which is an invalid const upgrade.

## Minimal TypeScript

```ts
interface Registry { readonly definitions: Map<string, number> }

declare function resolve(registry: Readonly<Registry>, key: string): number | null;

export function read(
  registry: Readonly<Registry> | undefined,
  key: string,
): number | null {
  if (registry === undefined) return null;
  return resolve(registry, key);
}
```

## Expected C++

The guarded payload already has the callee's readonly row type and should pass through unchanged:

```cpp
if (!registry.has_value()) return std::nullopt;
return resolve(registry.value(), key);
```

No `RowWritable` construction is valid or necessary. Rebinding a present optional payload does not
grant mutation rights to its referent.

## Affected modules

`getDefineSignature` in `@flighthq/shading` triggers the invalid conversion in
`flight/shading/get_modifier_define_key.hpp`; `contract.hpp` and `_internal_index.hpp` fail only by
including that header.

## Current-pin Scene3D GL native handoff

G2 cluster `variant-cast-ccfedad4` covers nine mesh material renderers at compiler
`806d257d2f4100de745cc97340e61c2f301b0f24` / Flight
`a62784923f3be814463286c9fb28edfe6ab15789`. Despite the cluster's original name, its first
diagnostic concerns a readonly structural row, not a variant conversion.

The pinned TypeScript declares `GlScene3DRuntime.activeMeshProgram: GlMeshProgram | null`.
`beginGlMeshDraw` stores its `Readonly<GlMeshProgram>` argument there; a renderer's `draw`
reads the slot once, returns on null, and passes the present program to `drawGlMeshSubset`,
whose program parameter is also `Readonly<GlMeshProgram>`.

The effective native storage is supplied by
`overrides/include/flight/types/gl_scene_3d_runtime.hpp`. Unlike the nominal reference in the
generated type header, it retains a readonly structural view of the actual program allocation:

```cpp
using ProgramRead = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::GlMeshProgram>>>>;
std::optional<ProgramRead> active_mesh_program;
```

That representation is deliberate: concrete family programs are unrelated native structs, and
the bind-to-draw slot must preserve their shared owner and lazy uniform-cache cells. The override's
manifest records this storage choice under `native-e2e-types-gl_scene_3d_runtime`.

The generated consumers still reconstruct the guarded payload through a writable row:

```cpp
auto program = get_gl_scene_3d_runtime(state)->active_mesh_program;
if (!program.has_value()) return;
draw_gl_mesh_subset(state,
    flight::structural_ref_cast<ProgramRead>(
        flight::StructuralRef<flight::RowWritable<
            flight::RowOf<flight::Ref<flight::types::GlMeshProgram>>>>(program.value())),
    proxy, geometry);
```

The required handoff, once native storage and consumer emission agree on that carrier, is:

```cpp
if (!program.has_value()) return;
draw_gl_mesh_subset(state, program.value(), proxy, geometry);
```

The runtime correctly rejects the intermediate readonly-to-writable construction, including when
both rows describe the same subject. Its existing structural-row tests assert that rejection.
Granting writable authority, recovering an unchecked nominal reference, or allocating a base-program
copy would break this contract. The readonly draw argument must keep the same allocation and cells.

This is the same guarded readonly helper seam as the request above, with an important provenance
distinction: the pinned source slot is nominal, while the effective native override slot is readonly.
The emitted call and represented storage must be reconciled together; this example does not claim
that the unpatched pinned source declares a readonly slot.

The affected generated `flight/scene3d_gl/` consumers and first cast lines are:

| header | line |
| --- | --- |
| `gl_depth_mesh_material_renderer.hpp` | 66 |
| `gl_emissive_mesh_material_renderer.hpp` | 82 |
| `gl_lambert_mesh_material_renderer.hpp` | 95 |
| `gl_matcap_mesh_material_renderer.hpp` | 83 |
| `gl_normal_mesh_material_renderer.hpp` | 71 |
| `gl_phong_mesh_material_renderer.hpp` | 118 |
| `gl_toon_mesh_material_renderer.hpp` | 106 |
| `gl_unlit_mesh_material_renderer.hpp` | 84 |
| `gl_vertex_color_mesh_material_renderer.hpp` | 76 |

All nine remain G1 failures. This request update changes no runtime, binding, source patch, generated
header or override, so no header compilation was repeated. The shading workaround below is not
applied to this draw path: copying or inlining the renderer's GL and cache operations is not a bounded
replacement for transporting the existing program owner.

## Carrying workaround

[`shading-explicit-reference-storage.patch`](../../source-patches/shading-explicit-reference-storage.patch)
reads `registry.definitions.get(kind) ?? null` after the same guard. That is exactly the body of
`resolveModifier`, so it preserves one lookup, the returned definition's identity, and the null miss
sentinel while avoiding the broken helper boundary. Remove the direct lookup when the guarded payload
can pass to an identically readonly parameter without a writable round trip.
