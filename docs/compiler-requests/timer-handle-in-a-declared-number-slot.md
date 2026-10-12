# A timer handle assigned to a source-declared `number` slot

A module that stores `setTimeout`/`setInterval` handles in variables it declares as `number` (the web
shape) emits a double binding cell, then tries to coerce the handle into it. Four sites in one header,
and the header cannot compile.

Measured at `2b687ea`, flight `a627849`. This is the **third position** of a family the repository has
already patched twice: [`return-type-of-bound-external-in-union.md`](return-type-of-bound-external-in-union.md)
covers a declared `ReturnType<typeof setInterval>`, and
[`bound-host-timer-local-storage-carrier.md`](bound-host-timer-local-storage-carrier.md) covers a local
initialized from the call. This one is a variable whose declared type is a *numeric token the source
chose*.

## Minimal TypeScript

`@flighthq/input`, `packages/input/src/inputManager.ts:356`:

```ts
let delayId = 0;
let intervalId = 0;
const stop = () => {
  clearTimeout(delayId);
  clearInterval(intervalId);
  delayId = 0;
  intervalId = 0;
};
const start = (callback: () => void) => {
  stop();
  callback();
  delayId = setTimeout(() => {
    callback();
    intervalId = setInterval(callback, options.interval) as unknown as number;
  }, options.delay) as unknown as number;
};
```

The `as unknown as number` asserts are the module saying "this handle is a number", because on the web it
is.

## Observed

`generated/include/flight/input/input_manager.hpp:244`, `:245`, `:252`, `:254`:

```cpp
const auto delay_id_capture = flight::make_binding_cell(double{0.0});
const auto interval_id_capture = flight::make_binding_cell(double{0.0});
std::function<void()> stop = [=]() {
  flight::host::clear_timeout(delay_id_capture.read_binding());
  flight::host::clear_interval(interval_id_capture.read_binding());
  …
};
…
delay_id_capture.rebind(static_cast<flight::Any>(flight::host::set_timeout(…)).as_number());
```

```
error: no viable conversion from 'double' to 'TimerHandle'        (×2)
error: no matching conversion for static_cast from 'TimerHandle' to 'flight::Any'   (×2)
```

The call side is already right — the binding gives `set_timeout`/`set_interval` their handle carriers —
and the destination is a `double` because the source said so. `static_cast<flight::Any>(TimerHandle)` is
not a conversion any runtime offers, and `as_number()` on it would invent a token even if it were.

## Expected behavior

The destination has to be able to hold the carrier the binding returns, which means the "unset" state has
to be representable rather than `0`: `std::optional<flight::TimerHandle>` (or the runtime's own absent
carrier), with `clear_timeout`/`clear_interval` called only when a handle is present. That is a lowering
decision the emitter can make from the binding's `callResultType`; whether the source's `0` sentinel is
kept as a *source* shape is a separate question, and a source patch that introduces the absent state is
not obviously more faithful than lowering it.

## Affected modules

`@flighthq/input` — `inputManager.ts`, and the header behind it. The shape (a handle in a numeric
variable) is a web habit, so other packages that assume DOM timer types plausibly hit it.

## Carrying workaround

`input-manager-refused-state-and-timer-boundaries` supplies the module, and its timer boundary is the
reason this request is filed rather than a re-derivation: the override's `start` invokes the callback once
and never schedules the delay or the interval, so the key-repeat behavior the source documents ("invoked
immediately on press, then again after `options.delay` ms, then every `options.interval` ms until
`stop()`") is absent, and its `stop` is inert. Its `remaining` said "None in input_manager.ts at this pin"
until this increment corrected it; see the correction in `overrides/manifest.json`.
