# Skeleton3D native inventory at a627849

The pinned `@flighthq/skeleton3d` package has 10 source modules. Package-scoped generation emits 9
and refuses 1. The package-scoped compiler gate nevertheless compiles all 10 public headers and
classifies the package as assisted and shippable.

That compiler result predates this parity slice. It is supplied by the existing
`skeleton3d-explicit-storage` source patch, the package's declared emission repairs, and two complete
overrides:

- `skeleton3d-complete-module` supplies the refused core module over exact Skeleton3D and Node3D
  owners, including construction, validation, clone/hierarchy operations, lookup, bind pose,
  palettes, equality, and disposal.
- `skeleton3d-prepare-scene-skinning-exact-owners` supplies mesh preparation and enabled-subtree
  traversal while retaining exact mesh, runtime-bounds, and child owners.
- The package-local repairs identified by the tier report cover explicit storage, unqualified type
  names, Node3D transform access, and exact mesh-weight ownership. The manifest additionally records
  the related joints and four-component attribute-owner recoveries.

This work does not alter those mechanisms or regenerate output. It adds independent behavioral
evidence for the already-compiling public package surface.

The upstream inventory contains 8 files, 62 `it(...)` cases, and 198 `expect(...)` calls:

| Upstream file | Cases | Assertions | Principal surface |
| --- | ---: | ---: | --- |
| `enableSkeleton3DGuards.test.ts` | 4 | 8 | guard enable, disable, and state |
| `getMeshSkinBounds.test.ts` | 5 | 26 | exact and conservative posed bounds |
| `prepareScene3DSkinning.test.ts` | 4 | 13 | mesh preparation and enabled scene traversal |
| `skeleton3d.test.ts` | 22 | 92 | construction, lookup, hierarchy, clone, bind/palette, validation, disposal |
| `skinMeshGeometry.test.ts` | 7 | 24 | bind-pose capture, geometry writes, deform-input refresh |
| `skinVertices.test.ts` | 15 | 28 | position, covector-normal, and tangent skinning |
| `updateMeshDeformation.test.ts` | 2 | 3 | morph-before-skin composition |
| `updateMeshSkin.test.ts` | 3 | 4 | runtime bind-pose installation and reuse |

## Focused P-3D2 native evidence

`tests/generated_skeleton3d_test.cpp` is registered as the exact
`flight_cpp.generated_skeleton3d` CTest with only the `skeleton3d` package label. It includes the
package contract rather than implementation headers and exercises the following material contracts:

| Evidence family | Native assertion |
| --- | --- |
| Construction, lookup, and failures | Construction sizes every matrix palette from dense joint order, retains authored names, resolves names and local-to-world joint matrices, rejects invalid indices without touching the destination, distinguishes absent names, reports exact validation lengths, and does not mutate the joint graph on failure. |
| Hierarchy and ordering | Hierarchy cloning calls the policy in dense joint order, rebuilds only parent links whose source parent belongs to the skeleton, leaves an external source parent untouched, and composes the cloned chain in parent-before-child order. |
| Clone ownership | Hierarchy clones independently own nodes, arrays, names, and every palette. Shallow clones share only joint nodes, independently own mutable carriers, compare equal initially, and stop comparing equal after isolated edits. |
| Bind pose and palettes | Bind-pose inversion and live joint matrices produce exact translations; normal palettes use the padded three-row layout and inverse-transpose scaling. Singular input takes the guarded fallback and later valid poses recover. |
| CPU skinning | Multiple influences blend in authored order, zero total weight retains bind pose, normals use covector math, tangents use vector math while preserving handedness, in-place aliases remain safe, and empty tangent streams are accepted. |
| Bind-pose capture and geometry writes | Interleaved geometry is deinterleaved into independently owned position, normal, tangent, joint, and weight carriers. Skinning rewrites only deformable channels in place and advances geometry version without replacing storage. |
| Deformation and runtime slots | Deform-input refresh retains static influence/scratch owners while updating position and normal input. Mesh skin updates install, reuse, clear, and bypass the bind-pose slot on the correct paths, including a rigid no-op. |
| Composition order | Changed morph weights recompose from immutable base geometry before refreshing the skin input and applying the current palette, preventing first-frame freezing and repeated-frame accumulation. |
| Exact and conservative bounds | Exact bounds skin every vertex tightly; conservative bounds contain that result and ignore unreferenced palette entries. Both paths preserve inputs and return the canonical empty-box infinities for empty geometry. |
| Mesh and scene preparation | GPU preparation captures and reuses bind-pose and bounds owners without posing CPU vertices, no-ops rigid meshes, traverses enabled children in order, and skips a disabled subtree whole. |
| Guard reset and disposal | Guard toggles expose exact state and reset the callback seam. Disposal releases joint/name references, retains caller-observable palette owners and bytes, and is idempotent. |

Ten independent mutation modes suppress one family oracle each:

- `--mutate-hierarchy-order`
- `--mutate-clone-ownership`
- `--mutate-palette`
- `--mutate-skinning`
- `--mutate-deformation-state`
- `--mutate-composition-order`
- `--mutate-bounds`
- `--mutate-failures`
- `--mutate-scene-preparation`
- `--mutate-reset-disposal`

Normal execution passes. Every mutation execution must fail its named assertion, so registration and
successful execution alone cannot masquerade as behavioral evidence.

## Verification boundary and residual

The package-scoped compiler gate passes 10/10 headers, and `pkg:tier` reports 9/10 emitted modules,
one refused module, zero failing or unattempted headers, and assisted/shippable status. The focused
CTest is registered under both CMake and Bazel; its CMake target builds and passes, and the repository
package-scope audit accepts its sole direct package label.

This is focused parity evidence, not a line-for-line port of all 62 upstream cases. The material
portable behavior families exposed by the current native contract are represented. Remaining cases
are lower-risk permutations such as additional joint counts, equivalent packed channel layouts, and
alternate logging payloads. The compiler debt remains explicit in the pre-existing patch, repairs,
and two complete overrides; this slice found no new source or compiler gap and therefore required no
compiler request, production patch, override, or generated-file change.
