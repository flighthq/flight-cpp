# Open modifier stacks and registries must retain concrete value owners

The modifier system has two deliberately open families. `Modifier` accepts built-in and vendor descriptors;
`ModifierDefinition` accepts substrate-neutral definitions and backend snippets. At Flight `a627849` and
flight-compiler `2b687ea`, storing either family through only its base interface discards the concrete native
owner. The GL path then asserts a `Modifier` back to AnimatedNormal, Dissolve, Emissive, EnvReflect, Fog, Rim,
Toon or VertexDisplace, and reads GL snippet callbacks after a `GlModifierSnippet` passed through a
`ModifierDefinition` registry.

The committed current-pin inventory stops first in
`packages/scene3d-gl/src/glShadedBuiltInModifiers.ts:352`: the asserted `Readonly<ToonModifier>` row asks for
`smoothness` and `steps`, but its retained `Modifier` owner declares neither cell. That is only the first
reached built-in; all eight callbacks have the same erased input shape. The definition side is latent behind
the same package dependency chain: a registry key identifies a snippet but does not prove the stored native
owner or restore its `bind`, `contribution` and `declarations` callbacks.

## Required native shape

Keep the open arms and redundantly name the concrete owners:

```ts
export type ModifierValue =
  | Modifier
  | AnimatedNormalModifier
  | DissolveModifier
  | EmissiveModifier
  | EnvReflectModifier
  | FogModifier
  | RimModifier
  | ToonModifier
  | VertexDisplaceModifier;

export type ModifierDefinitionValue =
  | ModifierDefinition
  | GlModifierSnippet
  | WgpuModifierSnippet;
```

Callback inputs use a distributed readonly form with exactly those nine modifier arms. The open arms keep
vendor kinds and substrate-only definitions accepted. The concrete arms let the existing checked
exact-owner union lowering select one native owner without a pointer cast, replacement row or identity map.

## Exact current-pin replacement targets

| current source | exact replacement |
| --- | --- |
| `packages/types/src/ModifierValue.ts` (new), `contract.ts`, `index.ts` | export `ModifierValue` and a duplicate-free distributed `ReadonlyModifierValue` |
| `ModifierDefinitionValue.ts` (new), `contract.ts`, `index.ts` | export the open definition union with GL and WGPU snippet arms |
| `ShadedMaterial.ts`, `ShadedMaterialOptions.ts` | store `readonly ModifierValue[]` |
| `ModifierDefinition.ts` | `getDefineSignature` receives `ReadonlyModifierValue` |
| `GlModifierSnippet.ts`, `WgpuModifierSnippet.ts` | every descriptor callback receives the same concrete-preserving readonly value |
| `ModifierRegistry.ts` | map `ModifierKind` to `ModifierDefinitionValue` |
| `packages/shading/src/getModifierDefineKey.ts`, `getUnregisteredModifierKinds.ts`, `isModifierStackValid.ts`, `orderModifierStack.ts` | carry `ModifierValue` through lookup, ordering and validation |
| `modifierRegistry.ts`, `registerBuiltInModifiers.ts` | register and resolve `ModifierDefinitionValue`; type built-in signature callbacks with `ReadonlyModifierValue` |
| `packages/scene3d-gl/src/glShadedBuiltInModifiers.ts`, `glShadedPrelude.ts` | carry the value union through every bind, declaration, contribution, compile and assembly callback |
| `glShadedMeshMaterialRenderer.ts` | replace the stale `shadedGlMeshMaterialRenderer.ts` target and carry `ModifierValue` through binding and the empty-stack constant |

The WGPU definition arm is required even though this is a 3D/GL batch: `ModifierRegistry` is the shared
substrate registry. Omitting it would make the type-only replacement narrow a public registry that currently
accepts either backend's definition.

One expression in the same pending patch is independent of owner retention. In
`glShadedBuiltInModifiers.ts`, replace

```ts
const dir = displace.direction ?? { x: 1, y: 0, z: 0 };
gl.uniform3f(gl.getUniformLocation(context.program, name), dir.x, dir.y, dir.z);
```

with one local read of `direction`, one local uniform-location lookup, and an explicit absent/present branch
that uploads `(1, 0, 0)` or the same `direction.x/y/z`. This avoids forming a union between the optional
`Vector3Like` owner and a fresh anonymous object owner.

## Equivalence

The `Modifier` and `ModifierDefinition` arms remain, so vendor descriptors, vendor definitions and both
backend snippet shapes remain accepted. The new unions and propagated annotations erase in JavaScript. The
same modifier objects stay in the same array; ordering remains stable within a slot; registry replacement is
still last-write-wins on the same `Map`; lookup uses the same kind; definition and snippet callback objects
retain identity; define keys, GLSL/WGSL, texture order and uniform uploads do not change.

The direction branch reads `displace.direction` once after frequency and speed, resolves the same uniform
location once and issues exactly one `uniform3f`. Present values upload the same three members; absence uploads
the same `1, 0, 0` default. Only the unobservable temporary default object allocation is removed. No callback
is duplicated and no evaluation is moved across a GL call.

## Focused tests

1. **Every value arm.** Build one stack containing all eight built-ins plus a vendor `Modifier`. Assert
   `orderModifierStack`, `getModifierDefineKey`, validation and unregistered-kind reporting preserve the
   existing order, tokens, duplicate handling and object identities. A test that reaches only Toon is
   insufficient because the current inventory stops at the first refusal.
2. **Every definition arm.** Register a substrate-only definition, a `GlModifierSnippet`, a
   `WgpuModifierSnippet`, and a same-kind replacement. Assert `resolveModifier` returns the exact objects and
   the last write, and that the GL callbacks remain callable after lookup. Generate the registry and both
   snippet contracts and verify each concrete alternative is present beside the open arm.
3. **GL callback path.** Generate `glShadedBuiltInModifiers`, `glShadedPrelude` and the renamed
   `glShadedMeshMaterialRenderer`; run their existing tests for all eight signature/contribution/bind families,
   cache-key reuse and last-write-wins snippet replacement. No base-to-concrete structural assertion may
   remain in those roots.
4. **Direction fallback.** Call the vertex-displacement binder once with no direction and once with a concrete
   direction. Assert one location lookup and one upload per call, exact `(1,0,0)` fallback values, exact present
   values, and unchanged frequency-before-speed-before-direction call order.

## Carrying workaround and status

`source-patches/modifier-concrete-value-path.patch` carries the older form against Flight `7e2fc7d`. It has a
`pendingRebase` marker because `shadedGlMeshMaterialRenderer.ts` became
`glShadedMeshMaterialRenderer.ts`; it is not applied at the current pin. The source-patch lane owns the rebase.
This request changes no patch, repair or generated file. It expires when heterogeneous open-interface storage
retains checked concrete owners, or when Flight adopts these explicit value unions.
