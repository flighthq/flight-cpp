// The generated @flighthq/textureatlas-formats package at a627849, exercised natively where its
// emitted page-metadata helpers do not cross the still-partial @flighthq/textureatlas parser path.
// The cases mirror textureAtlasPageMeta.test.ts: absent/invalid scales fall back to one, valid
// numeric scales survive, and resetting page metadata does not clear atlas regions.
#include <flight/textureatlas_formats/texture_atlas_page_meta.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <variant>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

using Scale = std::optional<std::variant<double, flight::String>>;

void scale_reading() {
  using flight::textureatlas_formats::read_texture_atlas_scale;

  check(read_texture_atlas_scale(std::nullopt) == 1.0, "absent scale defaults to one");
  check(read_texture_atlas_scale(Scale{std::variant<double, flight::String>{2.5}}) == 2.5,
        "positive numeric scale is retained");
  check(read_texture_atlas_scale(Scale{std::variant<double, flight::String>{0.0}}) == 1.0,
        "zero scale defaults to one");
  check(read_texture_atlas_scale(Scale{std::variant<double, flight::String>{-2.0}}) == 1.0,
        "negative scale defaults to one");
  check(read_texture_atlas_scale(Scale{std::variant<double, flight::String>{
            std::numeric_limits<double>::infinity()}}) == 1.0,
        "infinite scale defaults to one");
  check(read_texture_atlas_scale(Scale{std::variant<double, flight::String>{
            std::numeric_limits<double>::quiet_NaN()}}) == 1.0,
        "NaN scale defaults to one");
}

void page_meta_reset() {
  auto atlas = flight::make_ref<flight::types::TextureAtlas>();
  auto region = flight::make_ref<flight::types::TextureAtlasRegion>();
  atlas->image_height = 512.0;
  atlas->image_name = flight::String("atlas.png");
  atlas->image_width = 1024.0;
  atlas->regions.push(region);
  atlas->scale = 2.0;

  flight::textureatlas_formats::reset_texture_atlas_page_meta(atlas);

  check(atlas->image_height == 0.0, "reset clears image height");
  check(!atlas->image_name.has_value(), "reset clears image name");
  check(atlas->image_width == 0.0, "reset clears image width");
  check(atlas->scale == 1.0, "reset restores unit scale");
  check(atlas->regions.size() == 1 && atlas->regions.get(0).value() == region,
        "reset preserves parsed regions");
}

}  // namespace

int main() {
  scale_reading();
  page_meta_reset();
  return failures == 0 ? 0 : 1;
}
