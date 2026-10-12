# An assertion to a type parameter has no owner to cast to

A generic helper that returns one of its result union's arms builds the arm and asserts it to the type
parameter. The emitter materializes the arm as a reference owner named after the *generic*
(`flight::Ref<outcome_082700eee8e08c32<Result>>`) and then has to cast that to `Result` — a type
parameter, which is no declaration at all until instantiation, so there is no heritage path to follow.

Measured at `2b687ea`, flight `a627849`; the reduction below reproduces the message verbatim, minimised
from the real site.

## Minimal TypeScript

```ts
interface Tray { readonly id: string }

export function fail<Result extends { readonly outcome: string }>(tray: Tray): Result {
  void tray;
  return { outcome: 'tray-destroyed' } as unknown as Result;
}
```

```
a reference assertion from flight::Ref<outcome_082700eee8e08c32<Result>> to Result has no heritage to
cast along: the declarations share no emitted C++ class-heritage path (source interface and intersection
relationships flatten into independent owners), and the source carrier does not retain a checked dynamic
owner that can recover the target…
```

## The real site

`@flighthq/tray`, `packages/tray/src/tray.ts:423` — the helper every update and read in the module goes
through:

```ts
async function invokeUpdate<Provider, Result extends { readonly outcome: string }>(
  provider: Readonly<Provider>,
  tray: TrayIcon,
  failure: Result['outcome'],
  operation: (provider: Readonly<Provider>) => Promise<Result>,
): Promise<Result> {
  const runtime = getActiveTrayRuntime(tray);
  if (runtime === null) return { outcome: 'tray-destroyed' } as Result;
  try {
    return await operation(provider);
  } catch (error) {
    return { error, outcome: failure } as unknown as Result;
  }
}
```

There the emitted from-type is `flight::Ref<error_outcome_a8c907e8a016a45c<Provider, Result>>`: the arm
is a generic owner and the destination is the parameter it is an instance of.

## Expected behavior

The value being asserted is exactly the shape the parameter's constraint declares — `{ outcome: string }`,
optionally with an `error` — and it is built at that point in the source, not recovered from weaker
storage. So the emission has to name the *instantiated* arm owner: the assertion is to the parameter, and
a generic C++ function's `Result` is a real type at instantiation time. Where the emitter needs a
declaration to hold that owner, the constraint supplies it (`{ readonly outcome: string }`), and the arm
the source built is the one it should be materialized as.

## Distinguish from the existing widening requests

There are three requests in this directory about owners without heritage, and this is not one of them:

- [`compiler-interface-heritage-owner-widening.md`](compiler-interface-heritage-owner-widening.md) — a value
  whose *type* is a sub-interface cannot be stored in a slot typed as its super-interface, because both are
  declarations with no C++ base relation.
- [`flattened-interface-refinement-identity.md`](flattened-interface-refinement-identity.md) — the same
  problem for a refinement (`Texture2D` → `RenderTexture`), where materializing would replace identity.
- [`interface-heritage-widening.md`](interface-heritage-widening.md) — declared heritage, emitted as a
  declaration repair.

All three are about **two named declarations** and a missing base relation. Here there is only one
declaration (the arm) and a **type parameter** as the destination; nothing is being widened to a
supertype, and no identity is at stake — a fresh arm value is being typed as the parameter it was built
for. That is why it needs its own entry rather than a line in one of those.

## Affected modules

`@flighthq/tray` — `invokeUpdate` (and `invokeRead`, which forwards to it), i.e. every update and read
helper in the module behind it. No other package is known to use this shape today.

## Carrying workaround

None. The helper is generic over every request and response in the module, so there is nothing narrower
to rewrite it to; `@flighthq/tray` stays `partial`.
