# An optional property call needs its callable's result evidence

`runtime.ownerSubscriptions?.delete(subscription)` is refused: the call goes through an optional
property, and the emitter will not emit a call whose result it has no evidence for.

Measured at `2b687ea`, flight `a627849`.

## Minimal TypeScript

`@flighthq/midi`, `packages/midi/src/midiSubscription.ts` — the field is declared at 138
(`ownerSubscriptions: Set<Entity> | null`) and assigned at 181; the call is at 280:

```ts
interface SubscriptionRuntime {
  ownerSubscriptions: Set<Entity> | null;
}

function untrackMidiSubscription(subscription: Subscription): void {
  const runtime = getSubscriptionRuntime(subscription);
  if (runtime === undefined) return;
  runtime.ownerSubscriptions?.delete(subscription);
}
```

```
cpp emission failed for @flighthq/midi/packages/midi/src/midiSubscription.ts: optional property call
delete requires callable result evidence
```

## Expected behavior

The optional chain is a presence test around an ordinary call whose result is discarded:

```cpp
if (runtime->owner_subscriptions.has_value()) runtime->owner_subscriptions.value().erase(subscription);
```

The callable is `Set<Entity>::delete`, a bound method on the value the chain proved present, so the
lowering has everything the source has: the receiver, the presence proof, and the callee's return type
(`boolean` here, discarded). The refusal reads as a demand for evidence that the chain itself already
supplies.

## Why this is worth a request rather than a source edit

The rewrite — hoisting `runtime.ownerSubscriptions` into a local and testing it — is a different
program shape that a reader has to check, and the same chain appears in other module shapes
(`map?.delete(k)`, `map?.get(k)` with a result). The chain's semantics are ordinary TypeScript with
defined narrowing, and one expression refusing a module is the case these requests exist for.

## Affected modules

`@flighthq/midi` — `midiSubscription.ts`, which refuses the whole module alongside
[`captured-referent-mutation.md`](captured-referent-mutation.md) in `midiPort.ts`. Its four refused
declarations are `untrackMidiSubscription` (this site), `detachMidiSubscription`,
`disposeMidiSubscription` and `performMidiAttach`.

## Carrying workaround

`midi-declare-refused-untrack-subscription` in `repairs/emission-repairs.json` declares the refused
`untrack_midi_subscription` from the two carriers the retained calls construct
(`flight::Ref<flight::types::Entity>`, `flight::Ref<MidiSubscriptionRuntime>`). It supplies no body, no
`Set` mutation and no null write, so any call still fails to link and the unsubscribe remains
unimplemented; the repair exists so the retained callers compile. Recompiled at `-j1`, the four midi
headers now fail on the next omitted callee, `detach_midi_subscription` — recorded in
[retained declaration calls omitted callee](retained-declaration-calls-omitted-callee.md). The refusal
itself stays open, and `@flighthq/midi` stays `partial` with its remaining refusals named.
