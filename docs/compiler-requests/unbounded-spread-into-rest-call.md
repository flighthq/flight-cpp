# Spreading an unbounded array into a rest parameter must lower to a fold

`f(a, b, ...rest)` where `f` takes `...parts: Uint8Array[]` and `rest` is an array of unknown length. The
emitter refuses: "spreading an unbounded collection requires a fold or a variadic target", although the
target's rest parameter is itself an array.

## Minimal TypeScript

```ts
declare function join(...parts: Uint8Array[]): Uint8Array;
export function file(header: Uint8Array, tags: readonly Uint8Array[]): Uint8Array {
  return join(header, ...tags);
}
```

## Required C++ shape

The callee's rest parameter is a `flight::Array<Uint8Array>`; the call builds it by concatenating the
fixed arguments and the spread array, in order:

```cpp
flight::Array<flight::Uint8Array> parts{header};
for (const auto& tag : tags) parts.push(tag);
return join(parts);
```

## Affected modules

`swfTagStreamTestHelper.ts` in `@flighthq/swf` (a test helper): refused, not supplied.
