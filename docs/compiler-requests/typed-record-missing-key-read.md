# A typed Record read of a missing key must yield undefined, not throw

`Record<K, V>` is a JavaScript dictionary: `record[key]` for a key the object does not hold is
`undefined`. The emitter lowers a string-keyed typed Record read through the runtime dictionary
(`Record::get`), whose `std::optional` uses *empty* for a missing key and an engaged `Any(undefined)`
for a key present with value undefined. The read appends `.value()`, so the missing case throws
`std::bad_optional_access` where the source returns undefined.

Measured at flight-compiler `806d257` / Flight `a627849`.

## Minimal TypeScript

```ts
export function countSection(
  counts: Map<string, number>,
  record: Record<string, unknown>,
  key: string,
): void {
  const value = record[key];
  if (!Array.isArray(value) || value.length === 0) return;
  counts.set('count', value.length);
}
```

## Observed C++

`@flighthq/skeleton2d-formats` `dragonBonesSectionCounts.ts` `countSection`:

```cpp
auto value = record.get(json_key).value();   // throws for a key the record does not hold
if (!flight::is_array(value) || static_cast<double>(value.array_size()) == 0.0) return;
```

A real consumer calling the emitted `count_section` with an absent section key aborts with
`std::bad_optional_access`; the source returns early. The same applies to the named form
(`record.armature` emits `.get(flight::String("armature")).value()`).

## Required C++ shape

For a record whose value type is the erased dynamic value, the read must keep the erased carrier:

```cpp
auto value = record.get(key).value_or(flight::Any(flight::undefined));
```

A present `undefined` or `null` element is engaged and reads as itself; only a missing key becomes
undefined. The lookup is evaluated once, and records with a concrete value type are unaffected and
keep the strict read.

## Affected modules

`@flighthq/skeleton2d-formats` `dragonBonesSectionCounts.ts`, `spineJsonSectionCounts.ts` and
`skeleton2dJsonAnalyzer.ts` (typed Record reads of erased values). Carried by source patch
`compiler-typed-record-missing-key`; the consumer case lives in
`tests/generated_skeleton2d_formats_dragon_bones_section_counts_test.cpp`.
