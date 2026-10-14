# Lazy uniform-location caches must keep their three states through a mutable local

`GlMeshProgram` caches optional uniform locations as `loc?: WebGLUniformLocation | null`. The three source
states are distinct and deliberate:
- `undefined`: not looked up yet;
- `null`: the compiled shader variant omits the uniform;
- a present location.

`glMeshProgram.ts` resolves each one lazily through an inferred mutable local:

```ts
// packages/scene3d-gl/src/glMeshProgram.ts:57 at a627849
let loc = program.locInstanceColorPalette;
if (loc === undefined) {
  loc = gl.getUniformLocation(program.program, 'u_instanceColorPalette');
  (program as GlMeshProgram).locInstanceColorPalette = loc;
}
if (loc !== null) gl.uniform1i(loc, INSTANCE_COLOR_PALETTE_TEXTURE_UNIT);
```

The active compiler patch for [external optional result widening](compiler-external-optional-result-widening.md)
handles an immutable result stored into the dual-sentinel cell. It does not handle the local above. That local
is inferred from the three-state cell and then assigned the binding's two-state `optional` result, so its
storage has to answer to both domains. The module refuses:

```
glMeshProgram.ts:65: external binding storage std::optional<flight::host_sdl::WebGlUniformLocation>
requires one unresolved optional value domain
```

## Why this was not visible at the live base

At the live base, `glMeshProgram.ts` first refuses at emitted line 295, in `isGlMeshAlphaCoverage`:
`(material as Readonly<Material3D>).alphaMode`. That site belongs to the pending `material-concrete-value-path`
([its request](material3d-concrete-value-path.md)).

To measure this patch's own refusal, a scratch probe that is **not** equivalent replaced that one assertion.
The module then stopped at line 65 with the message above, so this refusal family persists at the pin. The
patch can be judged on its own only after the material-value predecessor lands, or with the same probe.

## Current-pin sites

Seven lazy caches use the `let x = program.locX; if (x === undefined) { … }` shape, at pin lines:

| line | cache field(s) | uniform(s) |
| --- | --- | --- |
| 57 | `locInstanceColorPalette` | `u_instanceColorPalette` |
| 75 | `locInstancePalette` | `u_instancePalette` |
| 94 | `locUvTransform` | `u_uvTransform` |
| 147 | `locColorMatrix0` (also writes `1`–`3` and `Offset`) | `u_flightColorMatrix0`–`3`, `u_flightColorMatrixOffset` |
| 167 | `locColorScale` (also writes `locColorBias`) | `u_flightColorScale`, `u_flightColorBias` |
| 371 | `locObjectAlpha` | `u_objectAlpha` |
| 378 | `locAlphaIsCoverage` | `u_alphaIsCoverage` |

The pending patch's other hunks are its present-only helpers, which collapse non-lazy optional reads in
`bindGlMeshSkinPalette` and `drawGlMeshSubset`. They target code that still exists at the pin (46 of 52 removed
lines are still present). They are not measured here, because each lazy site above refuses first.

## Exact replacement (measured on the first three sites)

Each lazy lookup moves behind a field-specific resolver with an exact `WebGLUniformLocation | null` return:

```ts
function resolveGlInstanceColorPaletteLocation(
  gl: GlContext,
  program: Readonly<GlMeshProgram>,
): WebGLUniformLocation | null {
  if (program.locInstanceColorPalette !== undefined) return program.locInstanceColorPalette;
  const location: WebGLUniformLocation | null = gl.getUniformLocation(program.program, 'u_instanceColorPalette');
  (program as GlMeshProgram).locInstanceColorPalette = location;
  return location;
}
// at the caller
const loc: WebGLUniformLocation | null = resolveGlInstanceColorPaletteLocation(gl, program);
```

This resolver form was applied in scratch to the first three sites, at pin lines 57, 75 and 94, with the
alpha-coverage probe in place. All three emit. The module moves to the next site of the same family,
`uploadGlMeshDrawAlpha` (`locObjectAlpha`, pin line 371), with the identical message.

The resolver form therefore still works at the pin. The old patch cannot be applied mechanically: it was cut
against a stacked `7e2fc7d` file and does not apply to that commit's file alone. So it must be re-derived site
by site over the seven caches above, not 3-way merged.

## Equivalence

Each resolver:
- reads the same cache field;
- calls `getUniformLocation` exactly once, and only when that field is `undefined`;
- stores the identical `null`-or-location result back on the same program object;
- returns that result to the same uniform call.

So `null` stays cached as "omitted" and is never retried, and `undefined` stays "not yet looked up". The
present-only helpers map `undefined` to `null` only where the original code used a loose null check, which
treats the two identically.

Palettes, transforms, alpha values, colour adjustments, texture units, GL calls, cache objects and draw ordering
are unchanged.

## Focused tests

1. **Upstream lazy-cache semantics:** `glMeshProgram.test.ts`:
   - "resolves the location once and skips the upload for a null texture" (`:133`)
   - "uploads u_objectAlpha with the proxy alpha when the program has an object-alpha location" (`:240`)
   - "is a no-op for object alpha when the program has no object-alpha location" (`:279`)
   - "resolves u_objectAlpha lazily on first draw when unresolved (undefined)" (`:302`)
   - the instance-palette cases (`:80`, `:103`)

   Together they pin all three states and the single lookup.
2. **Lookup count:** count `getUniformLocation` calls per uniform across two draws. Expect exactly one for
   an initially `undefined` cache, and zero for both a pre-cached location and a pre-cached `null`.
3. **Focused generation:** generate scene3d-gl after the material-value predecessor and this patch. Assert
   that `glMeshProgram` emits with no `external binding storage` refusal, then compile `gl_mesh_program.hpp`.

## Status

The patch is still pending on the GL lane, behind `material-concrete-value-path` for measurement. This request
changes neither source patches nor generated inventory.

It expires on one of two conditions:
- flight-compiler keeps dual-sentinel property state through inferred mutable locals that receive a two-state
  external result;
- Flight adopts explicit lazy-location resolvers upstream.
