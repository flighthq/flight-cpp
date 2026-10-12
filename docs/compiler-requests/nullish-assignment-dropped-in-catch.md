# A nullish assignment as a statement in a catch body is dropped

The emitter lowers `x ??= y` correctly in an expression position and drops it in a statement position
inside a catch body: the emitted handler contains bare `failure;` — the assignment target as a discarded
value, with neither the operator nor the right-hand side. `failure` therefore stays at its initial
`nullptr`, the source's `if (failure !== null) throw failure;` never fires, and the first failure is
**lost** while the loop reports success.

Measured at flight-compiler `2b687ea` / flight `a627849` against emitted `@flighthq/power`.

## The rule's condition

Source, `packages/power/src/power.ts:139-148`:

```ts
  for (const provider of pending) {
    try {
      if (provider.destroy !== undefined) assertSyncVoid(provider.destroy());
    } catch (error) {
      failure ??= error;
      continue;
    }
    _destroyedKeepAwake.add(provider);
  }
  if (failure !== null) throw failure;
```

Emitted, `generated/include/flight/power/power.hpp:284-298`:

```cpp
  flight::Any failure = nullptr;
  for (auto provider_2 : pending) {
    try {
      if (provider_2->destroy.has_value()) {
        assert_sync_void<flight::Undefined>((provider_2->destroy.value()(), flight::undefined));
      }
    }
    catch (const std::exception& error) {
      failure;            // <- the assignment is gone
      continue;
    }
    destroyed_keep_awake.add(provider_2);
  }
  if (([&]() { const auto& presence_operand = failure; return !presence_operand.is_null(); }())) {
    throw failure;
  }
```

The same operator *is* implemented for an expression position — `particleEmitterSignals.ts:23` lowers to a
full nullish-assignment block (`assignment_receiver` / `assignment_existing` / `assignment_value`,
`flight/particles/particle_emitter_signals.hpp:42`) — so this is the statement lowering inside a handler,
not a missing operator.

## Where it fires

| site | source | emitted |
| --- | --- | --- |
| `destroyPowerKeepAwake` | `power.ts:143` | `power.hpp:292` — `failure;` |
| `detachPower` | `power.ts:160` | `power.hpp:159` — `failure;` |

Both are the same construct, in the same file, and both lose the value their source records.

## Reduction

One provider throws, a second would have succeeded, and the caller observes whether the failure came back:

| probe | measured | required |
| --- | --- | --- |
| `destroyPowerKeepAwake` with one throwing provider and one healthy sibling | sibling destroyed (1), **nothing rethrown** | sibling destroyed (1), the first error rethrown |

The attempt-all half is correct — the sibling *is* attempted and the loop *does* continue — so the defect
is only the record-and-rethrow half. That is enough to make a failed teardown indistinguishable from a
successful one at the call site.

## Second measured site: `@flighthq/menu` `destroyAppMenu`

The same emission, from the P-PL3 increment, on the attempt-all teardown of the application menu
provider:

```cpp
  flight::Any failure = nullptr;
  for (auto provider_2 : pending) {
    try {
      if (provider_2->destroy.has_value()) {
        assert_sync_void<flight::Undefined>((provider_2->destroy.value()(), flight::undefined));
      }
    }
    catch (const std::exception& error) {
      failure;            // <- the assignment is gone here too
      continue;
    }
    destroyed_app_loop.add(provider_2);
  }
  if (([&]() { const auto& presence_operand = failure; return !presence_operand.is_null(); }())) {
    throw failure;
  }
```

`generated/include/flight/menu/menu.hpp:247`, from `packages/menu/src/menu.ts`'s `destroyAppMenu`. The
retention half is correct — the provider whose `destroy` threw is not added to `destroyed_app_loop`, so a
later call retries exactly the failures, and a provider that succeeded is never destroyed twice — and the
record-and-rethrow half is lost exactly as in `power.ts`: measured against the emitted function, the
first failure is never rethrown. The behaviour test that covers this function asserts the retention and
deliberately does not assert the rethrow.

## What would close this

Lower `??=` in a statement position to the same nullish-assignment block the expression position already
produces, discarding the result rather than omitting the write. The emitted `flight::Any` carrier can hold
the caught value the same way the `std::exception_ptr` slot does in
[`catch-handler-breadth-attempt-all.md`](catch-handler-breadth-attempt-all.md), so the two requests touch
the same handler and should be resolved together if the fix is shared.

## Distinguish from the requests already filed

No other request mentions `??=`. [`catch-handler-breadth-attempt-all.md`](catch-handler-breadth-attempt-all.md)
is about which thrown values reach the handler; this one is about what the handler does with the value it
already has, and both are observable in the same function with a `std::runtime_error`.

## Carrying workaround

None. `@flighthq/power` is `assisted` at this pin and its failure path is a source-patch candidate — the
pinned TypeScript can be rewritten to `if (failure === null) failure = error;` with no change in meaning —
but a patch is a pin-pinned, verified instrument and this repository does not apply one speculatively.

## Carried by a compiler patch

[`compiler-nullish-assignment-statement-in-catch.patch`](../../source-patches/compiler-nullish-assignment-statement-in-catch.patch)
repairs the one condition that caused this: a `??=` whose declared target type has no absent member was
emitted as the target alone, on the reasoning that such a carrier can never be nullish. That reasoning
misses the erased carrier -- an `unknown`/`any` target is emitted as `flight::Any`, whose nullishness is a
runtime property of the value -- so the branch now keeps the no-op only where the carrier really cannot
hold null or undefined and otherwise emits the carrier's own loose nullish test:

```cpp
([&]() { auto&& assignment_target = failure; if (assignment_target.is_nullish()) assignment_target = error; return assignment_target; }())
```

The loose test is the point: `??=` assigns for both `null` and `undefined`, so the first failure is
retained across later iterations exactly as the source says. The patch's own backend test pins the
statement and expression positions, the loose guard, and that a number target still takes the no-op; the
manifest entry records the run. Regenerating `@flighthq/power` package-scoped is what shows both call
sites above carrying the write.

**Not carried, and still open**: the catch breadth in
[`catch-handler-breadth-attempt-all.md`](catch-handler-breadth-attempt-all.md). The handler is still
`catch (const std::exception&)`, so this fix records and rethrows the failures that reach it -- every
`std::exception` -- while a non-class throw still escapes the loop. The two requests touch the same
handler and should be resolved together if the fix is shared.

**Measured outcome of the patch, and the blocker beside it.** With the patch applied, regenerating
`@flighthq/power` package-scoped does emit the guarded assignment at both call sites -- and the module does
not compile, because the slot the write lands in is the wrong carrier for a caught value:
`flight/power/power.hpp:159` and `:292` fail with `no viable overloaded '='`. That is the defect filed as
[`catch-recorded-slot-carrier.md`](catch-recorded-slot-carrier.md), and the package's slice was left
unregenerated rather than committed in a state where a shippable module stops compiling. The two have to
land together: this patch is the write, that request is the slot.
