# Readonly must retain a bound external value carrier and its member policy

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` / compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, `Readonly<GlContext>` loses the external
binding that `GlContext` itself uses. This is distinct from readonly nominal owner recovery,
readonly-to-writable authority, and the named-interface weak-map-key policy request.

## Exact source and observed emission

`@flighthq/scene3d-gl` `enableGlScene3DCustomShaderGuards.ts` exposes this reduced boundary:

```ts
import type { GlContext, GlRenderState } from '@flighthq/types/contract';
function enumName(gl: Readonly<GlContext>, type: number): string {
  return type === gl.FLOAT_MAT4 ? 'mat4' : `gl-type-${type}`;
}
export function reportType(state: GlRenderState, type: number): string {
  return enumName(state.gl, type);
}
```

`warnGlCustomShaderUniformTypes` reads the actual `state.gl` and calls this helper for the
reported uniform type and expected type. The generated helper instead takes
`StructuralRef<RowReadonly<RowOf<WebGl2Context>>>` and uses
`row_get<RowKey<"FLOAT_MAT4">>(gl)` (and six analogous uppercase keys). The caller's context is
`flight::host_sdl::WebGl2Context`, so the recorded first error at
`flight/scene3d_gl/enable_gl_scene_3d_custom_shader_guards.hpp:85` is no matching
`gl_uniform_type_name` (G2 `call-has-no-matching-35224a3f`, original one failing header).
There is no refused/missing helper or callback declaration to supply.

The native `types/gl_context.hpp` already aliases GlContext to WebGl2Context; `bindings/sdl-gl.json`
already binds WebGL2RenderingContext to that same shared value type. WebGl2Context's constants
are present (`float_mat4`, `float_mat3`, `float_mat2`, `float_vec4`, `float_vec3`, `float_vec2`,
`float_`). Its copyable view retains the existing shared SDL surface/context identity. It is not
a generated ReferenceEnabled owner that implicitly enters a StructuralRef. The generated generic
member table does already map the seven uppercase keys to their native constant members; that
lookup support does not supply the missing value-to-row carrier boundary. Inventing an owning
wrapper or allocating a different context is not a declaration repair. Preserve the existing
shared surface identity through the external carrier instead.

## Required lowering

Resolve Readonly through the declared external binding carrier, retaining its original owner and
read authority, and apply the same external member mapping as for GlContext. For this read-only
helper a native WebGl2Context value/const view and `gl.float_mat4` etc. suffice; no mutable structural
view, unchecked nominal recovery or change to the uniform/program/state carrier is needed. The
whole host domain and unknown-enum fallback remain. Refuse an unsupported bound view rather than
emitting an unrepresented structural row.

## Bounded carrying workaround

`source-patches/scene3d-gl-custom-uniform-context-carrier.patch` changes only the private helper's
parameter annotation from Readonly<GlContext> to GlContext. Its body, all callers and every public
signature are unchanged. The helper only reads seven enum constants and returns fixed names or
`gl-type-${type}`; it never writes/calls/escapes the context, compares identity or allocates a new
owner. Both annotations erase in JavaScript. This local spelling therefore retains exactly the
same runtime values and effects without granting any new operation in the implementation.

The pin/equivalence/expiry are recorded in source-patches/manifest.json. The workaround expires
when Readonly preserves the external carrier/member policy here, or Flight adopts this private
annotation. Generation and scoped header results are recorded with the owned inventory disposition;
source tests inspected below are requirements, not native live rendering proof.

## Preserve the entire guard contract

The enable function installs the same `(GlRenderState, WebGLProgram, string) => void` callback in
the original state-keyed GlScene3DRuntime slot, idempotently. Enabled means neither null nor
undefined; the nullable slot keeps opt-in logging out of the production draw dependency path.
This callback already matches the native `std::function<void(Ref<GlRenderState>, WebGlProgram,
String)>` alternative; there is no state/material/callback owner mismatch to cast away.

On invocation, the module WeakSet checks the original program identity, returns on a hit, and
adds a miss BEFORE querying uniforms (including when a later operation throws). Preserve that
weak identity/lifetime and ordering, not a shader-key-only or per-state replacement cache. Read
the same state.gl, ACTIVE_UNIFORMS count and each active uniform in order; skip null info, custom
names and matching types. Only the four built-ins are expected: u_model/u_viewProjection mat4,
u_normalMatrix mat3, u_cameraPosition vec3. Keep the seven name mappings and raw-enum fallback.

For each mismatch call logOnce with `scene-gl:custom-shader-uniform-type:${shaderKey}:${name}`,
Warn, the actual declared/expected type names and corrective declaration, category scene-gl.
The same program is not re-introspected on repeated binds; distinct programs still rely on logOnce's
existing shader/uniform deduplication. Do not rewrite the shader, coerce a uniform, suppress all
warnings, change enumeration/error ordering or fabricate a successful draw.

The custom material renderer invokes the nullable hook after resolving/compiling the same material
shaderKey/source and before beginGlMeshDraw, view/camera setup and custom uniform/texture uploads.
Missing/null material, empty key or missing source retains the active-program-null skip path. This
patch changes no material/mesh/proxy/camera ownership, program cache, shader source or draw behavior,
and makes no full-renderer capability claim.

The pinned source guard tests cover false-before-enable/true-after-enable, one corrective mat4-to-mat3
normal-matrix warning across repeated binds, silence for matching built-ins, and ignoring custom
uniforms. They use a fake GL source fixture and were read only; they are not new SDL/GL execution.

## G2 scoped result and limits

The annotation-only source patch is carried. Normal exact-pin scoped generation of scene3d-gl
completed: **64/69 modules emitted, 5 refused**, unchanged emission counts; its dependency closure
emitted 1536/1643 modules across 36 packages with 107 refusals. The generated helper takes
WebGl2Context and reads the same seven mapped native constants. The now-unused seven FLOAT keys
are removed from the generated SDK member lookup/type/binding/widening tables; no other key changes.
The generated manifest adds only this patch's digest/provenance. The guard callback, cache, logging,
material renderer and every public API/body expression remain unchanged.

The changed original-one header compiled with **Clang 22.1.8, one worker: 1 attempted / 1 pass /
0 fail / 0 unattempted, 116.3 s**. The old cluster is removed, with previousCluster and this request
retained on the passing row. Source-patch recovery tests passed **2/2**. An exact emitted-diff check
proves only the helper carrier/seven member reads/unused include and their unused SDK keys changed.
This is compile evidence, not native program introspection, log-sink, WeakSet lifetime, renderer or
pixel execution. That header check ran no link/live test; the helper-only execution below is
separate evidence and carries no broader package/shipping claim.

Scoped generation also produced unrelated FakeGl2 member/hash order and pipeline anonymous-hash
drift; both headers were restored, with that diff preserved in scratch. The run printed an existing
scene2d-gl-clip-rectangle-viewport-number-domain warning. That detector matches the refusal text
anywhere in the package, so this alone does not identify a clip-rectangle regression. No scene2d
header or patch is carried or rerun, and no global/scoped CURRENT claim is inferred. Evidence lives
under `/tmp/builder4-g2-custom-shader-guards/` (generate.log, headers-report.json,
source-patches-tests.log, rejected-foreign-generation-drift.diff). The compiler's dependency file
records 2,199 actual included files for post-rebase content validation without repeating the compile.

## Follow-up: full JavaScript erasure and actual native helper execution

Foreman aa23f750 requested this additional behavior evidence. The entire original pinned
`enableGlScene3DCustomShaderGuards.ts` module and the entire module with the exact carried
one-annotation patch were transpiled with the pinned TypeScript **5.9.3**, using Flight's
ES2022/ESNext options. Comments were retained; source maps and declarations were disabled as
separate, non-executable artifacts. Both transpiles reported **zero diagnostics** and produced
**byte-identical 3,825-byte JavaScript modules**, SHA-256
`ff82d9104b4e173827ce193001ab0a19a087ea0cca7b03b33af8a7b2496c490f`.
The comparison checked the full file, not an extracted helper or a subset of statements; it did
not execute the imported module or substitute a mocked guard.

`tests/generated_gl_registries_test.cpp` now directly calls the actual generated
`flight::scene3d_gl::gl_uniform_type_name`. Its seven native WebGl2Context constants must return
mat4, mat3, mat2, vec4, vec3, vec2 and float respectively. Three nonmatching inputs exercise the
raw-number default: TEXTURE_2D (3553) -> gl-type-3553, 0 -> gl-type-0, and -1.5 -> gl-type--1.5.
The helper reads static enum constants only, so this uses the actual native host value carrier's
default construction; no SDL surface, fake context, program or GL call is needed. It exercises
no uniform introspection, cache, log sink, renderer or pixel path.

The existing GL registries target and its CMake/Bazel registrations are reused, with all prior
registry assertions intact. After committing the test, the targeted **-j1 build/link passed** and
exactly one CTest run passed **1/1 in 0.01 s** (0.03 s total), printing that all seven named and
three default cases passed. No additional target, test infrastructure, generation, old/global
proof run or compiler-input hash inventory was added. This closes the private helper behavior
evidence without widening the earlier guard/rendering claims. Full JS outputs, the comparison
script/report, build and CTest logs are in `/tmp/builder4-custom-uniform-behavior/` and attached
to the follow-up handoff.
