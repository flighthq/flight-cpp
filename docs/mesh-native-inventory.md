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
| Weld equivalence and remapping | Welding compares every byte of the complete packed record: bit-exact duplicates collapse, an adjacent Float32 bit pattern does not, and equal positions with different packed attributes remain separate. Unique records retain first-source order, authored elements remap in exact order, invalid indices return an independent unchanged clone, and empty input stays empty. Sequential indexing switches from `Uint16Array` to `Uint32Array` only above 65,535 vertices. |
| Subset ownership and boundaries | Add and set replace the subset array and deep-copy every range without changing buffers or version. Triangle-list and strip lookup require all three elements inside one range, overlapping ranges choose the first declaration, gaps remain unowned, unsupported topologies return sentinels, and list/strip triangle counts follow their distinct formulas. |
| Merge order, rebasing, and isolation | Indexed and sequential sources concatenate in source order with exact vertex rebasing. Every source subset is independently cloned and rebased over the merged element stream, bounds are the exact union, and vertices, indices, subset arrays/elements, bounds, and runtime state are independently owned. Empty geometry remains valid; an empty request plus layout/topology mismatches return absence without touching inputs. |
| Validation branch matrix | The native oracle reaches the documented stride, alignment, index-range, every-float-channel, integer-packed-channel, subset integer/sign/bounds, subset primitive-shape, and whole-stream topology branches in their public order. Empty streams and valid point/list/strip shapes pass; truncated shapes fail. Every successful or failed call preserves all bytes, indices, ranges, owners, bounds/runtime identities, topology, and version. |
| Transform components and failures | A mixed-axis affine matrix pins column-major point and translation order under both distinct-output and in-place aliasing. The upstream-exposed non-uniform diagonal case distinguishes inverse-scale/inverse-transpose normal handling from plain-matrix tangent handling, preserving a unit perpendicular frame. Singular and below-threshold matrices reject without changing output owners, channels, bounds, or version; a determinant just above the threshold succeeds, and zero direction vectors stay zero. |
| Mirrored winding and handedness | A negative general-matrix determinant reverses every indexed triangle in place and negates every tangent handedness. Negative scale swaps complete second/third records for each non-indexed triangle, retaining attached UVs; strips deliberately retain element order while still flipping handedness. Positive determinants preserve both order and handedness, and no operation replaces the index or vertex owner. |
| Transform bounds and cache state | Translation and scaling update an existing AABB owner to exact extrema and advance the geometry version while deliberately leaving the runtime bounds stamp stale until the next ensure. Translation preserves normals, tangents, and UVs; centering computes missing bounds, and already-centered or position-less geometry is an exact no-op. |
| UV axes, order, and ownership | Offset then scale resolves `uv0` by semantic despite declaration/byte-order differences in a custom stride, keeps u/v axes distinct, and produces order-sensitive values. It retains the vertex/index/layout/bounds owners and every non-UV word, advances the generic version/cache stamp once per nonempty operation, does not silently rebuild tangent handedness after a UV mirror, and no-ops for absent or empty UV streams. |
| Morph capture and target/channel order | Capture resolves position, normal, and tangent by semantic despite declaration order, deinterleaves exact vertex order, and allocates independently owned immutable bases and reusable scratch. Blending pairs each target with its authored weight, retains geometry/layout/target/weight owners, restores from capture rather than current vertices, and rewrites only position/normal/tangent xyz while preserving tangent handedness and UVs. |
| Morph weight domain and sparse targets | Weights are additive coefficients rather than a clamped interpolation: zero skips, negative and greater-than-one values apply exactly. Short delta arrays affect only their prefix; unmatched extra targets and weights are ignored at the shorter count, and every input target and weight remains unchanged. |
| Morph accumulation and reset | Every direct blend resets the same scratch owner from the immutable base before accumulation. Repeated calls are value-idempotent, weight changes never compound from a prior frame, zero restores the base, and direct calls advance the version once even when their resulting bytes match. |
| Deformation clone isolation and bind-pose precedence | A deformation clone restores an exact morph bind pose ahead of an also-present skin pose, otherwise restores skin position and normal while leaving tangent data alone. It recomputes tight bounds, owns independent vertices/runtime/bounds, clears both deformation slots, and cannot mutate the already-deformed source; source slot getters preserve the exact installed owners. |
| Morph update runtime and bounds state | The node-level update lazily installs one bind-pose owner and a non-aliasing weight snapshot, reuses scratch and same-length snapshot owners, reallocates only when the weight count changes, and reblends from the base. Each changed update advances the version and leaves bounds stale until ensure refreshes the same AABB; settled weights plus null or undefined morph slots are exact no-ops. |
| Morph compatibility and failure atomicity | Missing optional normal/tangent deltas validly preserve those base channels and all unrelated data. Sparse targets and target/weight count mismatches follow the documented bounded prefixes, while an undersized bind-pose scratch raises before changing geometry bytes/owner/version, bounds/cache state, targets, or weights. |
| Packed channel ownership and order | A non-zero-offset `Float32Array` view is retained exactly with its authored layout. Attribute declaration order deliberately differs from byte order; semantic lookup still reads `uv1`, `color0`, `joints0`, and `weights0` into exact x/y/z/w component order from the second interleaved record. |
| Stride, offsets, and packed interpretation | Reads honor both the view's byte offset and the 32-byte vertex stride. Packed writes clamp and round `uint16x4`, clamp/round/normalize `unorm8x4`, preserve float UV interpretation, and increment the version exactly once per successful channel. Bytes outside the view remain untouched. |
| Normals, tangents, and groups | Smooth normals and tangents update in place without replacing vertex or index owners, produce unit +Z/+X frames for canonical UVs, preserve exact version increments, and distinguish positive from negative handedness. Position groups choose the first bit-exact representative, never escape vertex bounds, reject an adjacent Float32 bit pattern, and area-weight grouped normals across a seam. |
| Layout conversion | Conversion matches attributes by semantic rather than declaration order, installs the exact target layout and a fresh runtime/vertex/index owner while retaining subsets, zero-fills target-only components, preserves the source, and round-trips packed color/joint bytes through float storage. Canonical rigid/skinned strides and skin-channel offsets are asserted. |
| Primitive topology, order, and ownership | All 17 emitted builders assert an exact vertex/index count and compare every authored index in order for representative parameters. Every result is a valid `triangle-list` with finite orthogonal unit normal/tangent frames and applicable outward winding. Repeated calls share only the canonical layout: vertices, indices, subsets/elements, bounds, and runtime owners are isolated, and mutating one call cannot reach another. |
| Builder bounds | Every builder's cached AABB is recomputed independently from every emitted position and must be byte-for-byte tight at the current version. Box, capsule, plane, and torus also pin semantic half-extents/radius extents. |
| Primitive seams | Box corners retain face-specific records; capsule, circle, cylinder, ring, UV sphere, torus, and torus-knot closures retain duplicate positions/normals with their required UV endpoints. Cone apex vertices remain duplicated across segment U values. Spherical polyhedra and the icosphere use independent face records, lift crossing longitudes past 1, never leave a face split across 0/1, and place pole U between its face-mates. |
| Builder parameter guards | Every count-bearing builder floors fractional counts, clamps below-minimum counts, and throws `RangeError` for NaN and infinity on each count parameter. Zero-height cone/truncated-cylinder cases contain no dead triangles; a zero-height cylinder and collapsed/negative box or quad dimensions retain finite canonical records. Custom polyhedron seed positions are observably projected to the requested radius. |
| Failures | Negative and past-end triangle queries leave the caller's output unchanged. Negative and past-end position writes leave bytes and version unchanged. Invalid weld indices return a deep clone, merge mismatch returns absence, validation rejection is non-mutating, singular transforms preserve the entire destination, and incompatible morph scratch rejects atomically. Packed attributes with negative offsets or records extending past stride reject reads and writes without changing output, bytes, or version. |

Twenty-seven independent modes (`--mutate-owner-identity`, `--mutate-bounds`,
`--mutate-index-order`, `--mutate-failure`, `--mutate-channel-order`,
`--mutate-stride-offset`, `--mutate-normalization`, `--mutate-group-bounds`,
`--mutate-builder-topology`, `--mutate-builder-seams`, `--mutate-builder-bounds`,
`--mutate-builder-guards`, `--mutate-weld-equivalence`, `--mutate-index-remap-order`,
`--mutate-subset-boundaries`, `--mutate-merge-aliasing`,
`--mutate-validation-branches`, `--mutate-transform-components`,
`--mutate-mirrored-transform`, `--mutate-transform-bounds`,
`--mutate-uv-axes-order`, `--mutate-morph-target-order`,
`--mutate-morph-weight-math`, `--mutate-morph-reset`,
`--mutate-morph-aliasing`, `--mutate-morph-bounds`, and
`--mutate-morph-validation`) suppress one family oracle each. Normal execution must pass; every
mutation execution must fail its named assertion.

## Verification boundary

The package-scoped compiler gate passes all 16/16 emitted mesh headers with the declared override
stack. The exact `flight_cpp.generated_mesh` CTest builds and passes. The broader exact `pkg:mesh`
label contains that test plus the pre-existing frozen Scene3D consumer; it reports the mesh test as
passed but cannot build/run `flight_cpp.generated_scene3d` because
`overrides/include/flight/scene3d/mesh.hpp` includes the absent
`flight/types/material_value.hpp`. This mesh work does not modify or mask that frozen consumer.

## Honest residual

This is a focused P-3D2 slice, not a claim that all 213 upstream cases are ported. Ranked by remaining
observable breadth, the largest executable families are now: (1) the remaining vertex-format
permutations and accessor rejection matrix; (2) residual construction, compute, index, and operation
permutations outside the focused ownership and boundary oracles above; and (3) residual transform,
UV, morph/deformation, and bind-pose combinations beyond the representative edge matrices above. The
package currently has no refusal-ledger row, and this slice required no new production patch or
compiler request. Its
assisted-tier compiler debt remains explicit in the three existing source patches, one emission
repair, and six declared overrides rather than being recast as native parity.
