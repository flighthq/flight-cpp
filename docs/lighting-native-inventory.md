# Lighting native inventory

## Current pin and generated boundary

`@flighthq/lighting` is pinned to Flight `a62784923f3be814463286c9fb28edfe6ab15789`.
The package contains twelve behavioral modules plus `contract.ts` and `index.ts`. Scoped generation
emits 13 of 14 modules and refuses `lightAnalysis.ts`; the complete
`flight/lighting/light_analysis.hpp` override supplies that module without changing the native owner
carried by callers. `environment.ts` also retains a complete override because its texture union must
preserve the exact texture alternative and owner.

`sceneForwardLights.ts` uses the `lighting-dense-forward-selection-scratch` source patch. The arrays
remain dense and exact-owner: point and spot storage is separate, a write at the live-prefix length
uses `push`, and existing slots use indexed replacement. The underlying compiler defect is filed as
[An indexed array write at length must grow the array](compiler-requests/array-index-write-must-grow.md).

The generated inventory warns that `entity`, `geometry`, `log`, and `math` are still recorded under
older binding profiles during the repository-wide transition. This is not hidden as a lighting
success criterion: the lighting-only header compile is the ABI boundary check. No registry, render,
render-gl, effects, scene2d-gl, scene2d-wgpu, or scene3d-wgpu generated output, override, patch, or
inventory row is changed by this work.

## Upstream test inventory and native evidence

The twelve upstream suites contain 159 cases. `tests/generated_lighting_test.cpp` is the focused
renderer-independent native probe registered as `flight_cpp.generated_lighting` and labeled
`lighting`.

| Upstream suite | Cases | Native evidence |
| --- | ---: | --- |
| `ambientLight.test.ts` | 4 | Exact option propagation, kind, distinct clone identity, scalar copy |
| `areaLight.test.ts` | 6 | Defaults, normalized orientation, preserved half-extents, zero-input stability, four fresh clone vectors |
| `directionalLight.test.ts` | 10 | Cascade order/copy, complete shadow configuration, normalized direction and target, zero-input stability, fresh clone storage |
| `environment.test.ts` | 4 | Enabled/intensity propagation, exact texture variant identity, shared GPU resource across a distinct light clone |
| `hemisphereLight.test.ts` | 4 | Exact sky/ground colors, unit and intensity propagation, kind |
| `lightAnalysis.test.ts` | 45 | Enabled/shadow gates, packed-color luminance, finite influence bounds, overlap, disabled behavior, range window and decay contribution |
| `lightIntensity.test.ts` | 20 | Positive/negative exposure, unitless/lux/candela/lumen anchors, candela-lumen round trip, photometric set/get |
| `lightProbe.test.ts` | 30 | Position/coefficient copies, exact SH basis and normalized normal, alias-safe evaluation, trilinear grid sample, disabled-corner failure, cardinality failures |
| `pointLight.test.ts` | 4 | Position ownership, layer/priority/range propagation, complete shadow data, distinct clone owners |
| `sceneForwardLights.test.ts` | 12 | Independent four-light family budgets, priority-before-contribution, distance ordering, layer filtering, exact owner retention, positive/negative encoded source indices |
| `sceneLights.test.ts` | 4 | Null single-light defaults and materialized empty family arrays |
| `spotLight.test.ts` | 16 | Cone degree/cosine round trip, clamped blend, normalized target, zero-target stability, selection/shadow data, fresh clone vectors |
| **Total** | **159** | Focused exact/numeric native assertions across every behavioral module |

The native probe deliberately checks reference identity where the source promises copies or shared
resources; it does not replace those checks with value-only comparisons. Ordering assertions compare
the concrete owners and encoded source indices. Numeric assertions use exact equality for authored
fields and documented anchors, and tight tolerances only for trigonometric, luminance, SH, and
attenuation calculations.

## Failure and mutation evidence

The probe requires both invalid probe coefficient length and invalid grid cardinality to throw. It
also exercises safe zero-length transform inputs and a disabled weighted grid corner. Seven mutation
modes must make the executable fail:

- `--mutate-identity`
- `--mutate-order`
- `--mutate-transform`
- `--mutate-shadow`
- `--mutate-intensity`
- `--mutate-probe`
- `--mutate-failure`

These modes demonstrate that the identity, ordering, transform, shadow, intensity, probe, and
failure-path assertions are behaviorally active rather than incidental compile coverage.
