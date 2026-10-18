# A chained call's null and undefined sentinels must both survive into a dual-sentinel result

`handler.instantiate?.createPlacementNode?.(...)` has three source outcomes: an absent link
(`undefined`), the callee's own `null` return, and the present node. The declared local's type is
`Node2D | null | undefined`, which the emitter represents as a dual-sentinel variant — but the chain
lowering produces `std::optional<payload>`, and `std::optional<T>` has no converting constructor for
`std::variant<T, Null, Undefined>`. The emitted line therefore does not compile, and the optional
form also erases which sentinel occurred.

## Minimal TypeScript

```ts
interface Node2D { x: number }
interface Instantiate { createPlacementNode?(parsed: number): Node2D | null }
interface Handler { instantiate?: Instantiate }

export function create(handlers: readonly Handler[]): number {
  for (const handler of handlers) {
    const node: Node2D | null | undefined = handler.instantiate?.createPlacementNode?.(1);
    void node;
  }
  return 0;
}
```

## Emitted before and after

Before, the local's carrier and its initializer disagree:

```cpp
std::variant<flight::Ref<Node2D>, flight::Null, flight::Undefined> node =
    ([&]() -> std::optional<flight::Ref<Node2D>> { /* ... */ }());   // no conversion
```

After, the same receiver projection and the same invocation produce the declared carrier directly:

```cpp
std::variant<flight::Ref<Node2D>, flight::Null, flight::Undefined> node =
    ([&]() -> std::variant<flight::Ref<Node2D>, flight::Null, flight::Undefined> {
       auto optional_chain_receiver = /* the receiver chain */;
       if (/* link absent */) return {std::in_place_type<flight::Undefined>, flight::undefined};
       auto optional_chain_result = optional_chain_receiver.value()(1.0);
       if (!optional_chain_result.has_value()) return {std::in_place_type<flight::Null>, flight::null};
       return {std::in_place_type<flight::Ref<Node2D>>, optional_chain_result.value()};
     }());
```

The conversion is applied only when the destination represents **both** sentinels with exactly one
value slot. A destination that keeps only one sentinel (`Node2D | undefined`) keeps the ordinary
optional chain, because there the optional carrier is the declared representation and nothing is
collapsed.

## Equivalence

- **Sentinel identity.** A missing link stays `undefined`; the callee's `null` stays `null`. Neither
  becomes the other, and neither becomes a value.
- **Evaluation.** The receiver chain is evaluated once for both the absence test and the invocation;
  the callback runs exactly once per present path and never on an absent link.
- **Ownership.** A present result is the reference the callback returned — the same `Ref`, not a copy
  or a reconstruct — and its live cells are the returned owner's.
- **Everything else unchanged.** The chain is still the ordinary optional form for every other
  destination; only the dual-sentinel contextual construction is routed to this reconstruction.

## A callee that itself declares both sentinels

The callback's declared return can itself be `Node2D | null | undefined`. Then its emitted result is
already the destination carrier, and the only extra state the chain adds is the absent link's
`undefined`. The lowering therefore evaluates the link test and returns the callee's result unchanged;
turning it into an optional first would collapse a **returned** `undefined` into `null`, which the
source distinguishes. For a callee that returns only `Node2D | null`, the empty result is the callee's
`null` and is reconstructed as such.

A receiver link whose property type is itself nullable (`instantiate?: Instantiate | null`) is not
silently mishandled: it is refused by the existing
`cpp-dual-sentinel-optional-chain-projection-unproven` rule, which reports that several value
alternatives cannot be projected through one nullish guard. That refusal is unchanged and is not
claimed here; the SWF shape's `instantiate?` is undefined-only.

## Evidence

The fixture is a complete source module (`first(handlers: Handler[], parsed)` returning the chain
directly) emitted by the backend as a whole header, not a hand-copied expression. A consumer compiled
and linked against the pinned runtime calls the emitted function over `flight::Array<flight::Ref<...>>`
owners and asserts, matching a JavaScript oracle run over the same source:

| case | emitted result | callback calls |
| --- | --- | --- |
| absent `instantiate` link | `Undefined` | 0 |
| present link, missing callable | `Undefined` | 0 |
| callee returns `undefined` | `Undefined` (not `Null`) | 1 |
| callee returns `null` | `Null` (not `Undefined`) | 1 |
| callee returns a node | the same `Ref`, live cell | 1 |
| callee throws | the exact exception/message propagates | 1 |

The consumer printed `undefined-link=1 undefined-callable=1 returned-undefined=1 returned-null=1
present=1 identity=1 throw=callee-threw calls=4` and exited 0; the oracle printed
`undefined-link=undefined undefined-callable=undefined returned-undefined=undefined
returned-null=null present=node identity=true throw=callee-threw calls=4` and exited 0. Focused
compiler tests cover the dual-sentinel reconstruction, the one-sentinel control, and the tri-state
callee pass-through (3 tests). The fixture's `instantiate` is a data property, so there is no getter
to throw; a throwing **callee** is covered, and the absent/missing cases prove the call is lazy.

- Focused compiler test `cppOptionalChainResultVariant.test.ts`: 2 pass — the dual-sentinel form
  carries both sentinel constructions, one receiver evaluation and one callback read; a one-sentinel
  destination keeps `std::optional`.
- The emitted expression, copied verbatim into a native fixture, compiles against the pinned runtime
  and runs: `undefined-link=undefined undefined-callable=undefined null-result=null present=node
  calls=2`, with the present `Ref` equal to the returned owner (`held.get() == node.get()`) and its
  cell live (`x == 7`).
- The source oracle (`node` semantics for the same four cases) prints the same states, identity and
  two callback invocations.
- Managed patch: `source-patches/compiler-optional-chain-dual-sentinel-result.patch`; it expires when
  upstream lowers a chained call directly into the dual-sentinel carrier it declares.

## Original site

`packages/swf/src/swfDocument.ts`, `createSwfHandlerPlacementNode`:

```ts
const node = handler.instantiate?.createPlacementNode?.(parsed, characterId, bounds, diagnostics);
if (node !== undefined && node !== null) return node;
```

This is the first failure of `swf_content_capabilities.hpp`, `swf_document.hpp`, `swf_header.hpp`,
`swf_requirements.hpp` and `swf_tag_census.hpp` at `flight/swf/swf_document.hpp:222` after the
class/interface adoption cleared the earlier diagnostic. No source body is rewritten and no native
owner is fabricated or copied: the fix is the one carrier the declared type already names.
