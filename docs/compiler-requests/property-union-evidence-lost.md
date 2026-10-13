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

`@flighthq/loader` carries no override and compiles: 4/4 headers at `-j2`, with the guarded read emitted as
`flight::Uint8Array(flight::union_kind_or_throw<flight::ArrayBuffer>(response->body))`. `@flighthq/audio`
still carries an override, and after this patch it supplies exactly one thing -- the logical-or lambda return
type, which is its own filed request.

## Closed: the guard carries the proof, not the declaration

The pair of evidence patches left one hop open: `response.body` is declared through an **alias**
(`body: NetResponseBody`), so the analysis states the name of the union rather than its alternatives, and the
narrowing had nothing to compare. Three routes were measured (below), and the one that reached the site
traded a bookkeeping record for it. The route taken instead is the one the emitter was already holding:
**the guard names the alternative AND the carrier it tested**, so the backend carries that name into the
branch the guard controls and emits the read from the same carrier expansion.

`source-patches/compiler-guarded-path-narrowing.patch` does that, and resolves nothing:
`flight::union_kind_if<T>(response->body)` proves the branch, and a read of `response.body` inside it is
emitted as `flight::union_kind_or_throw<T>(response->body)` -- the throwing twin of the very question that
admitted the branch, over the same carrier expression. No alias is opened a second time and no type
reference is materialized that the module never wrote.

Its lifetime is TypeScript's, and each rule is a withdrawal rather than an invention -- where the proof is
gone the read is emitted exactly as it was before this patch:

| the proof survives | the proof is withdrawn |
| --- | --- |
| inside the branch the guard controls (including the fall-through of a terminating guard, `if (!(test)) throw;`) | at the branch's end |
| through a call in the same statement, whose arguments are evaluated before it runs | for the statements AFTER a call, because a call may write the property |
| | on a write to the path, or to any prefix of it (`response = ...` invalidates `response.body`) |

A read the semantic layer's own proof already reaches is left to that proof, so nothing is projected twice,
and a projection that runs where its guard did not hold is a `std::logic_error` -- a compiler defect with no
TypeScript shape, never a `flight::Error` a source `catch` could take for an application failure.

Controls: `cppGuardedPathNarrowing.test.ts`, 11 cases -- the evidence-starved read, the read with evidence,
a read the semantic proof already reaches, a terminating guard's fall-through, identity (both name the same
carrier), and the four withdrawals plus the branch boundary and a refusal. Mutations: branch recording
disabled fails 3, the write withdrawal 2, the call withdrawal 1, emitting the carrier with the proof live
(a double projection) 6, the terminating-guard recording 1.

## The three routes that were measured and rejected, kept for the record

| route | reaches the measured site | why not |
| --- | --- | --- |
| resolve the alias through the declaration's shape (`getIrTypeConstructionTargetShape`) | yes | it also materializes the declaration's members, which registers type references the module never wrote: `SignalData` appeared in `load.ts`'s imported-name record in `initialization.json` and in the refused `resourceLoader` placeholder |
| resolve it through the IR type-binding table by binding id | no | the lookup misses an imported alias |
| resolve it through the checker's `aliasSymbol` on the declared type | no | the checker's declared type for these members carries no alias to follow |
