# An imported `Record` alias loses its emitted runtime representation

An exported alias from `@flighthq/types` can have an exact generated C++ carrier and still be
classified as an unsupported ambient reference when another package imports it. The refusal happens
before any operation on the value is considered, so one imported alias refuses every function in the
consumer module even though its defining module emits the required `flight::Record` representation.

## Minimal TypeScript

```ts
// types.ts
export interface Entry {
  value?: string;
}
export type Entries = Record<string, Entry>;

// consumer.ts
import type { Entries } from './types.ts';

export function createEntries(): Entries {
  return {};
}
```

At flight-compiler `2b687ea` and flight `a627849`, the alias definition has the exact generated
carrier:

```cpp
using Entries = flight::Record<flight::String, flight::Ref<Entry>>;
```

The consumer nevertheless refuses with rule
`cpp-imported-type-unsupported:unsupportedAmbientReference`:

```text
imported type Entries names an ambient type this runtime profile does not bind, so the target has no
representation for it
```

The import is not ambient source: it resolves to a package declaration the same SDK generation pass
already emitted. Its representation should therefore be recovered from that declaration rather than
looked up as a host binding.

## Pinned boundary

| source shape | result |
| --- | --- |
| emit `Entries` in its defining types module | emits `Record<String, Ref<Entry>>` |
| import `Entries` into another package's return type | refuses as an unsupported ambient reference |
| use the imported alias as a parameter or local annotation | refuses the containing function for the same reason |
| import the nominal `Entry` interface | emits `Ref<Entry>` normally |
| use `Record<string, Entry>` directly in the consumer | has a represented `flight::Record` carrier |

This is not a request to invent an unchecked representation for a genuinely ambient host type. The
required evidence is the resolved exported declaration and the carrier already selected for it in the
same generated SDK.

## Affected Flight module

`@flighthq/capture` `captureBaseline.ts` imports `CaptureBaseline` from
`@flighthq/types/contract`. At the same `a627849` pin,
`flight/types/capture_baseline.hpp` defines it as
`flight::Record<flight::String, flight::Ref<CaptureColumnBaseline>>`, while the capture module refuses
all seven public baseline operations at the first function's use of the alias. The remaining
`provenanceMember` helper emits because its imported field alias is represented as `flight::String`.

The refusal regresses the older `7e2fc7d` package emission, which represented `CaptureBaseline` and
reached the later presence-test and keyed-access boundaries. A package-local override restores the
seven source operations using the exact generated types; it does not change the shared runtime or
pretend an ambient type is bound.

## Required compiler behavior

Resolve the imported symbol to its exported declaration before the ambient-binding check. When that
declaration has a generated target representation, reuse its exact carrier, including nested key and
value carriers and the normal generated include. Preserve the refusal for unresolved declarations and
for declarations whose target representation is itself unsupported.

The recovery must be alias-transparent: it must not materialize a nominal owner for a `Record`, copy
its storage, or flatten its value references. In this case the consumer must use the same
`flight::Record<String, Ref<CaptureColumnBaseline>>` carrier and shared record identity that the types
header declares.
