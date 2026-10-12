# Asserting a `Promise<unknown>` to `Promise<T>` needs a typed task view

A cache that stores `Promise<unknown>` and hands it back to a generic caller asserts it to the caller's
type parameter:

```ts
interface Entry { loadPromise: Promise<unknown> | null; value: unknown }
export function share<T = unknown>(entry: Entry): Promise<T> {
  return entry.loadPromise as Promise<T>;
}
```

`Promise<unknown>` is emitted as `flight::Task<flight::Any>`, and the assertion target is the template
`flight::Task<T>`. The emitter refuses the whole module:

```text
type assertion target must identify exactly one C++ variant alternative: target flight::Task<T> against [flight::Task<flight::Any>]
```

Measured at flight-compiler `2b687ea`, with every carried patch applied, against flight `a627849`.

## Expected

TypeScript's assertion is unchecked and does not touch the promise: the caller observes the same settlement,
and the fulfilled value is read at `T`. The C++ shape is a typed view of the same task that projects the
`flight::Any` result to `T` when it settles, and leaves rejection untouched. It is not a new task. For
`T = flight::Any`, the default here, the view is the identity. Whether the projection checks the `Any`
(throwing a `TypeError` on mismatch) or trusts it is a runtime-contract decision. Either way it must keep
one settlement, so it cannot be a copy that settles separately.

## Affected

`@flighthq/assets` `assetLibrary.ts` `acquireAsset` (lines 45 and 47, `existing.value as T` and
`existing.loadPromise as Promise<T>`), the module's first refusal at `a627849`.

## Carrying workaround

The `assets-reject-task-template-parameter` override hand-writes `acquire_asset<T>`.
