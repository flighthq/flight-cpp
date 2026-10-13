// Native evidence for the independently compilable @flighthq/spritesheet-formats outputs at
// a627849. The Cocos parser is covered end to end. Aseprite/TexturePacker parsing remains blocked by
// their declared parallel-schema refusals and JSON-to-nominal-union conversion; libGDX/Starling
// parsing remains blocked in textureatlas-formats; detection remains blocked by the frozen Registry
// row. Their absence here is not a substitute. The two JSON serializers compile, but their runtime
// output remains blocked by json-stringify-nominal-ref-empty-object.md, so this test does not bless
// their `{}` result.
//
// MUTATION SENSITIVITY, measured 2026-10-13 against isolated shadow headers: two one-at-a-time
// mutations were compiled and run, then discarded. The suite killed reversal of the Cocos page-size
// pair and reversal of Starling's serialized width/height attribute order.
#include <flight/spritesheet_formats/cocos_plist_parse.hpp>
#include <flight/spritesheet_formats/cocos_plist_serialize.hpp>
#include <flight/spritesheet_formats/libgdx_atlas_format.hpp>
#include <flight/spritesheet_formats/starling_serialize.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <variant>

namespace {

namespace formats = flight::spritesheet_formats;
namespace types = flight::types;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using Writable = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Type>>>>;
  using Readonly = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return flight::structural_ref_cast<Readonly>(Writable(value));
}

flight::Ref<types::SpritesheetData> sample_data() {
  auto frame = flight::make_ref<types::SpritesheetFrameData>(types::SpritesheetFrameData{
      .height = 20.0,
      .name = flight::String("hero"),
      .offset_x = 2.0,
      .offset_y = 3.0,
      .pivot_x = 0.25,
      .pivot_y = 0.75,
      .rotated = true,
      .source_height = 48.0,
      .source_width = 64.0,
      .width = 30.0,
      .x = 4.0,
      .y = 5.0,
  });
  auto animation = flight::make_ref<types::SpritesheetAnimationData>(types::SpritesheetAnimationData{
      .direction = flight::String("forward"),
      .frame_duration = 80.0,
      .frame_durations = flight::Array<double>{125.0},
      .frame_names = flight::Array<flight::String>{flight::String("hero")},
      .repeat_count = 2.0,
      .name = flight::String("walk"),
      .origin_x = 0.0,
      .origin_y = 0.0,
  });
  return flight::make_ref<types::SpritesheetData>(types::SpritesheetData{
      .animations = flight::Array<flight::Ref<types::SpritesheetAnimationData>>{animation},
      .frames = flight::Array<flight::Ref<types::SpritesheetFrameData>>{frame},
      .image_file = flight::String("atlas.png"),
      .image_height = 64.0,
      .image_width = 128.0,
      .scale = 2.0,
  });
}

flight::String minimal_cocos_plist() {
  return flight::String(R"PLIST(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>frames</key>
  <dict>
    <key>hero.png</key>
    <dict>
      <key>frame</key>
      <string>{{0,0},{64,64}}</string>
      <key>spriteOffset</key>
      <string>{2,3}</string>
      <key>spriteSize</key>
      <string>{64,64}</string>
      <key>spriteSourceSize</key>
      <string>{68,70}</string>
      <key>spriteTrimmed</key>
      <true/>
      <key>textureRotated</key>
      <false/>
    </dict>
    <key>coin.png</key>
    <dict>
      <key>frame</key>
      <string>{{64,0},{32,32}}</string>
      <key>spriteOffset</key>
      <string>{0,0}</string>
      <key>spriteSize</key>
      <string>{32,32}</string>
      <key>spriteSourceSize</key>
      <string>{32,32}</string>
      <key>spriteTrimmed</key>
      <false/>
      <key>textureRotated</key>
      <false/>
    </dict>
  </dict>
  <key>metadata</key>
  <dict>
    <key>format</key>
    <integer>3</integer>
    <key>size</key>
    <string>{256,128}</string>
    <key>textureFileName</key>
    <string>atlas.png</string>
  </dict>
</dict>
</plist>)PLIST");
}

void cocos_parse() {
  flight::Array<flight::Ref<types::ImportDiagnostic>> diagnostics;
  const auto parsed = formats::parse_cocos_plist_spritesheet_document(
      minimal_cocos_plist(),
      std::optional<flight::Array<flight::Ref<types::ImportDiagnostic>>>{diagnostics});

  check(diagnostics.empty(), "well-formed Cocos plist produces no diagnostics");
  check(parsed->document->frames.size() == 2 &&
            parsed->document->metadata->format == 3.0 &&
            parsed->document->metadata->size == flight::String("{256,128}") &&
            parsed->document->metadata->texture_file_name == flight::String("atlas.png"),
        "Cocos document parsing preserves exact metadata and frame shape");
  const auto hero_document = parsed->document->frames.get(flight::String("hero.png"));
  check(hero_document.has_value() && hero_document.value()->frame == flight::String("{{0,0},{64,64}}") &&
            hero_document.value()->sprite_offset == flight::String("{2,3}") &&
            hero_document.value()->sprite_size == flight::String("{64,64}") &&
            hero_document.value()->sprite_source_size == flight::String("{68,70}") &&
            hero_document.value()->sprite_trimmed && !hero_document.value()->texture_rotated,
        "Cocos document parsing retains every recognized frame field");

  const auto data = parsed->data;
  check(data->frames.size() == 2 && data->animations.empty() &&
            data->frames.element(0.0)->name == flight::String("hero.png") &&
            data->frames.element(1.0)->name == flight::String("coin.png"),
        "Cocos conversion preserves insertion order and produces no implicit animations");
  const auto hero = data->frames.element(0.0);
  check(hero->x == 0.0 && hero->y == 0.0 && hero->width == 64.0 && hero->height == 64.0 &&
            hero->offset_x == 2.0 && hero->offset_y == 3.0 && hero->source_width == 68.0 &&
            hero->source_height == 70.0 && !hero->rotated && !hero->pivot_x.has_value() &&
            !hero->pivot_y.has_value(),
        "Cocos conversion preserves exact geometry, trim offsets, source size, and absent pivots");
  check(data->image_file == flight::String("atlas.png") && data->image_width == 256.0 &&
            data->image_height == 128.0 && data->scale == 1.0,
        "Cocos conversion preserves page metadata exactly");

  const auto rotated = formats::parse_cocos_plist_spritesheet(flight::String(R"PLIST(<plist><dict>
<key>frames</key><dict><key>turn.png</key><dict>
<key>frame</key><string>{{7,9},{32,64}}</string>
<key>spriteOffset</key><string>{0,0}</string>
<key>spriteSize</key><string>{64,32}</string>
<key>spriteSourceSize</key><string>{64,32}</string>
<key>spriteTrimmed</key><false/><key>textureRotated</key><true/>
</dict></dict><key>metadata</key><dict><key>format</key><integer>3</integer>
<key>size</key><string>{128,128}</string><key>textureFileName</key><string>turn.png</string>
</dict></dict></plist>)PLIST"));
  const auto turned = rotated->frames.element(0.0);
  check(turned->name == flight::String("turn.png") && turned->x == 7.0 && turned->y == 9.0 &&
            turned->rotated && turned->width == 64.0 && turned->height == 32.0,
        "Cocos parsing transposes packed dimensions for a rotated frame");

  flight::Array<flight::Ref<types::ImportDiagnostic>> malformed_diagnostics;
  const auto malformed = formats::parse_cocos_plist_spritesheet(
      flight::String(R"PLIST(<plist><dict><key>frames</key><dict><key>bad.png</key><dict>
<key>unknownKey</key><string>value</string></dict></dict></dict></plist>)PLIST"),
      std::optional<flight::Array<flight::Ref<types::ImportDiagnostic>>>{malformed_diagnostics});
  check(malformed->frames.empty() && malformed_diagnostics.size() == 1,
        "Cocos parsing drops an unrecognized frame and emits one diagnostic");
  const auto diagnostic = malformed_diagnostics.element(0.0);
  const auto frame_detail = diagnostic->detail.has_value()
                                ? diagnostic->detail->get(flight::String("frame"))
                                : std::nullopt;
  check(diagnostic->kind == flight::String("spritesheet.cocos-plist.unrecognized-frame") &&
            diagnostic->severity == flight::String("Drop") &&
            diagnostic->origin == flight::String("parseCocosPlistXml") &&
            frame_detail.has_value() && std::holds_alternative<flight::String>(frame_detail.value()) &&
            std::get<flight::String>(frame_detail.value()) == flight::String("bad.png"),
        "Cocos diagnostics preserve exact kind, severity, origin, and frame detail");
}

void libgdx_format() {
  const auto atlas = formats::format_libgdx_atlas(readonly(sample_data()));
  check(atlas == flight::String(
                     "atlas.png\n"
                     "  size: 128, 64\n"
                     "  format: RGBA8888\n"
                     "  filter: Linear, Linear\n"
                     "  repeat: none\n"
                     "hero\n"
                     "  rotate: true\n"
                     "  xy: 4, 5\n"
                     "  size: 20, 30\n"
                     "  orig: 64, 48\n"
                     "  offset: 2, 3\n"
                     "  index: -1\n"),
        "libGDX formatting preserves page and rotated frame geometry");
}

void xml_serializers() {
  const auto data = sample_data();
  const auto starling = formats::serialize_starling_spritesheet(readonly(data));
  check(starling == flight::String(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<TextureAtlas imagePath=\"atlas.png\">\n"
            "\t<SubTexture name=\"hero\" x=\"4\" y=\"5\" width=\"20\" height=\"30\" "
            "frameX=\"-2\" frameY=\"-3\" frameWidth=\"64\" frameHeight=\"48\" "
            "pivotX=\"16\" pivotY=\"36\" rotated=\"true\"/>\n"
            "</TextureAtlas>"),
        "Starling serialization is byte exact for trimming, pivot, rotation, and page metadata");

  const auto cocos = formats::serialize_cocos_plist_spritesheet(readonly(data));
  check(cocos == flight::String(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
            "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\">\n<dict>\n\t<key>frames</key>\n\t<dict>\n"
            "\t\t<key>hero</key>\n\t\t<dict>\n"
            "\t\t\t<key>frame</key>\n\t\t\t<string>{{4,5},{20,30}}</string>\n"
            "\t\t\t<key>spriteOffset</key>\n\t\t\t<string>{2,3}</string>\n"
            "\t\t\t<key>spriteSize</key>\n\t\t\t<string>{30,20}</string>\n"
            "\t\t\t<key>spriteSourceSize</key>\n\t\t\t<string>{64,48}</string>\n"
            "\t\t\t<key>spriteTrimmed</key>\n\t\t\t<true/>\n"
            "\t\t\t<key>textureRotated</key>\n\t\t\t<true/>\n"
            "\t\t</dict>\n\t</dict>\n\t<key>metadata</key>\n\t<dict>\n"
            "\t\t<key>format</key>\n\t\t<integer>3</integer>\n"
            "\t\t<key>size</key>\n\t\t<string>{128,64}</string>\n"
            "\t\t<key>textureFileName</key>\n\t\t<string>atlas.png</string>\n"
            "\t</dict>\n</dict>\n</plist>"),
        "Cocos serialization is byte exact for frame shape, flags, and page metadata");
}

}  // namespace

int main() {
  cocos_parse();
  libgdx_format();
  xml_serializers();
  return failures == 0 ? 0 : 1;
}
