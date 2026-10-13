# A property union loses the guard's evidence: a host-bound alternative, and an `unknown` segment

`NetResponseBody` is `string | unknown | ArrayBuffer | Blob | null`. Two modules read that property the
same way -- guard it with `instanceof ArrayBuffer`, then read it behind the guard -- and both fail, but not
at the guard:

```
cpp emission failed for @flighthq/audio/.../audioResourceFrom.ts: contextual C++ union conversion
requires equivalent source union evidence: target optionalSingle carriers [flight::ArrayBuffer] from
property source optionalVariant carriers [flight::ArrayBuffer, flight::Blob, flight::String,
flight::Any]. Narrow or convert the source ...
```

Measured at flight `a627849` / flight-compiler `2b687ea`, on `@flighthq/audio`'s `audioResourceFrom.ts`
and `@flighthq/loader`'s `load.ts`. It is not an `instanceof` defect: `typeof o.body === 'string'` on the
same property refuses too, so the operator is not what is missing -- the **union** is. Two independent
axes are measured below, and the two modules sit on both of them at once.

## The measurement

One interface, one property, one guard, one read; each row changes exactly one thing:

```ts
export interface Obj { body: <union>; }
export function read(o: Obj): T | null {
  if (<guard>) return o.body;
  return null;
}
```

| `body` declared as | guard | `Blob` in the ambient surface | type recorded on the read | narrowed type on the read | emitted |
| --- | --- | --- | --- | --- | --- |
| `string \| ArrayBuffer \| null` | `o.body instanceof ArrayBuffer` | n/a | the union | `ArrayBuffer` | guard lowers, read projects |
| `string \| ArrayBuffer \| null` | `typeof o.body === 'string'` | n/a | the union | `string` | same |
| `string \| ArrayBuffer \| URL \| null` | `o.body instanceof URL` | n/a | the union | `URL` | same |
| `string \| Blob \| null` | `typeof o.body === 'string'` | **absent** | none | none | refuses (the message above) |
| `string \| Blob \| null` | `typeof o.body === 'string'` | **declared** | none | `string` | guard lowers, read projects |
| `string \| ArrayBuffer \| Blob \| null` | `o.body instanceof ArrayBuffer` | **absent** | none | none | refuses (the message above) |
| `string \| ArrayBuffer \| Blob \| null` | `o.body instanceof ArrayBuffer` | **declared** | none | `ArrayBuffer` | guard lowers, read projects |
| `string \| unknown \| ArrayBuffer \| null` | `o.body instanceof ArrayBuffer` | n/a | the union, with an `unknown` member | **none** | refuses (the message above) |
| `string \| unknown \| ArrayBuffer \| null` | `typeof o.body === 'string'` | n/a | the union, with an `unknown` member | **none** | refuses at the guard: "typeof requires closed runtime type evidence" |
| `string \| unknown \| ArrayBuffer \| Blob \| null` | `o.body instanceof ArrayBuffer` | absent | none | none | refuses (the message above) -- the measured site |

## Axis 1: an alternative the compiler has no representation for

`Blob` is host-owned. It is bound for this profile by a declared external binding
(`bindings/runtime.json`), and it is deliberately absent from the compiler's own ambient surface, whose
header comment says "a member absent here is a member the compiler has not decided how to lower". Rows
four and six show what that costs a **property**: a declared union naming `Blob` records no type and
narrows nothing, for `typeof` and `instanceof` alike -- while the same union with `Blob` declared
narrows and emits correctly (rows five and seven).

So the surface and the binding manifest disagree about which types exist: the backend will name a carrier
for `Blob` and emit it, and the semantic layer cannot type anything that mentions it. A profile's
host-bound types need to reach the analysis as declared types, from the binding manifest rather than from
a second hand-maintained list.

## Axis 2: a segment the checker absorbs to `unknown`

`string | unknown | ...` is `unknown` to the TypeScript checker, and the property-narrowing rule asks the
checker:

```
if (!declared.isUnion()) return undefined;
```

`getTypeScriptCheckerFlowNarrowedTypeEvidence` in `typeScriptSemanticLowering.ts`, called from
`getTypeScriptPropertyNarrowedType`. For these unions `declared` is `unknown`, so the rule returns before
it compares a single alternative -- even though the IR did record the union, with the unresolved segment
as an `unknown` member (rows eight and nine show the recorded type). Two consequences: the guarded read
gets no narrowing, and `typeof` on a union with an unresolved member refuses outright.

The recorded union is the evidence the rule should read when the checker's declared type absorbed it.

## What this is not

Not a missing runtime carrier, and not a compiler request to file against a lowering that does not exist:
rows five and seven emit both the guard and the projection today, from the evidence the semantic layer
already knows how to produce when the union is representable. The fix is upstream of emission, in which
types the analysis can see and which proof it reads.

## Carrying workaround

None for `@flighthq/loader`, whose generated header fails to compile at the projection.
`@flighthq/audio` carries a hand-written override that supplies the projection itself; its manifest entry
records that this is what remains after `source-patches/compiler-runtime-instanceof-lowering.patch` made
the guard the emitter's own.
