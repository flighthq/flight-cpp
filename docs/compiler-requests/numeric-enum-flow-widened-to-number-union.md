# A numeric enum value is widened to a `number` union in flow

`@flighthq/debug` `debug.ts` is refused at a627849 in `enableDebug` (reported at its declaration, line 41):

```text
contextual C++ union conversion requires equivalent source union evidence: target multiVariant carriers
[double, flight::Any] from binary source singleValue carriers [flight::types::LogLevel]
```

`LogLevel` is a numeric `enum`. Every value that `enableDebug` handles is a `LogLevel`:
`options.level ?? LogLevel.Debug`, `getLogChannelLevel(channel)`, the saved levels restored through
`setLogLevel`/`setLogChannelLevel`. Yet the emitter builds a target union of `double` and `flight::Any`,
and cannot place the enum value in either alternative.

The underlying widening is visible in isolation. With the full patch stack:

```ts
export enum LogLevel { None = 0, Error = 1, Warn = 2, Info = 3, Debug = 4 }
export interface Options { level?: LogLevel }
export function f(options: Readonly<Options> = {}): LogLevel {
  const level = options.level ?? LogLevel.Debug;
  return level;
}
```

This stores `level` as `std::variant<LogLevel, double>`, although TypeScript types it `LogLevel`. The
enum's numeric nature is being treated as a union with `number`. In `enableDebug`, that widened value
meets a `[double, Any]` target, and the conversion has no evidence for the enum arm. The isolated repro
emits; the exact binary expression inside `enableDebug` that fails was not isolated (the refusal names
only the function).

## Expected

A numeric enum stays one carrier, the C++ `enum class`, through `??`, locals and calls typed `LogLevel`.
It widens to `double` only where the source itself widens it to `number`, which happens nowhere in this
function.

## Affected

`@flighthq/debug` `debug.ts`; `contract`, `debugTiming` and `index` cascade from it.

## Carrying workaround

None for `debug.ts`.
