# A Set of a union carrier must compare the subject, not the alternative

`new Set([a, b])` where `a` and `b` are two different capability slots lowers to a `flight::Set` whose
element is a two-alternative `std::variant`. `flight::Set` compares elements with `SameValueZero`, and for
a variant that is `std::variant::operator==`, which compares `index()` **first**. Two rows over the *same*
subject under two alternatives are therefore unequal, while the source's `Set` compares the referenced
object and deduplicates them. At `@flighthq/mediasession` this turns one provider into two destroy calls —
a double release of whatever native resource the provider owns, and exactly what the source comment at
`mediasession.ts:52-54` exists to prevent.

Measured at flight-compiler `2b687ea` / flight `a627849`, from the emitted `@flighthq/mediasession`
(3/3 modules emitted, 0 refusals) and from an isolated `flight::Set` probe.

## The rule's condition

`SameValueZero` is the default element equality of `flight::Set` (`include/flight/equality.hpp:8-21`):

```cpp
template <typename Value>
struct SameValueZero {
  bool operator()(const Value& left, const Value& right) const noexcept(noexcept(left == right)) {
    if constexpr (std::floating_point<Value>) {
      return left == right || (std::isnan(left) && std::isnan(right));
    } else {
      return left == right;
    }
  }
};
```

and the emitted element is a union of two structural references
(`generated/include/flight/mediasession/mediasession.hpp:132-133`):

```cpp
flight::Set<std::variant<StructuralRef<RowReadonly<RowOf<Ref<HostMediaSessionCapability>>>>,
                         StructuralRef<RowReadonly<RowOf<Ref<HostMediaSessionActionCapability>>>>>> providers
    = flight::Set<…>(flight::Array<…>{variant{in_place_type<CapRow>, host_media_session},
                                      variant{in_place_type<ActionRow>, host_media_session_action}});
```

A structural reference already compares as its subject — the runtime's own equality answers reference
identity for rows, which is what a JS `Set` does. Wrapping two of them in a `std::variant` replaces that
with C++'s tag-first comparison, so the identity the rows did carry is lost at the union boundary. In
TypeScript the union has no runtime tag: both slots hold the same object.

## Where it fires

| site | source | what it does |
| --- | --- | --- |
| `destroyMediaSession` | `packages/mediasession/src/mediasession.ts:59` | `new Set([hostMediaSession, hostMediaSessionAction])`, destroyed in a loop |

`flight::Set<std::variant<…>>` appears in exactly one generated header at this pin
(`flight/mediasession/mediasession.hpp`), so the blast radius today is one function. The rule is general,
though: any emitted `Set` or `Map` built from a union of reference-shaped capabilities or entities has the
same element comparison, and the emitter produces this shape whenever one call takes two capability slots.

## Reduction

One subject passed through both slots, against the pinned TypeScript's own contract. The upstream suite
states it directly — *"deduplicates an aliased provider identity across both Host slots"* — and the source
comment adds that "passing it as both provider arguments cannot double-release it".

| probe | measured | required |
| --- | --- | --- |
| `destroyMediaSession(shared, shared)` — one provider, both slots | **2 destroy calls** | 1 |
| `flight::Set` of one subject, *same* alternative twice | 1 element | 1 |
| `flight::Set` of one subject, *across* alternatives | **2 elements** | 1 |
| `flight::Set` of two distinct subjects | 2 elements | 2 |

The middle two rows isolate the mechanism: the same subject is deduplicated when both variants carry the
same alternative, and is not when they carry different ones. Nothing else about the two inputs differs.

A C++ caller reaches this shape by serving both slots from one object — a provider type deriving from both
capability subjects, passed to both parameters, which the runtime already accepts (each parameter
flattens the derived reference). Both rows then refer to the same subject, which is precisely the input
the source deduplicates.

## What would close this

Element equality for a union carrier whose alternatives are *reference-shaped* should compare the
referenced subject and ignore which alternative carries it, because the alternative is a static
projection and not a runtime tag. Value-domain unions (`number | string`, records compared structurally)
keep the existing comparison, where a variant's alternatives are disjoint by type and the index costs
nothing. Either home is acceptable and the runtime's is preferable:

* extend the runtime so `flight::Set`/`flight::Map` on a union of structural references and `Ref`s
  answers subject identity across alternatives; or
* have the emitter emit a subject-identity equality for a Set element it lowers to a union of references.

A third option — emitting the deduplication explicitly in `destroyMediaSession` rather than relying on the
Set — would fix this one site and leave the rule in place, so it is not preferred.

## Distinguish from the requests already filed

[`canonical-union-carrier-order.md`](canonical-union-carrier-order.md) is about the emitter expanding one
named union in two places with different *alternative order*, so the specializations are unrelated types.
This one is about the *equality* a union carrier inherits once it is built correctly.
[`compiler-concrete-owner-union-row-assertions.md`](compiler-concrete-owner-union-row-assertions.md) is
about asserting a concrete owner out of a union; this is about two references to one owner inside it.

## Carrying workaround

None, and none is proposed. The affected site is one function's aliased input, and an override would put
the whole module below shippable for it. `@flighthq/mediasession` is unaffected for distinct providers,
which is the shape the emitted signature separates statically.
