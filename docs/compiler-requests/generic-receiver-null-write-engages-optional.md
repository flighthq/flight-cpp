# A `null` written through a generic receiver must produce the member's absent state, not an engaged null

Silent miscompilation, and a crash.

A member declared `T | null` with a pointer-like `T` is stored as `std::optional<P>`, where `P` is a `Ref`,
`std::function`, or `ErasedRef`. Every emitted reader tests that member's null with `has_value()`.

When the member is written through a concrete receiver, the emitter writes `std::nullopt`.
`@flighthq/render`'s `destroyRenderState` lowers `runtime.registryMiss = null` to
`runtime->registry_miss = std::nullopt`, and `@flighthq/entity` writes `binding = std::nullopt`.

When the same statement is lowered inside a function generic over the receiver, the emitter cannot see the
member's storage type and writes `nullptr`. Assigning `nullptr` to a `std::optional<Ref<...>>`, or passing it
to `row_set` for one, produces an ENGAGED optional holding a null pointer.
- `initializeRenderStateRuntime<Runtime, Registries>` writes `runtime->registry_miss = nullptr`.
- `initializeRenderStateFields<State>` writes `row_set<"displayObjectClipHooks">(state, nullptr)`.

Readers then see `has_value()` true for a value that is null.

## Reproduction

The upstream lifecycle `createRenderState()` then `destroyRenderState(state)` segfaults natively at flight
`a627849`. `destroy_render_state` tests `runtime->registry_miss` for presence and calls `->clear()` through
the null pointer. `tests/generated_render_state_test.cpp` reproduces it.

The same writes leave:
- `state.displayObjectClipHooks` reading as present (upstream expects `null`);
- `runtime.renderAdaptHook` as an engaged empty `std::function`, so calling it after a presence test would
  throw `bad_function_call`;
- `runtime.binding` as an engaged null `ErasedRef`.

## Minimal TypeScript

```ts
interface Hooks { clear(): void }
interface Runtime { miss: Hooks | null }

function initialize<R extends Runtime>(runtime: R): R {
  runtime.miss = null;          // emitted: runtime->miss = nullptr;   -> engaged, null
  return runtime;
}

export function teardown(runtime: Runtime): void {
  runtime.miss?.clear();        // emitted: if (miss.has_value()) miss.value()->clear();  -> null dereference
  runtime.miss = null;          // emitted: runtime->miss = std::nullopt;                 (correct)
}
```

## Required C++ shape

A `null` written to a member whose storage is `std::optional<P>` writes `std::nullopt`, whether the receiver is
concrete or generic. For the generic case, the member's storage is known once the template is instantiated.
So the write can be spelled `= {}` (value-initialize the member's own type) instead of `= nullptr`, and
`row_set` can be given `std::nullopt`. `row_set` already maps `nullptr` to the absent state for storage that
cannot hold a null (`writes_unrepresentable_null_as_absence`), so its pointer-valued case is the only one
this request changes.

## Affected modules

At flight `a627849`, confirmed natively: `@flighthq/render` `renderState.ts`, in
`initializeRenderStateRuntime` (`binding`, `renderAdaptHook`, `registryMiss`) and
`initializeRenderStateFields` (`displayObjectClipHooks`, written twice: the default branch and the `??`
fallback).

19 generic-receiver `member = nullptr` writes exist across 11 generated headers. Each is a candidate with the
same defect when its member's storage is a pointer-like optional. They are not examined here.

## Workaround carried

The `render-state-generic-null-*` emission repairs respell the five render sites to `std::nullopt`. They also
respell the `??` fallback's null arm to `std::nullopt` with the present arm wrapped in `std::optional{...}`.
Each writes exactly what the concrete-receiver lowering of the same statement writes, and what every emitted
reader tests for. They expire when generic-receiver null writes produce the absent state.
