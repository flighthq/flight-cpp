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

`k in r` over a `flight::Record` is `r.has(k)`. `Record::has` answers by own key, which is what `in` answers for
a plain record object: no prototype chain is involved for these carriers. A numeric key goes through the same
canonical record key the runtime already uses for `get` and `set`.

## Affected modules

- `@flighthq/particles-formats` `starlingPexParse.ts`, in the plist reader (lines 276 and 284:
  `!(currentKey in dict)`).
- `@flighthq/log`: `_applySerializers` and `_redactPath` (row 6 of
  [log-native-compile-defects](log-native-compile-defects.md)), supplied there by an override.
