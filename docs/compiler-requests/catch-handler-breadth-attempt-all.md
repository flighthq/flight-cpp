# A `catch (error)` whose body continues must catch every thrown value

The emitter lowers `catch (error)` to `catch (const std::exception& error)`. That is fine when the handler
is the whole failure story, and wrong when the *catch is load-bearing*: where the source catches, records,
and continues — attempting a sibling provider, or the next iteration of a teardown loop — a thrown value
outside the `std::exception` hierarchy escapes the handler immediately, so the remaining work is skipped.
C++ has a value channel for thrown values (`throw 42;`), so this is reachable and not a representation
limit, and the value escapes with its type intact rather than being normalized.

Measured at flight-compiler `2b687ea` / flight `a627849` against emitted `@flighthq/mediasession` (3/3
modules emitted, 0 refusals) and `@flighthq/power`.

## The rule's condition

Emitted handler, `generated/include/flight/mediasession/mediasession.hpp:140-145`:

```cpp
    catch (const std::exception& error) {
      if (!has_error) {
        first_error = std::current_exception();
      }
      (has_error = true);
    }
```

The source it lowers (`mediasession.ts:63-68`) catches *any* thrown value:

```ts
    try { assertSyncVoid(provider.destroy()); } catch (error) { if (!hasError) firstError = error; hasError = true; }
  }
  if (hasError) throw firstError;
```

Its contract is stated in the source comment at `mediasession.ts:52-54`: "Every distinct provider is
destroyed once; … one throwing provider does not prevent the other distinct provider from being
attempted." `@flighthq/power` states the same rule more strongly at `power.ts:130-132`: "Attempt-all:
every obligation is tried even after one throws, and the first error is rethrown once the siblings have
run."

## Where it fires

Twelve handlers across eleven generated headers are lowered to `catch (const std::exception&)` at this
pin. The measured ones are the attempt-all loops, because there the narrowing changes behavior rather than
just the diagnostic:

| site | source | shape |
| --- | --- | --- |
| `destroyMediaSession` | `packages/mediasession/src/mediasession.ts:55` | loop over a `Set`, records the first error, rethrows after the loop |
| `destroyPowerKeepAwake` | `packages/power/src/power.ts:133` | loop over a `Set`, `continue`s past a failure, rethrows the first after the loop |
| `detachPower` | `packages/power/src/power.ts:151` | loop over recorded teardowns, same attempt-all shape |

## Reduction

A distinct sibling provider is present in both cases, so "the sibling was attempted" and "the first error
came back" are separately observable.

| probe | measured | required |
| --- | --- | --- |
| `destroyMediaSession` where the first provider throws `std::runtime_error` | sibling destroyed, error rethrown | same |
| `destroyMediaSession` where the first provider throws `42` | **sibling never attempted (0), the `int` escapes** | sibling attempted (1), then 42 rethrown |
| `destroyPowerKeepAwake` where one provider throws `std::runtime_error` | sibling attempted (1) | 1 |

The `int` case is the whole defect: the same call, the same input shape, a thrown value that is not a
class. Nothing about the provider changed but the type of the value it throws.

## What would close this

Lower a catch whose body continues — anything with a statement after the handler's exit path, or a
`continue`/`break`/record-then-fall-through — to `catch (...)` and recover the value where the source
reads it, or to a handler that catches `...` and stores `std::current_exception()`, which the
`destroyMediaSession` lowering already does for the first-error slot. A handler that only returns a
sentinel may keep the narrow form; the discriminator is whether the source's `error` binding is *used* and
whether execution continues after the handler.

## Distinguish from the requests already filed

[`catch-binding-across-await.md`](catch-binding-across-await.md) is about the `catch-await-hoisting` pass
reading its binding after the handler ends; this one is about which thrown values reach the handler at all.
[`compiler-erased-error-view-and-instanceof-guard.md`](compiler-erased-error-view-and-instanceof-guard.md)
is about the shape of a caught error once it is a value; here the value never arrives.

## Carrying workaround

None. No override is carried for `@flighthq/mediasession` or `@flighthq/power`; the sibling-attempt path is
correct for every `std::exception`, which is what the runtime's own error types are, and the gap is the
non-class throw a C++ host can still make.

## Carried by a compiler patch, for the handlers this asks about

[`compiler-catch-handler-breadth.patch`](../../source-patches/compiler-catch-handler-breadth.patch) widens
exactly the handlers this request names. A handler whose binding is used **only** as the right side of a
record into a slot carried as `std::exception_ptr` -- which the record lane lowers to
`std::current_exception()` and which never mentions the binding -- needs no name for the caught value, so it
is emitted as `catch (...)`. `throw 42` then reaches the handler, the slot reifies it, and
`std::rethrow_exception` hands the caller the same `int`: measured in `@flighthq/power`, the sibling provider
is attempted and the caller catches that `int`, and with an `int` throw followed by an exception throw the
retained failure is the `int`.

The boundary is narrower than "fixed", and it is deliberate. A handler that **reads** the caught value -- a
member, an argument, a direct rethrow -- keeps `catch (const std::exception& name)`, because C++ has no way
to name a non-class caught value at all. The backend test pins both sides of that line: the reify-only
handler widens, a handler that rethrows its binding or records it into a non-exception slot does not, and
neutralising the condition fails the widened case while the boundary controls keep passing. Widening those
would not be a representation limit but a lie about the value, which is why this stops here rather than
boxing it into something it never was.

`@flighthq/mediasession`'s `destroyMediaSession` has the same reify-only shape (`firstError = error` into an
`std::exception_ptr` slot), so it should pick the fix up when its slice is next regenerated; that package was
not touched by this unit, which stayed package-scoped to `@flighthq/power`.
