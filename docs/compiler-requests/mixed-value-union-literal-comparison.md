# A strict literal test on a number/string member must inspect the active alternative

At Flight `a627849` / compiler `806d257`, `packages/layout/src/flexLayout.ts` validates an untrusted
`object | null` as `Partial<FlexLayoutItemStyle>`. The declared `basis` is optional `number | 'auto'`;
the assertion does not validate a runtime object's fields. Its source condition is:

```ts
(style.basis === undefined || style.basis === 'auto' || isNonNegativeNumber(style.basis))
```

The emitted anonymous style owner carries `basis` as
`std::optional<std::variant<double, flight::String>>`. In `is_flex_layout_item_style`, the first
presence guard is correct, but the next comparison uses raw variant `== String("auto")`, which has
no operator. The final `isNonNegativeNumber` argument is projected through `std::get<0>` even though
an invalid runtime string must be rejected as a value, rather than throwing `bad_variant_access`.

The literal test must read the active alternative without number/string coercion. The subsequent
unknown-valued numeric guard must still reject another string, null, booleans, objects, NaN and
infinities. Preserve each original member read in its short-circuit position; a getter can return a
different value or throw on the second or third read. A local snapshot or a new `typeof` member guard
changes those effects. The [nullable present-value equality request](nullish-variant-strict-equality.md)
is narrower: its other alternatives are only null/undefined, and it does not cover this numeric/string
choice or the validator's subsequent numeric projection.

## Equivalent source workaround

`source-patches/layout-flex-basis-unknown-comparison.patch` places each of the two present-value tests
in its own boolean helper over the field's existing `number | 'auto'` domain. The literal helper uses
`typeof value === 'string' && value === 'auto'`; the numeric helper repeats the original
`typeof value === 'number' && Number.isFinite(value) && value >= 0`. Both operate on a value already
read at its original member-expression position and introduce no type predicate or extra member read.
The annotations erase in JavaScript and do not change runtime acceptance or rejection. This avoids a
raw variant/string operator, numeric-only projection, and a new implicit variant-to-Any conversion.
No coercion, snapshot, field/owner copy, mutation, global equality overload or runtime Any widening is added.

Original and patched complete TypeScript modules agree in **30 validator cases**, including null,
absent/auto/finite nonnegative basis, negative zero, other strings and primitive types, NaN/infinities,
short circuits, changing getter values and exceptions on each of the three reads. This is source
validator evidence, not a native layout algorithm or nullable owner-recovery proof.

Normal exact-pin one-module generation reproduces the two helpers as explicit variant-index guards,
with no refusal added to the validator. The prior nullable-style refusals remain. Native checks against
the actual emitted anonymous style owner pass for absent basis, both valid alternatives, negative zero,
invalid strings, NaN/infinities, negative numbers, alignment and grow/shrink rejection, preserving the
original owner and stored values. Recovery of arbitrary external style owners is not proved by this
represented-carrier check. The original one-header serial gate is **1/1 pass** after the separate
resolver declaration repair; no full flex layout behavior or callable resolver is supplied.
