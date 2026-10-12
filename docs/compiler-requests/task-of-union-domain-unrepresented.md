# An async function returning an outcome union needs a represented `Task` domain

`detachMidiSubscription` is refused with "contextual union value type
`flight::Task<flight::types::MidiSubscriptionDetachOutcome>` is not a represented runtime domain". The
union itself is fine — its alias is emitted, with two distinct arms — and a sibling function in the same
module that returns the *attach* outcome emits. So the missing piece is not the union but the carrier for
an async function whose value is that union.

Measured at flight-compiler `2b687ea` / flight `a627849` against emitted `@flighthq/midi`.

## Observed

`generated/include/flight/midi/midi_subscription.hpp:170`, `cpp-contextual-union-value-type-unrepresented
[compiler-restriction]`:

```
cpp emission failed for @flighthq/types/packages/types/src/Midi.ts: contextual union value type
flight::Task<flight::types::MidiSubscriptionDetachOutcome> is not a represented runtime domain
```

The source it stood for, `packages/midi/src/midiSubscription.ts:217`:

```ts
async function detachMidiSubscription(subscription: Entity): Promise<MidiSubscriptionDetachOutcome> {
  const runtime = subscriptionStates.get(subscription);
  if (runtime === undefined) return { reason: 'not-attached' };
  runtime.generation++;
  let invalidatedAttach = false;
  if (runtime.attaching !== null) {
    const outcome = await runtime.attaching;
    if (outcome.reason === 'operation-failed' && outcome.releaseFailed) {
      return { reason: 'operation-failed', releaseFailed: true };
    }
    invalidatedAttach = outcome.reason === 'ok';
  }
  if (runtime.attachment === null) return { reason: invalidatedAttach ? 'ok' : 'not-attached' };
  if (!(await releaseTrackedMidiAttachment(subscription, runtime))) {
    return { reason: 'operation-failed', releaseFailed: true };
  }
  return { reason: 'ok' };
}
```

The union it returns is declared in `packages/types/src/Midi.ts:116` and **is** emitted
(`generated/include/flight/types/midi.hpp:220`):

```ts
export type MidiSubscriptionDetachOutcome =
  | { readonly reason: 'not-attached' | 'ok' }
  | { readonly reason: 'operation-failed'; readonly releaseFailed: true };
```

```cpp
using MidiSubscriptionDetachOutcome = std::variant<flight::Ref<reason_release_failed_df3bba2de4a03669>,
                                                   flight::Ref<reason_43a745d20647bfb6>>;
```

Two arms, structurally distinguishable, both carriers present. Its *sibling* in the same module —
`MidiSubscriptionAttachOutcome`, whose second arm carries `attachFailed` and `reason` — is returned by
functions that emit and compile, including the `Task`-returning `attach_failure()` call sites.

## What the reduction narrows, and what it does not confirm

Three things are true at once and only one of them is the cause; the measurements do not yet separate
them, so this request names them rather than picking:

1. **The `Task` carrier of a union.** `midiPermission` emits `Task<PermissionQueryOutcome>` for a
   three-arm named union in the same package and compiles, so `Task<union>` is not itself the gap.
2. **The single-member arm whose only field is a literal union.** `{ reason: 'not-attached' | 'ok' }`
   becomes one arm whose `reason` is a union of two literals, where the attach union's arms each carry a
   second field. If the emitter needs a discriminator to select the arm, this arm has none that is not
   the literal itself.
3. **The conditional literal in a return.** `{ reason: invalidatedAttach ? 'ok' : 'not-attached' }`
   computes the arm's literal from a runtime value — the only return in the function that does. Every
   other return spells its literal.

A reduction that separates them would return each arm through a spelled literal (removing 3), then narrow
through a discriminant (removing 2). That has not been run, because it can only be run against the pinned
compiler with a package fixture, and this repository does not modify a pinned checkout outside the
`source-patches/` mechanism.

## Where it fires

| site | source line | what is lost |
| --- | --- | --- |
| `detachMidiSubscription` | `midiSubscription.ts:217` | the one function every `detach*Subscription` export delegates to |

The module's four `detach*` and `dispose*` exports are one-line delegations to it
(`detach_midi_access_state_subscription`, `detach_midi_input_message_subscription`, and the dispose pair),
so this single refusal is the whole detach surface of `@flighthq/subscription`'s midi half.

## Distinguish from the requests already filed

[`object-literal-into-outcome-variant.md`](object-literal-into-outcome-variant.md) is the same module and
the *same construct* — object literals returned into an outcome union — but a different failure: there the
module emitted and the header did not compile, and **that is now fixed** (see the correction recorded in
that request). Here nothing is emitted at all, and the message names the `Task` carrier rather than the
initializer. If a compiler-side reader judges the two to share one fix, this request is the half that is
still live. [`async-closure-coroutine-lowering.md`](async-closure-coroutine-lowering.md) is about closures
spanning a suspension, not a coroutine's own return type.

## Carrying workaround

None. `@flighthq/midi` is `partial`; a hand supply of the detach chain would be a copy of the module and
the union carrier the profile should express.
