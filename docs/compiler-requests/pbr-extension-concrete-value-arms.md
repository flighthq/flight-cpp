# PBR extension storage must retain each built-in extension owner

`ExtendedPbrMaterial.extensions` is `readonly PbrExtension[]`, and every GL extension registration receives
`Readonly<PbrExtension>`. The seven built-in GL registrations assert that value back to their concrete
interface:

```ts
// packages/scene3d-gl/src/anisotropyPbrGlExtension.ts at a627849
bind(context, value): void {
  const extension = value as Readonly<AnisotropyPbrExtension>;
  context.setFloat('u_flightAnisotropyStrength', extension.anisotropyStrength);
```

flight-compiler emits `PbrExtension` and each built-in interface as independent flattened owners. An extension
first retained through the open base arm has no cells for the built-in fields, so each registration refuses:

```
anisotropyPbrGlExtension.ts:6: the asserted row from flight::Ref<flight::types::PbrExtension> to
flight::Ref<flight::types::AnisotropyPbrExtension> reads anisotropyMap, anisotropyMapUvSet, … which the source
type does not declare
```

These are the same refusals at `clearcoat` (:13), `iridescence`, `sheen`, `specular` (:6), `transmissionVolume`
(:10) and `wrappedDiffuse` (:6). They root the package's `contract.ts`, `index.ts` and `scene3DGlPipeline.ts`.
The general compiler request is [row assertions to undeclared members](assertion-to-undeclared-row-member.md):
a checked owner recovery instead of a row read.

## Exact current-pin replacement

The pending `pbr-extension-concrete-value-arms` patch still states the right fix. It names the seven built-in
interfaces as redundant arms beside the open one, and carries that value union through storage and the GL
boundaries:

```ts
// packages/types/src/PbrExtension.ts
export type PbrExtensionValue =
  | PbrExtension
  | AnisotropyPbrExtension
  | ClearcoatPbrExtension
  | IridescencePbrExtension
  | SheenPbrExtension
  | SpecularPbrExtension
  | TransmissionVolumePbrExtension
  | WrappedDiffusePbrExtension;
```

| current source | replacement |
| --- | --- |
| `packages/types/src/PbrExtension.ts` | add the seven `import type … from './XPbrExtension.ts'` lines and `PbrExtensionValue` |
| `packages/types/src/ExtendedPbrMaterial.ts` | `extensions: readonly PbrExtensionValue[]` |
| `packages/types/src/GlPbrExtensionRegistration.ts` | `bind`, `createShaderContribution` and `isSupported` take `Readonly<PbrExtensionValue>` |
| `packages/types/src/GlScene3DRuntime.ts` | `pbrExtensionGuard?: ((extensions: readonly PbrExtensionValue[]) => void) \| null` |
| `packages/scene3d-gl/src/glPbrExtensionRegistry.ts` | `bindGlPbrExtensions`, `explainGlPbrExtensions` and `resolveGlPbrExtensionContributions` take `readonly PbrExtensionValue[]` |
| `packages/scene3d-gl/src/enableGlPbrExtensionGuards.ts` | `warnGlPbrExtensionIssues` takes `readonly PbrExtensionValue[]` |

The drift since `7e2fc7d` is only the `.ts` import specifiers. The seven built-in headers still exist, and
`PbrExtension.ts` is still re-exported through `@flighthq/types/contract`. The registration modules themselves
need no change.

One new reader at the pin is outside the patch: `Scene3DResources.ts` declares
`Scene3DPbrExtensionTextureLister = (extension: Readonly<PbrExtension>, out: Texture[]) => void`. It receives
the same values, so the owning lane should decide whether it joins the value union when the patch is
re-landed.

## Equivalence

Every named built-in interface already extends `PbrExtension`, so `PbrExtensionValue` accepts exactly the same
TypeScript values as `PbrExtension`. That includes arbitrary vendor extensions, through the open arm. Type
aliases and redundant union arms erase from JavaScript.

None of the following changes:
- extension objects, arrays, array order, or registration callbacks;
- duplicate-kind detection or unsupported-extension reporting;
- texture-unit accounting, shader composition order, binding, or identity.

The open public contract stays open. Vendor extensions remain accepted through the original arm.

## Measured at Flight `a627849` / compiler `2b687ea` with the live stack

The patch was re-derived at its manifest position and applied in a scratch worktree, together with
`scene3d-gl-erased-upload-cache`. It was measured with a scoped scene3d-gl generation:

- **The named refusal is cleared:** none of the seven registrations carries its own refusal. Each is now held
  back only by `glPbrExtensionRegistry.ts`, which depends on `glScene3DRuntime.ts`; see
  [the upload-cache request](scene3d-gl-erased-upload-cache.md) and
  [the result of `??=` is present](nullish-assignment-result-presence.md).
- **Emitted modules:** 39 → 46.
- **The re-emitted types headers compile:** `types/pbr_extension.hpp`, `types/extended_pbr_material.hpp` and
  `types/gl_pbr_extension_registration.hpp`.

## Stack and package coupling

- **Types regeneration:** the rewrite edits four `packages/types` sources, so re-landing it regenerates
  `@flighthq/types`.
- **Composition break:** the active `scene3d-gl-erased-program-cache` patch was rebased while this one was
  pending. Its `packages/types/src/GlScene3DRuntime.ts` hunk carries the unrewritten line
  `pbrExtensionGuard?: ((extensions: readonly PbrExtension[]) => void) | null;` as context. Applying this
  patch therefore makes that later active patch fail, unless its one context line is refreshed in the same
  move. Measured: with that line refreshed, the full stack applies in order.

## Focused tests

1. **Upstream registry semantics:** `glPbrExtensionRegistry.test.ts`:
   - "binds a resolved extension list" (`:41`)
   - "reports missing and duplicate registrations as plain data" (`:82`)
   - "reports unsupported extensions and extension texture-unit exhaustion" (`:91`)
   - "preserves descriptor order" (`:176`)

   Each registration's own suite, for example `anisotropyPbrGlExtension.test.ts` "registers only the
   anisotropy extension kind" (`:23`), pins that each binder still sees its concrete fields.
2. **Owner retention:** in focused generation, assert that no registration contains a row assertion from
   `PbrExtension`, and that each concrete binder reads its fields from the matching value arm.
3. **Vendor control:** a vendor extension with an unregistered kind must still be reported by
   `explainGlPbrExtensions` through the open arm. Losing the open arm would silently drop vendor support.

## Status

The patch is still pending on the GL lane. This request changes neither source patches nor generated inventory.
It expires when heterogeneous structural storage preserves checked concrete owner identity through open
base-interface arms, or when Flight adopts the value union upstream.
