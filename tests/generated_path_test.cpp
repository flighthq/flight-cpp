// Focused native parity for @flighthq/path at Flight a627849.
//
// This suite exercises the public path contract directly: command/data ownership and ordering,
// construction and invalid-input atomicity, copy/reverse/edit aliasing, contour flattening,
// geometric queries, winding, transforms, morph preparation/sampling, and fill/stroke meshes.
//
// MUTATION SENSITIVITY, measured 2026-10-13: four one-at-a-time generated implementation
// mutations were compiled and run, then restored. The suite killed a changed default winding
// (1 failure), shallow clone storage (2), omitted X translation (2), and an invalid segment query
// that partially wrote its output (1).
#include <flight/path/contract.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace {

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

flight::Ref<types::Vector2> vector2(double x = 0.0, double y = 0.0) {
  return flight::make_ref<types::Vector2>(types::Vector2{.x = x, .y = y});
}

flight::Ref<types::Rectangle> rectangle(double x = 0.0, double y = 0.0,
                                        double width = 0.0,
                                        double height = 0.0) {
  return flight::make_ref<types::Rectangle>(
      types::Rectangle{.height = height, .width = width, .x = x, .y = y});
}

flight::Ref<types::Matrix> matrix(double a, double b, double c, double d,
                                  double tx, double ty) {
  return flight::make_ref<types::Matrix>(
      types::Matrix{.a = a, .b = b, .c = c, .d = d, .tx = tx, .ty = ty});
}

std::size_t command_count(const flight::Ref<types::Path>& value,
                          double command) {
  return static_cast<std::size_t>(std::count(value->commands.begin(),
                                             value->commands.end(), command));
}

void construction_segments_and_failure_atomicity() {
  const auto empty = path::create_path();
  const auto other = path::create_path(flight::String("evenOdd"));
  check(empty->winding == flight::String("nonZero") && empty->commands.empty() &&
            empty->data.empty(),
        "default construction owns an empty non-zero path");
  check(other->winding == flight::String("evenOdd") &&
            empty->commands.identity() != other->commands.identity() &&
            empty->data.identity() != other->data.identity(),
        "construction honors winding and gives each path independent arrays");

  const auto authored = path::create_path();
  path::append_path_move_to(authored, 1.0, 2.0);
  path::append_path_line_to(authored, 3.0, 4.0);
  path::append_path_quadratic_curve_to(authored, 5.0, 6.0, 7.0, 8.0);
  path::append_path_cubic_curve_to(authored, 9.0, 10.0, 11.0, 12.0, 13.0,
                                   14.0);
  path::append_path_close(authored);
  check(array_equals(authored->commands,
                     {types::path_command->move_to,
                      types::path_command->line_to,
                      types::path_command->quadratic_curve_to,
                      types::path_command->cubic_curve_to,
                      types::path_command->close}) &&
            array_equals(authored->data,
                         {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14}),
        "append helpers retain exact command and coordinate ordering");
  const auto last = path::get_path_last_point(readonly(authored));
  check(last.has_value() && array_equals(*last, {1, 2}),
        "close restores the active subpath origin as the last point");

  std::vector<flight::String> segment_kinds;
  path::for_each_path_segment(readonly(authored), [&](types::PathSegment segment) {
    std::visit([&](const auto& item) { segment_kinds.push_back(item->kind); },
               segment);
  });
  check(segment_kinds ==
            std::vector<flight::String>{flight::String("moveTo"),
                                        flight::String("lineTo"),
                                        flight::String("quadraticCurveTo"),
                                        flight::String("cubicCurveTo"),
                                        flight::String("close")},
        "segment iteration preserves authored order and normalized ownership");

  const auto rectangle_path = path::create_path();
  path::append_path_rectangle(rectangle_path, 10, 20, 100, 50);
  check(array_equals(rectangle_path->commands,
                     {types::path_command->move_to,
                      types::path_command->line_to,
                      types::path_command->line_to,
                      types::path_command->line_to,
                      types::path_command->close}) &&
            array_equals(rectangle_path->data,
                         {10, 20, 110, 20, 110, 70, 10, 70}),
        "rectangle construction emits four ordered corners and a close");

  const auto circle = path::create_path();
  path::append_path_circle(circle, 10, 20, 30);
  check(circle->commands[0] == types::path_command->move_to &&
            circle->commands[circle->commands.size() - 1] ==
                types::path_command->close &&
            command_count(circle, types::path_command->cubic_curve_to) == 4 &&
            close(circle->data[0], 40) && close(circle->data[1], 20),
        "circle construction owns four cubics and closes at the rightmost start");

  const auto guarded = path::create_path();
  path::append_path_move_to(guarded, 7, 8);
  const auto commands_before = guarded->commands.clone();
  const auto data_before = guarded->data.clone();
  const void* commands_identity = guarded->commands.identity();
  const void* data_identity = guarded->data.identity();
  path::append_path_polygon(guarded, flight::Array<double>{0, 0, 10, 10});
  path::append_path_polyline(guarded, flight::Array<double>{0, 0});
  path::append_path_arc(guarded, 0, 0, 0, 0, 3.141592653589793);
  path::append_path_elliptical_arc_to(guarded, 10, 10, 0, false, true, 7, 8);
  check(guarded->commands.identity() == commands_identity &&
            guarded->data.identity() == data_identity &&
            arrays_equal(guarded->commands, commands_before) &&
            arrays_equal(guarded->data, data_before),
        "invalid or degenerate constructors fail atomically without replacing arrays");
}

void copy_reverse_and_edit_aliasing() {
  const auto source = path::create_path(flight::String("evenOdd"));
  path::append_path_rectangle(source, 0, 0, 10, 5);
  const auto clone = path::clone_path(readonly(source));
  check(clone != source && clone->commands.identity() != source->commands.identity() &&
            clone->data.identity() != source->data.identity() &&
            arrays_equal(clone->commands, source->commands) &&
            arrays_equal(clone->data, source->data) &&
            clone->winding == source->winding,
        "clone deep-copies command/data ownership and preserves winding");
  clone->commands.push(99);
  clone->data[0] = 99;
  check(source->commands.size() == 5 && source->data[0] == 0,
        "mutating a clone cannot mutate the source arrays");

  const auto copied = path::create_path();
  path::append_path_move_to(copied, -1, -1);
  const auto copy_result = path::copy_path(readonly(source), copied);
  check(copy_result == copied && copied->winding == flight::String("evenOdd") &&
            arrays_equal(copied->commands, source->commands) &&
            arrays_equal(copied->data, source->data),
        "copy writes into caller-owned output and replaces all public fields");
  const void* copied_commands = copied->commands.identity();
  const void* copied_data = copied->data.identity();
  path::copy_path(readonly(copied), copied);
  check(copied->commands.identity() == copied_commands &&
            copied->data.identity() == copied_data,
        "self-copy is an identity-preserving no-op");

  const auto open = path::create_path(flight::String("evenOdd"));
  path::append_path_move_to(open, 0, 0);
  path::append_path_line_to(open, 10, 0);
  path::append_path_line_to(open, 10, 10);
  const auto reversed = path::create_path();
  path::reverse_path(readonly(open), reversed);
  check(array_equals(reversed->commands,
                     {types::path_command->move_to,
                      types::path_command->line_to,
                      types::path_command->line_to}) &&
            array_equals(reversed->data, {10, 10, 10, 0, 0, 0}) &&
            reversed->winding == flight::String("evenOdd"),
        "reversal flips open segment order and retains winding");
  path::reverse_path(readonly(open), open);
  check(arrays_equal(open->commands, reversed->commands) &&
            arrays_equal(open->data, reversed->data),
        "reversal is alias-safe when source and output are identical");

  const auto dirty = path::create_path();
  path::append_path_move_to(dirty, 0, 0);
  path::append_path_line_to(dirty, 5, 0);
  path::append_path_line_to(dirty, 5, 0);
  path::append_path_line_to(dirty, 10, 0);
  path::append_path_line_to(dirty, 10, 10);
  path::append_path_line_to(dirty, 0, 10);
  path::append_path_close(dirty);
  path::clean_path(readonly(dirty), 0.001, dirty);
  check(array_equals(dirty->commands,
                     {types::path_command->move_to,
                      types::path_command->line_to,
                      types::path_command->line_to,
                      types::path_command->line_to,
                      types::path_command->close}) &&
            array_equals(dirty->data, {0, 0, 10, 0, 10, 10, 0, 10}),
        "cleaning removes duplicate/collinear points in place without losing closure");

  const auto dashed_source = path::create_path();
  path::append_path_move_to(dashed_source, 0, 0);
  path::append_path_line_to(dashed_source, 100, 0);
  const auto dashed_commands = dashed_source->commands.clone();
  const auto dashed_data = dashed_source->data.clone();
  const auto dashed = path::create_path();
  path::dash_path(readonly(dashed_source), flight::Array<double>{10, 5}, 0,
                  dashed);
  check(command_count(dashed, types::path_command->move_to) >= 6 &&
            command_count(dashed, types::path_command->close) == 0 &&
            arrays_equal(dashed_source->commands, dashed_commands) &&
            arrays_equal(dashed_source->data, dashed_data),
        "dashing produces ordered open subpaths without mutating its source");
}

void contours_bounds_and_queries() {
  const auto multi = path::create_path();
  path::append_path_move_to(multi, 0, 0);
  path::append_path_line_to(multi, 30, 0);
  path::append_path_move_to(multi, 0, 10);
  path::append_path_line_to(multi, 0, 50);
  const auto contours = path::flatten_path(readonly(multi));
  check(contours.size() == 2 && array_equals(contours[0], {0, 0, 30, 0}) &&
            array_equals(contours[1], {0, 10, 0, 50}),
        "flattening preserves subpath boundaries and point order");
  const auto contour_lengths = path::get_path_contour_lengths(readonly(multi));
  check(array_equals(contour_lengths, {30, 40}) &&
            close(path::get_path_length(readonly(multi)), 70),
        "per-contour lengths sum to the total across subpaths");

  const auto curve = path::create_path();
  path::append_path_move_to(curve, 0, 0);
  path::append_path_quadratic_curve_to(curve, 50, 50, 100, 0);
  const auto fine = path::flatten_path(readonly(curve), 0.1);
  const auto coarse = path::flatten_path(readonly(curve), 50.0);
  check(fine.size() == 1 && coarse.size() == 1 &&
            fine[0].size() > coarse[0].size() && fine[0][0] == 0 &&
            close(fine[0][fine[0].size() - 2], 100) &&
            close(fine[0][fine[0].size() - 1], 0),
        "curve subdivision keeps endpoints and responds to flattening tolerance");

  const auto closed = path::create_path();
  path::append_path_move_to(closed, 0, 0);
  path::append_path_line_to(closed, 100, 0);
  path::append_path_line_to(closed, 100, 100);
  path::append_path_close(closed);
  const auto closed_contour = path::flatten_path(readonly(closed))[0];
  check(array_equals(closed_contour, {0, 0, 100, 0, 100, 100, 0, 0}),
        "flattened closed contours append their origin exactly once");

  const auto empty_bounds = rectangle(1, 2, 3, 4);
  check(!path::get_path_bounds(readonly(path::create_path()), empty_bounds) &&
            empty_bounds->x == 0 && empty_bounds->y == 0 &&
            empty_bounds->width == 0 && empty_bounds->height == 0,
        "empty geometry reports no bounds and deterministically zeroes the output");
  const auto curve_bounds = rectangle();
  path::get_path_bounds(readonly(curve), curve_bounds);
  check(close(curve_bounds->x, 0) && close(curve_bounds->y, 0) &&
            close(curve_bounds->width, 100) && close(curve_bounds->height, 25),
        "quadratic bounds use the curve extremum instead of its control point");

  const auto point = vector2(99, 99);
  const auto tangent = vector2(99, 99);
  check(path::get_path_position_at_distance(readonly(multi), 15, point, tangent) &&
            close(point->x, 15) && close(point->y, 0) &&
            close(tangent->x, 1) && close(tangent->y, 0),
        "distance sampling returns a position and normalized tangent together");
  check(path::get_path_point_at_distance(readonly(multi), -5, point) &&
            close(point->x, 0) && close(point->y, 0) &&
            path::get_path_point_at_distance(readonly(multi), 500, point) &&
            close(point->x, 0) && close(point->y, 50),
        "distance sampling clamps before the start and after the final contour");

  const auto nearest = vector2(9, 9);
  const auto horizontal = path::create_path();
  path::append_path_move_to(horizontal, 0, 0);
  path::append_path_line_to(horizontal, 100, 0);
  check(close(path::get_path_nearest_point(readonly(horizontal), 50, 30, nearest),
              30) &&
            close(nearest->x, 50) && close(nearest->y, 0),
        "nearest-point queries project onto the closest segment");
  nearest->x = 7;
  nearest->y = 8;
  check(path::get_path_nearest_point(readonly(path::create_path()), 5, 5,
                                     nearest) == -1 &&
            nearest->x == 7 && nearest->y == 8,
        "empty nearest-point queries fail without partially mutating output");

  const auto segment_out = vector2(7, 8);
  check(!path::get_path_segment_point_at_parameter(readonly(horizontal), 9, 0.5,
                                                   segment_out) &&
            segment_out->x == 7 && segment_out->y == 8 &&
            !path::get_path_segment_curvature_at_parameter(readonly(horizontal), 9,
                                                           0.5)
                 .has_value(),
        "invalid segment indices fail atomically for point and curvature queries");
  check(path::get_path_segment_point_at_parameter(readonly(horizontal), 0, 0.5,
                                                  segment_out) &&
            close(segment_out->x, 50) && close(segment_out->y, 0) &&
            path::get_path_segment_tangent_at_parameter(readonly(horizontal), 0,
                                                        0.5, segment_out) &&
            close(segment_out->x, 100) && close(segment_out->y, 0),
        "valid segment parameters evaluate points and authored derivatives");
  check(close(path::get_path_curvature_at_distance(readonly(horizontal), 50), 0),
        "straight segments have zero distance-sampled curvature");
}

void winding_transforms_morphs_and_meshes() {
  const auto even_odd = path::create_path(flight::String("evenOdd"));
  const auto non_zero = path::create_path(flight::String("nonZero"));
  for (const auto& value : {even_odd, non_zero}) {
    path::append_path_rectangle(value, 0, 0, 60, 60);
    path::append_path_rectangle(value, 40, 0, 60, 60);
  }
  check(!path::contains_path_point(readonly(even_odd), 50, 30) &&
            path::contains_path_point(readonly(even_odd), 20, 30) &&
            path::contains_path_point(readonly(non_zero), 50, 30),
        "even-odd and non-zero windings distinguish overlap holes from unions");
  const auto degenerate = path::create_path();
  path::append_path_move_to(degenerate, 0, 0);
  path::append_path_line_to(degenerate, 50, 0);
  path::append_path_line_to(degenerate, 100, 0);
  check(!path::contains_path_point(readonly(degenerate), 50, 0) &&
            path::get_path_signed_area(readonly(degenerate)) == 0 &&
            path::get_path_contour_orientation(readonly(degenerate)) ==
                flight::String("degenerate"),
        "degenerate contours have no interior, area, or orientation");
  const auto area = path::create_path();
  path::append_path_rectangle(area, 0, 0, 100, 50);
  check(close(std::abs(path::get_path_signed_area(readonly(area))), 5000) &&
            path::get_path_contour_orientation(readonly(area)) !=
                flight::String("degenerate"),
        "closed rectangles expose signed area and a concrete orientation");

  const auto transformed = path::create_path();
  path::append_path_move_to(transformed, 1, 2);
  path::append_path_line_to(transformed, 3, 4);
  const auto transformed_source = transformed->data.clone();
  const auto out = path::create_path();
  path::transform_path(readonly(transformed),
                       readonly(matrix(2, 0, 0, 3, 10, -5)), out);
  check(array_equals(out->data, {12, 1, 16, 7}) &&
            arrays_equal(transformed->data, transformed_source),
        "affine transforms apply scale/translation without mutating the source");
  path::translate_path(readonly(transformed), 10, 20, transformed);
  check(array_equals(transformed->data, {11, 22, 13, 24}),
        "translation is alias-safe");

  const auto morph_start = path::create_path(flight::String("evenOdd"));
  path::append_path_move_to(morph_start, 0, 0);
  path::append_path_line_to(morph_start, 6, 0);
  const auto morph_end = path::create_path(flight::String("evenOdd"));
  path::append_path_move_to(morph_end, 2, 2);
  path::append_path_quadratic_curve_to(morph_end, 5, 8, 8, 2);
  const auto morph = path::create_path_morph(readonly(morph_start),
                                             readonly(morph_end));
  check(morph.has_value() &&
            array_equals(morph.value()->commands,
                         {types::path_command->move_to,
                          types::path_command->cubic_curve_to}),
        "morph preparation normalizes compatible segment topology");
  const auto morph_out = path::create_path();
  path::sample_path_morph(morph_out, readonly(morph.value()), 0.5);
  check(array_equals(morph_out->data, {1, 1, 3, 3, 5, 3, 7, 1}) &&
            morph_out->winding == flight::String("evenOdd"),
        "morph sampling writes midpoint coordinates and winding into caller output");
  const auto incompatible = path::create_path(flight::String("nonZero"));
  check(!path::create_path_morph(readonly(incompatible), readonly(even_odd))
             .has_value(),
        "incompatible morph winding is rejected without mutating either endpoint");

  const auto square = path::create_path();
  path::append_path_polygon(square,
                            flight::Array<double>{0, 0, 10, 0, 10, 10, 0, 10});
  const auto mesh = path::tessellate_path(readonly(square));
  check(mesh->vertices.size() == 8 && mesh->indices.size() == 6,
        "fill tessellation owns four vertices and two triangles for a square");
  const auto mesh_out = flight::make_ref<types::PathMesh>(types::PathMesh{
      .vertices = flight::Array<double>{99, 99},
      .indices = flight::Array<double>{99},
  });
  const void* vertices_identity = mesh_out->vertices.identity();
  const void* indices_identity = mesh_out->indices.identity();
  path::tessellate_path_into(readonly(square), mesh_out);
  check(mesh_out->vertices.identity() == vertices_identity &&
            mesh_out->indices.identity() == indices_identity &&
            arrays_equal(mesh_out->vertices, mesh->vertices) &&
            arrays_equal(mesh_out->indices, mesh->indices),
        "fill tessellation clears and reuses caller-owned output arrays");

  const auto line = path::create_path();
  path::append_path_move_to(line, 0, 0);
  path::append_path_line_to(line, 100, 0);
  const auto valid_style = flight::make_ref<types::StrokeStyle>(
      types::StrokeStyle{.cap = flight::String("butt"), .width = 10});
  const auto invalid_style = flight::make_ref<types::StrokeStyle>(
      types::StrokeStyle{.width = std::numeric_limits<double>::quiet_NaN()});
  const auto stroke_mesh =
      path::tessellate_stroke_path(readonly(line), readonly(valid_style));
  const auto line_commands = line->commands.clone();
  const auto line_data = line->data.clone();
  check(stroke_mesh.has_value() && stroke_mesh.value()->indices.size() == 6 &&
            !path::tessellate_stroke_path(readonly(line), readonly(invalid_style))
                 .has_value() &&
            arrays_equal(line->commands, line_commands) &&
            arrays_equal(line->data, line_data),
        "stroke tessellation rejects non-finite style parameters and never mutates input");
}

}  // namespace

int main() {
  construction_segments_and_failure_atomicity();
  copy_reverse_and_edit_aliasing();
  contours_bounds_and_queries();
  winding_transforms_morphs_and_meshes();

  if (failures == 0) {
    std::cout << "generated path tests passed\n";
    return 0;
  }
  std::cerr << failures << " generated path test(s) failed\n";
  return 1;
}
