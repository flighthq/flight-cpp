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
| `texture` | Construction covers absent defaults and exact source/resource identity; clone and video lifecycle cover shared versus copied owners. Sampler/2D equality covers null/undefined, same-owner, value-state, and distinct-source identity. Copy/initialize evidence distinguishes construction-row identity, cloned mutable sampling state, shared source owners, preserved destination owners, self-alias safety, and fresh array/cube list storage; every legitimate dimension mismatch and both non-2D source setters reject before mutation. Source/readiness/size coverage now includes first-source selection, all-absent forms, 2D replacement/version transitions, CPU/GPU kinds, width/height/view derivation for 2D, array, cube, and 3D, non-positive dimensions, and voxel depth/data identity. | UV matrix and mutation behavior remains. Cube-only and render-texture helpers still reach declared compiler refusals, so they are not independent test-only slices. |
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

The copy/initialize follow-on exercises `initializeTexture2D` through its real
`EntityConstruction<Texture2D>` view and proves that `finishEntity` retains that row owner while
sampler and UV inputs are cloned. `copyTexture` covers the four dimension-specific content paths:
2D and 3D keep source identity, 2D-array and cube allocate fresh list storage, destination sampler
and UV owners survive, and copying an owner onto itself is safe. All twelve ordered mismatched pairs
among 2D, 2D-array, 3D, and cube reject with the exact error before changing destination state.
`setTextureSource` and `setVideoTextureSource` likewise reject all three non-2D dimensions before
changing their versions.

Nine independent mutations cover initializer view/copy ownership, destination owner preservation,
self-alias safety, list isolation, and the four destination-dimension guards. Two more mutations
independently suppress the ordinary and video source-setter dimension oracles. The per-arm dimension
checks after `copyTexture`'s leading equality guard are not independently reachable for a coherent
TypeScript discriminated union: once the two public dimensions compare equal, the selected source
arm necessarily has that same dimension. The native test therefore covers the complete legitimate
4-by-4 mismatch domain and does not manufacture an inconsistent C++ variant/discriminator pair.

The source/readiness/size follow-on exercises the common helpers over real 2D, 2D-array, cube, and
3D owners. It proves direct and first-non-null source identity, image/render-target/voxel kinds,
present and absent readiness, `-1` absent dimensions, CPU and GPU width/height, and the view-size
derivations for unrotated, quarter-turn, and half-turn transforms. A bound source with zero or
negative dimensions remains ready because readiness is source presence, while its view size is
zero. The 2D setter covers bind, same-owner no-op, replacement, clear, and u32 wrap. Voxel coverage
also preserves the exact source owner, its depth and byte-array identity, and invalidation wrap.
Twenty-nine focused mutation modes independently suppress source selection, kind, readiness,
replacement/clear/version, each width/height family, depth, per-form view size, absent-domain, and
invalid-size oracles.

This slice does not invent a `getTextureDepth` function: no such public helper exists at the pinned
Flight revision, so depth is observed only through the exact `VoxelGrid` owner returned by
`getTextureSource`. Likewise, generic cube first-source, width/height, readiness, and view behavior
is available and tested, but `getCubeTextureFaceSize` remains refused as
`cpp-contextual-union-missing-expression-type:optionalVariant`, `isCubeTextureComplete` remains
refused as `cpp-presence-test-without-absence-storage`, and `setCubeTextureFace` remains refused at
dynamic tuple projection. `createRenderTexture` still stops at the declared
`cpp-reference-assertion-without-heritage` boundary; using a real `RenderTarget` as a common texture
source tests the emitted generic source queries without substituting for that missing constructor.

## Next useful work

The next independent texture slice is UV matrices and mutation helpers. Image settlement is more
valuable globally, but is not independent at this pin:
it requires the existing browser URL/location and erased-owner compiler requests to land first.
