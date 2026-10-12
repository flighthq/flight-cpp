// Native evidence for the independently compilable @flighthq/spritesheet-formats outputs at
// a627849. Parser headers are intentionally absent: their shared FlightDocumentValue dependency is
// recursively ill-formed and recorded in recursive-value-union-complete-storage.md. The Aseprite
// and TexturePacker serializer headers compile, but their end-to-end runtime output remains blocked
// by json-stringify-nominal-ref-empty-object.md, so this test does not bless their `{}` result.
#include <flight/spritesheet_formats/cocos_plist_serialize.hpp>
#include <flight/spritesheet_formats/libgdx_atlas_format.hpp>
#include <flight/spritesheet_formats/starling_serialize.hpp>

#include <iostream>
#include <memory>
#include <optional>

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
  check(starling.includes(flight::String("<TextureAtlas imagePath=\"atlas.png\">")),
        "Starling serialization preserves the image path");
  check(starling.includes(flight::String(
            "<SubTexture name=\"hero\" x=\"4\" y=\"5\" width=\"20\" height=\"30\" "
            "frameX=\"-2\" frameY=\"-3\" frameWidth=\"64\" frameHeight=\"48\" "
            "pivotX=\"16\" pivotY=\"36\" rotated=\"true\"/>")),
        "Starling serialization preserves trimming, pivot, and rotation");

  const auto cocos = formats::serialize_cocos_plist_spritesheet(readonly(data));
  check(cocos.includes(flight::String("<key>hero</key>")),
        "Cocos serialization keeps the frame name");
  check(cocos.includes(flight::String("<string>{{4,5},{20,30}}</string>")) &&
            cocos.includes(flight::String("<string>{2,3}</string>")) &&
            cocos.includes(flight::String("<string>{64,48}</string>")),
        "Cocos serialization preserves packed, offset, and source geometry");
  check(cocos.includes(flight::String("<key>spriteTrimmed</key>\n\t\t\t<true/>")) &&
            cocos.includes(flight::String("<key>textureRotated</key>\n\t\t\t<true/>")),
        "Cocos serialization preserves trimming and rotation flags");
  check(cocos.includes(flight::String("<string>{128,64}</string>")) &&
            cocos.includes(flight::String("<string>atlas.png</string>")),
        "Cocos serialization preserves page metadata");
}

}  // namespace

int main() {
  libgdx_format();
  xml_serializers();
  return failures == 0 ? 0 : 1;
}
