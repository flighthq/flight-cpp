# CORE lane — upstream parity inventory

The CORE lane is the shared data and services layer — type declarations, math and geometry, registries, codecs, and transport. This document is an inventory, not a scorecard: it
records, package by package, what the pinned upstream source asserts in its own tests and what
of that this repository covers today. It carries no judgement about which gaps matter.

## Pin and provenance

| | |
| --- | --- |
| `flight` | `a62784923f3be814463286c9fb28edfe6ab15789` |
| `flight-compiler` | `2b687eaf6b631e0fa4d7d6e18dad93b845f99633` |

Every upstream test file, case count, `describe` name, and example status below was read from
the `flight` checkout under the gitignored `.dependencies/` at the commit named above, and from
nowhere else. The *generated* figures are a different measurement: they come from the committed
`generated/manifest.json` and `generated/refusals.json`, whose tree was produced at flight
`7e2fc7d` and which a pin move does not regenerate by itself. A package whose upstream directory was
renamed at the newer pin is listed under the name the approved plan uses and read under the
name the pin ships (`registry-catalog` → `requirement-catalog`, `registry-codegen` → `requirement-codegen`, `requirements` → `requirement`).

## How the lane is defined

The lane is the approved fleet plan's package set, given verbatim. It is not a reading of the
upstream package map, which groups packages differently and would put, for instance,
`collision` and `spatial` outside an animation lane.

- `@flighthq/math`
- `@flighthq/geometry`
- `@flighthq/types`
- `@flighthq/signals`
- `@flighthq/command`
- `@flighthq/encoding`
- `@flighthq/compression`
- `@flighthq/xml`
- `@flighthq/abc`
- `@flighthq/net`
- `@flighthq/socket`
- `@flighthq/loader`
- `@flighthq/assets`
- `@flighthq/registry`
- `@flighthq/registry-catalog`
- `@flighthq/registry-codegen`
- `@flighthq/log`
- `@flighthq/debug`
- `@flighthq/requirements`
- `@flighthq/importdiagnostics`

## Method

- **Upstream tests** are the colocated `packages/<name>/src/**/*.test.ts` vitest files at the
  pin. The case count per file is the number of `it(`/`test(` blocks in it; the behaviors listed
  are that file's `describe` blocks, which upstream names after the exported function or object
  under test. A file with more than 12 blocks lists that many and names the
  remainder as a count; the file itself is one open away.
- **Existing CTest coverage** is read from `tests/CMakeLists.txt`, `examples/CMakeLists.txt`, and
  `examples/upstream/CMakeLists.txt`, and attributed by which of the package's *generated* (or
  hand-written override) headers a registered test source includes directly. A runtime header
  that merely shares a package's directory name — `flight/host/console.hpp` against
  `@flighthq/host` — is not SDK package coverage and is not counted. A test whose registered
  name also matches the package is marked *dedicated*; anything else reaches the package through
  the closure it links, and is *incidental*.
- **Web-only exclusions** are read from `flight.environment` in each package's own
  `package.json`, which is how this repository derives applicability (`deferred-packages.json`
  lists `applicableEnvironments: []`, so any declaration excludes the package). They are never
  inferred from a package name, and a web symbol in a refusal is evidence to read, not a
  classification — `@flighthq/webcam` was nearly parked for its name and is shippable, and
  `@flighthq/render-wgpu`'s `HTMLImageElement[value]` refusals are SDL image sources.

## Lane roll-up

| package | upstream modules | test files | cases | local CTest | web-only declared | generated emitted / refused |
| --- | ---: | ---: | ---: | --- | --- | --- |
| `@flighthq/math` | 17 | 15 | 325 | dedicated | none | 17 / 0 |
| `@flighthq/geometry` | 30 | 28 | 1198 | dedicated | none | 30 / 0 |
| `@flighthq/types` | 1026 | 47 | 160 | incidental | none | 1001 / 0 |
| `@flighthq/signals` | 10 | 8 | 86 | dedicated | none | 8 / 2 |
| `@flighthq/command` | 8 | 6 | 64 | dedicated | none | 6 / 2 |
| `@flighthq/encoding` | 3 | 1 | 8 | dedicated | none | 3 / 0 |
| `@flighthq/compression` | 7 | 5 | 91 | dedicated | none | 6 / 0 |
| `@flighthq/xml` | 4 | 2 | 50 | dedicated | none | 4 / 0 |
| `@flighthq/abc` | 4 | 2 | 17 | dedicated | none | 4 / 0 |
| `@flighthq/net` | 5 | 3 | 15 | dedicated | none | 5 / 0 |
| `@flighthq/socket` | 5 | 4 | 42 | dedicated | none | 5 / 0 |
| `@flighthq/loader` | 4 | 2 | 91 | none | none | 3 / 1 |
| `@flighthq/assets` | 5 | 3 | 33 | dedicated | none | 4 / 1 |
| `@flighthq/registry` | 3 | 1 | 12 | dedicated | none | 3 / 0 |
| `@flighthq/registry-catalog` | 4 | 1 | 15 | none | none | 4 / 0 |
| `@flighthq/registry-codegen` | 3 | 1 | 12 | none | none | 3 / 0 |
| `@flighthq/log` | 3 | 1 | 139 | dedicated | none | 2 / 1 |
| `@flighthq/debug` | 4 | 2 | 30 | none | none | 3 / 1 |
| `@flighthq/requirements` | 4 | 2 | 6 | dedicated | none | 4 / 0 |
| `@flighthq/importdiagnostics` | 4 | 2 | 7 | dedicated | none | 4 / 0 |

`upstream modules` counts the `.ts` files under `packages/<name>/src`; the generated manifest
counts its own `sourceModules`, and the two differ where declaration files or generated outputs
are involved.

**20** packages, **136** upstream test files, **2401** cases.
7 of 20 packages are reached by at least one local CTest executable.
5 by a test dedicated to the package: `math`, `geometry`, `socket`, `registry`, `log`.

## `@flighthq/math`

Scalar numeric utilities — interpolation, clamping, angles, rounding, randomness, hashing, statistics, and number theory

17 upstream modules under `packages/math/src`.
 Depends on `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/angle.test.ts` | 21 | `degToRad`, `deltaAngle`, `normalizeAngle`, `radToDeg` |
| `src/clamp.test.ts` | 17 | `clamp`, `inRange`, `saturate` |
| `src/comparison.test.ts` | 14 | `approxEqual`, `approxEqualRelative`, `approxZero` |
| `src/constants.test.ts` | 10 | `constants`, `CIRCLE_KAPPA`, `DEG_TO_RAD`, `EPSILON`, `HALF_PI`, `RAD_TO_DEG`, `TAU` |
| `src/hash.test.ts` | 17 | `createRandomSourceFromHash`, `hash2D`, `hash3D`, `hashCombine`, `hashUint32` |
| `src/interpolation.test.ts` | 24 | `inverseLerp`, `lerp`, `remap`, `smoothStep`, `step` |
| `src/interpolationAdvanced.test.ts` | 29 | `damp`, `lerpAngle`, `moveTowards`, `pingPong`, `repeat`, `smootherStep` |
| `src/nextPowerOfTwo.test.ts` | 18 | `isPowerOfTwo`, `nextMultipleOf`, `nextPowerOfTwo`, `previousPowerOfTwo`, `previousPowerOfTwo` |
| `src/numberTheory.test.ts` | 23 | `factorial`, `gcd`, `hypot2`, `isEven`, `isOdd`, `lcm` |
| `src/random.test.ts` | 5 | `createRandomSource` |
| `src/randomDistributions.test.ts` | 60 | `pick`, `randomExponential`, `randomGaussian`, `randomGaussianPair`, `randomInsideUnitDisc`, `randomInsideUnitSphere`, `randomOnUnitCircle`, `randomOnUnitSphere`, `randomPoisson`, `randomWeighted`, `shuffle`, `shuffleInPlace` |
| `src/randomRange.test.ts` | 18 | `randomBool`, `randomInt`, `randomRange`, `randomSign` |
| `src/rounding.test.ts` | 22 | `ceilTo`, `euclideanMod`, `floorTo`, `fract`, `roundTo` |
| `src/scalar.test.ts` | 13 | `ceilPowerOfTwo`, `floorPowerOfTwo`, `quantize`, `sign` |
| `src/statistics.test.ts` | 34 | `mean`, `median`, `standardDeviation`, `variance`, `weightedAverage` |

Upstream carries per-package records at `flight/agents/packages/math/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.upstream_math_geometry_parity` — `tests/generated/upstream_math_geometry_parity.cpp` (dedicated)
- `flight_cpp.example.sdk_math` — `examples/sdk_math_example.cpp` (dedicated)

### Local generation status

17 of 17 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/geometry`

2D/3D math primitives: vectors, matrices, rectangles, and object pools

30 upstream modules under `packages/geometry/src`.
 Depends on `@flighthq/entity`, `@flighthq/log`, `@flighthq/math`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/aabb.test.ts` | 41 | … `cloneAabb`, `containsAabbPoint`, `copyAabb`, `createAabb`, `expandAabbByPoint`, `expandAabbBySphere`, `getAabbCenter`, `getAabbExtents`, `getAabbSize`, `getClosestPointOnAabb`, `initializeAabb`, `intersectAabb` (+6 more describe blocks in the file) |
| `src/boundingSphere.test.ts` | 32 | `cloneBoundingSphere`, `containsBoundingSpherePoint`, `copyBoundingSphere`, `createBoundingSphere`, `getClosestPointOnBoundingSphere`, `initializeBoundingSphere`, `isBoundingSphereIntersectingBoundingSphere`, `mergeBoundingSphere`, `setBoundingSphere`, `setBoundingSphereFromAabb`, `transformBoundingSphereByMatrix4` |
| `src/capsule.test.ts` | 34 | `createCapsule`, `getClosestPointOnCapsule`, `initializeCapsule`, `intersectRay3DCapsule`, `isCapsuleIntersectingAabb`, `isCapsuleIntersectingCapsule`, `isCapsuleIntersectingSphere`, `setCapsule` |
| `src/enableGeometryPoolGuards.test.ts` | 3 | `areGeometryPoolGuardsEnabled`, `disableGeometryPoolGuards`, `enableGeometryPoolGuards` |
| `src/frustum.test.ts` | 21 | `createFrustum`, `getFrustumCorners`, `initializeFrustum`, `isFrustumContainingPoint`, `isFrustumIntersectingAabb`, `isFrustumIntersectingObb`, `isFrustumIntersectingSphere`, `setFrustumFromMatrix4` |
| `src/geometryPoolGuards.test.ts` | 1 | `setGeometryPoolReleaseGuard` |
| `src/matrix.test.ts` | 114 | … `cloneMatrix`, `copyMatrix`, `copyMatrixColumnFromVector3`, `copyMatrixColumnToVector3`, `copyMatrixRowFromVector3`, `copyMatrixRowToVector3`, `createGradientTransformMatrix`, `createMatrix`, `createTransformMatrix`, `equalsMatrix`, `initializeMatrix`, `inverseMatrix` (+25 more describe blocks in the file) |
| `src/matrix3.test.ts` | 86 | … `cloneMatrix3`, `copyMatrix3`, `copyMatrix3ColumnFromVector3`, `copyMatrix3ColumnToVector3`, `copyMatrix3RowFromVector3`, `copyMatrix3RowToVector3`, `createMatrix3`, `equalsMatrix3`, `getMatrix3Determinant`, `getMatrix3Element`, `initializeMatrix3`, `inverseMatrix3` (+14 more describe blocks in the file) |
| `src/matrix3Pool.test.ts` | 8 | `acquireIdentityMatrix3`, `acquireMatrix3`, `clearMatrix3Pool`, `releaseMatrix3` |
| `src/matrix4.test.ts` | 163 | … `appendMatrix4`, `appendRotationMatrix4`, `appendScaleMatrix4`, `appendTranslationMatrix4`, `cloneMatrix4`, `composeMatrix4`, `copyMatrix4`, `copyMatrix4ColumnFromVector4`, `copyMatrix4ColumnToVector4`, `copyMatrix4RowFromVector4`, `copyMatrix4RowToVector4`, `createMatrix4` (+41 more describe blocks in the file) |
| `src/matrix4Pool.test.ts` | 8 | `acquireIdentityMatrix4`, `acquireMatrix4`, `clearMatrix4Pool`, `releaseMatrix4` |
| `src/matrixPool.test.ts` | 8 | `acquireIdentityMatrix`, `acquireMatrix`, `clearMatrixPool`, `releaseMatrix` |
| `src/obb.test.ts` | 33 | `createObb`, `getClosestPointOnObb`, `initializeObb`, `intersectRay3DObb`, `isObbIntersectingAabb`, `isObbIntersectingObb`, `isObbIntersectingSphere`, `setObb`, `transformObbByMatrix4` |
| `src/plane.test.ts` | 24 | `clonePlane`, `copyPlane`, `createPlane`, `getClosestPointOnPlane`, `getPlaneCoplanarPoint`, `getPlaneSignedDistanceToPoint`, `initializePlane`, `normalizePlane`, `projectVector3OntoPlane`, `setPlane`, `setPlaneFromNormalAndPoint`, `setPlaneFromPoints` |
| `src/quaternion.test.ts` | 66 | … `cloneQuaternion`, `conjugateQuaternion`, `copyQuaternion`, `createQuaternion`, `equalsQuaternion`, `getQuaternionAngleBetween`, `getQuaternionAxisAngle`, `getQuaternionDot`, `getQuaternionEuler`, `initializeQuaternion`, `inverseQuaternion`, `multiplyQuaternion` (+11 more describe blocks in the file) |
| `src/quaternionPool.test.ts` | 7 | `acquireIdentityQuaternion`, `acquireQuaternion`, `clearQuaternionPool`, `releaseQuaternion` |
| `src/ray3d.test.ts` | 52 | `createRay3D`, `getClosestPointBetweenRay3Ds`, `getClosestPointOnRay3D`, `getRay3DPointAt`, `initializeRay3D`, `intersectRay3DAabb`, `intersectRay3DPlane`, `intersectRay3DSphere`, `intersectRay3DTriangle`, `ray-shape direction scale parity`, `setRay3D`, `transformRay3DByMatrix4` |
| `src/rectangle.test.ts` | 122 | … `cloneRectangle`, `computeRectangleIntersection`, `containsRectanglePoint`, `containsRectanglePointXY`, `copyRectangle`, `createRectangle`, `enclosesRectangle`, `equalsRectangle`, `expandRectangleToPoint`, `getRectangleBottom`, `getRectangleBottomRight`, `getRectangleLeft` (+30 more describe blocks in the file) |
| `src/rectanglePool.test.ts` | 8 | `acquireEmptyRectangle`, `acquireRectangle`, `clearRectanglePool`, `releaseRectangle` |
| `src/transform2d.test.ts` | 10 | `createTransform2D`, `decomposeMatrixToTransform2D`, `initializeTransform2D` |
| `src/transform3d.test.ts` | 4 | `composeMatrix4FromTransform3D`, `createTransform3D`, `decomposeMatrix4ToTransform3D`, `initializeTransform3D` |
| `src/typedarray.test.ts` | 15 | `reserveFloat32Array`, `reserveInt16Array`, `reserveUint16Array` |
| `src/vector2.test.ts` | 130 | … `addVector2`, `clampVector2`, `cloneVector2`, `copyVector2`, `createVector2`, `createVector2FromPolar`, `crossVector2`, `divideVector2`, `equalsVector2`, `getVector2Angle`, `getVector2AngleBetween`, `getVector2Distance` (+23 more describe blocks in the file) |
| `src/vector2Pool.test.ts` | 8 | `acquireEmptyVector2`, `acquireVector2`, `clearVector2Pool`, `releaseVector2` |
| `src/vector3.test.ts` | 100 | … `addVector3`, `clampVector3`, `cloneVector3`, `copyVector3`, `createVector3`, `createVector3FromSpherical`, `crossVector3`, `divideVector3`, `equalsVector3`, `getVector3AngleBetween`, `getVector3Distance`, `getVector3DistanceSquared` (+26 more describe blocks in the file) |
| `src/vector3Pool.test.ts` | 8 | `acquireEmptyVector3`, `acquireVector3`, `clearVector3Pool`, `releaseVector3` |
| `src/vector4.test.ts` | 84 | … `addVector4`, `clampVector4`, `cloneVector4`, `copyVector4`, `createVector4`, `divideVector4`, `equalsVector4`, `getVector4AngleBetween`, `getVector4Distance`, `getVector4DistanceSquared`, `getVector4Dot`, `getVector4Length` (+22 more describe blocks in the file) |
| `src/vector4Pool.test.ts` | 8 | `acquireEmptyVector4`, `acquireVector4`, `clearVector4Pool`, `releaseVector4` |

Upstream carries per-package records at `flight/agents/packages/geometry/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.upstream_math_geometry_parity` — `tests/generated/upstream_math_geometry_parity.cpp` (dedicated)
- `flight_cpp.generated_dialog` — `tests/generated_dialog_test.cpp` (incidental: includes the package as part of a larger closure)
- `flight_cpp_upstream_camera2d_smoke` — `examples/upstream/camera2d.cpp` (incidental: includes the package as part of a larger closure)
- `flight_cpp_upstream_motionpath_smoke` — `examples/upstream/motionpath.cpp` (incidental: includes the package as part of a larger closure)
- `flight_cpp_upstream_scene3d_smoke` — `examples/upstream/scene3d.cpp` (incidental: includes the package as part of a larger closure)

### Local generation status

30 of 30 modules emitted, 0 refused; 3 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/types`

Shared TypeScript interfaces, enums, and symbol constants used across all packages

1026 upstream modules under `packages/types/src`.
 This is a declaration layer — one module per named type or descriptor — so its tests assert shapes, kinds, and boundaries rather than computation.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/Adjustment.test.ts` | 3 | `Adjustment`, `open base contract` |
| `src/AdvancedBlendMode.test.ts` | 4 | `AdvancedBlendMode` |
| `src/AnimatedNormalModifier.test.ts` | 3 | `AnimatedNormalModifier`, `AnimatedNormalModifierKind`, `descriptor shape` |
| `src/BitmapCompositeMode.test.ts` | 3 | `BitmapCompositeMode` |
| `src/BlendEffect.test.ts` | 2 | `BlendEffect` |
| `src/CompositeEffect.test.ts` | 2 | `CompositeEffect` |
| `src/CompositeOperator.test.ts` | 3 | `CompositeOperator` |
| `src/CustomShaderMaterial.test.ts` | 1 | `CustomShaderMaterialKind` |
| `src/DissolveModifier.test.ts` | 2 | `DissolveModifier`, `DissolveModifierKind`, `descriptor shape` |
| `src/Effect.test.ts` | 3 | `Effect`, `open base contract` |
| `src/EmissiveModifier.test.ts` | 3 | `EmissiveModifier`, `EmissiveModifierKind`, `EmissiveModifierFacing`, `descriptor shape` |
| `src/Entity.test.ts` | 6 | `Entity`, `EntityRuntimeKey`, `EntityWithoutRuntime`, `Kind` |
| `src/EnvReflectModifier.test.ts` | 2 | `EnvReflectModifier`, `EnvReflectModifierKind`, `descriptor shape` |
| `src/FlightDocument.test.ts` | 9 | `FlightDocument`, `FlightDocumentScene`, `FlightDocumentScene2D`, `FlightDocumentScene2DMaterialization`, `FlightDocumentScene3D`, `FlightDocumentScene3DMaterialization` |
| `src/FlightDocumentLayout.test.ts` | 3 | `FlightDocumentLayout` |
| `src/FlightDocumentRefusalExplanation.test.ts` | 7 | `FlightDocumentRefusalReason` |
| `src/FlightDocumentToken.test.ts` | 4 | `FlightDocumentToken` |
| `src/FogModifier.test.ts` | 3 | `FogModifier`, `FogModifierKind`, `FogModifierMode`, `descriptor shape` |
| `src/GlContextLoss.test.ts` | 1 | `GlContextLossSignals` |
| `src/Layout.test.ts` | 2 | `layout header` |
| `src/LottieRegistry.test.ts` | 4 | `LottieLayerKind`, `LottieShapeItemKind` |
| `src/Material.test.ts` | 5 | `Material`, `Material base contract`, `MaterialLike`, `MaterialData` |
| `src/Material2D.test.ts` | 9 | `Material2D`, `Material3D` |
| `src/Menu.test.ts` | 3 | `menu Entity boundaries` |
| `src/MethodsOf.test.ts` | 3 | `MethodsOf` |
| `src/Modifier.test.ts` | 3 | `Modifier`, `open base contract` |
| `src/ModifierSlot.test.ts` | 3 | `ModifierSlot` |
| `src/Node.test.ts` | 6 | `Node`, `NodeKind`, `NodeOf`, `NodeAny`, `NodeRuntime` |
| `src/Node2D.test.ts` | 2 | `Node2DRuntime` |
| `src/PartialNode.test.ts` | 4 | `PartialNode` |
| `src/ParticleForce.test.ts` | 10 | `ParticleForce`, `closed discriminated union`, `union membership` |
| `src/Permission.test.ts` | 1 | `HostPermissionsCapability` |
| `src/Power.test.ts` | 3 | `power Entity boundaries` |
| `src/RequirementFacet.test.ts` | 2 | `RequirementFacet` |
| `src/ResourceResolutionState.test.ts` | 2 | `ResourceResolutionState` |
| `src/RimModifier.test.ts` | 3 | `RimModifier`, `RimModifierKind`, `descriptor shape` |
| `src/Scale9Sprite.test.ts` | 3 | `Scale9Sprite`, `Scale9SpriteKind`, `Scale9SpriteData` |
| `src/Scene3DResourceRef.test.ts` | 4 | `ImageResourceReference`, `ImageResourceReferenceKind`, `descriptor shape` |
| `src/SceneCoverageEntry.test.ts` | 3 | `SceneCoverageEntry` |
| `src/ShadedMaterial.test.ts` | 3 | `ShadedMaterial`, `ShadedMaterialKind`, `composable base material` |
| `src/Shell.test.ts` | 3 | `HostShellProcessCapability`, `ShellProcess` |
| `src/Signal.test.ts` | 5 | `Signal`, `Signal`, `SignalData` |
| `src/Socket.test.ts` | 2 | `TcpSocketConnection`, `TcpSocketOptions` |
| `src/Sprite.test.ts` | 2 | `Sprite`, `SpriteKind`, `SpriteData` |
| `src/StandardMaterial.test.ts` | 1 | `StandardMaterialKind` |
| `src/ToonModifier.test.ts` | 2 | `ToonModifier`, `ToonModifierKind`, `descriptor shape` |
| `src/VertexDisplaceModifier.test.ts` | 3 | `VertexDisplaceModifier`, `VertexDisplaceModifierKind`, `VertexDisplaceModifierSource`, `descriptor shape` |

Upstream carries per-package records at `flight/agents/packages/types/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_render_state` — `tests/generated_render_state_renderer_include_test.cpp` (incidental: includes the package as part of a larger closure)
- `flight_cpp.generated_render_state` — `tests/generated_render_state_test.cpp` (incidental: includes the package as part of a larger closure)
- `flight_cpp.host_sdl` — `tests/host_sdl_test.cpp` (incidental: includes the package as part of a larger closure)
- `flight_cpp.structural_row` — `tests/structural_row_test.cpp` (incidental: includes the package as part of a larger closure)
- `flight_cpp.example.sound_sdl` — `examples/sound/sound_sdl_example.cpp` (incidental: includes the package as part of a larger closure)
- `flight_cpp_upstream_sdl_host` — `examples/upstream/sdl_host.cpp` (incidental: includes the package as part of a larger closure)
- `flight_cpp_upstream_spatial_smoke` — `examples/upstream/spatial.cpp` (incidental: includes the package as part of a larger closure)

### Local generation status

1001 of 1001 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `types-log-sink-reference-parameter`, `types-render-state-deferred-cycle-body`, `types-surface-runtime-heritage`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/signals`

Strictly-typed signals and slots for event dispatching

10 upstream modules under `packages/signals/src`.
 Depends on `@flighthq/entity`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/connection.test.ts` | 11 | `connectSignalTracked`, `disconnectSignalConnection`, `pauseSignalConnection`, `resumeSignalConnection` |
| `src/emitter.test.ts` | 8 | `cancelSignal`, `emitSignal` |
| `src/internal.test.ts` | 1 | `nullSignalEmit` |
| `src/safe.test.ts` | 7 | `emitSignalSafe` |
| `src/scope.test.ts` | 15 | `createSignalScope`, `disconnectSignalScope`, `initializeSignalScope` |
| `src/signal.test.ts` | 3 | `createSignal`, `initializeSignal` |
| `src/slot.test.ts` | 27 | `clearSignal`, `connectSignal`, `disconnectSignal`, `emitSignal dispatch mutation`, `hasSignalSlots`, `isSlotConnected` |
| `src/throttle.test.ts` | 14 | `connectSignalAtFrameRate`, `connectSignalDebounced`, `connectSignalThrottled` |

Upstream carries per-package records at `flight/agents/packages/signals/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_signals` (dedicated) — `tests/generated_signals_test.cpp`, the native port of the `signal`,
  `internal`, `slot`, `emitter`, `safe` and `scope` cases at the pin: creation and its initial data and no-op emit,
  a null emit doing nothing, `clearSignal` removing every slot, connect with insertion order, typed arguments,
  scope membership, and `callable_identity_equal` on the `flight::Function` carrier — a bare lambda has no identity,
  so disconnect refuses it rather than guessing.
- `flight_cpp.generated_signals_throttle` (dedicated) — `tests/generated_signals_throttle_test.cpp`:
  `connectSignalAtFrameRate` as emitted (accumulating and keeping the remainder, cleanup disconnecting), and the
  debounced and throttled connects the package's one override supplies (leading-only, re-arm after the window,
  trailing with the most recent arguments, cleanup clearing the pending timer).
- Incidental: `generated_clipboard`, `generated_clock`, `generated_dialog`, `generated_lifecycle`,
  `generated_movieclip`, `generated_socket`, `generated_spritesheet`, `generated_timeline` — each includes the
  package as part of a larger closure.

### Local generation status

8 of 10 modules emitted, 2 refused; 5 refusing modules (2 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/command`

Undo/redo command history — commands are plain kind-tagged data (property set, add/remove/reorder child, composite) whose execute/undo/merge are free functions registered per kind in a keyed table and resolved at dispatch, with a stack tracking applied/redoable entries, per-kind merge coalescing for drag sequences, nestable transaction brackets that fold many commands into one entry, an optional maxSize cap, and an opt-in onChange signal; no closures on the stack, so entries stay inspectable data

8 upstream modules under `packages/command/src`.
 Depends on `@flighthq/entity`, `@flighthq/node`, `@flighthq/registry`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/command.test.ts` | 12 | `createAddNodeChildCommand`, `createCompositeCommand`, `createRemoveNodeChildCommand`, `createReorderNodeChildCommand`, `createSetNodePropertyCommand`, `createSetNodePropertyCommandBatch`, `initializeAddNodeChildCommand`, `initializeCompositeCommand`, `initializeRemoveNodeChildCommand`, `initializeReorderNodeChildCommand` |
| `src/commandBinding.test.ts` | 16 | `createCommandBindingTable`, `getCommandBinding`, `hasCommandBinding`, `initializeSetNodePropertyCommand`, `registerCommandBinding`, `registerDefaultCommandBindings` |
| `src/commandHistory.test.ts` | 17 | … `canRedoCommand`, `canUndoCommand`, `clearCommandHistory`, `createCommandHistory`, `executeCommand`, `getCommandHistoryEntries`, `getCommandHistoryIndex`, `getCommandHistoryRedoLabel`, `getCommandHistoryUndoLabel`, `initializeCommandHistory`, `notifyCommandHistoryChanged`, `redoCommand` (+1 more describe blocks in the file) |
| `src/commandHistorySignals.test.ts` | 4 | `enableCommandHistorySignals`, `getCommandHistorySignals` |
| `src/commandTransaction.test.ts` | 11 | `abortCommandTransaction`, `beginCommandTransaction`, `endCommandTransaction`, `isCommandTransactionOpen` |
| `src/explainCommandDispatch.test.ts` | 4 | `explainCommandDispatch` |

Upstream carries per-package records at `flight/agents/packages/command/`: `status.md`, `charter.md`.

### Existing CTest coverage

`flight_cpp.generated_command` (dedicated) -- `tests/generated_command_test.cpp`, the native port of the callable
cases of `command.test.ts` and `commandHistorySignals.test.ts` at the pin, against the `command.hpp` override and the
generated `command_history_signals.hpp`. Covers reading the current value, the before/after capture of an executed
command, and the history-signal connections. Gaps, as the test states them: `command_binding.hpp` and every header
including it stop in the registry row, which is still at 7e2fc7d -- its override names `flight::types::KeyedTable`,
which the a627849 types row no longer declares -- so the binding, history, transaction and explain-dispatch cases have
no callable surface; the two `NodeChildCommand` cases need a parent with attached children, which the node row cannot
supply without fabricating graph state.

### Local generation status

6 of 8 modules emitted, 2 refused; 7 refusing modules (2 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `command-undeclared-node-helpers`, `command-binding-nominal-table-boundary`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/encoding`

Portable UTF-8 conversion between strings and byte arrays

3 upstream modules under `packages/encoding/src`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/utf8.test.ts` | 8 | `decodeUTF8`, `encodeUTF8` |

Upstream carries per-package records at `flight/agents/packages/encoding/`: `status.md`, `charter.md`.

### Existing CTest coverage

`flight_cpp.generated_encoding` (dedicated) -- `tests/generated_encoding_test.cpp`, all eight cases of `utf8.test.ts`
at the pin, against the generated headers with no override. Covers two-, three- and four-byte sequences, the invalid
and truncated forms, and inputs that need exact UTF-16 code units (lone surrogates, NUL) built from `std::u16string`
so the input is the code-unit sequence the JavaScript string holds. No gap: every upstream case is ported.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/compression`

Compressed-byte decoders and the shared decompressor registry consumers resolve through

7 upstream modules under `packages/compression/src`.
 Depends on `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/compress.test.ts` | 24 | `compressDeflate`, `compressDeflateZlib`, `sdkHostCompressDeflate` |
| `src/deflate.test.ts` | 23 | `decompressDeflate`, `sdkHostDecompressDeflate` |
| `src/deflateFormat.test.ts` | 8 | `computeAdler32`, `deflateFormat tables` |
| `src/lzma.test.ts` | 20 | `decompressLzma`, `sdkHostDecompressLzma` |
| `src/lzmaCompress.test.ts` | 16 | `compressLzma`, `sdkHostCompressLzma` |

Upstream carries per-package records at `flight/agents/packages/compression/`: `status.md`, `charter.md`.

### Existing CTest coverage

`flight_cpp.generated_compression` (dedicated) -- `tests/generated_compression_test.cpp`, the native port of
`deflateFormat`, `compress`, `deflate`, `lzma` and `lzmaCompress` cases at the pin, against the generated headers
with no override; the compressed fixtures are the upstream ones, embedded as base64, so Node's zlib and Python's lzma
are independent oracles. Covers Adler-32 (initial value, the RFC 1950 checksum, order sensitivity, 32-bit
wraparound), the LENGTH and DISTANCE code tables and the distance alphabet's tiling, empty-input and deflate/LZMA
round trips. Not ported, as the test states: the two cases asserting a slot's `decompress` is the SAME function object
as the bare function, which `std::function` cannot express -- their behavioural twins are ported. The deflate cases
additionally depend on source-patches/compiler-constructor-parameter-property-assignment.patch; without it
`InflateState`'s parameter properties are never assigned and decompression returns null.

### Local generation status

6 of 6 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/xml`

Lightweight tree-building (DOM-style) XML parser: parseXmlDocument builds an XmlElement tree, plus parseXmlAttributes and element query helpers, for atlas, plist, and sprite-sheet format files

4 upstream modules under `packages/xml/src`.
 Depends on `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/xmlParse.test.ts` | 40 | `parseXmlAttributes`, `parseXmlDocument`, `parseXmlDocument internal DTD entities` |
| `src/xmlQuery.test.ts` | 10 | `getXmlElementAttribute`, `getXmlElementAttributeNumber`, `getXmlElementChildByName`, `getXmlElementChildrenByName` |

Upstream carries per-package records at `flight/agents/packages/xml/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

`flight_cpp.generated_xml` (dedicated) -- `tests/generated_xml_test.cpp`, every case of `xmlParse.test.ts` and
`xmlQuery.test.ts` at the pin, against the generated headers with no override. Covers attribute parsing with double
and single quotes, entity decoding (`&amp;`, `&lt;`, `&gt;`, `&quot;`, `&apos;`) and numeric character references,
and the query surface. No gap: every upstream case is ported.

### Local generation status

4 of 4 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/abc`

ABC (AVM2) bytecode container parsing into plain data

4 upstream modules under `packages/abc/src`.
 Depends on `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/abcFile.test.ts` | 8 | `readAbcFile` |
| `src/abcInstruction.test.ts` | 9 | `readAbcInstructions` |

Upstream carries per-package records at `flight/agents/packages/abc/`: `status.md`, `charter.md`.

### Existing CTest coverage

`flight_cpp.generated_abc` (dedicated) -- `tests/generated_abc_test.cpp`, all seventeen cases of `abcFile.test.ts`
and `abcInstruction.test.ts` at the pin, against the generated headers with no override; `build_abc` mirrors the
upstream `buildAbc()` byte for byte, so the reader is driven by a fixture the test builds itself. The reader depends
on source-patches/compiler-constructor-parameter-property-assignment.patch: without it `AbcReader`'s `source`
parameter property is never assigned and every read sees an empty buffer.

### Local generation status

4 of 4 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/net`

HTTP(S) transport (URLLoader/URLRequest) over a swappable web/native backend (fetch by default)

5 upstream modules under `packages/net/src`.
 Depends on `@flighthq/log`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/enableNetGuards.test.ts` | 5 | `areNetGuardsEnabled`, `disableNetGuards`, `enableNetGuards` |
| `src/net.test.ts` | 7 | `explainNetResponse`, `R3 boundary`, `sendNetRequest`, `setNetGuard` |
| `src/netForm.test.ts` | 3 | `formatNetFormBody`, `NetFormContentType` |

Upstream carries per-package records at `flight/agents/packages/net/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

`flight_cpp.generated_net` (dedicated) -- `tests/generated_net_test.cpp`, the callable cases of `net.test.ts`,
`netForm.test.ts` and `enableNetGuards.test.ts` at the pin. Covers request and response construction, the form
encoding path, and the guard notice surface. Not ported, as the test states: `enableNetGuards`' three warning cases,
because a warning reaches `log_once` -> `log` -> `merge_span_fields`, a stub in this profile's log override, so it
throws instead of logging, and the 'R3 boundary' case, which reflects over the module's runtime export object and has
no native counterpart.

### Local generation status

5 of 5 modules emitted, 0 refused; 3 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/socket`

Bidirectional persistent-connection transport (Socket/WebSocket) over a swappable web/native backend (WebSocket by default)

5 upstream modules under `packages/socket/src`.
 Depends on `@flighthq/entity`, `@flighthq/log`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/enableSocketGuards.test.ts` | 5 | `areSocketGuardsEnabled`, `disableSocketGuards`, `enableSocketGuards` |
| `src/explainSocketSendFailure.test.ts` | 4 | `explainSocketSendFailure` |
| `src/socket.test.ts` | 29 | `attachSocket`, `closeSocket`, `createSocket`, `detachSocket`, `disposeSocket`, `enableSocketSignals`, `getSocketReadyState`, `openTcpSocket`, `sendSocketMessage`, `setSocketGuard` |
| `src/socketHost.test.ts` | 4 | `createSocket` |

Upstream carries per-package records at `flight/agents/packages/socket/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_socket_statechart` — `tests/generated_socket_statechart_test.cpp` (dedicated)

### Local generation status

5 of 5 modules emitted, 0 refused; 3 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `socket-complete-lifecycle`, `socket-guard-reporting`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/loader`

Group resource loading with progress and completion signals

4 upstream modules under `packages/loader/src`.
 Depends on `@flighthq/entity`, `@flighthq/net`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/load.test.ts` | 2 | `loadBytes`, `loadText` |
| `src/resourceLoader.test.ts` | 89 | … `bandwidth throttle (maxBytesPerSecond)`, `byte progress`, `bytes progress`, `cancelResourceLoad`, `createResourceLoader`, `disposeResourceLoader`, `enableResourceLoaderItemSignals`, `error policy`, `getResourceLoadBytes`, `getResourceLoadCounts`, `getResourceLoadItemStatus`, `getResourceLoadProgress` (+14 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/loader/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 4 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/assets`

Id-keyed asset library over @flighthq/loader — manifests, group preload, refcounted ownership, and an open per-type loader registry

5 upstream modules under `packages/assets/src`.
 Depends on `@flighthq/entity`, `@flighthq/loader`, `@flighthq/log`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/assetLibrary.test.ts` | 27 | … `acquireAsset`, `createAssetLibrary`, `disposeAssetLibrary`, `getAsset`, `getAssetGroupIds`, `getAssetIds`, `getAssetRefCount`, `initializeAssetLibrary`, `loadAssetGroup`, `registerAssetDescriptor`, `registerAssetLoader`, `registerAssetManifest` (+3 more describe blocks in the file) |
| `src/enableAssetGuards.test.ts` | 4 | `areAssetGuardsEnabled`, `disableAssetGuards`, `enableAssetGuards` |
| `src/explainAssetLoad.test.ts` | 2 | `explainAssetLoad` |

Upstream carries per-package records at `flight/agents/packages/assets/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

`flight_cpp.generated_assets` (dedicated) -- `tests/generated_assets_test.cpp`, against the generated
`asset_library.hpp` and the hand-written `acquireAsset` the assets-reject-task-template-parameter override supplies,
so the two are exercised together. Covers an unknown id rejecting, an id with no loader for its type rejecting with
the loader's failure, and the reference count holding at two across two acquires.

### Local generation status

4 of 5 modules emitted, 1 refused; 4 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `assets-reject-task-template-parameter`, `assets-enable-asset-guards-callable-identity`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/registry`

Registry helpers — immutable kind-map and ordinal-table utilities

3 upstream modules under `packages/registry/src`.
 Depends on `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/registryTable.test.ts` | 12 | `concatKindMap`, `createOrdinalTable`, `getKindMapKeys`, `getOrdinalTableEntry`, `withKindMapEntry`, `withoutKindMapEntry` |

Upstream carries per-package records at `flight/agents/packages/registry/`: `status.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_registry` — `tests/generated_registry_test.cpp` (dedicated)

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `registry-table-entry-resolution`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/registry-catalog`

Caller-owned open registry of backend-specific registration remedies

4 upstream modules under `packages/requirement-catalog/src`.
 The pin ships this package as `@flighthq/requirement-catalog`; the approved plan names it `@flighthq/registry-catalog`.
 Depends on `@flighthq/entity`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/requirementCatalog.test.ts` | 15 | `createRequirementCatalog`, `findRequirementCatalogEntries`, `getRequirementCatalogEntries`, `registerRequirementCatalogEntry`, `unregisterRequirementCatalogEntry` |

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

4 of 4 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/registry-codegen`

Emission-neutral planning kernel for selective registry generation

3 upstream modules under `packages/requirement-codegen/src`.
 The pin ships this package as `@flighthq/requirement-codegen`; the approved plan names it `@flighthq/registry-codegen`.
 Depends on `@flighthq/entity`, `@flighthq/requirement-catalog`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/requirementCodegen.test.ts` | 12 | `createRequirementCodegenPlan`, `createRequirementCodegenPlan with dispositions`, `initializeRequirementCodegenPlan` |

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/log`

Leveled, capture-aware logging: structured entries to a sink, plus gated console output

3 upstream modules under `packages/log/src`.
 Depends on `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/log.test.ts` | 139 | … `addLogSink`, `beginLogGroup`, `clearLogChannelLevel`, `clearLogChannelLevels`, `clearLogGroups`, `clearLogOnceKeys`, `clearLogRedactionPaths`, `clearLogSerializers`, `clearLogSinks`, `clearMemoryLogSink`, `createBufferedLogSink`, `createChildLogContext` (+50 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/log/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_log` — `tests/generated_log_test.cpp` (dedicated)

### Local generation status

2 of 3 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `log-module-qualification-and-operators`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/debug`

Opt-in diagnostics umbrella over @flighthq/log — an open subsystem registry driving per-channel log levels, guard-enablers, and a dev console sink

4 upstream modules under `packages/debug/src`.
 Depends on `@flighthq/log`, `@flighthq/render`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/debug.test.ts` | 19 | `disableDebug`, `enableDebug`, `enableFlightDiagnostics`, `isDebugEnabled`, `registerDebugSubsystem`, `unregisterDebugSubsystem` |
| `src/debugTiming.test.ts` | 11 | `beginDebugSpan`, `endDebugSpan`, `markDebugFrame`, `measureDebugSpan` |

Upstream carries per-package records at `flight/agents/packages/debug/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 4 modules emitted, 1 refused; 4 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `debug-timing-log-timer-boundary`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/requirements`

Plain requirement collection and set algebra for content-to-registration resolution

4 upstream modules under `packages/requirement/src`.
 The pin ships this package as `@flighthq/requirement`; the approved plan names it `@flighthq/requirements`.
 Depends on `@flighthq/entity`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/requirementCollector.test.ts` | 2 | `collectScene2DRequirements`, `reportRequirement` |
| `src/requirementSet.test.ts` | 4 | `createRequirementSet`, `diffRequirementSets`, `initializeRequirementSet`, `mergeRequirementSets` |

### Existing CTest coverage

`flight_cpp.generated_requirement` (dedicated) -- `tests/generated_requirement_test.cpp`, ported from the two
upstream test files: the copied, sorted, deduplicated pair of dimensions, the covered-facet difference, the merged
facts with intersected completeness, the collector's four scene2d facets, and the identity a requirement is
deduplicated by. It also carries the compiler defect that port exposed -- a U+0000 separator emitted through a
strlen-measured `const char*` was truncated, made the identity a bare concatenation and silently dropped a
distinct requirement -- which is filed in `docs/compiler-requests/nul-string-literal-truncated.md`.

### Local generation status

4 of 4 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/importdiagnostics`

Shared structured-diagnostics seam for the *-formats importers — reportImportDiagnostic colocates a stable-kind ImportDiagnostic at each parser data-dropping branch (a no-op when no collector is passed, so an unopted parse pays only one undefined check), collectImportDiagnostics runs a parse with a collector installed as the drift-free explain* alternative to re-walking drop conditions, and formatImportDiagnostic renders one line for humans; deps types only, sideEffects false

4 upstream modules under `packages/importdiagnostics/src`.
 Depends on `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/importDiagnosticCollector.test.ts` | 5 | `collectImportDiagnostics`, `reportImportDiagnostic` |
| `src/importDiagnosticText.test.ts` | 2 | `formatImportDiagnostic` |

Upstream carries per-package records at `flight/agents/packages/importdiagnostics/`: `status.md`, `charter.md`.

### Existing CTest coverage

`flight_cpp.generated_importdiagnostics` (dedicated) -- `tests/generated_importdiagnostics_test.cpp`, ported from
the two upstream test files: the collector's crumbs and their order, the extended-crumb push and the absence of
detail, the silent path with no sink, and the rendered line's shape, sorted detail keys and no-detail case. The
generation unit that compiled this package added three cases the port did not carry -- a recorded crumb rendering
as its line, each detail value domain rendering as its own text, and the collector keeping two identical crumbs --
and mutation-checked the whole file.

### Local generation status

4 of 4 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## Lane notes

### Upstream examples

6 of the 34 upstream examples are ported and smoke-tested.
 None of the ported ones belongs to this lane.


### Upstream functional scenes

Upstream also asserts behaviour visually, as image-diff cells under `flight/functional/scenes/`
(383 scene files at the pin). No scene takes a package in this lane as its subject, so
there is no upstream image-diff evidence about one.

Some scene files do import a lane package directly, always incidentally — for a type or a
helper the renderer scene needs, never as its subject:

- `@flighthq/types` — 6 files (`application-render-view.webgl.ts`, `render-pass-shared-context.webgl.ts`, `render-pass-viewport.webgl.ts`)
- `@flighthq/geometry` — 2 files (`application-render-view.webgl.ts`, `render-pass-viewport.webgl.ts`)
- `@flighthq/log` — 2 files (`material-blend-modes.webgl.ts`, `material-blend-modes.webgpu.ts`)
- `@flighthq/signals` — 1 file (`application-render-view.webgl.ts`)
- `@flighthq/compression` — 1 file (`swf-mirrored-placement.ts`)

### Local substrate shared by every lane

These CTest executables are the repository's own parity surface. They cover the language
runtime and the emitted-SDK contracts rather than any one lane package, and they are listed
here so that a missing lane CTest is not read as an absence of local evidence about the runtime
the lane's emitted code calls.

| CTest | source | what it covers |
| --- | --- | --- |
| `flight_cpp.runtime` | `tests/runtime_test.cpp` | `Array`, `Map`, `Set`, `String`, `Number`, `Date`, `RegExp`, `Task`, `Blob`, typed arrays, `Any`, structural rows, Intl carriers |
| `flight_cpp.runtime_conformance` | `tests/runtime_conformance_test.cpp` + `conformance/portable-typescript-v1.json` | the committed profile semantics |
| `flight_cpp.generated_runtime` | `tests/generated_runtime_test.cpp` + `tests/generated/semantic_runtime.ts` | semantics of compiled-from-TypeScript containers, closures, and iteration |
| `flight_cpp.structural_row` | `tests/structural_row_test.cpp` | structural rows and the named-property view |
| `flight_cpp.c_api`, `flight_cpp.c_api_threads` | `tests/c_api_test.c`, `tests/c_api_thread_test.cpp` | the `Flight::C` foreign ABI |
| header self-containment | `tests/CMakeLists.txt` `flight_cpp_public_headers` | each installed `flight/*.hpp` compiles alone |
| `flight_cpp.host_sdl` | `tests/host_sdl_test.cpp` | the SDL host seam |
| `flight_cpp.upstream_math_geometry_parity` | `tests/generated/upstream_math_geometry_parity.cpp`, `tests/upstream_test_harness.hpp` | a pilot that lowers `@flighthq/math` and `@flighthq/geometry` upstream tests to native C++ (18 tests); it is the first local CTest sourced from the upstream suites this document inventories |

Outside CTest, `scripts/*Oracle.mjs` (`npm run <surface>:oracle`) compares individual runtime
surfaces against Node directly. Those are gates about the language runtime, not tests of a lane
package.
