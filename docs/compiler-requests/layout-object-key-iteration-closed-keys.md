# Object key iteration needs the key evidence the type already carries

A `for…in` whose only job is to answer "does this object have any own key" is refused, even though the
operand's type is a closed declared shape whose keys are known.

Measured at `2b687ea`, flight `a627849`, from the package's refusal ledger.

## Minimal TypeScript

`@flighthq/layout`, `packages/layout/src/anchorLayout.ts:55`:

```ts
function hasOwnKeys(value: Readonly<Record<string, unknown>>): boolean {
  for (const _key in value) return false;
  return true;
}
```

## Observed

```
cpp emission failed for @flighthq/layout/packages/layout/src/anchorLayout.ts: object key iteration requires
closed key evidence
```

## Expected behavior

A closed structural shape has a known key set and the runtime already carries the member table that
enumerates it, so the iteration is answerable without a dynamic key traversal: the loop's body runs for
the first own key and the check reduces to "the table has at least one entry". Where the operand really is
an open record, the refusal is the right answer — but here the declaration is closed, and the refusal
message asks for evidence the type system already has.

## Affected modules

`@flighthq/layout` — `anchorLayout.ts`, whose only refusal this is. Its sibling modules refuse separately
(see [`assertion-to-nullable-variant-alternative.md`](assertion-to-nullable-variant-alternative.md)).

## Carrying workaround

None. `@flighthq/layout` is `partial` with three refused modules; no supply is carried for this one.
