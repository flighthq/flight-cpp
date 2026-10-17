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

## G2 anchor container is an open object at the current pin

At Flight `a627849` / compiler `806d257`, the actual anchorLayout.ts:53 helper is
isEmptyStyle(value: object|null), with `if (value===null) return true;` followed by
`for (const _key in value) return false; return true;`. The operand comes from the original
LayoutNode<> containerStyle, an optional ErasedRef in C++, not the closed-record reduction
above. That historical example does not prove the current arbitrary object's key evidence.
Source for-in observes enumerable keys (including inherited enumerable keys); an own-key-only
or empty static table shortcut is not equivalent to this source contract.

Preserve null acceptance and enumerate the actual retained object's keys/effects, or provide
an explicit source/runtime key contract that faithfully represents that observation. A
fabricated empty container, copied fields, narrowing to built-in layout shapes or unconditional
success would change InvalidContainerStyle rejection and its ordering before item validation.
The helper remains omitted; the resolver is independently refused at nullable erased-style
recovery. The [anchor follow-up](assertion-to-nullable-variant-alternative.md#g2-anchor-resolver-retains-the-original-style-and-layout-buffers)
records the source failure/write contract. Header availability after numeric-guard annotation
and exact external binding declaration does not implement either missing body. No native
container enumeration, resolver registration or anchor layout execution is claimed.
