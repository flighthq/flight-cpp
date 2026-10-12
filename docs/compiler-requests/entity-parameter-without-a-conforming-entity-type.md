# A shared function over the entity base cannot be called with the entities that conform to it

The emitter declares the module's shared subscription functions over `flight::Ref<flight::types::Entity>`
and the per-type wrappers over `flight::Ref<NotificationActionSubscription>` and its four siblings. The
subscription structs each declare the entity's own field — `entity_runtime_key` — but derive from
`flight::ReferenceEnabled`, not from `Entity`, so the wrapper's argument does not convert and ten emitted
calls cannot resolve.

Measured at flight-compiler `2b687ea` / flight `a627849` against emitted `@flighthq/notification`.

## Observed

```cpp
// notification.hpp:284 — the shared function
inline flight::Task<flight::types::NotificationSubscriptionDetachOutcome> detach_notification_subscription(flight::Ref<flight::types::Entity> subscription) { … }

// notification.hpp:288 — the per-type wrapper
inline flight::Task<flight::types::NotificationSubscriptionDetachOutcome> detach_notification_action_subscription(flight::Ref<flight::types::NotificationActionSubscription> subscription) {
  return detach_notification_subscription(subscription);
}
```

```
notification.hpp:294:10: error: no matching function for call to 'detach_notification_subscription'
```

Five detach wrappers, five attach wrappers, the same shape. In TypeScript the argument is accepted
because `TSubscription extends Entity` is a structural constraint the subscription satisfies; in C++ the
parameter is a nominal base the subscription does not derive from:

```
Sub derives from Entity: no
Ref<Sub> converts to Ref<Entity>: no
```

`flight::Ref<T>` is `std::shared_ptr<T>` for every one of these types, so there is no conversion path —
not a narrowing, not a widening, and no runtime helper is reached by the emitted call.

## Why it is more than one wrapper

The shared functions are emitted once and called from the per-type wrappers *and* from the generic
`attachNotificationSubscription`, which takes `TSubscription` and passes it to
`detach_notification_subscription` in the same way. So the argument's type is the module's own choice of
base, and the fix is a decision the emitter has to make once:

- declare the shared functions over a template parameter constrained the way the source's
  `TSubscription extends Entity` is (an entity-row projection of the subscription, or the runtime's
  structural entity view), or
- make the emitted subscription entities derive from `Entity`, which is what their repeated
  `entity_runtime_key` field already says they are.

Either closes all ten calls, plus the generic one.

## Distinguish from the requests already filed

[`retained-declaration-calls-omitted-callee.md`](retained-declaration-calls-omitted-callee.md) is a call
whose callee was never emitted. Here the callee exists and is correct; it is the argument that cannot
convert, and no placeholder or declaration would help.

## Carrying workaround

None. `@flighthq/notification`'s supply predates this emission; the package's tier is unchanged by this
defect and remains `partial` with its declared gap.
