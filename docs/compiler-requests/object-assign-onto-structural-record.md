# `Object.assign` lowers only for a `Record` target

Assigning into a closed structural record — an interface, or an entity an allocation produced — is
refused, whatever the source is. The lowering exists only for `Record<K, V>`.

## Minimal TypeScript

The shape that refuses the whole `@flighthq/notification` module:

```ts
interface NotificationActionSubscription { onNotificationAction: ... }
export function createNotificationSubscription<TSubscription extends Entity>(
  fields: Omit<TSubscription, keyof Entity>,
): TSubscription {
  const subscription = allocateEntity<TSubscription>();
  Object.assign(subscription, fields);
  finishEntity(subscription);
  return subscription;
}
```

and the four forms measured directly, each in its own module:

| source | target | result |
| --- | --- | --- |
| `Box` | `Box` | refused |
| `Partial<Box>` | `Box` | refused |
| `{ a: 1, b: 'x' }` | `Box` | refused |
| `{ a: 1, b: 'x' }` | a local `Box` | refused |
| `Record<string, number>` | `Record<string, number>` | **emitted** |

## Expected behavior

A closed structural target has a known field plan — the same one the emitter already uses to
construct its row — so the assignment lowers field by field:

```cpp
target->a = source->a;
target->b = source->b;
```

## Observed at `2b687ea`, flight `a627849`

```
Object.assign requires one closed callable-object target and an exact compatible object literal source
```

The message names a condition its own lowering then refuses in every form tried: a closed interface
target is one condition, an exact object literal is another, and a call that satisfies both is still
refused. Only the `Record` row is emitted.

## Affected modules

`packages/notification/src/notification.ts` — the refusal takes the whole module, because
`createNotificationSubscription` is what the five subscription factories call. Supplied by the
`notification-subscription-lifecycle` override, whose declared `remaining` records that the
subscription lifecycle (attach, detach, dispose) is not implemented while the emitter refuses the
module that would construct it.

## Carrying workaround

None at the expression level: the target here is a freshly allocated entity whose identity must not
change, so rebuilding it would be a different object. The override supplies the module by hand; the
lifecycle functions it cannot yet supply are declared in its manifest entry rather than stubbed
silently.
