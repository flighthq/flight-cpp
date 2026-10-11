# Nullable and optional storage needs two absence channels

A member declared `T | null` uses `std::optional<T>` for null. Making that member optional as part of
`Partial<Row>` asks the same optional to also represent missing/`undefined`. A read can no longer
distinguish the two source states, causing presence-test refusals and invalid three-way conversions.

## Minimal TypeScript

```ts
interface NodeData { runtime: object | null }
export function state(value: Readonly<Partial<NodeData>>): number {
  if (value.runtime === undefined) return 0;
  if (value.runtime === null) return 1;
  return 2;
}
```

## Expected C++

The partial nullable cell needs all three states:

```cpp
struct NodeDataPartial {
  flight::Presence<flight::Ref<void>> runtime;
};

if (std::holds_alternative<flight::Undefined>(value->runtime)) return 0.0;
if (std::holds_alternative<flight::Null>(value->runtime)) return 1.0;
return 2.0;
```

Members that are only nullable or only optional should remain `std::optional<T>`.

## Affected modules

Current generated refusals span `@flighthq/node`, lighting, loader, mesh, render, texture, WGPU, and
format parsers. The committed refusal taxonomy records this as
`cpp-presence-test-without-absence-storage`.

## Carrying workaround

There is no semantics-preserving downstream representation workaround: converting the existing
`optional<T>` after the read would invent whether absence meant null or undefined. Individual source
patches avoid some optional-chain spellings, while the refusal ledger deliberately keeps the remaining
sites blocked. This request closes when flight-compiler emits `flight::Presence<T>` (or an equivalent
three-state carrier) only for optional-and-nullable cells.
