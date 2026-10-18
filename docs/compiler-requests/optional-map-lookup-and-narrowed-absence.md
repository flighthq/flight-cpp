# A nullable map lookup and a partially narrowed union must keep their own sentinels

Two adjacent representations in one source function (`@flighthq/swf`'s `resolveSwfMaskClip`) were
refused, and the retained caller of the refused function then failed to compile:

1. `map.get(key)` on a `Map<K, V>` whose stored value is itself nullable returns
   `optional<optional<T>>` in C++: the outer layer is a missing entry, the inner layer is the stored
   null. The declared local's type is `T | null | undefined`, a dual-sentinel variant, and the nested
   optional has no conversion to it.
2. After `if (cached !== undefined) return cached;`, the source value is `T | null`: the strict test
   removed `undefined` but deliberately kept null, and the destination (`T | null`) keeps it too. The
   emitter refused to project the storage into that narrowed carrier.

## Emitted before and after

Before, the local was initialized from the raw lookup and the narrowed return was refused with
`cpp-member-projection-without-present-storage`.

After:

```cpp
std::variant<Ref<T>, Null, Undefined> cached = ([&]() -> std::variant<Ref<T>, Null, Undefined> {
  auto lookup = by.value().get(placement);
  if (!lookup.has_value()) return {std::in_place_type<Undefined>, flight::undefined};
  if (!lookup.value().has_value()) return {std::in_place_type<Null>, flight::null};
  return {std::in_place_type<Ref<T>>, lookup.value().value()};
}());
```

and

```cpp
return ([&]() -> std::optional<Ref<T>> {
  if (const auto* present = std::get_if<Ref<T>>(&cached)) return std::optional<Ref<T>>{*present};
  if (std::holds_alternative<flight::Null>(cached)) return {};
  throw std::logic_error("narrowed union holds a sentinel the narrowing excluded");
}());
```

## Equivalence

- A missing key is `undefined`, a stored null stays `null`, and a present entry is the same reference
  the map holds. The two absence layers are never merged.
- A narrowing that still admits one sentinel keeps that sentinel as the narrowed carrier's own
  absence; the sentinel the narrowing excluded is unreachable and throws rather than being folded
  into a different state or read as a value.
- One lookup, one guard, one projection: evaluation order and read counts are unchanged. No copied
  owner, no source rewrite, no declaration-only substitute for a body.

## Original site

`packages/swf/src/swfDocument.ts` `resolveSwfMaskClip` (source line 670), refused at
`flight/swf/swf_document.hpp` first; after this fix the five document-dependent SWF headers advance
to the separate `createSwfMaskClipRegion` refusal (`cpp-array-length-sparse-runtime-required`).

## Evidence

- `cppNarrowedUnionAbsencePreservation.test.ts`: 3 pass (lookup rebuilt into the three-state carrier;
  the narrowed projection keeps Null and treats the excluded Undefined as unreachable; a `!= null`
  control keeps the direct value read).
- Complete compiler-emitted native fixture with string keys, compiled/linked/run against the pinned
  runtime: `raw-missing=undefined raw-null=null raw-value=value identity=1`, narrowed
  `missing-null=1 value=1`; the JavaScript source oracle prints the same states and identity.
- Scratch measurement over the SWF closure (approved, no splice): `resolveSwfMaskClip` emits, its
  marker is gone, and the two projections appear in the measured module.

Managed patch: `source-patches/compiler-narrowed-union-absence-preservation.patch`; it expires when
upstream lowers the nested-optional lookup into the declared carrier and preserves a sentinel a strict
narrowing left reachable.
