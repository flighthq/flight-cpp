# Camera native parity addendum

This is an additive audit of `@flighthq/camera` at Flight revision
`a62784923f3be814463286c9fb28edfe6ab15789`. Its baseline is the authoritative integrated commit
`8e214245`. Nothing from that live baseline was copied into this checkout, replaced, narrowed, or
edited.

## Authoritative live inventory

The exact snapshot supplied by Integration contains both registered camera-labeled tests:

| CTest name | authoritative source | lines | explicit checks | SHA-256 |
|---|---|---:|---:|---|
| `flight_cpp.generated_camera` | `tests/generated_camera_test.cpp` | 439 | 49 | `8f8955d4029faa097a1a933e8315a58f52079f2954e936abd06df6092ef1c000` |
| `flight_cpp.generated_camera_controls` | `tests/generated_camera_controls_test.cpp` | 530 | 58 | `165f48febc673090cf28525e2dd4255dccff6885c8a9e97760502f5e498231a0` |

The first target owns Camera2D projection/failure behavior; projection construction, kind and
ownership; Camera3D view, jitter, cache, picking, culling and depth failure seams; frustum corners;
reflection; tight shadow fitting; and guard state. The second is camera-labeled because its
controller tests also exercise Camera2D visible bounds and Camera3D look-at/world-to-screen behavior,
in addition to follow, fly, orbit, framing and shake.

The conditional `flight_cpp_upstream_camera2d_smoke` is not one of those two labeled targets, but is
relevant incidental evidence: it directly checks the exact 2D view matrix, visible bounds, three
parallax factors, anchored zoom and follow-driven motion.

This addendum registers only `tests/generated_camera_edge_test.cpp`, separately as
`flight_cpp.generated_camera_edge` with the `pkg:camera` label. Its 48 grouped checks contain only
axes whose mutants survived both authoritative targets.

## Pinned divergence

There is exactly one Camera divergence in `overrides/manifest.json`:
`camera-frustum-corners-reference-member-assignment`. It supplies
`overrides/include/flight/camera/frustum_corners.hpp` and records the package as complete at 21/21
headers with no other Camera defect.

The emitter wrote the three component assignments as direct member access on an
`Array<Vector3Like>` element even though that element is a `shared_ptr`. The override changes only
those three accesses from `.` to `->`, preserving the caller-owned pointees and TypeScript's
in-place mutation. Its SHA-256 stayed
`868b62e5e69b72f3f179c0bee51d6c7106f8acf2ce9fd7094b878e53db5c02b6` throughout this audit.

## Mechanical upstream census

The pinned Camera source contains 19 test files and 128 literal `it(...)`/`test(...)` declarations.
Separate scans found zero `it.each`, `test.each`, `describe.each`, or loop-/`forEach`-generated test
declarations.

| upstream module | declarations | native evidence after this audit |
|---|---:|---|
| `basis` | 6 | additive: signed forward, position, right and up extraction |
| `camera` | 18 | live construction/view/jitter/cache/near-plane state; additive orthographic aspect |
| `camera2d` | 5 | live defaults/look-at plus the Camera2D smoke |
| `cubeCapture` | 5 | additive square 90-degree projection, clip preservation, position and face direction |
| `culling` | 8 | live point, sphere and box front/behind/empty behavior |
| `depth` | 9 | live degenerate range; additive perspective endpoints/midpoint and orthographic mapping |
| `enableCameraGuards` | 6 | live public lifecycle; additive installation/removal of both callback seams |
| `explainCamera3DView` | 4 | additive identity, reflection and scaled-view diagnostics |
| `frustumCorners` | 3 | live success/failure and near/far mutation through the pinned override |
| `intersection` | 10 | live ray/plane success; additive sphere ray and parallel/behind failure branches |
| `parallax` | 3 | Camera2D smoke at three distinct factors |
| `picking` | 7 | live world/screen/ray round-trip, behind-camera and singular failures |
| `projection` | 15 | live construction/ownership/kinds/perspective use; additive texel, raw and orthographic matrix branches |
| `projection2d` | 4 | live rotated project/unproject round-trip and zero viewport |
| `reflection` | 8 | live double reflection; additive exact oblique depth coefficients and clip-plane mapping |
| `shadowCamera` | 5 | live tight/padded/fallback fit; additive regular sphere fit and placement |
| `viewMatrix` | 4 | Camera2D smoke exact matrix plus live rotated projection |
| `visibleBounds` | 6 | live rotated/degenerate bounds plus Camera2D smoke |
| `zoom` | 2 | live rotated anchoring plus Camera2D smoke anchoring |
| **total** | **128** | **19 source modules; zero unreachable Camera modules** |

This is behavioral-axis coverage, not a claim that all 128 Vitest bodies were ported statement for
statement. Constructor calls exercise their initializers directly, and higher-level culling and
picking calls exercise their matrix/frustum dependencies.

The warning callbacks installed by `enableCameraGuards` are verified as installed and removed, while
the diagnostic predicate they call is covered separately. Executing their final log payload is
outside the Camera boundary in this native profile: the existing Log override throws at
`merge_span_fields`, a known Log-package limitation. This addendum therefore makes no claim about
native warning payload delivery and does not misclassify that cross-package boundary as a Camera
header defect.

## Measured additive gaps

Comparison against the exact live sources found these distinct gaps, all now in the supplemental
target:

- signed 3D basis/position extraction and cube-face camera projection, position and direction;
- rigid, reflected and scaled view diagnostics;
- bounding-sphere ray success/empty/behind behavior and parallel/behind plane intersections;
- orthographic texel selection and aspect mutation;
- raw and orthographic `setProjectionMatrix4` branches;
- perspective and orthographic nondegenerate depth conversions;
- rotation-stable directional shadow fitting, including two-radius camera placement;
- exact oblique near-plane coefficients and mapping; and
- actual installation/removal of both public guard callbacks, beyond the live boolean lifecycle.

The live suite already closes Camera2D defaults/projection/zoom/bounds, Camera3D ownership and
perspective aspect, culling/picking, frustum-corner assignment, double reflection, tight shadow fit
and its own eleven recorded mutation probes. Those assertions remain solely in the authoritative
baseline.

## Frozen mutation evidence

The two authoritative sources were frozen at the hashes above; the supplemental source was frozen at
`ff26bc01fd3c2c52ffcbfbd31d2f710f96e5372872b559845643e6e5f70c096b`. Every mutant was applied
alone, confirmed as a one-file generated-header diff, compiled against both authoritative tests,
then compiled against the supplemental target and restored before the next mutant. The
frustum-corners override was never mutated.

All fourteen mutants passed both live targets and failed the supplemental target by the named
assertion:

| axis | isolated mutant | additive failure |
|---|---|---|
| basis | removed the negation from forward x extraction | signed forward x / cube +X direction |
| cube capture | square projection aspect `1` changed to `2` | square cube projection |
| view diagnostics | forced `is_orthonormal` false | identity/reflection classification |
| sphere intersection | empty-radius threshold `0` changed to `100` | visible sphere ray |
| plane intersection | parallel denominator sentinel `0` changed to `123` | parallel rejection |
| texel size | larger footprint selection changed from maximum to minimum | non-square texel footprint |
| camera aspect | orthographic half-width multiply changed to addition | retained half-height aspect |
| regular shadow | eye distance changed from two radii to three | scene-center placement |
| depth | perspective numerator factor `2` changed to `3` | near/far/midpoint depth |
| oblique clip | clip-plane scale factor `2` changed to `3` | exact depth row coefficients |
| raw projection | matrix copy changed to copying the output into itself | verbatim raw matrix |
| orthographic projection | left extent used half-height instead of half-width | symmetric half-extents |
| 3D guard wiring | enabling installed no Camera3D view callback | both callback seams installed |
| 2D guard wiring | enabling installed no visible-bounds callback | both callback seams installed |

The live source hashes and the pinned override hash were unchanged after the run. Generated and
override files have no repository diff.

## Verification

- standalone clean runs: both exact authoritative sources and the additive source passed;
- isolated mutation matrix: 14/14 survived both live targets and were killed by the additive target;
- focused CMake target: `flight_cpp_generated_camera_edge_tests`, built with at most `-j2`;
- package-scope and CMake/Bazel executable-source health checks are run after the task commit; and
- final tree audit confirms no generated or override changes.
