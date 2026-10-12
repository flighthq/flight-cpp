# A named union's arms must have one carrier, not a flattened copy at one use

`MidiPortResourceState = MidiInputPortResourceState | MidiOutputPortResourceState`, where both arms extend
a shared base interface. The emitter expands that union twice with **different arm carriers**: at one use
the arms are anonymous structural shapes named by their flattened members, at the other they are the
declared named interfaces. A value obtained from the union therefore has no checked conversion into the
declared union's carrier, and the two public entry points that pass it refuse.

Measured at flight-compiler `2b687ea` / flight `a627849` against emitted `@flighthq/midi`.

## Observed

`generated/include/flight/midi/midi_subscription.hpp:420` and `:451`, both
`cpp-contextual-union-inequivalent [target-runtime]`:

```
cpp emission failed for @flighthq/midi/packages/midi/src/midiSubscription.ts: contextual C++ union
conversion requires equivalent source union evidence: target optionalVariant carriers
[flight::Ref<kind_message_subscriptions_operations_dispose_completed_dispose_pending_disposed_flight_opened_state_subscriptions_7ec6bd6eb4b74aba>,
 flight::Ref<kind_operations_dispose_completed_dispose_pending_disposed_flight_opened_state_subscriptions_b6243e116f9628ed>]
from call source optionalVariant carriers [flight::Ref<MidiOutputPortResourceState>,
flight::Ref<MidiInputPortResourceState>]. The alternatives match uniquely, but no checked target-runtime
conversion exists between their C++ carriers. Keep both sides on the same declared union alias; for a Map or
WeakMap lookup followed by a rebind, construct its declared value owner and ensure every rebound member
already has that owner member's carrier -- a Readonly structural view does not become a mutable nominal Ref.
Otherwise add an identity-preserving runtime conversion contract.
```

The source, `packages/midi/src/midiSubscription.ts:39` and `:54`:

```ts
interface MidiPortResourceStateBase { disposeCompleted: boolean; /* … */ }
interface MidiInputPortResourceState extends MidiPortResourceStateBase { kind: 'input'; messageSubscriptions: Set<…>; operations: … }
interface MidiOutputPortResourceState extends MidiPortResourceStateBase { kind: 'output'; operations: … }
type MidiPortResourceState = MidiInputPortResourceState | MidiOutputPortResourceState;

const state = getMidiPortResourceState(input);      // a MidiPortResourceState
if (state === undefined || state.kind !== 'input' || state.disposed) return …;
return attachMidiSubscription(subscription, state.messageSubscriptions, …, …);
```

`midiResource.ts` emits the declared interfaces *and* a `using MidiPortResourceState = std::variant<…>`
built from them; the union consumed at the subscription site is the flattened anonymous pair instead. Two
carriers for one declared union, so the alternatives "match uniquely" and cannot convert.

## Where it fires

| site | source line | what is lost |
| --- | --- | --- |
| `attachMidiInputMessageSubscription` | `midiSubscription.ts:39` | the public attach entry point for input messages |
| `attachMidiPortStateSubscription` | `midiSubscription.ts:54` | the public attach entry point for port-state changes |

Both are *exported* entry points, and the module's two other omissions are filed separately
([`optional-property-call-result-evidence.md`](optional-property-call-result-evidence.md),
[`object-literal-into-outcome-variant.md`](object-literal-into-outcome-variant.md)); this one is why the
attach pair is missing. The four headers that currently fail to compile do so for a *third* reason — a
retained declaration calling an omitted one,
[`retained-declaration-calls-omitted-callee.md`](retained-declaration-calls-omitted-callee.md) — so this
refusal is not what is blocking the build today. It is what keeps the package's public attach surface
absent once that is fixed.

## Third measured site: `@flighthq/statusbar`, and the same rule on a *record*

`pushStatusBarStyleEntry` refuses with the same code and the same advice, over a declared **record**
rather than a union:

```
cpp emission failed for @flighthq/statusbar/packages/statusbar/src/statusbar.ts: contextual C++ union
conversion requires equivalent source union evidence: target optionalSingle carriers
[flight::Ref<applied_baseline_color_provider_entries_overlays_provider_style_provider_visibility_provider_4a6fee11134dc47d>]
from call source optionalSingle carriers [flight::Ref<StyleStackState>]. The alternatives match uniquely, but
no checked target-runtime conversion exists between their C++ carriers. Keep both sides on the same declared
union alias; for a Map or WeakMap lookup followed by a rebind, construct its declared value owner and ensure
every rebound member already has that owner member's carrier -- a Readonly structural view does not become a
mutable nominal Ref.
```

The source creates the stack state as an object literal and rebinds it into the WeakMap's value slot:

```ts
let state = _styleStacks.get(hostStatusBarInfo);
if (state === undefined) {
  state = { applied: { ...baseline }, baseline, colorProvider, entries: [], overlaysProvider, styleProvider, visibilityProvider };
  _styleStacks.set(hostStatusBarInfo, state);
}
```

The emitter builds an anonymous carrier for that literal (`applied_baseline_color_provider_…_4a6fee11134dc47d`)
and the declared `StyleStackState` on the other side, and the two do not convert — exactly the flattened-arms
problem of the first two sites, on a record. `@flighthq/statusbar` is `assisted` because
`overrides/include/flight/statusbar/statusbar.hpp` supplies that one declaration over the emitted carriers;
the rest of the module, including the full `applyTopStyleEntry` resolver that was refused at an earlier pin,
is the emission's own.

## Fourth measured site: `@flighthq/shortcut`'s attach

`attachGlobalShortcut` refuses under the same code and the same wording, on an awaited provider outcome:

```
cpp emission failed for @flighthq/shortcut/packages/shortcut/src/shortcutExplicitDependency.ts: contextual
C++ union conversion requires equivalent source union evidence: target multiVariant carriers
[flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<reason_4890f49c03969010>>>>,
 flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<reason_subscription_aad5bb6f4cfbab8b>>>>]
from await source multiVariant carriers …
```

The module *does* emit the subscribe outcome union — `ShortcutTriggerSubscribeOutcome`, whose second
alternative is a row over `reason_subscription_c16c6a57c767b33b` — but the awaited call site needs the same
union with the arms spelled `…aad5bb6f4cfbab8b`, so the two expansions are once again unrelated carriers and
the conversion has nothing to convert along. `@flighthq/shortcut` compiles because
`overrides/include/flight/shortcut/shortcut_explicit_dependency.hpp` supplies the one refused function over
the emitted carriers.

## Reduction

The missing piece is the guard's own emission record: the union alias is emitted once from the named
interfaces (`midi_resource.hpp`) and once from flattened anonymous shapes (`midi_subscription.hpp`), and
the refusal is the conversion between them. The two anonymous names differ only in their member lists,
which is the flattened input arm and the flattened output arm respectively.

## What would close this

One carrier per declared union, as
[`canonical-union-carrier-order.md`](canonical-union-carrier-order.md) already asks for its ordering half:
expand `MidiPortResourceState` to `std::variant<flight::Ref<MidiInputPortResourceState>,
flight::Ref<MidiOutputPortResourceState>>` at every use, including a use reached through a WeakMap lookup.
The emitter's message allows a second shape — an identity-preserving runtime conversion between the two
carriers — but that leaves two native types for one TypeScript union, so it is the weaker answer.

## Distinguish from the requests already filed

[`canonical-union-carrier-order.md`](canonical-union-carrier-order.md) is the same rule at a different
trigger: one named union expanded twice with the *same* arms in a different order, failing at
`std::invoke` in `@flighthq/texture`. This one is expanded with *different* arm spellings — flattened
anonymous shapes against declared names — and fails at emission. Both close when a named union reuses its
canonical native carrier.
[`callable-arm-in-contextual-union.md`](callable-arm-in-contextual-union.md) is about a callable that is
not a represented arm; [`compiler-concrete-owner-union-row-assertions.md`](compiler-concrete-owner-union-row-assertions.md)
is about asserting one owner out of a union.

## Carrying workaround

None. `@flighthq/midi` is `partial`; the refused `midiPort` module in the same package is
[`captured-referent-mutation.md`](captured-referent-mutation.md), so a hand supply of `midiSubscription`
would not move the package's own tier and would reproduce a union carrier the profile should express.
