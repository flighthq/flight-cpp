// Focused native parity for the supported @flighthq/shape shape.ts, shapeCommands.ts,
// scale9ShapeCommands.ts, and shapeStrokeOutline.ts subsets at Flight a627849. The suite covers
// default construction, command-array identity, element copying, valid-stream counting, command
// encoding/defaults, path expansion, Scale9 remapping, stroke-outline routing, and node revision
// invalidation. Partial-node options, ShapeRuntime bounds methods, malformed command streams, and
// MorphShape paint bindings remain explicit override debt.
//
// MUTATION SENSITIVITY, measured 2026-10-14 against isolated one-at-a-time override changes:
// five mutants produced 3/3/3/2/1 failures for array assignment in place of element copying,
// an inverted array-identity guard, omitted copy invalidation, a false visibility default, and
// omitted command clearing, respectively. Each mutant was restored before the next measurement.
// Five shapeCommands mutants produced 5/1/2/1/2 failures for omitted bounds invalidation, a zero
// default thickness, rejection of a valid polygon, forced two-segment quarter arcs, and invalidating
// an elliptical-arc no-op. Each mutant was likewise restored before the next measurement.
// Six shapeStrokeOutline mutants produced 3/3/1/1/1/1 failures for wrong MOVE_TO X and Y operands,
// accepting a one-segment return, tightening closure epsilon, omitting texture fallback, and omitting
// style-span flush. Each mutant was restored before the next measurement.
// One scale9ShapeCommands mutant produced 10 failures when the distinct-array identity guard was
// inverted. It was restored after the measurement.
#include <flight/shape/scale9_shape_commands.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/shape/shape_stroke_outline.hpp>

#include <cmath>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace {

namespace shape = flight::shape;
namespace types = flight::types;

using Token = types::ShapeCommandToken;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

Token string_token(const char* value) {
  return Token{std::in_place, std::in_place_type<flight::String>,
               flight::String(value)};
}

Token number_token(double value) {
  return Token{std::in_place, std::in_place_type<double>, value};
}

Token array_token(flight::Array<double> value) {
  return Token{std::in_place, std::in_place_type<flight::Array<double>>,
               std::move(value)};
}

bool is_string(const Token& token, const char* expected) {
  return token.has_value() &&
         std::holds_alternative<flight::String>(token.value()) &&
         std::get<flight::String>(token.value()) == flight::String(expected);
}

bool is_number(const Token& token, double expected) {
  return token.has_value() && std::holds_alternative<double>(token.value()) &&
         std::get<double>(token.value()) == expected;
}

bool is_near_number(const Token& token, double expected) {
  return token.has_value() && std::holds_alternative<double>(token.value()) &&
         std::abs(std::get<double>(token.value()) - expected) < 1e-12;
}

bool is_bool(const Token& token, bool expected) {
  return token.has_value() && std::holds_alternative<bool>(token.value()) &&
         std::get<bool>(token.value()) == expected;
}

const flight::Array<double>* as_number_array(const Token& token) {
  if (!token.has_value() ||
      !std::holds_alternative<flight::Array<double>>(token.value())) {
    return nullptr;
  }
  return &std::get<flight::Array<double>>(token.value());
}

bool numbers_are(const flight::Array<double>& actual,
                 std::initializer_list<double> expected) {
  if (actual.size() != expected.size()) return false;
  std::size_t index = 0;
  for (const double value : expected) {
    if (actual[index++] != value) return false;
  }
  return true;
}

flight::Array<Token> remap_copy(
    flight::Array<Token> source,
    const flight::Ref<types::Scale9Mapper>& mapper) {
  flight::Array<Token> out(source.size());
  out[0] = string_token("sentinel");
  out[1] = number_token(static_cast<double>(source.size() - 2));
  shape::map_scale9_shape_commands(out, source, mapper);
  return out;
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return flight::structural_ref_cast<View>(value);
}

flight::Ref<types::Path> make_path(flight::Array<double> commands,
                                   flight::Array<double> data) {
  return flight::make_ref<types::Path>(types::Path{
      .commands = commands,
      .data = data,
      .winding = flight::String("nonZero"),
  });
}

void check_shape_defaults(const flight::Ref<types::Shape>& value,
                          const std::string& label) {
  check(value->data != nullptr && value->data->commands.empty() &&
            !value->data->entity_runtime_key.has_value(),
        label + " owns empty, unbound ShapeData");
  check(value->kind == types::shape_kind && value->enabled && value->visible &&
            value->alpha == 1.0,
        label + " has the Shape identity and visibility defaults");
  check(!value->name.has_value() && !value->blend_mode.has_value() &&
            !value->clip.has_value() && !value->material.has_value() &&
            !value->material_data.has_value(),
        label + " preserves absent optional appearance fields");
  check(value->pivot_x == 0.0 && value->pivot_y == 0.0 &&
            value->rotation == 0.0 && value->scale_x == 1.0 &&
            value->scale_y == 1.0 && value->skew_x == 0.0 &&
            value->skew_y == 0.0 && value->x == 0.0 && value->y == 0.0,
        label + " has exact transform defaults");
  check(value->entity_runtime_key.has_value(),
        label + " owns a Node2D runtime");
  if (!value->entity_runtime_key.has_value()) return;

  const auto& runtime = value->entity_runtime_key.value();
  check(runtime != nullptr && runtime->traits.has_value() &&
            runtime->traits.value() == types::node_2d_traits_key,
        label + " runtime carries the Node2D trait key");
  check(runtime->appearance_id == 0.0 && runtime->children_id == 0.0 &&
            runtime->local_bounds_id == 0.0 &&
            runtime->local_content_id == 0.0 &&
            runtime->local_transform_id == 0.0 &&
            runtime->parent_reference_id == 0.0 &&
            runtime->world_transform_id == 0.0,
        label + " runtime revisions start at source values");
  check(runtime->bounds_using_local_bounds_id == -1.0 &&
            runtime->bounds_using_local_transform_id == -1.0 &&
            runtime->local_bounds_using_local_bounds_id == -1.0 &&
            runtime->local_transform_using_local_transform_id == -1.0 &&
            runtime->world_bounds_using_local_bounds_id == -1.0 &&
            runtime->world_bounds_using_world_transform_id == -1.0 &&
            runtime->world_transform_using_local_transform_id == -1.0 &&
            runtime->world_transform_using_parent_transform_id == -1.0,
        label + " runtime cache dependencies start invalid");
  check(!runtime->children.has_value() && !runtime->parent.has_value() &&
            !runtime->interaction_signals.has_value() &&
            !runtime->interaction_state.has_value() &&
            !runtime->node_signals.has_value() &&
            !runtime->color_adjustments.has_value() &&
            !runtime->resolved_color_matrix.has_value() &&
            !runtime->resolved_color_scale_bias.has_value() &&
            !runtime->color_adjustments_unsupported,
        label + " runtime preserves absent relationship and cache fields");
}

void construction_defaults_and_identity() {
  const auto first = shape::create_shape();
  const auto second = shape::create_shape(std::nullopt);
  check_shape_defaults(first, "default createShape");
  check_shape_defaults(second, "absent-options createShape");
  check(first != second && first->data != second->data &&
            first->data->commands.identity() != second->data->commands.identity() &&
            first->entity_runtime_key.value() != second->entity_runtime_key.value(),
        "each shape owns isolated node, data, command-array, and runtime identities");

  const auto first_data = shape::create_shape_data();
  const auto second_data = shape::create_shape_data(std::nullopt);
  check(first_data != second_data && !first_data->entity_runtime_key.has_value() &&
            !second_data->entity_runtime_key.has_value() &&
            first_data->commands.empty() && second_data->commands.empty() &&
            first_data->commands.identity() != second_data->commands.identity(),
        "createShapeData allocates isolated empty command arrays");
}

void copy_preserves_destination_identity() {
  const auto source = shape::create_shape();
  source->data->commands = flight::Array<Token>{
      string_token("moveTo"), number_token(2.0), number_token(10.0),
      number_token(20.0), string_token("endFill"), number_token(0.0)};

  const auto target = shape::create_shape();
  target->data->commands = flight::Array<Token>{
      string_token("lineTo"), number_token(2.0), number_token(-1.0),
      number_token(-2.0)};
  const void* target_commands = target->data->commands.identity();
  const auto& target_runtime = target->entity_runtime_key.value();

  shape::copy_shape_commands(target, source);
  check(target->data->commands.identity() == target_commands &&
            target->data->commands.identity() != source->data->commands.identity(),
        "copy preserves the destination command-array identity without aliasing source");
  check(target->data->commands.size() == 6 &&
            is_string(target->data->commands[0], "moveTo") &&
            is_number(target->data->commands[1], 2.0) &&
            is_number(target->data->commands[2], 10.0) &&
            is_number(target->data->commands[3], 20.0) &&
            is_string(target->data->commands[4], "endFill") &&
            is_number(target->data->commands[5], 0.0),
        "copy preserves command token order and values");
  check(target_runtime->local_content_id == 1.0 &&
            target_runtime->local_bounds_id == 1.0,
        "copy invalidates local content and bounds exactly once");

  source->data->commands[2] = number_token(99.0);
  check(is_number(target->data->commands[2], 10.0),
        "later source element replacement cannot mutate the copied destination");

  const auto aliased = shape::create_shape();
  aliased->data->commands = source->data->commands;
  const void* shared_commands = source->data->commands.identity();
  shape::copy_shape_commands(aliased, source);
  check(aliased->data->commands.identity() == shared_commands &&
            source->data->commands.identity() == shared_commands &&
            is_number(source->data->commands[2], 99.0),
        "copy between owners sharing one command array is a data no-op");
  check(aliased->entity_runtime_key.value()->local_content_id == 1.0 &&
            aliased->entity_runtime_key.value()->local_bounds_id == 1.0,
        "aliased-array copy still invalidates the destination exactly once");

  const void* self_commands = target->data->commands.identity();
  shape::copy_shape_commands(target, target);
  check(target->data->commands.identity() == self_commands &&
            target->data->commands.size() == 6 &&
            is_number(target->data->commands[2], 10.0),
        "self-copy neither clears nor replaces the command stream");
  check(target_runtime->local_content_id == 2.0 &&
            target_runtime->local_bounds_id == 2.0,
        "self-copy follows the source operation's single invalidation");
}

void readonly_copy_retains_live_source_owner() {
  using ReadonlyShape = flight::StructuralRef<flight::RowReadonly<
      flight::RowOf<flight::Ref<types::Shape>>>>;
  const auto source = shape::create_shape();
  const ReadonlyShape view{source};
  const auto original_data = source->data;
  const auto replacement_data = shape::create_shape_data();
  replacement_data->commands = flight::Array<Token>{
      string_token("moveTo"), number_token(2.0), number_token(7.0), number_token(8.0)};
  source->data = replacement_data;
  const void* source_commands = replacement_data->commands.identity();
  const auto target = shape::create_shape();
  const void* target_commands = target->data->commands.identity();

  shape::copy_shape_commands(target, view);
  check(view.shared_exact_object<types::Shape>() == source &&
            source->data == replacement_data && original_data->commands.empty() &&
            source->data->commands.identity() == source_commands,
        "readonly copy retains the exact source owner and reads its replaced data cell");
  check(target->data->commands.identity() == target_commands &&
            target_commands != source_commands && target->data->commands.size() == 4 &&
            is_number(target->data->commands[2], 7.0) &&
            is_number(target->data->commands[3], 8.0),
        "readonly copy preserves destination storage and reads live source token order");
  check(target->entity_runtime_key.value()->local_content_id == 1.0 &&
            target->entity_runtime_key.value()->local_bounds_id == 1.0 &&
            source->entity_runtime_key.value()->local_content_id == 0.0 &&
            source->entity_runtime_key.value()->local_bounds_id == 0.0,
        "readonly copy invalidates only the destination exactly once");

  source->data->commands[2] = number_token(19.0);
  shape::copy_shape_commands(target, view);
  check(is_number(target->data->commands[2], 19.0),
        "a retained readonly view observes later source command mutations");

  const auto aliased = shape::create_shape();
  aliased->data->commands = source->data->commands;
  shape::copy_shape_commands(aliased, view);
  shape::copy_shape_commands(source, view);
  check(aliased->data->commands.identity() == source_commands &&
            source->data->commands.identity() == source_commands &&
            is_number(source->data->commands[2], 19.0) &&
            aliased->entity_runtime_key.value()->local_content_id == 1.0 &&
            aliased->entity_runtime_key.value()->local_bounds_id == 1.0 &&
            source->entity_runtime_key.value()->local_content_id == 1.0 &&
            source->entity_runtime_key.value()->local_bounds_id == 1.0,
        "readonly shared-array and self copies preserve storage and invalidate once");

  using LegacyCopy = void (*)(const flight::Ref<types::Shape>&,
                              const flight::Ref<types::Shape>&);
  const LegacyCopy legacy_copy = &shape::copy_shape_commands;
  source->data->commands.clear();
  legacy_copy(target, source);
  check(target->data->commands.empty() &&
            target->data->commands.identity() == target_commands &&
            target->entity_runtime_key.value()->local_content_id == 3.0 &&
            target->entity_runtime_key.value()->local_bounds_id == 3.0,
        "the legacy typed entry point retains empty-copy and invalidation behavior");
}

void clear_count_and_empty() {
  const auto value = shape::create_shape();
  value->data->commands = flight::Array<Token>{
      string_token("moveTo"), number_token(2.0), number_token(1.0),
      number_token(2.0), string_token("endFill"), number_token(0.0)};
  check(shape::get_shape_command_count(value) == 2.0 &&
            !shape::is_shape_empty(value),
        "valid command streams report command count and non-empty state");

  const void* commands = value->data->commands.identity();
  const auto& runtime = value->entity_runtime_key.value();
  shape::clear_shape_commands(value);
  check(value->data->commands.identity() == commands &&
            value->data->commands.empty() && shape::is_shape_empty(value) &&
            shape::get_shape_command_count(value) == 0.0,
        "clear preserves command-array identity and empties the stream");
  check(runtime->local_content_id == 1.0 && runtime->local_bounds_id == 1.0,
        "clear invalidates local content and bounds exactly once");
}

void command_defaults_and_invalidation() {
  const auto value = shape::create_shape();
  const auto& runtime = value->entity_runtime_key.value();

  shape::append_shape_begin_fill(value, 0x123456);
  check(value->data->commands.size() == 4 &&
            is_string(value->data->commands[0], "beginFill") &&
            is_number(value->data->commands[1], 2.0) &&
            is_number(value->data->commands[2], 0x123456) &&
            is_number(value->data->commands[3], 1.0),
        "beginFill appends its key, arity, color, and default alpha in order");
  check(runtime->local_content_id == 1.0 && runtime->local_bounds_id == 1.0,
        "beginFill invalidates both Shape node revisions exactly once");

  runtime->local_content_id = 4294967295.0;
  runtime->local_bounds_id = 4294967295.0;
  shape::append_shape_line_style(value, std::nullopt, 0xabcdef);
  check(value->data->commands.size() == 14 &&
            is_string(value->data->commands[4], "lineStyle") &&
            is_number(value->data->commands[5], 8.0) &&
            is_number(value->data->commands[6], 1.0) &&
            is_number(value->data->commands[7], 0xabcdef) &&
            is_number(value->data->commands[8], 1.0) &&
            is_bool(value->data->commands[9], false) &&
            is_string(value->data->commands[10], "normal") &&
            is_string(value->data->commands[11], "none") &&
            is_string(value->data->commands[12], "round") &&
            is_number(value->data->commands[13], 3.0),
        "lineStyle preserves every source default after the explicit nullopt boundary");
  check(runtime->local_content_id == 0.0 && runtime->local_bounds_id == 0.0,
        "Shape command invalidation wraps both revisions as unsigned 32-bit values");

  const auto unbound = shape::create_shape();
  unbound->entity_runtime_key = std::nullopt;
  bool threw = false;
  try {
    shape::append_shape_begin_fill(unbound, 7.0);
  } catch (const flight::Error& error) {
    threw = error.message() ==
            flight::String("Cannot invalidate a Shape without its node runtime");
  }
  check(threw && unbound->data->commands.size() == 4,
        "an unbound Shape fails loudly at invalidation after preserving the source append order");
}

void polygon_and_arc_expansion() {
  const auto short_polygon = shape::create_shape();
  shape::append_shape_polygon(short_polygon, flight::Array<double>{0.0, 0.0});
  check(short_polygon->data->commands.empty() &&
            short_polygon->entity_runtime_key.value()->local_content_id == 0.0 &&
            short_polygon->entity_runtime_key.value()->local_bounds_id == 0.0,
        "a polygon with fewer than two points is a command and invalidation no-op");

  const flight::Array<double> points{0.0, 1.0, 10.0, 11.0, 20.0, 21.0};
  const auto polygon = shape::create_shape();
  shape::append_shape_polygon(polygon, points);
  check(polygon->data->commands.size() == 16 &&
            is_string(polygon->data->commands[0], "moveTo") &&
            is_number(polygon->data->commands[2], 0.0) &&
            is_number(polygon->data->commands[3], 1.0) &&
            is_string(polygon->data->commands[4], "lineTo") &&
            is_number(polygon->data->commands[6], 10.0) &&
            is_number(polygon->data->commands[7], 11.0) &&
            is_string(polygon->data->commands[8], "lineTo") &&
            is_number(polygon->data->commands[10], 20.0) &&
            is_number(polygon->data->commands[11], 21.0) &&
            is_string(polygon->data->commands[12], "lineTo") &&
            is_number(polygon->data->commands[14], 0.0) &&
            is_number(polygon->data->commands[15], 1.0),
        "polygon expands each point and closes back to the first point");
  check(polygon->entity_runtime_key.value()->local_content_id == 1.0 &&
            polygon->entity_runtime_key.value()->local_bounds_id == 1.0,
        "polygon expansion invalidates both revisions once rather than per emitted segment");

  const auto polyline = shape::create_shape();
  shape::append_shape_polyline(polyline, points);
  check(polyline->data->commands.size() == 12 &&
            is_string(polyline->data->commands[0], "moveTo") &&
            is_string(polyline->data->commands[4], "lineTo") &&
            is_string(polyline->data->commands[8], "lineTo") &&
            is_number(polyline->data->commands[10], 20.0) &&
            is_number(polyline->data->commands[11], 21.0),
        "polyline expands the same points without a closing segment");

  const auto arc = shape::create_shape();
  shape::append_shape_arc(arc, 10.0, 20.0, 2.0, 0.0,
                          flight::pi / 2.0);
  check(arc->data->commands.size() == 12 &&
            is_string(arc->data->commands[0], "moveTo") &&
            is_near_number(arc->data->commands[2], 12.0) &&
            is_near_number(arc->data->commands[3], 20.0) &&
            is_string(arc->data->commands[4], "cubicCurveTo") &&
            is_number(arc->data->commands[5], 6.0) &&
            is_near_number(arc->data->commands[10], 10.0) &&
            is_near_number(arc->data->commands[11], 22.0),
        "a quarter arc expands to one cubic with exact start and end points");
  check(arc->entity_runtime_key.value()->local_content_id == 1.0 &&
            arc->entity_runtime_key.value()->local_bounds_id == 1.0,
        "arc expansion invalidates both revisions exactly once");

  const auto elliptical = shape::create_shape();
  shape::append_shape_elliptical_arc_to(elliptical, 2.0, 3.0, 0.0,
                                        false, true, 0.0, 0.0);
  check(elliptical->data->commands.empty() &&
            elliptical->entity_runtime_key.value()->local_content_id == 0.0 &&
            elliptical->entity_runtime_key.value()->local_bounds_id == 0.0,
        "an elliptical arc whose endpoint equals the current pen is a full no-op");
  shape::append_shape_elliptical_arc_to(elliptical, 0.0, 3.0, 0.0,
                                        false, true, 4.0, 5.0);
  check(elliptical->data->commands.size() == 4 &&
            is_string(elliptical->data->commands[0], "lineTo") &&
            is_number(elliptical->data->commands[2], 4.0) &&
            is_number(elliptical->data->commands[3], 5.0) &&
            elliptical->entity_runtime_key.value()->local_content_id == 1.0 &&
            elliptical->entity_runtime_key.value()->local_bounds_id == 1.0,
        "a zero-radius elliptical arc falls back to lineTo and invalidates once");
}

void scale9_command_mapping_and_identity() {
  const auto mapper = flight::make_ref<types::Scale9Mapper>(
      types::Scale9Mapper{
          .map_x = [](double x) { return x * 2.0; },
          .map_y = [](double y) { return y * 3.0; },
      });

  flight::Array<Token> style_source{
      string_token("beginFill"), number_token(2.0),
      number_token(0xff0000ff), number_token(1.0)};
  flight::Array<Token> style_out(style_source.size());
  style_out[0] = string_token("sentinel");
  style_out[1] = number_token(2.0);
  const void* source_identity = style_source.identity();
  const void* out_identity = style_out.identity();
  shape::map_scale9_shape_commands(style_out, style_source, mapper);
  check(style_source.identity() == source_identity &&
            style_out.identity() == out_identity &&
            style_out.identity() != style_source.identity(),
        "Scale9 mapping copies distinct arrays without replacing either owner identity");
  check(style_out.size() == 4 && is_string(style_out[0], "beginFill") &&
            is_number(style_out[1], 2.0) &&
            is_number(style_out[2], 0xff0000ff) &&
            is_number(style_out[3], 1.0),
        "Scale9 mapping passes style commands through unchanged");
  style_source[2] = number_token(17.0);
  check(is_number(style_out[2], 0xff0000ff),
        "Scale9 mapping copies distinct command elements instead of aliasing the source array");

  const auto move = remap_copy(
      flight::Array<Token>{string_token("moveTo"), number_token(2.0),
                           number_token(10.0), number_token(20.0)},
      mapper);
  check(move.size() == 4 && is_number(move[2], 20.0) &&
            is_number(move[3], 60.0),
        "Scale9 mapping remaps moveTo coordinates");

  const auto line = remap_copy(
      flight::Array<Token>{string_token("lineTo"), number_token(2.0),
                           number_token(100.0), number_token(50.0)},
      mapper);
  check(line.size() == 4 && is_number(line[2], 200.0) &&
            is_number(line[3], 150.0),
        "Scale9 mapping remaps lineTo coordinates");

  const auto rectangle = remap_copy(
      flight::Array<Token>{string_token("drawRectangle"), number_token(4.0),
                           number_token(10.0), number_token(20.0),
                           number_token(50.0), number_token(30.0)},
      mapper);
  check(rectangle.size() == 6 && is_number(rectangle[2], 20.0) &&
            is_number(rectangle[3], 60.0) &&
            is_number(rectangle[4], 100.0) &&
            is_number(rectangle[5], 90.0),
        "Scale9 mapping remaps rectangle corners and recomputes its size");

  const auto rounded = remap_copy(
      flight::Array<Token>{string_token("drawRoundedRectangle"),
                           number_token(5.0), number_token(10.0),
                           number_token(20.0), number_token(50.0),
                           number_token(30.0), number_token(20.0)},
      mapper);
  check(rounded.size() == 7 && is_number(rounded[2], 20.0) &&
            is_number(rounded[3], 60.0) &&
            is_number(rounded[4], 100.0) &&
            is_number(rounded[5], 90.0) &&
            is_number(rounded[6], 20.0),
        "Scale9 mapping remaps rounded-rectangle corners without changing its radius");

  const auto circle = remap_copy(
      flight::Array<Token>{string_token("drawCircle"), number_token(3.0),
                           number_token(50.0), number_token(50.0),
                           number_token(25.0)},
      mapper);
  check(circle.size() == 5 && is_number(circle[2], 100.0) &&
            is_number(circle[3], 150.0) && is_number(circle[4], 25.0),
        "Scale9 mapping remaps a circle center without changing its radius");

  const auto ellipse = remap_copy(
      flight::Array<Token>{string_token("drawEllipse"), number_token(4.0),
                           number_token(50.0), number_token(25.0),
                           number_token(50.0), number_token(25.0)},
      mapper);
  check(ellipse.size() == 6 && is_number(ellipse[2], 100.0) &&
            is_number(ellipse[3], 75.0) && is_number(ellipse[4], 100.0) &&
            is_number(ellipse[5], 75.0),
        "Scale9 mapping remaps ellipse extrema and recomputes its center and radii");

  const auto curves = remap_copy(
      flight::Array<Token>{
          string_token("quadraticCurveTo"), number_token(4.0),
          number_token(10.0), number_token(20.0), number_token(30.0),
          number_token(40.0), string_token("cubicCurveTo"),
          number_token(6.0), number_token(1.0), number_token(2.0),
          number_token(3.0), number_token(4.0), number_token(5.0),
          number_token(6.0)},
      mapper);
  check(curves.size() == 14 && is_number(curves[2], 20.0) &&
            is_number(curves[3], 60.0) && is_number(curves[4], 60.0) &&
            is_number(curves[5], 120.0) && is_number(curves[8], 2.0) &&
            is_number(curves[9], 6.0) && is_number(curves[10], 6.0) &&
            is_number(curves[11], 12.0) && is_number(curves[12], 10.0) &&
            is_number(curves[13], 18.0),
        "Scale9 mapping remaps every quadratic and cubic control and endpoint");

  flight::Array<Token> in_place{
      string_token("moveTo"), number_token(2.0), number_token(7.0),
      number_token(11.0)};
  const void* in_place_identity = in_place.identity();
  shape::map_scale9_shape_commands(in_place, in_place, mapper);
  check(in_place.identity() == in_place_identity && in_place.size() == 4 &&
            is_number(in_place[2], 14.0) && is_number(in_place[3], 33.0),
        "Scale9 mapping preserves an aliased command array while rewriting it in place");

  const auto path = remap_copy(
      flight::Array<Token>{
          string_token("drawPath"), number_token(2.0),
          array_token(flight::Array<double>{1.0, 3.0, 4.0, 6.0}),
          array_token(flight::Array<double>{
              1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,
              9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0})},
      mapper);
  const auto* path_data = path.size() == 4 ? as_number_array(path[3]) : nullptr;
  check(path_data != nullptr &&
            numbers_are(*path_data,
                        {2.0, 6.0, 6.0, 12.0, 10.0, 18.0, 7.0, 8.0,
                         18.0, 30.0, 22.0, 36.0, 26.0, 42.0, 30.0, 48.0}),
        "Scale9 mapping remaps drawPath data for every represented path verb");
}

void stroke_outline_regions_and_fallbacks() {
  const auto open = shape::create_shape();
  shape::append_shape_line_style(open, 8.0, 0x123456ff, 0.5, false,
                                 flight::String("normal"),
                                 flight::String("square"),
                                 flight::String("bevel"), 4.0);
  shape::append_shape_move_to(open, 0.0, 0.0);
  shape::append_shape_line_to(open, 20.0, 0.0);
  const auto open_regions =
      shape::get_shape_stroke_outline_regions(open->data->commands);
  check(open_regions.has_value() && open_regions->size() == 1 &&
            open_regions->element(0.0)->color == 0x123456ff &&
            open_regions->element(0.0)->alpha == 0.5 &&
            !open_regions->element(0.0)->path->commands.empty() &&
            !open_regions->element(0.0)->path->data.empty(),
        "a solid open stroke becomes one non-empty fill region with its color and alpha");

  const auto arc = shape::create_shape();
  shape::append_shape_line_style(arc, 8.0, 0x123456ff);
  shape::append_shape_move_to(arc, 0.0, 0.0);
  shape::append_shape_elliptical_arc_to(arc, 50.0, 25.0, 0.0, false,
                                        true, 100.0, 0.0);
  const auto arc_regions =
      shape::get_shape_stroke_outline_regions(arc->data->commands);
  check(arc_regions.has_value() && arc_regions->size() == 1 &&
            !arc_regions->element(0.0)->path->commands.empty(),
        "an open elliptical arc becomes one fillable stroke outline");

  const auto closed = shape::create_shape();
  shape::append_shape_line_style(closed, 8.0, 0x000000ff);
  shape::append_shape_rectangle(closed, 0.0, 0.0, 20.0, 10.0);
  check(!shape::get_shape_stroke_outline_regions(closed->data->commands)
             .has_value(),
        "a geometrically closed stroke defers to the renderer fallback");

  const flight::Array<Token> gradient{
      string_token("lineGradientStyle"), number_token(1.0),
      number_token(0.0)};
  const flight::Array<Token> texture{
      string_token("lineTextureStyle"), number_token(1.0),
      number_token(0.0)};
  check(!shape::get_shape_stroke_outline_regions(gradient).has_value() &&
            !shape::get_shape_stroke_outline_regions(texture).has_value(),
        "gradient and texture strokes both defer before outline construction");

  const auto disabled = shape::create_shape();
  shape::append_shape_line_style(disabled, 0.0, 0xabcdef);
  shape::append_shape_move_to(disabled, 1.0, 2.0);
  shape::append_shape_line_to(disabled, 3.0, 4.0);
  const auto disabled_regions =
      shape::get_shape_stroke_outline_regions(disabled->data->commands);
  check(disabled_regions.has_value() && disabled_regions->empty(),
        "a zero-width lineStyle disables outline collection without forcing fallback");

  const auto spans = shape::create_shape();
  shape::append_shape_line_style(spans, 2.0, 0x111111, 0.25);
  shape::append_shape_move_to(spans, 0.0, 0.0);
  shape::append_shape_line_to(spans, 10.0, 0.0);
  shape::append_shape_line_style(spans, 3.0, 0x222222, 0.75);
  shape::append_shape_move_to(spans, 20.0, 0.0);
  shape::append_shape_line_to(spans, 30.0, 0.0);
  const auto span_regions =
      shape::get_shape_stroke_outline_regions(spans->data->commands);
  check(span_regions.has_value() && span_regions->size() == 2 &&
            span_regions->element(0.0)->color == 0x111111 &&
            span_regions->element(0.0)->alpha == 0.25 &&
            span_regions->element(1.0)->color == 0x222222 &&
            span_regions->element(1.0)->alpha == 0.75,
        "a new lineStyle flushes the previous span before collecting the next one");

  const auto seeded = shape::create_shape();
  shape::append_shape_move_to(seeded, 7.0, 9.0);
  shape::append_shape_line_style(seeded, 2.0, 0x333333);
  shape::append_shape_line_to(seeded, 20.0, 9.0);
  const auto seeded_regions =
      shape::get_shape_stroke_outline_regions(seeded->data->commands);
  check(seeded_regions.has_value() && seeded_regions->size() == 1 &&
            !seeded_regions->element(0.0)->path->commands.empty(),
        "a stroke span beginning with lineTo seeds its centerline from the running pen");
}

void stroke_centerline_closure() {
  const double move = types::path_command->move_to;
  const double wide_move = types::path_command->wide_move_to;
  const double line = types::path_command->line_to;
  const double close = types::path_command->close;

  const auto near_closed = make_path(
      flight::Array<double>{move, line, line},
      flight::Array<double>{10.0, 20.0, 30.0, 20.0,
                            10.0 + 0.5e-6, 20.0 - 0.5e-6});
  check(shape::is_centerline_closed(readonly(near_closed)),
        "two segments returning within epsilon close a centerline");

  const auto one_segment = make_path(
      flight::Array<double>{move, line},
      flight::Array<double>{10.0, 20.0, 10.0, 20.0});
  check(!shape::is_centerline_closed(readonly(one_segment)),
        "one zero-area return segment does not form a closed centerline");

  const auto completed_before_move = make_path(
      flight::Array<double>{move, line, line, move},
      flight::Array<double>{3.0, 4.0, 8.0, 4.0, 3.0, 4.0, 50.0, 60.0});
  check(shape::is_centerline_closed(readonly(completed_before_move)),
        "closure is detected before a following subpath resets its start");

  const auto wide_closed = make_path(
      flight::Array<double>{wide_move, line, line},
      flight::Array<double>{-1.0, -2.0, 12.0, 21.0, 30.0, 21.0,
                            12.0, 21.0});
  check(shape::is_centerline_closed(readonly(wide_closed)),
        "wideMoveTo uses its trailing coordinate pair as the subpath start");

  const auto explicit_close = make_path(flight::Array<double>{close},
                                        flight::Array<double>{});
  check(shape::is_centerline_closed(readonly(explicit_close)),
        "an explicit close verb always closes the centerline");
}

}  // namespace

int main() {
  construction_defaults_and_identity();
  copy_preserves_destination_identity();
  readonly_copy_retains_live_source_owner();
  clear_count_and_empty();
  command_defaults_and_invalidation();
  polygon_and_arc_expansion();
  scale9_command_mapping_and_identity();
  stroke_outline_regions_and_fallbacks();
  stroke_centerline_closure();

  if (failures == 0) {
    std::cout << "shape construction, command emission, Scale9 mapping, and stroke outlines match the supported Flight subset\n";
  }
  return failures == 0 ? 0 : 1;
}
