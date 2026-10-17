# Guarded external optionals must unwrap before member lowering

A null guard over a nullable external call result does not narrow the local's C++ storage. The
following property read also bypasses the binding's `numericPropertyView`, producing a member access on
`std::optional` instead of a checked external-property call.

## Minimal TypeScript

```ts
export function anisotropy(context: WebGL2RenderingContext): number {
  const extension = context.getExtension('EXT_texture_filter_anisotropic');
  if (extension === null) return 0;
  return extension.MAX_TEXTURE_MAX_ANISOTROPY_EXT;
}
```

The binding declares the result as nullable and maps numeric properties to
`GlExtension::get_number(String)`.

## Expected C++

```cpp
auto extension = context.get_extension(
    flight::String("EXT_texture_filter_anisotropic"));
if (!extension.has_value()) return 0.0;
return extension.value().get_number(
    flight::String("MAX_TEXTURE_MAX_ANISOTROPY_EXT"));
```

Both halves are required: unwrap the proven-present local, then apply the member view to its payload.

## Affected modules

The `nativeGlExtensions` SDL/GL conformance probe and any nullable external value whose narrowed local
is read through a binding-provided member view.

## Carrying workaround

There is no compiler patch. [`sdlGlProfileOracle.mjs`](../../scripts/sdlGlProfileOracle.mjs) retains the
probe and prints this ownership-specific diagnostic when compilation fails. The gate is intentionally
not weakened or allowlisted; the request closes when the expected C++ above compiles.

## G2 WGPU capture guards the exact retained texture

At Flight `a627849` / compiler `806d257`, `wgpuScreenCapture.ts:73` acquire refuses height as
`cpp-optional-member-access-unproven` even though `existing = target.captureTexture` is checked for
null before both dimensions. Encode at93 binds the same property to `texture`, returns if disabled
or null, then refuses width. Preserve each local's exact present-value proof through the external
GPUTexture binding; do not repeat a getter, unwrap an absent optional or copy a target/texture owner.
Native [texture width/height support](webgpu-texture-size-properties.md) remains a separate contract.

Original1 stays0/1 at missing acquire122, Clang22.1.8 jobs1,12s. Two exact callback-value declarations
and the directly called map prototype match existing mechanisms but actual unused-header compilation
fails at native [getMappedRange](webgpu-buffer-mapped-readback.md) before link. The
[retained declaration request](retained-declaration-calls-omitted-callee.md#g2-wgpu-capture-three-exact-declarations-still-fail-native-readback)
records rejection. No repair/generation/texture/provider or readback implementation is supplied.

Preserve logical capture dimensions after AA scaling, existing texture reuse/reallocation, same-frame
copy, mapped/pending writer exclusion with unknown-state fallthrough,256-byte rows, retained-buffer
identity and actual mapped bytes. The separate [captured timer](captured-referent-mutation.md#g2-wgpu-capture-timer-initial-storage)
and normal-path unmap/timeout behavior cannot be replaced by a fake successful capture.
