# A bound WebGPU texture must expose its actual size properties

At Flight `a627849` / compiler `806d257`, the existing SDL binding maps `GPUTexture` to the shared
`flight::host_sdl::WgpuTexture` alias of `WgpuObject<WgpuTextureTag>`. Generated screen-target resize
code reads `texture.width` and `texture.height`, but the carrier exposes neither property. The real
texture's dimensions must be accessible through that binding while preserving its owner identity.

## Measured witness

Stock `flight/render_wgpu/wgpu_screen_render_target.hpp` remains0/1 at missing initializer151,
Clang22.1.8 jobs1,10.9 seconds. A separate actual unused header with only the exact initializer
declaration fails compilation at scratch159 (stock157): `no member named width` in WgpuTextureTag.
The source resize helper's width/height comparison is retained, and only width is the measured first
error; missing height is inspected on the same native carrier. No positive link/run or real-factory
negative object/link is reached. The otherwise valid declaration candidate is rejected.

```ts
const texture = target.depthStencilTexture;
if (texture.width === target.width && texture.height === target.height) return;
texture.destroy();
const replacement = createWgpuScreenDepthStencilTexture(target.device, target.width, target.height);
target.depthStencilTexture = replacement;
target.depthStencilView = replacement.createView();
```

## Required native contract

Expose the actual bound GPUTexture width/height through the provider/handle contract and match the
emitter's member access. Keep shared texture identity and lifecycle, including adopted/borrowed
handles; do not substitute target dimensions, add default numeric fields or infer correctness from an
unexecuted creation descriptor. The comparison decides whether to destroy/reallocate the original
depth-stencil storage, so fake dimensions can suppress a required resize or trigger extra destruction.

The target retains its exact device/context/live presentation-surface owner. Every pass resyncs the
surface extent, clamps dimensions and applies the antialias2x scale; matching texture size is a no-op,
while mismatch preserves source destroy/create/view/write order. Screen targets acquire fresh frame
views and are not sampleable texture targets. No fake target, surface snapshot or GPU provider is valid.

The initializer independently refuses the [format union](distinct-union-domains-one-carrier.md#g2-wgpu-screen-target-format-fallback).
Bind/destroy remain refused at optional-call evidence, and current native createTexture/createView
operations explicitly report unavailable; exposing dimensions alone supplies none of that behavior.
See [declaration qualification](retained-declaration-calls-omitted-callee.md#g2-wgpu-screen-target-initializer-declaration-cannot-pass-the-unused-header).
No C++/binding/repair/generated body, attachment, resize, capture or GPU lifecycle implementation is carried.
