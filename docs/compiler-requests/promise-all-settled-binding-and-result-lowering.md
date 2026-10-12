# `Promise.allSettled` needs a static binding and settled-result lowering

flight-cpp already implements the aggregate operation and both settled-result arms. The compiler's
semantic lowering also retains the instantiated `PromiseSettledResult<T>` type. C++ emission stops
one step earlier: `Promise.allSettled` is absent from the compiler-owned `Promise` static-member
table.

## Minimal TypeScript

```ts
export async function wait(items: Promise<void>[]): Promise<void> {
  await Promise.allSettled(items);
}
```

At flight-compiler `2b687ea`, with flight `a627849` and flight-cpp's pinned runtime profile, this
refuses:

```text
ambient value Promise member allSettled has no C++ binding
```

## Pinned boundary

| source shape | result |
| --- | --- |
| `await Promise.all(items)` for `Promise<void>[]` | emits `co_await flight::all_tasks(items)` |
| `await Promise.allSettled(items)` and discard the result | refuses the missing `allSettled` member |
| infer and return `Promise.allSettled(items)` | refuses the same missing member |
| await the result and read `result.status` | refuses the same missing member before result lowering |
| explicitly return `Promise<PromiseSettledResult<number>[]>` | first refuses missing `PromiseSettledResult[type]` |

The explicit-return control is a second boundary, not a different cause for the corpus call. The
runtime profile maps `PromiseFulfilledResult<T>` to `flight::TaskFulfillment<T>` and
`PromiseRejectedResult` to `flight::TaskRejectionResult`, but it does not map their ambient union
alias. Compiler semantic tests already prove that `allSettled` inference retains the mapped result,
including `void`; the missing work is in the C++ binding and representation plan.

## Required lowering

The static call should lower to the runtime operation already present:

```cpp
co_await flight::all_settled_tasks(items);
```

When the result is observed, the source `PromiseSettledResult<T>[]` needs the runtime's
`Array<TaskSettlement<T>>` carrier. A `status === 'fulfilled'` or `status === 'rejected'` guard must
select the corresponding arm through `as_fulfilled` or `as_rejected`; only that selected arm exposes
`value` or `reason`. The compiler must not emit direct member access on a `std::variant`, add the
other arm's member, or erase rejection values to `Any`.

`all_settled_tasks` is already covered natively for source order, non-rejection of the aggregate,
exact rejection values, `void`, and an empty input. `TaskFulfillment`, `TaskRejectionResult`, and the
two narrowing helpers are also covered. This request does not ask for another runtime operation.

The binding must be compiler-owned. The built-in flight-cpp symbols own `Promise` and its static
members before downstream profiles are considered, so adding `allSettled` to an external binding
manifest cannot augment this table.

## P-3D2 reach and wider corpus

`packages/scene3d-resources/src/loadScene3DResources.ts` has two calls, both awaited only for
completion and both discarding the settlements. The committed inventory records the missing-member
diagnostic for that module. A disposable exact-pin member mapping moved this caller past the direct
refusal and into an already-refused dependency, so it did not change the emitted-header total; it
also did not prove the observed-result path.

Two other pinned Flight sites distinguish the lanes:

- `packages/assets/src/assetLibrary.ts` retains the aggregate, finds a rejected settlement by
  `status`, and throws its exact `reason`, so it needs the full arm lowering.
- `packages/host-tauri/src/tauriShortcut.ts` awaits and discards the result, like the Scene3D caller;
  that package is outside the native SDL applicability set.

The runtime implementation and its existing notes in
[`flight-compiler-adoption.md`](../flight-compiler-adoption.md) are supporting evidence, not an
existing compiler request. No source rewrite, compiler patch, or generated repair is carried.
