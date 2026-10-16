#pragma once

#include <flight/types/render_state.hpp>

namespace flight::render {

// The builder seed still has the pre-a627849 render implementation, which includes removed
// Renderer types before explain_scene_2d_coverage.hpp can be parsed. Keep this focused test at the
// live module boundary: the real accessor has the same owner read, while the unrelated stale
// implementation remains outside this override's test graph.
inline flight::Ref<flight::types::RenderStateRuntime> get_render_state_runtime(
    flight::Ref<flight::types::RenderState> state) {
  return flight::types::read_render_state_runtime_owner(state);
}

}  // namespace flight::render
