#include <flight/shape/register_default_shape_bounds_commands.hpp>
#include <flight/shape/shape_bounds.hpp>
#include <flight/shape/shape_bounds_registry.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
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

struct BoundsCall {
  std::string name;
  std::vector<double> numbers;
  std::vector<flight::String> styles;
};

bool same_number(double left, double right) {
  return std::isnan(left) ? std::isnan(right)
                         : left == right && (left != 0.0 || std::signbit(left) == std::signbit(right));
}

bool same_calls(const std::vector<BoundsCall>& actual, const std::vector<BoundsCall>& expected) {
  if (actual.size() != expected.size()) return false;
  for (std::size_t i = 0; i < actual.size(); ++i) {
    if (actual[i].name != expected[i].name || actual[i].styles != expected[i].styles ||
        actual[i].numbers.size() != expected[i].numbers.size()) return false;
    for (std::size_t j = 0; j < actual[i].numbers.size(); ++j)
      if (!same_number(actual[i].numbers[j], expected[i].numbers[j])) return false;
  }
  return true;
}

flight::Ref<flight::types::ShapeBoundsContext> recording_context(std::vector<BoundsCall>& calls) {
  auto context = flight::make_ref<flight::types::ShapeBoundsContext>();
  context->close_path = [&] { calls.push_back({"closePath", {}, {}}); };
  context->cubic_curve_to = [&](double a, double b, double c, double d, double e, double f) {
    calls.push_back({"cubicCurveTo", {a,b,c,d,e,f}, {}});
  };
  context->draw_circle = [&](double a, double b, double c) { calls.push_back({"drawCircle", {a,b,c}, {}}); };
  context->draw_ellipse = [&](double a, double b, double c, double d) { calls.push_back({"drawEllipse", {a,b,c,d}, {}}); };
  context->draw_rectangle = [&](double a, double b, double c, double d) { calls.push_back({"drawRectangle", {a,b,c,d}, {}}); };
  context->draw_rounded_rectangle = [&](double a, double b, double c, double d, double e) {
    calls.push_back({"drawRoundedRectangle", {a,b,c,d,e}, {}});
  };
  context->expand_point = [&](double a, double b) { calls.push_back({"expandPoint", {a,b}, {}}); };
  context->flush_path = [&] { calls.push_back({"flushPath", {}, {}}); };
  context->line_to = [&](double a, double b) { calls.push_back({"lineTo", {a,b}, {}}); };
  context->move_to = [&](double a, double b) { calls.push_back({"moveTo", {a,b}, {}}); };
  context->quadratic_curve_to = [&](double a, double b, double c, double d) {
    calls.push_back({"quadraticCurveTo", {a,b,c,d}, {}});
  };
  context->set_stroke_style = [&](double width, flight::types::CapsStyle caps,
                                  flight::types::JointStyle joints, double limit) {
    calls.push_back({"setStrokeStyle", {width,limit}, {caps,joints}});
  };
  return context;
}

bool exercise_kernel(const flight::types::ShapeBoundsCommandHandler& handler,
                     const std::vector<Argument>& arguments,
                     const std::vector<BoundsCall>& expected,
                     const std::vector<double>& expected_reads) {
  std::vector<BoundsCall> calls;
  std::vector<double> reads;
  const auto context = recording_context(calls);
  auto cursor = flight::make_ref<Cursor>();
  cursor->length = static_cast<double>(arguments.size());
  cursor->get_argument = [&](double index) -> Argument {
    reads.push_back(index);
    return arguments.at(static_cast<std::size_t>(index));
  };
  handler(context, CursorView(cursor));
  return check(same_calls(calls, expected), "default callback changed its original context call or values") &&
         check(reads == expected_reads, "default callback changed cursor reads or source evaluation order");
}

bool exercise_default_kernels() {
  namespace shape = flight::shape;
  if (!exercise_kernel(shape::default_shape_bounds_cubic_curve_to, {1.,2.,3.,4.,5.,6.},
                       {{"cubicCurveTo",{1,2,3,4,5,6},{}}}, {0,1,2,3,4,5}) ||
      !exercise_kernel(shape::default_shape_bounds_draw_circle,
                       {-0.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()},
                       {{"drawCircle",{-0.,std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity()}, {}}}, {0,1,2}) ||
      !exercise_kernel(shape::default_shape_bounds_draw_ellipse, {1.,2.,3.,4.}, {{"drawEllipse",{1,2,3,4},{}}}, {0,1,2,3}) ||
      !exercise_kernel(shape::default_shape_bounds_draw_rectangle, {1.,2.,3.,4.}, {{"drawRectangle",{1,2,3,4},{}}}, {0,1,2,3}) ||
      !exercise_kernel(shape::default_shape_bounds_draw_rounded_rectangle, {1.,2.,3.,4.,5.}, {{"drawRoundedRectangle",{1,2,3,4,5},{}}}, {0,1,2,3,4}) ||
      !exercise_kernel(shape::default_shape_bounds_line_style,
                       {2.,77.,0.5,true,flight::String("unused"),flight::String("none"),flight::String("bevel"),7.},
                       {{"setStrokeStyle",{2,7},{flight::String("none"),flight::String("bevel")}}}, {0,5,6,7}) ||
      !exercise_kernel(shape::default_shape_bounds_line_to, {1.,2.}, {{"lineTo",{1,2},{}}}, {0,1}) ||
      !exercise_kernel(shape::default_shape_bounds_move_to, {1.,2.}, {{"moveTo",{1,2},{}}}, {0,1}) ||
      !exercise_kernel(shape::default_shape_bounds_quadratic_curve_to, {1.,2.,3.,4.}, {{"quadraticCurveTo",{1,2,3,4},{}}}, {0,1,2,3}) ||
      !exercise_kernel(shape::default_shape_bounds_expand_point_pairs, {flight::Array<double>{1,2,3,4,5}},
                       {{"expandPoint",{1,2},{}},{"expandPoint",{3,4},{}}}, {0}) ||
      !exercise_kernel(shape::default_shape_bounds_draw_path,
                       {flight::Array<double>{0,1,2,3,4,5,6,7,99},
                        flight::Array<double>{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21}},
                       {{"moveTo",{0,1},{}},{"lineTo",{2,3},{}},{"quadraticCurveTo",{4,5,6,7},{}},
                        {"moveTo",{10,11},{}},{"lineTo",{14,15},{}},{"cubicCurveTo",{16,17,18,19,20,21},{}},{"closePath",{},{}}}, {0,1}) ||
      !exercise_kernel(shape::default_shape_bounds_flush, {}, {{"flushPath",{},{}}}, {})) return false;

  flight::Array<double> live{1,2,3,4,5};
  std::vector<std::pair<double,double>> points;
  auto context = flight::make_ref<flight::types::ShapeBoundsContext>();
  context->expand_point = [&](double x, double y) {
    points.emplace_back(x,y);
    if (points.size() == 1) live.element(2.) = 33.;
  };
  auto cursor = flight::make_ref<Cursor>();
  cursor->length = 1.;
  cursor->get_argument = [&](double) -> Argument { return live; };
  shape::default_shape_bounds_expand_point_pairs(context, CursorView(cursor));
  if (!check(points == std::vector<std::pair<double,double>>{{1,2},{33,4}},
             "point-pair callback copied its live source array")) return false;

  const auto marker = std::make_shared<int>(7);
  std::vector<double> reads;
  bool reached = false;
  context->cubic_curve_to = [&](double,double,double,double,double,double) { reached = true; };
  cursor->length = 6.;
  cursor->get_argument = [&](double index) -> Argument {
    reads.push_back(index);
    if (index == 1.) throw marker;
    return index;
  };
  bool exact_throw = false;
  try { shape::default_shape_bounds_cubic_curve_to(context, CursorView(cursor)); }
  catch (const std::shared_ptr<int>& error) { exact_throw = error == marker; }
  return check(exact_throw && reads == std::vector<double>{0,1} && !reached,
               "cursor exception changed its owner, read order or context side effects");
}

bool exercise_default_registry() {
  namespace shape = flight::shape;
  shape::clear_shape_bounds_commands();
  const double before = shape::get_shape_bounds_command_registry_revision();
  if (!check(!shape::get_shape_bounds_command("vendor:absent"), "missing key unexpectedly present")) return false;
  shape::register_default_shape_bounds_commands();
  for (const char* key : {"beginTextureFill","beginFill","beginGradientFill","cubicCurveTo","quadraticCurveTo",
                          "drawCircle","drawEllipse","drawPath","drawRectangle","drawRoundedRectangle","endFill",
                          "lineTextureStyle","lineGradientStyle","lineStyle","lineTo","moveTo"}) {
    const auto command = shape::get_shape_bounds_command(key);
    if (!check(command && flight::row_get<flight::RowKey<"key">>(*command) == flight::String(key),
               "generic default registration changed a key or omitted a command")) return false;
  }
  if (!check(shape::get_shape_bounds_command_registry_revision() == before + 16.,
             "first default registration changed revision increments")) return false;
  const auto retained = shape::get_shape_bounds_command("beginFill");
  const auto no_geometry = shape::get_shape_bounds_command("lineTextureStyle");
  if (!check(no_geometry && !flight::row_get<flight::RowKey<"fillBounds">>(*no_geometry) &&
             !flight::row_get<flight::RowKey<"strokeBounds">>(*no_geometry), "null handlers lost absence")) return false;
  std::vector<BoundsCall> calls;
  const auto context = recording_context(calls);
  const auto cursor = shape::create_shape_command_argument_cursor({});
  const flight::Ref<Cursor> cursor_contract = cursor;
  const CursorView cursor_view(cursor_contract);
  const auto handler = flight::row_get<flight::RowKey<"fillBounds">>(*retained);
  if (!check(handler.has_value(), "default flush handler absent")) return false;
  (*handler)(context, cursor_view);
  const double installed_revision = shape::get_shape_bounds_command_registry_revision();
  shape::register_default_shape_bounds_commands();
  if (!check(shape::get_shape_bounds_command_registry_revision() == installed_revision,
             "same original callback identities advanced revision") ||
      !check(flight::callable_identity_equal(*handler, *flight::row_get<flight::RowKey<"fillBounds">>(*shape::get_shape_bounds_command("beginFill"))),
             "repeat registration lost original callback identity")) return false;
  shape::unregister_shape_bounds_command("beginFill");
  if (!check(!shape::get_shape_bounds_command("beginFill"), "unregister retained lookup")) return false;
  (*handler)(context, cursor_view);
  shape::clear_shape_bounds_commands();
  (*handler)(context, cursor_view);
  return check(same_calls(calls, {{"flushPath",{},{}},{"flushPath",{},{}},{"flushPath",{},{}}}),
               "retained readonly handler lost lifetime after unregister/clear");
}

}  // namespace

int main() {
  if (!exercise_default_registry() || !exercise_default_kernels()) return 1;
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
  const flight::Ref<Cursor> cursor_contract = cursor;
  const CursorView cursor_view(cursor_contract);
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
    context->set_stroke_style(2.0, flight::String("none"),
                              flight::String("round"), 10.0);
    context->draw_rectangle(argument_number(arguments, 0.0),
                            argument_number(arguments, 1.0),
                            argument_number(arguments, 2.0),
                            argument_number(arguments, 3.0));
  };
  install_command(box);

  const auto shape = make_shape();
  append_command(shape, "box", {number(10.0), number(20.0), number(30.0), number(40.0)});

  const auto geometry = rectangle();
  const auto geometry_mode = flight::String("geometry");
  if (!check(compute_shape_bounds_rectangle(geometry, flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Shape>>>>(shape), geometry_mode),
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
  if (!check(compute_shape_bounds_rectangle(ink, flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Shape>>>>(shape)),
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
  if (!check(!compute_shape_bounds_rectangle(unknown_out, flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Shape>>>>(unknown)),
             "missing commands were reported complete") ||
      !check(guard_calls == 3 && guard_values_ok,
             "the guard did not observe every missing command in source order") ||
      !check(guard_owner_ok,
             "the guard received a replacement row instead of the exact Shape owner") ||
      !check(is_rectangle(unknown_out, 0.0, 0.0, 0.0, 0.0),
             "an empty incomplete traversal did not write the source empty bounds")) {
    return 1;
  }

  const auto explanation = explain_shape_bounds(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Shape>>>>(unknown));
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
    static_cast<void>(compute_shape_bounds_rectangle(sentinel, flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Shape>>>>(malformed)));
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
