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
| Construction defaults and metadata absence | Omitted and explicit-null indices produce non-indexed geometry, while a present empty typed array remains an indexed zero-element value. Construction distinguishes `Uint16`/`Uint32`, promotes only above 65,535 vertices, floors partial records, returns zero vertices for zero stride, retains authored vertex/layout/subset owners, and supplies exact topology/subset/runtime defaults without mutating any input. Metadata-only cloning preserves the upstream shared live index and layout owners while deep-copying subsets and bounds, allocating an empty vertex stream and fresh runtime, resetting version, and preserving absent bounds/indices exactly. |
| Full-clone aliasing matrix | Full clones retain topology/layout and exact values, index width, and present-empty state while allocating independent vertex, index, subset-array/element, AABB/vector, and runtime owners. Runtime binding, UID, attribute-view, upload, morph/skin, and blended-weight slots are deliberately not inherited, and mutating every cloned CPU carrier cannot reach the source. |
| Runtime lifecycle ordering | GL and WebGPU upload teardown clears each slot to true absence independently in either order and is idempotent when repeated. It leaves CPU buffers, descriptors, version/bounds cache, binding, UID, morph/skin owners, and blended weights untouched. |
| Runtime reset and missing-owner paths | Morph and skin bind poses retain their exact installed owners, clear independently, and tolerate repeated reset. Runtime lookup falls back to the identity-keyed owner after the public runtime cell is cleared; a manually constructed geometry with neither runtime route answers absent and every setter/teardown call is a non-mutating no-op. |
| Cache invalidation lifecycle | Direct public typed-array writes deliberately remain invisible to cached bounds until `invalidateMeshGeometry` advances the revision; invalidation itself retains the AABB and upload slots, and the next ensure refreshes the same bounds owner and stamp. Repeated invalidations accumulate, while repeated empty-geometry bounds queries remain absent and preserve CPU state. |
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
| Complete vertex-format component table | All six emitted formats (`float32x2/x3/x4`, `uint16x4`, `uint8x4`, and `unorm8x4`) report exact byte/component counts. Every component is written and read at a deliberately unaligned DataView base, with raw little-endian float/uint16 bytes, integer clamp/round, normalized quantization, and untouched adjacent bytes pinned independently. |
| Systematic interleaving and conversion | Two records carry all six formats at distinct offsets, including an unaligned packed `uint16x4`, inside a non-zero-offset view with padding and declaration order unrelated to byte order. Semantic conversion to six `float32x4` channels preserves every represented value, zero-fills only missing components, round-trips each source format, isolates output owners, and cannot mutate source or surrounding backing bytes. |
| Accessor representation matrix | A shared table exercises `color0`, `joints0`, and `weights0` across every four-component representation (`float32x4`, `uint16x4`, `uint8x4`, `unorm8x4`), plus the permitted `float32x3` color default and `float32x2` UV path. Reads/writes preserve exact float values, clamp/round only integer storage, decode normalized storage, supply color alpha 1, retain owners/padding, and version once. |
| Accessor bounds and invalid domains | Unknown format size/count/read calls throw while unknown void writes are exact no-ops. Negative and past-end components throw, fractional components follow DataView integer coercion, and exact-size views prevent partial writes. Unknown attribute formats, negative/spilling offsets, zero stride, partial records, invalid vertex indices, and misaligned float attributes follow their public throw-or-false/validation domains without mutating outputs, bytes, owners, or versions. |
| Accessor missing/default paths | Every getter and setter for position, normal, tangent, UV0/UV1, color, joints, and weights rejects an absent semantic with outputs, bytes, owners, and version untouched. Wrong component formats reject the same way; duplicate semantic lookup returns the first declaration, missing lookup returns absence, and missing or packed float offsets return `-1`. |
| Normals, tangents, and groups | Smooth normals and tangents update in place without replacing vertex or index owners, produce unit +Z/+X frames for canonical UVs, preserve exact version increments, and distinguish positive from negative handedness. Position groups choose the first bit-exact representative, never escape vertex bounds, reject an adjacent Float32 bit pattern, and area-weight grouped normals across a seam. |
| Layout conversion | Conversion matches attributes by semantic rather than declaration order, installs the exact target layout and a fresh runtime/vertex/index owner while retaining subsets, zero-fills target-only components, preserves the source, and round-trips packed color/joint bytes through float storage. Canonical rigid/skinned strides and skin-channel offsets are asserted. |
| Primitive topology, order, and ownership | All 17 emitted builders assert an exact vertex/index count and compare every authored index in order for representative parameters. Every result is a valid `triangle-list` with finite orthogonal unit normal/tangent frames and applicable outward winding. Repeated calls share only the canonical layout: vertices, indices, subsets/elements, bounds, and runtime owners are isolated, and mutating one call cannot reach another. |
| Builder bounds | Every builder's cached AABB is recomputed independently from every emitted position and must be byte-for-byte tight at the current version. Box, capsule, plane, and torus also pin semantic half-extents/radius extents. |
| Primitive seams | Box corners retain face-specific records; capsule, circle, cylinder, ring, UV sphere, torus, and torus-knot closures retain duplicate positions/normals with their required UV endpoints. Cone apex vertices remain duplicated across segment U values. Spherical polyhedra and the icosphere use independent face records, lift crossing longitudes past 1, never leave a face split across 0/1, and place pole U between its face-mates. |
| Builder parameter guards | Every count-bearing builder floors fractional counts, clamps below-minimum counts, and throws `RangeError` for NaN and infinity on each count parameter. Zero-height cone/truncated-cylinder cases contain no dead triangles; a zero-height cylinder and collapsed/negative box or quad dimensions retain finite canonical records. Custom polyhedron seed positions are observably projected to the requested radius. |
| Failures | Negative and past-end triangle queries leave the caller's output unchanged. Negative and past-end position writes leave bytes and version unchanged. Invalid weld indices return a deep clone, merge mismatch returns absence, validation rejection is non-mutating, singular transforms preserve the entire destination, and incompatible morph scratch rejects atomically. Unknown formats and invalid component windows throw or no-op according to their return domain; missing, misformatted, misaligned, and out-of-record attributes reject without changing output, bytes, owners, or version. |

Thirty-seven independent modes (`--mutate-owner-identity`, `--mutate-bounds`,
`--mutate-index-order`, `--mutate-failure`, `--mutate-channel-order`,
`--mutate-stride-offset`, `--mutate-normalization`, `--mutate-group-bounds`,
`--mutate-builder-topology`, `--mutate-builder-seams`, `--mutate-builder-bounds`,
`--mutate-builder-guards`, `--mutate-weld-equivalence`, `--mutate-index-remap-order`,
`--mutate-subset-boundaries`, `--mutate-merge-aliasing`,
`--mutate-validation-branches`, `--mutate-transform-components`,
`--mutate-mirrored-transform`, `--mutate-transform-bounds`,
`--mutate-uv-axes-order`, `--mutate-morph-target-order`,
`--mutate-morph-weight-math`, `--mutate-morph-reset`,
`--mutate-morph-aliasing`, `--mutate-morph-bounds`,
`--mutate-morph-validation`, `--mutate-format-components`,
`--mutate-accessor-offset-stride`, `--mutate-accessor-normalization`,
`--mutate-accessor-bounds`, `--mutate-accessor-missing-defaults`,
`--mutate-clone-aliasing`, `--mutate-omitted-metadata`,
`--mutate-lifecycle-ordering`, `--mutate-reset-disposal`, and
`--mutate-cache-invalidation`) suppress one family oracle
each. Normal execution must pass; every
mutation execution must fail its named assertion.

## Verification boundary

The package-scoped compiler gate passes all 16/16 emitted mesh headers with the declared override
stack. The exact `flight_cpp.generated_mesh` CTest builds and passes. The broader exact `pkg:mesh`
label contains that test plus the pre-existing frozen Scene3D consumer; it reports the mesh test as
passed but cannot build/run `flight_cpp.generated_scene3d` because
`overrides/include/flight/scene3d/mesh.hpp` includes the absent
`flight/types/material_value.hpp`. This mesh work does not modify or mask that frozen consumer.

## Honest residual

This is a focused P-3D2 slice, not a claim that all 213 upstream cases are ported. The construction,
clone, metadata, and runtime-lifecycle family is now covered systematically. Ranked by remaining
observable breadth, the largest executable families are: (1) bounding spheres, flat normals, explicit
bounds refresh, and residual compaction/wireframe/index cases; (2) attribute-driven construction and
residual merge/subset/operation permutations; and (3) residual builder, transform, UV,
morph/deformation, and bind-pose combinations beyond the representative edge matrices above. The
package currently has no refusal-ledger row. This slice required no new compiler request: the emitted
metadata clone already retains the live index owner, so the discovered copy and engaged-null upload
slots were implementation mistakes in the existing declared runtime-owner override, whose manifest
now records the corrected shared/deep boundaries and true-absence teardown. The assisted-tier compiler
debt remains explicit in the three existing source patches, one emission repair, and six declared
overrides rather than being recast as native parity.
