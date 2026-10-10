// Native completion of scene3d-gl/src/glEnvironmentCube.ts at the pinned Flight revision.
#pragma once

#include <cmath>
#include <optional>
#include <variant>

#include <flight/render_gl/gl_texture_upload.hpp>
#include <flight/types/environment.hpp>
#include <flight/types/gl_scene3_druntime.hpp>
#include <flight/types/texture.hpp>
#include <flight/types/texture_source_kind.hpp>

namespace flight::scene3d_gl {

inline flight::Ref<flight::types::GlScene3DRuntime> get_gl_scene3_druntime(
    flight::Ref<flight::types::GlRenderState> state);

using flight::types::GlRenderState;
using flight::types::GlScene3DRuntime;
using flight::types::TextureCube;
using flight::types::TextureSourceValue;

inline double gl_environment_source_version(const TextureSourceValue& source) {
  return std::visit([](const auto& value) { return value->version; }, source);
}

inline flight::String gl_environment_source_kind(
    const TextureSourceValue& source) {
  return std::visit(
      [](const auto& value) -> flight::String { return value->kind; }, source);
}

inline void destroy_gl_environment_source_cube(
    flight::Ref<GlRenderState> state) {
  const auto runtime = get_gl_scene3_druntime(state);
  if (!runtime->environment_source_cube.has_value()) return;
  state->gl.delete_texture(runtime->environment_source_cube.value());
  runtime->environment_source_cube = std::nullopt;
  runtime->environment_source_cube_color_space = flight::String("linear");
  runtime->environment_source_cube_face_versions = {};
  runtime->environment_source_revision = flight::unsigned_right_shift(
      runtime->environment_source_revision + 1.0, 0.0);
  runtime->environment_source_texture = std::nullopt;
  runtime->environment_source_texture_version = -1.0;
}

inline std::uint32_t get_gl_cube_face_target(
    flight::host_sdl::WebGl2Context gl, double face) {
  return gl.texture_cube_map_positive_x + static_cast<std::uint32_t>(face);
}

inline flight::Array<double> get_gl_environment_source_cube_face_versions(
    flight::Ref<TextureCube> cube) {
  flight::Array<double> versions;
  for (const auto& source : cube->sources) {
    versions.push(source.has_value()
        ? gl_environment_source_version(source.value()) : -1.0);
  }
  return versions;
}

inline bool has_gl_cube_face_pixels(flight::Ref<TextureCube> cube) {
  if (cube->sources.size() != 6) return false;
  for (const auto& image : cube->sources) {
    if (!image.has_value()) return false;
    const auto kind = gl_environment_source_kind(image.value());
    if (kind != flight::types::image_texture_source_kind &&
        kind != flight::types::bitmap_texture_source_kind) return false;
  }
  return true;
}

inline bool is_gl_environment_source_cube_current(
    flight::Ref<GlScene3DRuntime> runtime, flight::Ref<TextureCube> cube) {
  if (!runtime->environment_source_cube.has_value() ||
      !runtime->environment_source_texture.has_value() ||
      runtime->environment_source_texture_version != cube->version ||
      runtime->environment_source_cube_face_versions.size() != 6) {
    return false;
  }
  const auto* tracked = std::get_if<flight::Ref<TextureCube>>(
      &runtime->environment_source_texture.value());
  if (!tracked || *tracked != cube) return false;
  for (std::size_t face = 0; face < cube->sources.size(); ++face) {
    const auto& source = cube->sources.element(face);
    if (!source.has_value() ||
        runtime->environment_source_cube_face_versions.element(face) !=
            gl_environment_source_version(source.value())) return false;
  }
  return true;
}

inline void upload_gl_environment_image(
    flight::host_sdl::WebGl2Context gl, std::uint32_t target,
    const TextureSourceValue& image,
    std::optional<std::uint32_t> internal_format = std::nullopt) {
  const auto format = internal_format.value_or(gl.rgba);
  if (const auto* bitmap =
          std::get_if<flight::Ref<flight::types::Bitmap>>(&image)) {
    flight::render_gl::upload_gl_texture_data(
        gl, target, (*bitmap)->width, (*bitmap)->height,
        (*bitmap)->data, format);
    return;
  }
  if (const auto* resource =
          std::get_if<flight::Ref<flight::types::ImageResource>>(&image)) {
    using ImageView = flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::ImageResource>>>>;
    flight::render_gl::upload_gl_texture_image_resource(
        gl, target, ImageView(*resource), format);
    return;
  }
  throw flight::Error(flight::String(
      "environment cube faces must be bitmap or image resources"));
}

using EnvironmentView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::Environment>>>>;

inline std::optional<flight::host_sdl::WebGlTexture>
ensure_gl_environment_source_cube(
    flight::Ref<GlRenderState> state, EnvironmentView environment) {
  const auto runtime = get_gl_scene3_druntime(state);
  const auto candidate = environment->environment;
  if (!candidate.has_value()) {
    if (runtime->environment_source_cube.has_value())
      destroy_gl_environment_source_cube(state);
    return std::nullopt;
  }
  const auto* selected =
      std::get_if<flight::Ref<TextureCube>>(&candidate.value());
  if (!selected || !has_gl_cube_face_pixels(*selected)) {
    if (runtime->environment_source_cube.has_value())
      destroy_gl_environment_source_cube(state);
    return std::nullopt;
  }
  const auto cube = *selected;
  if (is_gl_environment_source_cube_current(runtime, cube)) {
    return runtime->environment_source_cube;
  }
  if (runtime->environment_source_cube.has_value()) {
    destroy_gl_environment_source_cube(state);
  }
  const auto gl = state->gl;
  const auto internal_format = cube->color_space == flight::String("srgb")
      ? gl.srgb8_alpha8 : gl.rgba;
  const auto texture = gl.create_texture();
  gl.bind_texture(gl.texture_cube_map, texture);
  for (std::size_t face = 0; face < 6; ++face) {
    upload_gl_environment_image(
        gl, get_gl_cube_face_target(gl, static_cast<double>(face)),
        cube->sources.element(face).value(), internal_format);
  }
  gl.tex_parameteri(gl.texture_cube_map, gl.texture_min_filter, gl.linear);
  gl.tex_parameteri(gl.texture_cube_map, gl.texture_mag_filter, gl.linear);
  gl.tex_parameteri(gl.texture_cube_map, gl.texture_wrap_s, gl.clamp_to_edge);
  gl.tex_parameteri(gl.texture_cube_map, gl.texture_wrap_t, gl.clamp_to_edge);
  gl.tex_parameteri(gl.texture_cube_map, gl.texture_wrap_r, gl.clamp_to_edge);
  gl.bind_texture(gl.texture_cube_map, nullptr);
  runtime->environment_source_cube = texture;
  runtime->environment_source_cube_color_space = cube->color_space;
  runtime->environment_source_cube_face_versions =
      get_gl_environment_source_cube_face_versions(cube);
  runtime->environment_source_revision = flight::unsigned_right_shift(
      runtime->environment_source_revision + 1.0, 0.0);
  runtime->environment_source_texture = flight::types::Texture{
      std::in_place_type<flight::Ref<TextureCube>>, cube};
  runtime->environment_source_texture_version = cube->version;
  return texture;
}

inline bool update_gl_environment_cube_face(
    flight::Ref<GlRenderState> state, double face,
    const TextureSourceValue& image) {
  const auto runtime = get_gl_scene3_druntime(state);
  if (!runtime->environment_source_cube.has_value() ||
      !std::isfinite(face) || std::floor(face) != face ||
      face < 0.0 || face >= 6.0) return false;
  const auto gl = state->gl;
  gl.bind_texture(gl.texture_cube_map,
                  runtime->environment_source_cube.value());
  upload_gl_environment_image(
      gl, get_gl_cube_face_target(gl, face), image,
      runtime->environment_source_cube_color_space == flight::String("srgb")
          ? gl.srgb8_alpha8 : gl.rgba);
  gl.bind_texture(gl.texture_cube_map, nullptr);

  auto versions = runtime->environment_source_cube_face_versions.slice();
  double version = -1.0;
  if (runtime->environment_source_texture.has_value()) {
    if (const auto* tracked = std::get_if<flight::Ref<TextureCube>>(
            &runtime->environment_source_texture.value())) {
      const auto& tracked_source = (*tracked)->sources.element(
          static_cast<std::size_t>(face));
      if (tracked_source.has_value() && tracked_source.value() == image) {
        version = gl_environment_source_version(image);
      }
    }
  }
  versions.element(static_cast<std::size_t>(face)) = version;
  runtime->environment_source_cube_face_versions = versions;
  runtime->environment_source_revision = flight::unsigned_right_shift(
      runtime->environment_source_revision + 1.0, 0.0);
  return true;
}

}  // namespace flight::scene3d_gl
