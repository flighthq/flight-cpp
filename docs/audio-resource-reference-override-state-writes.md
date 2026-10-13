# The audio resource-reference override does not write its own state

`overrides/include/flight/audio/audio_resource_reference.hpp` supplies
`@flighthq/audio`'s `audioResourceReference.ts`. Every write it makes to a **state** or **failure**
member goes through `std::visit` with an `auto`-returning lambda:

```cpp
(std::visit([](const auto& value) { return value->state; }, ref) = flight::types::resource_resolution_state->unresolved);
(std::visit([](const auto& value) { return value->failure; }, ref) = std::nullopt);
```

`auto` return-type deduction drops the reference: `return value->state;` yields a **copy** of the
member, so the assignment lands on a temporary and is discarded. The member is never written. Reads
through the same shape are unaffected (they copy, which is what a read wants), which is why the
override looks right and passes its own headers.

## Measured

At flight `a627849`, driving the override through its public API:

```
before: state=Failed has_failure=1
reset returned: 1
after:  state=Failed has_failure=1
```

`reset_failed_audio_resource_reference` therefore returns `true` — it *detects* the failed state
correctly — while leaving the reference exactly as it was. Upstream expects the reference back at
`Unresolved` with no failure.

The same cause reaches `resolveAudioResourceReference`, which writes `Loading` before decoding and then
either `Resolved` or `Failed` (with a recorded `Unavailable` failure). None of those land:

- a successful resolution **returns** the resource and binds the decoded samples into the resource the
  reference already holds — that part works, because a copied `Ref` still points at the same object —
  but `state` stays `Unresolved`;
- an unreachable payload resolves to null **without throwing**, but `state` is never `Failed` and no
  failure is recorded;
- an external reference goes through the fetch seam and binds its samples, but again records no state.

So the reference's state machine and its failure reporting are inoperative, while every observable that
does not depend on them (construction, byte borrowing, name lookup, the detached explanation, the
resolve *result*) is correct.

## Status

Nine assertions in `tests/generated_audio_parity_test.cpp` are recorded BLOCKED against this: the file
asserts the resolve *results* and the `reset` return value, and names the missing state and failure
recording in place rather than asserting the broken values. Asserting them would certify the defect.

The override's manifest entry records it as `complete`, which is no longer true: it compiles and
supplies its module, but it does not supply the behavior it claims. `partial` — with
`remaining` naming the state and failure writes — is the honest status, and it would hold
`@flighthq/audio` below shippable, which is what a package whose reference state machine does not work
should be. Raising it is a repository decision, not this unit's to take silently.

## Not an emitter defect

Nothing here is a compiler or runtime gap: the emitted `std::visit` shape is the hand-written
override's own choice. A `decltype(auto)` return (or `return (value->state);` with an explicit
reference return type) would fix every site. That edit is proposed rather than made, because the two
prior override defects in this repository were recorded first and repaired second.
