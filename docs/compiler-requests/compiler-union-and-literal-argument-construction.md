# An argument is passed without constructing its declared parameter type

Two call sites in the emitted `@flighthq/intl` headers hand an argument straight to a function whose
declared parameter needs a construction step the emitter does not perform. Both are emission defects
visible only by compiling the emitted header — the modules themselves compile far enough to be emitted,
and the failure is the call.

Found at flight `a62784923f` with `pkg:compile -- intl` (4 of 9 headers fail on these two shapes).

## Shape 1 — a union argument reaching an overload set

```cpp
// generated/include/flight/intl/datetime.hpp
inline flight::String format_date_value(std::variant<flight::Date, double> date, …) {
  …
  return formatter.format(date);           // error: no matching member function for call to 'format'
}
```

The runtime declares two overloads — `String format(const Date&)` and `String format(double)` — and the
emitter passes the `std::variant` directly. `std::visit` over the active alternative (or a
`holds_alternative` dispatch) is the construction the emitter owes; TypeScript picks the member by the
argument's runtime type, and `Date | number` is exactly that choice.

## Shape 2 — an object literal passed where `std::optional<Record>` is declared

```cpp
// generated/include/flight/intl/number.hpp
return get_number_format(locale, {.notation = flight::String("compact")}).format(value);
// note: cannot convert initializer list argument to 'std::optional<flight::IntlNumberFormatOptions>'
```

```ts
return getNumberFormat(locale, { notation: 'compact', ...options }).format(value);
```

A braced initializer is not a `std::optional<T>`, and clang will not deduce the optional's payload from
one. The emitter should materialize the literal into its declared destination type and then wrap it —
the same missing step the `font-formats-name-woff-diagnostic-details` source patch works around by
binding each literal to its declared parameter type before the call.

## Minimal TypeScript

```ts
declare function f(v: Date | number): string;
declare function g(o?: Readonly<{ notation?: string }>): string;
export const a = f(1);                       // picks the number member
export const b = g({ notation: 'compact' }); // literal into an optional parameter
```

## Expected C++

```cpp
return std::visit([](const auto& v) { return formatter.format(v); }, date);

// `merged` must preserve the full defaults-then-caller-options spread, including overrides.
return get_number_format(locale, std::optional<IntlNumberFormatOptions>{merged}).format(value);
```

## Affected modules

`@flighthq/intl`'s `datetime.ts` and `number.ts`, and through them `contract.hpp` and
`_internal_index.hpp`. The literal shape is not intl-specific: any emitted module that passes a spread
object literal into an optional-parameter call has it.

The P-2D4 `@flighthq/textureatlas` pass measured a third site in
`createTextureAtlasFromImageResource`. TypeScript selects the 2D `createTexture` overload from
`{ dimension: '2d', source: resource }`; the emitted C++ forwards that braced literal to the full
overload set, where it converts to none of the exact options owners or the options variant. The
owner-local override explicitly constructs the generated 2D options owner and calls that exact
overload.

## Carrying workaround

None for the general case. Shape 2 has a precedent workaround elsewhere in the repository — a source
patch that binds the literal to its declared type first — but neither was applied here, because both
modules are refused for its own reasons (see
[`compiler-intl-optional-and-union-carrier-evidence.md`](compiler-intl-optional-and-union-carrier-evidence.md)),
so fixing the call site would not yet make the module emit. They are recorded so the two are not
confused when intl is next picked up.

## G2 number declaration candidate stops before link qualification

At Flight `a627849` / compiler `806d257`, the actual retained `format_number` forwards its
`std::optional<flight::IntlNumberFormatOptions>` parameter unchanged. That and the source return type
establish the exact declaration candidate:

```cpp
flight::IntlNumberFormat get_number_format(
    flight::types::LocaleInput locale,
    std::optional<flight::IntlNumberFormatOptions> options);
```

A scratch application of the existing `declare-refused-function` mechanism matches exactly
`flight/intl/number.hpp`, preserves all five retained calls and reaches a fixed point. No body is added.
The real unused-header C++20 object checks run serially for `number.hpp`, `contract.hpp` and
`_internal_index.hpp`: **0/3 pass, 3 fail, 0 unattempted**. Each first fails at the retained compact
call (line 44 after the two-line declaration insertion):

```
no matching function for call to 'get_number_format'
candidate function not viable: cannot convert initializer list argument to
'std::optional<flight::IntlNumberFormatOptions>'
```

The currency, percent and unit calls have the same bare-literal shape by inspection; these are not
additional measured diagnostics. The declaration candidate is rejected before the unused-header
positive link or called-function exact negative link can be reached. No repair registry, generated C++,
manifest or source patch is carried, and no normal generation is needed to reproduce a rejected
scratch-only candidate. The production cluster `undeclared-identifier-1a056bbc` retains its original
three failures; the scratch diagnostic is not promoted into a new production cluster or a header pass.

Materializing the literal is necessary but insufficient: all four literal calls also omit their source
`...options`. The [existing spread request](object-spread-of-a-parameter-dropped.md#g2-intl-number-wrappers-drop-all-caller-option-spreads)
records that separate behavior gap. A non-optional overload, permissive runtime conversion or default-only
literal would conceal it. The helper's own
[typed-options refusal](compiler-intl-optional-and-union-carrier-evidence.md#g2-number-formatter-still-loses-the-typed-optional-options-boundary)
remains open independently.

## G2 anchor numeric guards need the declared argument carrier

At Flight `a627849` / compiler `806d257`, cluster call-has-no-matching-3b2f72dd
contains anchor_layout.hpp and its contract/_internal_index wrappers. The first recorded
failure is anchor_layout.hpp:76: six bottom/height/left/right/top/width cells have the exact
carrier std::variant<double, Null, Undefined>, but the emitted private
is_optional_number(Any) receives that outer variant directly. The runtime already represents
every active value, including distinct null/undefined and signed zero; it has no implicit
variant constructor for Any. The existing dynamic named-property any_from visitor does not
implicitly run at an ordinary function call. This is the argument-construction family above,
with an unknown parameter rather than an overloaded Date/number member.

A general fix must evaluate the argument once, dispatch on its active arm and construct the
declared unknown value without number coercion or loss of its sentinel. It must not project
the number arm before the guard or treat a variant container as a new opaque object. This
request does not justify a permissive global Any constructor for unrepresented owners.

The carried source patch layout-anchor-number-guard-carrier repeats the six fields' existing
number|null|undefined domain in ONLY the private helper parameter annotation. Its predicate
return, body, member reads and callers remain unchanged. The complete original/patched modules
emit byte-identical JavaScript, preserving malformed-field rejection, getter short circuits,
throws, finite/negative/negative-zero values, NaN/infinities and both absences. No runtime layout
or public style domain is narrowed. Normal exact-pin one-module generation emits the exact
variant parameter and guarded isfinite projection; all six callers construct their union
argument while preserving their source member read position.

Native checks use the actual emitted anonymous style owner: every numeric field accepts
undefined, null and finite positive/negative/zero/negative-zero/extreme/subnormal numbers,
rejects NaN and both infinities, and retains its active arm/value and original style identity.
All eight source alignments and invalid alignment strings are checked too. This proves the
represented guard domain, not recovery of arbitrary external style owners as this anonymous
owner or a layout algorithm. The [nullable-style request](assertion-to-nullable-variant-alternative.md#g2-anchor-resolver-retains-the-original-style-and-layout-buffers)
and [key-iteration request](layout-object-key-iteration-closed-keys.md#g2-anchor-container-is-an-open-object-at-the-current-pin)
retain those independent body obligations. The scoped original three headers pass after the
separate exact external-resolver declaration; explicit registration still cannot link.
