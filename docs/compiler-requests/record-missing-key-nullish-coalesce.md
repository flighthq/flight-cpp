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
