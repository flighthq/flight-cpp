# An object literal returning into an outcome union is emitted as a record initializer

The emitter builds a reference-backed outcome variant with a designated initializer list, which is not
valid C++ for a `std::variant`. The module emits, the header does not compile.

Measured at `2b687ea`, flight `a627849`; the reproduction below is exact, and the same construct is
already recorded against another package.

## Minimal TypeScript

`@flighthq/types`, `packages/types/src/Midi.ts:108`, and `@flighthq/midi`,
`packages/midi/src/midiSubscription.ts:260`:

```ts
export type MidiSubscriptionAttachOutcome =
  | { readonly reason: 'ok' }
  | { readonly attachFailed: boolean; readonly reason: 'operation-failed'; readonly releaseFailed: boolean };

// midiSubscription.ts
function attachFailure(): MidiSubscriptionAttachOutcome {
  return { attachFailed: true, reason: 'operation-failed', releaseFailed: false };
}
```

## Observed

`generated/include/flight/midi/midi_subscription.hpp:115` (and the same shape again at 299):

```cpp
inline flight::Ref<MidiSubscriptionAttachOutcome> attach_failure() {
  return flight::make_ref<MidiSubscriptionAttachOutcome>(MidiSubscriptionAttachOutcome{.attach_failed = true, .reason = flight::String("operation-failed"), .release_failed = false});
}
```

```
error: initialization of non-aggregate type 'MidiSubscriptionAttachOutcome'
       (aka 'std::variant<std::shared_ptr<flight::types::attach_failed_reason_release_failed_a670bac8ba62ef94>,
                          std::shared_ptr<flight::types::reason_43a745d20647bfb6>>')
       with a designated initializer list
```

Four of `@flighthq/midi`'s seven headers fail on this one line: `midi_subscription.hpp` itself, and
`contract.hpp`, `midi_access.hpp` and `_internal_index.hpp` through it.

## Expected behavior

The literal selects an alternative, the way the emitter already does when the target is not a
reference-backed union:

```cpp
return flight::make_ref<MidiSubscriptionAttachOutcome>(MidiSubscriptionAttachOutcome{
    std::in_place_index<0>,
    flight::make_ref<flight::types::attach_failed_reason_release_failed_a670bac8ba62ef94>(
        flight::types::attach_failed_reason_release_failed_a670bac8ba62ef94{
            .attach_failed = true, .reason = flight::String("operation-failed"), .release_failed = false})});
```

The alternative's own struct may keep its designated initializer — that part is an aggregate and is
correct. What must not happen is the initializer list landing on the variant.

## This is the mirror of a request already filed

[`reference-backed-outcome-variant-narrowing.md`](reference-backed-outcome-variant-narrowing.md) covers
the **read** side: a member read on a reference-backed outcome variant is emitted as a record access.
This is the **construction** side of the same misclassification, so a reader who fixes one should check
the other.

## Other packages with the same construct

`@flighthq/log` — [`log-native-compile-defects.md`](log-native-compile-defects.md) entries **8** and
**9** record the identical shape ("an object literal assigned into `LogData` (a variant) is emitted as a
designated initializer" and the same defect behind `Json::stringify`). That file is a package-scoped
inventory of log's instances; this request is the general one, and it now has two packages behind it.

## Affected modules

`@flighthq/midi` — `midiSubscription.ts` (two emitted sites) and the three headers downstream of it.
`@flighthq/log` at its own refusals.

## Carrying workaround

None for midi: the module is refused anyway, so a supply would have to answer this *and* the refusal
that names the module. No source patch is carried — the construct is an ordinary object literal whose
correct lowering the emitter already has.
