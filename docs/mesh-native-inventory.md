# Mesh native inventory at a627849

The pinned `@flighthq/mesh` package has 16 source modules. Fourteen implementation modules export
87 functions and two canonical-layout constants; `contract.ts` and `index.ts` re-export that surface.
Package-scoped regeneration emits all 16 modules with zero refused modules and no mesh header drift.
It records three existing source patches and the declaration-only `mesh-unqualified-types-names`
emission repair. Six declared complete overrides shadow generated headers for runtime-owner recovery,
attribute access, layout conversion, operations, compute, and deformation clone behavior; the scoped
compiler gate exercises those declared mechanisms rather than editing or silently bypassing them.

The upstream test inventory contains 14 files, 213 `it(...)` cases, and 586 `expect(...)` calls:

| Upstream file | Cases | Assertions | Declared implementation surface |
| --- | ---: | ---: | --- |
| `meshGeometry.test.ts` | 22 | 40 | construction, clone/metadata, runtime slots, counts, invalidation |
| `meshGeometryAttributes.test.ts` | 31 | 115 | channel lookup, reads/writes, packed formats, rejection |
| `meshGeometryBuilders.test.ts` | 22 | 76 | 17 primitive builders and degeneracy/seam rules |
| `meshGeometryCompute.test.ts` | 32 | 87 | bounds/sphere, normals, grouping, tangents, cache refresh |
| `meshGeometryDeformationClone.test.ts` | 2 | 9 | deformation clone and bind-pose precedence |
| `meshGeometryIndex.test.ts` | 15 | 59 | compaction, wireframe, expansion, indexing, welding |
| `meshGeometryLayout.test.ts` | 14 | 47 | canonical layouts and conversion |
| `meshGeometryOperations.test.ts` | 27 | 49 | attribute construction, triangle order, merge, validation |
| `meshGeometrySubset.test.ts` | 7 | 16 | subset replacement, counts, and triangle lookup |
| `meshGeometryTransforms.test.ts` | 19 | 38 | center/scale/transform/translate and winding invariants |
| `meshGeometryUvs.test.ts` | 7 | 13 | UV offset and scale |
| `morphMeshGeometry.test.ts` | 7 | 18 | capture and morph blending |
| `updateMeshMorph.test.ts` | 4 | 14 | mesh morph update state |
| `vertexFormat.test.ts` | 4 | 5 | byte/component counts and component I/O |

## Focused P-3D2 native evidence

`tests/generated_mesh_test.cpp` adds an independent mesh-labelled CTest over the public package
surface. It deliberately concentrates on the highest-risk observable contracts instead of treating
header compilation or the existing Scene3D smoke test as behavioral coverage:

| Evidence family | Native assertion |
| --- | --- |
| Geometry ownership and identity | Construction retains the caller's layout and vertex buffer, copies and narrows the authored index view, installs an exact runtime owner, and creates the default subset. Clone retains layout identity while independently owning geometry, runtime, vertices, indices, subset array/elements, and bounds; clone mutation cannot reach the source. Version invalidation wraps as unsigned 32-bit state. |
| Bounds and mutation | The first ensure computes the exact AABB and stamps the geometry version. An unchanged ensure returns the same bounds owner. A successful position write increments the version, after which ensure refreshes that same bounds owner to the new extrema and advances the runtime stamp. |
| Indexing and order | Triangle-list decoding preserves both authored windings. Wireframe edge order is exact. Index expansion follows element order without mutating the source, and compaction assigns new vertices in first-reference order with the exact remapped indices. |
| Failures | Negative and past-end triangle queries leave the caller's output unchanged. Negative and past-end position writes leave bytes and version unchanged. Out-of-range indices and zero-stride layouts fail validation while the original geometry remains valid. |

Four independent modes (`--mutate-owner-identity`, `--mutate-bounds`,
`--mutate-index-order`, and `--mutate-failure`) suppress one family oracle each. Normal execution
must pass; every mutation execution must fail its named assertion.

## Honest residual

This is a focused P-3D2 slice, not a claim that all 213 upstream cases are ported. The largest
remaining executable families are primitive-builder topology and seams; extended packed vertex
channels; normal/tangent and position-group computation; layout conversion; index/weld breadth;
subset operations; mirrored transforms; UV transforms; morph/deformation flows; and vertex-format
permutations. The package currently has no refusal-ledger row, and this slice required no new
production patch or compiler request. Its assisted-tier compiler debt remains explicit in the three
existing source patches, one emission repair, and six declared overrides rather than being recast as
native parity.
