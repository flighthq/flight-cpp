#pragma once

#include <flight/erased_ref.hpp>
#include <flight/types/node_2d.hpp>
#include <flight/types/node_renderer.hpp>
#include <functional>
#include <optional>

namespace flight::types {
struct GlRenderState;

// A native callable binding for the same NodeRenderer descriptor. The emitter's
// flattened RenderState/GlRenderState and RendererData owners are unrelated C++
// classes. Keep the actual backend arguments and exact data owner at dispatch;
// never copy a state/proxy into a base-shaped replacement.
struct NativeGlScene2DRendererBinding {
  std::function<std::optional<flight::ErasedRef>(flight::Ref<GlRenderState>, Node2D)> create_data;
  std::function<void(flight::Ref<GlRenderState>, flight::ErasedRef)> destroy_data;
  std::function<bool(flight::Ref<GlRenderState>, Node2D, std::optional<flight::ErasedRef>)> is_dirty;
  std::function<void(flight::Ref<GlRenderState>, flight::Ref<RenderProxy2D>)> submit;
};

} // namespace flight::types
