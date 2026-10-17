# Readonly map key enumeration must retain the map's value carrier

At Flight `a627849` / compiler `806d257`, the original one-header cluster
`no-viable-conversion-0faa55ca` stops at `render/explain_scene_2d_pipeline_coverage.hpp:60`:
`Map<String,Ref<NodeRenderer>>` cannot convert to `Map<String,Any>`.

## Source and native boundary

`registry/registryTable.ts` declares `getKindMapKeys(out: Kind[], map: ReadonlyMap<Kind,unknown>)`.
Its body clears out.length, iterates map.keys(), appends each key and sorts out. The pipeline coverage
caller passes the original `getRenderStateRuntime(state).registries.nodeRenderers` table. TypeScript
permits that readonly key operation for every value type, but native Map<K,V> retains its own typed
storage and does not widen to Map<K,Any>. The caller's values are exact NodeRenderer references, not
missing registry data or an omitted helper.

The required property is key access over that same typed map without copying/boxing its entries,
replacing renderers, inspecting value truthiness or granting writable Map<Any> authority. A readonly
key view or correctly inferred generic helper can provide it. A broad converting Map constructor would
claim more than this key-only operation needs and must not manufacture another registry.

## Carried source annotation

`registry-kind-map-keys-value-parameter` changes only the helper declaration:

```ts
export function getKindMapKeys<MapType extends ReadonlyMap<Kind,unknown>>(out: Kind[], map: MapType): void {
  out.length = 0;
  for (const key of map.keys()) out.push(key);
  out.sort();
}
```

The whole-map parameter retains the original ReadonlyMap<Kind,unknown> constraint, including unions
of maps with different value types. Compatibility controls cover those unions, an unknown-valued map,
nested reference values and assignment to the original function signature. A parameter generic only
in V can reject heterogeneous map unions during inference, so it is not the carried form. The complete
original and patched module transpile to byte-identical JavaScript. The same out array is cleared and
sorted, and only map keys are read. Native MapType retains the caller's existing typed map and values;
no erased-container conversion is introduced. The emitter uses map->keys() on MapType; the runtime
Map supplies a const operator-> self-view of the current value handle. It returns this without
allocating, copying entries or replacing shared storage; keys() keeps the original live iterator.
No data fields or value/identity policy change.

Normal exact-pin single-module generation lowered 1,035 modules across two packages, emitted the
selected module with no refusals, and applied 255 source patches and two existing repairs. Its final
header uses MapType and map->keys(), with the runtime self-view above; the source module body and
caller expressions are unchanged. The manifest carries only this patch's applied digest, with
existing package/global inventory totals unchanged.

Final original-header check: **1/1 pass, 0 failures, 0 unattempted**, Clang 22.1.8, one worker,
73 seconds. The changed native registry test passes its typed map directly and compiles/links/runs
with exit 0. The direct actual-helper probe also compiles/links/runs with exit 0 for the exact
NodeRenderer-reference map, numeric/Any/empty maps, caller-array aliases and clearing, open UTF16
key sorting, map order and original value owners. It checks the Map self-view points to its handle;
no renderer or coverage body is invoked. The scoped runtime CMake target builds and
flight_cpp.runtime passes 1/1 (0.02 seconds). Source-patch mechanism tests pass 2/2; the final
whole-map source/type-domain oracle passes. The old cluster is removed; the pass row keeps its
previousCluster and request. These checks qualify key enumeration, not the graph-owner obligations below.


## Full node-kind coverage contract

The source accepts the original RenderState and a readonly NodeAny root. It first collects kinds with
an iterative stack: include the root, read each current node's kind, then read its actual runtime children
and push children in reverse order. This yields live depth-first pre-order. There are no enabled,
visible, material, backend, registered-kind or built-in-kind filters, and no node-identity visited set.
Repeated occurrences still traverse; only kind strings are deduplicated by the local Set.

After graph collection, enumerate the original state's node-renderer registry keys and sort them
(the helper and caller both sort, unchanged). Used kinds are sorted separately. uncoveredKinds is
used minus registered; unusedRegistrations is registered minus used. Return four fresh arrays in a
fresh result, never registry storage or cached answers. Preserve open vendor strings, exact equality,
default string ordering and source getter/exception order. A root-only graph still uses its root kind;
with no registrations that kind is uncovered. Registration means a key is present, not that its value
was invoked or proved usable. No renderer/provider is executed or synthesized by this query.

This API collects node kinds itself; it does not call Scene2D's separate four-facet getScene2DKindUsage
walk or add blend/material/shape-command/texture-source requirements. Broader backend coverage is a
different operation.

## Separate owner obligations and coverage limits

The current collect_used_kinds body still converts a readonly NodeAny row back to nominal NodeAny,
then selects get_node_runtime<Ref<NodeTraits>> and pushes NodeOf<Ref<NodeTraits>> children into its
readonly NodeAny stack. The existing [inferred-any/default-runtime request](node-any-generic-traits-leak.md)
records that carrier family. Preserve the actual graph, node/runtime/children owner through these reads;
a key-helper fix does not prove the runtime cell's type or justify a node/graph copy, mutable promotion,
default trait alias or narrowed Node2D-only API. These later owner findings are inspected, not a new
native walk result or a newly measured next diagnostic.

The five inspected explainScene2DPipelineCoverage source cases cover unused registration, uncovered
custom kinds, no registrations with a root, sorted arrays and iterative descendants. The two source
getKindMapKeys cases cover sorted keys and reused-array clearing. Existing native registry tests use
an explicitly Any-valued table, while generated_render_scene2d_coverage_test.cpp manually constructs
usage records for another coverage API; neither proves this pipeline's graph walk. No renderer, live
pipeline, registry-provider or full graph-coverage success is inferred from key-helper/header checks.

Expiry: the compiler preserves typed readonly maps for key-only calls, or Flight adopts the equivalent
generic annotation. Pinned source/compiler revisions and the declared source-patch manifest remain
authoritative; no compiler patch, runtime container conversion or owner copy is supplied.
