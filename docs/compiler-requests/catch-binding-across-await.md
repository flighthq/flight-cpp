# A catch body that awaits and reads its binding needs an owned caught value

C++ forbids `co_await` inside a handler, so the `catch-await-hoisting` lowering moves a catch body that
awaits out of the handler:

```text
{ bool caught = false; try { … } catch (…) { caught = true; } if (caught) { <body> } }
```

That is sound only while the body does not read the catch binding, because the hoisted body runs after
the handler ends and the handler's `const std::exception&` is gone. When the body reads the binding,
anywhere, the pass refuses the whole module. The refusal is right; it should not stay one.

Measured at flight-compiler `2b687ea`, with every carried patch applied, against flight `a627849`. At this
pin it is the only refusal of this kind in the inventory.

## Minimal TypeScript

```ts
export async function run(load: () => Promise<number>, retry: () => Promise<void>, reject: (e: unknown) => void): Promise<void> {
  try {
    await load();
  } catch (error) {
    if (Math.random() < 0.5) {
      await retry();
      return;
    }
    reject(error);
  }
}
```

## Refusal

```text
Compiler lowering pass catch-await-hoisting failed: a caught source value used after an await requires
an owned target representation that preserves the thrown unknown value across suspension
```

## Expected C++

The handler keeps the owning `std::exception_ptr`, which keeps the exception object alive, and the hoisted
body re-binds the same `const std::exception&` that a non-awaiting catch binds:

```cpp
std::exception_ptr caught;
try { co_await load(); } catch (const std::exception&) { caught = std::current_exception(); }
if (caught) {
  const std::exception& error = flight::caught_exception(caught);  // proposed runtime helper: rethrow-and-catch into a reference
  ...
}
```

The binding has the representation every non-awaiting `catch (error)` already uses, so absence, identity
and equality do not change. Only its lifetime is carried across the suspension. The handler still catches
only `std::exception`, as the non-awaiting lowering does.

## Affected

`@flighthq/loader` `resourceLoader.ts` `runEntry`: the catch retries after `await delay(delayMs)`, and its
other paths pass `error` to `entry.reject(error)` and test `error instanceof DOMException`. `resource_loader.hpp`
is a refused placeholder, and `contract`/`index` cascade from it.

## Carrying workaround

None. Loader stays partial on this module.
