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
export function getKindMapKeys<T>(out: Kind[], map: ReadonlyMap<Kind,T>): void {
  out.length = 0;
  for (const key of map.keys()) out.push(key);
  out.sort();
}
```

All map value types accepted by the original unknown-valued readonly input remain accepted. T is
unconstrained and inferred, including unknown itself; no public key/value domain is narrowed. The
complete original and patched module transpile to byte-identical JavaScript. The same out array is
cleared and sorted, and the map is only read. Native generic Map<String,T> preserves its storage and
original values; this introduces no erased-container conversion or new runtime API.

Normal exact-pin single-module generation lowered 1,035 modules across two packages, emitted the one
selected module with no refusals, and applied 255 source patches and two existing repairs. The generated header
changes only the template/value parameter and removes its unused Any include; the manifest carries
only this patch's applied digest, with existing package/global counts unchanged. Original1 native
header check is **1/1 pass, 0 failures, 0 unattempted**, Clang 22.1.8, one worker, 76.4 seconds. The changed existing
native registry test now passes its typed String map directly; it compiles, links and runs with exit 0.
A direct actual-helper probe also compiles/links/runs with exit 0 for the exact NodeRenderer-reference map,
numeric and Any values, empty maps, caller-array alias/clear behavior, open UTF16 key sorting, unchanged
map order and original value owners. It invokes no renderer or pipeline coverage body. Source-patch
mechanism tests pass 2/2. The old cluster is removed; the pass row keeps previousCluster and this request.
These checks qualify the map-key boundary, not graph/runtime capability or complete coverage behavior.

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
