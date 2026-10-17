# Optional-chain payload union storage

An optional chain over a nullable multi-arm member emits `optional<variant<...>>`, but immediate
nullish coalescing plans a flat sentinel variant and applies `std::get_if` to the optional wrapper.

## Minimal TypeScript

```ts
interface Texture2D { readonly width: number }
interface TextureCube { readonly size: number }
type Texture = Texture2D | TextureCube;
interface Material { map: Texture | null }

export function apply(target: Material, opts?: Readonly<Partial<Material>>): void {
  target.map = opts?.map ?? null;
}
```

## Expected C++

The coalesce must use the carrier actually emitted by the optional chain:

```cpp
std::optional<Texture> left = /* optional receiver/member read */;
target->map = left.has_value()
    ? std::optional<Texture>{left.value()}
    : std::nullopt;
```

It must not emit `std::get_if<T>(&left)` where `left` is an `std::optional`.

## Affected modules

Classic, specular-glossiness, standard, and extended PBR constructors whose `Partial<Material>`
options carry nullable texture unions.

## Carrying workaround

[`compiler-flattened-optional-chain-union-storage.patch`](../../source-patches/compiler-flattened-optional-chain-union-storage.patch)
records the optional payload-variant plan and reuses the existing equivalent optional-union path. It
expires when contextual coalescing is planned from the emitted optional-chain carrier upstream.

## G2 Scene2D slot resolver coalesces an optional callback result

At Flight `a627849` / compiler `806d257`, the original one-header cluster
`call-has-no-matching-76a6a61a` retains `no matching function for call to 'get_if'` at
`scene2d_resources/resolve_scene_2d_resources.hpp:49`. This docs-only audit retains **0/1** and the
exact diagnostic/counts; no native or generation rerun is claimed. The function is emitted in full,
with no refused callee or override for this module.

Source `packages/scene2d-resources/src/resolveScene2DResources.ts:26` reads:

```ts
const content = options?.resolveSlotContent?.(reference) ?? null;
```

The options value is `optional<StructuralRef<RowReadonly<RowOf<Ref<ResolveScene2DResourcesOptions>>>>>`.
Its resolveSlotContent cell is `optional<Scene2DSlotContentResolver>`, whose std::function takes a
readonly row of the original Scene2DSlotReference and returns `optional<Node2D>`. Node2D is one merged
structural view, `Node<Node2DTraits> & Node2DTraits`; it is not a material/texture variant. Both the
inner optional-call lambda and its immediate nullish-coalesce operand actually return optional<Node2D>,
but the coalesce applies `std::get_if<Node2D>(&nullish_coalesce_left)` to that optional wrapper.
The local content and the following setter parameter already use optional<Node2D>.

This is the same actual-storage versus planned-union disagreement as the property case above, with
a distinct **optional call and single structural payload** trigger. The carried compiler patch's
getCppFlattenedOptionalChainStoragePlanCpp explicitly requires an optional-chain `property` expression
and at least two value arms. Its property/Texture regression does not qualify this callback result;
that guard must not be removed indiscriminately because other chains preserve null/undefined separately.
The resolver's callable and result are already represented, so this is not a refusal for missing
[optional-property callable result evidence](optional-property-call-result-evidence.md).

For this expression `?? null` intentionally maps absent options, an absent callback and null/undefined
results to null. Once the existing chain has yielded optional<Node2D>, the coalesce can reuse exactly
that carrier without another call, variant extraction, materialization or sentinel reinterpretation:

```cpp
using NodeValue = flight::types::Node2D;
std::optional<NodeValue> left = /* existing optional-call result, evaluated once */;
std::optional<NodeValue> content = left;
```

A present row retains the provider's original node and shared owner; an empty optional stays empty,
rather than becoming an engaged null pointer or a default node. No std::get_if overload on optionals,
callback substitute, generated-body edit or permissive conversion is carried. A declaration repair
cannot replace this expression. No source/compiler/runtime candidate or successful generation is claimed.

Preserve the synchronous selected-working-set operation. Read the document's live slots in order;
the optional select callback receives the same complete readonly reference. A rejected slot is untouched
and absent from both result arrays and required counts. Do not cache callbacks outside the loop, double
the resolver read/invocation, change receiver behavior or snapshot the slots/required fields: callbacks
may observe or mutate live state. Count required before resolving, invoke the optional resolver once,
normalize its result, and call the real setter before classifying the entry. For a null result, reread
required when incrementing requiredUnresolved, as the source does. Exceptions propagate with earlier
callback, graph and reference writes intact; there is no retry, rollback, asynchronous task or catch.

The setter owns actual graph reconciliation over the original reference, target and content nodes. If
the retained content is identical and already parented to the target (or both are null), return without
reordering children. Otherwise detach the prior managed content only if its current parent is that
target, assign reference.content, then attach the new non-null node through addNodeChild. Preserve
authored siblings, parent/runtime cells, effects and child order; do not clone a provider node, clear
unmanaged children or hide a node-runtime owner gap with fake parent state. Repeated/shared provider
objects remain the same objects and follow the existing hierarchy rules. Current native hierarchy access
still exposes its explicit unavailable checked NodeRuntime boundary; this callback-carrier audit supplies
no hierarchy implementation or successful attachment claim.

Each call creates fresh resolved/unresolved arrays. Resolved entries hold the same content and reference
owners; unresolved contains the selected original references, including optional misses, in source order.
After the loop, requiredUnresolved>0 reports exactly one notice through the installed hook with operation
resolveScene2DResources, reason required-slots-unresolved, selected requiredTotal/requiredUnresolved and
url:null. With no hook it has no logging effect; a throwing hook propagates before return. Return the
same document and its root read at the end, after callbacks/reporting. No successful provider is invented,
and image/audio fetch/decode/load state remains the separate source lanes.

Inspected source coverage: three resolveScene2DResources cases pin synchronous same-node resolution and
parenting, stale-content clearing without a resolver, and excluded-slot preservation. Two setter cases
pin stable child order on identical content and replacement of only the managed child. The diagnostics
suite also routes a required-slot failure through the installed guard. These were not run in this unit.
The native generated_scene2d_resources_test.cpp and generated_scene2d_resource_loading_test.cpp cover
other document/coverage/image/audio operations; neither calls this resolver or slot setter. No native
provider/slot-resolution or header-availability success follows from those fixtures. Only this appendix,
the original failed row/cluster and worklist100 change; all C++/runtime/source/patch/repair/override/
manifest/pins and foreign records remain unchanged.
