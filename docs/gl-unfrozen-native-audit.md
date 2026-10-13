# Unfrozen GL native-behavior audit

This audit is pinned to Flight `a62784923f3be814463286c9fb28edfe6ab15789` and is limited to
texture, texture-formats, shading, materials, lighting, image, image-codec, surface, capture,
snapshot, quadbatch, color, and adjustments. It does not assess or modify registry, render,
render-gl, effects, effects-gl, scene2d-gl, scene2d-wgpu, scene3d-wgpu, or generated ownership rows.

The audit reads the upstream test intent in `.dependencies/flight/packages/*/src/*.test.ts` against
the focused native executables and their package registrations. Coverage means an observable native
assertion, not merely a compiling header. The ordering used to select work was absence, identity,
equality, ordering, exceptions, settlement, then format breadth.

| Package | Focused native evidence | Highest remaining audited gap |
| --- | --- | --- |
| `texture` | Construction covers absent defaults and exact source/resource identity; clone and video lifecycle cover shared versus copied owners. The focused test now also ports sampler and 2D texture equality, including null/undefined, same-owner, value-state, and distinct-source-identity cases. | Copy/initialize, source/readiness/size queries, and UV matrix/mutation behavior remain. Cube and render-texture behavior also reaches declared compiler refusals, so it is not an independent test-only slice. |
| `texture-formats` | The byte-exact test covers all nine upstream modules: bounded/endian reads, detection and explanations, level layout, selection, and ATF/Basis/DDS/KTX2 parsing and rejection. Two restored source mutations prove mip-count and normalized image-order assertions. | No high-priority semantic gap was found. More malformed-header combinations would add breadth rather than a missing behavior family. |
| `shading` | The focused test covers all modifier constructors, shared option identity, stable stack ordering, validation, registry isolation/replacement, built-in registration, and define keys. | Remaining cases are repeated option and invalid-value permutations; no uncovered absence, identity, equality, or ordering family was found. |
| `materials` | `materials-native-inventory.md` records constructor/default, owner identity, equality, ordering, validation, conversion, guard lifecycle, and eight mutation oracles across all 22 suites. | Lower-priority detail remains in color-scale/bias helper permutations, whole-object Phong conversion precedence, forwarded optional-map permutations, and exact diagnostic text. |
| `lighting` | `lighting-native-inventory.md` maps all 159 cases to native identity, math, ordering, exception, and failure assertions, with seven mutation modes. | No material portable behavior gap was found. |
| `image` | The focused test ports every portable case from image-resource, source-dimensions, and test-helper suites, including absence, borrowed identity, invalidation, bounds, cleanup, and failure behavior. | Reference resolution/failure retention and `imageResourceFrom` loading, abort, cleanup, and task settlement remain behind the filed erased-value/heritage and browser URL/location boundaries. They must not be simulated with native fakes. |
| `image-codec` | The byte-exact test ports every case in all three suites, including explicit MIME precedence, detection, option forwarding, missing-provider exceptions, and three restored mutations. | No high-priority semantic or format-family gap was found. |
| `surface` | The focused test ports every case across all five suites, including exact handle/runtime identity, dimensions, option routing, acquisition/release order, rejection, and four restored mutations. | No material portable behavior gap was found. |
| `capture` | All 30 upstream declarations plus lifecycle, byte layout, identity, malformed-input, parity, and regression boundaries are native, with seven restored mutations. | No material portable behavior gap was found. |
| `snapshot` | All 55 declarations plus graph order, aliasing, freeze, equality, restore, interpolation, cycle, and `DataCloneError` behavior are native, with eight restored mutations. | No material portable behavior gap was found. |
| `quadbatch` | `quadbatch-native-inventory.md` maps all 88 cases to exact native assertions and a layout mutation oracle. | Four generic node-bounds expectations remain at the documented flattened-owner boundary. The focused CTest is registered but is not yet discoverable through a `quadbatch` package label. |
| `color` | All 89 declarations (94 concrete parameterized cases) plus exact bytes, identity, aliasing, transfer thresholds, clamping, and six restored mutations are native. | No material portable behavior gap was found. |
| `adjustments` | All 137 declarations are grouped into exact native construction, resolution, order, matrix, LUT/cache, numeric, identity, clamp, alias, and exception assertions, with eight restored mutations. | No material portable behavior gap was found. |

## Implemented independent slice

`tests/generated_texture_test.cpp` now ports the upstream sampler and 2D texture equality contracts.
It distinguishes value-equal samplers from unequal fields, shared texture-source identity from an
equal-looking different owner, each common texture state field, same-owner equality, and all absent
operand directions. `--mutate-sampler-equality` and `--mutate-texture-equality` independently
invalidate the corresponding positive oracle; the baseline must pass and both mutation runs must
fail.

## Next useful work

The next independent semantic slice is texture copy/initialize alias safety and dimension-mismatch
exception behavior. Image settlement is more valuable globally, but is not independent at this pin:
it requires the existing browser URL/location and erased-owner compiler requests to land first.
