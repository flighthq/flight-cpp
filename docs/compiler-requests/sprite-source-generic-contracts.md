# Original Sprite source generic contracts

The two owned TS2345 errors in the retained strict220 receipt describe incompatible source contracts. They are independent of the optional mapped-member compiler evidence loss owned by Builder4 and the generic function-value/unknown assertion failure owned by Builder5. The existing `finishEntity` inference preserves the argument's owner type in the measured controls. This proof adds no compiler, Types, runtime or SDK implementation.

The runnable [diagnostic fixture](../../artifacts/sprite-source-generic-contracts.ts) contains declaration-only reductions and callers of the frozen Types and original Entity module. It does not replace a Sprite, node, initializer or callback body. [The evidence record](../../artifacts/sprite-source-generic-contracts.json) identifies the immutable input archive and the replay command.

## Exact retained diagnostics

Builder4 supplied the full baseline/candidate diagnostics, the exact check script/configuration, three frozen source overlays, effective Types/entity, callback IR and selected whole-emission outputs. The strict receipt used the three actual Sprite/displayObject/node roots, parsed tsconfig options, `noEmit: true`, `composite: false` and `types: []`.

| Receipt | Total diagnostics | Owned diagnostics | Other diagnostics |
| --- | ---: | ---: | ---: |
| Baseline | 220 | 2 | 218 |
| Candidate | 220 | 2 | 218 |

The other 218 entries have an identical multiset of file, code and full message, retaining duplicate counts and ignoring only line movement. This establishes more than equal totals. It is a comparison of the delivered receipts: the complete canonical runtime source context was not delivered, so this work does not claim a fresh full220 rerun. The separate scoped fixture retains its 203 foreign diagnostics explicitly.

The displayObject baseline rejects nullable `PartialNode<Node2D>.data` at `NodeInitialization<Node2DData>`. The candidate preserves the generic `N` in `PartialNode<N>` but still cannot prove its independently chosen data belongs to `D`. The Sprite diagnostic is unchanged: the installed function requires `Readonly<Sprite>` while the base bounds slot permits `Readonly<BoundsNode<Node2DTraits>>`.

## Data input, output and owner must be distinguished

The original declarations admit `{ data: null }` as `PartialNode<Node2D>`. `NodeInitialization<D>` and `NodeDataFactory<D>` admit an optional `Readonly<Partial<D>>`, which excludes null. Further, `createNode2D` independently quantifies `N` and `D`. A declaration with that signature accepts a Sprite result paired with an unrelated Node2DData factory. An omitted factory can also promise Sprite while the source stores null data.

The source controls execute the supplied original bodies. A caller-supplied factory observes undefined, null or the exact input object unchanged. The runtime factory runs before the data getter and the data callback. A throwing callback retains the thrown owner and prevents later options reads. A factory returning unrelated data stores that same object; the source does not manufacture missing SpriteData. Coalescing null to undefined, projecting/copying the input, or restricting the proof to Sprite options would change the measured contract.

A bounded design should keep the existing Node/Entity construction family and separate the initializer's input domain from the factory's output. The same input type must reach `obj.data` and the factory argument; the same output type must describe the stored data. Factory absence must describe null. Runtime factory output must likewise match the returned runtime slot. These are type and owner relations; they require no new runtime carrier or initializer body.

There is an unresolved source-contract decision: the existing signature both admits independent mismatched families and promises `N`. A compiler cannot preserve that promise and derive a valid data relation from these declarations. A truthful result projection that keeps the accepted input domain would change the declared result; constraining the generic arguments would narrow the accepted calls. Neither change is authorized here. Foreman and the sole Types owner must choose the intended public relation before a source type change. Blanket nullable widening of every data factory would also incorrectly promise that callbacks which reject null can handle it.

## Bounds callbacks require the same receiver through storage and dispatch

The exact Types show two independent assignment failures:

- `BoundsNode<Node2DTraits>` to Sprite fails because data can be null.
- Sprite to `BoundsNode<Node2DTraits>` fails at `EntityRuntimeKey`: SpriteRuntime's callback takes the Node2D receiver, whereas `BoundsRectangleRuntimeView` promises a callback accepting `BoundsNodeAny`.

The data-only and runtime-slot-omitted controls accept Sprite, localizing the second failure to the runtime view. The original Sprite callback, invoked through its installed runtime with a broad null-data Node2D, throws TypeError before writing the output rectangle. The original Sprite/runtime pairing succeeds and retains both objects and the callback identity.

Using method syntax does not establish safety. The reduced contract without the runtime view accepts its method-syntax control, but the complete frozen Types rejects it. The first actual-Type experiment expected the reduced result and passed only11/12; that failed assumption and its diagnostics are retained. The corrected18-case fixture expects the actual refusal. Merely changing the methods parameter to `MethodsOf<R>` also still rejects the runner because SpriteRuntime inherits the broad Node2D callback contract.

The bounded proposal is to preserve the existing runtime objects and callback fields while carrying one receiver family through callback registration, the symbol-keyed runtime slot and bounds dispatch. Both compute and validity callbacks must use that receiver. A broad dispatcher must not promise that a Sprite-only function accepts arbitrary BoundsNodeAny. Generic bounds operations and mutable slot assignments must preserve the node/runtime pairing; fallback callbacks may accept a broader receiver when contravariance proves that safe. Optional/nullable callbacks, accessor order, exceptions and identities must retain their original semantics.

This is a source type-contract design requiring Types-owner review, not permission for a new C++ family or an unproved adapter. No Any erasure, unchecked cast, bivariant substitution, callback widening, initializer rewrite or owner replacement closes the demonstrated gap. If the current declarations must remain byte-identical, the two strict errors must remain visible.

## Existing inference and separate compiler ownership

Implicit `finishEntity(out)` and explicit `finishEntity<N>(out)` both emit `finish_entity<N>(out)`. A derived construction returned through a public base result still calls `finish_entity<Ref<Derived>>(out)` before normal base widening. The unchanged original Entity implementation passes10 native checks for pointer/control-block identity, live writes, lifetime and rejection of an unrelated owner. These controls do not qualify fresh anonymous Sprite allocation or the complete native factory.

Builder4's delivered IR distinguishes the exact authored/reference-planner `NodeOf<Traits>` from optional member expression evidence degraded to an unknown/Any receiver. Its existing mapped-subject recovery candidate is the bounded ordinary compiler fix for that independently demonstrated loss. Builder3 does not edit or duplicate it. Builder5 owns the later public-image compile failure involving an uninstantiated generic function value under an unknown/Any assertion. No function-value fix is proposed by this proof.

## Verification and limits

The archive retains the full220 receipt/config, all three frozen bundles, original overlays and effective Types/entity, IR and selected emission,12 reduced cases,18 actual-Type cases, the first failed assumption, source39 plus10 contract checks, emitted finish owner/native10, commands and native header closure. Every archive file is checksummed. The source replay includes the exact source files read by its loader, without depending on a mutable sibling checkout.

```sh
node scripts/spriteSourceGenericContractProof.mjs /path/to/builder3-sprite-source-generic-contracts-v1.tgz
node scripts/spriteSourceGenericContractProof.mjs /path/to/builder3-sprite-source-generic-contracts-v1.tgz --typescript=/path/to/typescript/lib/typescript.js --cxx=/path/to/c++
```

The first command checks immutable evidence. Supplying TypeScript5.9.3 reruns all18 expected actual-Type outcomes, retains and compares203 foreign diagnostics, and executes the unchanged source39 plus10 controls. Supplying a C++20 compiler rebuilds/runs the emitted finish10 against the archived runtime header closure. No command generates the SDK or activates a candidate. Actual native Sprite/image closure remains unqualified; the two source contracts and the other owners' independent compiler/runtime work remain open.
