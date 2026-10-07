// Hand-written override that breaks the generated render-state/device-loss include cycle. The device
// loss helpers need only the retained state/runtime types; they do not need the render-state module's
// large implementation graph.
#pragma once

#include <functional>
#include <optional>

#include <flight/host_sdl/wgpu.hpp>
#include <flight/runtime.hpp>
#include <flight/signals/signal.hpp>
#include <flight/types/wgpu_device_runtime.hpp>
#include <flight/types/wgpu_device_signals.hpp>
#include <flight/types/wgpu_render_state.hpp>

namespace flight::render_wgpu {

// Defined by wgpu_render_state.hpp when that module is present. Its generated body currently relies on
// a separate nominal-runtime repair, so this header preserves the boundary without recursively including
// the module that includes us.
inline flight::Ref<flight::types::WgpuRenderStateRuntime> get_wgpu_render_state_runtime(
    flight::Ref<flight::types::WgpuRenderState> state);

inline void dispose_wgpu_device_signals(flight::Ref<flight::types::WgpuRenderState> state) {
  get_wgpu_render_state_runtime(state)->context->signals = std::nullopt;
}

inline flight::Ref<flight::types::WgpuDeviceSignals> enable_wgpu_device_signals(
    flight::Ref<flight::types::WgpuRenderState> state) {
  auto runtime = get_wgpu_render_state_runtime(state)->context;
  if (!runtime->signals.has_value()) {
    using Listener = std::function<void(flight::host_sdl::WgpuDeviceLostInfo)>;
    runtime->signals = flight::make_ref<flight::types::WgpuDeviceSignals>(
        flight::types::WgpuDeviceSignals{
            .on_device_lost = flight::signals::create_signal<Listener>(),
        });
  }
  return runtime->signals.value();
}

inline std::optional<flight::host_sdl::WgpuDeviceLostInfo> get_wgpu_device_loss(
    flight::Ref<flight::types::WgpuRenderState> state) {
  return get_wgpu_render_state_runtime(state)->context->lost;
}

inline bool is_wgpu_device_lost(flight::Ref<flight::types::WgpuRenderState> state) {
  return get_wgpu_device_loss(state).has_value();
}

// Native WgpuDevice has no Promise-like `lost` member in this profile, so the host-specific observer
// remains a declared binding boundary rather than fabricating synchronous loss observation.
inline void observe_wgpu_device_loss(flight::Ref<flight::types::WgpuDeviceRuntime> device_runtime);

}  // namespace flight::render_wgpu
