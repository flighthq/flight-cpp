# `record[key] ?? null` throws for a missing key instead of yielding null

When a record's element type already admits `undefined`, `record[key] ?? fallback` is emitted as
`record.get(key).value()`. `Record::get` answers a missing key with an empty outer optional, so `.value()`
throws `std::bad_optional_access`. In TypeScript a missing key reads as `undefined`, and `??` replaces it with
the fallback. The `?? null` is dropped entirely: an explicit `undefined` stored at the key also has to become
`null`. The code compiles, so the failure appears only at run time, and only on the path the fallback exists
for.

Measured at flight-compiler `2b687ea` with all 44 compiler source patches applied: 1 module emitted, 0 refused.

## Minimal TypeScript (measured)

```ts
export interface Thing {
  value: number;
}

const slot = Symbol('slot');

export function readSlot(state: object): Thing | null {
  return (state as Record<symbol, Thing | undefined>)[slot] ?? null;
}

export function readKey(table: Record<string, Thing | undefined>, key: string): Thing | null {
  return table[key] ?? null;
}
```

## Observed

```cpp
inline std::optional<flight::Ref<Thing>> read_slot(flight::Ref<void> state) {
  return static_cast<flight::Record<flight::Symbol, std::optional<flight::Ref<Thing>>>>(state).get(slot).value();
}

inline std::optional<flight::Ref<Thing>> read_key(flight::Record<flight::String, std::optional<flight::Ref<Thing>>> table, flight::String key) {
  return table.get(key).value();
}
```

Natively, `readSlot` on an object that was never written throws `bad_optional_access`.

## Expected behavior

Flatten the read's two absences into the one the source has: no key, or a key holding `undefined`. For
example, `auto e = record.get(key); return e.has_value() && e->has_value() ? *e : fallback;`. That is the same
lowering `??=` already receives in the same module (`enableParticleEmitterSignals`), which tests
`has_value()` at both layers.

## Affected modules

`@flighthq/particles` `particleEmitterSignals.ts`, in `getParticleEmitterSignals`. Before any
`enableParticleEmitterSignals` on that state, the function throws instead of returning null. Upstream's
"returns null when signals have not been enabled" case and every emitter that queries signals it never
enabled hit this path.

## Carrying workaround

The `particles-signals-missing-slot-reads-null` override carries the generated module unchanged except for
`getParticleEmitterSignals`, which tests both absences.

A source patch was measured first, and no equivalent spelling lowers correctly:
- `const v = s[slot]; return v === undefined ? null : v` is refused: the contextual union value type is not a
  represented runtime domain.
- `Partial<Record<symbol, T>>` is refused: it has an open member set.
- `const v: T | undefined = s[slot]; return v ?? null` emits `return value;` from an
  `optional<optional<Ref<T>>>`. The coalesce is dropped again, and the result does not compile.

## A plain index read has the same defect (measured)

No `??` is needed to hit it. Without `noUncheckedIndexedAccess`, `d[key]` on a `Record<string, T>` has type `T`,
and the emitter lowers it as `d.get(key).value()`. A missing key then throws where JavaScript reads
`undefined`. Measured at flight-compiler `2b687ea`, with all 44 compiler patches applied:

```ts
export function numA(d: Record<string, string | number | boolean>, key: string, def = 0): number {
  const v = d[key];                       // const auto v = d.get(key).value();  -> throws for a missing key
  return typeof v === 'number' && Number.isFinite(v) ? v : def;
}
```

A typed local, `const v: string | number | boolean | undefined = d[key]`, still throws. The spelling
`Object.hasOwn(d, key) ? d[key] : undefined` returns the default.

This affects `@flighthq/particles-formats` `particleDesignerParse.ts` `num` and `str`, which every document
parse and every incomplete plist reaches. It is carried by the `particles-formats-pd-dict-own-key-reads`
source patch.

## Native Collada id read

The same missing-key unwrap occurs in colladaXml.ts colladaIdOf: the source
`element.attributes.id ?? null` emits `attributes.get("id").value()`. The bounded native Collada
override returns that original get optional directly: a missing key is null, a present empty string
remains present, and the original attributes are read once without an own-key-only guard. The
[public-header carry and qualification](builtin-numeric-predicate-callback.md#native-collada-public-header-carry-2026-10-17)
records the paired finite-callback correction, actual original-header compile/link/run of all14
pinned cases plus numeric/owner edges, and the unchanged generated source/debt. This resolves this
native site only; the general Record lowering request remains open.

## Primitive dictionary storage at nullish boundaries

The actual SVG cascade also reaches this defect through named reads, nested coalesces
and numeric-parser arguments. A missing `values.color` is unwrapped before its fallback;
`first ?? values['color'] ?? 'fallback'` drops the second lookup's undefined channel.
A native baseline throws `bad_optional_access` for a missing key instead of executing
the source fallback.

`compiler-primitive-record-nullish-storage` retains the existing optional lookup storage
for nonnullable string, number and boolean dictionary payloads. It carries that storage
through named nullish reads, nullish result evidence, matching immutable local storage
and contextual union arguments. A matching single optional is reused; a wider target
receives either the original payload or exactly Undefined, keeping Null distinct.
Receiver/key evaluation and fallback laziness are unchanged. Empty strings, false,
signed zero and NaN remain present values. Public function ABI and erased dictionary
or named-view storage are unchanged.

The native prototype passes named/indexed/nested/optional-chain/local cases, readonly
aliases, exact dual-sentinel arguments, primitive edges and evaluation/throw order.
Eleven same-stack Any, unknown, readonly named-view, nullable payload, Map, non-Record,
direct-read, optional-receiver, multi-payload and object-payload counterexamples retain
byte-identical output or the same refusal. This is a bounded storage correction;
other independent missing-key/nullable payload boundaries in this request remain open.
Declared-stack and normal original public consumers are still being qualified.

The ordered 79-compiler/201-Flight stack now applies with byte-exact tested source, builds,
passes all 14 focused tests and reverses both pins cleanly. The selected existing groups
are 16 pass/1 unchanged failure on both the preceding 77 stack and this candidate. The
full compiler check retains the same four failing gates and five foreign fixture errors,
with no new fixture errors. Normal generation and final original consumers remain pending.

Final qualification: the declared81compiler/201Flight stack builds and passes14focused
groups, with clean ordered apply/revert. Normal scene2d-formats generation emits the
actual style/color bodies and retains every prior definition/refusal. Registered original
gradient/cascade/color+lookup consumers build/link/CTest2/2PASS; standalone four SVG
roots compile. See [the final original closure evidence](numeric-optional-chain-result-erased.md#final-original-stylecolorgradient-qualification-2026-10-18)
for full source cases, exact scope and independent gate limitations.
