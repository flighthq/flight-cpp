// Native parity coverage for @flighthq/textureatlas-formats at Flight a627849.
// Registry-backed detection is intentionally absent: the frozen Registry row does not yet expose
// KeyedTable, so testing a private substitute would bless behavior the public package cannot compile.
#include <flight/textureatlas/texture_atlas.hpp>
#include <flight/textureatlas_formats/texture_atlas_aseprite_parse.hpp>
#include <flight/textureatlas_formats/texture_atlas_libgdx_parse.hpp>
#include <flight/textureatlas_formats/texture_atlas_page_meta.hpp>
#include <flight/textureatlas_formats/texture_atlas_starling_parse.hpp>
#include <flight/textureatlas_formats/texture_packer_atlas_parse.hpp>

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

void check_near(double actual, double expected, const char* message) {
  check(std::abs(actual - expected) < 1e-9, message);
}

bool same_string(const std::optional<flight::String>& actual, const char* expected) {
  return actual.has_value() && actual.value() == flight::String(expected);
}

flight::Ref<flight::types::TextureAtlas> make_atlas() {
  return flight::textureatlas::create_texture_atlas(std::nullopt);
}

flight::Ref<flight::types::TextureAtlasRegion> region(
    const flight::Ref<flight::types::TextureAtlas>& atlas,
    double index) {
  return atlas->regions.element(index);
}

using Scale = std::optional<std::variant<double, flight::String>>;

void page_metadata_helpers() {
  using flight::textureatlas_formats::read_texture_atlas_scale;

  check(read_texture_atlas_scale(std::nullopt) == 1.0, "absent scale defaults to one");
  check(read_texture_atlas_scale(Scale{std::variant<double, flight::String>{2.5}}) == 2.5,
        "positive numeric scale is retained");
  check(read_texture_atlas_scale(Scale{std::variant<double, flight::String>{
            std::in_place_type<flight::String>, flight::String("0.5")}}) == 0.5,
        "positive string scale is parsed");
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

  auto atlas = make_atlas();
  atlas->image_height = 512.0;
  atlas->image_name = flight::String("atlas.png");
  atlas->image_width = 1024.0;
  atlas->regions.push(flight::make_ref<flight::types::TextureAtlasRegion>());
  atlas->scale = 2.0;
  const auto regions = atlas->regions;

  flight::textureatlas_formats::reset_texture_atlas_page_meta(atlas);

  check(atlas->image_height == 0.0, "reset clears image height");
  check(!atlas->image_name.has_value(), "reset clears image name");
  check(atlas->image_width == 0.0, "reset clears image width");
  check(atlas->scale == 1.0, "reset restores unit scale");
  check(atlas->regions.size() == 1 && atlas->regions == regions,
        "reset preserves region storage and contents");
}

void aseprite_document_shape() {
  auto rect = flight::make_ref<flight::types::TextureAtlasAsepriteRect>(
      flight::types::TextureAtlasAsepriteRect{.h = 4.0, .w = 8.0, .x = 1.0, .y = 2.0});
  auto source_size = flight::make_ref<flight::types::TextureAtlasAsepriteSize>(
      flight::types::TextureAtlasAsepriteSize{.h = 16.0, .w = 12.0});
  auto source_rect = flight::make_ref<flight::types::TextureAtlasAsepriteRect>(
      flight::types::TextureAtlasAsepriteRect{.h = 4.0, .w = 8.0, .x = 3.0, .y = 5.0});
  auto frame = flight::make_ref<flight::types::TextureAtlasAsepriteBaseFrame>(
      flight::types::TextureAtlasAsepriteBaseFrame{
          .duration = 75.0,
          .frame = rect,
          .rotated = true,
          .source_size = source_size,
          .sprite_source_size = source_rect,
          .trimmed = true,
      });
  flight::Record<flight::String, flight::Ref<flight::types::TextureAtlasAsepriteBaseFrame>> frames;
  frames.set(flight::String("hero.png"), frame);
  auto meta_size = flight::make_ref<flight::types::TextureAtlasAsepriteSize>(
      flight::types::TextureAtlasAsepriteSize{.h = 64.0, .w = 128.0});
  auto meta = flight::make_ref<flight::types::TextureAtlasAsepriteMeta>(
      flight::types::TextureAtlasAsepriteMeta{
          .app = flight::String("aseprite"),
          .format = flight::String("RGBA8888"),
          .frame_tags = std::nullopt,
          .image = flight::String("aseprite.png"),
          .scale = std::variant<double, flight::String>{
              std::in_place_type<flight::String>, flight::String("0.5")},
          .size = meta_size,
          .version = flight::String("1.3"),
      });
  auto hash = flight::make_ref<flight::types::TextureAtlasAsepriteHashDocument>(
      flight::types::TextureAtlasAsepriteHashDocument{.frames = frames, .meta = meta});
  flight::types::TextureAtlasAsepriteDocument document{
      std::in_place_type<flight::Ref<flight::types::TextureAtlasAsepriteHashDocument>>, hash};
  auto atlas = make_atlas();

  const auto result = flight::textureatlas_formats::parse_texture_atlas_aseprite_document(document, atlas);
  check(result == atlas, "Aseprite document parser returns the same atlas");
  check(atlas->regions.size() == 1, "Aseprite document parser keeps one hash frame");
  check(same_string(atlas->image_name, "aseprite.png"), "Aseprite document keeps image name");
  check(atlas->image_width == 128.0 && atlas->image_height == 64.0,
        "Aseprite document keeps page dimensions");
  check_near(atlas->scale, 0.5, "Aseprite document parses string scale");
  const auto parsed = region(atlas, 0.0);
  check(parsed->id == 0.0 && same_string(parsed->name, "hero.png"),
        "Aseprite document keeps name and sequential id");
  check(parsed->x == 1.0 && parsed->y == 2.0 && parsed->width == 8.0 && parsed->height == 4.0,
        "Aseprite document keeps packed rectangle");
  check(parsed->trimmed && parsed->original_width == 12.0 && parsed->original_height == 16.0,
        "Aseprite document keeps trim size");
  check(parsed->source_x == 3.0 && parsed->source_y == 5.0,
        "Aseprite document keeps trim offsets");
  check(parsed->rotation == flight::types::TextureAtlasRotation::Clockwise90,
        "Aseprite document keeps clockwise rotation");
  check(!parsed->pivot_x.has_value() && !parsed->pivot_y.has_value(),
        "Aseprite document leaves pivots absent");
}

void aseprite_json_shape_and_failures() {
  const flight::String hash_json(R"json({
    "frames": {
      "missing.png": {},
      "idle.png": {"frame":{"x":0,"y":1,"w":8,"h":4},"rotated":false,"trimmed":false},
      "walk.png": {"frame":{"x":8,"y":1,"w":5,"h":7},"rotated":true,"trimmed":true,
        "sourceSize":{"w":12,"h":16},"spriteSourceSize":{"x":3,"y":4,"w":5,"h":7}}
    },
    "meta": {"app":"aseprite","image":"hash.png","scale":"0.25","size":{"w":32,"h":16}}
  })json");
  auto atlas = make_atlas();
  const auto result = flight::textureatlas_formats::parse_texture_atlas_aseprite_json(hash_json, atlas);
  check(result == atlas, "Aseprite JSON parser returns the same atlas");
  check(atlas->regions.size() == 2, "Aseprite JSON skips only the malformed frame");
  check(same_string(region(atlas, 0.0)->name, "idle.png") &&
            same_string(region(atlas, 1.0)->name, "walk.png"),
        "Aseprite JSON preserves hash insertion order");
  check(region(atlas, 0.0)->id == 0.0 && region(atlas, 1.0)->id == 1.0,
        "Aseprite JSON assigns dense ids after skipped frames");
  check(region(atlas, 1.0)->rotation == flight::types::TextureAtlasRotation::Clockwise90,
        "Aseprite JSON preserves rotation");
  check(region(atlas, 1.0)->original_width == 12.0 &&
            region(atlas, 1.0)->original_height == 16.0 &&
            region(atlas, 1.0)->source_x == 3.0 && region(atlas, 1.0)->source_y == 4.0,
        "Aseprite JSON preserves trim shape exactly");
  check(same_string(atlas->image_name, "hash.png") && atlas->image_width == 32.0 &&
            atlas->image_height == 16.0 && atlas->scale == 0.25,
        "Aseprite JSON preserves page metadata");

  const auto before_region = region(atlas, 0.0);
  const auto before_name = atlas->image_name;
  check(flight::textureatlas_formats::parse_texture_atlas_aseprite_json(
            flight::String("{ not json"), atlas) == atlas,
        "malformed Aseprite JSON returns the same atlas");
  check(atlas->regions.size() == 2 && region(atlas, 0.0) == before_region &&
            atlas->image_name == before_name,
        "malformed Aseprite JSON leaves atlas content unchanged");

  const flight::String array_json(R"json({
    "frames": [
      {"filename":"tile-b.png","frame":{"x":4,"y":0,"w":4,"h":4}},
      {"filename":"tile-a.png","frame":{"x":0,"y":0,"w":4,"h":4}}
    ],
    "meta": {"image":"array.png","size":{"w":8,"h":4},"scale":1}
  })json");
  auto array_atlas = make_atlas();
  flight::textureatlas_formats::parse_texture_atlas_aseprite_json(array_json, array_atlas);
  check(array_atlas->regions.size() == 2 &&
            same_string(region(array_atlas, 0.0)->name, "tile-b.png") &&
            same_string(region(array_atlas, 1.0)->name, "tile-a.png"),
        "Aseprite JSON preserves array order and filenames");
}

void texture_packer_document_and_json() {
  auto rect = flight::make_ref<flight::types::TexturePackerAtlasRect>(
      flight::types::TexturePackerAtlasRect{.h = 9.0, .w = 6.0, .x = 2.0, .y = 3.0});
  auto source_size = flight::make_ref<flight::types::TexturePackerAtlasSize>(
      flight::types::TexturePackerAtlasSize{.h = 18.0, .w = 12.0});
  auto pivot = flight::make_ref<flight::types::TexturePackerAtlasPivot>(
      flight::types::TexturePackerAtlasPivot{.x = 0.25, .y = 0.75});
  auto frame = flight::make_ref<flight::types::TexturePackerAtlasHashFrame>(
      flight::types::TexturePackerAtlasHashFrame{
          .frame = rect,
          .pivot = pivot,
          .rotated = true,
          .source_size = source_size,
          .sprite_source_size = rect,
          .trimmed = true,
      });
  flight::Record<flight::String, flight::Ref<flight::types::TexturePackerAtlasHashFrame>> frames;
  frames.set(flight::String("sprites/hero.png"), frame);
  auto meta = flight::make_ref<flight::types::TexturePackerAtlasMeta>(
      flight::types::TexturePackerAtlasMeta{
          .app = flight::String("texturepacker"),
          .format = flight::String("RGBA8888"),
          .frame_tags = std::nullopt,
          .image = flight::String("packer.png"),
          .scale = std::variant<double, flight::String>{2.0},
          .size = source_size,
          .version = flight::String("1.0"),
      });
  auto hash = flight::make_ref<flight::types::TexturePackerAtlasHashDocument>(
      flight::types::TexturePackerAtlasHashDocument{.frames = frames, .meta = meta});
  flight::types::TexturePackerAtlasDocument document{
      std::in_place_type<flight::Ref<flight::types::TexturePackerAtlasHashDocument>>, hash};
  auto options = flight::make_ref<flight::types::TexturePackerAtlasParseOptions>(
      flight::types::TexturePackerAtlasParseOptions{.strip_path_prefix = true});
  auto atlas = make_atlas();

  const auto result = flight::textureatlas_formats::parse_texture_packer_atlas_document(
      document, atlas, options);
  check(result == atlas && atlas->regions.size() == 1,
        "TexturePacker document parser returns and fills the same atlas");
  const auto parsed = region(atlas, 0.0);
  check(same_string(parsed->name, "hero.png"), "TexturePacker document strips path prefix");
  check(parsed->pivot_x == 0.25 && parsed->pivot_y == 0.75,
        "TexturePacker document keeps pivots");
  check(parsed->width == 6.0 && parsed->height == 9.0 &&
            parsed->rotation == flight::types::TextureAtlasRotation::Clockwise90,
        "TexturePacker document preserves packed rotated dimensions");

  const flight::String json(R"json({
    "frames": {
      "bad.png": {"rotated":false},
      "dir/coin.png": {"frame":{"x":7,"y":8,"w":10,"h":11},"rotated":true,"trimmed":true,
        "sourceSize":{"w":20,"h":22},"spriteSourceSize":{"x":2,"y":3,"w":10,"h":11},
        "pivot":{"x":0.4,"y":0.6}}
    },
    "meta": {"app":"texturepacker","image":"packed.png","scale":"0.5","size":{"w":64,"h":32}}
  })json");
  auto json_atlas = make_atlas();
  check(flight::textureatlas_formats::parse_texture_packer_atlas_json(
            json, json_atlas, options) == json_atlas,
        "TexturePacker JSON parser returns the same atlas");
  check(json_atlas->regions.size() == 1, "TexturePacker JSON skips malformed frames");
  const auto json_region = region(json_atlas, 0.0);
  check(same_string(json_region->name, "coin.png") && json_region->id == 0.0,
        "TexturePacker JSON strips path and assigns dense id");
  check(json_region->x == 7.0 && json_region->y == 8.0 &&
            json_region->width == 10.0 && json_region->height == 11.0,
        "TexturePacker JSON keeps packed rectangle");
  check(json_region->original_width == 20.0 && json_region->original_height == 22.0 &&
            json_region->source_x == 2.0 && json_region->source_y == 3.0,
        "TexturePacker JSON keeps trim shape");
  check(json_region->pivot_x == 0.4 && json_region->pivot_y == 0.6,
        "TexturePacker JSON keeps pivot");
  check(same_string(json_atlas->image_name, "packed.png") &&
            json_atlas->image_width == 64.0 && json_atlas->image_height == 32.0 &&
            json_atlas->scale == 0.5,
        "TexturePacker JSON keeps page metadata");

  const auto before = json_region;
  flight::textureatlas_formats::parse_texture_packer_atlas_json(
      flight::String("[]"), json_atlas, options);
  check(json_atlas->regions.size() == 1 && region(json_atlas, 0.0) == before,
        "non-object TexturePacker JSON leaves atlas unchanged");
}

void libgdx_shape() {
  const flight::String text(R"atlas(page0.png
size: 128,64
format: RGBA8888
idle
  rotate: false
  xy: 1, 2
  size: 8, 4
  orig: 8, 4
  offset: 0, 0
  index: -1
walk
  rotate: true
  xy: 9, 2
  size: 5, 7
  orig: 12, 10
  offset: 3, 1
  index: 2

page1.png
size: 32,32
format: RGBA8888
coin
  rotate: 270
  xy: 0, 0
  size: 3, 6
  orig: 6, 3
  offset: 0, 0
  index: -1
)atlas");
  auto atlas = make_atlas();
  const auto result = flight::textureatlas_formats::parse_texture_atlas_libgdx_atlas(text, atlas);
  check(result == atlas && atlas->regions.size() == 3,
        "libGDX parser returns same atlas with all pages");
  check(same_string(atlas->image_name, "page0.png"), "libGDX atlas keeps first page image");
  check(same_string(region(atlas, 0.0)->name, "idle") &&
            same_string(region(atlas, 1.0)->name, "walk_2") &&
            same_string(region(atlas, 2.0)->name, "coin"),
        "libGDX parser preserves order and indexed names");
  check(region(atlas, 0.0)->id == 0.0 && region(atlas, 2.0)->id == 2.0,
        "libGDX parser assigns sequential ids across pages");
  check(same_string(region(atlas, 0.0)->page_name, "page0.png") &&
            same_string(region(atlas, 2.0)->page_name, "page1.png"),
        "libGDX parser preserves each region page");
  check(region(atlas, 1.0)->rotation == flight::types::TextureAtlasRotation::Counterclockwise90 &&
            region(atlas, 2.0)->rotation == flight::types::TextureAtlasRotation::Clockwise90,
        "libGDX parser preserves true and 270-degree rotation directions");
  check(region(atlas, 1.0)->trimmed && region(atlas, 1.0)->original_width == 12.0 &&
            region(atlas, 1.0)->original_height == 10.0 &&
            region(atlas, 1.0)->source_x == 3.0 && region(atlas, 1.0)->source_y == 1.0,
        "libGDX parser preserves trim geometry");
}

void starling_shape_and_failures() {
  const flight::String xml(R"xml(<?xml version="1.0"?>
<TextureAtlas imagePath="starling.png">
  <SubTexture x="99" y="99" width="1" height="1"/>
  <SubTexture name="idle" x="1" y="2" width="8" height="4"/>
  <SubTexture name="walk" x="9" y="2" width="5" height="7" frameX="-3" frameY="-1" frameWidth="12" frameHeight="10" pivotX="4" pivotY="6"/>
  <SubTexture name="rot" x="14" y="2" width="20" height="40" rotated="true" frameX="0" frameY="0"/>
</TextureAtlas>)xml");
  auto atlas = make_atlas();
  const auto result = flight::textureatlas_formats::parse_texture_atlas_starling_xml(xml, atlas);
  check(result == atlas && atlas->regions.size() == 3,
        "Starling parser skips unnamed entry and keeps readable frames");
  check(same_string(atlas->image_name, "starling.png"), "Starling parser keeps imagePath");
  check(same_string(region(atlas, 0.0)->name, "idle") &&
            region(atlas, 0.0)->rotation == flight::types::TextureAtlasRotation::None,
        "Starling parser accepts an absent rotated attribute as false");
  check(region(atlas, 1.0)->original_width == 12.0 &&
            region(atlas, 1.0)->original_height == 10.0 &&
            region(atlas, 1.0)->source_x == 3.0 && region(atlas, 1.0)->source_y == 1.0 &&
            region(atlas, 1.0)->pivot_x == 4.0 && region(atlas, 1.0)->pivot_y == 6.0,
        "Starling parser preserves trim and pivot shape");
  check(region(atlas, 2.0)->rotation == flight::types::TextureAtlasRotation::Clockwise90 &&
            region(atlas, 2.0)->original_width == 40.0 &&
            region(atlas, 2.0)->original_height == 20.0,
        "Starling rotated fallback dimensions are upright and ordered");

  flight::textureatlas_formats::parse_texture_atlas_starling_xml(flight::String(""), atlas);
  check(atlas->regions.empty() && !atlas->image_name.has_value() && atlas->scale == 1.0,
        "malformed Starling XML clears regions and resets page metadata without throwing");
}

}  // namespace

int main() {
  page_metadata_helpers();
  aseprite_document_shape();
  aseprite_json_shape_and_failures();
  texture_packer_document_and_json();
  libgdx_shape();
  starling_shape_and_failures();
  return failures == 0 ? 0 : 1;
}
