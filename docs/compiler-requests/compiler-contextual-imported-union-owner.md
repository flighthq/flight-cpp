# Contextual construction of an imported union

Contextual construction enters an imported union alias's declaring module while still holding the
consumer-side import binding. The alias then degrades to a nominal reference instead of its declared
variant.

## Minimal TypeScript

```ts
// types.ts
export interface Source { kind: string }
export interface Bitmap extends Source { kind: 'bitmap'; bytes: Uint8Array }
export type SourceValue = Source | Bitmap;

// consumer.ts
import type { Source, SourceValue } from './types.js';
export function retain(source: Source | null): SourceValue | null { return source; }
```

## Expected C++

The result must be the declaring module's optional variant and construct its `Source` arm:

```cpp
std::optional<types::SourceValue> retain_source(std::optional<flight::Ref<types::Source>> source) {
  if (!source) return std::nullopt;
  return types::SourceValue{
      std::in_place_type<flight::Ref<types::Source>>, source.value()};
}
```

## Affected modules

Core texture queries and GL upload dispatch that return nullable imported `TextureSourceValue` in
`@flighthq/texture` and its consumers.

## P-3D2 residual boundary at the exact pins

The carried patch fixes the direct alias-owner handoff above. It does not settle every expression that
enters an imported union, and the render-gated Scene3D inventory still records four sites in this
family. The inventory was emitted from Flight `7e2fc7df5378313ceaf21ba8d6ef39cc18a6f4ad` and retained
while the dependency pin advanced to `a62784923f3be814463286c9fb28edfe6ab15789`; the reductions below
use flight-compiler `2b687eaf6b631e0fa4d7d6e18dad93b845f99633`, that current Flight pin, and the
committed runtime-carrier binding profile.

Two diagnostic-detail sites reduce exactly. `ImportDiagnostic.detail` and
`GltfCoreFeatureContext.reportDiagnostic` both contextualize values as
`boolean | number | string`. A property access whose declared result is unambiguously `number` loses
that evidence before the union is constructed:

```ts
interface Context {
  report(detail?: Record<string, boolean | number | string>): void;
}

export function lengths(context: Context, values: ArrayLike<number>, seen: Set<number>): void {
  context.report({ length: values.length });
  context.report({ count: seen.size });
}
```

Each call independently refuses with:

```text
contextual multiVariant construction requires expression type evidence
```

The controls bound the defect narrowly: `{ value: 1 }`, `{ value: 3 * values.length }`, and
`{ value: Math.min(...seen) }` emit. The imported `Record` and its three-arm value union are therefore
represented; the missing step is carrying the known result type of `ArrayLike.length` or `Set.size`
into the contextual number arm. These are the exact shapes in
`scene3d-formats/gltfAnimations.ts` (`firstActual: values.length`) and `md2Parse.ts`
(`distinctIndices: outOfRangeNormals.size`).

The other two sites are corpus-context boundaries rather than smaller reproductions:

- the AWD camera builder declares `let projection: Projection`, assigns only the imported
  `OrthographicProjection` and `PerspectiveProjection` arms, then stores it in a camera whose imported
  `Projection` also admits `RawProjection`. The recorded source carrier is the two-arm flow subset and
  the target is the three-arm declared alias. A two-package reduction with the same declared local and
  cross-package factory results emits, so this page records the exact site without claiming that the
  subset alone reproduces the refusal;
- the COLLADA morph diagnostic passes the guarded `morph.attributes.method` Record lookup into the
  same diagnostic-detail value union. The inventory records an optional-string source against the
  three-arm target, while the equivalent imported-function reduction emits. It therefore remains a
  rerun site after the exact property-evidence fix, not evidence for another request.

At `a62784923f3be814463286c9fb28edfe6ab15789` the AWD builder lives in
`awd2CameraHandler.ts` and the COLLADA helper in `colladaControllerDecoder.ts`; the retained inventory
names their pre-split `awd2Parse.ts:1978` and `colladaParse.ts:510` owners.

## Exhaustive Scene3D contextual-site map

The nine direct contextual/exact-union records in the render-gated `scene3d-formats` and
`scene3d-resources` rows are accounted for as follows:

| recorded site | existing request or carried resolution |
| --- | --- |
| `awd2Parse.ts:1978` | declared imported `Projection` flow subset; corpus site above |
| `colladaParse.ts:510` | imported diagnostic-detail value union; corpus site above |
| `gltfAnimations.ts:118` | exact `ArrayLike.length` reduction above |
| `gltfPunctualLights.ts:47` | [interface-heritage owner widening](compiler-interface-heritage-owner-widening.md) |
| `md2Parse.ts:381` | exact `Set.size` reduction above |
| `objParse.ts:603` | [typed-array static `from` and its following readonly-union insertion](typed-array-static-from-binding.md) |
| `gltfLoad.ts:36` | [existing Record value-carrier projection](contextual-record-value-carrier-projection.md) |
| `resolveScene3DResources.ts:175` | named by the carried [`texture-like-concrete-value-arms`](../../source-patches/texture-like-concrete-value-arms.patch) source patch; a named-arm control emits |
| `sceneDocumentSource.ts:16` | [equivalent structural rows inside generic owners](media-audio-mixer-runtime-set-union-conversion.md) |

This is the complete measured backlog for that two-package filter. It intentionally creates no new
request for a site already owned by one of these rules, and the source-patch site must be remeasured
only when the render gate permits regeneration.

## Carrying workaround

[`compiler-contextual-imported-union-owner.patch`](../../source-patches/compiler-contextual-imported-union-owner.patch)
resolves the import in the consumer before reconstructing the alias in its declaring module. It
expires when contextual union construction preserves imported alias ownership upstream.
