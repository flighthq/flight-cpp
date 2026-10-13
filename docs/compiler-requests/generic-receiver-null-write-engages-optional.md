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

## Workaround carried before the patch (retired)

The `render-state-generic-null-*` emission repairs respelled the five render sites to `std::nullopt`. They also
respelled the `??` fallback's null arm to `std::nullopt` with the present arm wrapped in `std::optional{...}`.
Each writes exactly what the concrete-receiver lowering of the same statement writes, and what every emitted
reader tests for. Three were retired when the compiler patch below made the same output without them; two remain (see below).

## Carried compiler patch (supersedes the render repairs)

[`compiler-generic-receiver-null-write.patch`](../../source-patches/compiler-generic-receiver-null-write.patch) gives a property write
through a type-parameter receiver its constraint's member type, but only where no other target evidence
exists. The written value is then lowered exactly as through a concrete receiver. Its fixtures are in
`cppGenericReceiverNullWrite.test.ts`.

Under focused render regeneration, three of the five `render-state-generic-null-*` repairs became mechanically
obsolete and are retired: the `binding`, `renderAdaptHook` and `registryMiss` writes in
`initializeRenderStateRuntime`, including the crash. The two `displayObjectClipHooks` writes in
`initializeRenderStateFields` remain repaired. Their receiver is `state: EntityConstruction<State>`, a mapped type
over the type parameter, written through a structural-row `row_set`; the patch deliberately does not reach that
shape. Reaching it would mean resolving mapped types over type parameters, which is a larger change than this
defect needs.

The patch also leaves an unused anonymous callable struct, templated on `<Runtime, Registries>`, in
`render_state.hpp`. It is the `registryMiss` intersection type spelled in the template's scope once the member type
is resolved there. It compiles and adds no behavior.

## Inventory for owners: same symptom through other receiver shapes, not changed by this patch

The patch covers a receiver whose type IS a type parameter (`runtime: R`). At flight a627849, sixteen other
generated writes put `nullptr` into a member stored as `std::optional<...>`, so they have the same
engaged-null symptom. Their receivers are not bare type parameters, though, so this patch does not change
them. Each needs its own reproduction before a fix is chosen. None was edited.

| Package | Header | Write | Receiver shape |
| --- | --- | --- | --- |
| effects-wgpu | `wgpu_effect_state.hpp` | `out->scene_target`, `out->velocity_texture` | `EntityConstruction<Ref<WgpuEffectState>>` |
| interaction | `focus_manager.hpp` | `out->focused` | `EntityConstruction<Ref<FocusManager<N>>>` in a template over `N` |
| interaction | `interaction_manager.hpp` | `out->cursor_target`, `out->dispatch_layers` | `EntityConstruction<Ref<InteractionManager<N>>>` |
| interaction | `interaction_manager.hpp` | `state->last_click_target`, `last_pointer_click_interaction_state`, `last_pointer_click_target` | `Ref<InteractionPointerState<N>>` |
| interaction | `node_interaction_state.hpp` | `out->cursor`, `out->hit_area` | `EntityConstruction<Ref<NodeInteractionState>>` |
| picking | `pick_scene3_d.hpp` | `out->node` | `EntityConstruction<Ref<Scene3DHit>>` |
| scene2d-formats | `rive_import_registry.hpp` | `out->skeleton` | `EntityConstruction<Ref<RiveArtboardImportContext>>` |
| scene3d-resources | `resolve_scene3_dresources.hpp` | `ref->failure` | `Ref<ImageResourceReference>` |
| skeleton3d | `skeleton3d.hpp` | `skeleton->names` | `Ref<Skeleton3D>` |
| textlayout | `rich_text_content.hpp` | `runtime->rich_text_content` | `Ref<RichTextRuntime>` |
| texture | `video_texture.hpp` | `texture->source` | `Ref<Texture2D>` |

Not every member here is pointer-like optional storage (`pick_scene3_d.hpp`'s `node` is a bare
`NodeAny`, where `nullptr` is the faithful null). Each row is a candidate for its owner, not a confirmed
defect.
