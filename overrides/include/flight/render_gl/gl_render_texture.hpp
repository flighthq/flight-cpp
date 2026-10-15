// Native completion of render-gl/src/glRenderTexture.ts at the pinned Flight revision.
#pragma once

#include <functional>
#include <optional>
#include <type_traits>

#include <flight/render/render_target.hpp>
#include <flight/render_gl/gl_draw.hpp>
#include <flight/render_gl/gl_fullscreen_pass.hpp>
#include <flight/render_gl/gl_render_pass.hpp>
#include <flight/render_gl/gl_render_state_bracket.hpp>
#include <flight/render_gl/gl_render_target.hpp>
#include <flight/types/gl_render_texture.hpp>
#include <flight/types/render_texture.hpp>

namespace flight::render_gl {

using flight::types::GlRenderPass;
using flight::types::GlRenderState;
using flight::types::GlRenderTextureEntry;
using flight::types::GlRenderTextureExplanation;
using flight::types::GlTextureRealization;
using flight::types::GlTextureRenderTarget;
using flight::types::RenderTargetClear;
using flight::types::RenderTexture;
using flight::types::SamplerLike;

using RenderTextureView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<RenderTexture>>>>;
using GlTextureTargetView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<GlTextureRenderTarget>>>>;

inline flight::Ref<RenderTexture> render_texture_owner(RenderTextureView view) {
  auto owner = view.template shared_exact_object<RenderTexture>();
  if (!owner) {
    throw flight::Error(flight::String(
        "render texture operation requires a native RenderTexture owner"));
  }
  return owner;
}

inline flight::WeakMap<flight::Ref<RenderTexture>,
                       flight::Ref<GlRenderTextureEntry>>
get_gl_render_texture_entries(flight::Ref<GlRenderState> state) {
  auto& entries = get_gl_render_state_runtime(state)->context->gl_render_texture_cache;
  if (!entries.has_value()) {
    entries = flight::WeakMap<flight::Ref<RenderTexture>,
                              flight::Ref<GlRenderTextureEntry>>{};
  }
  return entries.value();
}

inline std::optional<flight::Ref<GlRenderTextureEntry>> get_entry(
    flight::Ref<GlRenderState> state, flight::Ref<RenderTexture> texture) {
  const auto& entries = get_gl_render_state_runtime(state)->context->gl_render_texture_cache;
  return entries.has_value() ? entries.value().get(texture) : std::nullopt;
}

inline std::optional<flight::Ref<GlRenderTextureEntry>> get_entry(
    flight::Ref<GlRenderState> state, RenderTextureView texture) {
  return get_entry(state, render_texture_owner(texture));
}

inline void destroy_gl_render_texture(
    flight::Ref<GlRenderState> state, flight::Ref<RenderTexture> texture) {
  auto& entries = get_gl_render_state_runtime(state)->context->gl_render_texture_cache;
  if (!entries.has_value()) return;
  auto entry = entries.value().get(texture);
  if (!entry.has_value()) return;
  destroy_gl_texture_render_target(entry.value()->target);
  entries.value().erase(texture);
}

inline void destroy_gl_render_texture(
    flight::Ref<GlRenderState> state, RenderTextureView texture) {
  destroy_gl_render_texture(state, render_texture_owner(texture));
}

inline void set_gl_render_texture_guard(
    flight::Ref<GlRenderState> state,
    std::optional<flight::types::GlRenderTextureGuard> guard) {
  auto& slot = get_gl_render_state_runtime(state)->gl_render_texture_guard;
  using Slot = std::remove_reference_t<decltype(slot)>;
  if (guard.has_value()) {
    slot = Slot{
        std::in_place_type<flight::types::GlRenderTextureGuard>, guard.value()};
  } else {
    slot = Slot{std::in_place_type<flight::Null>, flight::null};
  }
}

inline bool matches_gl_render_texture_allocation(
    flight::Ref<GlTextureRenderTarget> target,
    flight::Ref<flight::types::ResolvedRenderTargetDescriptor> descriptor) {
  const auto requested = target->requested_axes;
  if (requested->format != descriptor->format ||
      requested->color_attachments != descriptor->color_attachments ||
      requested->color_formats.size() != descriptor->color_formats.size() ||
      requested->sample_count != descriptor->sample_count ||
      requested->depth != descriptor->depth ||
      requested->color_space != descriptor->color_space) {
    return false;
  }
  for (std::size_t index = 0; index < requested->color_formats.size(); ++index) {
    if (requested->color_formats.element(index) !=
        descriptor->color_formats.element(index)) return false;
  }
  return true;
}

inline flight::Ref<GlRenderTextureEntry> ensure_entry(
    flight::Ref<GlRenderState> state, flight::Ref<RenderTexture> texture) {
  const auto descriptor = texture->source;
  using DescriptorView = flight::StructuralRef<flight::RowReadonly<
      flight::RowOf<flight::Ref<flight::types::RenderTarget>>>>;
  const DescriptorView descriptor_view(descriptor);
  auto entries = get_gl_render_texture_entries(state);
  auto entry = entries.get(texture);
  const auto resolved = flight::render::resolve_render_target_descriptor(descriptor_view);
  if (!entry.has_value()) {
    entry = flight::make_ref<GlRenderTextureEntry>(GlRenderTextureEntry{
        .status = flight::String("unrendered"),
        .target = create_gl_texture_render_target(state, descriptor_view)});
    entries.set(texture, entry.value());
    return entry.value();
  }
  if (matches_gl_render_texture_allocation(entry.value()->target, resolved)) {
    resize_gl_texture_render_target(
        state, entry.value()->target, descriptor->width, descriptor->height);
    entry.value()->target->requested_axes =
        flight::make_ref<flight::types::RenderTargetAxes>(
            flight::types::RenderTargetAxes{
                .width = resolved->width, .height = resolved->height,
                .format = resolved->format,
                .color_attachments = resolved->color_attachments,
                .color_formats = resolved->color_formats.slice(),
                .sample_count = resolved->sample_count,
                .depth = resolved->depth,
                .color_space = resolved->color_space});
  } else {
    destroy_gl_texture_render_target(entry.value()->target);
    entry.value()->target = create_gl_texture_render_target(state, descriptor_view);
    entry.value()->status = flight::String("unrendered");
  }
  return entry.value();
}

inline flight::Ref<GlRenderTextureEntry> ensure_entry(
    flight::Ref<GlRenderState> state, RenderTextureView texture) {
  return ensure_entry(state, render_texture_owner(texture));
}

template <typename T>
inline T write_gl_render_texture_target(
    flight::Ref<GlRenderState> state, flight::Ref<RenderTexture> texture,
    std::function<T(flight::Ref<GlTextureRenderTarget>)> callback) {
  const auto entry = ensure_entry(state, texture);
  const auto previous_status = entry->status;
  entry->status = flight::String("writing");
  bool rendered = false;
  if constexpr (std::is_void_v<T>) {
    try {
      callback(entry->target);
      rendered = true;
    } catch (...) {
      entry->status = previous_status == flight::String("writing")
          ? flight::String("writing") : flight::String("unrendered");
      throw;
    }
    entry->status = flight::String("ready");
    texture->color_space = entry->target->color_space;
    texture->version = flight::unsigned_right_shift(texture->version + 1.0, 0.0);
  } else {
    try {
      auto result = callback(entry->target);
      rendered = true;
      entry->status = flight::String("ready");
      texture->color_space = entry->target->color_space;
      texture->version = flight::unsigned_right_shift(texture->version + 1.0, 0.0);
      return result;
    } catch (...) {
      if (!rendered) {
        entry->status = previous_status == flight::String("writing")
            ? flight::String("writing") : flight::String("unrendered");
      }
      throw;
    }
  }
}

inline void clear_gl_render_texture(
    flight::Ref<GlRenderState> state, flight::Ref<RenderTexture> texture) {
  write_gl_render_texture_target<void>(
      state, texture, [state](flight::Ref<GlTextureRenderTarget> target) {
        with_gl_render_state<void>(state, [target]() {
          auto clear = flight::make_ref<RenderTargetClear>(RenderTargetClear{
              .color = flight::Array<double>{0.0, 0.0, 0.0, 0.0}});
          clear_gl_render_target(GlTextureTargetView(target),
                                 RenderTargetClearView(clear));
        });
      });
}

inline void render_into_gl_render_texture(
    flight::Ref<GlRenderState> state, flight::Ref<RenderTexture> texture,
    std::function<void(flight::Ref<GlRenderPass>)> callback,
    RenderTargetClearView clear) {
  write_gl_render_texture_target<void>(
      state, texture,
      [state, callback, clear](flight::Ref<GlTextureRenderTarget> target) {
        with_gl_render_state<void>(state, [state, callback, clear, target]() {
          const auto pass = begin_gl_render_pass(state, target, clear);
          try {
            callback(pass);
          } catch (...) {
            end_gl_render_pass(pass);
            throw;
          }
          end_gl_render_pass(pass);
        });
      });
}

inline flight::Ref<GlRenderTextureExplanation> explain_gl_render_texture(
    flight::Ref<GlRenderState> state, RenderTextureView texture) {
  const auto owner = render_texture_owner(texture);
  const auto entry = get_entry(state, owner);
  return flight::make_ref<GlRenderTextureExplanation>(GlRenderTextureExplanation{
      .height = entry.has_value() ? entry.value()->target->height
                                  : owner->source->height,
      .status = entry.has_value() ? entry.value()->status
                                  : flight::String("unrendered"),
      .width = entry.has_value() ? entry.value()->target->width
                                 : owner->source->width});
}

inline flight::types::TextureColorSpace get_gl_render_texture_color_space(
    flight::Ref<GlRenderState> state, RenderTextureView texture) {
  const auto entry = get_entry(state, texture);
  return entry.has_value() ? entry.value()->target->color_space
                           : render_texture_owner(texture)->color_space;
}

inline void invalidate_gl_render_texture(
    flight::Ref<GlRenderState> state, RenderTextureView texture,
    std::optional<flight::String> status = std::nullopt) {
  const auto entry = get_entry(state, texture);
  if (entry.has_value()) {
    entry.value()->status = status.value_or(flight::String("unrendered"));
  }
}

inline void notify_gl_render_texture_guard(
    flight::Ref<GlRenderState> state, RenderTextureView texture) {
  auto& guard = get_gl_render_state_runtime(state)->gl_render_texture_guard;
  if (auto* callback = std::get_if<flight::types::GlRenderTextureGuard>(&guard)) {
    (*callback)(state, texture, explain_gl_render_texture(state, texture));
  }
}

inline std::optional<flight::host_sdl::WebGlTexture> bind_gl_render_texture(
    flight::Ref<GlRenderState> state, RenderTextureView texture,
    std::optional<std::optional<SamplerLike>> sampler = std::nullopt) {
  const auto entry = get_entry(state, texture);
  if (!entry.has_value() || entry.value()->status != flight::String("ready")) {
    notify_gl_render_texture_guard(state, texture);
    bind_gl_texture_realization(state, std::nullopt);
    return std::nullopt;
  }
  const auto handle = entry.value()->target->texture;
  const auto runtime = get_gl_render_state_runtime(state);
  bind_gl_texture_realization(
      state, flight::make_ref<GlTextureRealization>(GlTextureRealization{
                 .straight_alpha = false, .texture = handle}));
  const auto owner = render_texture_owner(texture);
  const std::optional<SamplerLike> selected = sampler.has_value()
      ? sampler.value() : std::optional<SamplerLike>{owner->sampler};
  apply_gl_sampler_state(state, runtime, handle, selected, std::nullopt);
  return handle;
}

inline std::optional<GlTextureTargetView> get_gl_render_texture_target(
    flight::Ref<GlRenderState> state, RenderTextureView texture) {
  const auto entry = get_entry(state, texture);
  if (entry.has_value() && entry.value()->status == flight::String("ready")) {
    return GlTextureTargetView(entry.value()->target);
  }
  notify_gl_render_texture_guard(state, texture);
  return std::nullopt;
}

inline bool is_gl_render_texture_ready(
    flight::Ref<GlRenderState> state, RenderTextureView texture) {
  const auto entry = get_entry(state, texture);
  const bool ready = entry.has_value() &&
                     entry.value()->status == flight::String("ready");
  if (!ready) notify_gl_render_texture_guard(state, texture);
  return ready;
}

}  // namespace flight::render_gl
