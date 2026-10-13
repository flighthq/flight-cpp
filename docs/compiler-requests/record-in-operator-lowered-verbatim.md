# `key in record` is emitted as C++ text

TypeScript's `in` operator on a `Record<string, T>` is copied into C++ as `key in record`, which is not
an expression. `flight::Record` already answers the same question as `record.has(key)`.

Measured at flight-compiler `2b687ea` with all 44 compiler source patches applied: 1 module emitted, 0
refused, and 2 compile errors.

## Minimal TypeScript (measured)

```ts
export function isNewKey(dict: Record<string, number>, key: string | null): boolean {
  return key !== null && !(key in dict);
}
```

## Observed

```cpp
inline bool is_new_key(flight::Record<flight::String, double> dict, std::optional<flight::String> key) {
  return (key.has_value() && !(key.value() in dict));   // invalid argument type 'flight::String' to unary expression
}
```

## Expected behavior

`k in r` tests own keys and the prototype chain. A `Record` written as `{}` is an ordinary object, so it also
answers `true` for `Object.prototype`'s names: `constructor`, `toString`, `valueOf`, `hasOwnProperty`,
`isPrototypeOf`, `propertyIsEnumerable`, `toLocaleString`, `__proto__`, `__defineGetter__`,
`__defineSetter__`, `__lookupGetter__` and `__lookupSetter__`. `flight::Record::has(k)` answers own keys only,
so the lowering is `r.has(k)` combined with a test against that fixed set of prototype names. It may be
`r.has(k)` alone only where the record provably has no prototype (`Object.create(null)`). A numeric key goes
through the canonical record key the runtime already uses for `get` and `set`.

## Affected modules

- `@flighthq/particles-formats` `starlingPexParse.ts`, in the plist reader (lines 276 and 284:
  `!(currentKey in dict)`).
- `@flighthq/log`: `_applySerializers` and `_redactPath` (row 6 of
  [log-native-compile-defects](log-native-compile-defects.md)), supplied there by an override.
