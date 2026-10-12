# A type assertion must not lower to an unchecked `static_cast`

A TypeScript assertion is a compile-time claim about a value the program already holds. It lowers to
`static_cast<T>(x)`, which is a value conversion — so when the source type is an erased carrier, the
assertion becomes either a hard compile error or, where some conversion happens to exist, an unchecked
reinterpretation the source never asked for.

## Minimal TypeScript

```ts
// packages/tween/src/internal.ts — the same shape appears at three sites in one module.
const propertyMap = tween.propertyMap as Record<string, number>;   // :61, :83
```

`tween.propertyMap` is `Readonly<NumericProps<T>>`, which the emitter erases to `flight::Any` (see
[the companion request](generic-mapped-type-erased-to-any.md)). The assertion says "this erased value
holds a numeric record"; what it means at runtime is a *checked* read, because the runtime cannot know
it is true.

## Expected behavior

Where the runtime can check the claim, the assertion lowers onto the checked path and the failure is
explicit: `flight::any_to<Record<String, double>>(value)` returns an `optional`, and an absent value is
the assertion being false. Where the runtime cannot check it and no conversion is defined, the
expression is outside the represented domain and belongs in a structured refusal, not in emitted code.

## Observed at `2b687ea` / `a627849`

After the `tween-record-membership` patch supplies the record view, the module still fails, and every
failing header fails on the same line:

```
flight/tween/_internal_internal.hpp:61:  no matching conversion for static_cast from
  'flight::types::NumericProps<T>' (aka 'flight::Any') to 'flight::Record<flight::String, double>'
```

with the emission it comes from:

```cpp
flight::Array<flight::String> keys = flight::object_keys(static_cast<flight::Record<flight::String, double>>(property_map));
```

The same cast shape is emitted on the read path (`_internal_internal.hpp:42` and `:94`) for the
unasserted `tween->property_map`, so it is the lowering and not the assertion's spelling.

`flight::Any` deliberately offers no conversion to an arbitrary `T` — the runtime's own rule is that
an erased value is read *checked* and never coerced (`include/flight/any.hpp:575-600`), because a
coerced read would hand a typed reader something that type never holds. `static_cast` is exactly the
unchecked operation that rule exists to refuse.

## Affected modules

`packages/tween/src/internal.ts` today; in general any `as` whose source type the emitter has erased.
It is independent of the erased *type*: representing `NumericProps<T>` would fix the declaration while
leaving `as` lowering unchecked, and this request would remain.
