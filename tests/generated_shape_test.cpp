// Focused native parity for the supported @flighthq/shape shape.ts subset at Flight a627849.
// The suite covers default construction, command-array identity, element copying, valid-stream
// counting, and node revision invalidation. Partial-node options, ShapeRuntime bounds methods,
// malformed command streams, and MorphShape paint bindings remain explicit override debt.
//
// MUTATION SENSITIVITY, measured 2026-10-14 against isolated one-at-a-time override changes:
// five mutants produced 3/3/3/2/1 failures for array assignment in place of element copying,
// an inverted array-identity guard, omitted copy invalidation, a false visibility default, and
// omitted command clearing, respectively. Each mutant was restored before the next measurement.
#include <flight/shape/shape.hpp>

#include <iostream>
#include <optional>
#include <string>
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

bool is_string(const Token& token, const char* expected) {
  return token.has_value() &&
         std::holds_alternative<flight::String>(token.value()) &&
         std::get<flight::String>(token.value()) == flight::String(expected);
}

bool is_number(const Token& token, double expected) {
  return token.has_value() && std::holds_alternative<double>(token.value()) &&
         std::get<double>(token.value()) == expected;
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
            runtime->traits.value() == types::node2_dtraits_key,
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

}  // namespace

int main() {
  construction_defaults_and_identity();
  copy_preserves_destination_identity();
  clear_count_and_empty();

  if (failures == 0) {
    std::cout << "shape construction and command ownership match the supported Flight subset\n";
  }
  return failures == 0 ? 0 : 1;
}
