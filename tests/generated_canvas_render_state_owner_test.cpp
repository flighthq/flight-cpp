// Native allocation/accessor identity invariants for the canvas render-state declared owner.
//
// This is the invariant the request's equivalence argument claims and the one an assertion-based rewrite
// would break: the runtime owner is written and read through DECLARED accessors rather than an asserted
// entity-runtime slot, so the read must return the very runtime that was written -- not a copy -- and the
// state and its runtime must stay two distinct allocations.
//
// It targets the CANVAS accessors, not the render twin, and therefore compiles only against a
// regeneration that carries them: read/write_canvas_render_state_runtime_owner live in
// flight/types/canvas_render_state.hpp and are emitted by the canvas render-state source patch. It must
// therefore be registered atomically with the regeneration that moves that header.
#include <flight/render/render_state.hpp>
#include <flight/types/canvas_render_state.hpp>

#include <iostream>
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

  if (failures == 0) {
    std::cout << "canvas render-state owner identity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
