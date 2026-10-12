# `for...of` over a string needs a code-point iteration runtime contract

Iterating a string with `for...of` is refused, because the runtime contract exposes no Unicode
code-point iteration for `flight::String`:

```
cpp emission failed for .../textInputEditing.ts: string for-of requires a Unicode code-point iteration
runtime contract
```

This matters beyond one package because `for...of` over a string is the idiomatic way to walk a
string's characters, and TypeScript's semantics for it are code points, not UTF-16 units — a `for` loop
over indices would be a different program for anything outside the BMP.

Measured at flight `a62784923f`. Two packages are blocked by it directly:

- `@flighthq/textinput`'s `textInputEditing.ts` (one refusal)
- `@flighthq/bitmaptext`'s `updateBitmapText.ts` and `explainBitmapTextMissingGlyphs.ts` (two refusals)

## Minimal TypeScript

```ts
export function countCharacters(value: string): number {
  let count = 0;
  for (const character of value) count += character.length;
  return count;
}
```

The emitted form would have to iterate code points, not code units: a lone surrogate half in an
unpaired sequence is its own character, and a paired surrogate is one.

## Expected C++

Either a bound iteration protocol on `flight::String`:

```cpp
for (auto codepoint : value.code_points()) { ... }
```

or an index-based lowering the emitter owns, which advances by the code point's encoded length rather
than by one:

```cpp
for (std::size_t i = 0; i < value.length();) {
  const auto codepoint = value.code_point_at(i);
  i += utf16_length_of(codepoint);
  ...
}
```

Whichever is chosen, the iteration count and the values handed to the body must match TypeScript's for
the same string — measured in code points, in order, with no replacement of unpaired surrogates.

## Affected modules

The three named above. The remedy is a runtime capability on `flight::String` (or an emission rule),
not a source rewrite: rewriting each loop into an index loop by hand would silently change the program
for any string containing a surrogate pair, which is exactly the class of change this runtime exists to
prevent.

## Carrying workaround

None, and none should be carried: a hand-rewritten index loop is not equivalent, so it would be a
workaround that alters the semantics the package's tests are meant to certify.
