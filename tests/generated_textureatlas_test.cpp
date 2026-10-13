#include <flight/textureatlas/enable_texture_atlas_guards.hpp>
#include <flight/textureatlas/texture_atlas_from.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <variant>

// Native ports of the override-sensitive assertions at Flight a627849. The 1e-8 tolerance matches
// the upstream region suite's toBeCloseTo assertions. The browser object-URL loaders and built-in
// warning callback remain declared package gaps and are intentionally not simulated here.

namespace {

namespace textureatlas = flight::textureatlas;
namespace types = flight::types;

int failures = 0;
constexpr double tolerance = 1e-8;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void check_close(double actual, double expected, const std::string& message) {
  check(std::abs(actual - expected) <= tolerance, message);
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return View(value);
}

flight::Ref<types::ImageResource> image(double width, double height) {
  auto result = flight::make_ref<types::ImageResource>();
  result->width = width;
  result->height = height;
  result->kind = flight::String("image");
  return result;
}

flight::Ref<types::ImageResource> texture_source(
    const flight::Ref<types::TextureAtlas>& atlas) {
  if (!atlas->texture.has_value() || !atlas->texture.value()->source.has_value()) {
    return nullptr;
  }
  const auto* source = std::get_if<flight::Ref<types::ImageResource>>(
      &atlas->texture.value()->source.value());
  return source == nullptr ? nullptr : *source;
}

void test_image_construction_and_host_loading() {
  const auto resource = image(128.0, 64.0);
  const auto first = textureatlas::create_texture_atlas_from_image_resource(resource);
  const auto second = textureatlas::create_texture_atlas_from_image_resource(resource);

  check(first != second, "image construction returns a fresh atlas");
  check(first->regions.empty(), "image construction starts with no regions");
  check(texture_source(first) == resource,
        "image construction retains the exact ImageResource owner");
  check(first->texture.value()->dimension == flight::String("2d"),
        "image construction selects the 2D texture overload");

  auto host = flight::make_ref<types::HostImageCapability>();
  flight::String seen_url;
  std::optional<flight::String> seen_cross_origin;
  std::optional<flight::AbortSignal> seen_signal;
  int loads = 0;
  host->load_image_from_url =
      [&](flight::String url,
          std::optional<flight::String> cross_origin,
          std::optional<flight::AbortSignal> signal) {
        ++loads;
        seen_url = std::move(url);
        seen_cross_origin = std::move(cross_origin);
        seen_signal = std::move(signal);
        return flight::Task<flight::Ref<types::ImageResource>>::resolve(resource);
      };

  const flight::AbortSignal signal;
  const auto from_url = textureatlas::load_texture_atlas_from_url(
      readonly(host), flight::String("https://example.invalid/a.png"),
      flight::String("anonymous"), signal).get();
  check(loads == 1 && seen_url == flight::String("https://example.invalid/a.png") &&
            seen_cross_origin == std::optional<flight::String>{flight::String("anonymous")} &&
            seen_signal == std::optional<flight::AbortSignal>{signal},
        "URL loading forwards URL, cross-origin policy and signal to the host capability");
  check(texture_source(from_url) == resource,
        "URL loading preserves the host's ImageResource owner");

  const auto from_base64 = textureatlas::load_texture_atlas_from_base64(
      readonly(host), flight::String("abc123"), flight::String("image/png"), signal).get();
  check(loads == 2 && seen_url == flight::String("data:image/png;base64,abc123") &&
            !seen_cross_origin.has_value() &&
            seen_signal == std::optional<flight::AbortSignal>{signal},
        "base64 loading delegates with the exact data URL, signal and no cross-origin policy");
  check(texture_source(from_base64) == resource,
        "base64 loading preserves the host's ImageResource owner");
}

void test_region_texture_cache_and_window() {
  const auto atlas =
      textureatlas::create_texture_atlas_from_image_resource(image(100.0, 50.0));
  textureatlas::add_texture_atlas_region(atlas, 10.0, 5.0, 20.0, 15.0);
  textureatlas::add_texture_atlas_region(atlas, 30.0, 5.0, 20.0, 15.0);

  const auto first = textureatlas::get_texture_atlas_region_texture(readonly(atlas), 0.0);
  const auto again = textureatlas::get_texture_atlas_region_texture(readonly(atlas), 0.0);
  const auto second = textureatlas::get_texture_atlas_region_texture(readonly(atlas), 1.0);
  check(first.has_value() && again == first,
        "a region reuses its cached texture identity");
  check(second.has_value() && second != first,
        "distinct regions receive distinct cached textures");
  check(std::get<flight::Ref<types::ImageResource>>(first.value()->source.value()) ==
            texture_source(atlas),
        "a region texture retains the atlas image owner");
  check_close(first.value()->uv_offset->x, 0.1, "region x becomes a normalized UV offset");
  check_close(first.value()->uv_offset->y, 0.1, "region y becomes a normalized UV offset");
  check_close(first.value()->uv_scale->x, 0.2, "region width becomes a normalized UV scale");
  check_close(first.value()->uv_scale->y, 0.3, "region height becomes a normalized UV scale");

  const auto region = textureatlas::get_texture_atlas_region_by_id(readonly(atlas), 0.0).value();
  region->x = 50.0;
  region->y = 25.0;
  region->width = 40.0;
  region->height = 25.0;
  const auto refreshed = textureatlas::get_texture_atlas_region_texture(readonly(atlas), 0.0);
  check(refreshed == first, "an edited region rewrites the existing cached texture");
  check_close(first.value()->uv_offset->x, 0.5, "an in-place x edit refreshes the UV offset");
  check_close(first.value()->uv_offset->y, 0.5, "an in-place y edit refreshes the UV offset");
  check_close(first.value()->uv_scale->x, 0.4, "an in-place width edit refreshes the UV scale");
  check_close(first.value()->uv_scale->y, 0.5, "an in-place height edit refreshes the UV scale");

  const auto page = atlas->texture.value();
  page->uv_offset->x = 0.25;
  page->uv_offset->y = 0.1;
  page->uv_scale->x = 0.5;
  page->uv_scale->y = 0.8;
  region->x = 10.0;
  region->y = 5.0;
  region->width = 20.0;
  region->height = 15.0;
  static_cast<void>(textureatlas::get_texture_atlas_region_texture(readonly(atlas), 0.0));
  check_close(first.value()->uv_offset->x, 0.35,
              "a region x composes with the page's pixel-space window");
  check_close(first.value()->uv_offset->y, 0.2,
              "a region y composes with the page's pixel-space window");
  check_close(first.value()->uv_scale->x, 0.2,
              "a region width remains normalized to the source image");
  check_close(first.value()->uv_scale->y, 0.3,
              "a region height remains normalized to the source image");
}

void test_rotation_failure_guard_and_zero_source() {
  const auto atlas =
      textureatlas::create_texture_atlas_from_image_resource(image(100.0, 50.0));
  textureatlas::add_texture_atlas_region(atlas, 10.0, 5.0, 20.0, 15.0);
  const auto page = atlas->texture.value();
  const auto region = atlas->regions.get(0.0).value();
  page->flip_x = true;
  page->flip_y = true;
  region->rotation = types::TextureAtlasRotation::Clockwise90;

  const auto rotated = textureatlas::get_texture_atlas_region_texture(readonly(atlas), 0.0).value();
  check(rotated->flip_x && rotated->flip_y,
        "a packed quarter-turn folds both page flips into the region view");
  check_close(rotated->uv_offset->x, 0.9, "clockwise packing anchors the rotated x edge");
  check_close(rotated->uv_offset->y, 0.6, "clockwise packing anchors the rotated y edge");
  check_close(rotated->uv_rotation, flight::pi / 2.0,
              "clockwise packing rotates the view by one quarter-turn");
  check_close(rotated->uv_scale->x, 0.3, "rotated height becomes horizontal scale");
  check_close(rotated->uv_scale->y, 0.2, "rotated width becomes vertical scale");

  bool warned = false;
  page->uv_rotation = 0.25;
  textureatlas::set_texture_atlas_region_texture_guard(
      [&](auto seen_atlas, double id, auto explanation) {
        warned = seen_atlas.shared_native_object().get() == atlas.get() && id == 0.0 &&
                 flight::row_get<flight::RowKey<"status">>(explanation) ==
                     flight::String("rotated-page");
      });
  check(!textureatlas::get_texture_atlas_region_texture(readonly(atlas), 0.0).has_value() && warned,
        "a rotated page refuses the view and invokes the configured diagnostic hook");

  warned = false;
  textureatlas::disable_texture_atlas_guards();
  static_cast<void>(textureatlas::get_texture_atlas_region_texture(readonly(atlas), 0.0));
  check(!textureatlas::are_texture_atlas_guards_enabled() && !warned,
        "disabling guards clears a custom diagnostic hook and reports false");

  page->uv_rotation = 0.0;
  page->source = image(0.0, 50.0);
  static_cast<void>(textureatlas::get_texture_atlas_region_texture(readonly(atlas), 0.0));
  check(!rotated->flip_x && !rotated->flip_y && rotated->uv_rotation == 0.0 &&
            rotated->uv_offset->x == 0.0 && rotated->uv_offset->y == 0.0 &&
            rotated->uv_scale->x == 0.0 && rotated->uv_scale->y == 0.0,
        "a zero-sized source resets every derived sampling field");
}

}  // namespace

int main() {
  test_image_construction_and_host_loading();
  test_region_texture_cache_and_window();
  test_rotation_failure_guard_and_zero_source();

  if (failures == 0) {
    std::cout << "texture-atlas image loading and region windows match Flight\n";
  }
  return failures == 0 ? 0 : 1;
}
