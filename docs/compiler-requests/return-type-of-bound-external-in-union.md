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

## Carrying workaround

None. Moving the assignment onto the state object does not help, because the destination union is
planned the same way. This needs the union planner to consult the external binding's `callResultType`,
as `emitType` already does for a bare `ReturnType<…>`.
