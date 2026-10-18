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

A second instance is measured in `@flighthq/input`: `connectInputStateToInputManager` connects its ten handlers as
bare lambdas and returns a disposer that disconnects them, so **calling the disposer throws**. Because the identity
check inspects every stored slot and refuses on the first erased one, no slot on a manager's signal can be
disconnected once a state has been connected to it — an identity-bearing `flight::Function` slot disconnects
cleanly only while every other slot on that signal is also identity-bearing. That upstream case ("returns a
disposer that stops tracking") therefore cannot hold at the current pin; it is pinned as a divergence by
`tests/generated_input_state_test.cpp`.

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

### `@flighthq/scene2d-gl`, the same test inverted (measured 2026-10-17)

`scene2d-gl` `enableGlColorAdjustmentGuards.ts` stores `warnGlColorAdjustmentNotEnabled` into
`GlRenderRegistries.colorAdjustmentFeatureGuard`, declared `GlColorAdjustmentMaterialFeatureGuard | null`
with the guard taking `(state: GlRenderState, colorScaleBias: Readonly<ColorScaleBias | TintMaterialData |
readonly number[]>) => void`. The test is inverted -- `runtime.registries.colorAdjustmentFeatureGuard !==
warnGlColorAdjustmentNotEnabled` -- but the lowering is the same: `!=` between
`std::variant<std::function<void (Ref<GlRenderState>, variant<Array<double>, Ref<ColorScaleBias>,
Ref<TintMaterialData>>)>, Null, Undefined>` and `warn_gl_color_adjustment_not_enabled` (`void ()`), at
`flight/scene2d_gl/enable_gl_color_adjustment_guards.hpp:35`. The three failing headers are that one,
`scene2d_gl/contract.hpp` and `scene2d_gl/_internal_index.hpp`.

The identity semantics are the render site's: the module function has one JavaScript object identity, the
assignment adapts its zero parameters to the guard's two, and the comparison must be
`callable_identity_equal` over the present alternative, false for both `Null` and `Undefined`. It is not the
readonly-owner recovery family (render's `ColorScaleBias` return), and the
`assets-acquire-guard-callable-identity` respell that changes an asset guard's storage carrier to
`flight::Function` does not answer it: that is a type spelling, while this needs the stored adapter and the
compared function to be one retained identity. No workaround is carried.

## Second site: `@flighthq/log`'s LogSink -- identity is required for registry idempotence

`packages/log/src/log.ts` declares `LogSink` as a function type and keeps sinks in an array that is mutated by
identity:

```ts
export function addLogSink(sink: LogSink): void {
  if (_sinks.includes(sink)) return;   // reference equality
  _sinks.push(sink);
}
export function removeLogSink(sink: LogSink): boolean {
  const idx = _sinks.indexOf(sink);    // reference equality
  ...
}
```

The emitter spells the alias as `std::function` (`generated/include/flight/types/log.hpp` line 27) and, more
damagingly, spells the PARAMETERS of the functions that carry a sink as `std::function` too -- for example
`set_log_sink` takes
`std::optional<std::function<void(StructuralRef<RowReadonly<RowOf<Ref<LogEntry>>>>>)>`. A `flight::Function`
handed to a `std::function` parameter is copied into a new heap object, and storing it back into the sink array
constructs a SECOND `Function::State`. The identity is destroyed by that round trip, so registration stops being
idempotent and removal stops finding its target.

Measured, not argued: with the parameter respelled to `flight::types::LogSink`, the project's own assertion
`a generated handle or sink registry lost callable identity` turns from FAIL to PASS, with no other change. The
run-time symptom is exactly the erasure this request is about, one step removed -- it is not a comparison that
throws, it is an identity that is silently lost before the comparison happens.

The override `log-module-qualification-and-operators` therefore retains the REF spelling of `LogSink`
(`flight::Function<void(flight::Ref<LogEntry>)>`) against an emission that now consistently produces the ROW
spelling, and the two cannot be reconciled by re-spelling call sites. Deciding which one the runtime
standardises on is a representation choice with semantic consequences, so it was escalated rather than taken.
Note that `LogFormatter` is deliberately NOT respelled: formatters are never compared, so they need no identity,
and the override leaves them as `std::function`.

This request closes when emitted function values that are compared by identity keep it through erasure -- here,
when a `LogSink` parameter is emitted as a `flight::Function` rather than a `std::function`. Until then the
override has to carry the REF spelling, and the ROW spelling the emitter produces is a second site of the same
defect rather than a new one.

## G3 Shape registry carry: retained literals and strict nullable comparison

The original `registerDefaultShapeBoundsCommands` consumer reaches this defect after checked generic
`K` substitution. `registerShapeBoundsCommand` compares `previous?.fillBounds` with the new command's
nullable handler. The emitted variant/optional `==` fails to compile, and merely making those carriers
comparable would still lose each original default arrow function's identity.

[`compiler-callable-value-identity.patch`](../../source-patches/compiler-callable-value-identity.patch)
retains synchronous closed contextual literals in the existing `flight::Function` state before their
unchanged `std::function` ABI. Copies keep one function identity and its live captured owners; another
evaluation of the same literal creates another identity. Compatible closed callable `===` and `!==`
evaluate their operands once, left to right, into a callable/null/undefined union. Equal sentinel
alternatives compare equal; the callable alternative uses `callable_identity_equal`.

The existing runtime helper also distinguishes two represented categories: a declared function and
an identity-bearing closure are different source objects. It continues to reject an arbitrary erased
lambda whose identity was lost. No comparison of lambda target types or replacement owner is used.

The original emitted regression compiles, links and runs with same/fresh closures, an escaped live
captured owner, named functions, both strict operators, exact null/undefined states, a readonly optional
registry slot, and a left operand throwing before the right is read. Independently executing the same
TypeScript agrees. Omitting only identity retention still compiles, then fails with unavailable erased
identity. The 71-patch production build and ordered apply/revert pass; selected existing regressions
report 56 passes and one render-owner refusal reproduced on the preceding 70-patch baseline. The full
static check still reports existing peer fixture types, formatting, lint and import-order failures.

This is a bounded carry, not closure of every site above: generic or incompatible callable signatures,
async literals, and ABI-changing adapters retain their existing boundaries. Original Shape public
consumer and normal all-callback qualification remain pending; no header row is promoted by this proof.
