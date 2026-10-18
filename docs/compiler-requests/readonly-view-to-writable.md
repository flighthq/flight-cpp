# A readonly structural view is cast to a writable one

## Reproduction

`flight/scene3d_gl/gl_blinn_phong_mesh_material_renderer.hpp` at `806d257`, line 126:

```cpp
flight::scene3d_gl::draw_gl_mesh_subset(state,
  flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GlMeshProgram>>>>(
    flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::GlMeshProgram>>>>(program.value())),
  ...);
```

The inner conversion builds a `StructuralRef<RowWritable<...>>` from a value that is then handed to a
`StructuralRef<RowReadonly<...>>` parameter through a second cast:

```
error: no matching conversion for functional-style cast from
       'StructuralRef<RowReadonly<RowOf<shared_ptr<GlMeshProgram>>>>' to
       'StructuralRef<RowWritable<RowOf<shared_ptr<GlMeshProgram>>>>'
```

## Required shape

A readonly view cannot be manufactured into a writable one; the writable carrier has to come from the
declared owner, or the parameter has to receive the readonly view it actually declares. The emitter already
knows which one the callee takes.

## Affected

One swept header at G1 (`scene3d_gl`'s `_internal_index.hpp`, which includes the file above). Filed because
it is a distinct emitter defect, not because of its size.

## What this repository carries meanwhile

Nothing: the cluster is open.

## G2 Gaussian blur RenderTexture source target

At Flight `a627849` / compiler `806d257`, gl_blur_effect.hpp112 is the same defect as the mesh
case above. Source applyGaussianBlurToGlRenderTextures reads sourceTarget=getGlRenderTextureTarget
and passes it to applyGaussianBlurToGl's Readonly<GlTextureRenderTarget> source. The source getter
explicitly returns Readonly<GlRenderTextureEntry['target']>|null; both its generated signature and the
native override return optional<readonly GlTextureRenderTarget row>. There is no writable authority
at this call and no disagreement caused by the override's public carrier.

A scratch header adds only the exact refused Gaussian helper prototype and refused runner-slot
extern through existing guarded declaration mechanisms. Actual unused-header native compilation
(Clang22.1.8, one worker) fails at candidate116/original112:

```text
no matching conversion for functional-style cast from
const StructuralRef<RowReadonly<RowOf<shared_ptr<GlTextureRenderTarget>>>>
to StructuralRef<RowWritable<RowOf<Ref<GlTextureRenderTarget>>>>
```

The runtime constructor's row_convertible_to constraint rejects readonly-to-writable correctly.
The emitter wraps source_target.value() in RowWritable only to cast it back to the identical
RowReadonly schema expected by the callee. Pass the already-proven readonly view directly; do not
loosen runtime readonly checks, change the source getter's contract, recover mutable authority, copy
the target or add side storage. Destination/scratch callbacks legitimately receive mutable target
owners; that does not grant the independently resolved source target the same authority.

Stock original1 remains0/1 at the earlier omitted helper93 (75.1s), while the changed candidate fails
before any link gate and is rejected. No declaration/body/runtime/source/binding/override or normal
generation is carried. The [qualification record](retained-declaration-calls-omitted-callee.md#g2-gaussian-blur-declaration-candidate-fails-the-unused-header)
and [full Gaussian/RenderTexture contract](interface-heritage-widening.md#g2-plain-gaussian-blur-preserves-two-compositing-passes)
separate actual source-target authority from program-cache and refused-runner debt. No GL execution.

## G2 SVG transform preserves the guarded readonly Matrix

At Flight `a627849` / compiler `806d257`, `no-viable-conversion-deeedfb9` is the single
`flight/scene2d_formats/svg_transform.hpp` row from the historical Number-callback group.
Its recorded first failure at line 52 is the exact invalid readonly-to-writable construction:

```cpp
// Parameter: optional<StructuralRef<RowReadonly<RowOf<Ref<Matrix>>>>> matrix
if (!matrix.has_value()) return;
decompose_matrix_to_transform_2d(transform,
    flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::types::MatrixLike>>>>(
        flight::StructuralRef<flight::RowWritable<
            flight::RowOf<flight::Ref<flight::types::Matrix>>>>(matrix.value())));
```

Source `applySvgTransform(target: Node2D, matrix: Readonly<Matrix> | null)` returns on null
and calls `decomposeMatrixToTransform2D(transform, matrix)` with the same proven-present
readonly matrix. The callee declares `source: Readonly<MatrixLike>`. Source Matrix is the
existing entity with six affine numbers; MatrixLike is EntityWithoutRuntime<Matrix>.
Current generated `types/matrix.hpp` aliases MatrixLike to Ref<Matrix>, so both native
readonly row schemas are exactly the same type. The correct call passes `matrix.value()`
directly. A static type assertion checks that identity; a separate assertion confirms that
readonly-to-writable construction remains unavailable.

The bounded `scene2d-formats-svg-transform-readonly-matrix` entry uses the existing exact
`respell-reference-member-access` mechanism, matching only that full expression in this
header. It removes one redundant conversion and changes no body operation, branch, owner,
allocation, order, numeric default or accepted input. Fixed-point application and reversing
only the replacement recover the exact original header. No runtime weakening, mutable
recovery, matrix copy, fake parser/transform, source patch or override is carried.

An actual whole-header native probe compiles, links and runs (all exit 0, Clang 22.1.8).
It calls the real apply function with null, translation, a reused readonly view after its
actual owner changes, rotation in degrees, signed reflection and skew. It checks unchanged
source matrix values, the same source/node owners and continued readonly-to-writable rejection.
Normal pinned scoped generation exits 0: 1467 modules lowered across 30 packages, 1 complete module emitted, 0 refusals,
252 source patches and 2 repair applications to this same header (the existing
unqualified-types-names repair and this new readonly entry). Its whole emitted header is byte-identical
to that native-tested candidate. Final scoped original one: **1 attempted / 1 pass / 0 fail / 0 unattempted**. Repair mechanism
tests: **29 pass, 1 skip, 0 fail**.

The complete source contract remains the requirement for `svgTransform.ts`:

- `applySvgTransform` returns immediately for null. Otherwise it allocates the existing identity
  Transform2D, decomposes the same readonly Matrix, and assigns all nine
  pivot/rotation/scale/skew/position fields to the same Node2D. It does not replace either owner or
  mutate the input matrix. Decomposition resets pivot to zero, retains translation, uses degrees for
  rotation/skew, and carries a negative determinant in signed scaleY; it is not a translation-only
  shortcut.

- Matrix is the existing entity-backed six-number owner (`a,b,c,d,tx,ty`); source MatrixLike omits
  entity runtime requirements, and current native MatrixLike aliases `Ref<Matrix>`. Readonly Matrix
  and readonly MatrixLike therefore use the identical native row schema. `multiplySvgMatrices(a,b)`
  allocates a fresh output and calls the real multiplyMatrix with unchanged argument order. In the
  affine convention `[a c tx; b d ty; 0 0 1]`, the output is a*b: a parent translation followed by
  child scale keeps the parent's translation. Input owners/values stay live and unchanged, not
  cloned to justify writes.

- `parseSvgTransform` takes string|null, returns null for null or trimmed-empty input, and otherwise
  allocates an identity result before scanning the original string. Its global
  `/([a-zA-Z]+)\s*\(([^)]*)\)/g` searches for functions, rather than requiring an anchored
  whole-attribute grammar. Captures 1 and 2 are mandatory on a successful match; capture 2 may be
  empty. Capture 0 is the entire function text. Operation names are compared case-sensitively,
  including `skewX` and `skewY`. Do not treat every RegExp capture as required or replace the
  regexp/capture representation globally.

- The body parses only capture 2 with the real `parseSvgNumberList`: global, case-insensitive signed
  decimal/exponent token matching followed by Number conversion, preserving token order,
  whitespace/comma forms, adjacent signs, leading/trailing decimal points and e/E exponents.
  Non-number text is skipped by that extractor; it is not a stricter delimiter/finite/whole-argument
  validator. No tokens means a fresh empty numeric list. Preserve the source's actual
  overflow/non-finite behavior and exceptions, not added clamping, unit conversion or rejected
  syntax.

- `matrix` uses the first six values only when there are at least six. `translate` defaults each
  missing component to zero. `scale` defaults x to one and y to the first present of y, x, one;
  explicit zero remains zero. `rotate` defaults angle to zero, converts degrees to radians, and uses
  the actual sine/cosine matrix. With at least three numbers it composes T(cx,cy)*R*T(-cx,-cy),
  otherwise it ignores an incomplete center. `skewX` and `skewY` default the angle to zero and put
  its tangent into c or b respectively. Extra numeric arguments retain the source's ignored
  behavior.

- Each supported operation is composed as result*operation in encounter order. `matched` becomes
  true for every regexp function match before dispatch: unknown names and short `matrix` forms skip
  that operation but still distinguish an authored no-op from absence. Thus `bogus(1)` returns the
  existing fresh identity, whereas a nonempty string with no function match returns null. Do not
  collapse identity to null, return a shared identity object, change operation case, reject all
  unknown text, reorder operations or supply a fake transform.

- `createSvgViewportMatrix` prefers each present optional viewport x/y/width/height, retaining
  explicit zero; absent x/y use numeric attributes with zero fallback. `viewBox` uses the same
  number extractor; fewer than four numbers yields null only at x=y=0, otherwise a fresh
  translation. Width/height use parseSvgLength with the corresponding viewBox extent as fallback.
  That helper uses parseFloat, accepts numeric prefixes/unit suffixes and chooses fallback for
  non-finite parses. Full viewBoxes go to the existing fitter with the actual preserveAspectRatio
  attribute, default `xMidYMid meet`.

- `createSvgViewBoxMatrix` uses the first four viewBox numbers and actual viewport x/y/width/height.
  Zero source extent gives scale 1 on that axis before fitting. Exact trimmed `none` keeps
  independent scales. Otherwise whitespace parts retain the source `defer`/missing-alignment
  fallback, `slice` selects max scale and default meet selects min; xMax/xMid and YMax/YMid
  substring decisions distribute spare extents. Preserve negative/zero dimensions and actual
  unrecognized-alignment behavior; no new aspect-ratio grammar or dimension validation is implied.

The existing SVG transform tests specify imported translation and degree rotation, meet/none/slice
fits, viewport sizing, parent/child multiplication order, ordinary parsing, null and
unknown-function identity. Their fieldwise matrix comparisons explicitly avoid confusing equal
numbers with entity identity. SVG XML tests specify numeric extraction. Gradient consumers compose
parsed gradientTransform with their existing matrices. These are source requirements; a changed-seam
native probe does not imply complete SVG parsing, viewport import, gradients, scene import or
painting execution.

The inspected `scale` branch has a separate preexisting
[indexed fallback debt](out-of-range-array-index-widened-to-null.md#g2-svg-transform-empty-scale-arguments-retain-the-final-one-default):
source `values[1] ?? values[0] ?? 1` retains the final one for an empty list, while emitted
code's second-arm read uses throwing `element(0)` and omits that final fallback. This is not
a measured next compile failure or a newly executed parser case; the changed-seam probe
above exercises apply/decomposition, not the parser. The parser's empty-argument domain must
remain admitted, and a header pass does not certify this existing behavior gap as resolved.

This readonly conversion also differs from the
[SVG style numeric optional-chain result](numeric-optional-chain-result-erased.md): that
failure erases a match length into optional<Any>. Here the guarded payload is a complete
readonly Matrix row, and svgXml's numeric extraction correctly retains optional<Array<double>>
through map(Number); neither is repaired by an Any-to-number conversion. No numeric request
or style cluster is used to classify this Matrix failure. The same guarded-row issue is also
recorded in [guarded readonly helper calls](guarded-readonly-optional-helper-call.md).

Only this scoped repair, normally generated header/applied record and owned coverage/products
change. The original missing conversion cluster is removed after its one header passes;
that row retains its historical Number `previousCluster` and records the resolved intermediate
cause. Historical Number 2/16/table counts, all foreign compact rows/records and pinned inputs
remain intact. Unchanged C++ is not rerun for final docs/base-only updates. No scene importer,
viewport parser, gradient or rendering execution is claimed by the apply-function probe.

## G2 Effects-GL: a rebindable readonly target stored through a writable lift

At Flight `a627849` / compiler `806d257`, `flight/effects_gl/gl_render_texture_effect.hpp:130`
(cluster `variant-cast-7dd5385f`) is the same defect inside a reassigned local's binding cell.
`applyGlEffectsToRenderTexture`'s `withGlRenderState` closure binds `let current = sourceTarget`
(a `Readonly<GlTextureRenderTarget>` returned by `getGlRenderTextureTarget`) and rebinds it after
each `writeGlRenderTextureTarget` call to the target of the alternating output texture. The
emitter stores `current` in a `BindingCell` whose declared value is the readonly target view, but
manufactures that initial value as a writable row and casts it straight back:

```cpp
const auto current_capture = flight::make_binding_cell(
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GlTextureRenderTarget>>>>{
      flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<
          flight::RowOf<flight::Ref<flight::types::GlTextureRenderTarget>>>>(
        flight::StructuralRef<flight::RowWritable<
            flight::RowOf<flight::Ref<flight::types::GlTextureRenderTarget>>>>(
          source_target.value()))});
```

```text
no matching conversion for functional-style cast from
const StructuralRef<RowReadonly<RowOf<shared_ptr<GlTextureRenderTarget>>>>
to StructuralRef<RowWritable<RowOf<Ref<GlTextureRenderTarget>>>>
```

`BindingCell::rebind` replaces the stored view; it never writes through it, so no writable
authority is needed. The source getter returns a readonly target view (both its generated
signature and the native override), and the runner's `source` parameter is readonly too, so
`current` never carries mutation authority. The required shape is to initialize the cell with
`source_target.value()` directly, exactly as the SVG transform site above passes its guarded
readonly matrix.

The operation list (`effect_runner_1044da39c1a2f71d`), the explanation report and the
`write_gl_render_texture_target` callback compile unchanged; the writable lift exists only in
the capture initialization, which is why this is one expression rather than a missing owner.

Carried meanwhile: emission repair `effects-gl-binding-cell-readonly-target`
(`respell-reference-member-access`) removes only the writable round trip, keeping the same
view, the same live target owner and the same rebind order. It expires when the emitter
initializes a rebindable readonly binding cell without the writable lift.

## G2 custom material source: native uniform ABI cleared, readonly view still rewrapped

At Flight `a627849` / compiler `806d257`, original-one
`scene3d_gl/gl_custom_shader_mesh_material_renderer.hpp` first failed at uniform2fv127
(`call-has-no-matching-c8046ec6`). The emitted source branch is correct to upload a number array:
CustomShaderMaterial.uniforms is Record<string, number | number[]>|null, and the pinned DOM
Float32List includes both Float32Array and GLfloat[]. The generated call supplies the two optional
range arguments as std::nullopt. Native uniform1/2/3/4fv already accepted two-argument Array<double>
and four-argument Float32Array, but lacked the four-argument Array forms. This was a host ABI gap,
not a reason to replace the material array, shader, renderer, callback or binding value carrier.

The bounded native fix adds those four Array overloads. Two absent selectors forward the identical
Array to the unchanged legacy upload; explicit selectors share the existing checked bounds and use
a numeric value slice before the unchanged double-to-GLfloat conversion/count/GL call. Offset
absence means zero; length absence or zero means the remaining elements. Invalid/nonintegral/
negative/out-of-range selectors retain the typed path's range_error rules. Existing two-argument
and typed overloads remain, and typed selection still uses its original subarray. This immediate
numeric upload window is not a substitute material, context, program or shader-source owner.
No generated header, source patch, emission repair, override, binding manifest or pin was changed.

The changed existing host SDL GL application target initially built/linked and passed **1/1,
0.74 s** (0.76 s total). The test was then strengthened to reset each uniform to a distinct sentinel
before every upload, so a no-op cannot inherit the preceding expected value. After rebuilding the
changed target, this final version passed **1/1, 0.13 s** (0.14 s total). Each version ran once.
In one real context, a linked uniform fixture exercises all four widths: 24 numeric-array uploads
(absent/explicit zero, prefixed offset, zero/rest length, bounded length), four typed-array offset
uploads, and 16 invalid numeric-array ranges. The 28 sentinel resets use unchanged two-argument
typed uploads: 56 successful upload calls in total, including the 28 cases under test. Actual
glGetUniformfv readback verifies every selected component after native float conversion, including
fractional numbers: 32 reads include one after the rejected ranges for each width, proving unchanged
values and no GL error. The fixture uses the host's existing typed GL entry-point loader; no model
callback is cast or substituted. This is host upload proof, not execution of the custom material
renderer, its cache/guard, or its shader/draw path. The test-only strengthening changes no production
or SDK header input, so the scoped header compile below was not repeated.

After that fix, changed original1 compilation with Clang22.1.8 jobs1 completes **1 attempted /
0 pass / 1 fail / 0 unattempted, 124.9 s**, at the independent readonly conversion at **:175**:

```cpp
// get_gl_custom_material_shader_source returns optional<ReadonlySource>.
// ensure_gl_custom_shader_program's third parameter is already ReadonlySource.
ensure_gl_custom_shader_program(state, shader_key,
    structural_ref_cast<ReadonlySource>(WritableSource(source.value())));
```

ReadonlySource is StructuralRef<RowReadonly<RowOf<Ref<GlCustomMaterialShaderSource>>>>;
WritableSource is its RowWritable counterpart. The preceding null check proves presence, not
writable authority. Pass the already-proven source.value() readonly view directly, retaining the
same source registry object and its live vertex/fragment strings. The runtime correctly rejects
readonly-to-writable construction; do not weaken it, recover mutable authority, copy the source
or allocate a replacement owner. This is this request's existing family, now another measured
site in active emitter-conversion-b504f8c4 (1 -> 2 headers). The old uniform root is removed;
this row remains failed with previousCluster and the request, so there is no header/module gain.
No candidate readonly repair, source rewrite, normal generation or additional compile is carried.

The untouched renderer contract remains:

- Source registration replaces the same state's persistent customMaterialShaders table with
  withKindMapEntry; last write wins lookup, snapshots retain their prior table and source identity.
  Lookup returns the same source or null. The compiled program cache remains keyed by
  `custom:${shaderKey}`; a cache hit keeps the identical compiled program even after source
  re-registration, and a new key forces a miss. Compile the original vertex/fragment strings
  verbatim, retain the real program and nullable model/normal/view-projection/camera locations.
- Bind keeps the same state/material/camera/light owners. Null material, empty key or missing
  source sets activeMeshProgram null and skips draw setup. Otherwise preserve ensure-program,
  nullable opt-in customShaderGuard, beginGlMeshDraw, view/camera upload, custom uniforms and
  textures in that order. No copied base program, callback cast or narrower material domain.
- Each uniform bind uses original Object.keys order, queries its location before reading its
  value, and skips a null location. Scalars and length-one arrays use uniform1f; lengths2/3/4
  use the matching vector upload; all other lengths, including empty, use uniform1fv. Keep
  the original number domain and existing float conversion, without clamping or retained copies.
- Texture enumeration queries each location, selects TEXTURE0+unit, resolves the same full Texture
  value, and uploads/increments only on success. Failed resolution retains that unit for the
  next entry. Keep source color-space realizations and shader-owned premultiplication/mask
  discard; do not inject replacement GLSL or narrow dimensions. Draw still skips a null active
  program and uses the same proxy, geometry and subset with drawGlMeshSubset otherwise.

The 12 pinned source tests covering source identity/registration, cache-facing bind, uniform dispatch,
skips, indexed subset draw, persistent snapshots and opt-in renderer registration were read only.
They are not additional native proof. Recorded draw/proxy rewrapping expressions are independent
inspected follow-ons, not measured later errors. Evidence is under
`/tmp/builder4-g2-custom-material/` (host build/CTest/LastTest and scoped header report).

## G3 matching present readonly returns

Retiring the obsolete same-namespace Canvas using repair exposes this independent boundary in
`scene2d_canvas/canvas_render_surface.hpp`, through the original Canvas state header's dependency
on the texture resolver and surface module. The authored `getCanvasHost` reads its optional readonly
host once, throws when it is undefined, and returns `Readonly<HostCanvasCapability>`. Its binding
remains `optional<StructuralRef<RowReadonly<RowOf<Ref<HostCanvasCapability>>>>>`, but flow narrowing
names the nominal subject. The general contextual conversion then manufactures a writable view of
that already readonly payload before converting it back to the original readonly type.

[`compiler-present-readonly-row-context.patch`](../../source-patches/compiler-present-readonly-row-context.patch)
uses the binding's actual storage for this exact case. A proven-present identifier, an `optionalSingle`
readonly row payload, and the identical emitted readonly destination carrier are all required. The
existing projection is emitted once, so the return is `canvas_host.value()`. This retains the exact row
owner, native owner, live cells and readonly capability. The original source, signature, guard, property
snapshot, source evaluation and exceptions stay unchanged. No object, cell or forwarding function is
copied or supplied. Still-absent results, variant-backed unions, non-readonly storage, different carriers
and nominal-owner recovery retain their existing paths; this does not close the broader request.

The candidate production compiler builds with the preceding 67 compiler patches. Thirteen direct
compiler controls pass: eight exact projections and five counterexamples with byte-identical output
or the same refusal as an independently transpiled preceding-stack source baseline. Four actually
emitted native functions compile, link and run for null/undefined guards, a property snapshot, an
effectful callback and a partial readonly schema. They preserve row/native owner identity, live numeric
cells including signed zero and NaN, replacement attachments, single evaluation, missing-value throws
and an exception's exact owner. Compiler Vitest and Oxlint executables and the static-check launcher
are unavailable locally; no upstream unit/lint/static gate pass is claimed. Original Canvas headers
and their real public consumer still require scoped normal regeneration and qualification.

The later full 70-compiler-patch composition applies and reverts cleanly, builds, and passes the same
13 controls against its preceding 69-patch source baseline. The four emitted native function headers
remain byte-identical to their qualified version, so that native check is not repeated. A separate
origin spotcheck transpiles the raw `806d257` backend source on the same fixture IR: it already emits
the invalid writable intermediate before any repair, and applying the complete declared repair set
leaves that return unchanged. This isolates the real emitter defect; it does not claim a full raw
compiler build or replace the pending original Canvas consumer qualification.

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
