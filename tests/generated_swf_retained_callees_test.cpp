// Native original consumers of the two SWF retained-callee functions, exercised through the generated
// control and place-object tag handlers. Both functions were refused for a dual-sentinel optional
// property result until their color-transform reads were hoisted into explicitly narrowed locals
// (source-patches/swf-button-definition-dual-sentinel-locals.patch and
// source-patches/swf-legacy-place-object-dual-sentinel-locals.patch).
//
// These assertions are the source semantics of packages/swf/src/swfControlHandler.ts and
// packages/swf/src/swfPlaceObjectHandler.ts at a627849: a nullable transform is the only absence, so a
// missing transform and a null adjustment list both leave the optional adjustment fields empty; a
// present tint carries one array through both adjustment fields by identity; alpha is the transform's
// clamped multiply alpha in DefineButton2 and 1 in the RGB-only legacy PlaceObject path; invalid or
// zero-character records are absence rather than an error; a non-up button state reports Skip and
// holds no placement; and the placements Map keeps insertion order.
#include <flight/importdiagnostics/import_diagnostic_collector.hpp>
#include <flight/swf/swf_control_handler.hpp>
#include <flight/swf/swf_place_object_handler.hpp>
#include <flight/swf/swf_reader.hpp>
#include <flight/types/import_diagnostic.hpp>
#include <flight/types/swf_tag_parse_state.hpp>

#include <iostream>
#include <optional>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

namespace swf = flight::swf;
namespace types = flight::types;

using Bytes = flight::Uint8Array;
using State = types::SwfTagParseState;
using Timeline = types::SwfTagTimelineState;

flight::Ref<swf::SwfReader> reader(const Bytes& bytes) {
  return flight::make_ref<swf::SwfReader>(bytes, 0.0, static_cast<double>(bytes.size()));
}

flight::Ref<State> state_with(
    std::optional<flight::Array<flight::Ref<types::ImportDiagnostic>>> diagnostics = std::nullopt) {
  return flight::make_ref<State>(State{.diagnostics = diagnostics});
}

flight::Ref<Timeline> timeline_state() {
  return flight::make_ref<Timeline>(Timeline{});
}

std::vector<double> placement_depths(const flight::Map<double, flight::Ref<types::SwfTagPlacement>>& placements) {
  std::vector<double> depths;
  for (const double depth : placements.keys()) depths.push_back(depth);
  return depths;
}

bool same_array_identity(const std::optional<flight::Array<flight::Ref<types::Adjustment>>>& left,
                         const std::optional<flight::Array<flight::Ref<types::Adjustment>>>& right) {
  return left.has_value() && right.has_value() && left->identity() == right->identity();
}

// DefineButton, version 1: button id 0x1234, up-state character 0x5678 at depth 5, then character
// 0x9abc at depth 3. Two records are enough to pin the Map's insertion order.
const Bytes kButtonVersion1{
    0x34, 0x12,        // buttonId 0x1234
    0x01,              // flags: BUTTON_STATE_UP
    0x78, 0x56,        // characterId 0x5678
    0x05, 0x00,        // depth 5
    0x00,              // identity matrix (hasScale 0, hasRotate 0, no translate, then aligned)
    0x01,              // flags: BUTTON_STATE_UP
    0xbc, 0x9a,        // characterId 0x9abc
    0x03, 0x00,        // depth 3
    0x00,              // identity matrix
    0x00,              // flags 0 ends the records
};

// DefineButton2: button id 0x1234, reserved byte and action offset, one up-state record at depth 1
// carrying an RGB color transform whose multiplies are 1/256: hasAdd 0, hasMultiply 1, nbits 2.
const Bytes kButtonVersion2{
    0x34, 0x12,        // buttonId 0x1234
    0x00,              // reserved
    0x00, 0x00,        // action offset
    0x01,              // flags: BUTTON_STATE_UP
    0x78, 0x56,        // characterId 0x5678
    0x01, 0x00,        // depth 1
    0x00,              // identity matrix
    0x49, 0x54,        // transform: hasAdd 0, hasMultiply 1, nbits 2, multiplies 1,1,1,1
    0x00,              // flags 0 ends the records
};

// DefineButton2 whose only record is a non-up interaction state (flags 0x02) for character 0x5678.
const Bytes kButtonDownState{
    0x34, 0x12,        // buttonId 0x1234
    0x00,              // reserved
    0x00, 0x00,        // action offset
    0x02,              // flags: not BUTTON_STATE_UP
    0x78, 0x56,        // characterId 0x5678
    0x01, 0x00,        // depth 1
    0x00,              // identity matrix
    0x49, 0x54,        // transform (read before the state test, as the source does)
    0x00,              // flags 0 ends the records
};

void button_version1_consumer() {
  // The original tag handler is the consumer: its parse callback is the retained call site.
  auto body = reader(kButtonVersion1);
  auto state = state_with();
  auto timeline = timeline_state();
  const bool handled = swf::swf_control_handler->parse(body, swf::tag_define_button, state, timeline, std::nullopt);
  check(handled, "the control handler claims DefineButton");
  check(state->defined_characters.has(0x1234), "the button id is defined");
  check(state->sprites.has(0x1234), "the button owns a one-frame sprite");

  auto sprite = state->sprites.get(0x1234);
  check(sprite.has_value(), "the sprite is reachable by button id");
  if (!sprite.has_value()) return;
  check(sprite.value()->frames.size() == 1.0, "the button has exactly one frame");
  if (sprite.value()->frames.size() != 1.0) return;
  auto placements = sprite.value()->frames.element(0);
  check(placements.size() == 2.0, "both up-state records are held");

  // JS Map order is insertion order, not numeric depth order.
  const auto depths = placement_depths(placements);
  check(depths.size() == 2 && depths[0] == 5.0 && depths[1] == 3.0, "placement order is insertion order");

  auto first = placements.get(5.0);
  auto second = placements.get(3.0);
  check(first.has_value() && second.has_value(), "both placements are keyed by depth");
  if (!first.has_value() || !second.has_value()) return;
  const auto& placement = first.value();
  check(placement->character_id == 0x5678, "the record keeps its character id");
  check(placement->depth == 5.0, "the record keeps its depth");
  check(placement->alpha == 1.0, "a version-1 button has no transform, so alpha is 1");
  check(!placement->color_adjustments.has_value(), "a missing transform leaves no adjustments");
  check(!placement->color_transform_adjustments.has_value(), "both adjustment fields stay empty");
  check(placement->blend_mode == types::blend_mode->normal, "the up state uses Normal blending");
  check(!placement->advanced_blend_mode.has_value(), "no advanced blend mode is stored");
  check(!placement->direct_linkage.has_value() && !placement->name.has_value(), "no linkage or name is invented");
  check(placement->ratio == 0.0 && placement->clip_depth == 0.0, "the non-transform defaults are the source's");
  check(placement->matrix->a == 1.0 && placement->matrix->b == 0.0 && placement->matrix->c == 0.0 &&
            placement->matrix->d == 1.0 && placement->matrix->tx == 0.0 && placement->matrix->ty == 0.0,
        "an identity matrix decodes to identity");
  check(placement->effects.identity() == swf::empty_effects.identity(), "the shared empty effect array is reused");
  check(placement->filter_adjustments.identity() == swf::empty_adjustments.identity(),
        "the shared empty adjustment array is reused");
  check(second.value()->character_id == 0x9abc && second.value()->depth == 3.0, "the second record is its own");
}

void button_version2_tint_presence() {
  auto body = reader(kButtonVersion2);
  auto state = state_with();
  auto timeline = timeline_state();
  swf::read_swf_button_definition(body, state, 2.0);

  auto sprite = state->sprites.get(0x1234);
  check(sprite.has_value(), "the version-2 button is defined");
  if (!sprite.has_value() || sprite.value()->frames.size() != 1.0) return;
  auto placement = sprite.value()->frames.element(0).get(1.0);
  check(placement.has_value(), "the version-2 record is placed");
  if (!placement.has_value()) return;
  check(placement.value()->alpha == 1.0 / 256.0, "alpha is the transform's clamped multiply alpha");
  check(placement.value()->color_adjustments.has_value(), "a tint produces adjustments");
  check(same_array_identity(placement.value()->color_adjustments, placement.value()->color_transform_adjustments),
        "both adjustment fields carry the same array, by identity");
}

void button_down_state_reports_skip() {
  auto diagnostics = flight::Array<flight::Ref<types::ImportDiagnostic>>{};
  auto body = reader(kButtonDownState);
  auto state = state_with(std::optional<flight::Array<flight::Ref<types::ImportDiagnostic>>>{diagnostics});
  auto timeline = timeline_state();
  swf::read_swf_button_definition(body, state, 2.0);

  check(state->defined_characters.has(0x1234), "a button with no up-state records is still defined");
  check(diagnostics.size() == 1.0, "the non-up state reports one Skip diagnostic");
  if (diagnostics.size() == 1.0) {
    const auto& diagnostic = diagnostics.element(0);
    check(diagnostic->severity == types::import_diagnostic_severity->skip, "the severity is Skip");
    check(diagnostic->kind == flight::String("swf.button-interaction-state"), "the diagnostic kind names the state");
    check(diagnostic->origin == flight::String("readSwfButtonDefinition"), "the origin names the original function");
  }
  auto sprite = state->sprites.get(0x1234);
  check(sprite.has_value(), "the button still owns a sprite");
  if (sprite.has_value()) {
    check(sprite.value()->frames.size() == 1.0, "the sprite still has one frame");
    if (sprite.value()->frames.size() == 1.0) {
      check(sprite.value()->frames.element(0).size() == 0.0, "the down state holds no up-state placement");
    }
  }
}

void button_absence_and_duplicate() {
  // A record for character id 0 adds no placement, but the button itself is still defined: the source
  // records the id and a one-frame sprite after the loop whether or not any record placed anything.
  auto zero_body = reader(Bytes{
      0x34, 0x12, 0x01, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
  });
  auto zero_state = state_with();
  swf::read_swf_button_definition(zero_body, zero_state, 1.0);
  check(zero_state->defined_characters.has(0x1234), "a zero-character record still defines the button");
  auto zero_sprite = zero_state->sprites.get(0x1234);
  check(zero_sprite.has_value(), "the zero-character button still owns a one-frame sprite");
  if (zero_sprite.has_value() && zero_sprite.value()->frames.size() == 1.0) {
    check(zero_sprite.value()->frames.element(0).size() == 0.0, "the zero-character record places nothing");
  }

  // A record list that ends immediately has the same observable result.
  auto empty_body = reader(Bytes{0x34, 0x12, 0x00});
  auto empty_state = state_with();
  swf::read_swf_button_definition(empty_body, empty_state, 1.0);
  check(empty_state->defined_characters.has(0x1234), "an empty record list still defines the button");
  auto empty_sprite = empty_state->sprites.get(0x1234);
  check(empty_sprite.has_value(), "an empty record list still owns a one-frame sprite");
  if (empty_sprite.has_value() && empty_sprite.value()->frames.size() == 1.0) {
    check(empty_sprite.value()->frames.element(0).size() == 0.0, "an empty record list places nothing");
  }

  // An already-defined button is left untouched.
  auto duplicate_state = state_with();
  duplicate_state->defined_characters.add(0x1234);
  auto duplicate_body = reader(kButtonVersion1);
  swf::read_swf_button_definition(duplicate_body, duplicate_state, 1.0);
  check(!duplicate_state->sprites.has(0x1234), "a duplicate button id is not redefined");

  // A truncated record is invalid: the source returns before recording the button.
  auto short_body = reader(Bytes{0x34, 0x12, 0x01, 0x78});
  auto short_state = state_with();
  swf::read_swf_button_definition(short_body, short_state, 1.0);
  check(!short_state->sprites.has(0x1234), "an invalid reader defines nothing");
}

void legacy_place_object_consumer() {
  // No color transform follows the matrix, so the nullable transform is absent.
  auto body = reader(Bytes{0x78, 0x56, 0x01, 0x00, 0x00});
  auto state = state_with();
  auto timeline = timeline_state();
  const bool handled = swf::swf_place_object_handler->parse(body, swf::tag_place_object, state, timeline, std::nullopt);
  check(handled, "the place-object handler claims PlaceObject");
  auto placement = timeline->placements.get(1.0);
  check(placement.has_value(), "the legacy placement is stored by depth");
  if (!placement.has_value()) return;
  check(placement.value()->character_id == 0x5678, "the legacy placement keeps its character id");
  check(placement.value()->alpha == 1.0, "an absent transform leaves alpha 1");
  check(!placement.value()->color_adjustments.has_value(), "an absent transform leaves no adjustments");
  check(!placement.value()->color_transform_adjustments.has_value(), "both adjustment fields stay empty");
  check(placement.value()->blend_mode == types::blend_mode->normal, "the legacy placement uses Normal blending");
  check(placement.value()->matrix->a == 1.0, "the legacy matrix decodes to identity");
}

void legacy_place_object_tint_presence() {
  // The RGB-only transform (channel count 3) carries a tint; alpha stays 1 because there is no alpha channel.
  auto body = reader(Bytes{0x78, 0x56, 0x02, 0x00, 0x00, 0x49, 0x50});
  auto state = state_with();
  auto timeline = timeline_state();
  swf::read_legacy_place_object(body, timeline->placements);
  auto placement = timeline->placements.get(2.0);
  check(placement.has_value(), "the tinted legacy placement is stored");
  if (!placement.has_value()) return;
  check(placement.value()->alpha == 1.0, "a three-channel transform leaves alpha 1");
  check(placement.value()->color_adjustments.has_value(), "the tint produces adjustments");
  check(same_array_identity(placement.value()->color_adjustments, placement.value()->color_transform_adjustments),
        "both adjustment fields carry the same array, by identity");
}

void legacy_place_object_absence_and_invalid() {
  // Character id 0 is absence.
  auto zero_body = reader(Bytes{0x00, 0x00, 0x01, 0x00, 0x00});
  auto zero_timeline = timeline_state();
  swf::read_legacy_place_object(zero_body, zero_timeline->placements);
  check(zero_timeline->placements.size() == 0.0, "a zero character id stores no placement");

  // A truncated record is invalid rather than zero-filled into a placement.
  auto short_body = reader(Bytes{0x78, 0x56});
  auto short_timeline = timeline_state();
  swf::read_legacy_place_object(short_body, short_timeline->placements);
  check(short_timeline->placements.size() == 0.0, "an invalid record stores no placement");
}

}  // namespace

int main() {
  button_version1_consumer();
  button_version2_tint_presence();
  button_down_state_reports_skip();
  button_absence_and_duplicate();
  legacy_place_object_consumer();
  legacy_place_object_tint_presence();
  legacy_place_object_absence_and_invalid();
  if (failures != 0) {
    std::cerr << failures << " failure(s)\n";
    return 1;
  }
  std::cout << "generated swf retained-callee consumers passed\n";
  return 0;
}
