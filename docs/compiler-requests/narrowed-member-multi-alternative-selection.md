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

Two details from the emitted placeholder worth recording, both measured after the `types` refresh
(`0dc94418`) landed: the refusal carries **no rule code** — the placeholder records it as
`refusal: unruled [compiler-restriction]`, because the `emissionError` at that site passes no rule —
and the module is now refused **wholesale**: the placeholder says "no part of it was generated", where
the earlier refusal still produced a best-effort partial with ten marked omissions. So the per-function
refusals that were observable before cannot be re-measured until this one clears.

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

Also refused at a627849: `computeStaggerDelay` in `@flighthq/tween` (`tweenStagger.ts`), which narrows
`from: 'center' | 'end' | 'start' | number` through `===` comparisons. Measured: testing
`typeof from === 'number'` first (equivalent over the declared domain) is still refused at the same site,
so no patch is carried. The `tween-stagger-origin-selection` override supplies it over the emitted
`std::variant<double, flight::String>`: each literal test selects the string alternative holding that
literal, and the final branch reads the numeric alternative.

## Root cause and carried compiler patch

The refusal concealed an unsound complement calculation. `TrayCreateCapabilityResult` stores the
`created` and `cancelled` source members in one C++ alternative. Excluding `created` dropped that whole
slot, including `cancelled`, and incorrectly told the later `'error' in result` expression that only the
three error members remained. Selecting that alternative would have thrown `bad_variant_access` for a
cancelled value.

[`compiler-coalesced-union-member-narrowing.patch`](../../source-patches/compiler-coalesced-union-member-narrowing.patch)
computes the complement per source member, retaining coalesced siblings. When the narrowed source union
still covers every C++ alternative with the same slot plan and no absence, it keeps the original binding
instead of projecting or copying it. Each narrowed member must still match exactly one alternative; an
unmatched or ambiguous member remains a refusal.

For Tray, `'error' in result` becomes an alternative test over the original variant, `cancelled` reaches
the false arm, and each error arm retains its error object by identity. A scratch regeneration advances
`createTrayIcon` to `cpp-contextual-union-inequivalent`: the generic alias `TrayCreateResult<Tray>`
re-emits provider-failure arms as templated twins, documented in
[generic-alias-non-dependent-anonymous-arms.md](generic-alias-non-dependent-anonymous-arms.md).
`startTrayIconAnimation` stops at a distinct contextual-union mismatch involving a nested variant with
a duplicated alternative. No Tray generated rows are committed here.

The patch does not change writes through multi-alternative narrowed bindings, optional-chain
discriminant lowering, or member-presence tests under optional bindings. Those measured boundaries
remain separate compiler issues. No source rewrite is carried.
