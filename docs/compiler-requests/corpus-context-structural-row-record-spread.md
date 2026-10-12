# A corpus-context structural row construction loses an exact Record spread

The committed P-3D2 inventory refuses the object passed to `reportImportDiagnostic` in
`threeDsParse.ts`:

```text
structural-row construction requires explicit named properties
```

The expression has one spread and one named property. Its source and destination use the same
`Record<string, boolean | number | string>` value domain, so no Record cell conversion is required.
Unlike the other requests in this directory, however, the exact source-shaped fixture reaches a
different Record-spread rule under the pinned runtime profile. This page records a corpus-context
defect boundary, not a minimal reproducer of the committed diagnostic that is ready to turn into an
upstream unit test.

## Exact source shape

```ts
type DetailValue = boolean | number | string;

interface Tally {
  count: number;
  detail: Record<string, DetailValue>;
}

function report(detail?: Readonly<Record<string, DetailValue>>): void {}

export function flush(tallies: Map<string, Tally> | null): void {
  if (tallies === null) return;
  for (const tally of tallies.values()) {
    report({ ...tally.detail, count: tally.count });
  }
}
```

At flight-compiler `2b687ea`, with flight-cpp's pinned runtime bindings, this single-module fixture
refuses at the adjacent `cpp-record-spread-source-unrepresented` rule. Assigning the narrowed map to an
explicit `Map<string, Tally>` local makes it emit. Putting `Tally` and `report` in separate packages
also makes the otherwise unchanged fixture emit. The complete pinned package graph, by contrast,
records the structural-row refusal for `packages/scene3d-formats/src/threeDsParse.ts`.

## Reduction boundary and non-reproductions

| source shape | result |
| --- | --- |
| spread from a `Tally` parameter | emits |
| spread while iterating a non-null `Map<string, Tally>` | emits |
| exact single-module nullable-Map fixture above, with the runtime binding profile | reaches adjacent `cpp-record-spread-source-unrepresented` |
| assign the narrowed map to an explicit `Map<string, Tally>` local | emits |
| exact fixture split across types, diagnostics, and parser modules | emits |
| complete pinned `threeDsParse.ts` package-graph emission | refuses with the committed diagnostic |

The adjacent result proves that iterator element evidence is sensitive to the narrowed `Map | null`
carrier, while the cross-package result proves that the simplified module boundary changes which
evidence wins. Neither explains why the complete graph classifies the target as a structural-row
construction. A fix to generic Record spread lowering alone cannot be claimed to clear the corpus
diagnostic.

## Required behavior

The complete graph should construct the same readonly Record carrier that the minimal forms construct.
It must evaluate `tally.detail` once, enumerate its own enumerable string entries in Record order, then
write `count` last so it replaces an existing `count` entry. It must not turn the Record into a closed
interface, drop dynamic keys, or copy a pre-existing object merely to change its C++ owner.

The next compiler reduction should retain the exact imported owners and package barrel graph from the
failing module, then determine which additional declaration changes construction planning. Until that
test reproduces, this request deliberately makes no narrower root-cause claim.

## P-3D2 reach and adjacent requests

The measured site is the one diagnostic flush in
`packages/scene3d-formats/src/threeDsParse.ts`. It is a committed first refusal.

`materialPresets.ts` currently carries the same final diagnostic, but its spreads target named
`Partial<StandardPbrMaterial>` rows and are not evidence for this exact Record spread. The older
[named-interface Partial request](named-interface-partial-shape.md) now records that its minimal fixture
emits at this compiler pin. [`Object.assign`](object-assign-onto-structural-record.md) is also separate:
it mutates an existing target, while this expression constructs a fresh value.

No source rewrite, compiler patch, or generated repair is carried for the 3DS site.
