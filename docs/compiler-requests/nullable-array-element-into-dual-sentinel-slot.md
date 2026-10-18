# Nullable array elements are not lifted into an optional-nullable slot

G1 cluster `other-52e846a6` reaches the same attachment assignment through 18 ANIM headers.
The element carrier admits a reference or null; the destination additionally admits undefined.
The emitter adapts the literal null arm to the destination but leaves the indexed element in its
different nullable carrier, so the conditional has incompatible C++ operands.

Observed with Flight `a62784923f3be814463286c9fb28edfe6ab15789`, compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, the repository's registered source patches,
and Clang 22.1.8. The first failure is
`flight/skeleton2d/skeleton2d_animation_target.hpp:217`.

## Source and reduced shape

`packages/skeleton2d/src/skeleton2dAnimationTarget.ts:239`,
`bindSkeleton2DSlotAttachment`, reads an already checked attachment table. The source types are
`Skeleton2DSlotAnimationTarget.attachments?: readonly (Attachment2D | null)[] | null` and
`Slot2D.attachment?: Attachment2D | null`.

```ts
interface Attachment { name: string }
interface Slot { attachment?: Attachment | null }

export function bind(slot: Slot, table: readonly (Attachment | null)[], index: number): void {
  slot.attachment = index >= 0 && index < table.length ? table[index] : null;
}
```

The real binder rounds the sampled index before this statement. Missing tables and negative
keyframes return before it; an out-of-range index deliberately writes null.

## Actual emission

The field is `std::variant<flight::Ref<Attachment2D>, flight::Null, flight::Undefined>`, while the
table contains `std::optional<flight::Ref<Attachment2D>>`. Using `SlotStorage` below only as shorthand
for that field's emitted type, the failing expression is:

```cpp
slot->attachment = bounds
  ? std::get<0>(table).element(index)
  : SlotStorage{std::in_place_type<flight::Null>, flight::null};
```

The diagnostic begins `incompatible operand types ('value_type' (aka
'std::optional<std::shared_ptr<flight::types::Attachment2D>>') and 'std::variant<...>')`.
The same source boundary also needs conversion without a conditional: a normal focused emission of
the equivalent `if (bounds) slot.attachment = table[index]; else slot.attachment = null;` writes the
optional element directly into the variant. It does not supply a carrier conversion.

The initial cluster-only `-j1` run attempts all 18 headers: zero pass, 18 fail, none are
unattempted, and every first diagnostic is at the original site. Compiling only the fresh branch
candidate header at `-j1` also fails: `no viable overloaded '='` at its line 218. That changed
diagnostic is still a failure; no source patch or generated change is carried.

## Required lowering

Reconcile each selected value with the declared slot before forming the conditional or assigning a
branch. A represented attachment must retain its exact owner; a null element and the explicit null
fallback must select the null domain. Preserve undefined wherever the source can produce it, and
leave an existing undefined slot untouched on the binder's early returns. Do not collapse both
sentinels, allocate a replacement attachment, or select a variant alternative by position.

## Route and disposition

The C++ conditional and assignment operate on standard-library carrier types. An unused runtime
helper or host binding cannot change their typing, and a declaration-only repair cannot add this
conversion. The cheap source branch rewrite still requires the compiler's missing conversion and
is not carried. An override would copy a partially refused module merely to repair this expression;
the bounded G2 disposition keeps the compiler debt open instead.

This request covers `other-52e846a6` and its exact 18 rows in
[`generated/header-status.json`](../../generated/header-status.json). The root is the skeleton2d
animation target; the remaining rows reach it through skeleton2d helpers and DragonBones/Spine
timeline handlers. It is distinct from array-owner or sampling-easing issues in
[`animation-readonly-row-array-carriers.md`](animation-readonly-row-array-carriers.md).

## Carried semantic patch (bounded to the source's bounds guard)

Source patch `compiler-nullable-element-dual-sentinel` lifts a nullable array element selected in a
conditional whose own condition proves the index in range -- the same index bounded below that same
array's `length` and at or above zero, which is exactly the guarded binder above -- into a
dual-sentinel destination: `T | null` maps an empty optional to the Null arm, `T | undefined` to the
Undefined arm, and an engaged optional to the value arm with the same reference.

It deliberately does not generalize: an UNGUARDED `table[index]` (conditional or bare assignment)
can be a hole or a past-end index, which JavaScript yields as `undefined`, so its empty state must
not be lifted to Null from the declared `T | null` alone. Those forms, and the statement-guard
(if/else) spelling whose guard the emitter does not model, keep their existing emission. The
counterexample test holds that line.

Measured: new `cppNullableElementDualSentinel.test.ts` 4 pass (guarded lift; undefined-only element
to the Undefined arm with the explicit null fallback still Null; optional destination stays direct;
unguarded forms not lifted); the patch applies at its stack position (all compiler patches in order,
stacked build green); a runtime consumer on the guarded form proves a present element keeps the same
owner and a null element and the guarded out-of-range fallback select Null. Scoped regeneration and
the public timeline/slot consumer are pending a generation slot.
