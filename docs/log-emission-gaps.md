# `@flighthq/log` emission gaps

Measured at Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`2b687eaf6b631e0fa4d7d6e18dad93b845f99633`, with every active source patch and compiler patch in
`source-patches/manifest.json` applied.

## Refusals

Package-scoped generation of `packages/log/src/log.ts` refuses at exactly one expression:
`console[method](prefix, data)` in `_writeConsoleCaptureEntry`, which passes an object argument to a
variadic console method. That is the open request in
[console-variadic-arguments.md](compiler-requests/console-variadic-arguments.md). No formatting
contract for console object data is supplied here, by design.

A diagnostic run with that argument removed (never committed) emits the whole module with zero
refusals. So at the refusal level, the console call is the module's only open item.

## Emitted C++ that does not compile

That diagnostic `log.hpp`, compiled without the hand-written `flight/log/log.hpp` override, fails
with 42 clang errors in 20 functions. None of them involve console. They reduce to these defects:

| # | Functions | Defect |
| - | --------- | ------ |
| 1 | `serialize_log_error` | Negation precedence is lost around a comparison: `!value.error_if() != nullptr`. |
| 2 | `serialize_log_error`, `operation_failure_message` | `String(unknown)` lowers to `flight::to_string(Any)`, which has no overload. |
| 3 | `serialize_log_error` | A record element write is lowered as `result.get(k).value() = v`. |
| 4 | `exit_log_span` | `span_stack.index_of(span)`: the array holds rows while the argument is a `Ref`. |
| 5 | `destroy_file_log_sink_state` | The catch binding is referenced as `error_2` but never declared. |
| 6 | `apply_serializers`, `redact_path` | TypeScript `in` is emitted verbatim as C++ text. |
| 7 | `apply_serializers`, `redact_path` | `named_properties(...)` does not convert to `Any`, and the ternary operand types disagree. |
| 8 | `merge_context_fields`, `merge_span_fields` | An object literal assigned into `LogData` (a variant) is emitted as a designated initializer. |
| 9 | `create_json_log_formatter` | A braced initializer list appears on the right of `?:`, and a designated initializer is passed to `Json::stringify`. |
| 10 | `create_file_log_sink`, `create_buffered_log_sink` | The closure references `handle`, the const it is initializing, as a WeakMap key; the name is undeclared. |
| 11 | `end_log_timer` | The `log_debug(LogData, channel)` overload does not match. |
| 12 | buffered, child-context, context, span, memory, and rate-limited constructors (7 sites) | `StructuralRef<RowMerge<..., Entity>>` from a spread merge with an entity does not convert to the declared return type. |
| 13 | `enable_log_signals` | `optional<Ref<LogSignals>>` construction, the Signal handler type, and an initializer list in `?:`. |
| 14 | module state | `Record<LogLevel, String>`: an enum key is not a `PropertyKey` domain. |
| 15 | sink registry | `Array<LogSink>::index_of` selects a deleted `==` on `std::function`. The `types-log-sink-reference-parameter` override addresses this. |

The `log-module-qualification-and-operators` override makes the package compile. It does so with 12
throwing stubs:

- `dispose_log_sink`
- `destroy_file_log_sink`
- `apply_serializers`
- `redact_path`
- `begin_log_group`
- `end_log_group`
- `create_file_log_sink`
- `create_console_capture_sink`
- `create_console_log_sink`
- `merge_span_fields`
- `initialize_log_entity`
- `create_json_log_formatter`

`@flighthq/log` is therefore partial for these defects as well as the console call. Upstream
parity tests cannot pass for the stubbed functions.
