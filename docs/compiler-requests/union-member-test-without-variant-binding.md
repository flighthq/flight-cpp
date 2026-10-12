# A union-member test needs the binding's carrier to still be a variant

`emitUnionMemberTestCpp` refuses when the binding a member test reads does not resolve to a union in the
emitter: the test has no variant to bind to. It fires at two sites across two packages, both of them the
*shared* helper of a family, so one refusal holds ten public functions.

Measured at `2b687ea` / `a627849` from the emitted placeholders — the source lines and messages below are
the emitter's own record for each refused declaration, not a paraphrase.

## The rule's condition

`packages/compiler-backend-cpp/src/cppCompilerBackend.ts:15216`:

```ts
function emitUnionMemberTestCpp(evidence, context) {
  const bindingType = getCppBindingTypeCpp(evidence.binding.id, context);
  const union = bindingType ? getIrUnionTypeCpp(bindingType, context, new Set()) : undefined;
  if (!union) emissionError(context, 'union member test requires a C++ variant binding');
  …
```

So the failure is not about the test's syntax: it is that the *binding's declared type* no longer carries
the union alias by the time the member test is emitted.

## Where it fires

| package | declaration (source line) | the test |
| --- | --- | --- |
| `@flighthq/tray` | `startTrayIconAnimation` (351) | `if (first.outcome !== 'updated') return first;` over `TrayImageUpdateResult`, where `first` came from `await queueAnimationWrite(…)` — and the destination is `TrayAnimationStartResult`, whose third arm is `Exclude<TrayImageUpdateResult, { outcome: 'updated' }>` |
| `@flighthq/notification` | `attachNotificationSubscription` (362) | the `outcome.reason === 'operation-failed'` tests on the awaited attach result |
| `@flighthq/notification` | `disposeNotificationSubscription` (433) | the same tests on the awaited detach result |

```
cpp emission failed for @flighthq/notification/packages/notification/src/notification.ts: union member
test requires a C++ variant binding
cpp emission failed for @flighthq/tray/packages/tray/src/tray.ts: union member test requires a C++
variant binding
```

## Impact, and why it is worth more than one function

In `@flighthq/notification` both refusals are the **shared helpers**, not leaves: the five
`attachNotification*Subscription` wrappers call the first and the five `disposeNotification*Subscription`
wrappers call the second. The hand-written `notification-subscription-lifecycle` override reproduces that
split — its declared `remaining` records that the lifecycle is unavailable — so this single rule is what
keeps ten public functions from being implemented. In `@flighthq/tray` it refuses
`startTrayIconAnimation` and, through the `Exclude<…>` arm, is entangled with the create-result chain.

## Reduction: what was tried, and where it stopped

Four reductions were run against the pinned compiler and **none reached this message** — recorded so the
next reader does not repeat them:

| reduction | result |
| --- | --- |
| discriminant test on a value-domain union, 3 arms, read of a second member | `property releaseFailed on a C++ variant requires proven union member access` |
| the same after an `await` | same as above |
| `if (!('releaseFailed' in outcome)) return;` | **emitted** |
| discriminant test on a `Ref`-returning capability result with two payload-bearing arms | `property releaseFailed on a C++ variant requires proven union member access` |

The reductions land on a *neighbouring* rule instead, which narrows the suspicion without confirming it:
an `in` test on a value-domain union emits cleanly, and the discriminant forms fail one rule later. What
the real sites share is that the tested binding is produced by a call — an `await`ed generic helper — and
the destination is a union alias that mentions the source alias through `Exclude<…>`. So the alias
identity is being lost between the call and the test, and a faithful reduction has to reproduce that
loss, not just the test.

## Distinguish from the requests already filed

[`reference-backed-outcome-variant-narrowing.md`](reference-backed-outcome-variant-narrowing.md) is about
*reading a member* on a reference-backed variant that the emitter treats as a record (the value side).
[`narrowed-member-multi-alternative-selection.md`](narrowed-member-multi-alternative-selection.md) is about
a member-presence narrow that matches several alternatives (the selection side). This one is the
**binding** side: the test cannot be emitted at all because the carrier is not a variant any more.

## Carrying workaround

None. `@flighthq/tray` stays `partial`; the notification override declares its gap rather than filling it.
