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
## Affected

`@flighthq/loader` `resourceLoader.ts` `runEntry`: the catch retries after `await delay(delayMs)`, and its
other paths pass `error` to `entry.reject(error)` and test `error instanceof DOMException`. `resource_loader.hpp`
is a refused placeholder, and `contract`/`index` cascade from it -- and they still do, for the reason the last
section records.



## Carried by a compiler patch, with the runtime half in this repository

`source-patches/compiler-catch-await-owned-carrier.patch` carries the mechanism, and
`include/flight/error.hpp` carries the one runtime piece it needs. The expected C++ above is what the emitter
now writes; `flight::caught_exception` is that runtime piece.

What happens, in the order the value takes:

- The handler records an **owned carrier**: a mutable `std::exception_ptr` local, captured from the caught
  binding by the same assignment shape the target already lowers to `std::current_exception()` for a
  catch-recorded slot. Nothing new is represented -- the carrier is the slot the runtime already owns the
  exception object in -- it is only carried further than a rethrow needed.
- The hoisted body **re-binds the same binding** from it: `const std::exception& error =
  flight::caught_exception(caught);`, which rethrows the pointer and returns the object it owns. The object is
  the one that was thrown, not a copy, so identity, the subclass, and every downstream rule
  (`static_cast<flight::Error>`, the erased Error view, a task rejection) answer exactly as they do for a
  non-awaiting `catch (error)`.
- The handler **stays narrow** (`catch (const std::exception& error)`) whenever its carrier is re-bound. A
  thrown value that is not a class exception has no `const std::exception&` to re-bind -- and the source's own
  narrowing never admits one -- so widening here would run a body it cannot give a value to. A handler whose
  body only reifies the caught value is still widened by
  [`catch-handler-breadth-attempt-all.md`](catch-handler-breadth-attempt-all.md), and the two rules compose:
  the carrier's reader decides.
- A body that never reads its binding still hoists as a flag alone, and a catch that never awaits is
  untouched, so nothing moves where nothing needed to.

The one read of the carrier this admits is the re-binding itself. That is load-bearing rather than incidental:
a slot read by anything else is disqualified from being an exception carrier (it is ordinary erased storage
again), so the re-binding is the only shape that can consume the carrier this way -- and the pass accepts it
for exactly that reason.

Controls: `cppCatchAwaitCarrier.test.ts` (7 cases, five mutations with teeth -- carrier not recognized 4
fails, re-binding not emitted 3, handler widening 2, nothing recorded 5) and `tests/caught_exception_test.cpp`
(6 checks: identity across the re-binding, across a rethrow and across a task rejection, the subclass
surviving, and the absent-carrier failure shape -- minting a replacement Error there fails 3).

## What it unblocks, and what the module still needs

`@flighthq/loader`'s `resourceLoader.ts` now passes this pass. It is refused for the **next** reason, which is
a different mechanism and is already filed:

```text
a reference assertion from flight::Ref<flight::types::ResourceLoader> to flight::Ref<ResourceLoaderInternal>
has no heritage to cast along
```

`resourceLoader.ts:118` declares `interface ResourceLoaderInternal extends ResourceLoader`, and the module
narrows through `const internal = loader as ResourceLoaderInternal`. That is
[`flattened-interface-refinement-identity.md`](flattened-interface-refinement-identity.md) exactly.

And the part of the module that now DOES emit does not compile, for a second, unfiled reason:
`_noopLoad`'s `Promise.resolve(undefined)` settles a `Task<flight::Undefined>` where the declaration states
`Task<flight::Any>`, recorded as
[`task-resolved-value-domain-mismatch.md`](task-resolved-value-domain-mismatch.md). So the package's compile
gate regresses from 4/4 (a placeholder that compiled) to 1/4 (the module, emitted and not yet compiling)
until that one is fixed -- which is the honest cost of emitting the module instead of hiding it behind a
placeholder. Nothing in this unit's mechanism touches either defect, and no loader-local override or stub is
carried in their place.
