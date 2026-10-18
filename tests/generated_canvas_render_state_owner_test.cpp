// Native allocation/accessor identity invariants for the canvas render-state declared owner.
//
// This is the invariant the request's equivalence argument claims and the one an assertion-based rewrite
// would break: the runtime owner is written and read through DECLARED accessors rather than an asserted
// entity-runtime slot, so the read must return the very runtime that was written -- not a copy -- and the
// state and its runtime must stay two distinct allocations.
//
// The whole original Canvas module must also compile and expose its actual runtime getter and helpers.
// Its public state constructor and private registry builder remain refused; neither is supplied here.
#include <flight/render/render_state.hpp>
#include <flight/scene2d_canvas/canvas_render_state.hpp>
#include <flight/types/canvas_render_state.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <optional>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

using CanvasState = flight::Ref<flight::types::CanvasRenderState>;
using CanvasRuntime = flight::Ref<flight::types::CanvasRenderStateRuntime>;
// The derived construction is a writable ROW over the state, not the state reference itself -- that is
// what makes the owner write a declared operation rather than a cast.
using CanvasConstruction = flight::types::EntityConstruction<CanvasState>;

CanvasConstruction construct() {
  return flight::render::create_derived_render_state_construction<CanvasState>(std::nullopt);
}

CanvasRuntime allocateRuntime() {
  return flight::make_ref<flight::types::CanvasRenderStateRuntime>(flight::types::CanvasRenderStateRuntime{});
}

} // namespace

int main() {
  // One allocation each: the state construction and its runtime are distinct owners, not one object.
  const auto state = construct();
  const auto runtime = allocateRuntime();
  check(state.shared_owner().get() != static_cast<const void*>(runtime.get()),
        "a canvas state and its runtime are two distinct allocations");

  // Non-vacuity: before the write the slot is genuinely unset, so the accessor reads real storage rather
  // than returning a constant. If it invented an owner, this read would not throw.
  bool threwBeforeWrite = false;
  try {
    (void)flight::types::read_canvas_render_state_runtime_owner(state);
  } catch (...) {
    threwBeforeWrite = true;
  }
  check(threwBeforeWrite, "reading an owner that was never written throws rather than returning an invented one");

  // The declared write, then the declared read: the read must return the very runtime that was written.
  flight::types::write_canvas_render_state_runtime_owner(state, runtime);
  const auto readBack = flight::types::read_canvas_render_state_runtime_owner(state);
  check(readBack.get() == runtime.get(),
        "read_canvas_render_state_runtime_owner returns the exact runtime write_canvas_render_state_runtime_owner stored, not a copy");
  check(static_cast<const void*>(readBack.get()) != state.shared_owner().get(),
        "the runtime the accessor returns is not the state owner itself");

  // A second state keeps its own runtime, so the attachment is per-owner rather than shared storage.
  const auto other = construct();
  const auto otherRuntime = allocateRuntime();
  check(other.shared_owner().get() != state.shared_owner().get(),
        "each canvas state is its own allocation");
  flight::types::write_canvas_render_state_runtime_owner(other, otherRuntime);
  check(flight::types::read_canvas_render_state_runtime_owner(other).get() == otherRuntime.get(),
        "each canvas state keeps the runtime written to it");
  check(flight::types::read_canvas_render_state_runtime_owner(state).get() == runtime.get(),
        "writing a runtime to a second state does not disturb the first");

  const auto stateRef = flight::structural_ref_cast<CanvasState>(state);
  check(stateRef.get() == state.shared_owner().get(), "the public helper receives the original state owner");
  auto getter = &flight::scene2d_canvas::get_canvas_render_state_runtime;
  check(getter(stateRef).get() == runtime.get(), "the original public getter returns the attached runtime");
  const auto runtimeAlias = getter(stateRef);
  runtimeAlias->current_alpha = 0.25;
  check(runtime->current_alpha == 0.25, "the public getter retains writable runtime identity");

  flight::types::write_canvas_render_state_runtime_owner(state, otherRuntime);
  check(getter(stateRef).get() == otherRuntime.get(), "the getter reads a replacement attachment on each call");
  check(runtimeAlias.get() == runtime.get(), "replacing the attachment preserves a previously returned owner");
  flight::types::write_canvas_render_state_runtime_owner(state, runtime);
  auto resolvers = flight::make_ref<flight::types::CanvasTextureResolvers>();
  runtime->canvas_texture_resolvers = resolvers;
  check(flight::scene2d_canvas::get_canvas_render_state_texture_resolvers(stateRef).get() == resolvers.get(),
        "the public texture resolver getter returns the exact stored owner");

  int callbacks = 0;
  bool callbackOwnersMatch = true;
  for (int ordinal : {1, 2}) {
    flight::scene2d_canvas::register_canvas_render_state_teardown(stateRef, [&, ordinal](CanvasState received) {
      callbacks = callbacks * 10 + ordinal;
      callbackOwnersMatch = callbackOwnersMatch && received.get() == stateRef.get();
    });
  }
  check(callbacks == 0 && runtime->teardowns.size() == 2, "registration appends callbacks without executing them");
  for (const auto& callback : runtime->teardowns) callback(stateRef);
  check(callbacks == 12 && callbackOwnersMatch, "stored callbacks preserve order and state identity");

  stateRef->allow_smoothing = true;
  for (const auto& filter : {"nearest", "nearest-mipmap", "linear", "Nearest", ""}) {
    const bool expected = flight::String(filter).starts_with(flight::String("nearest")) == false;
    check(flight::scene2d_canvas::resolve_canvas_texture_smoothing(stateRef, flight::String(filter)) == expected,
          "smoothing retains the exact case-sensitive nearest prefix rule");
  }
  stateRef->allow_smoothing = false;
  check(!flight::scene2d_canvas::resolve_canvas_texture_smoothing(stateRef, flight::String("linear")),
        "state policy disables texture smoothing");

  runtime->current_alpha = std::numeric_limits<double>::quiet_NaN();
  flight::scene2d_canvas::set_canvas_global_alpha(stateRef, 0.5);
  check(runtime->current_alpha == 0.5 && stateRef->context.global_alpha == 0.5,
        "the actual alpha helper writes runtime and context");
  stateRef->context.global_alpha = 0.75;
  flight::scene2d_canvas::set_canvas_global_alpha(stateRef, 0.5);
  check(stateRef->context.global_alpha == 0.75, "equal cached alpha skips the context write");
  runtime->current_alpha = 0.0;
  flight::scene2d_canvas::set_canvas_global_alpha(stateRef, -0.0);
  check(!std::signbit(runtime->current_alpha) && stateRef->context.global_alpha == 0.75,
        "signed zeros compare equal without replacing cached storage");
  flight::scene2d_canvas::set_canvas_global_alpha(stateRef, std::numeric_limits<double>::quiet_NaN());
  check(std::isnan(runtime->current_alpha) && std::isnan(stateRef->context.global_alpha),
        "NaN alpha is preserved and reaches the context");
  stateRef->context.global_alpha = 0.75;
  flight::scene2d_canvas::set_canvas_global_alpha(stateRef, std::numeric_limits<double>::quiet_NaN());
  check(std::isnan(stateRef->context.global_alpha), "NaN does not take the equal-alpha shortcut");

  runtime->image_smoothing_enabled = true;
  flight::scene2d_canvas::set_canvas_image_smoothing(stateRef, false);
  check(!runtime->image_smoothing_enabled && !stateRef->context.image_smoothing_enabled,
        "the actual image smoothing helper writes runtime and context");
  stateRef->context.image_smoothing_enabled = true;
  flight::scene2d_canvas::set_canvas_image_smoothing(stateRef, false);
  check(stateRef->context.image_smoothing_enabled, "equal cached smoothing skips the context write");

  if (failures == 0) {
    std::cout << "canvas render-state owner identity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
