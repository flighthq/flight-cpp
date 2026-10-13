# Reading the elements of an erased array must use the runtime's exact array recovery

JSON-shaped parsers take `raw: unknown`, narrow it with `Array.isArray(raw)`, iterate it, and test each
element against `null` before reading it as an object. TypeScript types the narrowed array as `any[]`, so
each element is `any`. The emitter refuses the null test on that element, and every faithful respelling
measured below fails one step later, because it never reads the erased value back as an array.

The runtime already has the carrier: `flight::Any` holds an erased array as `detail::AnyArray`, which keeps
the array's own identity and its exact type, and `Any::external_if<flight::Array<T>>()` recovers it only
when it was stored as exactly that type. The emitter's own refusal text says "flight::Any has no array
alternative or checked exact-array extraction", which is the part that is out of date.

Measured at flight-compiler `2b687ea` with all 42 of this repository's compiler source patches applied, one
module per case; the emitting cases were then compiled with clang 22.

## Minimal TypeScript (measured)

```ts
// a_isarray_iterate_null_test.ts
export function countObjects(raw: unknown): number {
  if (!Array.isArray(raw)) return 0;
  let n = 0;
  for (const entry of raw) {
    if (entry === null || typeof entry !== 'object') continue;
    n++;
  }
  return n;
}
```

```text
cpp emission failed for @local/source/a_isarray_iterate_null_test.ts: a presence test against null has no
absence channel in the emitted C++ storage for identifier
```

## Pinned boundary

| case | emission | C++ |
| --- | --- | --- |
| `Array.isArray(raw)`, iterate `raw`, `entry === null` | **refused** (no absence channel) | — |
| `raw: readonly unknown[]` parameter, same loop and test | emits | — |
| `entry: unknown` alone, `entry === null` | emits | — |
| `Array.isArray(raw)`, then `raw.length` | emits (`flight::is_array`, `named_properties(raw).get("length")`) | — |
| narrowed, then `const items: readonly unknown[] = raw` and iterate `items` | emits | **fails**: no viable conversion from `flight::Any` to `flight::Array<flight::Any>` |
| narrowed, then iterate `raw as readonly unknown[]` | **refused**: no identity-preserving reading as `flight::Array<flight::Any>` | — |
| `Array.isArray(owner.bone)`, then the typed local | emits | **fails**, as above |

## Required C++ shape

After `flight::is_array(raw)`, recover the array through the checked exact recovery and iterate it, with each
element an `flight::Any`, whose null test is already represented:

```cpp
if (!flight::is_array(raw)) return 0.0;
const auto* items = raw.external_if<flight::Array<flight::Any>>();
if (items == nullptr) /* stored as another element type: no honest erased reading */;
for (const flight::Any& entry : *items) {
  if (entry.is_null() || !entry.is_object()) continue;
  n += 1;
}
```

The recovery is only exact for an array stored as `Array<Any>`. Producers that store erased JSON arrays
must store that type (JSON parsing currently produces `flight::JsonValue`, not `Any`, and the parser
modules' own `JSON.parse` sites are not emitted either), so both halves need to agree on it.

## Affected Flight modules

`@flighthq/skeleton2d-formats` at flight `a627849`; every site has the measured shape.

Refused as "a presence test against null has no absence channel":
- DragonBones: `dragonBonesAnimationsHandler.ts`, `dragonBonesBoneTimelineHandler.ts`, `dragonBonesParse.ts`,
  `dragonBonesParseHelpers.ts`, `dragonBonesSectionCounts.ts`, `dragonBonesSlotTimelineHandler.ts`,
  `dragonBonesSlotsHandler.ts`;
- Spine JSON: `spineJsonBoneTimelineHandler.ts`, `spineJsonBonesHandler.ts`, `spineJsonSlotsHandler.ts`,
  `spineParse.ts`.

Refused as "no array alternative" (reading an erased value as an array):
- `dragonBonesBonesHandler.ts`, `dragonBonesSkinsHandler.ts` (`flight::Array<double>`), and
  `skeleton2dJsonAnalyzer.ts` (`flight::Array<flight::Any>`).

Together these refuse 14 of the package's 59 modules directly, and most of its 24 dependency cascades.

## Carrying workaround

None. No faithful source spelling reads an erased JSON array: each respelling measured above refuses or
fails to compile.
