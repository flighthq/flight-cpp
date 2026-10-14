# @flighthq/log: the emitter has advanced far enough to obsolete most of the override

Measured 2026-10-13 at pin a627849, by running the package-scoped generation the log CORE override unit asks
for FIRST (`npm run pkg:generate -- @flighthq/log -j2`), before touching the override.

## What regeneration produced

The override `overrides/include/flight/log/log.hpp` was authored 2026-10-07 against an emission that had **23
omitted declarations**. Regenerating at the current pin produces a header with **ONE**:

```
missing: function _writeConsoleCaptureEntry -- source line 816
refusal: cpp-closed-key-external-member-arguments [compiler-restriction]
```

Every function the override's `remaining` field lists as absent is now emitted. Checked individually:
`merge_span_fields`, `initialize_log_entity`, `log_info_with`, `create_child_log_context`,
`create_text_log_formatter`, `serialize_log_error` and `create_buffered_log_sink` are all present, in a header
carrying **90** `inline` definitions. Neither `_merge_context_fields` nor `_writeConsoleCaptureEntry` is called
by any retained code, so the omitted-callee defect is NOT present in this module.

**The override's `remaining`, `reason`, `change` and `derivedFrom` are therefore all stale**, and so is the
second, easily-missed override beside it: `overrides/include/flight/types/log.hpp`, whose whole purpose was to
respell `LogSink` from the row spelling the emitter used to produce to the reference spelling it wanted. The
regenerated module now passes ROWS consistently, so that override actively BREAKS it -- five extra errors that
disappear the moment it is set aside.

## What still stops the re-derivation

The regenerated header does not compile: **20 errors in 12 functions**, and the count does not fall as they are
fixed -- fixing some unmasks others (9 functions on the first measurement, 12 after five were repaired). The
defect classes, all of them known emitter classes rather than new ones:

| class | site |
| --- | --- |
| TypeScript's `in` emitted as C++ text | `redact_path`, `apply_serializers` |
| a dropped spread merge | `mergeSpanFields`, `mergeContextFields` (both fixed) |
| designated initializer on a non-aggregate variant | the same two, plus `create_json_log_formatter` |
| `Ref` converts to bool, so `!= nullptr` has no operator | `serialize_log_error`, `operation_failure_message` |
| `flight::to_string(Any)` -- no such overload | the same two |
| an initializer list as one arm of a ternary | `create_json_log_formatter` |
| a catch binding named inconsistently (`error_2`) | `destroy_file_log_sink_state` |
| `NamedProperties` where `flight::Any` is wanted | `redact_path` |
| a captured `handle` that is never declared | `create_buffered_log_sink`, `create_rate_limited_log_sink` |
| `apply_serializers` / `redact_path` / `operation_failure_message` call shapes | `create_json_log_formatter`, `create_buffered_log_sink` |

Five functions (`merge_context_fields`, `merge_span_fields`, `operation_failure_message`, `exit_log_span`,
`serialize_log_error`) were repaired faithfully during this attempt -- merge order and precedence matched to the
source, including the `msg`-first ordering and the data-wins-on-collision rule -- and the repairs are recorded
in this note rather than committed, because a partially repaired override that does not compile cannot replace
one that does.

## State left behind

* `generated/**` retains the regeneration, so the next attempt starts from the emitter's current output rather
  than from a 2026-10-07 copy. `overrides:check` correctly reports the drift and names the next action:
  "Re-derive the override from the current generated file, or drop it if it is no longer needed."
* Both overrides are restored, so `@flighthq/log` still compiles 3/3 and the package is not regressed.
* `log.ts` stays refused on `console[method](prefix, data)`, as
  `docs/compiler-requests/console-variadic-arguments.md` records, and the omitted-callee request is retained.

## The honest size of the remaining unit

This is not a supply-a-few-functions job. Re-deriving the override means writing twelve functions against ten
emitter defect classes, each of which must preserve the source's merge order, absence semantics and entry
identity -- and each repair so far has unmasked another. It is bounded and mechanical, but it is larger than
one pass, and the count moving 9 -> 12 while five were repaired is the measurement that says so.

## Second pass: bounded progress, and the compiler's error cap

Approved by foreman as a deliberate multi-pass unit. Working from the regenerated current-pin state, nine
functions were repaired by hand; the live override is unchanged and the package is not regressed (probe: 0
errors with the committed overrides, `pkg:compile` 3/3, both log tests PASS). The repaired header is parked at
`docs/log-override-wip.hpp`, deliberately OUTSIDE `overrides/include/` so it cannot shadow anything, and it
does NOT compile yet.

### A measurement correction that matters for planning

An earlier note in this file said "20 errors" and that the count did not fall. Part of that was an artifact:
clang stops at 20 errors by default, so the number was a CAP and the 9 -> 12 function movement was measured
through it. With `-ferror-limit=0` the true count is **21**, and it now reads **17** after the repairs below.
Any future measurement here must pass `-ferror-limit=0`; the default view is misleading in exactly the
direction that matters.

### Repairs made (all faithful to log.ts)

| function | defect | repair |
| --- | --- | --- |
| `mergeContextFields`, `mergeSpanFields` | the spread merge was DROPPED entirely, and the string arm was a designated initializer on a non-aggregate variant | the merge is restored with the source's precedence: `{ msg: data, ...fields }` assigns fields AFTER msg so a field named `msg` wins; `{ ...fields, ...data }` assigns data AFTER fields so DATA wins. Absence is preserved: no fields means the input is returned unchanged. |
| `operationFailureMessage` | `error_if() != nullptr` -- `flight::Ref` converts to bool, so the comparison degraded to `bool != nullptr` | test the `Ref<Error>` directly; `String(value)` becomes `Any::as_string()` |
| `serializeLogError` | the same bool guard, plus `to_string(Any)` (no such overload) and an `Any& = Record` assignment | guard on the Ref; `Any::external(...)` where a Record is stored |
| `exitLogSpan` | `span_stack.index_of(span)` -- the array holds `Ref<LogSpan>`, the parameter is its row view | `structural_ref_cast<Ref<LogSpan>>` at the call |
| `createJsonLogFormatter` | an initializer list as one arm of a ternary | the two arms are built into a named Record first |
| `initializeLogEntity` | declared to RETURN `StructuralRef<RowMerge<...>>`, which no call site can accept | returns `Type` -- the same object it was given, which is what the source's `value as Type & Entity` means |
| `createBufferedLogSink`, `createFileLogSink` | `handle` used by closures declared BEFORE it. JS captures the BINDING; C++ needs the declaration first | `handle` hoisted and the closures capture it by reference (`[=, &handle]`), leaving every other capture by copy |
| `createChildLogContext` | `auto merged = {}` -- an empty braced initializer cannot deduce | typed as `Record<String, Any>` |

### What remains (17 errors, 9 functions)

`redactPath` (`in` operator, and `NamedProperties` where a Record is wanted), `applySerializers` (`in`), the two
`Json::stringify` call sites (a designated initializer where the template must deduce a Value), one `log_debug`
call shape, `optional<Ref<LogSignals>>` construction, the `defaultJsonFormatter` global's lambda, and
`_writeConsoleCaptureEntry` -- which IS called, from `createConsoleLogSink`, and is the one remaining emitter
omission. That last one is the console object-argument path the unit keeps refused pending a user decision, so
it is supplied or stubbed only with that decision.
