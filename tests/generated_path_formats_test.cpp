// Focused native parity for @flighthq/path-formats at Flight a627849.
//
// The package's applicable surface is the SVG path-data codec. It has no detection, transform, or
// unit API; this suite owns exact grammar/serialization shape, optional precision, command ordering,
// winding preservation, diagnostics, malformed/truncated input, round trips, and append atomicity.
// Mutation audit (2026-10-13): append-target corruption triggered 2 assertions; relative-H,
// smooth-cubic reflection, and precision-factor corruption each triggered 1 assertion.
#include <flight/path_formats/contract.hpp>

#include <cmath>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace formats = flight::path_formats;
namespace path = flight::path;
namespace types = flight::types;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool close(double actual, double expected, double epsilon = 1e-6) {
  return std::abs(actual - expected) <= epsilon;
}

bool array_equals(const flight::Array<double>& actual,
                  std::initializer_list<double> expected,
                  double epsilon = 1e-6) {
  if (actual.size() != expected.size()) return false;
  std::size_t index = 0;
  for (double value : expected) {
    if (!close(actual[index++], value, epsilon)) return false;
  }
  return true;
}

bool arrays_equal(const flight::Array<double>& left,
                  const flight::Array<double>& right,
                  double epsilon = 1e-6) {
  if (left.size() != right.size()) return false;
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (!close(left[index], right[index], epsilon)) return false;
  }
  return true;
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return flight::structural_ref_cast<View>(value);
}

std::optional<flight::Ref<types::Path>> parse(const char* data) {
  return formats::parse_svg_path_data(flight::String(data));
}

void append_order_winding_and_atomicity() {
  const auto destination = path::create_path(flight::String("evenOdd"));
  path::append_path_move_to(destination, -1, -2);
  path::append_path_line_to(destination, -3, -4);
  const void* command_identity = destination->commands.identity();
  const void* data_identity = destination->data.identity();

  check(formats::append_svg_path_data(destination,
                                      flight::String("M0 0 L10 0")) &&
            array_equals(destination->commands,
                         {types::path_command->move_to,
                          types::path_command->line_to,
                          types::path_command->move_to,
                          types::path_command->line_to}) &&
            array_equals(destination->data,
                         {-1, -2, -3, -4, 0, 0, 10, 0}) &&
            destination->winding == flight::String("evenOdd") &&
            destination->commands.identity() == command_identity &&
            destination->data.identity() == data_identity,
        "append preserves prior segments, order, winding, and destination arrays");

  const auto commands_before = destination->commands.clone();
  const auto data_before = destination->data.clone();
  check(!formats::append_svg_path_data(destination,
                                       flight::String("M10 20 L30")) &&
            arrays_equal(destination->commands, commands_before) &&
            arrays_equal(destination->data, data_before) &&
            destination->commands.identity() == command_identity &&
            destination->data.identity() == data_identity &&
            destination->winding == flight::String("evenOdd"),
        "a truncated append fails atomically without replacing storage or winding");
  check(formats::append_svg_path_data(destination, flight::String(" \t\n")) &&
            arrays_equal(destination->commands, commands_before) &&
            arrays_equal(destination->data, data_before),
        "empty path data succeeds as an exact no-op");
}

void absolute_relative_and_shorthand_grammar() {
  const auto authored = parse(
      "M0 0 L10 0 Q15 5 20 0 C22 2 24 2 26 0 Z");
  check(authored.has_value() &&
            array_equals(authored.value()->commands,
                         {types::path_command->move_to,
                          types::path_command->line_to,
                          types::path_command->quadratic_curve_to,
                          types::path_command->cubic_curve_to,
                          types::path_command->close}) &&
            array_equals(authored.value()->data,
                         {0, 0, 10, 0, 15, 5, 20, 0,
                          22, 2, 24, 2, 26, 0}) &&
            authored.value()->winding == flight::String("nonZero"),
        "absolute M/L/Q/C/Z parsing preserves exact command and data shape");

  const auto repeated = parse("M0 0 1 1 2 2");
  check(repeated.has_value() &&
            array_equals(repeated.value()->commands,
                         {types::path_command->move_to,
                          types::path_command->line_to,
                          types::path_command->line_to}) &&
            array_equals(repeated.value()->data, {0, 0, 1, 1, 2, 2}),
        "repeated moveto operands become ordered implicit lineto commands");

  const auto relative = parse("m10 10 h5 v10 l-5 0 H20 V5");
  check(relative.has_value() &&
            array_equals(relative.value()->commands,
                         {types::path_command->move_to,
                          types::path_command->line_to,
                          types::path_command->line_to,
                          types::path_command->line_to,
                          types::path_command->line_to,
                          types::path_command->line_to}) &&
            array_equals(relative.value()->data,
                         {10, 10, 15, 10, 15, 20, 10, 20, 20, 20, 20, 5}),
        "relative and horizontal/vertical commands resolve against the current point");

  const auto smooth_cubic = parse("M0 0 C1 2 3 4 5 6 S3 3 4 4");
  check(smooth_cubic.has_value() &&
            array_equals(smooth_cubic.value()->data,
                         {0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 3, 3, 4, 4}),
        "S reflects the preceding cubic control point in absolute space");
  const auto smooth_quadratic = parse("M0 0 Q1 1 2 2 T4 4");
  check(smooth_quadratic.has_value() &&
            array_equals(smooth_quadratic.value()->commands,
                         {types::path_command->move_to,
                          types::path_command->quadratic_curve_to,
                          types::path_command->quadratic_curve_to}) &&
            array_equals(smooth_quadratic.value()->data,
                         {0, 0, 1, 1, 2, 2, 3, 3, 4, 4}),
        "T reflects the preceding quadratic control and retains command value 3");

  const auto closed_arc = parse("M0 0 L10 0 Z A5 5 0 0 1 10 0");
  check(closed_arc.has_value() &&
            closed_arc.value()->commands[2] == types::path_command->close &&
            closed_arc.value()->commands[3] == types::path_command->move_to &&
            closed_arc.value()->commands[4] ==
                types::path_command->cubic_curve_to &&
            close(closed_arc.value()->data[closed_arc.value()->data.size() - 2],
                  10) &&
            close(closed_arc.value()->data[closed_arc.value()->data.size() - 1],
                  0),
        "an arc after close restarts at the subpath origin and ends exactly at its anchor");
}

void tokenization_diagnostics_and_malformed_inputs() {
  const auto packed = parse("M.5.5L10-5L+2,+3L10. 20.");
  check(packed.has_value() &&
            array_equals(packed.value()->data,
                         {0.5, 0.5, 10, -5, 2, 3, 10, 20}),
        "tokenization accepts packed decimals, signs, commas, and trailing dots");
  const auto scientific = parse("M1e1,1e1 L2E1,-1.5e1");
  check(scientific.has_value() &&
            array_equals(scientific.value()->data, {10, 10, 20, -15}),
        "tokenization accepts signed scientific notation");
  const auto finite = parse("M1e308 -1e308");
  check(finite.has_value() && finite.value()->data[0] == 1e308 &&
            finite.value()->data[1] == -1e308,
        "the parser retains large finite coordinates at both signs");

  const auto packed_flags = parse("M0 0A5 5 0 0110 0");
  check(packed_flags.has_value() &&
            packed_flags.value()->commands.size() >= 2 &&
            close(packed_flags.value()->data[packed_flags.value()->data.size() - 2],
                  10) &&
            close(packed_flags.value()->data[packed_flags.value()->data.size() - 1],
                  0),
        "arc flags may be packed without separators");

  const auto valid = formats::explain_svg_path_data(flight::String("M0 0 L1 1"));
  const auto missing_move =
      formats::explain_svg_path_data(flight::String("L10 10"));
  const auto unknown =
      formats::explain_svg_path_data(flight::String("M0 0 X10 10"));
  const auto truncated =
      formats::explain_svg_path_data(flight::String("M10 20 L30"));
  const auto bad_flag = formats::explain_svg_path_data(
      flight::String("M0 0 A5 5 0 2 1 10 0"));
  check(!valid.has_value() && missing_move.has_value() &&
            missing_move.value()->position == 0 &&
            missing_move.value()->reason == flight::String("expected-moveto") &&
            unknown.has_value() && unknown.value()->position == 5 &&
            unknown.value()->reason == flight::String("expected-command") &&
            truncated.has_value() &&
            truncated.value()->reason ==
                flight::String("expected-coordinate-pair") &&
            bad_flag.has_value() &&
            bad_flag.value()->reason ==
                flight::String("expected-arc-parameters"),
        "diagnostics distinguish command, coordinate, and arc-parameter failures");

  check(!parse("M1e 0").has_value() && !parse("M1e999 0").has_value() &&
            !parse("M-1e999 0").has_value() && !parse("M").has_value() &&
            !parse("M0 0 C1 2 3 4 5").has_value() &&
            !parse("M0 0 X10 10").has_value(),
        "dangling exponents, overflow, truncation, and unknown commands are rejected");
  const auto empty = parse("");
  const auto whitespace = parse(" \t\n ");
  check(empty.has_value() && whitespace.has_value() &&
            empty.value()->commands.empty() && whitespace.value()->commands.empty(),
        "empty and whitespace-only inputs return valid empty paths");
}

void exact_formatting_and_round_trips() {
  const auto authored = parse(
      "M0 0 L10 0 Q15 5 20 0 C22 2 24 2 26 0 Z");
  check(authored.has_value() &&
            formats::format_svg_path_data(readonly(authored.value())) ==
                flight::String("M0 0L10 0Q15 5 20 0C22 2 24 2 26 0Z"),
        "formatting emits compact absolute M/L/Q/C/Z text exactly");

  const auto decimals = parse("M0.123456 0.5 L10.987654 20");
  const auto precision = flight::make_ref<types::SvgPathDataFormatOptions>(
      types::SvgPathDataFormatOptions{.precision = 2});
  const auto default_options =
      flight::make_ref<types::SvgPathDataFormatOptions>(
          types::SvgPathDataFormatOptions{.precision = std::nullopt});
  check(decimals.has_value() &&
            formats::format_svg_path_data(
                readonly(decimals.value()),
                std::optional{readonly(precision)}) ==
                flight::String("M0.12 0.5L10.99 20") &&
            formats::format_svg_path_data(
                readonly(decimals.value()),
                std::optional{readonly(default_options)}) ==
                flight::String("M0.123456 0.5L10.987654 20"),
        "optional precision rounds and trims while absent precision remains exact");

  const auto arc = parse("M0 0 A5 5 0 0 1 10 0");
  check(arc.has_value(), "the arc formatting fixture parses");
  if (arc.has_value()) {
    const flight::String arc_text =
        formats::format_svg_path_data(readonly(arc.value()));
    check(arc_text.index_of(flight::String("A")) == -1 &&
              arc_text.index_of(flight::String("M0 0C")) == 0,
          "arcs serialize in their stored cubic shape rather than inventing an A verb");
  }

  const std::vector<const char*> cases{
      "M0 0 L10 0 L10 10 Z",
      "M0 0 C1 2 3 4 5 6 S7 8 9 10",
      "M0 0 Q1 1 2 2 T4 4 T6 0",
      "m10 10 h20 v20 h-20 z",
      "M0 0 A5 5 0 0 1 10 0 A5 5 0 0 1 20 0",
  };
  for (const char* source : cases) {
    const auto first = parse(source);
    if (!first.has_value()) {
      check(false, std::string("round-trip fixture parses: ") + source);
      continue;
    }
    const auto serialized = formats::format_svg_path_data(readonly(first.value()));
    const auto second = formats::parse_svg_path_data(serialized);
    check(second.has_value() &&
              arrays_equal(first.value()->commands, second.value()->commands) &&
              arrays_equal(first.value()->data, second.value()->data) &&
              first.value()->winding == second.value()->winding,
          std::string("round trip preserves command/data shape for: ") + source);
  }
}

}  // namespace

int main() {
  append_order_winding_and_atomicity();
  absolute_relative_and_shorthand_grammar();
  tokenization_diagnostics_and_malformed_inputs();
  exact_formatting_and_round_trips();

  if (failures == 0) {
    std::cout << "generated path-formats tests passed\n";
    return 0;
  }
  std::cerr << failures << " generated path-formats test(s) failed\n";
  return 1;
}
