# A locally declared handler needs callable identity

A handler declared as a local arrow function, connected to a signal and later disconnected by a cleanup
closure, is emitted as a bare lambda assigned to `std::function`. Disconnect compares slots with
`flight::callable_identity_equal`, which cannot recover identity from an erased lambda and throws
`callable identity is unavailable after std::function erasure`. In JavaScript the cleanup removes the
same function object it added.

## Minimal TypeScript

```ts
export function connectCounting(source: Signal<(n: number) => void>): () => void {
  let total = 0;
  const handler = (n: number) => { total += n; };
  connectSignal(source, handler);
  return () => disconnectSignal(source, handler);
}
```

## Expected C++

A function value that can be compared by identity must keep it through erasure. The runtime already
carries that identity as `flight::Function`:

```cpp
std::function<void(double)> handler = flight::Function<void(double)>([=](double n) { /* ... */ });
connect_signal(source, handler, std::nullopt);
return [=]() { disconnect_signal(source, handler); };
```

## Affected modules

`connectSignalAtFrameRate` in `@flighthq/signals` (`throttle.ts`). Any emitted function value that is
compared by identity after `std::function` erasure has the same defect.

## Carrying workaround

Override [`signals-throttle-typed-argument-buffers`](../../overrides/manifest.json) declares every
handler as a `flight::Function`. The request closes when locally declared function values keep identity.

## Same family: a module-level function compared against a nullable registry slot (render, flight a627849)

`@flighthq/render` `enableColorAdjustmentGuards.ts` stores the module function `warnUnsupportedColorAdjustment`
into `RenderRegistries.colorAdjustmentUnsupportedGuard`. That slot is declared
`((state: RenderState, node: NodeAny) => void) | null` and is optional. The function then tests idempotence with
`runtime.registries.colorAdjustmentUnsupportedGuard === warnUnsupportedColorAdjustment`. At a627849 the emitter
generates the function, where the old pin refused it, and lowers that test to `==` between
`std::variant<std::function<...>, Null, Undefined>` and the function. This fails to compile with
`invalid operands to binary expression` (`flight/render/enable_color_adjustment_guards.hpp:62`).

The required shape is the one above. The stored value and the compared value must both be the same
`flight::Function` (one retained identity), so that `===` is `callable_identity_equal` over the present
alternative and is false for null and undefined. No workaround is carried. A std::function comparison or a
target-pointer comparison would invent an equality JavaScript does not have.

This one header blocks all four `@flighthq/debug` headers, because `debug.ts` imports
`enableColorAdjustmentGuards` through render's contract.
