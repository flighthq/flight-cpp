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

## Second measured site: `@flighthq/useragent`

Same lowering, same erased receiver, and here it is the first of two errors on one expression. `packages/useragent/src/userAgent.ts` `parseUserAgentRuntime`:

```ts
const proc = win.process as Record<string, unknown> | undefined;
```

```cpp
std::optional<flight::Record<flight::String, flight::Any>> proc =
    static_cast<std::optional<flight::Record<flight::String, flight::Any>>>(
        window.get(flight::String("process")).value());
```

```
generated/include/flight/useragent/user_agent.hpp:172:69: error: no matching conversion for static_cast
  from 'flight::Any' to 'std::optional<flight::Record<flight::String, flight::Any>>'
```

Two things about this site are worth recording against the request:

- the value's own read is unguarded as well: `Record::get` returns `std::optional<Any>`, and the emitted
  `.value()` throws for a key the object does not have, where the source's read yields `undefined`. Both
  halves are the same defect family -- a read whose absence the source defines and the emission does not.
- the checked path this request's *Expected behavior* names exists, but is not spelled `flight::any_to`: the
  runtime's `any_to` lives in `flight::detail` (`include/flight/any.hpp:632`), and for a `Record` carrier the
  recovery the emitted `Any` values actually offer is `Any::external_if<Record<String, Any>>()`, which is what
  the carrying override uses. A fix that reaches for `flight::any_to<Record<...>>` should either publish it or
  use the member.

The module is emitted with **no** PARTIAL banner: the header reads as complete while its own body cannot
compile, and only the package's override keeps it out of the compile gate's way.

## Carrying workaround: `@flighthq/tween`

The tween overrides (`tween-internal-erased-numeric-record` and the three modules that write the target)
replace each `static_cast<flight::Record<...>>` with the keyed view the assertion stands for: an object's
own members through `flight::named_properties`, or a `Record<String, number>` the `Any` carries as an
external, recovered with `external_if`. Both write the caller's object rather than a copy.
