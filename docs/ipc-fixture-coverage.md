# @flighthq/ipc -- upstream fixture coverage

`@flighthq/ipc` is fully emitted at a627849: 3 of 3 modules, ZERO refusals, no override, every header
compiles. One upstream suite specifies it, `ipc.test.ts`, with 12 cases -- **all behavioural**, with no
"is exported" padding. All six exports are exercised.

`tests/generated_ipc_test.cpp` was written earlier in this session (49ed7c0d) and is RE-AUDITED here: the
prior prose and its green status were treated as unverified, and the audit found two defects in the test.

## Inventory

Brace-depth scanner (skips comments and string bodies so braces inside them cannot shift depth, attributes
each `it` to its enclosing describe, expands `it.each`). This fixture has no `it.each`; the scanner is used
because it is the one that is right when it matters.

| # | describe :: case | state |
| --- | --- | --- |
| 1 | `invokeIpc` :: requires the invoke capability and returns its response | reachable -- asserted in `tests/generated_ipc_test.cpp` |
| 2 | `onceIpcMessage` :: delivers the first message and then releases the subscription | reachable -- asserted in `tests/generated_ipc_test.cpp` |
| 3 | `onceIpcMessage` :: returns an unsubscribe that works before any message arrives | reachable -- asserted in `tests/generated_ipc_test.cpp` |
| 4 | `onceIpcMessage` :: releases exactly once across repeated stop calls | reachable -- asserted in `tests/generated_ipc_test.cpp` |
| 5 | `onceIpcMessage` :: releases even when the provider delivers during subscribe | starred -- the acquisition race |
| 6 | `onIpcInvoke` :: requires the handle capability, spreads arguments, and returns its release | reachable -- asserted in `tests/generated_ipc_test.cpp` |
| 7 | `onIpcMessage` :: delivers every message on its channel with the sent arguments | reachable -- asserted in `tests/generated_ipc_test.cpp` |
| 8 | `onIpcMessage` :: delivers only its own channel | reachable -- asserted in `tests/generated_ipc_test.cpp` |
| 9 | `onIpcMessage` :: unsubscribes exactly its own subscription | starred -- origin-pinned release |
| 10 | `onIpcMessage` :: routes each subscription to the host it was given | starred -- two hosts live at once |
| 11 | `sendIpcMessage` :: requires the send capability and forwards the arguments | reachable -- asserted in `tests/generated_ipc_test.cpp` |
| 12 | `sendIpcMessageTo` :: requires a capability for the target type and forwards that target unchanged | reachable -- asserted in `tests/generated_ipc_test.cpp` |

## Two defects the audit found, both in the TEST

**1. Every forwarding case checked only the FIRST argument.** `first_argument` was the only reader of an
argument list anywhere in the file, and every case passed one argument. Upstream forwards two or three
(`invokeIpc(host, 'compute', 1, 2)`, `sendIpcMessage(host, 'log', 'hello', 7)`) and compares the whole array.
So an implementation that forwarded the head and dropped the rest passed the entire file. All five
forwarding cases now pass several arguments and compare them with `arguments_of`, in order.

**2. The pass summary was printed BEFORE the last block.** A run that failed only the final (rogue-host)
block printed `generated ipc: all checks passed` and then its FAIL lines. That was MEASURED, not argued:
with the old placement restored, a mutation removing the rogue-host guard produced

```
generated ipc: all checks passed
FAIL: a delivery to an ALREADY-RELEASED subscription is not passed to the listener
FAIL: and neither is a third
```

Anything grepping for the pass string -- which is what this session's own verification does -- read a failing
run as green. The summary is now the last statement.

**That second defect was not unique to this file.** Sweeping every `tests/*_test.cpp` for the same shape
found it in two more of my own suites: `generated_filesystem_test.cpp`, where the summary sat **479 lines**
before the end -- every assertion in the second half of the file could fail while the run still announced
success -- and `generated_permissions_test.cpp` (63 lines). Both are corrected in a separate commit
(`91c161c3`), and the sweep now reports none. Those two packages had been certified this session on evidence
that could have read green while failing.

## Mutation battery

Eight, each verified to hit exactly one site before its result was read. All eight caught, zero survivors,
and the harness also asserts that **no failing run prints the pass summary**.

| # | mutation | caught by |
| --- | --- | --- |
| M1 | the post-subscribe acquisition-race guard is removed | "the subscription is released even though it arrived after the delivery" |
| M2 | the delivered-listener `done` guard is removed | "a delivery to an ALREADY-RELEASED subscription is not passed" + "neither is a third" |
| M3 | release is no longer idempotent | "stop is idempotent" |
| M4 | invokeIpc forwards an empty argument list | "the whole argument list, in order, not merely its first element" |
| M5 | sendIpcMessage drops the argument list | "sendIpcMessage forwards the channel and the whole argument list" |
| M6 | sendIpcMessageTo swaps the target and the channel | "sendIpcMessageTo forwards the target unchanged" |
| M7 | onIpcMessage subscribes on a fixed channel | "the other listener on the same channel still receives" |
| M8 | onIpcInvoke registers on a fixed channel | "onIpcInvoke registers on the channel it was given" |

M2 is the mutation that produced finding 2: it fails ONLY the last block, which is exactly what the old
summary placement hid.

## The case a compliant host cannot produce

The final block drives a host that KEEPS delivering to a listener after its unsubscribe has run. A mutation
removing the listener's `done` guard survives every other assertion in the file, because the fakes drop the
listener on release and the second delivery never reaches it. The upstream source guards that case
explicitly, so it is pinned against a deliberately non-compliant host rather than left to a host's good
manners.

## Not expressible here

Three cases open with `expectTypeOf(fn).parameter(0).toEqualTypeOf<...>()`, asserting the CAPABILITY TYPE
each function takes -- a compile-time claim in TypeScript with no runtime counterpart. On this surface the
capability is part of the emitted signature, so a caller passing the wrong one does not compile; the
behavioural half of each of those three cases is asserted.
