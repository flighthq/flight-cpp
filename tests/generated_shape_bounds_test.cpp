#include <flight/shape/shape_bounds.hpp>
#include <flight/shape/shape_bounds_registry.hpp>

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// Mutation evidence (2026-10-13): ten one-site mutants were rebuilt and run;
// all ten were killed. They cloned cursor storage, ignored cursor offset, froze
// cursor length, dropped flush forwarding, dropped the structural fill callback,
// selected stroke bounds in geometry mode, replaced the guard's Shape owner,
// stopped missing-key deduplication, wrote the output before a malformed count
// threw, and dropped guard dispatch.

namespace {

using Argument = flight::shape::ShapeCommandArgument;
using Command = flight::types::ShapeBoundsCommand<flight::String>;
using CommandView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<flight::types::ShapeBoundsCommand<
        flight::types::ShapeCommandKey>>>>>;
using Cursor = flight::types::ShapeCommandArgumentCursor;
using CursorView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<Cursor>>>>;
using Rectangle = flight::types::Rectangle;
using Shape = flight::types::Shape;
using Token = flight::types::ShapeCommandToken;

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

Token number(double value) {
  return Token{std::in_place, std::in_place_type<double>, value};
}

Token string(const char* value) {
  return Token{std::in_place, std::in_place_type<flight::String>,
               flight::String(value)};
}

void append_command(const flight::Ref<Shape>& shape, const char* key,
                    std::initializer_list<Token> arguments) {
  shape->data->commands.push(string(key));
  shape->data->commands.push(number(static_cast<double>(arguments.size())));
  for (const auto& argument : arguments) shape->data->commands.push(argument);
}

flight::Ref<Rectangle> rectangle(double x = 0.0, double y = 0.0,
                                 double width = 0.0, double height = 0.0) {
  return flight::make_ref<Rectangle>(Rectangle{
      .entity_runtime_key = std::nullopt,
      .height = height,
      .width = width,
      .x = x,
      .y = y,
  });
}

bool is_rectangle(const flight::Ref<Rectangle>& value, double x, double y,
                  double width, double height) {
  return value->x == x && value->y == y && value->width == width &&
         value->height == height;
}

flight::Ref<Shape> make_shape() {
  auto shape = flight::make_ref<Shape>();
  shape->data = flight::make_ref<flight::types::ShapeData>();
  return shape;
}

double argument_number(const CursorView& cursor, double index) {
  const auto exact_cursor =
      flight::structural_ref_cast<flight::Ref<Cursor>>(cursor);
  return std::get<double>(exact_cursor->get_argument(index));
}

double argument_length(const CursorView& cursor) {
  return flight::structural_ref_cast<flight::Ref<Cursor>>(cursor)->length;
}

void install_command(const flight::Ref<Command>& command) {
  using WritableCommand = flight::StructuralRef<
      flight::RowWritable<flight::RowOf<flight::Ref<Command>>>>;
  flight::shape::commands.set(
      command->key,
      flight::structural_ref_cast<CommandView>(WritableCommand(command)));
}

}  // namespace

int main() {
  using flight::shape::clear_shape_bounds_commands;
  using flight::shape::compute_shape_bounds_rectangle;
  using flight::shape::create_shape_command_argument_cursor;
  using flight::shape::default_shape_bounds_flush;
  using flight::shape::explain_shape_bounds;
  using flight::shape::set_shape_bounds_guard;
  using flight::shape::set_shape_command_argument_cursor;

  // The cursor retains the source Array handle. A clone here would make this
  // post-construction mutation invisible and change JavaScript reference semantics.
  flight::Array<Token> cursor_tokens;
  cursor_tokens.push(number(11.0));
  cursor_tokens.push(string("tail"));
  cursor_tokens.push(Token{});
  const auto cursor = create_shape_command_argument_cursor(cursor_tokens);
  cursor_tokens.element(0.0) = number(17.0);
  set_shape_command_argument_cursor(cursor, 0.0, 3.0);
  if (!check(cursor->length == 3.0, "cursor length did not track the selected command") ||
      !check(std::get<double>(cursor->get_argument(0.0)) == 17.0,
             "cursor cloned its token array instead of retaining the live handle") ||
      !check(std::get<flight::String>(cursor->get_argument(1.0)) == flight::String("tail"),
             "cursor changed argument order or token value") ||
      !check(std::holds_alternative<flight::Undefined>(cursor->get_argument(2.0)),
             "an absent token did not read as undefined") ||
      !check(std::holds_alternative<flight::Undefined>(cursor->get_argument(-1.0)) &&
                 std::holds_alternative<flight::Undefined>(cursor->get_argument(3.0)),
             "cursor bounds did not return undefined")) {
    return 1;
  }
  set_shape_command_argument_cursor(cursor, 1.0, 1.0);
  if (!check(cursor->length == 1.0 &&
                 std::get<flight::String>(cursor->get_argument(0.0)) ==
                     flight::String("tail"),
             "cursor offset or replacement length was ignored")) {
    return 1;
  }

  int flush_calls = 0;
  auto flush_context = flight::make_ref<flight::types::ShapeBoundsContext>();
  flush_context->flush_path = [&] { ++flush_calls; };
  const CursorView cursor_view(
      std::static_pointer_cast<flight::types::ShapeCommandArgumentCursor>(cursor));
  default_shape_bounds_flush(flush_context, cursor_view);
  if (!check(flush_calls == 1,
             "the two-argument default flush handler did not forward exactly once")) {
    return 1;
  }

  clear_shape_bounds_commands();
  std::vector<std::string> calls;
  bool fill_arguments_ok = true;
  bool stroke_arguments_ok = true;
  auto box = flight::make_ref<Command>();
  box->key = flight::String("box");
  box->fill_bounds = [&](flight::Ref<flight::types::ShapeBoundsContext> context,
                         CursorView arguments) {
    calls.emplace_back("fill");
    fill_arguments_ok =
        argument_length(arguments) == 4.0 &&
        argument_number(arguments, 0.0) == 10.0 &&
        argument_number(arguments, 1.0) == 20.0 &&
        argument_number(arguments, 2.0) == 30.0 &&
        argument_number(arguments, 3.0) == 40.0;
    context->draw_rectangle(argument_number(arguments, 0.0),
                            argument_number(arguments, 1.0),
                            argument_number(arguments, 2.0),
                            argument_number(arguments, 3.0));
  };
  box->stroke_bounds = [&](flight::Ref<flight::types::ShapeBoundsContext> context,
                           CursorView arguments) {
    calls.emplace_back("stroke");
    stroke_arguments_ok =
        argument_length(arguments) == 4.0 &&
        argument_number(arguments, 0.0) == 10.0 &&
        argument_number(arguments, 3.0) == 40.0;
    context->set_stroke_style(2.0, flight::make_ref<flight::String>("none"),
                              flight::make_ref<flight::String>("round"), 10.0);
    context->draw_rectangle(argument_number(arguments, 0.0),
                            argument_number(arguments, 1.0),
                            argument_number(arguments, 2.0),
                            argument_number(arguments, 3.0));
  };
  install_command(box);

  const auto shape = make_shape();
  append_command(shape, "box", {number(10.0), number(20.0), number(30.0), number(40.0)});

  const auto geometry = rectangle();
  const auto geometry_mode = flight::make_ref<flight::String>("geometry");
  if (!check(compute_shape_bounds_rectangle(geometry, shape, geometry_mode),
             "a registered command was reported incomplete") ||
      !check(fill_arguments_ok, "fill callback did not receive the exact cursor slice") ||
      !check(stroke_arguments_ok, "stroke callback setup failed before its mode was exercised") ||
      !check(calls == std::vector<std::string>{"fill"},
             "geometry mode invoked stroke bounds or changed callback order") ||
      !check(is_rectangle(geometry, 10.0, 20.0, 30.0, 40.0),
             "fill bounds did not reach the output rectangle")) {
    return 1;
  }

  calls.clear();
  const auto ink = rectangle();
  if (!check(compute_shape_bounds_rectangle(ink, shape),
             "ink traversal reported a registered command incomplete") ||
      !check(fill_arguments_ok && stroke_arguments_ok,
             "structural registry dispatch changed cursor values") ||
      !check(calls == std::vector<std::string>{"fill", "stroke"},
             "ink mode did not preserve fill-before-stroke order") ||
      !check(is_rectangle(ink, 9.0, 19.0, 32.0, 42.0),
             "stroke bounds were absent or used the wrong width")) {
    return 1;
  }

  const auto unknown = make_shape();
  append_command(unknown, "missingA", {});
  append_command(unknown, "missingB", {});
  append_command(unknown, "missingA", {});
  int guard_calls = 0;
  bool guard_owner_ok = true;
  bool guard_values_ok = true;
  flight::types::ShapeBoundsGuard guard =
      [&](flight::StructuralRef<flight::RowReadonly<
                flight::RowOf<flight::Ref<Shape>>>> seen,
          flight::String mode, flight::String key) {
    ++guard_calls;
    guard_owner_ok = guard_owner_ok &&
                     flight::structural_ref_cast<flight::Ref<Shape>>(seen) == unknown;
    guard_values_ok = guard_values_ok && mode == flight::String("ink") &&
                      (key == flight::String("missingA") ||
                       key == flight::String("missingB"));
      };
  set_shape_bounds_guard(guard);
  const auto unknown_out = rectangle(91.0, 92.0, 93.0, 94.0);
  if (!check(!compute_shape_bounds_rectangle(unknown_out, unknown),
             "missing commands were reported complete") ||
      !check(guard_calls == 3 && guard_values_ok,
             "the guard did not observe every missing command in source order") ||
      !check(guard_owner_ok,
             "the guard received a replacement row instead of the exact Shape owner") ||
      !check(is_rectangle(unknown_out, 0.0, 0.0, 0.0, 0.0),
             "an empty incomplete traversal did not write the source empty bounds")) {
    return 1;
  }

  const auto explanation = explain_shape_bounds(unknown);
  if (!check(!explanation->complete && explanation->missing_command_keys.size() == 2,
             "explanation did not suppress duplicate missing keys") ||
      !check(explanation->missing_command_keys.element(0.0) == flight::String("missingA") &&
                 explanation->missing_command_keys.element(1.0) == flight::String("missingB"),
             "explanation changed first-seen missing-key order")) {
    return 1;
  }

  // A malformed discriminator/count must fail before registry callbacks, guards,
  // flushes, or the caller's rectangle are touched.
  const auto malformed = make_shape();
  malformed->data->commands.push(string("box"));
  malformed->data->commands.push(string("not-a-count"));
  const auto sentinel = rectangle(1.0, 2.0, 3.0, 4.0);
  calls.clear();
  const int guards_before_malformed = guard_calls;
  bool threw = false;
  try {
    static_cast<void>(compute_shape_bounds_rectangle(sentinel, malformed));
  } catch (const std::bad_variant_access&) {
    threw = true;
  }
  set_shape_bounds_guard(std::nullopt);
  clear_shape_bounds_commands();
  if (!check(threw, "a malformed argument count did not fail loudly") ||
      !check(calls.empty() && guard_calls == guards_before_malformed,
             "malformed input invoked a callback before throwing") ||
      !check(is_rectangle(sentinel, 1.0, 2.0, 3.0, 4.0),
             "malformed input wrote the output rectangle before throwing")) {
    return 1;
  }

  std::cout << "generated shape bounds: all checks passed\n";
  return 0;
}
