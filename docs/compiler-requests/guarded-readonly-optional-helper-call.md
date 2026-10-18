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

## G2 shaded material follow-on

The scoped original two-header modifier check now clears the old missing resolver and measures
**1 pass / 1 fail**, Clang22.1.8, one worker. `gl_shaded_prelude.hpp` passes;
`gl_shaded_mesh_material_renderer.hpp:169` reaches the same readonly active-program handoff above:
its retained draw callback reads the effective optional readonly program, guards absence, and then
constructs a writable `GlMeshProgram` row before calling readonly `draw_gl_mesh_subset`.
The first fatal diagnostic rejects exactly that writable construction. The existing native program
storage and its shared uniform/cache cells must pass through with readonly authority.

This adds one actually measured consumer to this request without rerunning or changing the nine
historical renderer rows/clusters. The modifier prelude is restored with full owner domains, real
shader strings and only a declaration for its refused Map guard; that separate
[constructor-evidence gap](readonly-map-instanceof-member-evidence.md) still prevents a positive
resolver/shader execution claim. No renderer override, mutable conversion or copied owner is carried.

## G3 readonly argument source projection

The historical batch-two membership is exactly 16 Scene3D GL rows: 14 routed through the guarded
helper request and two through the readonly-to-writable request. The package declares no
`flight.environment`, so all 16 are applicable; these historical failures are not a measured gain.

The raw pinned backend independently reproduces a shared cause before repairs: when flow evidence
names a nominal subject but a contextual helper parameter is readonly, the general conversion first
constructs a writable view of that subject. The actual native active-program slot already retains a
readonly view, so that intermediate claims authority it does not hold. A readonly helper needs only
a readonly source projection. The original nominal source slot and the effective native readonly
slot remain distinct provenance, as described above.

[`compiler-readonly-contextual-source-projection.patch`](../../source-patches/compiler-readonly-contextual-source-projection.patch)
requests that readonly source view before the existing structural conversion. It preserves the
source expression's single evaluation, the original owner and live cells, destination shape/storage
checks, guard and absence behavior. Writable destinations keep their existing path. Builder3's
already-landed exact present-readonly return/binding fast path remains unchanged. No runtime
relaxation, nominal recovery, caller cast, copied owner or narrowed source domain is introduced.

Five focused backend groups pass, including imported calls, guarded nominal slots, base reads,
single evaluation and distinct readonly/writable contexts. The four existing present-readonly-return
groups pass. The independently transpiled raw `806d257` backend and a separately built preceding
72-patch backend reproduce the writable intermediate on the same frontend IR; this is an emitter
origin spotcheck, not a full raw compiler build. Writable and invalid-readonly-call controls retain
byte-identical output. The qualification below covers one scoped normal generation and the original
public consumer; it does not establish a 16-header gain or complete renderer/package coverage.

The declared 73-patch compiler stack applies and builds. Its nine focused groups pass. A native
fixture matching the existing readonly bind-to-draw storage compiles, links and runs with the emitted
candidate: original row/native owner and control block, live signed-zero/NaN cells, null/undefined
guards, attachment replacement, single evaluation, exact thrown owner and final release are checked.
The preceding compiler's output fails at the writable intermediate; the readonly-to-writable negative
remains rejected. The writable-call and invalid-readonly-call emitted controls remain byte-identical.

The compiler static check on the 73-patch floor retains four preexisting failing gates: formatting, lint, import ordering
and five foreign fixture type diagnostics. Touched-file lint and the new test's formatting pass. The
source archive initially lacked Git index context for documentation/license enumeration; both gates
pass with the pinned index used read-only against that archive. No dependency checkout is modified.

After the clean base update, the combined 76-patch compiler stack also applies, builds and passes
all nine focused tests (five helper-projection tests and four existing present-readonly returns).
Reversing all 76 patches after those checks restores all 1784 tracked source files exactly to the
clean raw pin and removes every added patch source file. This does not require a second SDK run.

One coordinated normal `node scripts/pkgGenerate.mjs @flighthq/scene3d-gl --progress` run exits zero.
The owned package remains 64/69 modules emitted with five existing refusals; 51 headers change through
normal generation, and the inventory remains uniformly pinned to Flight `a627849`. The temporary
closure reports 1539/1643 modules emitted across 36 packages and 104 refusals; those are closure
counts, not a package or whole-SDK qualification. The independent Scene2D GL viewport source-patch
warning remains. No generated C++ is hand-edited. All 16 historical present-program call sites now
retain readonly intermediates by content audit; they have not been counted as 16 native header passes.

The existing `flight_cpp_generated_gl_readonly_present_tests` consumer compiles and links with the
full original generated `gl_custom_shader_mesh_material_renderer.hpp`, including its original bind
and draw bodies. Its registered `flight_cpp.generated_gl_readonly_present` CTest passes (1/1).
Alongside the existing real SDL GL presentation checks, the public source registration/getter path
checks absent lookup, the same readonly row and native owner/control block, live original vertex
cells, last-write lookup after re-registration, and retained views/persistent registry snapshots.
No caller cast, narrowed source domain or replacement renderer is used. This runtime test does not
execute renderer bind/draw; the separately emitted native fixture checks the guarded argument seam.
It does not qualify the full renderer or package.

After generation, `npm run overrides:check` reports 227 declared overrides, 13 source drifts, zero
unresolved entries and zero unmanifested headers. The 13 retained native Scene3D GL copies require
separate source audits; no override body or digest is changed in this compiler task. The result is
not a global override-integrity pass. Existing refusal, renderer and package debts remain explicit.
