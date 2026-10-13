// Complete native implementation of Flight a627849 lassoSelection.ts.
#pragma once

#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>

#include <flight/geometry/rectangle.hpp>
#include <flight/node/bounds_rectangle.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/contract.hpp>
#include <flight/types/lasso_selection.hpp>
#include <flight/types/lasso_selection_runtime.hpp>
#include <flight/types/path.hpp>
#include <flight/types/rectangle.hpp>

namespace flight::selection {

using ReadonlyLassoSelection =
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::LassoSelection>>>>;
using ReadonlyPath =
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Path>>>>;

#ifndef FLIGHT_SELECTION_BOUNDS_NODE_ADAPTER_HPP
#define FLIGHT_SELECTION_BOUNDS_NODE_ADAPTER_HPP
namespace detail {

using ReadonlyBoundsNodeAny = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::BoundsNodeAny>>>>;
using ReadonlyBoundsRectangle = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Rectangle>>>>;

inline ReadonlyBoundsRectangle get_selection_candidate_bounds(
    const flight::Ref<flight::types::BoundsNodeAny>& candidate) {
  if (!candidate->entity_runtime_key.has_value()) throw std::bad_cast();
  auto runtime = candidate->entity_runtime_key.value();
  ReadonlyBoundsNodeAny source(candidate);
  const bool valid =
      runtime->local_bounds_using_local_bounds_id == runtime->local_bounds_id &&
      (!runtime->is_local_bounds_rectangle_valid.has_value() ||
       runtime->is_local_bounds_rectangle_valid.value()(source));
  if (!valid) {
    if (!runtime->local_bounds_rectangle.has_value()) {
      runtime->local_bounds_rectangle = flight::geometry::create_rectangle();
    }
    runtime->compute_local_bounds_rectangle(runtime->local_bounds_rectangle.value(), source);
    runtime->local_bounds_using_local_bounds_id = runtime->local_bounds_id;
  }
  return flight::structural_ref_cast<ReadonlyBoundsRectangle>(
      runtime->local_bounds_rectangle.value());
}

template <typename NodeType>
inline auto get_selection_candidate_bounds(NodeType candidate) {
  return flight::node::get_node_local_bounds_rectangle(candidate);
}

} // namespace detail
#endif

inline flight::Ref<flight::types::LassoSelection> exact_lasso_selection(
    const ReadonlyLassoSelection& selection) {
  auto exact = selection.template shared_exact_object<flight::types::LassoSelection>();
  if (!exact) throw std::bad_cast();
  return exact;
}

inline flight::Ref<flight::types::LassoSelectionRuntime> get_lasso_selection_runtime(
    const flight::Ref<flight::types::LassoSelection>& selection) {
  if (!selection->entity_runtime_key.has_value()) throw std::bad_cast();
  return std::static_pointer_cast<flight::types::LassoSelectionRuntime>(
      selection->entity_runtime_key.value());
}

inline flight::Ref<flight::types::LassoSelectionRuntime> get_lasso_selection_runtime(
    const ReadonlyLassoSelection& selection) {
  return get_lasso_selection_runtime(exact_lasso_selection(selection));
}

inline void initialize_lasso_selection(
    flight::types::EntityConstruction<flight::Ref<flight::types::LassoSelection>>) {}

inline flight::Ref<flight::types::LassoSelection> create_lasso_selection() {
  auto selection = flight::make_ref<flight::types::LassoSelection>();
  auto path = flight::make_ref<flight::types::Path>();
  path->entity_runtime_key = std::nullopt;
  path->commands = flight::Array<double>{};
  path->data = flight::Array<double>{};
  path->winding = flight::String("evenOdd");

  auto runtime = flight::make_ref<flight::types::LassoSelectionRuntime>();
  runtime->binding = std::nullopt;
  runtime->uid = std::nullopt;
  runtime->active = false;
  runtime->path = path;
  selection->entity_runtime_key = runtime;
  return selection;
}

inline void add_lasso_selection_point(
    flight::Ref<flight::types::LassoSelection> selection, double x, double y) {
  auto runtime = get_lasso_selection_runtime(selection);
  if (!runtime->active) return;
  runtime->path->commands.push(flight::types::path_command->line_to);
  runtime->path->data.push(x, y);
}

inline void begin_lasso_selection(
    flight::Ref<flight::types::LassoSelection> selection, double start_x, double start_y) {
  auto runtime = get_lasso_selection_runtime(selection);
  runtime->active = true;
  runtime->path->commands.resize(std::size_t{0});
  runtime->path->data.resize(std::size_t{0});
  runtime->path->commands.push(flight::types::path_command->move_to);
  runtime->path->data.push(start_x, start_y);
}

inline flight::Ref<flight::types::Path> end_lasso_selection(
    flight::Ref<flight::types::LassoSelection> selection) {
  auto runtime = get_lasso_selection_runtime(selection);
  if (runtime->active) runtime->path->commands.push(flight::types::path_command->close);
  runtime->active = false;
  return runtime->path;
}

inline ReadonlyPath get_lasso_selection_path(ReadonlyLassoSelection selection) {
  return flight::structural_ref_cast<ReadonlyPath>(get_lasso_selection_runtime(selection)->path);
}

inline bool is_point_on_segment(
    double x, double y, double start_x, double start_y, double end_x, double end_y) {
  const double cross = (x - start_x) * (end_y - start_y) -
                       (y - start_y) * (end_x - start_x);
  const double scale = flight::maximum(
      1.0, std::abs(end_x - start_x), std::abs(end_y - start_y));
  if (std::abs(cross) > std::numeric_limits<double>::epsilon() * scale * 16.0) return false;
  return x >= flight::minimum(start_x, end_x) && x <= flight::maximum(start_x, end_x) &&
         y >= flight::minimum(start_y, end_y) && y <= flight::maximum(start_y, end_y);
}

inline bool is_point_in_polygon(flight::Array<double> points, double x, double y) {
  bool inside = false;
  const double point_count = static_cast<double>(points.size()) / 2.0;
  double previous_index = point_count - 1.0;
  for (double current_index = 0.0; current_index < point_count; current_index += 1.0) {
    const double current_x = points.element(current_index * 2.0);
    const double current_y = points.element(current_index * 2.0 + 1.0);
    const double previous_x = points.element(previous_index * 2.0);
    const double previous_y = points.element(previous_index * 2.0 + 1.0);
    if (is_point_on_segment(x, y, previous_x, previous_y, current_x, current_y)) return true;
    if ((current_y > y) != (previous_y > y) &&
        x < (previous_x - current_x) * (y - current_y) /
                    (previous_y - current_y) +
                current_x) {
      inside = !inside;
    }
    previous_index = current_index;
  }
  return inside;
}

template <typename NodeType>
inline flight::Array<NodeType> find_nodes_in_lasso_selection(
    ReadonlyLassoSelection selection, flight::Array<NodeType> candidates) {
  auto path = get_lasso_selection_runtime(selection)->path;
  if (path->data.size() < 6) return {};
  flight::Array<NodeType> matches;
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    NodeType candidate = candidates.element(static_cast<double>(i));
    auto bounds = detail::get_selection_candidate_bounds(candidate);
    if (is_point_in_polygon(
            path->data,
            flight::row_get<flight::RowKey<"x">>(bounds) +
                flight::row_get<flight::RowKey<"width">>(bounds) / 2.0,
            flight::row_get<flight::RowKey<"y">>(bounds) +
                flight::row_get<flight::RowKey<"height">>(bounds) / 2.0)) {
      matches.push(candidate);
    }
  }
  return matches;
}

} // namespace flight::selection
