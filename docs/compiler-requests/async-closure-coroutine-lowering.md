# An async closure needs the coroutine lowering a module-level function gets

`async` inside an object literal or passed as a callback is refused, while the same `await` in a
module-level function lowers to a coroutine. The capability exists; the closure form does not reach it.

Both reproductions below were measured with the pinned compiler at `2b687ea`, flight `a627849`.

## Minimal TypeScript

```ts
interface Release { released: boolean; release(): Promise<string> }

export function make(): Release {
  const release: Release = {
    released: false,
    async release(): Promise<string> {
      return 'done';
    },
  };
  return release;
}
```

```ts
export function chain(tail: Promise<void>): Promise<void> {
  return tail.then(async () => {
    await tail;
  });
}
```

Both refuse with the same message:

```
async closures require C++ coroutine lowering
```

## Expected behavior

Each closure is a coroutine with exactly the body the source wrote: the async method becomes a callable
returning `flight::Task<std::optional<flight::String>>` (or the emitted carrier for its declared result),
and the async callback handed to a promise combinator becomes the same `Task`-returning callable. The
statement-level lowering the module-level `async function` already gets is what these need; nothing about
the body differs.

## Affected modules

`@flighthq/tray`, three functions and part of a fourth:

| site | shape |
| --- | --- |
| `attachTrayEvent` (299) | an `async release(): Promise<TrayReleaseResult>` inside the `TrayReleaseRuntime` literal it builds |
| `queueAnimationWrite` (383) | an `async () => { … }` passed to `Promise.prototype.then` |
| `startTrayIconAnimation` (371–379) | the same release-literal shape, behind that function's first refusal |

`@flighthq/tray` is the only package where this shape has been measured. `@flighthq/notification`'s
attach/dispose path was checked for it and does **not** contain an async closure — every `async` there is
a module-level function, which lowers — so this request has one package behind it, not two.

## Carrying workaround

None. `@flighthq/tray` is `partial` with no header compiling; this is one of the refusals inventoried
read-only for it, and no source patch rewrites the closures.
