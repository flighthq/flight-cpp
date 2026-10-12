# A member-presence narrow must select across every alternative that carries the member

`'error' in result` narrows a discriminated union to *several* alternatives, and the emitter then has
to read the member it tested. It looks for exactly one matching C++ alternative, finds three, and
refuses — so one expression blocks the whole module.

This is the module-level refusal that keeps `@flighthq/tray` from emitting today, immediately behind the
arm/value sibling-owner defect that
[`entity-intersection-arm-sibling-carriers.md`](entity-intersection-arm-sibling-carriers.md) and the
source patch `tray-create-result-and-runtime-owner` carry.

## Minimal TypeScript

`@flighthq/types`, `packages/types/src/Tray.ts:58`, and `@flighthq/tray`,
`packages/tray/src/tray.ts:83`:

```ts
export type TrayCreateCapabilityResult =
  | { readonly outcome: 'created' }
  | { readonly outcome: 'cancelled' }
  | { readonly error?: unknown; readonly outcome: 'runtime-api-unavailable' }
  | { readonly error?: unknown; readonly outcome: 'invalid-icon' }
  | { readonly error?: unknown; readonly outcome: 'tray-create-failed' };

// tray.ts
if (result.outcome !== 'created') {
  const out = allocateEntity<TrayCreateProviderFailureResult>();
  initializeTrayCreateProviderFailureResult(out, 'error' in result ? result.error : undefined, result.outcome);
  return finishEntity(out);
}
```

## Expected behavior

After `'error' in result` the narrowed type is the *union* of the alternatives that declare the
member — three of the four failure arms here — and reading `result.error` has to lower over all of
them. The alternatives disagree only in their discriminant, so a `std::visit` across the matching
alternatives (or a narrowed sub-variant built from them) is the shape; which one is an emitter
decision, but selecting a single alternative is not available and must not be required.

## Observed at `2b687ea`, flight `a627849`

```
cpp emission failed for @flighthq/tray/packages/tray/src/tray.ts: narrowed member structural switch
case must identify one C++ variant alternative
```

Emitted from `emitter`'s narrowed-union-member path (`emitNarrowedUnionMemberCpp` →
`emitCppNarrowedUnionValueCpp`), which matches the narrowed type against `representation.alternatives`
by exact equality, target type, member name, discriminant, and object representation, and raises when
`matches.length !== 1`. A member test over a subtype slice of a union is exactly the case where that
count is greater than one.

## Why a source rewrite is not carried

The obvious rewrite — `result.outcome === 'cancelled' ? undefined : result.error` — is not faithful,
and the pinned checkers say so twice over:

- **It answers a different question.** `'error' in result` tests *property presence*; the rewrite tests
  the *discriminant*. They agree here only because of an accident of the current declaration: of the
  four failure arms, `'cancelled'` happens to be the only one that does not declare `error`. Add a
  failure arm without `error`, or let a provider construct one of these arms and omit the property, and
  the rewrite silently keeps answering the old question. A compiler should not depend on that
  coincidence, and a source patch that encodes it would.
- **It would be a source change made solely for the emitter.** AGENTS.md's rule is that a workaround may
  not replace an ordinary optional property check with a target-specific spelling; that is precisely
  what this rewrite is, and the module is blocked by one expression rather than by an unrepresented
  capability.

**For the record, on `exactOptionalPropertyTypes`:** it is **not enabled anywhere that governs this
test**. The compiler's own analysis options (`createCompilerTypeScriptAnalysisIdentity`) set
`strictNullChecks` and `noImplicitAny` only, and the pinned checkout's `tsconfig.base.json` sets
`"strict": true`, which does not include `exactOptionalPropertyTypes`. So where the compiler checks this
source, present-`undefined` and absent are the same type, and `'error' in result` narrows purely by
declared property membership. Nothing about this request depends on a presence distinction the checker
makes; the request is that the emitter lower the test the source wrote, over the alternatives that
declare the member, instead of requiring exactly one.

## Affected modules

`@flighthq/tray` — `createTrayIcon` and the whole module behind it. 3 of 4 modules are emitted and no
header compiles; `contract.ts`, `enableTrayGuards.ts` and `index.ts` are dependency-refused.

## Carrying workaround

None, deliberately. The landed patch `tray-create-result-and-runtime-owner` removes the sibling-owner
blocker *ahead* of this one and leaves this refusal standing rather than papering it over with the
rewrite above. The rest of `tray.ts`'s refusal chain is inventoried separately.
