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

## Third pass: the module now compiles with ZERO errors -- and promotion is NOT narrow

Every non-console compile failure is repaired. The repaired header builds clean at `-ferror-limit=0`: **0
errors**, from 21 at the start of this pass. The only thing not supplied is `createConsoleCaptureSink`, which is
**omitted rather than stubbed** so the console object-argument refusal stands -- and it is the one function whose
body calls the emitter's single remaining omission, `_writeConsoleCaptureEntry`.

Repairs in this pass, all faithful to log.ts: `redactPath` (a one-level shallow copy built from
`NamedProperties` and recursed into -- `flight::Record` shares storage on copy, so the local handle and the one
stored in the object are the same object, which is what the source's `obj[key] = {...next}` then recursion
means); `applySerializers` (`'__kind' in value` over a named-properties view); both `Json::stringify` sites (a
designated initializer cannot be deduced through the template, and the envelope is now built explicitly, with an
absent channel passed as `undefined` so stringify OMITS it, matching `JSON.stringify`); the `log_debug` call
shape; the three anonymous carriers the emitter invented (`sink_65ee…`, `channel_fields_…`,
`on_log_entry_on_log_error_…`) replaced by the DECLARED named types (`BufferedLogSink`, `LogContext`,
`LogSignals`) -- each is exactly the named type minus `entity_runtime_key`, which is what `initialize_log_entity`
exists to set -- and their now-superseded definitions deleted; and the `Record<LogLevel, String>` rekey to
`Record<double, String>` with all twelve keys and five lookups cast at the boundary, exactly as the previous
override's `change` field describes.

### WHY IT IS NOT PROMOTED: the residual failures, exactly

Promotion does not stay narrow, because the LogSink representation it settles on does not keep the SDK green:

* `tests/generated_log_test.cpp` **FAILS AT RUN TIME**:
  `a generated handle or sink registry lost callable identity` -- so the identity property the types override
  exists to preserve is not preserved by this shape.
* `tests/generated_log_levels_and_sinks_test.cpp` **FAILS TO BUILD**: six errors, at lines 103/109/115
  (`no viable conversion from 'Ref<std::variant<Record<...>>>'`) and 153/154/169
  (`no viable conversion from a lambda to LogSink`).

Both were measured against the repaired override, not inferred. The seven LogSink lambda parameters and one
static_assert in the tests were converted to the row spelling to get as far as this, and the conversion is not
sufficient: the identity failure is a RUNTIME semantic difference, not a spelling one, so it cannot be fixed by
re-spelling call sites.

That is the whole reason the override exists, and it means the promotion decision is not mine to take by
choosing a spelling: the module can be made to COMPILE with the emitter's row spelling, but the project's own
tests pin callable identity through the REF spelling, and the two disagree. Recorded here rather than resolved
by weakening either side.

## Fourth pass: the Ref-preserving adaptation satisfies the identity assertion

Manager approved the Ref-preserving bounded adaptation and REJECTED row-spelling standardization. This pass
implements it, within the Log override and Log tests only. Analysis first, because the framing in my third pass
was wrong in a way that mattered:

### The identity loss was NOT the row spelling -- it was an emitted `std::function`

I had concluded the row spelling loses callable identity. It does not. `flight::Function::identity()` is
`state_.get()`, independent of the signature. The actual cause, found by experiment:

`set_log_sink` is EMITTED with a parameter of type
`std::optional<std::function<void(StructuralRef<RowReadonly<RowOf<Ref<LogEntry>>>>>)>` -- a **`std::function`**,
not `flight::types::LogSink`. A `flight::Function` handed to a `std::function` parameter is copied into a NEW
heap object, and the `std::function` is then stored back into an `Array<LogSink>`, constructing a SECOND
`Function::State`. The identity is destroyed by that round trip, not by any spelling choice.

Evidence: with the row-spelled alias, the repaired module, the converted tests AND that one parameter respelled
to `std::optional<flight::types::LogSink>`, `tests/generated_log_test.cpp` PASSES as written, including
"a generated handle or sink registry lost callable identity".

### What the assertion protects

A sink stored through a generated entity member (`MemoryLogSink::sink`), then registered, still compares
IDENTICAL to the original. That is the property `addLogSink`'s idempotence and `removeLogSink`'s targeted
removal depend on: registration twice must not stack duplicates, and removal must erase exactly the registered
object. JavaScript gives this for free through function reference equality; C++ needs a carrier that preserves
it across copies, which is why the committed override respells `LogSink` as `flight::Function` and leaves
`LogFormatter` as `std::function` -- formatters are never compared, so they need no identity.

### The bounded adaptation, measured

| change | count | scope |
| --- | --- | --- |
| sink lambdas retyped to `Ref<LogEntry>` | 9 | Log override |
| formatter lambdas LEFT row-spelled | 3 | Log override (correctly unchanged) |
| `sink(...)` / `target(...)` call sites respelled | 4 | Log override |
| row-local initialised from `entry` | 3 | Log override |
| redundant casts removed | 2 | Log override |
| `set_log_sink` parameter respelled | 1 | Log override |
| **tests, types override, any other package** | **0** | untouched |

The tests are unmodified and pass as written; the console refusal is preserved; `derivedFrom` re-derivation and
the manifest entry update are still outstanding. One finding worth carrying: I initially retyped the FORMATTER
lambdas too, and the committed override proves that is wrong -- identity is needed only for sinks.

### Remaining boundary: the log* family's data parameter (4 errors in the sinks test)

`tests/generated_log_levels_and_sinks_test.cpp` still fails to BUILD. The tests pass
`flight::makeRef<LogData>(...)`, while the emitted `log`, `log_debug`, `log_error`, `log_info`, `log_verbose`
and `log_warn` take a three-arm VALUE variant (`function<LogData()>` / `Record` / `String`); the committed
override used a two-arm Ref variant. Porting the 13 bodies is NOT mechanical, and the reason is the important
part: the committed override's `merge_span_fields` is a **`NotImplementedInThisProfile` variadic stub** that
accepts whatever it is handed and throws at conversion -- which is precisely why every log call was unobservable
before this unit. A REAL `merge_span_fields` takes a `LogData` value, so the ported bodies need a working
dereference of `Ref<LogData>` that this pass did not establish. Attempted, reverted, and recorded rather than
half-landed.

## Fifth pass: PROMOTED -- both Log suites pass, and two packages' parked assertions are restored

The faithful conversion turned out to be the source's own union, not a new carrier, so it is Log-local and no
compiler request was needed for it.

### The conversion, derived request-first

Tracing the domain first settled it. The source says `LogData = string | Record<string, unknown>` and
`LogDataProvider` is its thunk, so the public parameter is `LogData | LogDataProvider`. The emitter FLATTENS that
into a three-arm variant (`function<LogData()>` / `Record` / `String`), which is why a `LogData` value -- what
the tests pass, and what `Ref<LogData>` IS, since `LogData` is not `ReferenceEnabled` and `Ref<T>` therefore
resolves to `T` -- cannot convert into it.

The smallest faithful repair is therefore NOT a carrier change but a re-spelling: keep the union structured as
`std::variant<flight::types::LogDataProvider, flight::types::LogData>`, which is the source's own
`LogData | LogDataProvider`. A `LogData` converts into its second arm, a provider into its first, and the body
resolves exactly as the source does -- `typeof data === 'function' ? data() : data`:

```cpp
  flight::types::LogData resolved = (data.index() == 0 ? std::get<0>(data)() : std::get<1>(data));
```

No dereference is needed (my fourth pass wrongly assumed a pointer), no runtime or compiler-wide carrier change
is involved, and absence, error and object identity are preserved because a `LogData` is carried by value
through the union exactly as before. Merge precedence is untouched: `merge_span_fields` still takes a `LogData`
value and still applies data-wins-over-spans, msg-wins-in-the-string-arm, and no-fields-returns-input.

The one further spelling, in the same class: `createFilterLogSink`'s predicate takes the entry, so it is spelled
`std::function<bool(flight::Ref<LogEntry>)>` like the sink it filters, not with the emitted row spelling.

### Result

* `@flighthq/log` compiles 3/3 with **no override drift**; tier stays `partial`, as required.
* `tests/generated_log_test.cpp` and `tests/generated_log_levels_and_sinks_test.cpp` **PASS, tests unmodified**.
* The override is PROMOTED, and its manifest entry now carries the re-derived `derivedFrom`, the Ref-retention
  rationale, and `remaining` naming only the console refusal.
* `createConsoleCaptureSink` remains OMITTED, not stubbed -- the console object-argument path stays refused.

### The parked checks, now run

* **TextBidi**: the four `enableTextBidiGuards` cases were recorded BLOCKED because they assert on log entries
  and emitting one threw. They are now ported and pass -- including "warns once and names the kernel fixing
  call", which is direct evidence that the logging path runs end to end rather than merely compiling.
* **Media**: the guard pair's assertions were replaced by a boundary note for the same reason. Restored, and
  the two mutations that had survived with "no test able to distinguish them" (`enable` never installing the
  guard, `disable` never clearing it) are now CAUGHT.
