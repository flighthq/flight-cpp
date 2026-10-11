# Owner-preserving dynamic named write

The compiler refuses every write through an owner-preserving dynamic named view even though the
runtime exposes a checked `NamedProperties::set(String, Any)` operation on that same retained owner.

## Minimal TypeScript

```ts
interface Effect { kind: string; amount?: number }
export function write(effect: Effect, key: string, value: number): void {
  const view = effect as unknown as Record<string, unknown>;
  view[key] = value;
}
```

## Expected C++

```cpp
const auto& receiver = effect;
const auto name = key;
auto assigned = value;
if (!flight::named_properties(receiver).set(name, flight::Any(assigned))) {
  throw std::logic_error("Flight named-property write rejected key or value");
}
```

Receiver, key, and value are evaluated once in source order. Rejection is observable; no record copy
or side storage is allowed.

## Affected modules

The shared material clone, copy, equality, base construction, and exact generic construction module
used by classic, PBR, unlit, and custom Scene3D materials.

## Carrying workaround

[`compiler-owner-preserving-named-write.patch`](../../source-patches/compiler-owner-preserving-named-write.patch)
uses the checked runtime setter only for values exactly representable in `flight::Any`. It expires when
upstream emits this owner-preserving mutation contract.
