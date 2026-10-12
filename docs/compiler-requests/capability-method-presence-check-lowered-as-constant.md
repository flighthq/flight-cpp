# A capability-method presence check lowers to a constant, so an unset method is misreported

`typeof hostGeolocation.promptForAccess !== 'function'` is emitted as `flight::String("function") !=
flight::String("function")` — a constant false. That is the right answer for a *type* test on a value,
which a typed language cannot fail, but wrong for this one: the source's test is a *member presence* test,
and its C++ analogue is an unset `std::function`, which is reachable. The guard therefore falls through,
the unset call throws, and the surrounding `catch (...)` reports a different reason than the source does.

Measured at flight-compiler `2b687ea` / flight `a627849` against emitted `@flighthq/permissions`.

## Observed

Source, `packages/permissions/src/permission.ts:230`:

```ts
async function requestGeolocationAccessPermission(
  hostGeolocation: Readonly<HostGeolocationCapability> | undefined,
): Promise<PermissionRequestOutcome> {
  if (hostGeolocation === undefined || typeof hostGeolocation.promptForAccess !== 'function') {
    return { reason: 'runtime-unavailable' };
  }
  …
```

Emission, `generated/include/flight/permissions/permission.hpp:264`:

```cpp
inline flight::Task<PermissionRequestOutcome> request_geolocation_access_permission(std::optional<CAP> host_geolocation) {
  if ((!host_geolocation.has_value() || (flight::String("function") != flight::String("function")))) {
    co_return … {.reason = flight::String("runtime-unavailable")} …;
  }
  …
```

The first operand is the source's `=== undefined` branch and works. The second is the constant false, so a
capability that supplies the slot's type but leaves the function unset reaches the call:

```cpp
  try {
    (outcome = co_await flight::row_get<flight::RowKey<"promptForAccess">>(host_geolocation.value())());
  }
  catch (...) {
    co_return … operation-failed …;
  }
```

## Measurement

One host, `prompt_for_access` left unset, `requestPermission(hostPermissions, undefined, hostGeolocation,
'geolocation')`:

| | reason |
| --- | --- |
| pinned source | `runtime-unavailable` |
| emitted C++ | `operation-failed` |

The difference is observable through the public API, and it is the one reason the source's comment for this
function is about: a *missing route* is reported as missing, not as an operation that failed.

## Why the constant is right elsewhere

`isValidProtocolScheme` tests `typeof scheme !== 'string'`, which lowers to the same constant false and is
**correct** there: `scheme` is a parameter whose type cannot be anything else, so the branch is genuinely
unreachable and false is the honest answer. The rule needs to tell the two apart — a type test on a value
(unreachable) against a *member presence* test on an object (reachable, and in C++ an unset
`std::function`).

## What would close this

Lower a `typeof <member> !== 'function'` test on an object to the emitted member's own emptiness — for a
`std::function` member, a falsy test — rather than to a constant. Capability slots in this profile are
exactly the place where the source's optional provider methods become plain members whose unset state must
stay meaningful.

## Carrying workaround

None. `@flighthq/permissions` is `assisted` through an unrelated repair
(`conditional-absent-branch-optional`), and this difference is a reason value for one unset slot rather
than a missing behavior; the package's native test covers the four *set* prompt outcomes and deliberately
does not assert this one.
