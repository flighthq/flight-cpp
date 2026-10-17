// Native glyph-capability binding for upstream glTextLabel.ts. Layout, revisions,
// original owners, standard material/texture realization and quad packets are retained.
// PARTIAL: the CanvasHost overloads remain unavailable on this native profile.
#pragma once
#include <flight/render/gl_scene2d_bindings.hpp>
#include <flight/types/render_proxy_2d.hpp>
#include <flight/text/text_label.hpp>
#include <flight/bitmap/bitmap.hpp>
#include <flight/scene2d_gl/gl_quad_batch_writer.hpp>
#include <flight/render_gl/gl_quad_material_registry.hpp>
#include <flight/render/scene2d_working_color_space.hpp>
#include <flight/types/glyph_source.hpp>
#include <map>
#include <tuple>
#include <cmath>
namespace flight::scene2d_gl {
class NativeGlyphTextProvider {
 public:
  explicit NativeGlyphTextProvider(flight::Ref<flight::types::HostGlyphRasterizerCapability> host) : host_(std::move(host)) {
    if (!host_ || !host_->rasterize) throw std::invalid_argument("TextLabel requires a glyph rasterizer capability");
  }
  flight::Ref<flight::types::GlyphRasterizedBitmap> glyph(double codepoint, flight::Ref<flight::types::TextFormat> format, double ratio = 1) {
    auto family = format->font.value_or(flight::String("sans-serif"));
    auto style = format->italic.value_or(false) ? flight::String("italic") : flight::String("normal");
    const double size = format->size.value_or(12) * ratio;
    const bool bold = format->bold.value_or(false);
    const Key key{family.to_utf8(), style.to_utf8(), size, bold, codepoint};
    if (auto found = glyphs_.find(key); found != glyphs_.end()) return found->second;
    auto options = flight::make_ref<flight::types::GlyphRasterizeOptions>();
    options->font_family = family; options->font_size = size; options->font_style = style;
    options->font_weight = bold ? 700.0 : 400.0;
    auto result = host_->rasterize(codepoint, options);
    if (!result) throw std::runtime_error("TextLabel glyph rasterizer cannot supply code point " + std::to_string(codepoint));
    glyphs_.emplace(key, *result);
    return *result;
  }
  double measure(flight::String text, flight::Ref<flight::types::TextFormat> format) {
    double width = 0;
    for (double i = 0; i < static_cast<double>(text.length());) {
      const double cp = text.code_point_at(i).value();
      width += glyph(cp, format)->advance;
      i += cp > 65535 ? 2 : 1;
    }
    return width;
  }
 private:
  using Key = std::tuple<std::string, std::string, double, bool, double>;
  flight::Ref<flight::types::HostGlyphRasterizerCapability> host_;
  std::map<Key, flight::Ref<flight::types::GlyphRasterizedBitmap>> glyphs_;
};
struct NativeGlTextLabelData : flight::ReferenceEnabled {
  std::optional<flight::Ref<flight::types::Bitmap>> bitmap;
  double last_content_id = -1, last_pixel_ratio = 0, log_w = 0, log_h = 0;
};
inline auto get_native_gl_text_label_data(flight::ErasedRef value) {
  auto data = value.as<NativeGlTextLabelData>();
  if (!data) throw std::logic_error("TextLabel renderer-data owner mismatch");
  return data;
}
inline void release_native_text_bitmap(flight::Ref<flight::types::GlRenderState> state, flight::Ref<NativeGlTextLabelData> data) {
  if (!data->bitmap) return;
  auto context = flight::render_gl::get_gl_render_state_runtime(state)->context;
  const flight::types::TextureSourceValue source{std::in_place_type<flight::Ref<flight::types::Bitmap>>, *data->bitmap};
  auto release = [&](auto& cache) {
    if (auto entry = cache.get(source)) { state->gl.delete_texture((*entry)->texture); cache.erase(source); }
  };
  release(context->texture_source_premultiplied_texture_cache);
  release(context->texture_source_premultiplied_srgb_texture_cache);
  release(context->texture_source_straight_texture_cache);
  release(context->texture_source_straight_srgb_texture_cache);
  data->bitmap.reset();
}
inline void draw_native_gl_text_label(flight::Ref<flight::types::GlRenderState> state,
    flight::Ref<flight::types::RenderProxy2D> proxy, const std::shared_ptr<NativeGlyphTextProvider>& provider) {
  auto label = proxy->source.shared_exact_object<flight::types::TextLabel>();
  if (!label) throw std::logic_error("GL TextLabel source owner mismatch");
  if (label->data->text.length() == 0 || !proxy->renderer_data) return;
  using Material = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Material>>>>;
  std::optional<Material> material;
  if (proxy->material) material = Material(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Material2D>>>>(*proxy->material));
  auto renderer = flight::render_gl::resolve_gl_quad_material_renderer(state, material);
  if (!renderer) return;
  auto data = get_native_gl_text_label_data(*proxy->renderer_data);
  auto label_runtime = flight::text::get_text_label_runtime(label);
  const double version = label_runtime->local_content_id, ratio = state->pixel_ratio;
  if (version != data->last_content_id || ratio != data->last_pixel_ratio) {
    flight::types::TextMeasureFunction measure = [provider](auto text, auto format) { return provider->measure(text, format); };
    auto params = label_runtime->build_text_layout_params(label, measure);
    auto layout = flight::textlayout::get_text_layout_result(label_runtime);
    flight::textlayout::compute_text_layout(layout, params);
    data->last_content_id = version; data->last_pixel_ratio = ratio;
    data->log_w = data->log_h = 0;
    double max_x = 0, max_y = 0;
    for (auto group : layout->groups) {
      max_x = std::max(max_x, group->offset_x + group->width);
      max_y = std::max(max_y, group->offset_y + group->ascent + group->descent);
    }
    const double w = std::ceil(max_x), h = std::ceil(max_y);
    if (w <= 0 || h <= 0) return;
    const int pw = static_cast<int>(std::ceil(w * ratio)), ph = static_cast<int>(std::ceil(h * ratio));
    if (!data->bitmap || (*data->bitmap)->width != pw || (*data->bitmap)->height != ph) {
      release_native_text_bitmap(state, data);
      data->bitmap = flight::bitmap::create_bitmap(pw, ph);
    }
    auto bitmap = *data->bitmap;
    bitmap->data.fill(0);
    for (auto group : layout->groups) {
      const auto color = static_cast<std::uint32_t>(group->format->color.value_or(0x000000ff));
      double pen = group->offset_x * ratio;
      const double baseline = (group->offset_y + group->ascent * 0.815) * ratio;
      std::size_t advance_index = 0;
      for (double i = group->start_index; i < group->end_index;) {
        const double cp = label->data->text.code_point_at(i).value();
        if (cp != 9 && cp != 10 && cp != 13) {
          auto glyph = provider->glyph(cp, group->format, ratio);
          const int left = static_cast<int>(std::round(pen + glyph->bearing_x));
          const int top = static_cast<int>(std::round(baseline - glyph->bearing_y));
          for (int y = 0; y < static_cast<int>(glyph->height); ++y) for (int x = 0; x < static_cast<int>(glyph->width); ++x) {
            const int dx = left + x, dy = top + y;
            if (dx < 0 || dy < 0 || dx >= pw || dy >= ph) continue;
            const auto si = static_cast<std::size_t>((y * static_cast<int>(glyph->width) + x) * 4);
            const auto di = static_cast<std::size_t>((dy * pw + dx) * 4);
            const double alpha = glyph->pixels[si + 3] / 255.0 * (color & 255U) / 255.0;
            const double old_alpha = bitmap->data[di + 3] / 255.0;
            const double out_alpha = alpha + old_alpha * (1 - alpha);
            if (out_alpha == 0) continue;
            for (unsigned c = 0; c < 3; ++c) {
              const double channel = (color >> (24U - 8U * c)) & 255U;
              bitmap->data.set_index(static_cast<double>(di + c), (channel * alpha + bitmap->data[di + c] * old_alpha * (1 - alpha)) / out_alpha);
            }
            bitmap->data.set_index(static_cast<double>(di + 3), out_alpha * 255);
          }
        }
        pen += group->positions[advance_index++] * ratio;
        i += cp > 65535 ? 2 : 1;
      }
    }
    flight::bitmap::invalidate_bitmap(bitmap);
    data->log_w = w; data->log_h = h;
  }
  if (data->log_w <= 0 || data->log_h <= 0) return;
  auto runtime = flight::render_gl::get_gl_render_state_runtime(state);
  ensure_gl_quad_batch_shader(state);
  auto texture = flight::render_gl::bind_gl_bitmap_texture(state, *data->bitmap, std::nullopt, std::nullopt, true, flight::render::scene_2d_working_color_space);
  const bool straight = runtime->context->current_texture_realization.value()->straight_alpha;
  const double index = prepare_gl_quad_batch_write(state, texture, straight, std::nullopt, proxy->blend_mode, material, *renderer, 1);
  write_gl_quad_batch_instance(runtime->quad_batch_writer_instance_data, index * quad_batch_instance_floats, proxy->transform_2d,
      data->log_w, data->log_h, 0, 0, 1, 1, proxy->alpha);
  pack_gl_quad_batch_material_instance(state, proxy->material_data, index);
  if (std::holds_alternative<flight::Array<double>>(proxy->color_matrix)) record_gl_quad_batch_color_scale_bias(state, std::get<flight::Array<double>>(proxy->color_matrix), index);
  else if (proxy->color_scale_bias) record_gl_quad_batch_color_scale_bias(state, *proxy->color_scale_bias, index);
  else record_gl_quad_batch_color_scale_bias(state, flight::null, index);
  ++runtime->quad_batch_writer_count;
}
inline flight::Ref<flight::types::NodeRenderer> create_native_gl_text_label_renderer(std::shared_ptr<NativeGlyphTextProvider> provider) {
  auto descriptor = flight::make_ref<flight::types::NodeRenderer>();
  descriptor->format = flight::types::BatchFormat::Quad;
  auto binding = std::make_shared<flight::types::NativeGlScene2DRendererBinding>();
  binding->create_data = [](auto, auto) -> std::optional<flight::ErasedRef> { return flight::ErasedRef(flight::make_ref<NativeGlTextLabelData>()); };
  binding->destroy_data = [](auto state, auto data) { release_native_text_bitmap(state, get_native_gl_text_label_data(data)); };
  binding->submit = [provider](auto state, auto proxy) { draw_native_gl_text_label(state, proxy, provider); };
  descriptor->native_gl_scene_2d = binding;
  return descriptor;
}
} // namespace flight::scene2d_gl
