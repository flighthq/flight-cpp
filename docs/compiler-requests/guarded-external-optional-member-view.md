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
