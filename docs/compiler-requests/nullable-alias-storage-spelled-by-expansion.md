# Nullable storage of an imported alias is spelled by its expansion

```ts
let _installedSink: LogSink | null = null;
function _installDebugSink(sink: LogSink): void { _installedSink = sink; addLogSink(sink); }
```

`@flighthq/debug` `debug.hpp:86` (a627849) assigns through the alias's expansion, not its name:

```cpp
installed_sink = std::optional<std::function<void(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::LogEntry>>>>)>>{sink};
```

This compiles only while `flight::types::LogSink` is exactly that `std::function`. The
`types-log-sink-reference-parameter` override redefines the alias as
`flight::Function<void(flight::Ref<LogEntry>)>`, to give sinks callable identity and the reference
parameter every sink takes. The expansion then names a different type, and the conversion fails:

```text
no matching constructor for initialization of 'std::optional<std::function<void (flight::StructuralRef<...LogEntry...>)>>'
```

## Expected

Storage and conversions of an alias-typed value spell the alias, `std::optional<flight::types::LogSink>`,
as the `compiler-imported-alias-*` patches already do for parameters, locals and outer unions. A program
should not depend on an alias's expansion where it named the alias.

## Affected

`@flighthq/debug` `debug.hpp` `install_debug_sink`, in the committed partial emission of `debug.ts`.
In-tree it is hidden for now, because debug's headers first fail on `@flighthq/render`'s 7e2fc7d headers,
which include `flight/types/renderer.hpp`, gone since `types` moved. Compiled against the a627849 closure,
it is the one error in debug's own code.

## Carrying workaround

None yet. A `respell-reference-alias`-style emission repair could carry it once `debug.ts` emits.
