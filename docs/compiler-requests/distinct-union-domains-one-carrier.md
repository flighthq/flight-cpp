# Distinct union domains must not collapse onto one carrier

`options.charstrings ?? [new Uint8Array([14])]` unions two array types whose elements are different
domains. The emitter refuses because both map to `flight::Array<flight::Uint8Array>` without a
discriminator or checked projection, so the value's origin would be lost.

## Minimal TypeScript

```ts
interface Options { charstrings?: readonly Uint8Array[] }
export function charstrings(options: Readonly<Options>): readonly Uint8Array[] {
  return options.charstrings ?? [new Uint8Array([14])];
}
```

## Required C++ shape

When both arms have the same element carrier and nothing downstream discriminates them, the union is the
carrier itself; a `??` with same-carrier arms needs no variant:

```cpp
return options->charstrings.has_value() ? options->charstrings.value()
                                        : flight::Array<flight::Uint8Array>{flight::Uint8Array{14}};
```

## Affected modules

`openTypeTestHelper.ts` in `@flighthq/font-formats` (a test helper): refused, not supplied.

## G2 WGPU screen target format fallback

At Flight `a627849` / compiler `806d257`, `render-wgpu/wgpuScreenRenderTarget.ts:66` refuses
`initializeWgpuScreenRenderTarget` as `cpp-union-runtime-domains-erased`, naming
`GPUTextureFormat | primitive -> flight::String`. The source computes
`const format = options.format ?? 'bgra8unorm'`; `bindings/web-types.json` already maps GPUTextureFormat
to String, and the options/target/host-attachment fields retain that scalar. Recover bound alias and
valid literal evidence without inventing distinct object owners, a format discriminator or a closed
subset of caller formats. Preserve absent/default versus supplied values and the original host call.

Original1 remains0/1 at missing initializer151, Clang22.1.8 jobs1,10.9s. The exact existing declaration
witness matches, but actual unused-header compilation fails at the separate
[GPUTexture size-property binding gap](webgpu-texture-size-properties.md) before link. The
[retained-callee evidence](retained-declaration-calls-omitted-callee.md#g2-wgpu-screen-target-initializer-declaration-cannot-pass-the-unused-header)
records the rejected candidate; no repair or normal generation is carried. Bind/destroy optional-call
refusals, live surface/context identity and actual resource acquisition/teardown remain separate.
