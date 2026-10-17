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
