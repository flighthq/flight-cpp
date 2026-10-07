// Hand-written profile boundary for the WebGPU render-pass module. The SDL handle ABI currently
// exposes identity-only encoder handles, so pass recording cannot execute; the state queries and
// CPU-side transform helper remain available and every recording entry point fails explicitly.
#pragma once

#include <optional>

#include <flight/geometry/matrix.hpp>
#include <flight/runtime.hpp>
#include <flight/types/render_target_clear.hpp>
#include <flight/types/wgpu_render_pass.hpp>
#include <flight/types/wgpu_render_state.hpp>
#include <flight/types/wgpu_render_target.hpp>

namespace flight::render_wgpu {

inline flight::Ref<flight::types::WgpuRenderStateRuntime> get_wgpu_render_state_runtime(
    flight::Ref<flight::types::WgpuRenderState> state);

inline std::optional<flight::Ref<flight::types::WgpuRenderPass>> get_wgpu_active_render_pass(
    flight::Ref<flight::types::WgpuRenderState> state) {
  return get_wgpu_render_state_runtime(state)->pass_stack.at(-1.0);
}

inline flight::StructuralRef<flight::RowReadonly<flight::RowOf<
    flight::Ref<flight::types::WgpuRenderPassViewport>>>> get_wgpu_render_pass_viewport(
    flight::Ref<flight::types::WgpuRenderState> state) {
  const auto viewport = get_wgpu_render_state_runtime(state)->render_target_viewport;
  if (!viewport.has_value()) {
    throw flight::Error(
        flight::String("No Wgpu render pass is open — call beginWgpuRenderPass first"));
  }
  return flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<
      flight::Ref<flight::types::WgpuRenderPassViewport>>>>>(
      flight::StructuralRef<flight::RowWritable<flight::RowOf<
          flight::Ref<flight::types::WgpuRenderPassViewport>>>>(viewport.value()));
}

inline void set_wgpu_render_transform2_d(
    flight::Ref<flight::types::WgpuRenderPass> pass,
    flight::Ref<flight::types::Matrix> transform) {
  auto next = flight::geometry::create_matrix(
      std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
  flight::geometry::copy_matrix(next, transform);
  pass->state->render_transform2_d = next;
}

[[noreturn]] inline flight::Ref<flight::types::WgpuRenderPass> begin_wgpu_render_pass(
    flight::Ref<flight::types::WgpuRenderState>,
    flight::Ref<flight::types::WgpuRenderTarget>,
    std::optional<flight::Ref<flight::types::RenderTargetClear>> = std::nullopt) {
  throw flight::Error(
      flight::String("render-wgpu: render-pass encoding is unavailable in this profile"));
}

[[noreturn]] inline void end_wgpu_render_pass(
    flight::Ref<flight::types::WgpuRenderPass>) {
  throw flight::Error(
      flight::String("render-wgpu: render-pass encoding is unavailable in this profile"));
}

[[noreturn]] inline void suspend_wgpu_render_pass(
    flight::Ref<flight::types::WgpuRenderPass>) {
  throw flight::Error(
      flight::String("render-wgpu: render-pass encoding is unavailable in this profile"));
}

}  // namespace flight::render_wgpu
