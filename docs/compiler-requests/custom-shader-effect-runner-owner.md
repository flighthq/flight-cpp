# A refused binding and resolver are still referenced by the emitted registration call

## Reproduction

`flight/effects_gl/gl_custom_shader_effect.hpp` at `806d257`. The file refuses two declarations:

```
// NOT GENERATED: variable (binding) -- source line 78
// refusal: cpp-structural-assertion-writable-capability-unproven [source-portability]
// NOT GENERATED: function isGlCustomShaderEffectResolvable -- source line 85
```

and then emits the call that passes both of them:

```cpp
inline void register_gl_custom_shader_effect(flight::Ref<flight::types::GlRenderState> state) {
  flight::effects_gl::register_gl_effect(state, flight::String("CustomShaderEffect"),
      gl_custom_shader_effect_runner, is_gl_custom_shader_effect_resolvable);
}
```

Neither name exists, so four `effects_gl` headers fail with
`use of undeclared identifier 'gl_custom_shader_effect_runner'`.

## Cause

`glCustomShaderEffectRunner` is refused because its body asserts `effect as CustomShaderEffect` to pass a
readonly parameter, and the source's writable claim is not carried by the derived owner. The refusal text
states the required shape itself:

> Preserve `CustomShaderEffect` in the callback parameter through a named typed runner or carrier, or make an
> intentionally erased registry validate and recover that owner before invoking the typed runner.

`isGlCustomShaderEffectResolvable` is refused separately. Either way, a module that refuses a binding must
not emit a later call that references it: the call is what turns a declared refusal into an undeclared name in
every importer.

## Required shape

Emit the runner with the owner-preserving carrier the refusal asks for, and the resolver it pairs with, or
refuse `registerGlCustomShaderEffect` as well so no call references either. Emitting the call without the
declarations is the one answer that is neither.

## What this repository carries meanwhile

Nothing, deliberately. A runtime definition would have to fake the effect runner or pass the input through
unchanged, which is exactly the behaviour the refusal says must not be invented. A declaration-only repair
was considered -- the symbols have no definition anywhere, so the call would be a genuine negative link --
but the repair mechanism declares *functions*; these are a variable binding and a second function, and adding
a variable-declaration kind is new apparatus rather than a cheap fix. The cluster stays open on this request.

## Affected

4 headers in the G1 sweep, all `effects_gl` (`gl_custom_shader_effect` and its importers).

## G2 SSAO runner

`flight/effects_gl/gl_ssao_effect.hpp` at Flight `a627849` / compiler `806d257` is the same omission.
`glSsaoEffectRunner` is refused with `cpp-structural-assertion-writable-capability-unproven` (it asserts
`effect as SsaoEffect` from `Readonly<Effect>`), yet `register_gl_ssao_effect` still passes
`gl_ssao_effect_runner` at line 88. Both owned headers, `gl_ssao_effect.hpp` and its importer
`gl_contact_shadows_effect.hpp`, fail first there (GCC 15.2, one worker).

The runner's callee is refused too: `applySsaoEffectToGl` hits `cpp-contextual-union-value-type-unrepresented`
because it passes `Readonly<GlTextureRenderTarget>` where `drawGlFullscreenPass` declares
`Readonly<GlRenderTarget> | null`. That is a derived-to-base widening, see
[interface-heritage-widening](interface-heritage-widening.md). `gl_contact_shadows_effect.hpp:41` also calls
it. A declaration of `apply_ssao_effect_to_gl` would meet all three witnesses, but neither row would move,
because the runner fails first. So nothing is carried: no runner definition, no declaration, and no
narrowed shader domain.

## G2 outline runner

`flight/effects_gl/gl_outline_effect.hpp` at Flight `a627849` / compiler `806d257` is the same omission as the
SSAO runner above:
- `glOutlineEffectRunner` is refused with `cpp-structural-assertion-writable-capability-unproven`. It asserts
  `effect as OutlineEffect` from `Readonly<Effect>`, which declares none of `color`, `thickness` and
  `threshold`.
- `register_gl_outline_effect` still passes `gl_outline_effect_runner` at line 96. The header fails first
  there (GCC 15.2, one worker, all errors shown: 1 attempted, 0 pass, 1 fail).
- Its callee `applyOutlineEffectToGl` is refused with `cpp-contextual-union-value-type-unrepresented`, the
  same `Readonly<GlTextureRenderTarget>` to `Readonly<GlRenderTarget> | null` widening as SSAO. See
  [interface-heritage-widening](interface-heritage-widening.md).
- No other header names either symbol. `contract.hpp` and `_internal_index.hpp` only include this one.

Nothing is carried: the runner is a variable, a definition would fake the effect, and the callee
declaration would not move the row. No shader, cache or narrowed domain is supplied.

## G2 film grain runner

`flight/effects_gl/gl_film_grain_effect.hpp` at Flight `a627849` / compiler `806d257` is the same omission
again:
- `glFilmGrainEffectRunner` (source line 26) is refused with
  `cpp-structural-assertion-writable-capability-unproven`, because `Readonly<Effect>` declares none of
  `intensity`, `seed` and `size`.
- `register_gl_film_grain_effect` still passes `gl_film_grain_effect_runner` at line 89, which is the
  cluster's recorded first failure.
- The callee `applyFilmGrainEffectToGl` (source line 9) is refused with the same `GlTextureRenderTarget`
  widening, see [interface-heritage-widening](interface-heritage-widening.md).

I validated the site by inspecting the emitted header and confirmed that no definition of either symbol
exists anywhere in the tree, and that no other header names either one. This is a docs-only disposition
within this family, so the unchanged header was not recompiled, and its recorded **0/1** failure stands.
Nothing is carried.

## G2 glitch runner

`flight/effects_gl/gl_glitch_effect.hpp` at Flight `a627849` / compiler `806d257` is the same omission:
- `glGlitchEffectRunner` (source line 29) is refused with
  `cpp-structural-assertion-writable-capability-unproven`, because `Readonly<Effect>` declares none of
  `blockSize`, `colorShift`, `intensity` and `seed`.
- `register_gl_glitch_effect` still passes `gl_glitch_effect_runner` at line 92.
- The callee `applyGlitchEffectToGl` (source line 9) is refused with the same `GlTextureRenderTarget`
  widening, see [interface-heritage-widening](interface-heritage-widening.md).

I validated the site by inspecting the emitted header. No definition of either symbol exists in the tree,
and no other header names either one. This is a docs-only disposition, so the unchanged header was not
recompiled, and its recorded **0/1** failure stands. Nothing is carried.

## G2 chromatic aberration runner

`flight/effects_gl/gl_chromatic_aberration_effect.hpp` at Flight `a627849` / compiler `806d257` is the same
omission:
- `glChromaticAberrationEffectRunner` (source line 30) is refused with
  `cpp-structural-assertion-writable-capability-unproven`, because `Readonly<Effect>` declares neither
  `intensity` nor `radial`.
- `register_gl_chromatic_aberration_effect` still passes the runner at line 88.
- The callee `applyChromaticAberrationEffectToGl` (source line 15) is refused with the same
  `GlTextureRenderTarget` widening, see [interface-heritage-widening](interface-heritage-widening.md).

I validated the site by inspecting the emitted header. Neither symbol is defined anywhere in the tree, and
no other header names either one. This is a docs-only disposition, so the unchanged header was not
recompiled, and its recorded **0/1** failure stands. Nothing is carried.

## G2 bokeh depth-of-field runner

`flight/effects_gl/gl_bokeh_depth_of_field_effect.hpp` at Flight `a627849` / compiler `806d257` is the same
omission:
- `glBokehDepthOfFieldEffectRunner` (source line 37) is refused with
  `cpp-structural-assertion-writable-capability-unproven`, because `Readonly<Effect>` declares none of
  `focusDistance`, `focusRange` and `maxBlur`.
- `register_gl_bokeh_depth_of_field_effect` still passes the runner at line 100.
- The callee `applyBokehDepthOfFieldEffectToGl` (source line 16) is refused with the same
  `GlTextureRenderTarget` widening, see [interface-heritage-widening](interface-heritage-widening.md).

I validated the site by inspecting the emitted header. Neither symbol is defined anywhere in the tree, and
no other header names either one. This is a docs-only disposition, so the unchanged header was not
recompiled, and its recorded **0/1** failure stands. Nothing is carried.

## G2 remaining effects_gl runner family

At Flight `a627849` / compiler `806d257`, 28 more single-header `effects_gl` clusters are the same omission as the SSAO, outline, film grain, glitch, chromatic aberration and bokeh runners above. Each header was checked by script and spot-checked by eye against three conditions:

1. Exactly two NOT GENERATED markers:
   - the `gl*EffectRunner` binding, refused `cpp-structural-assertion-writable-capability-unproven` for an `effect as <X>Effect` assertion from `Readonly<Effect>`;
   - its callee `apply<X>EffectToGl`, refused `cpp-contextual-union-value-type-unrepresented` on `StructuralRef<RowReadonly<RowOf<Ref<GlTextureRenderTarget>>>>` (see [interface-heritage-widening](interface-heritage-widening.md)).
2. The recorded first failure is the `register_gl_<x>_effect` call that passes the refused runner.
3. Neither symbol is defined anywhere in the tree, and no other header names either one.

Every disposition is docs-only, so the unchanged headers were not recompiled. Each recorded **0/1** failure stands, and nothing is carried: no runner, effect body, shader, cache or narrowed parameters.

| cluster | header:line | members `Readonly<Effect>` does not declare |
| --- | --- | --- |
| `undeclared-identifier-2363216f` | `gl_dither_effect.hpp:86` | levels |
| `undeclared-identifier-3582eeb5` | `gl_screen_space_fog_effect.hpp:98` | color, density, far and near |
| `undeclared-identifier-3e7f03c4` | `gl_sharpen_effect.hpp:86` | amount |
| `undeclared-identifier-44058981` | `gl_blend_effect.hpp:119` | backdropKey, mode and opacity |
| `undeclared-identifier-51763a3c` | `gl_tone_map_effect.hpp:96` | exposure, operator and white |
| `undeclared-identifier-529e49d7` | `gl_directional_blur_effect.hpp:91` | angle, length and samples |
| `undeclared-identifier-5be7be47` | `gl_scanlines_effect.hpp:87` | count and intensity |
| `undeclared-identifier-67f2c521` | `gl_god_rays_effect.hpp:106` | centerX, centerY, decay, density, exposure, samples and weight |
| `undeclared-identifier-6ad30a3d` | `gl_crt_effect.hpp:92` | aberration, curvature, scanlineIntensity and vignette |
| `undeclared-identifier-7d85a310` | `gl_lens_flare_effect.hpp:91` | ghosts, halo, intensity and threshold |
| `undeclared-identifier-7fd1550e` | `gl_camera_motion_blur_effect.hpp:88` | intensity and samples |
| `undeclared-identifier-7ff1c319` | `gl_vignette_effect.hpp:95` | color, intensity, radius and softness |
| `undeclared-identifier-898d7996` | `gl_displacement_effect.hpp:91` | frequency, intensity and seed |
| `undeclared-identifier-8ae633dc` | `gl_smaa_effect.hpp:86` | threshold |
| `undeclared-identifier-8b3950d6` | `gl_motion_blur_effect.hpp:92` | intensity and samples |
| `undeclared-identifier-8e604c65` | `gl_sketch_effect.hpp:86` | strength |
| `undeclared-identifier-a4a22a69` | `gl_lens_distortion_effect.hpp:87` | amount and scale |
| `undeclared-identifier-af7a1054` | `gl_median_effect.hpp:90` | radius |
| `undeclared-identifier-c3c55c65` | `gl_composite_effect.hpp:92` | backdropKey and operator |
| `undeclared-identifier-c45cc1bb` | `gl_fxaa_effect.hpp:86` | edgeThreshold and subpixel |
| `undeclared-identifier-c7551b38` | `gl_pixelate_effect.hpp:86` | size |
| `undeclared-identifier-cf2fed2b` | `gl_posterize_effect.hpp:85` | levels |
| `undeclared-identifier-d14c2215` | `gl_convolution_effect.hpp:131` | bias, clamp, color, divisor, matrix, matrixX, matrixY and preserveAlpha |
| `undeclared-identifier-d687475c` | `gl_halftone_effect.hpp:88` | angle and scale |
| `undeclared-identifier-d7a1d1b7` | `gl_tilt_shift_effect.hpp:90` | blur, center and width |
| `undeclared-identifier-e4eb111d` | `gl_kuwahara_effect.hpp:86` | radius |
| `undeclared-identifier-ebf3999b` | `gl_radial_blur_effect.hpp:96` | centerX, centerY, samples and strength |
| `undeclared-identifier-f020c1f7` | `gl_white_balance_effect.hpp:87` | temperature and tint |

Three candidates of the same symptom did **not** meet the conditions. They are excluded here and left unchanged:
- `undeclared-identifier-c29489a5` (`gl_bloom_effect.hpp:106`) and `undeclared-identifier-e4a7b51e` (`gl_lens_dirt_effect.hpp:112`): the callee's refused value is the plain `Ref<GlTextureRenderTarget>` of an intermediate target, not the readonly structural row.
- `undeclared-identifier-a89b9a39` (`gl_bitmap_displacement_effect.hpp:153`): the registration also passes a separately refused resolver (`isGlBitmapDisplacementEffectResolvable`, `cpp-structural-assertion-owner-unproven`). That is the two-symbol custom-shader shape, which needs its own unit.

## G2 bloom runner: the callee widens a plain pooled reference

`flight/effects_gl/gl_bloom_effect.hpp:106` is one of the three cases the family table excludes. Each refusal
was inspected on its own terms, at Flight `a627849` / compiler `806d257`.

- Runner: `glBloomEffectRunner` (source line 55) is refused with
  `cpp-structural-assertion-writable-capability-unproven`, the same as the family: `Readonly<Effect>` declares
  none of `intensity`, `passes`, `radius` and `threshold`. `register_gl_bloom_effect` still passes it at
  line 106, which is the header's only error (GCC 15.2, one worker, all errors shown: 1 attempted, 0 pass,
  1 fail).
- Callee: `applyBloomEffectToGl` (source line 22) is refused with `cpp-contextual-union-value-type-unrepresented`,
  but on a different value from the family's. The multi-pass recipe acquires intermediate targets with
  `acquireGlTextureRenderTarget(state, pool, descriptor)` and passes each one (`bright`, `blurred`, `temp`)
  as `drawGlFullscreenPass`'s `dest: Readonly<GlRenderTarget> | null`. The refused value is therefore a
  plain `flight::Ref<flight::types::GlTextureRenderTarget>`, not a readonly structural row. That is the core
  `Ref<Derived>` to `Ref<Base>` widening of [interface-heritage-widening](interface-heritage-widening.md),
  and it still needs real heritage, not a cast.
- No other header names `gl_bloom_effect_runner` or `apply_bloom_effect_to_gl`, and neither is defined
  anywhere.

Nothing is carried:
- The runner is a variable, which the declaration repair cannot declare.
- Declaring the callee would not move the row.
- A definition would fake bloom: the pool acquisition and release, the bright, blur and composite passes,
  and the program cache.

## G2 lens dirt runner: same plain pooled-reference callee as bloom

`flight/effects_gl/gl_lens_dirt_effect.hpp:112` is the second plain-`Ref` exclusion, inspected on its own at
Flight `a627849` / compiler `806d257`.

- Runner: `glLensDirtEffectRunner` (source line 57) is refused with
  `cpp-structural-assertion-writable-capability-unproven`, because `Readonly<Effect>` declares none of
  `intensity`, `seed` and `threshold`. `register_gl_lens_dirt_effect` still passes it at line 112.
- Callee: `applyLensDirtEffectToGl` (source line 21) has the bloom recipe's shape. It acquires `bright`,
  `blurred` and `temp` with `acquireGlTextureRenderTarget(state, pool, descriptor)`, draws into them through
  `drawGlFullscreenPass`, whose destination is `Readonly<GlRenderTarget> | null`, and releases them. The
  refused value is the plain `flight::Ref<flight::types::GlTextureRenderTarget>`, the `Ref<Derived>` to
  `Ref<Base>` widening of [interface-heritage-widening](interface-heritage-widening.md).
- No other header names `gl_lens_dirt_effect_runner` or `apply_lens_dirt_effect_to_gl`, and neither is
  defined anywhere.

This was validated by inspection. It is a docs-only disposition, so the unchanged header was not
recompiled, and its recorded **0/1** failure stands. Nothing is carried: no runner, pass, shader, pool or
narrowed domain.

## G2 bitmap displacement: runner and resolver, as in the custom shader

`flight/effects_gl/gl_bitmap_displacement_effect.hpp:153` is the third exclusion from the family table,
inspected on its own at Flight `a627849` / compiler `806d257`. It has the two-symbol shape this request was
filed for: `register_gl_bitmap_displacement_effect` passes both a refused runner and a refused resolver.

- Runner: `glBitmapDisplacementEffectRunner` (source line 60) is refused with
  `cpp-structural-assertion-writable-capability-unproven`, because `Readonly<Effect>` declares none of
  `componentX`, `componentY`, `edgeMode`, `map`, `scaleX` and `scaleY`.
- Resolver: `isGlBitmapDisplacementEffectResolvable` (source line 67) asserts
  `(effect as Readonly<BitmapDisplacementEffect>).map` to test whether the map texture resolves through
  `resolveGlTexture`. It is refused with `cpp-structural-assertion-owner-unproven`, the same code as
  `isGlCustomShaderEffectResolvable`.
- Callee: `applyBitmapDisplacementEffectToGl` (source line 17) is refused on the family's readonly structural
  `GlTextureRenderTarget` widening, see [interface-heritage-widening](interface-heritage-widening.md).
- No other header names any of the three symbols, and none is defined anywhere.

A declaration of the resolver alone would satisfy the guarded `retainedReference` witness, because it is
passed in a function-argument position. But the runner on the same line is a variable, which the repair
cannot declare, so the row would not move. Nothing is carried:
- no runner, resolver, texture provider, shader or narrowed domain;
- the resolver must not be answered with a fixed `false` or `true`.

This was validated by inspection. It is a docs-only disposition, so the unchanged header was not
recompiled, and its recorded **0/1** failure stands.

## G2 plain blur runner is separate from the Gaussian pass

At Flight `a627849` / compiler `806d257`, glBlurEffect.ts94 glBlurEffectRunner is refused
cpp-structural-assertion-writable-capability-unproven. Its GlEffectRunner slot takes readonly
GlEffectContext and readonly Effect; effect as BlurEffect claims a writable derived view solely to
pass Readonly<BlurEffect>. Readonly<Effect> also declares neither blurX nor blurY, so merely removing
the writable assertion cannot recover the actual derived owner. Preserve BlurEffect in a named typed
runner/carrier or validate/recover the exact retained owner at the registry boundary. A kind string,
copied Effect/BlurEffect prefix or unchecked cast is not that proof; keep the open effect domain.

The runner reads the original ctx.source width/height/format into the authored descriptor, acquires
a temporary from the same ctx.state/ctx.pool, applies blur with the same source/dest/effect, then
releases that temporary on normal completion. Do not invent other descriptor axes, new pool/cache,
replacement target or a cleanup guarantee absent from the source: a thrown apply prevents the normal
release. Explicit registerGlBlurEffect stores this real runner under BlurEffect in that state's
registry; no eager registration or fake pass-through runner is equivalent.

Unlike the family table's first failure at registration, this original1 header first fails earlier
at apply_gl_gaussian_blur_pass93: **0/1 pass**,75.1s, Clang22.1.8 jobs1. The readonly Gaussian helper
itself refuses at the [nullable base-target row](interface-heritage-widening.md#g2-plain-gaussian-blur-preserves-two-compositing-passes).
A declaration-only scratch provides its exact helper prototype and, with the existing exact marker
insert-forward-declaration mechanism, only extern GlEffectRunner gl_blur_effect_runner. No new
variable-declaration engine kind, runner object/storage or body is introduced. Actual unused compile
still fails at the [readonly source-target promotion](readonly-view-to-writable.md#g2-gaussian-blur-rendertexture-source-target),
so both candidates are rejected before link. See [measured qualification](retained-declaration-calls-omitted-callee.md#g2-gaussian-blur-declaration-candidate-fails-the-unused-header).
This precise existing-mechanism probe does not revise the older unqualified dispositions above.
No production declaration, registration, blur, shader/cache or target behavior is supplied/executed.
