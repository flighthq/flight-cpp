# A binding initialized from a non-null-asserted collection get is present

`const x = map.get(key)!` binds a value TypeScript types as present. The emitter keeps the call's
optional storage for `x` and records no present-value proof, so the first member read is refused as
unproven.

## Minimal TypeScript

```ts
interface State { buf: number[] }
const states = new WeakMap<object, State>();
export function size(handle: object): number {
  const state = states.get(handle)!;
  return state.buf.length; // refused: no present-value proof
}
```

## Expected C++

Either bind the unwrapped value (`auto state = states.get(handle).value();`), or keep the optional and
treat the assertion as the proof for every read of the immutable binding (`state.value()->buf`). The
second matches how the emitter already lowers a narrowed-present access.

Observed at `2b687ea`, flight `a627849`:

```
property buf on C++ absence-carrying storage requires narrowed access: no present-value proof reaches this read
```

## Affected modules

`packages/log/src/log.ts` (`createBufferedLogSink`) and `packages/glyphatlas/src/explainGlyphAtlasEntry.ts`
(`runtime.entries.get(codepoint)!`). The same shape appears in `glyphAtlasEntry.ts` and
`font-formats/openTypeTestHelper.ts`.

## Carrying workaround

[`log-buffered-sink-state-explicit-guard.patch`](../../source-patches/log-buffered-sink-state-explicit-guard.patch)
replaces the assertion with an explicit guard that throws the same TypeError. The binding's storage
type is computed from the initializer's declared `V | undefined` rather than from the asserted type.
A first attempt at a compiler patch, which took the non-nullable evidence for a narrowed-present call
initializer, did not reach the binding's storage choice; the fix belongs where the binding type is
chosen.
