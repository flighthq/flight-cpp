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

auto options = IntlNumberFormatOptions{.notation = flight::String("compact")};
return get_number_format(locale, std::optional<IntlNumberFormatOptions>{options}).format(value);
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
