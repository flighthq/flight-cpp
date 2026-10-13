# Quadbatch native inventory at a627849

The pinned upstream suite `packages/quadbatch/src/quadBatch.test.ts` contains 32 `describe` groups,
88 cases, and 198 `expect(...)` calls. `tests/generated_quadbatch_test.cpp` ports every behavioral
group and preserves the suite's concrete values; closely related expectations are sometimes grouped
into one native predicate so a single failure reports the source behavior, not an artificial C++
intermediate. It also pins the renderer-facing byte contract that the TypeScript suite only implies.

| Evidence family | Native evidence |
| --- | --- |
| Construction and lifecycle | Default and supplied data, fresh batch identity, fresh clone runtime, data/runtime/signal initializers, weak signal attachment, idempotent enable, append/remove/clear delivery |
| Batching and ordering | Sequential append indices, stable iteration, swap removal, stable sentinel compaction, overlapping-hit first-index order, no-op invalid ranges |
| Capacity | Automatic growth, explicit reserve, no allocation below/equal capacity, misaligned buffer minimum, shrinking logical counts, 300,000 untinted atlas instances |
| Vertex/index byte layout | `Uint16Array` ids are exactly 2 bytes per capacity slot; vector2 transforms are 2 `Float32` values / 8 bytes per slot; matrix3x2 transforms and iterator views are 6 `Float32` values / 24 bytes per slot; buffer identity is retained on no-op reserve and clone buffers are independent |
| Transform mutation | Indexed vector and matrix writes, contiguous range writes, exact identity-plus-translation expansion, translation-only collapse, unchanged-type buffer identity |
| Bounds and hits | Null/empty/invalid-id zero bounds, vector and matrix AABBs, rotated atlas logical extents, exclusive edges, exact rotated-quad rejection, point overloads |
| Failure paths | Out-of-range get/set/remove/tint, oversized/non-positive/negative ranges, empty iteration, missing atlas, empty atlas batch, invalid region ids |

Four upstream expectations call generic node-bounds APIs after quadbatch-specific mutation. They cannot
be claimed at this pin: `QuadBatch.entityRuntimeKey` retains the frozen
`NodeRuntime<Node2DTraits>` owner while `QuadBatchRuntime` is emitted as an unrelated flattened
owner. Native evidence instead asserts the exact boundary: the quadbatch runtime is identity-stable,
the rectangle is copied rather than aliased, the generic revision is invalidated, and
`copy_local_bounds_rectangle` returns the stored rectangle. The same honest treatment applies to
the flattened `MaterialData` / `TintMaterialData` siblings: the material slot and packed RGBA are
tested through `get_quad_batch_instance_tint`, but renderer-side base-to-derived recovery is not
claimed.

The test has a deliberate `--mutate-layout` mode that changes the first expanded matrix component
from `1` to `2` after conversion. Normal execution must pass; mutation execution must fail the exact
identity-plus-translation assertion. This guards the highest-value layout assertion against a
vacuous port.
