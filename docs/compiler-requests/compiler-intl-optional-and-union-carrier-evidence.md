# `@flighthq/intl` is refused for carried union and absence evidence

Seven of `@flighthq/intl`'s nine modules are refused at the Q0 pin, and they fail in three ways that
share one root: **evidence the TypeScript already has is not carried into the C++ boundary.** The
runtime can represent all of it — `std::optional<T>` for absence, a `std::variant` for the unions — so
this is a lowering gap, not a missing runtime type.

Measured at flight `a62784923f` on the SDL profile; `pkg:generate -- intl` reports 2/9 modules emitted
and 7 refused.

## The three shapes, with the emitter's own words

1. Union conversion where the target is a single optional carrier (4 modules — `collator.ts`, `list.ts`,
   `number.ts`, `relativeTime.ts`):

   ```
   contextual C++ union conversion requires equivalent source union evidence:
     target optionalSingle carriers [flight::ErasedRef]
     from identifier source optionalSingle ...
   ```

   The source passes an `options` parameter — already `Readonly<Intl.XOptions> | undefined` in
   TypeScript — into a call that the emitter has typed as `std::optional<flight::ErasedRef>`. The
   source's evidence says "this value is present-or-absent and its payload is the declared options
   record"; the target's carrier says "some erased reference". Lowering needs the source union's
   evidence to select the target alternative, exactly as the already-fixed `AudioContext` binding does.

2. A contextual union value whose payload type is not a represented domain (2 modules — `datetime.ts`,
   `plural.ts`):

   ```
   contextual union value type flight::IntlDateTimeFormatOptions is not a represented runtime domain
   contextual union value type flight::IntlPluralRulesOptions  is not a represented runtime domain
   ```

   `IntlDateTimeFormatOptions` and `IntlPluralRulesOptions` are declared all-optional interfaces. They
   are represented as C++ owner structs, but not as a *union domain*, so a value flowing into a
   contextual union position has nowhere to land. This is the same class as `Partial<TextFormat>`
   before the text-markup patch: a named all-optional interface needs to reach a carrier, and
   `std::optional<Struct>` is the natural one.

3. A presence test against `undefined` with no absence channel (1 module — `cache.ts`):

   ```
   a presence test against undefined has no absence channel in the emitted C++ storage for identifier …
   ```

   `Map<string, unknown>.get(key)` yields `unknown | undefined`, the source tests it for `undefined`,
   and the emitted storage has no way to say "absent" for an erased `unknown` payload.

## Minimal TypeScript

```ts
export function formatCompactNumber(
  value: number,
  locale: LocaleInput,
  options?: Readonly<Intl.NumberFormatOptions>,
): string {
  return getNumberFormat(locale, { notation: 'compact', ...options }).format(value);
}
```

## Expected C++

```cpp
// (1) the source's optional-with-payload evidence selects the optional carrier and its payload
auto merged = IntlNumberFormatOptions{.notation = flight::String("compact")};
return get_number_format(locale, std::optional<IntlNumberFormatOptions>{merged}).format(value);

// (2) an all-optional interface reaches its optional carrier the same way TextFormat does
// (3) absence for an erased payload is std::optional<flight::ErasedRef>, not a sentinel in unknown
```

## Affected modules

The seven refused modules above, and through them `contract.hpp`, `_internal_index.hpp`,
`datetime.hpp` and `number.hpp`, which do not compile independently while those modules are missing.

## Carrying workaround

One of the seven has a workaround: `cache.ts` is supplied by the `intl-formatter-cache` override, which
is the only reason `getCacheKey`/`getCached` exist at all in this profile. A target-neutral rewrite of
its eviction was tried first — iterating `keys()` and deleting the first, the same shape that cleared
`glyphatlas-entry-presence-proofs` — and it did **not** clear the refusal, which is the evidence that
shape (3) is about carried absence rather than the source's spelling. The other six have no workaround
and are carried as retained refusals.
