#include <flight/render/render_state.hpp>

#include <functional>
#include <iostream>
#include <optional>
#include <type_traits>

// Native port of @flighthq/render's renderState.test.ts at flight a627849.
//
// One upstream assertion is not representable and is not claimed: `getColorAdjustmentUnsupportedGuard(state)`
// is checked with `toBe(guard)`. The slot stores a std::function, which carries no identity, so the case below
// proves the slot returns a callable that reaches the stored guard, not that it is the same function object.
// See docs/compiler-requests/local-handler-callable-identity.md.

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL " << message << '\n';
    ++failures;
  }
}

using flight::render::RenderStateInitialization;

flight::Ref<RenderStateInitialization> initialization() {
  return flight::make_ref<RenderStateInitialization>(RenderStateInitialization{});
}

void test_create_render_state_defaults() {
  const auto state = flight::render::create_render_state();
  check(state->allow_smoothing == true, "createRenderState: allowSmoothing defaults to true");
  check(state->current_clip_depth == 0.0, "createRenderState: currentClipDepth defaults to 0");
  check(state->pixel_ratio == 1.0, "createRenderState: pixelRatio defaults to 1");
  check(state->render_alpha == 1.0, "createRenderState: renderAlpha defaults to 1");
  check(state->render_blend_mode == flight::types::blend_mode->normal, "createRenderState: renderBlendMode defaults to Normal");
  check(!state->display_object_clip_hooks.has_value(), "createRenderState: displayObjectClipHooks defaults to null");
  check(state->round_pixels == false, "createRenderState: roundPixels defaults to false");
}

void check_fresh_runtime(const flight::Ref<flight::types::RenderStateRuntime>& runtime, const char* message) {
  const bool machinery = runtime->current_frame_id == 0.0 && runtime->renderer_map_id == 0.0 && runtime->temp_stack.empty();
  const bool registries = runtime->registries->node_renderers.size() == 0 && !runtime->registries->stroke_tessellator.has_value() &&
                          std::holds_alternative<flight::Undefined>(runtime->registries->color_adjustments);
  check(machinery && registries, message);
}

void test_create_render_state_runtime_attached() {
  const auto state = flight::render::create_render_state();
  // `registries` is exactly { nodeRenderers: new Map(), strokeTessellator: null }: present and empty, present and
  // null, and colorAdjustments absent.
  check_fresh_runtime(flight::render::get_render_state_runtime(state),
                      "createRenderState attaches a render state runtime with the machinery fields");
}

void test_create_render_state_predefined() {
  auto base = initialization();
  base->allow_smoothing = false;
  base->pixel_ratio = 5.0;
  base->render_alpha = 0.5;
  base->render_blend_mode = flight::types::blend_mode->multiply;
  base->round_pixels = true;
  const auto state = flight::render::create_render_state(base);
  check(state->allow_smoothing == false && state->pixel_ratio == 5.0 && state->render_alpha == 0.5 &&
            state->render_blend_mode == flight::types::blend_mode->multiply && state->round_pixels == true,
        "createRenderState allows pre-defined values");
  // Fields the initialization leaves out still take their defaults.
  check(state->current_clip_depth == 0.0 && !state->display_object_clip_hooks.has_value(),
        "createRenderState defaults the fields a partial initialization omits");
}

void test_create_render_state_new_object() {
  const auto base = initialization();
  const auto state = flight::render::create_render_state(base);
  // A RenderState is a different owner from the initialization object it was built from.
  check(static_cast<const void*>(state.get()) != static_cast<const void*>(base.get()),
        "createRenderState returns a new object");
}

void test_create_render_state_runtime() {
  check_fresh_runtime(flight::render::create_render_state_runtime(), "createRenderStateRuntime initializes the machinery fields");
  const auto a = flight::render::create_render_state_runtime();
  const auto b = flight::render::create_render_state_runtime();
  check(a != b, "createRenderStateRuntime returns a distinct runtime each call");
  a->registries->node_renderers.set(flight::String("probe"), flight::make_ref<flight::types::NodeRenderer>());
  check(b->registries->node_renderers.size() == 0, "createRenderStateRuntime gives each runtime its own nodeRenderers map");
}

void test_destroy_render_state() {
  const auto state = flight::render::create_render_state();
  const auto runtime = flight::render::get_render_state_runtime(state);
  runtime->temp_stack.push(flight::types::NodeAny{});
  runtime->registries->effect_padding_resolvers = flight::Map<flight::String, flight::types::EffectPaddingResolver>{};
  flight::render::destroy_render_state(state);
  check(runtime->temp_stack.empty(), "destroyRenderState clears the traversal stack");
  check(!runtime->registries->effect_padding_resolvers.has_value(), "destroyRenderState clears the effect padding resolvers");
}

void test_get_color_adjustment_unsupported_guard() {
  using Guard = std::function<void(flight::Ref<flight::types::RenderState>, flight::types::NodeAny)>;
  const auto state = flight::render::create_render_state();
  const auto runtime = flight::render::get_render_state_runtime(state);
  check(!flight::render::get_color_adjustment_unsupported_guard(state).has_value(), "the guard is null before one is bound");

  int calls = 0;
  runtime->registries->color_adjustment_unsupported_guard =
      Guard([&](flight::Ref<flight::types::RenderState>, flight::types::NodeAny) { ++calls; });
  const auto bound = flight::render::get_color_adjustment_unsupported_guard(state);
  if (bound.has_value()) (*bound)(state, flight::types::NodeAny{});
  check(bound.has_value() && calls == 1, "a bound guard resolves to a callable that reaches the stored guard");

  runtime->registries->color_adjustment_unsupported_guard = flight::undefined;
  check(!flight::render::get_color_adjustment_unsupported_guard(state).has_value(), "an undefined guard slot resolves to null");
  runtime->registries->color_adjustment_unsupported_guard = flight::null;
  check(!flight::render::get_color_adjustment_unsupported_guard(state).has_value(), "a null guard slot resolves to null");
}

void test_get_render_state_runtime() {
  const auto state = flight::render::create_render_state();
  check(state->entity_runtime_key.has_value() &&
            flight::render::get_render_state_runtime(state) == state->entity_runtime_key.value(),
        "getRenderStateRuntime returns the runtime attached under EntityRuntimeKey");
  check(flight::render::get_render_state_runtime(state) == flight::render::get_render_state_runtime(state),
        "getRenderStateRuntime returns the same runtime on every read");
}

// Upstream: `typeof initializeRenderState === 'function'`, the construction initializer of createRenderState.
static_assert(std::is_invocable_v<decltype(&flight::render::initialize_render_state),
                                  flight::types::EntityConstruction<flight::Ref<flight::types::RenderState>>,
                                  std::optional<flight::Ref<RenderStateInitialization>>>);

} // namespace

int main() {
  test_create_render_state_defaults();
  test_create_render_state_runtime_attached();
  test_create_render_state_predefined();
  test_create_render_state_new_object();
  test_create_render_state_runtime();
  test_destroy_render_state();
  test_get_color_adjustment_unsupported_guard();
  test_get_render_state_runtime();
  return failures == 0 ? 0 : 1;
}
