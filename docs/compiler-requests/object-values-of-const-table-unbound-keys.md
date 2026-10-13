# `Object.values` of a const object table must enumerate its members

`Object.values(Table)` over an exported `as const` object -- the enum-like tables the SDK uses for string
vocabularies -- lowers to an enumeration of `flight::named_properties(table)`. That view reports the members the
SDK structural-member table binds, and the table is built from the row keys of emitted declarations. A const
table's own source keys (`Equal`, `GreaterThan`, ...) are not among them, so the view reports no keys at all, and
`Object.values(Table)` is an empty array. Nothing is refused and the code compiles; every membership test against
the table is silently false.

Measured at flight-compiler `2b687ea` with all 44 compiler source patches applied, one module per case, compiled
against this repository's runtime and member table and run.

## Minimal TypeScript (measured)

```ts
export const Comparison = { Equal: 'Equal', GreaterThan: 'GreaterThan' } as const;
export type Comparison = (typeof Comparison)[keyof typeof Comparison];

export function isComparison(value: string): boolean {
  return (Object.values(Comparison) as string[]).includes(value);
}
export function comparisonCount(): number {
  return Object.values(Comparison).length;
}
export function isComparisonExplicit(value: string): boolean {
  return value === Comparison.Equal || value === Comparison.GreaterThan;
}
```

```cpp
return ([&]() { const auto named_view = flight::named_properties(comparison); flight::Array<flight::Any> named_entries;
                for (const auto& named_key : named_view.keys()) { named_entries.push(named_view.get(named_key)); }
                return named_entries; }()).includes(value);
```

```text
isComparison("Equal") = false, comparisonCount() = 0, isComparisonExplicit("Equal") = true
```

## Required C++ shape

A const table is a closed, statically known object: its values can be enumerated from its own members in source
order, with no dynamic view at all:

```cpp
flight::Array<flight::String>{comparison->equal, comparison->greater_than}.includes(value)
```

Alternatively, the generated member table can bind each const table's source keys, so the dynamic view sees them.

## Affected Flight modules

`@flighthq/statechart` `statechart.ts` `validateStatechart`: `Object.values(StatechartComparison).includes(...)`
rejected every condition, so `createStatechartInstance` threw `TypeError: ... has an invalid comparison` for any
chart with a conditional transition. It is the only `Object.values` over a const table in the generated tree at
a627849.

## Carrying workaround

`statechart-comparison-closed-set`, a package-local source patch, spells the membership test as the six explicit
comparisons against `StatechartComparison`'s members. `generated_statechart_test` checks that every comparison is
accepted and an unknown one is still rejected.
