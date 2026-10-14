# `@flighthq/power` additive parity re-audit

Date: 2026-10-13

## Authority and boundary

Integration directly delivered the authoritative `69bb47a4` live target. Its
`tests/generated_power_test.cpp` is 701 lines with SHA-256
`2326c2a5bb59f7e44b85d0a1bcc0471ae88effe7e59bf73d44ef6e0de41a1251`.
That file was restored byte-for-byte before this audit began and frozen while
the first mutation pass ran.

The resulting test is additive relative to that snapshot: 149 inserted lines
(one `<chrono>` include and 148 lines of survivor-driven cases), zero removed
or replaced lines. The local branch predates the authoritative test additions,
so Git reports 277 insertions against the local parent; 128 of those insertions
are the byte-identical authoritative snapshot delta, not work selected from the
stale branch.

This audit changes tests and this ledger only. It does not change generated
headers, overrides, repairs, source patches, compiler/runtime code, pins, or
another package.

## Registered surface

The delivered registration files and repository inventory agree on exactly one
Power test source:

- CMake target `flight_cpp_generated_power_tests` builds
  `generated_power_test.cpp`, links `Flight::Cpp` and `Flight::SdkPreview`, and
  registers CTest `flight_cpp.generated_power`.
- `flight_cpp.generated_power` has the package label `power`.
- Bazel target `generated_power_test` builds the same sole source with
  `//:cpp` and `//:sdk_preview`, and is included in `generated_test_suite`.
- There is one `tests/*power*` source and no Power override.

The authoritative test header records three package modules, all three emitted,
none refused, and a 3/3 compiling slice at Flight revision
`a62784923f3be814463286c9fb28edfe6ab15789`.

## Pinned divergence inventory

The delivered source-patch manifest pins compiler patches to
`2b687eaf6b631e0fa4d7d6e18dad93b845f99633`. Two entries are directly owned by
`@flighthq/power`:

1. `compiler-readonly-view-argument-substitution` preserves type-parameter
   evidence through `Readonly<T>` so all three Power headers compile.
2. `compiler-host-timer-local-storage-carrier` stores the bound host interval
   result in the timer-handle carrier used by the idle poll loop.

Three compiler-wide entries are nevertheless part of the Power behavior being
certified, and their delivered patch files were inspected before gap mapping:

1. `compiler-nullish-assignment-statement-in-catch` retains the conditional
   `??=` record instead of dropping it.
2. `compiler-catch-recorded-slot-carrier` carries the first recorded failure as
   `std::exception_ptr`, tests its presence, and rethrows the original failure.
3. `compiler-catch-handler-breadth` emits reify-only handlers as `catch (...)`,
   allowing a non-class throw to reach the record/rethrow path while later
   teardown obligations still run.

The frozen test already distinguishes all three carried catch behaviors. Exact
controls that remove rethrow, overwrite the first failure, or narrow the
destroy handler were killed before any new assertion was added.

## Mechanical upstream map

The pinned upstream package contains one test file,
`packages/power/src/power.test.ts`. A TypeScript AST walk found:

- 19 `describe` suites;
- 31 ordinary `it` cases;
- no parameterized `.each` sites or rows; and
- 50 `expect` calls.

| Suite | Cases |
| --- | ---: |
| `acquirePowerKeepAwake` | 2 |
| `attachPower` | 5 |
| `createPower` | 1 |
| `destroyPowerKeepAwake` | 3 |
| `detachPower` | 3 |
| `disposePower` | 1 |
| `enablePowerSignals` | 1 |
| `getPowerBatteryHealth` | 1 |
| `getPowerIdlePollingIntervalMs` | 1 |
| `getPowerStatus` | 1 |
| `getPowerSystemIdleState` | 1 |
| `getPowerSystemIdleTime` | 1 |
| `getPowerThermalState` | 1 |
| `initializePower` | 1 |
| `isPowerKeepAwakeActive` | 1 |
| `makePowerBatteryHealth` | 2 |
| `makePowerStatus` | 3 |
| `releasePowerKeepAwake` | 1 |
| `setPowerIdlePollingIntervalMs` | 1 |

The authoritative C++ snapshot had 63 `check` call sites (64 textual
occurrences including the helper definition). It covered the upstream cases
and already added native evidence for fresh event payload identity,
attempt-all/first-failure semantics, primitive throws, direct signal clearing,
and registration erasure.

## Exact mutation measurement

Each probe was one exact unified diff against
`generated/include/flight/power/power.hpp`, compiled with its shadow directory
first in the include path. The test source stayed frozen for the first pass.
At most two probes compiled concurrently. All 34 probes compiled; there were no
invalid mutations.

The frozen snapshot killed 17 and allowed 17 to survive. The survivors were:

- signal completeness/idempotence:
  `enable_idle_signal_omitted`, `enable_resume_signal_omitted`,
  `enable_idle_signal_replaced`, and `enable_resume_signal_replaced`;
- idle forwarding/delivery: `idle_query_threshold_ignored`,
  `idle_attach_threshold_ignored`, `idle_poll_callback_disabled`, and
  `idle_change_emission_removed`;
- event streams: `suspend_subscription_omitted`,
  `resume_subscription_omitted`, `unlock_emission_removed`,
  `suspend_emission_removed`, and `resume_emission_removed`;
- disposal: `dispose_resume_clear_removed`,
  `dispose_unlock_clear_removed`, `dispose_thermal_clear_removed`, and
  `dispose_detach_removed`.

Four additive blocks answer exactly those survivors:

1. all nine signals must be allocated and retain identity across a second
   `enablePowerSignals` call;
2. query and attach thresholds are observed independently, and the native timer
   is pumped until a changed idle state reaches `onIdleStateChange`;
3. unlock, suspend, and resume subscriptions must exist and deliver to their
   matching signals; and
4. `disposePower` must release its provider registration and clear the resume,
   unlock, and thermal signals themselves.

The same 17 diffs were then replayed unchanged. All 17 were killed. The other
17 controls were already killed by unchanged authoritative assertions, and
additive checks cannot make an earlier failure pass, so the final measured
mutation result is 34/34 killed.

The final test has 74 `check` call sites (75 textual occurrences including the
helper definition), is 850 lines, and retains every authoritative assertion.
