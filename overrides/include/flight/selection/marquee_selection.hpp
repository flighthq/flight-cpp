// Complete native implementation of Flight a627849 marqueeSelection.ts.
#pragma once

#include <cmath>
#include <optional>
#include <stdexcept>

#include <flight/geometry/rectangle.hpp>
#include <flight/node/bounds_rectangle.hpp>
#include <flight/structural_ref.hpp>
#include <flight/types/marquee_selection.hpp>
#include <flight/types/marquee_selection_runtime.hpp>
#include <flight/types/rectangle.hpp>

namespace flight::selection {

using ReadonlyMarqueeSelection =
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::MarqueeSelection>>>>;
using ReadonlyRectangle =
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Rectangle>>>>;

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

inline flight::Ref<flight::types::MarqueeSelection> exact_marquee_selection(
    const ReadonlyMarqueeSelection& selection) {
  auto exact = selection.template shared_exact_object<flight::types::MarqueeSelection>();
  if (!exact) throw std::bad_cast();
  return exact;
}

inline flight::Ref<flight::types::MarqueeSelectionRuntime> get_marquee_selection_runtime(
    const flight::Ref<flight::types::MarqueeSelection>& selection) {
  if (!selection->entity_runtime_key.has_value()) throw std::bad_cast();
  return std::static_pointer_cast<flight::types::MarqueeSelectionRuntime>(
      selection->entity_runtime_key.value());
}

inline flight::Ref<flight::types::MarqueeSelectionRuntime> get_marquee_selection_runtime(
    const ReadonlyMarqueeSelection& selection) {
  return get_marquee_selection_runtime(exact_marquee_selection(selection));
}

inline flight::Ref<flight::types::MarqueeSelection> create_marquee_selection() {
  auto selection = flight::make_ref<flight::types::MarqueeSelection>();
  auto runtime = flight::make_ref<flight::types::MarqueeSelectionRuntime>();
  runtime->binding = std::nullopt;
  runtime->uid = std::nullopt;
  runtime->active = false;
  runtime->rectangle = flight::geometry::create_rectangle();
  runtime->start_x = 0.0;
  runtime->start_y = 0.0;
  selection->entity_runtime_key = runtime;
  return selection;
}

inline flight::Ref<flight::types::Rectangle> end_marquee_selection(
    flight::Ref<flight::types::MarqueeSelection> selection) {
  auto runtime = get_marquee_selection_runtime(selection);
  runtime->active = false;
  return runtime->rectangle;
}

template <typename NodeType>
inline flight::Array<NodeType> find_nodes_in_marquee_selection(
    ReadonlyMarqueeSelection selection,
    flight::Array<NodeType> candidates,
    std::optional<flight::types::MarqueeSelectionMode> mode = std::nullopt) {
  const auto effective_mode = mode.value_or(flight::String("intersect"));
  auto rectangle = get_marquee_selection_runtime(selection)->rectangle;
  auto selection_bounds = flight::structural_ref_cast<ReadonlyRectangle>(rectangle);
  flight::Array<NodeType> matches;
  for (std::size_t i = 0; i < candidates.size(); ++i) {
    NodeType candidate = candidates.element(static_cast<double>(i));
    auto bounds = detail::get_selection_candidate_bounds(candidate);
    const bool selected = effective_mode == flight::String("contain")
                              ? flight::geometry::encloses_rectangle(selection_bounds, bounds)
                              : flight::geometry::intersects_rectangle(selection_bounds, bounds);
    if (selected) matches.push(candidate);
  }
  return matches;
}

inline ReadonlyRectangle get_marquee_rectangle(ReadonlyMarqueeSelection selection) {
  return flight::structural_ref_cast<ReadonlyRectangle>(
      get_marquee_selection_runtime(selection)->rectangle);
}

inline void set_marquee_rectangle(
    flight::Ref<flight::types::Rectangle> rectangle,
    double start_x,
    double start_y,
    double end_x,
    double end_y) {
  rectangle->x = flight::minimum(start_x, end_x);
  rectangle->y = flight::minimum(start_y, end_y);
  rectangle->width = std::abs(end_x - start_x);
  rectangle->height = std::abs(end_y - start_y);
}

inline void begin_marquee_selection(
    flight::Ref<flight::types::MarqueeSelection> selection, double start_x, double start_y) {
  auto runtime = get_marquee_selection_runtime(selection);
  runtime->active = true;
  runtime->start_x = start_x;
  runtime->start_y = start_y;
  set_marquee_rectangle(runtime->rectangle, start_x, start_y, start_x, start_y);
}

inline void update_marquee_selection(
    flight::Ref<flight::types::MarqueeSelection> selection, double current_x, double current_y) {
  auto runtime = get_marquee_selection_runtime(selection);
  if (!runtime->active) return;
  set_marquee_rectangle(
      runtime->rectangle, runtime->start_x, runtime->start_y, current_x, current_y);
}

} // namespace flight::selection
