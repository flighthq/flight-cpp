# Scene3D Formats native parity re-audit — 2026-10-13

This is an additive audit of the callable native evidence for
`@flighthq/scene3d-formats`. It does not certify refused modules, placeholders,
or any Scene3D-GL or Registry behavior.

## Authoritative local baseline

Integration supplied `tests/generated_scene3d_formats_test.cpp` directly from
baseline `214a7976`. The immutable pre-audit file was 465 lines with SHA-256
`9a4f0963c39fc356baa6ea7fa7c44f36fcf054ab679c3561a58d190d5c2396c5`.
The matching authoritative CMake and Bazel snapshots register that single local
Scene3D Formats test source as:

- CMake target `flight_cpp_generated_scene3d_formats_tests` and CTest
  `flight_cpp.generated_scene3d_formats`, labeled `scene3d-formats`;
- Bazel target `generated_scene3d_formats_test`, also included in the aggregate
  generated test suite.

No second `tests/*scene3d_formats*` source is registered. The authoritative
override inventory contains no Scene3D Formats entry or path, so this audit did
not create, change, or claim an override.

The frozen test has seven composite evidence groups: AWD2 header parsing, MD2
feature detection, OBJ material-library references, recursive 3DS census,
glTF handler/Draco registries, callable MTL helpers, and callable MD5 animation
helpers. The last two headers contain useful emitted helpers even though their
public parse functions are refused; the checks below do not promote those
modules to emitted or claim the refused entry points work.

## Mechanical upstream map

The pinned Flight checkout is
`a62784923f3be814463286c9fb28edfe6ab15789`. A TypeScript-AST walk of every
`packages/scene3d-formats/src/*.test.ts` file found:

- 101 upstream test files;
- 1,296 ordinary `it`/`test` calls;
- 27 `it.each` sites containing 123 parameter rows;
- 1,419 expanded case executions; and
- 3,664 `expect` calls in source.

The parameterized sites were resolved through local array literals and named
constant arrays rather than counted as one case each:

| Upstream file | `each` sites | Parameter rows |
| --- | ---: | ---: |
| `awd2Parse.test.ts` | 2 | 32 |
| `colladaDecoderFamily.test.ts` | 2 | 6 |
| `colladaFeatures.test.ts` | 1 | 3 |
| `gltfParse.test.ts` | 1 | 5 |
| `gltfTreeShaking.test.ts` | 2 | 7 |
| `mergeAwd2ParseOptions.test.ts` | 3 | 6 |
| `mtlParse.test.ts` | 5 | 18 |
| `objMaterialModel.test.ts` | 2 | 9 |
| `objParse.test.ts` | 2 | 8 |
| `stlFeatures.test.ts` | 4 | 18 |
| `stlParse.test.ts` | 1 | 3 |
| `stlRequirements.test.ts` | 1 | 4 |
| `threeDsDocument.test.ts` | 1 | 4 |

The local native source is intentionally a callable subset, not a line-for-line
port of those 1,419 executions. Most upstream suites exercise modules that the
pinned compiler still refuses.

## Pinned compiler divergence

The authoritative generated manifest uses compiler
`2b687eaf6b631e0fa4d7d6e18dad93b845f99633` and records 111 package source
modules: 73 emitted and 38 directly refused. The refusal ledger also contains
45 dependency-propagated entries; those are consequences of the 38 direct
refusals, not additional emitted coverage.

The 38 direct refusal reasons group mechanically as follows:

| Refusal family | Direct modules |
| --- | ---: |
| Absent/optional payload not proven present | 11 |
| Contextual union or multivariant evidence missing | 6 |
| Erased structural owner cannot be recovered by assertion | 6 |
| Reference assertion has no emitted heritage path | 3 |
| Optional logical-OR lacks concrete value evidence | 3 |
| Ambient typed-array `from` binding missing | 2 |
| Structural-row construction lacks named properties | 2 |
| `Object.assign` target/source not exact | 1 |
| Unbounded collection spread lacks a fold | 1 |
| Dense `Array(length)` would require holes | 1 |
| Interface heritage has no owner-preserving carrier | 1 |
| Variant assertion does not identify one alternative | 1 |

Three declared source patches participate in this package:

- `scene3d-formats-optional-null-storage` (`mtlParse.ts`);
- `scene3d-formats-resource-ref-owner-lookup` (`shared.ts` plus image resource
  owner lookups); and
- `scene3d-formats-gltf-camera-diagnostic-details` (`gltfCameras.ts`).

Four emission-repair groups touch the emitted package closure:
`conditional-absent-branch-optional`, `array-from-tuple-construction`,
`scene3d-formats-unqualified-types-names`, and
`scene3d-formats-unqualified-image-codec-names`. These are generation facts,
not native coverage claims.

## Mutation audit and measured additions

The authoritative test was frozen before mutation. Seventeen exact unified
header diffs were compiled through isolated shadow include trees with at most
two concurrent compiler jobs. They covered binary length/endianness, feature
thresholds/order, whitespace and de-duplication, nested census/name formatting,
registry replacement/identity, callable defaults and optional presence,
diagnostic aggregation, component ordering, and reserved flag bits.

The frozen suite killed 9 of 17 mutants. Exactly eight survivors drove additive
assertions:

1. a one-frame MD2 contains a mesh but not animation;
2. an OBJ `mtllib` directive accepts a tab separator;
3. a small unknown 3DS chunk ID keeps four hexadecimal digits;
4. a second Draco registration replaces the first decoder;
5. every component of the canonical MTL default vectors is exact;
6. the remaining documented MTL optional defaults are absent;
7. an aggregated MTL tally retains the first directive and line; and
8. MD5 component counting ignores bits above the six defined flags.

Replaying those same eight diffs against the amended test killed all eight.
Together with the original nine kills, the final measured result is 17/17.

This remains evidence only for the callable helpers named by the seven local
groups. The 38 direct refusals, their dependency placeholders, full document
imports, Scene3D-GL, Registry-group behavior, runtime structure, and generated
SDK structure remain outside this audit.
