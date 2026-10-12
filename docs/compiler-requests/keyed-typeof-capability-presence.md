# A keyed `typeof` over a closed capability could enumerate its key set

A presence query indexed by a key drawn from a closed, declared key set is refused, even though the key
set is small enough to enumerate and the runtime already carries the member tables it would need.

## Minimal TypeScript

`@flighthq/lifecycle`, `packages/lifecycle/src/lifecycle.ts:89`:

```ts
export function explainLifecycleOperation(
  hostLifecycle: Readonly<HostLifecycleCapability>,
  operation: LifecycleOperation,
): BackendOperationExplanation {
  const implemented = typeof hostLifecycle[operation] === 'function';
  return { implemented, layer: implemented ? 'host' : 'sentinel', operation };
}
```

and the key set, declared in `@flighthq/types` (`packages/types/src/Lifecycle.ts:47`):

```ts
export type LifecycleOperation = Exclude<keyof HostLifecycleCapability, keyof Entity>;
```

## Expected behavior

The key set is closed and statically known, and each optional method on the generated capability is
emitted as `std::optional<std::function<…>>`, so `typeof x[key] === 'function'` is a presence test with
two statically-known sides: the enumerated keys on one and the record's optional cells on the other. A
switch over the declared keys — each arm testing its own cell — or a lookup in the row's member table is
the lowering; the runtime already emits those tables (`generated/include/flight/sdk/structural_members.hpp`),
so the mechanism exists and this is a question of the emitter using it for a computed key.

## Evidence, and its provenance

Recorded by this module's override, whose `reason` names it as the first of five defects the module hits
("the emitter refuses the keyed typeof capability query"). I did **not** re-measure it here: the module
refuses earlier on a different defect (`flight-cpp WeakMap value requires a proven C++ representation`),
so the typeof refusal is behind that one and not observable without clearing it. Recording the provenance
matters more than the phrasing: everything above the reproduction is the override author's measurement,
not mine.

## Affected modules

`@flighthq/lifecycle` — `lifecycle.ts`, supplied by the `lifecycle-complete-module` override. The shape
(a runtime key drawn from `keyof` a capability, tested for presence) plausibly recurs wherever a package
explains "which parts of this host are implemented", so it is worth filing on its own.

## Carrying workaround

`lifecycle-complete-module` supplies the module by hand, and its `change` explains why the author did not
patch instead: "Replacing the derived keyed operation query with a hand-written source roster would
create exactly the second source of truth the Flight type intentionally avoids." I agree with that
reasoning and have not added one.
