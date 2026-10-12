# `ReturnType<typeof boundExternal>` inside a union keeps the binding's result carrier

A host function bound with a `callResultType` (`setInterval` → `flight::host::TimerHandle`) is
correctly emitted as that carrier when its result is stored under a bare `ReturnType<typeof setInterval>`.
Inside a union, `ReturnType<typeof setInterval> | null` is planned from the lib declaration's `number`
instead, so the storage becomes `std::optional<double>`. The call result then has no destination slot.

## Minimal TypeScript

```ts
interface State { timer: ReturnType<typeof setInterval> | null }
export function start(state: State, tick: () => void): void {
  state.timer = setInterval(tick, 1000);
}
```

## Expected C++

`std::optional<flight::host::TimerHandle> timer;`, with the assignment storing the bound call's result.

Observed at `2b687ea`, flight `a627849`:

```
external call result type flight::host::TimerHandle is not one represented contextual runtime domain
```

The member is emitted as `std::optional<double> timer;`.

## Affected modules

11 sites in 7 packages: `log`, `signals`, `gui` (2), `tray`, `loader`, `render-wgpu`, and `host-web` (2,
a web package). `log.ts` (`createBufferedLogSink`) is refused on it after its other blockers are cleared.

## A second defect on the same call

Once the carrier is right, `setInterval(flush, intervalMs)` is refused with `missing required call
argument at position 2`. The call's optional-parameter plan comes from the library declaration
`setInterval(handler, timeout?, ...arguments)`, which counts the rest parameter as a declared position.
The callee, though, is a bound host function whose C++ target (`set_interval(callback, double delay_ms
= 0.0)`) has its own signature and defaults. A bound host call should pass the arguments the source wrote,
unpadded.

## Root cause

The semantic layer takes the checker's evidence for `ReturnType<…>` whenever a library supplies it, and
Flight's tsconfig has no Node typings. So `ReturnType<typeof setInterval>` arrives at the backend as the
DOM's `number`, the numeric token upstream's own timer contract forbids. In a program without a library,
the authored utility survives and the backend already maps it through the binding.

## Carrying workaround

[`compiler-host-timer-return-type-handle.patch`](../../source-patches/compiler-host-timer-return-type-handle.patch)
keeps the authored `ReturnType<typeof setInterval|setTimeout>` when the operand is the ambient host
function, and passes bound host calls their provided arguments unpadded. Every other `ReturnType<…>`
keeps its checker evidence; a fixture pins `ReturnType<typeof requestAnimationFrame>` to its numeric
domain. Without a timer binding the read is refused, never stored as a number. It expires when upstream
lowers ReturnType of a bound host timer to the binding carrier and stops padding bound host calls from
library signatures.

## Impact (read-only, at flight a627849)

Scoped generation with the patch, limited to the six files that store timers. Nothing was written to
`generated/`. None of the six is refused for the timer reason any more:

| file | result |
|---|---|
| `gui/scrollBarController.ts` | emitted; `std::optional<flight::host::TimerHandle> repeat_timer` |
| `gui/tooltipController.ts` | emitted; `std::optional<flight::host::TimerHandle> timer` |
| `tray/tray.ts` | timer stored as `std::optional<flight::host::TimerHandle> animation_timer`; module refused for an unrelated union-owner reason |
| `signals/throttle.ts` | refused for its pre-existing dependent-parameter-pack reason |
| `loader/resourceLoader.ts` | refused for catch-after-await hoisting |
| `render-wgpu/wgpuScreenCapture.ts` | refused for an unproven optional read |
| `log/log.ts` | `createBufferedLogSink` lowers; the module stops next at `createChildLogContext`'s `string \| null \| undefined` parameter ([nullable-optional-presence](nullable-optional-presence.md)) |
