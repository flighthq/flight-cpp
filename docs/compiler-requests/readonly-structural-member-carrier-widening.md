# A readonly structural member must widen the original cell value

This is a measured runtime capability gap, exposed by a rejected native override correction;
no compiler emission change is requested for the correctly emitted caller. Flight
`a62784923f3be814463286c9fb28edfe6ab15789`, compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, Clang 22.1.8.

## Source and carriers

The pinned `GlTextureRenderTarget` declares a present `framebuffer: WebGLFramebuffer`.
`GlRenderTarget` declares `framebuffer: WebGLFramebuffer | null`. A texture target may therefore
be passed to a readonly render-target parameter without changing the target object:

```ts
function framebufferOf(target: Readonly<GlRenderTarget>): WebGLFramebuffer | null {
  return target.framebuffer;
}
function textureFramebuffer(target: Readonly<GlTextureRenderTarget>): WebGLFramebuffer | null {
  return framebufferOf(target);
}
```

The existing row relation accepts the readonly widening from
`StructuralRef<RowReadonly<RowOf<Ref<GlTextureRenderTarget>>>>` to the corresponding
`GlRenderTarget` row. The row retains the original owner. But `row_get<RowKey<"framebuffer">>`
then asks that owner's `named_value` for `optional<WebGlFramebuffer>`; its concrete cell stores
`WebGlFramebuffer`. Exact erased-cell lookup throws `std::bad_cast` at
`include/flight/structural_ref.hpp:229`, reached from `row_get` at :1448. The declared readonly
value conversion is legal even though the concrete storage types differ.

## Required behavior

Read the original owner's current framebuffer and return it as an engaged optional, preserving
its host handle identity and owner lifetime. A later concrete-owner update must be visible on
the next read through the same view. Exact optional storage must still preserve its empty and
present states. This is read authority only: a broader nullable view must not acquire a write
that could put null into the concrete present cell. Do not fabricate a base target, cache a
copied member, recover an unrelated nominal owner or weaken a failed type proof.

The exact-type read/reference ABI needs a deliberate value-projection design and semantic
coverage; adding a permissive `named_value` cast is not a fix. The
[resolved merged-row request](merged-row-not-constructible.md) handles the reverse case,
reading an engaged optional as required, and
[absent sparse-owner reads](optional-row-read-from-sparse-owner.md) handle a missing cell.
Neither covers this present, non-optional cell read through a nullable schema.

## G2 Scene3D present: initial override drift and rejected candidate

The sole inventory row is `flight/scene3d_gl/present_gl_scene_3d.hpp:46`, cluster
`no-viable-conversion-bf84cbac`. Its generated caller correctly constructs a readonly texture-target
view for the pinned `presentGlRenderTarget` API. The effective native override instead accepts
`Ref<GlTextureRenderTarget>`; its linear-to-sRGB helper likewise takes a nominal source and
nominal destination. Those partial overrides, not a missing caller declaration or source heritage,
cause the recorded first compile failure. See also the
[structural argument boundary](structural-subtype-reference-argument-owner.md#g2-scene3d-present-native-override-carrier-drift).

A bounded candidate aligned only the two native helper parameter carriers with the pinned readonly
API, retaining the original row owner and reading its colorSpace/texture cells. It changed no shader,
program cache, GL operation, target storage or source domain. The exact original-one header compiled
**1 attempted / 1 pass / 0 fail / 0 unattempted, 176.6 s, one worker**. Its new targeted SDL/Mesa
executable also built. A real offscreen-context CTest then **failed 0/1 in 0.36 s** with
`readonly present failed: std::bad_cast`. An explicit destination was the same texture-target owner
widened into the readonly base row. GDB's `__cxa_throw` trace identifies:

```text
RowOwner::named_value<optional<WebGlFramebuffer>> (structural_ref.hpp:229)
row_get<"framebuffer", RowReadonly<RowOf<Ref<GlRenderTarget>>>> (:1448)
bind_gl_render_target (gl_fullscreen_pass.hpp:50)
draw_gl_fullscreen_pass (:171, :254)
present_gl_render_target (candidate :66)
```

The failure precedes the first draw/pixel assertion; real context/target allocation and program
compilation/cache lookup had already executed. No copy/OETF pixel result, nine-draw execution,
full Scene3D render, success/failure cleanup or cache teardown is claimed. Two debugger diagnostic
executions investigated this failure; they are not passing test runs. The override consistency
check passed 228/228, which does not establish runtime semantics.

The candidate, test, registrations and manifest edits were rejected and restored. Only this
request and documentation/inventory links remain. The stock header was not rerun; its historical
**0 pass / 1 fail** and original diagnostic remain authoritative. No row or module is promoted.
Scratch evidence: `/tmp/builder4-g2-present-scene3d/` (`candidate-headers-report.json`,
`initial-ctest.log`, `initial-LastTest.log`, `debug-throw.log`, rejected overlay/test/build logs).

## Full present contract to preserve

`presentGlScene3D` is the no-effects composition over the caller's state, target, scene, camera and
lights. It begins a pass with clear RGBA `[0,0,0,0]`, depth 1 and stencil 0; renders; always ends
an acquired pass in `finally` (restore state and resolve MSAA); and presents the same target to the
canvas only on success. If begin throws, no pass is ended. A render failure still ends the pass
and prevents present; an end failure supersedes a pending render failure. The source recommends a
resized linear rgba16f/depth-stencil intermediate without narrowing the public target domain.

Present reads that owner's declared colorSpace and texture. Linear content gets exactly one sRGB
OETF: RGB is clamped to at least zero, then uses 12.92*x below 0.0031308 and
1.055*pow(x, 1/2.4)-0.055 at/above it; alpha is preserved. Already-sRGB content is copied unchanged.
Default/null destination means canvas; an explicit compatible readonly destination retains its own
framebuffer, viewport, dimensions and GL context. Existing context-keyed copy/OETF program cache
identity, uniforms, draw ordering and teardown remain required. A fake successful present, copied
base target, unchecked owner cast, narrower destination domain or extra conversion is not equivalent.

## Native follow-up: live readonly nullable value projection qualified

The bounded 2026-10-17 follow-up resolves the measured runtime gap and aligns the two native
present/OETF helper signatures with the already-correct readonly source API. It includes the
required runtime change; the ABI-only candidate above remains rejected historical evidence.

For a non-required readonly named member whose schema reads optional<T>, row_get now returns a
fresh value on each read. It accepts the owner's exact optional<T> cell or its exact present T
cell and wraps one optional layer only. Empty, engaged-null-reference and engaged-reference
states remain distinct. The same owner and payload identity remain; a later concrete cell update
is read on the next access. No wrapper is cached and resetting the returned optional cannot write
null into a present cell. A missing sparse cell still reads empty; a present unrelated storage type
still throws bad_cast. Exact erased-cell lookup remains exact. Writable, required, partial and
non-optional readonly access keep their previous paths; no conversion proof or write authority
is added. The readonly optional return deliberately changes from a cell reference to a value;
there is no optional reference to return from a bare payload cell.

The structural-row target built/linked and passed **1/1, 0.01 s**, including thirteen new semantic
checks plus five compile-time assertions: same owner/reference, projected-value independence,
live updates, retained/released owner lifetime, exact optional empty/null/present states, nested
one-layer lifting, unchanged exact lookup and unrelated-cell rejection. Its existing required,
partial, sparse, write and identity checks also ran.

The new actual SDL/Mesa present target built/linked and passed **1/1, 0.42 s**. One CTest invocation
ran these two tests **2/2 pass, 0.43 s total**, once each. The real context executes eleven native
draws and RGBA8 readbacks using the unchanged copy and OETF shader programs. The fixture checks
both color-space branches (within two byte values of the expected channels), nominal/readonly
source compatibility, explicit readonly texture destinations whose framebuffer cell is present,
omitted/null canvas destinations, and an explicit base target whose optional framebuffer is
empty. Each output starts cleared, so a no-op cannot inherit a prior pixel. The same source view
observes a later texture-cell update. Both program caches retain the identical program on repeat
use; actual target deletion and context teardown callbacks complete without GL errors.

Changed original-one header compilation with Clang22.1.8, jobs1: **1 attempted / 1 pass / 0 fail /
0 unattempted, 173.6 s**. The row is PASS with previousCluster and request traceability; the
single-header active bf84cbac root is removed. No generated SDK header, source patch, emission
repair, binding manifest or pin changed. The two existing override records retain partial status
and match their generated derivations (**228/228 override consistency pass**).

This proves the generic readonly nullable read and actual native present helpers. It does not
execute the full generated presentGlScene3D scene composition, its begin/render/finally/end failure
ordering or MSAA resolve; those untouched source contracts above remain requirements. It does not
prove all OETF inputs, a complete module/package, or any DOM style provider. Foreman and Manager
explicitly retained the five DOM rows as unavailable under SDL; they were not recompiled. No
normal generation, full sweep or old failed-candidate rerun was performed. Historical measurements,
sweep/carry counters and foreign products remain intact. Current evidence is under
`/tmp/builder4-dom-style-present/` (build, CTest/LastTest, scoped header report and override check).
