# A property union loses the guard's evidence: a host-bound alternative, and an `unknown` segment

`NetResponseBody` is `string | unknown | ArrayBuffer | Blob | null`. Two modules read that property the
same way -- guard it with `instanceof ArrayBuffer`, then read it behind the guard -- and both failed, but
not at the guard:

```
cpp emission failed for @flighthq/audio/.../audioResourceFrom.ts: contextual C++ union conversion
requires equivalent source union evidence: target optionalSingle carriers [flight::ArrayBuffer] from
property source optionalVariant carriers [flight::ArrayBuffer, flight::Blob, flight::String,
flight::Any]. Narrow or convert the source ...
```

Measured at flight `a627849` / flight-compiler `2b687ea`, on `@flighthq/audio`'s `audioResourceFrom.ts`
and `@flighthq/loader`'s `load.ts`. It is not an `instanceof` defect: `typeof o.body === 'string'` on the
same property refuses too, so the operator is not what is missing -- the **union** is. Two independent
axes were measured, and the two modules sat on both of them at once.

## Carried by a compiler patch

`source-patches/compiler-profile-type-name-evidence.patch` carries both halves, and
`source-patches/compiler-runtime-instanceof-lowering.patch` carries the guard and projection that consume
them. The controls are `cppRuntimeInstanceOf.test.ts` (15 cases) and the runtime's own
`tests/union_kind_test.cpp`.

**Axis 1: an alternative the compiler had no representation for.** `Blob` is bound for this profile by a
declared external binding and was absent from the compiler's ambient surface. A probe showed the loss was
not in the type builder -- it produced the whole four-alternative union with `Blob` as a named ambient
reference -- but in one gate, `isIrExpressionValueTypeEvidence`, which accepted a named type only when it
was a `binding` or one of the compiler's own known receivers. The analysis is now handed the type-space
names the loaded binding profiles provide (`profileTypeNames`), as **explicit input**: the names come from
the profiles `sdkGeneration` actually loaded, are recorded in `generated/manifest.json` beside them, and
the compiler keeps no list of its own. A name the input does not provide keeps no evidence, so nothing is
invented for it -- which is the negative half of the control in `cppRuntimeInstanceOf.test.ts`.

**Axis 2: a segment the checker absorbs to `unknown`.** `string | unknown | ...` IS `unknown` to the
TypeScript checker, so the property-narrowing rule returned at `if (!declared.isUnion()) return undefined`
before comparing a single alternative -- even though the IR had recorded the union. Narrowing now falls
back to the alternatives the **declaration** states when the checker's declared type is not a union at
all. Only a real selection counts: the flow must have kept fewer alternatives than the declaration, so a
guard that merely removed absence stays on the presence lane, and exactly one declared alternative may
answer the flow's name.

## What the fix does not do, precisely

- A name no loaded profile provides gets no evidence and the read is not projected; the header then fails
  to compile at the conversion rather than refusing by rule at emission. The emitter spells the carrier
  from the declaration whether or not the analysis kept it, so there is nothing left at the emission site
  to detect. That is the residual shape of this defect, and it is pinned by a control rather than papered
  over.
- The **example generator** (`scripts/upstreamExampleGeneration.mjs`) does not thread the names yet, so an
  example reading such a union keeps today's behavior. The SDK path is wired; the example path is a
  follow-up in the same shape.
- `typeof o.body === 'string'` on a union with an unresolved member still refuses: that guard asks its own
  question about a closed runtime type and has no written-union lane. The measured sites use `instanceof`,
  which is lowered.

## The measurement, as it stood before the fix

One interface, one property, one guard, one read; each row changes exactly one thing.

| `body` declared as | guard | `Blob` in the ambient surface | type recorded on the read | narrowed type on the read | emitted |
| --- | --- | --- | --- | --- | --- |
| `string \| ArrayBuffer \| null` | `o.body instanceof ArrayBuffer` | n/a | the union | `ArrayBuffer` | guard lowered, read projected |
| `string \| ArrayBuffer \| null` | `typeof o.body === 'string'` | n/a | the union | `string` | same |
| `string \| ArrayBuffer \| URL \| null` | `o.body instanceof URL` | n/a | the union | `URL` | same |
| `string \| Blob \| null` | `typeof o.body === 'string'` | **absent** | none | none | refused (the message above) |
| `string \| Blob \| null` | `typeof o.body === 'string'` | **declared** | none | `string` | guard lowered, read projected |
| `string \| ArrayBuffer \| Blob \| null` | `o.body instanceof ArrayBuffer` | **absent** | none | none | refused (the message above) |
| `string \| ArrayBuffer \| Blob \| null` | `o.body instanceof ArrayBuffer` | **declared** | none | `ArrayBuffer` | guard lowered, read projected |
| `string \| unknown \| ArrayBuffer \| null` | `o.body instanceof ArrayBuffer` | n/a | the union, with an `unknown` member | **none** | refused (the message above) |
| `string \| unknown \| ArrayBuffer \| null` | `typeof o.body === 'string'` | n/a | the union, with an `unknown` member | **none** | refused at the guard: "typeof requires closed runtime type evidence" |
| `string \| unknown \| ArrayBuffer \| Blob \| null` | `o.body instanceof ArrayBuffer` | absent | none | none | refused (the message above) -- the measured site |

Declaring `Blob` in the ambient surface was tried and moved the CLI rows, which is what identified the
checker's view as one of the two things involved; it did not repair the recorded type, which is the gate
above. Both halves of axis 1 are now carried by the input rather than by the surface, so no package's
analysis depends on a name the compiler happens to declare.

## Carrying workaround

`@flighthq/audio`'s override supplies the projection through `flight::union_kind_or_throw`, the same
mechanism the emitter uses for the guard, and its native test passes; what else remains in that copy is the
logical-or lambda return type, which is its own filed request. `@flighthq/loader` carries no override and
does not compile.

## The residual, measured

Both patches land the mechanism and the two axes of evidence, but the two measured sites read a property
whose declared type is an **alias**: `response.body` is declared `body: NetResponseBody`, and the
alternatives live behind the name. The narrowing compares the alternatives *the declaration states* --
which for an aliased member is the name, not the union -- so the guarded read still has no proof to project
through, and `load.hpp` keeps `flight::Uint8Array(response->body)`.

Three routes were measured, each with the same synthetic reduction that other rows in this file use, and
then against `@flighthq/loader` itself:

| route | reaches the measured site | cost |
| --- | --- | --- |
| resolve the alias through the declaration's shape (`getIrTypeConstructionTargetShape`) | **yes** -- `union_kind_or_throw<flight::ArrayBuffer>(response->body)` in `load.hpp` | it also materializes the declaration's members, which registers type references the module never wrote: `SignalData` appeared in `load.ts`'s imported-name record in `initialization.json` and in the refused `resourceLoader` placeholder's import list |
| resolve it through the IR type-binding table by binding id | no -- the lookup misses an imported alias | none |
| resolve it through the checker's `aliasSymbol` on the declared type | no -- the checker's declared type for these members carries no alias to follow | none |

So the honest statement is: the site is one hop short, and the hop is reachable -- but not by any route that
is both effective and side-effect free as the analysis is currently given its input. The next unit should
decide between (a) teaching the analysis to hand the emitter's own alias resolution to the narrower, so the
projection is emitted from the same expansion the carrier came from, and (b) giving the backend the proof:
the guard it lowers already names the alternative, so a branch could carry it to the read without the
semantic layer resolving anything. (b) is the one that matches "one shared mechanism" most closely, and it
is the route this file recommends.
