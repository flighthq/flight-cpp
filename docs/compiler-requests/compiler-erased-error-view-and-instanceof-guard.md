# An erased Error read needs a checked projection, and its `instanceof` guard needs lowering

A property read on an erased value that the source narrowed with `instanceof Error` has two gaps, and
the emitter refuses rather than emitting either: the erased carrier has no Error members to read, and
the guard itself has no lowering — it was being emitted verbatim, as `cause instanceof flight::Error`,
which is not C++.

## Minimal TypeScript

```ts
export function message(cause: unknown): string | null {
  if (cause instanceof Error) return cause.message;
  return null;
}
```

## Expected behavior

The guard is a question about the object, so it lowers to the runtime's checked test:
`cause.error_if().has_value()` is true exactly when the value is an `Error` or a subclass of one, which
is what `instanceof Error` asks. Inside that branch the value is proven present, so the read lowers
through the same view — `cause.error_if().value()->message()` — and `.value()` is how this backend lowers
every other proven-present read: where the proof does not hold the throw is the answer rather than an
invented member. The view hands back the very object that was boxed, so identity and subclass names
survive; it is not reflection and it coerces nothing.

Observed at `2b687ea` / `a627849`, before this work:

```
reading Error.message after narrowing an erased value requires flight::Any to expose a checked Error view
  that accepts Error subclasses … rule: cpp-erased-error-view-runtime-required
```

and the guard, once the read was lowered, emitted `if ((cause instanceof flight::Error))` — a
TypeScript operator in a C++ header, which no compile gate can accept.

## Affected modules

`packages/log/src/log.ts` and `packages/audio/src/audioResourceReference.ts`.

## Carrying workaround

[`compiler-error-name-and-erased-error-reads.patch`](../../source-patches/compiler-error-name-and-erased-error-reads.patch)
lowers both the guard and the read. The runtime half landed in flight-cpp `48e4424`
(`Any::error_if()`, plus the reference-backed `Error` that makes the view hand back the object). It
expires when the emitter lowers an erased `instanceof` test and the read behind it itself — including
whichever shape upstream chooses for the absence channel, which here is the guard.
