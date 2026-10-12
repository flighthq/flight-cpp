# `Promise.allSettled` needs a binding and a settled-result lowering

```ts
const settlements = Promise.allSettled(itemPromises);
const results = await settlements;
const failed = results.find((result) => result.status === 'rejected');
if (failed?.status === 'rejected') throw failed.reason;
```

```text
ambient value Promise member allSettled has no C++ binding
```

The compiler's runtime binding table binds `Promise.all`, `Promise.reject` and `Promise.resolve`, but not
`allSettled`. A target profile cannot add it, because the runtime already binds `Promise` and a second
binding is refused as a duplicate.

The flight-cpp runtime already has the operation and the result vocabulary in `include/flight/task.hpp`:

- `flight::all_settled_tasks(const Array<Task<Value>>&) -> Task<Array<TaskSettlement<Value>>>`;
- the two arms `PromiseSettledResult` narrows to, `TaskFulfillment<Value>` (`status`, `value`) and
  `TaskRejectionResult` (`status`, `reason`), with `as_fulfilled`/`as_rejected` selection. `status` is a
  string because the source narrows on it.

## Expected

Bind `allSettled` beside `all`, and lower `PromiseSettledResult<T>` to the runtime's two arms, so that the
`status` narrowing and the `value`/`reason` reads above select the arm. The binding alone is not enough:
without the arm lowering, the `find` callback and `failed.reason` have no carrier.

## Affected

`@flighthq/assets` `assetLibrary.ts` `loadAssetGroup` (line 186). At `a627849` this is the module's second
refusal, found by stubbing the first.

## Carrying workaround

None. `loadAssetGroup` is absent from the `assets-reject-task-template-parameter` override.
