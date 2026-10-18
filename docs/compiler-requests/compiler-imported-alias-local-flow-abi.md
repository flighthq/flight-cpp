# Exact imported alias through local flow

An inferred local initialized from a nullable imported callable result discards the provider's exact
ABI alias, expands the source union for storage, and expands it again after a null guard.

## Minimal TypeScript

```ts
import type { HostImageSource, Resolver } from './provider.js';
declare function draw(image: CanvasImageSource): void;

export function resolveAndDraw(resolver: Resolver): void {
  const image = resolver();
  if (image === null) return;
  draw(image);
}
```

## Expected C++

```cpp
auto image = resolver(); // std::optional<provider::HostImageSource>
if (!image.has_value()) return;
draw_image(image.value());
```

The narrowed value remains the imported alias carrier rather than a reconstructed DOM union.

## Affected modules

Canvas resolver locals used by particle, quad, sprite, scale-9, tilemap, texture-window, and bitmap
pattern rendering.

## Carrying workaround

[`compiler-imported-alias-local-flow-abi.patch`](../../source-patches/compiler-imported-alias-local-flow-abi.patch)
preserves the foreign result alias in immutable local storage and returns its optional payload after a
presence proof. It expires when upstream local-flow lowering retains exact ABI aliases.

## G2 a reference-backed intersection alias: WebGPU effect pipelines

At Flight `a627849` / compiler `806d257`, `effects-wgpu/src/wgpuColorMatrixPass.ts` writes
`const pipeline = getWgpuEffectPipeline(state, …)`, where the callee returns the imported alias
`WgpuEffectPipeline = Entity & { pipeline; blendMode; compileForFormat?; variants? }`. It passes that local
straight to `drawWgpuEffectPass`. The callee is emitted correctly as returning
`Ref<flight::types::WgpuEffectPipeline>`, but the inferred local is re-expanded into a header-local anonymous
struct, `entity_runtime_key_pipeline_blend_mode_compile_for_format_variants_5a993344e208d790`, whose WebGPU
members are erased to `flight::Any`.

Neither conversion exists. GCC 15.2, one worker (1 attempted, 0 pass, 1 fail):
- `wgpu_color_matrix_pass.hpp:41` cannot initialize the local from the call;
- `:42` cannot pass it on as `Ref<WgpuEffectPipeline>`.

The required lowering is the same as above: the local keeps the provider's exact alias carrier, the same
`Ref<WgpuEffectPipeline>`. An anonymous re-expansion would copy the pipeline record and erase its WebGPU
handles. Nothing is carried: no conversion between the two structs, no copied pipeline, and no narrowed
domain.

## G3 regression exposed by typed descriptors: `effects_wgpu/wgpu_color_lut_pass.hpp` (pending this root)

Before `compiler-external-object-field-literal-type`, the emitter refused `applyColorLutPassToWgpu` with
`cpp-contextual-union-value-type-unrepresented`. The header compiled only because the function was missing. With typed
descriptor construction the function now emits, and its local `const fs = getWgpuEffectPassState(state)` is
re-expanded into a header-local anonymous struct whose WebGPU handle members are erased to `flight::Any`
(`uniform_bg_texture_bglayout_uniform_bglayout_sampler_acquire_slot_write_slot_begin_pass_9b3f1c7cd42b6c60`). The
providing module declares the same shape with typed handles (`..._6e85051ce2e6ac30`). That is this request's shape:
the inferred local must keep the provider's carrier.

The header fails at `wgpu_color_lut_pass.hpp:178` (`no viable conversion from Ref<..._6e85...> to Ref<..._9b3f...>`).
It is recorded as a lost pass, with the row moved to `no-viable-conversion-7e6bb150`, and nothing is suppressed.
`uploadLutTexture` (refused separately) is the next failure in the same function.
