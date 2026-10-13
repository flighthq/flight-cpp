# A `typeof` exclusion is forgotten one `else` later

In an `if / else if / else` chain over `typeof`, the final `else` should see the union with every tested
alternative removed. The emitter nests each `else if` inside the previous `else`, and inside that nesting it
removes only the innermost test's alternative. A `boolean` excluded by the first test is therefore treated
as still possible in the last branch. The value is rebuilt as a `std::variant<bool, String>` where the source
has a `string`, and the call does not compile.

Measured at flight-compiler `2b687ea` with all 44 compiler source patches applied: 1 module emitted, 0
refused, and 1 compile error.

## Minimal TypeScript (measured)

```ts
function escape(s: string): string {
  return s.replace(/&/g, '&amp;');
}

export function describe(value: string | number | boolean): string {
  if (typeof value === 'boolean') {
    return value ? 'yes' : 'no';
  } else if (typeof value === 'number') {
    return `${value}`;
  } else {
    return escape(value);
  }
}
```

## Observed

```cpp
    else {
      return escape((value.index() == 0 ? std::variant<bool, flight::String>{std::in_place_type<bool>, std::get<0>(value)}
                                         : std::variant<bool, flight::String>{std::in_place_type<flight::String>, std::get<2>(value)}));
      // no matching function for call to 'escape'
    }
```

## Expected behavior

Narrowing must accumulate across the chain. In the final branch `value` is `string`, so the read is
`std::get<2>(value)`. The same applies at any depth of nested `else`.

## Affected modules

`@flighthq/particles-formats` `particleDesignerSerialize.ts`, in `documentToPlist`'s `kv` helper: `string`
values are escaped after the `boolean` and `number` branches.
