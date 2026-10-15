#include <flight/texture/cube_texture.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <variant>

namespace {
int failures = 0;
void check(bool value, const char* message) {
  if (value) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}
}

int main() {
  namespace texture = flight::texture;
  namespace types = flight::types;
  const auto face = flight::make_ref<types::Bitmap>();
  face->width = 64.0;
  const auto source = texture::create_cube_texture();
  source->sources.element(0.0) = types::TextureSourceValue{
      std::in_place_type<flight::Ref<types::Bitmap>>, face};
  source->color_space = flight::String("linear");
  source->flip_x = true;
  source->uv_offset->x = 0.25;
  source->uv_offset->y = 0.5;
  source->uv_scale->x = 2.0;
  source->uv_scale->y = 3.0;
  source->uv_rotation = 0.75;
  source->sampler->anisotropy = 4.0;
  source->version = 7.0;
  const auto copy = texture::clone_cube_texture(source);
  check(copy != source && copy->sampler != source->sampler &&
            copy->uv_offset != source->uv_offset && copy->uv_scale != source->uv_scale,
        "clone allocates independent cube, sampler and UV owners");
  check(&copy->sources.element(0.0) != &source->sources.element(0.0) &&
            std::get<flight::Ref<types::Bitmap>>(*copy->sources.element(0.0)) == face &&
            !copy->sources.element(1.0),
        "clone creates fresh face storage while retaining source owners and nulls");
  check(texture::equals_cube_texture(copy, source), "clone retains all compared cube state");
  copy->sources.element(0.0) = std::nullopt;
  check(source->sources.element(0.0).has_value(), "clone slot changes cannot mutate source storage");
  texture::copy_cube_texture(copy, source);
  const auto sampler = copy->sampler;
  const auto offset = copy->uv_offset;
  const auto scale = copy->uv_scale;
  const auto old_sources = copy->sources;
  texture::copy_cube_texture(copy, source);
  check(copy->sampler == sampler && copy->uv_offset == offset && copy->uv_scale == scale &&
            texture::equals_cube_texture(copy, source),
        "copy preserves destination nested owners while copying value state");
  check(&copy->sources.element(0.0) != &old_sources.element(0.0),
        "each copy gives destination fresh face-list storage");
  const auto self_sources = source->sources;
  texture::copy_cube_texture(source, source);
  check(&source->sources.element(0.0) != &self_sources.element(0.0) &&
            source->sampler->anisotropy == 4.0,
        "self-copy preserves sampler state and still replaces face-list storage");
  source->version = 4294967297.0;
  check(texture::clone_cube_texture(source)->version == 1.0, "clone normalizes version to u32");
  texture::copy_cube_texture(copy, source);
  check(copy->version == 1.0 && source->version == 4294967297.0,
        "copy normalizes the captured version without changing its source");
  source->version = 7.0;
  texture::copy_cube_texture(copy, source);
  const auto check_difference = [&](auto mutate, auto restore) {
    mutate();
    check(!texture::equals_cube_texture(source, copy), "equality observes each independent value field");
    restore();
    check(texture::equals_cube_texture(source, copy), "restoring a field restores equality");
  };
  check_difference([&] { copy->color_space = flight::String("srgb"); }, [&] { copy->color_space = source->color_space; });
  check_difference([&] { copy->flip_x = false; }, [&] { copy->flip_x = source->flip_x; });
  check_difference([&] { copy->flip_y = true; }, [&] { copy->flip_y = source->flip_y; });
  check_difference([&] { copy->uv_rotation = 0.0; }, [&] { copy->uv_rotation = source->uv_rotation; });
  check_difference([&] { copy->uv_offset->x = 0.0; }, [&] { copy->uv_offset->x = source->uv_offset->x; });
  check_difference([&] { copy->uv_offset->y = 0.0; }, [&] { copy->uv_offset->y = source->uv_offset->y; });
  check_difference([&] { copy->uv_scale->x = 1.0; }, [&] { copy->uv_scale->x = source->uv_scale->x; });
  check_difference([&] { copy->uv_scale->y = 1.0; }, [&] { copy->uv_scale->y = source->uv_scale->y; });
  check_difference([&] { copy->version += 1.0; }, [&] { copy->version = source->version; });
  check_difference([&] { copy->sampler->mipmaps = false; }, [&] { copy->sampler->mipmaps = source->sampler->mipmaps; });
  const auto different_face = flight::make_ref<types::Bitmap>();
  different_face->width = face->width;
  copy->sources.element(0.0) = types::TextureSourceValue{
      std::in_place_type<flight::Ref<types::Bitmap>>, different_face};
  check(!texture::equals_cube_texture(source, copy), "equal-looking faces retain distinct owner identity");
  copy->sources.element(0.0) = source->sources.element(0.0);
  copy->sources.push(std::nullopt);
  check(!texture::equals_cube_texture(source, copy), "face-list length participates in equality");
  texture::copy_cube_texture(copy, source);
  using OptionalNative = std::variant<types::CubeTexture, flight::Null, flight::Undefined>;
  using OptionalReadonly = std::variant<texture::ReadonlyCubeTexture, flight::Null, flight::Undefined>;
  check(texture::equals_cube_texture(OptionalNative{source}, OptionalReadonly{texture::ReadonlyCubeTexture(copy)}),
        "native and emitted readonly optional carriers compare the same retained owners");
  check(!texture::equals_cube_texture(flight::Null{}, source) &&
            !texture::equals_cube_texture(source, flight::Undefined{}) &&
            !texture::equals_cube_texture(OptionalNative{flight::Undefined{}}, OptionalReadonly{flight::Null{}}),
        "null and undefined operands short-circuit to false");
  using EqualsSignature = bool (*)(texture::OptionalReadonlyCubeTexture,
                                    texture::OptionalReadonlyCubeTexture);
  const EqualsSignature equals_function = &texture::equals_cube_texture;
  check(equals_function(texture::ReadonlyCubeTexture(source), texture::ReadonlyCubeTexture(copy)),
        "the emitted readonly nullable function signature is available as a function value");
  source->version = std::numeric_limits<double>::quiet_NaN();
  check(texture::equals_cube_texture(source, source), "identical row owners compare true before NaN fields");
  const auto proxy = flight::make_structural_write_proxy(
      texture::ReadonlyCubeTexture(source), std::string("version"), [] {});
  check(!texture::equals_cube_texture(proxy, source),
        "a distinct proxy row cannot inherit the native owner's identity fast path");
  source->version = 7.0;
  const auto previous_color = copy->color_space;
  source->dimension = flight::String("2d");
  bool matching_error = false;
  try { texture::copy_cube_texture(copy, source); }
  catch (const flight::Error& error) {
    matching_error = error.message() == flight::String("copyTexture requires matching dimensions");
  }
  check(matching_error && copy->color_space == previous_color,
        "dimension mismatch throws the source error before mutating destination");
  source->dimension = flight::String("cube");
  copy->uv_offset = source->uv_scale;
  const auto expected_x = source->uv_offset->x;
  texture::copy_cube_texture(copy, source);
  check(source->uv_scale->x == expected_x && copy->uv_scale->x == expected_x,
        "cross-aliased UV writes retain the pinned copy operation order");
  return failures == 0 ? 0 : 1;
}
