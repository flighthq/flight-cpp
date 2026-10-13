# Quadbatch needs one retained owner across specialized construction boundaries

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`2b687eaf6b631e0fa4d7d6e18dad93b845f99633`, quadbatch's main module emits most of its batching
algorithms but omits ten declarations. The omissions cluster around one representation boundary: a
specialized `QuadBatch` is flattened independently from the Node2D owners and computed attachments it
refines, while contextual constructors are typed for those base owners.

## Measured omissions

| source declaration | emitted refusal | observable contract that must survive |
| --- | --- | --- |
| `copyLocalBoundsRectangle` | readonly `BoundsNode<Node2DTraits>` cannot refine to writable `QuadBatch` | read the same batch and its same runtime; do not copy or grant writes |
| `getQuadBatchRuntime` | readonly `Node2DRuntime` cannot refine to `QuadBatchRuntime` | return the runtime created for this exact batch |
| `setQuadBatchLocalBoundsRectangle` | base runtime cannot refine to writable `QuadBatchRuntime` | mutate the batch's retained runtime and invalidate its revision |
| `createQuadBatchRuntime` | `Node2DRuntime` and `QuadBatchRuntime` are unrelated flattened owners | initialize one specialized runtime with all base and quadbatch cells |
| `defaultMethods` | the specialized bounds callback does not match the base callback's flattened row signature | retain the typed callback without changing its argument identity |
| `createQuadBatch` | specialized data/runtime factories do not match `createNode2D`'s base factory signatures | construct one batch with its supplied data and fresh runtime |
| `cloneQuadBatch` | the fresh data literal resembles, but is not, the optional `PartialNode<QuadBatch>` arm | copy buffers, preserve atlas/material element identities, and create fresh batch/runtime identities |
| `setQuadBatchInstanceTint` | a fresh `Array<null>` is not elected as the nullable material-element carrier | allocate the array lazily at exact instance length and store one tint owner |
| `enableQuadBatchSignals` | `QuadBatch` cannot refine to the computed-symbol attachment owner | attach once to the same batch identity |
| `getQuadBatchSignals` | a readonly batch cannot refine to the writable attachment owner | read without allocating or granting mutation |

The readonly/writable diagnostic is not permission to cast away readonlyness. For the read paths the
target must be readonly; for mutation the specialized owner must be established at construction. Nor
may the backend materialize a sibling struct: clone is the only source operation that requests a new
batch identity.

## Related compile defects

The emitted body of `initializeQuadBatchData` also returns two `optional<Value>` row reads from lambdas
declared as `variant<Value, Null, Undefined>`; that site is recorded in
[`partial-row-three-state-return.md`](partial-row-three-state-return.md). Three retained mutators call
the omitted `getQuadBatchSignals`; that site is recorded in
[`retained-declaration-calls-omitted-callee.md`](retained-declaration-calls-omitted-callee.md). The
computed attachment itself is recorded in
[`computed-symbol-cell-bindings.md`](computed-symbol-cell-bindings.md).

## Required representation

A valid native lowering may use an identity-preserving checked view, real interface heritage, or a
construction API that creates the specialized owner directly. It must keep exactly one `QuadBatch`
owner, one runtime per constructed batch, the original atlas and material element references, and one
signal group per enabled batch. The typed-array clone is deliberately a copy, while iteration subarrays
must remain views into the original transform storage.

The package-local override supplies that contract explicitly and uses weak-keyed tables only for cells
that the generated `QuadBatch` owner cannot represent (`QuadBatchRuntime` and the private signal slot).
Those tables preserve lookup identity and do not extend batch lifetime. It expires when generated
specialized construction and computed-cell bindings make the emitted module compile without them.
