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
returning the same `flight::Task<…>` carrier the module-level `async function` already gets for that
declared result, and the async callback handed to a promise combinator becomes that same `Task`-returning
callable. The statement-level lowering exists; nothing about these bodies differs from it.

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

## Independent blocker on the same sites

Measured on a reproduction of `attachTrayEvent`'s shape, at the pin with every carried patch:
- **With the async method** (and a `catch` that does not read its binding): refused with this request's message.
- **Without any async closure,** `catch (error) { return { error, outcome: 'subscription-failed' }; }` is still refused
  (`contextual optionalSingle construction requires expression type evidence`). The release body has the same
  `{ error, outcome: 'release-failed' }`. That is
  [caught-value-into-unknown-member.md](caught-value-into-unknown-member.md), a runtime `flight::Any` capability.

So lowering async closures alone does not let `attachTrayEvent` emit; it also needs that capability.

## Carrying workaround

None. `@flighthq/tray` is `partial` with no header compiling; this is one of the refusals inventoried
read-only for it, and no source patch rewrites the closures.

## Carried compiler patch

[`compiler-async-closure-coroutine.patch`](../../source-patches/compiler-async-closure-coroutine.patch) with the runtime
`flight::retain_closure` (`include/flight/task.hpp`).

A coroutine lambda's frame does not copy the lambda's captures. It reads them through the closure object, so
dropping the closure while the coroutine is suspended leaves the frame reading released memory. That happens when
`then` drops its callback, or when an object's async method is still suspended after the object is gone. The
closure is therefore held in a shared owner, created where the closure is created. Each call goes through
`retain_closure`, which keeps that owner alive for the life of the call's coroutine frame and returns the same task,
with no added scheduling step:

```cpp
[async_closure = std::make_shared<const std::function<flight::Task<void>()>>([=]() -> flight::Task<void> {
  log.push(flight::String("before"));
  co_await tail;
  log.push(flight::String("after"));
  co_return;
})]() -> flight::Task<void> { return flight::retain_closure((*async_closure)(), async_closure); }
```

**Scope.** Lowered:
- async arrows and function expressions, expression-bodied or block-bodied;
- async object-literal methods.

Still refused, because they have no single callable type to hold:
- generic async closures;
- rest or pack parameters;
- `this`.

The catch representation and thrown-value identity are untouched.

**Verification.**
- `cppAsyncClosure.test.ts`: 7 fixtures.
- One upstream test that pinned the refusal now asserts the lowering.
- The backend suite has no new failures.
- `tests/async_closure_test.cpp` runs the emitted code verbatim and checks:
  - ordering through `then`;
  - a shared binding across calls;
  - an expression arrow;
  - a closure dropped while suspended keeps its capture alive and releases it after settlement;
  - an object method outlives its object.

  A mutant without `retain_closure` fails the lifetime check.

**Measured on `@flighthq/tray`** (scratch, not committed):
- `queueAnimationWrite` now emits and adds no compile errors;
- `attachTrayEvent` stops next at the caught-value refusal above;
- `startTrayIconAnimation` stops first at its own contextual-union refusal.

**Self-reference boundary.** `attachTrayEvent`'s release literal refers to itself
(`const release = { async release() { … release.released … } }`). Sync closures already capture `release` by value
inside its own initializer, before it is assigned. That is pre-existing capture behaviour and is not changed here,
so the shape is not claimed to work.
