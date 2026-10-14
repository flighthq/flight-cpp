# The single default material is already upstream: the pending patch has nothing left to answer

The pending `scene3d-gl-single-default-material` patch answered this refusal in `renderGlScene3D.ts`:

```
C++ runtime contract maps distinct union domains to one carrier without a discriminator or checked projection
(StandardMaterial | StandardMaterial -> flight::Ref<flight::types::StandardMaterial>)
```

At `7e2fc7d` that source had two module constants, `PROXY_DEFAULT_MATERIAL` and `DEFAULT_MATERIAL`, both
`StandardMaterial` literals. `material ?? DEFAULT_MATERIAL` formed two same-carrier arms. That is the general
case in [distinct union domains on one carrier](distinct-union-domains-one-carrier.md).

## What Flight `a627849` has instead

Upstream already made the change the patch made: there is now exactly one placeholder, and it is not a
`StandardMaterial`:

```ts
// packages/scene3d-gl/src/renderGlScene3D.ts:452 at a627849
const DEFAULT_MATERIAL = {
  alphaCutoff: 0.5,
  alphaMode: 'opaque',
  blendMode: BlendMode.Normal,
  doubleSided: false,
  kind: StandardMaterialKind,
} as Readonly<Material3D>;
```

That one constant serves the reused draw proxy (`proxy.material`, :467), pooled draw entries
(`createDrawEntry`, :438), and both null-subset fallbacks (`material ?? DEFAULT_MATERIAL`, :148 and :324).
`StandardMaterial` no longer appears in the module. At this pin it `extends Material2D`, which is why
[the material-value request](material3d-concrete-value-path.md) keeps this fallback in the open `Material3D` arm
rather than naming `StandardMaterial`.

The patch's helper `resolveAuthoredBlendMode` is also subsumed. `applyGlSurfaceBlendMode` now tests
`typeof surface.blendMode === 'string'` inline (:381).

## Measured

At the live base, `renderGlScene3D.ts`'s first refusal is the `Material` → `Material3D` row assertion at
emitted line 378 (`applyGlSurfaceBlendMode`). That site is owned by the pending `material-concrete-value-path`.
None of the seven scoped scene3d-gl generations measured for this batch contains a same-carrier or
distinct-union-domains refusal. That includes the baseline and the runs with the upload-cache, PBR-extension
and probe rewrites applied, where up to 47 modules emit.

The repin record counts 4 of the patch's 7 removed lines as still present. The surviving lines are the two
`material ?? DEFAULT_MATERIAL` call sites, which now coalesce into the single `Readonly<Material3D>` value, and
the placeholder comment, which upstream kept and extended. Neither of the two `StandardMaterial` constants
survives.

## Recommendation

Drop the pending patch as obsolete. The evidence:
- the construct it rewrote no longer exists at `a627849`;
- its single-placeholder design is upstream;
- the module's remaining blocker belongs to `material-concrete-value-path`.

The source-patch lane owns `source-patches/manifest.json` and the repin record's "Dropped as obsolete" table,
so this request does not edit them. Do not re-derive the patch: carrying a second placeholder or a
`MaterialValue` selector against the current source would re-introduce what upstream removed.

## Focused tests

These upstream suites pin the behaviour that the single placeholder must keep:
- `renderGlScene3D.test.ts` "skips a subset whose material has no registered renderer (no fallback)" (`:325`);
- "enables GL blend for subsets with alphaMode blend and disables it after" (`:153`);
- "keeps a fully-opaque mesh (node alpha 1) in the opaque pass" (`:310`).

Once `material-concrete-value-path` lands, a focused scene3d-gl generation should show `renderGlScene3D`
emitting with the default value in the open `Material3D` arm and no same-carrier refusal.
