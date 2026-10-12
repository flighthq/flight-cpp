# A generic coroutine's emitted declaration is truncated mid-parameter

The emitted declaration of `attachNotificationSubscription` stops in the middle of its third parameter:
no parameter name completes, no closing parenthesis, no opening brace. The function body that follows is
emitted in full and is attached to the truncated signature, so the whole declaration — and everything the
compiler makes of it — is malformed rather than merely unresolved.

Measured at flight-compiler `2b687ea` / flight `a627849` against emitted `@flighthq/notification`.

## Observed

`generated/include/flight/notification/notification.hpp:350`, with the body indented underneath it:

```cpp
template <typename TSubscription, typename TArgs>
inline flight::Task<flight::types::NotificationSubscriptionAttachOutcome> attach_notification_subscription(TSubscription subscription, NotificationAttach<TArgs> attach, std::function<void(TArgs)> list
  auto runtime = notification_subscriptions.get(subscription);
  if ((!runtime.has_value() || runtime.value()->disposed)) {
```

The source declares three parameters — `subscription`, `attach`, and `listener`:

```ts
async function attachNotificationSubscription<TSubscription extends Entity, TArgs extends unknown[]>(
  subscription: TSubscription,
  attach: NotificationAttach<TArgs>,
  listener: (...args: TArgs) => void,
): Promise<NotificationSubscriptionAttachOutcome> {
```

## What the compiler reports

```
notification.hpp:353:108: error: 'TSubscription' does not refer to a value
notification.hpp:353:130: error: expected '(' for function-style cast or type construction
notification.hpp:383:175: error: expected ':'
notification.hpp:383:175: error: expected expression
```

The second and later errors are this truncation's echo — the rest of the file is parsed as part of a
declaration that never ended — which matters for triage: the first error names the real site, and the
cascade hides every other defect in the module behind it.

## Why it is separate from the module's other failures

The module also refuses `createNotificationSubscription` and emits two further malformed forms
(an anonymous carrier referenced through the wrong namespace, and per-type wrappers whose argument does
not convert). Those are filed separately. This one is purely lexical: the emitter began writing the
parameter list and stopped, so no amount of correct qualification or conversion elsewhere makes this
declaration parse.

## What would close this

Emit the third parameter's name and the closing `)` and `{`. A faithful lowering of
`listener: (...args: TArgs) => void` already exists in the same header for the callback the attach
provider receives, so the shape is present; only the declaration's tail is missing.

## Carrying workaround

None. `@flighthq/notification` is supplied by `notification-subscription-lifecycle`, whose copy predates
this emission and is retained: supplying this declaration is not what the module needs while the three
other defects stand.
