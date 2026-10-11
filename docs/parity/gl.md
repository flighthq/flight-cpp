# GL upstream test parity inventory

This inventory is pinned to Flight revision
`a62784923f3be814463286c9fb28edfe6ab15789`. The checkout was materialized from
`dependencies.lock.json`, and `npm run rehydrate:check` confirmed the exact revision before the
test sources below were read.

The 17 package headings are the exhaustive GL lane assignment. For compactness, every name in an
**Upstream modules** line is relative to `packages/<package>/src/` and has the suffix `.test.ts`.
The lists are exhaustive: the lane contains 307 package-scoped test modules at this pin. The behavior
summaries describe assertions in those modules; they do not claim that the TypeScript suite passes in
this repository, that the tests have been lowered to C++, or that a package is ready for native use.
Existing native coverage comes from the repository's CTest registrations: package labels are the
authority for generated tests, while example smoke coverage is described as transitive. No build,
test execution, regeneration, or whole-SDK operation was used to produce this read-only inventory.

## Package inventory

### `render` — 24 modules

**Upstream modules:** `enableColorAdjustmentGuards`, `enableColorAdjustments`,
`enableSceneRenderGuards`, `explainScene2DCoverage`, `explainScene2DPipelineCoverage`,
`explainScene2DRender`, `explainScene3DPipelineCoverage`, `imageSurface`, `mergeRenderOptions`,
`renderAppearance`, `renderCache`, `renderMaterial`, `renderProxy`, `renderProxyAdapter`,
`renderQueue`, `renderRegistryGuards`, `renderRegistrySignals`, `renderState`, `renderTarget`,
`renderTransform2d`, `renderViewport`, `renderer`, `scene2dWorkingColorSpace`, `sceneRender`.

**Asserted upstream behaviors:** guard installation and diagnostics; inherited and composed color
adjustments; deterministic coverage explanations; host image-surface ownership; render-option
presence semantics; cache lifecycle and signals; material, appearance, clip, proxy, queue, target,
viewport, transform, renderer, working-color-space, and scene-walk behavior.

**Existing native CTest coverage:** the package-labelled `flight_cpp.generated_dialog` checks
render-view synchronization: window dimensions and device-pixel ratio update the render target,
viewport, render-state pixel ratio, and 2D render transform; attach is idempotent and detach stops
resize delivery. The conditional `flight_cpp_upstream_scene3d_smoke` exercises the render path
transitively and requires three verified frames with non-background pixels. The similarly named
`flight_cpp.generated_render_state` is labelled to `types`, not `render`, so it is not counted as
package coverage here. None of these tests is parity for the other upstream modules.

### `render-gl` — 31 modules

**Upstream modules:** `enableGlRenderStateGuards`, `enableGlRenderTextureGuards`,
`enableGlTextureResolverGuards`, `explainGlTextureResolution`, `glCompressedTexture`, `glContext`,
`glCubeRenderTarget`, `glDraw`, `glExternalTexture`, `glFullscreenPass`, `glLinearToSrgbPass`,
`glPresentRenderTarget`, `glProgram`, `glQuadMaterialRegistry`, `glReadback`, `glRenderPass`,
`glRenderState`, `glRenderStateBracket`, `glRenderTarget`, `glRenderTargetPool`, `glRenderTexture`,
`glRenderTexturePool`, `glRenderView`, `glShader`, `glShaderBinding`, `glShaderRegistry`,
`glSkinPaletteTexture`, `glTestHelper`, `glTextureResolver`, `glTextureUpload`,
`glTextureVideoUpload`.

**Asserted upstream behaviors:** GL guard diagnostics; compressed-format capability, decode, mip,
face, and layer uploads; context/state creation; cube and ordinary target lifecycle; blend, sampler,
draw, fullscreen, color-conversion, present, readback, pass-bracketing, pooling, program/shader,
registry, binding, skin-palette, resolver, image, and video upload behavior, including restoration of
physical and tracked GL state.

**Existing native CTest coverage:** the conditional `flight_cpp_upstream_scene3d_smoke` traverses
the generated GL render chain and requires three verified frames with non-background coverage.
`flight_cpp.host_sdl` checks the host-facing WebGL extension registry, context/image identities, and
`GlCanvas` reference equality, but is host-binding evidence rather than package parity. There is no
focused native port of the 31 upstream modules.

### `texture` — 7 modules

**Upstream modules:** `cubeTexture`, `renderTexture`, `sampler`, `texture`, `textureColorSpace`,
`videoTexture`, `voxelGrid`.

**Asserted upstream behaviors:** constructors, defaults, initialization, copy/clone independence,
equality and completeness; cube faces; render targets; sampler presets; source readiness and kinds;
UV matrices, transforms, flips, sizes, and pixel rectangles; color-space conversion; video texture
state; and voxel-grid dimensions and indexing.

**Existing native CTest coverage:** `flight_cpp.generated_texture` directly checks default and
explicit 2D construction, resource attachment, deep-cloned sampler/UV values, 2D-array, 3D, and cube
construction, plus cloned common state. The Scene3D smoke uses the texture path transitively. Video,
color-space algorithms, equality, and most mutation/query behavior remain uncovered.

### `texture-formats` — 9 modules

**Upstream modules:** `byteReader`, `detectTextureContainer`, `explainTextureContainerParse`,
`parseAtf`, `parseBasis`, `parseDds`, `parseKtx2`, `selectTextureContainer`, `textureLevelLayout`.

**Asserted upstream behaviors:** endian-aware bounded byte reads; container signature detection and
failure explanations; ATF, Basis, DDS, and KTX2 format mapping; mip, face, layer, array, volume, and
supercompression layout normalization; parser rejection of truncated, unsupported, or malformed
data; and container selection and byte-length calculations.

**Existing native CTest coverage:** none package-focused.

### `shading` — 17 modules

**Upstream modules:** `createAnimatedNormalModifier`, `createDissolveModifier`,
`createEmissiveModifier`, `createEnvReflectModifier`, `createFogModifier`, `createRimModifier`,
`createShadedMaterial`, `createToonModifier`, `createVertexDisplaceModifier`,
`getModifierDefineKey`, `getUnregisteredModifierKinds`, `isBuiltInModifierSlot`,
`isModifierStackValid`, `modifier`, `modifierRegistry`, `orderModifierStack`,
`registerBuiltInModifiers`.

**Asserted upstream behaviors:** canonical defaults and option forwarding for nine modifiers and the
shaded material; null/optional map variants; stable define keys; slot validation and ordering;
registry isolation, replacement, and resolution; unregistered-kind reporting; and complete built-in
registration.

**Existing native CTest coverage:** the conditional Scene3D smoke uses generated shading and PBR
state transitively. There is no focused modifier or registry CTest.

### `materials` — 22 modules

**Upstream modules:** `anisotropyPbrExtension`, `classicMaterials`, `clearcoatPbrExtension`,
`colorScaleBias`, `customShaderMaterial`, `enableMaterialConversionGuards`,
`explainMaterialConversion`, `extendedPbrMaterial`, `iridescencePbrExtension`, `material`,
`material3d`, `materialPresets`, `materialValidation`, `pbrExtension`, `pbrMaterials`, `phongToPbr`,
`sheenPbrExtension`, `specularPbrExtension`, `standardMaterial`,
`transmissionVolumePbrExtension`, `unlitMaterials`, `wrappedDiffusePbrExtension`.

**Asserted upstream behaviors:** classic, standard/specular-glossiness/extended PBR, unlit, custom,
and preset construction; the surface/material trailer; copy, clone, equality, lookup, validation,
and conversion diagnostics; Phong conversion; color-scale/bias algebra; and defaults, UV selection,
range validation, and initialization for every PBR extension family.

**Existing native CTest coverage:** `flight_cpp.generated_materials` directly checks surface option
forwarding and defaults, Blinn-Phong, specular-glossiness and standard PBR, unlit and extended PBR,
metal presets, glass, and transmission/IOR construction. The Scene3D smoke exercises material use
transitively. Conversion, equality, validation, and most extension families remain uncovered.

### `lighting` — 12 modules

**Upstream modules:** `ambientLight`, `areaLight`, `directionalLight`, `environment`,
`hemisphereLight`, `lightAnalysis`, `lightIntensity`, `lightProbe`, `pointLight`,
`sceneForwardLights`, `sceneLights`, `spotLight`.

**Asserted upstream behaviors:** defaults, initialization, cloning, normalized direction/target and
orientation for all light kinds; influence bounds and contribution; luminance, attenuation, range,
shadow, exposure, and unit conversion; spherical-harmonic probes and grids; scene light collection;
and deterministic forward-light selection by layer, priority, contribution, and capacity.

**Existing native CTest coverage:** the conditional Scene3D smoke creates ambient, directional, and
point lights and verifies rendered output. It does not isolate light math, probes, selection, or the
remaining light kinds.

### `effects` — 75 modules

**Upstream modules:** `autoExposureEffect`, `barrelDistortionEffect`, `bevelEffect`,
`bitmapDisplacementEffect`, `blendEffect`, `blendModeMath`, `bloomEffect`, `blurDownsample`,
`blurEffect`, `bokehDepthOfFieldEffect`, `boxBlurMath`, `cameraMotionBlurEffect`,
`chromaticAberrationEffect`, `colorTemperatureMath`, `compositeEffect`, `compositeOperatorMath`,
`contactShadowsEffect`, `convolutionEffect`, `crtEffect`, `customShaderEffect`, `depthMath`,
`directionalBlurEffect`, `displacementEffect`, `ditherEffect`, `dropShadowEffect`, `edgeDetectMath`,
`effect`, `effectCaptureGeometry`, `effectDefaults`, `effectInputs`, `effectInterpolation`,
`effectPadding`, `effectValidation`, `filmEmulationEffect`, `filmGrainEffect`, `fxaaEffect`,
`gaussianKernel`, `gaussianMath`, `glitchEffect`, `godRaysEffect`, `godRaysMath`,
`gradientBevelEffect`, `gradientGlowEffect`, `halftoneEffect`, `innerGlowEffect`,
`innerShadowEffect`, `kuwaharaEffect`, `kuwaharaMath`, `lensDirtEffect`, `lensDistortionEffect`,
`lensFlareEffect`, `linearSampledGaussian`, `medianEffect`, `motionBlurEffect`, `outerGlowEffect`,
`outlineEffect`, `panniniProjectionEffect`, `pixelateEffect`, `posterizeEffect`, `radialBlurEffect`,
`scanlinesEffect`, `screenSpaceFogEffect`, `sharpenEffect`, `sketchEffect`, `smaaEffect`,
`ssaoEffect`, `ssrEffect`, `stylizeMath`, `taaEffect`, `tiltShiftEffect`, `toneMapEffect`,
`toneMapMath`, `vignetteEffect`, `volumetricLightEffect`, `whiteBalanceEffect`.

**Asserted upstream behaviors:** construction, defaults, option propagation, catalog membership,
validation, interpolation, inputs, capture geometry, and padding for the effect families above;
W3C blend/composite math; blur, Gaussian, depth, tone-map, temperature, edge, god-ray, Kuwahara, and
stylization math; and signed, clamped, degenerate, and unknown-kind behavior.

**Existing native CTest coverage:** no package-focused CTest. The Scene3D smoke reaches an effects-GL
presentation path, but does not assert portable effect construction or math independently.

### `effects-gl` — 60 modules

**Upstream modules:** `enableGlEffectGuards`, `glBevelEffect`, `glBitmapDisplacementEffect`,
`glBlendEffect`, `glBloomEffect`, `glBlurEffect`, `glBokehDepthOfFieldEffect`,
`glCameraMotionBlurEffect`, `glChromaticAberrationEffect`, `glColorLutPass`, `glColorMatrixPass`,
`glCompositeEffect`, `glContactShadowsEffect`, `glConvolutionEffect`, `glCrtEffect`,
`glCustomShaderEffect`, `glDirectionalBlurEffect`, `glDisplacementEffect`, `glDitherEffect`,
`glDropShadowEffect`, `glEffectBlitShader`, `glEffectBoxBlur`, `glEffectGradientRamp`,
`glEffectProgramCache`, `glEffectRegistry`, `glEffectState`, `glEffectTintShader`,
`glFilmGrainEffect`, `glFxaaEffect`, `glGlitchEffect`, `glGodRaysEffect`,
`glGradientBevelEffect`, `glGradientGlowEffect`, `glHalftoneEffect`, `glInnerGlowEffect`,
`glInnerShadowEffect`, `glKuwaharaEffect`, `glLensDirtEffect`, `glLensDistortionEffect`,
`glLensFlareEffect`, `glMedianEffect`, `glMotionBlurEffect`, `glOuterGlowEffect`, `glOutlineEffect`,
`glPixelateEffect`, `glPosterizeEffect`, `glRadialBlurEffect`, `glRenderTextureEffect`,
`glScanlinesEffect`, `glScreenSpaceFogEffect`, `glShaderTestHelper`, `glSharpenEffect`,
`glSketchEffect`, `glSmaaEffect`, `glSpatialEffectRegistration`, `glSsaoEffect`,
`glTiltShiftEffect`, `glToneMapEffect`, `glVignetteEffect`, `glWhiteBalanceEffect`.

**Asserted upstream behaviors:** missing/dropped/copy-through/stale-output guard diagnostics;
per-effect runner registration and shader contracts; texture/map/backdrop resolution; multi-pass
target publication; blend and composite index lockstep; LUT and matrix upload, caching, binding, and
sampling; effect-state lifecycle; program caches and registries; and the GL implementation of each
listed effect.

**Existing native CTest coverage:** the conditional Scene3D smoke traverses the effects-GL pipeline
and verifies actual non-background frames. It is one integrated render scenario, not module-level
parity for the 60 upstream tests.

### `quadbatch` — 1 module

**Upstream modules:** `quadBatch`.

**Asserted upstream behaviors:** allocation, growth, reserve/resize, append/set/remove/clear,
compaction, clone independence, signals, vector and matrix transform storage, bounds, exact and AABB
hit testing, iteration, atlas-region handling, tint/material data, and deterministic swap removal.

**Existing native CTest coverage:** none package-focused.

### `surface` — 5 modules

**Upstream modules:** `canvasSurface`, `glSurface`, `surface`, `surfaceDisplay`, `wgpuSurface`.

**Asserted upstream behaviors:** creation from allocated and adopted native drawables; acquisition
refusal; option forwarding; capability-owned release; per-surface runtime/handle identity; resize and
logical display size routing; and Canvas, GL, and WebGPU device/context acquisition.

**Existing native CTest coverage:** `flight_cpp.generated_surface` directly checks Canvas surface
native-handle acquisition, shared owner/runtime identity, release routing, and null acquisition.
`flight_cpp.host_sdl` exercises GL surface identity indirectly. GL and WebGPU surface lifecycle and
display sizing remain uncovered by a package-focused test.

### `image` — 5 modules

**Upstream modules:** `imageResource`, `imageResourceFrom`, `imageResourceReference`,
`imageSourceDimensions`, `imageTestHelper`.

**Asserted upstream behaviors:** image-resource construction, clone/invalidation, compressed and
ordinary sources; URL/blob/base64/byte loading, MIME and same-origin behavior, abort and cleanup;
embedded/external reference resolution and failure retention; dimension-resolver registration; and
test-helper install, measurement, refusal, and cleanup.

**Existing native CTest coverage:** `flight_cpp.generated_image_test_helper` registers the test
dimension resolver, resolves a 2x1 RGBA host source, checks the reported dimensions, and verifies
cleanup. `flight_cpp.host_sdl` also checks shared GL/WebGPU image identity and weak locking. Loading,
references, failure paths, and general resource mutation remain uncovered.

### `image-codec` — 3 modules

**Upstream modules:** `decodeImage`, `detectImageMimeType`, `encodeImage`.

**Asserted upstream behaviors:** explicit versus detected MIME dispatch, missing-codec and unknown
format refusal, premultiplied decode options, PNG/JPEG/GIF/WebP/AVIF/ICO/TIFF/BMP detection,
truncation rejection, and encode option/byte forwarding.

**Existing native CTest coverage:** none package-focused.

### `color` — 9 modules

**Upstream modules:** `colorFromKelvin`, `hslColor`, `hsvColor`, `lerpColor`, `luminance`, `oklab`,
`packColor`, `premultiplyColorAlpha`, `srgbTransfer`.

**Asserted upstream behaviors:** Kelvin clamping and warm/cool output; HSL/HSV allocation,
conversion, and round trips; clamped packed and linear interpolation; luminance and contrast using
the specified transfer and Rec. 709/2020 weights; Oklab round trips including out-of-gamut values;
packing, unpacking, CSS/hex formatting, alpha mutation; premultiply/unpremultiply edge cases; and
sRGB/linear transfer functions.

**Existing native CTest coverage:** none package-focused. Packed colors appear in materials and host
tests, but those do not assert this package's algorithms.

### `adjustments` — 20 modules

**Upstream modules:** `adjustment`, `brightnessContrastAdjustment`, `channelMixerAdjustment`,
`colorAdjustmentResolution`, `colorBlindSimulationAdjustment`, `colorGradeAdjustment`, `colorLut`,
`colorLutAdjustment`, `colorLutCache`, `colorMatrixAdjustment`, `colorMatrixMath`,
`colorScaleBiasAdjustment`, `exposureAdjustment`, `grayscaleAdjustment`,
`hueSaturationAdjustment`, `invertAdjustment`, `liftGammaGainAdjustment`,
`lookupTableGradeAdjustment`, `sepiaAdjustment`, `tintAdjustment`.

**Asserted upstream behaviors:** identity/default construction and initialization for every listed
adjustment; affine versus channel-mixing/LUT resolution; matrix concatenation and application;
color-blind, grading, exposure, grayscale, hue, inversion, lift/gamma/gain, lookup-table, sepia, and
tint math; LUT sampling/baking and content-keyed caching; and clamp, alpha, and alias behavior.

**Existing native CTest coverage:** none package-focused. Render tests exercise adjustment
attachment/composition concepts, not these construction and math suites.

### `capture` — 2 modules

**Upstream modules:** `captureBaseline`, `captureComparison`.

**Asserted upstream behaviors:** baseline allocation, independent fields and provenance,
deterministic formatting/parsing, malformed-input refusal, copy-on-write semantics; fingerprint
distance, grid mismatch and parse failure; and parity/regression tolerance boundaries.

**Existing native CTest coverage:** none package-focused.

### `snapshot` — 5 modules

**Upstream modules:** `captureSnapshot`, `enableSnapshotGuards`, `equalsSnapshot`,
`interpolateSnapshots`, `restoreSnapshot`.

**Asserted upstream behaviors:** deep copy/freeze and source independence; cycles and shared
subtrees; unsupported-value diagnostics; documented strict leaf equality; schema-selected and
all-numeric interpolation with clamping/snap behavior; and in-place, mutable, unaliased restoration,
including container replacement and array resizing.

**Existing native CTest coverage:** none package-focused.

## Web-only exclusion

`effects-canvas` is browser-only at the pinned revision and is intentionally outside this native GL
lane. Its tests are not evidence for any package above and are not included in the 307-module total.
No other environment-agnostic package is added to this inventory: the lane assignment is exhaustive,
and packages owned by CORE, 2D, 3D, TEXT, ANIM, or PLAT remain in their respective inventories.
