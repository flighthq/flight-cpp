// Complete native implementation derived from Flight a627849 gizmoState.ts.
#pragma once
#include <cmath>
#include <cstdint>
#include <flight/any.hpp>
#include <flight/erased_ref.hpp>
#include <flight/number.hpp>
#include <flight/structural_ref.hpp>
#include <functional>
#include <limits>
#include <optional>
#include <random>
#include <typeinfo>
#include <variant>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

#include <flight/types/shape.hpp>

#include <flight/types/selection_state.hpp>

#include <flight/types/scene_2d.hpp>

#include <flight/types/rectangle.hpp>

#include <flight/types/node_2d.hpp>

#include <flight/types/gizmo_state.hpp>

#include <flight/types/camera_2d.hpp>

namespace flight::types { struct Camera2D; }
namespace flight::types { struct Entity; }
namespace flight::types { struct EntityRuntime; }
namespace flight::types { struct FocusEventData; }
namespace flight::types { struct GizmoSignals; }
namespace flight::types { struct InteractionSignals; }
namespace flight::types { struct KeyboardEventData; }
namespace flight::types { struct Node2DTraits; }
namespace flight::types { struct PointerEventData; }
namespace flight::types { struct Rectangle; }
namespace flight::types { struct Scene2D; }
namespace flight::types { struct Shape; }
namespace flight::types { struct Vector2; }
namespace flight::types { template <typename NodeType> struct GizmoCreateOptions; }
namespace flight::types { template <typename NodeType> struct GizmoNode2DFeatures; }
namespace flight::types { template <typename NodeType> struct GizmoState; }
namespace flight::types { template <typename NodeType> struct SelectionState; }
namespace flight::types { template <typename T> struct Signal; }
namespace flight::types { template <typename Traits> struct Node; }

namespace flight::gizmo {

using flight::types::Camera2D;
using flight::types::GizmoNode2DFeatures;
using flight::types::GizmoSignals;
using flight::types::GizmoState;
using flight::types::Rectangle;
using flight::types::Scene2D;
using flight::types::SelectionState;
using flight::types::Shape;
using flight::types::Node2DTraits;
struct GizmoDrag;
struct GizmoHandle;
template <typename NodeType>
struct GizmoRuntime;

} // namespace flight::gizmo

#include <flight/camera/projection2d.hpp>
#include <flight/geometry/rectangle.hpp>
#include <flight/geometry/vector2.hpp>
#include <flight/interaction/node_interaction_state.hpp>
#include <flight/math/constants.hpp>
#include <flight/math/rounding.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/revision.hpp>
#include <flight/selection/selection_state.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape.hpp>
#include <flight/signals/emitter.hpp>
#include <flight/signals/signal.hpp>
#include <flight/signals/slot.hpp>
#include <flight/types/camera_2d.hpp>
#include <flight/types/cursor.hpp>
#include <flight/types/display_object.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/focus_event_data.hpp>
#include <flight/types/gizmo_state.hpp>
#include <flight/types/hierarchy_node.hpp>
#include <flight/types/interaction_signals.hpp>
#include <flight/types/keyboard_event_data.hpp>
#include <flight/types/node.hpp>
#include <flight/types/node_2d.hpp>
#include <flight/types/node_interaction.hpp>
#include <flight/types/partial_node.hpp>
#include <flight/types/pointer_event_data.hpp>
#include <flight/types/rectangle.hpp>
#include <flight/types/scene_2d.hpp>
#include <flight/types/selection_state.hpp>
#include <flight/types/shape.hpp>
#include <flight/types/shape_command.hpp>
#include <flight/types/signal.hpp>
#include <flight/types/vector2.hpp>
#include <flight/types/viewport_align.hpp>
#include <flight/types/viewport_scale_mode.hpp>

namespace flight::gizmo {

struct GizmoDrag : public flight::ReferenceEnabled {
  double axis_rotation;
  flight::types::GizmoHandleKind handle;
  flight::types::GizmoTransformMode mode;
  double pivot_world_x;
  double pivot_world_y;
  double pivot_screen_x;
  double pivot_screen_y;
  double pointer_id;
  double screen_rotation;
  double start_screen_x;
  double start_screen_y;
  double start_world_x;
  double start_world_y;
};

using GizmoHandlePlacement = flight::String;

struct GizmoHandle : public flight::ReferenceEnabled {
  flight::types::GizmoHandleKind kind;
  flight::Ref<flight::types::Shape> node;
  GizmoHandlePlacement placement;
};

template <typename NodeType>
struct GizmoRuntime : public flight::types::EntityRuntime {
  flight::Ref<flight::types::Rectangle> bounds;
  std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Camera2D>>>>> camera;
  flight::Array<std::function<void()>> cleanups;
  double custom_pivot_x;
  double custom_pivot_y;
  bool disposed;
  std::optional<flight::Ref<GizmoDrag>> drag;
  std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GizmoNode2DFeatures<NodeType>>>>>> features;
  flight::types::Node2D handle_root;
  flight::Array<flight::Ref<GizmoHandle>> handles;
  flight::Ref<flight::types::Rectangle> node_bounds;
  flight::Ref<flight::types::Shape> outline;
  double outline_color;
  bool outline_enabled;
  flight::Array<double> outline_points;
  std::optional<flight::Ref<flight::types::Scene2D>> overlay;
  flight::types::Node2D overlay_root;
  flight::types::GizmoMode mode;
  double viewport_height;
  double viewport_width;
  flight::types::GizmoPivot pivot;
  flight::types::Vector2Like pivot_screen;
  flight::types::Vector2Like pivot_world;
  double snap_rotation;
  double snap_scale;
  double snap_translate;
  flight::types::GizmoSpace space;
  flight::types::Vector2Like scratch_point;
  std::optional<flight::Ref<flight::types::SelectionState<NodeType>>> selection;
  flight::Ref<flight::types::GizmoSignals> signals;
};

using GizmoPointerSlot = std::function<void(
    flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::PointerEventData>>>>)>;
using GizmoPointerFunction = flight::Function<void(
    flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::PointerEventData>>>>)>;

template <typename SignalRef>
inline SignalRef create_gizmo_interaction_signal() {
  using Slot = flight::template_argument_t<typename SignalRef::element_type>;
  return flight::signals::create_signal<Slot>();
}

inline flight::Ref<flight::types::InteractionSignals> create_gizmo_interaction_signals() {
  auto out = flight::make_ref<flight::types::InteractionSignals>();
  out->entity_runtime_key = std::nullopt;
  out->on_click = create_gizmo_interaction_signal<decltype(out->on_click)>();
  out->on_context_menu = create_gizmo_interaction_signal<decltype(out->on_context_menu)>();
  out->on_double_click = create_gizmo_interaction_signal<decltype(out->on_double_click)>();
  out->on_focus_in = create_gizmo_interaction_signal<decltype(out->on_focus_in)>();
  out->on_focus_out = create_gizmo_interaction_signal<decltype(out->on_focus_out)>();
  out->on_key_down = create_gizmo_interaction_signal<decltype(out->on_key_down)>();
  out->on_key_up = create_gizmo_interaction_signal<decltype(out->on_key_up)>();
  out->on_pointer_cancel = create_gizmo_interaction_signal<decltype(out->on_pointer_cancel)>();
  out->on_pointer_double_click =
      create_gizmo_interaction_signal<decltype(out->on_pointer_double_click)>();
  out->on_pointer_down = create_gizmo_interaction_signal<decltype(out->on_pointer_down)>();
  out->on_pointer_move = create_gizmo_interaction_signal<decltype(out->on_pointer_move)>();
  out->on_pointer_out = create_gizmo_interaction_signal<decltype(out->on_pointer_out)>();
  out->on_pointer_over = create_gizmo_interaction_signal<decltype(out->on_pointer_over)>();
  out->on_pointer_roll_out = create_gizmo_interaction_signal<decltype(out->on_pointer_roll_out)>();
  out->on_pointer_roll_over = create_gizmo_interaction_signal<decltype(out->on_pointer_roll_over)>();
  out->on_pointer_up = create_gizmo_interaction_signal<decltype(out->on_pointer_up)>();
  out->on_release_outside = create_gizmo_interaction_signal<decltype(out->on_release_outside)>();
  out->on_wheel = create_gizmo_interaction_signal<decltype(out->on_wheel)>();
  return out;
}

inline flight::Ref<flight::types::InteractionSignals> enable_gizmo_interaction_signals(
    const flight::Ref<flight::types::Shape>& source) {
  if (!source || !source->entity_runtime_key.has_value()) {
    throw flight::Error(flight::String("Gizmo handle interaction runtime is unavailable"));
  }
  auto runtime = source->entity_runtime_key.value();
  if (!runtime->interaction_signals.has_value()) {
    runtime->interaction_signals = create_gizmo_interaction_signals();
  }
  return runtime->interaction_signals.value();
}

inline flight::types::Node2D gizmo_node_view(
    const flight::Ref<flight::types::Shape>& source) {
  return flight::structural_ref_cast<flight::types::Node2D>(
      flight::StructuralRef<
          flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Shape>>>>(source));
}

inline auto get_gizmo_node_runtime(const flight::types::Node2D& source) {
  if (auto shape = source.template shared_exact_object<flight::types::Shape>()) {
    if (!shape->entity_runtime_key.has_value()) throw std::bad_cast();
    return shape->entity_runtime_key.value();
  }
  return flight::scene2d::get_node_2d_runtime(source);
}

inline void add_gizmo_node_child(
    const flight::types::Node2D& target,
    const flight::types::Node2D& child) {
  auto target_runtime = get_gizmo_node_runtime(target);
  auto child_runtime = get_gizmo_node_runtime(child);
  if (!target_runtime->children.has_value()) {
    target_runtime->children = flight::Array<flight::types::Node2D>{};
  }
  if (target_runtime->children.value().index_of(child) == -1.0) {
    target_runtime->children.value().push(child);
    target_runtime->children_id =
        flight::unsigned_right_shift(target_runtime->children_id + 1.0, 0.0);
  }
  child_runtime->parent = child == target
      ? std::optional<flight::types::Node2D>{}
      : std::optional<flight::types::Node2D>{target};
  child_runtime->parent_reference_id =
      flight::unsigned_right_shift(child_runtime->parent_reference_id + 1.0, 0.0);
  child_runtime->world_transform_using_parent_transform_id = -1.0;
  if (target_runtime->node_signals.has_value()) {
    flight::signals::emit_signal<std::function<void()>>(
        target_runtime->node_signals.value()->on_children_changed);
  }
  if (child_runtime->node_signals.has_value()) {
    flight::signals::emit_signal<std::function<void()>>(
        child_runtime->node_signals.value()->on_parent_changed);
  }
}

inline void remove_gizmo_node_child(
    const flight::types::Node2D& target,
    const flight::types::Node2D& child) {
  auto target_runtime = get_gizmo_node_runtime(target);
  auto child_runtime = get_gizmo_node_runtime(child);
  if (!target_runtime->children.has_value()) return;
  const double index = target_runtime->children.value().index_of(child);
  if (index < 0.0) return;
  target_runtime->children.value().splice(index, 1.0);
  if (target_runtime->children.value().empty()) target_runtime->children = std::nullopt;
  target_runtime->children_id =
      flight::unsigned_right_shift(target_runtime->children_id + 1.0, 0.0);
  child_runtime->parent = std::nullopt;
  child_runtime->parent_reference_id =
      flight::unsigned_right_shift(child_runtime->parent_reference_id + 1.0, 0.0);
  child_runtime->world_transform_using_parent_transform_id = -1.0;
  if (target_runtime->node_signals.has_value()) {
    flight::signals::emit_signal<std::function<void()>>(
        target_runtime->node_signals.value()->on_children_changed);
  }
  if (child_runtime->node_signals.has_value()) {
    flight::signals::emit_signal<std::function<void()>>(
        child_runtime->node_signals.value()->on_parent_changed);
  }
}

inline std::optional<flight::types::Node2D> get_gizmo_node_parent(
    const flight::types::Node2D& source) {
  return get_gizmo_node_runtime(source)->parent;
}

inline flight::types::ShapeCommandToken gizmo_shape_string(flight::String value) {
  return flight::types::ShapeCommandToken{
      std::in_place, std::in_place_type<flight::String>, std::move(value)};
}

inline flight::types::ShapeCommandToken gizmo_shape_number(double value) {
  return flight::types::ShapeCommandToken{
      std::in_place, std::in_place_type<double>, value};
}

inline flight::types::ShapeCommandToken gizmo_shape_boolean(bool value) {
  return flight::types::ShapeCommandToken{
      std::in_place, std::in_place_type<bool>, value};
}

inline void append_gizmo_shape_command(
    const flight::Ref<flight::types::Shape>& shape,
    flight::String name,
    flight::Array<flight::types::ShapeCommandToken> arguments) {
  shape->data->commands.push(gizmo_shape_string(std::move(name)));
  shape->data->commands.push(
      gizmo_shape_number(static_cast<double>(arguments.size())));
  for (std::size_t i = 0; i < arguments.size(); ++i) {
    shape->data->commands.push(arguments.element(static_cast<double>(i)));
  }
  flight::node::invalidate_content(shape);
}

inline void append_gizmo_begin_fill(
    const flight::Ref<flight::types::Shape>& shape,
    double color,
    double alpha = 1.0) {
  append_gizmo_shape_command(
      shape, flight::String("beginFill"),
      {gizmo_shape_number(color), gizmo_shape_number(alpha)});
}

inline void append_gizmo_end_fill(const flight::Ref<flight::types::Shape>& shape) {
  append_gizmo_shape_command(shape, flight::String("endFill"), {});
}

inline void append_gizmo_line_style(
    const flight::Ref<flight::types::Shape>& shape,
    double thickness,
    double color,
    double alpha) {
  append_gizmo_shape_command(
      shape, flight::String("lineStyle"),
      {gizmo_shape_number(thickness), gizmo_shape_number(color), gizmo_shape_number(alpha),
       gizmo_shape_boolean(false), gizmo_shape_string(flight::String("none")),
       gizmo_shape_string(flight::String("none")), gizmo_shape_string(flight::String("round")),
       gizmo_shape_number(3.0)});
}

inline void append_gizmo_move_to(
    const flight::Ref<flight::types::Shape>& shape, double x, double y) {
  append_gizmo_shape_command(
      shape, flight::String("moveTo"), {gizmo_shape_number(x), gizmo_shape_number(y)});
}

inline void append_gizmo_line_to(
    const flight::Ref<flight::types::Shape>& shape, double x, double y) {
  append_gizmo_shape_command(
      shape, flight::String("lineTo"), {gizmo_shape_number(x), gizmo_shape_number(y)});
}

inline void append_gizmo_circle(
    const flight::Ref<flight::types::Shape>& shape,
    double center_x,
    double center_y,
    double radius) {
  append_gizmo_shape_command(
      shape, flight::String("drawCircle"),
      {gizmo_shape_number(center_x), gizmo_shape_number(center_y),
       gizmo_shape_number(radius)});
}

inline void append_gizmo_rectangle(
    const flight::Ref<flight::types::Shape>& shape,
    double x,
    double y,
    double width,
    double height) {
  append_gizmo_shape_command(
      shape, flight::String("drawRectangle"),
      {gizmo_shape_number(x), gizmo_shape_number(y), gizmo_shape_number(width),
       gizmo_shape_number(height)});
}

inline void append_gizmo_polygon(
    const flight::Ref<flight::types::Shape>& shape,
    const flight::Array<double>& points) {
  if (points.size() < 4) return;
  auto& commands = shape->data->commands;
  auto push_segment = [&](const char* name, double x, double y) {
    commands.push(gizmo_shape_string(flight::String(name)));
    commands.push(gizmo_shape_number(2.0));
    commands.push(gizmo_shape_number(x));
    commands.push(gizmo_shape_number(y));
  };
  push_segment("moveTo", points.element(0.0), points.element(1.0));
  for (std::size_t i = 2; i + 1 < points.size(); i += 2) {
    push_segment(
        "lineTo", points.element(static_cast<double>(i)),
        points.element(static_cast<double>(i + 1)));
  }
  push_segment("lineTo", points.element(0.0), points.element(1.0));
  flight::node::invalidate_content(shape);
}

inline void set_gizmo_shape_hit_area(
    const flight::Ref<flight::types::Shape>& shape,
    const flight::Ref<flight::types::Rectangle>& rectangle) {
  using RectangleView = flight::StructuralRef<flight::RowReadonly<
      flight::RowOf<flight::Ref<flight::types::Rectangle>>>>;
  auto view = flight::structural_ref_cast<RectangleView>(
      flight::StructuralRef<flight::RowWritable<
          flight::RowOf<flight::Ref<flight::types::Rectangle>>>>(rectangle));
  auto state = flight::interaction::enable_node_interaction_state(shape);
  state->hit_area = flight::types::HitArea{
      std::in_place_type<RectangleView>, std::move(view)};
}

template <typename NodeType>
inline bool compute_gizmo_bounds(flight::Ref<GizmoRuntime<NodeType>> runtime, flight::Array<NodeType> selected) {
  const auto& features = runtime->features.value();
  bool found = false;
  double min_x = std::numeric_limits<double>::infinity();
  double min_y = std::numeric_limits<double>::infinity();
  double max_x = -std::numeric_limits<double>::infinity();
  double max_y = -std::numeric_limits<double>::infinity();
  {
    double i = 0.0;
    while ((i < static_cast<double>(selected.size()))) {
      {
        if (!flight::row_get<flight::RowKey<"getWorldBoundsRectangle">>(features)(runtime->node_bounds, selected.element(i))) {
          (i += 1.0);
          continue;
        }
        auto bounds = runtime->node_bounds;
        const double x0 = flight::minimum(bounds->x, (bounds->x + bounds->width));
        const double y0 = flight::minimum(bounds->y, (bounds->y + bounds->height));
        const double x1 = flight::maximum(bounds->x, (bounds->x + bounds->width));
        const double y1 = flight::maximum(bounds->y, (bounds->y + bounds->height));
        (min_x = flight::minimum(min_x, x0));
        (min_y = flight::minimum(min_y, y0));
        (max_x = flight::maximum(max_x, x1));
        (max_y = flight::maximum(max_y, y1));
        (found = true);
      }
      (i += 1.0);
    }
  }
  if (!found) {
    return false;
  }
  (runtime->bounds->x = min_x);
  (runtime->bounds->y = min_y);
  (runtime->bounds->width = (max_x - min_x));
  (runtime->bounds->height = (max_y - min_y));
  return true;
}

template <typename NodeType>
inline void finish_gizmo_transform(flight::Ref<GizmoRuntime<NodeType>> runtime) {
  if (!runtime->drag.has_value()) {
    return;
  }
  (runtime->drag = std::nullopt);
  flight::signals::emit_signal<std::function<void()>>(runtime->signals->on_transform_end);
}

inline flight::String get_gizmo_handle_cursor(flight::types::GizmoHandleKind kind) {
  if ((kind == flight::String("translate-x"))) {
    return flight::String("ew-resize");
  }
  if ((kind == flight::String("translate-y"))) {
    return flight::String("ns-resize");
  }
  if ((kind == flight::String("scale-x"))) {
    return flight::String("ew-resize");
  }
  if ((kind == flight::String("scale-y"))) {
    return flight::String("ns-resize");
  }
  if ((kind == flight::String("scale-xy"))) {
    return flight::String("nwse-resize");
  }
  if ((kind == flight::String("rotate"))) {
    return flight::String("nesw-resize");
  }
  return flight::String("move");
}


// NOT GENERATED: function getGizmoRuntime -- source line 492
// refusal: cpp-structural-assertion-owner-unproven [source-portability]
//
// The source it stood for:
//   function getGizmoRuntime<NodeType extends HierarchyNodeAny>(
//     state: Readonly<GizmoState<NodeType>>,
//   ): GizmoRuntime<NodeType> {
//     return state[EntityRuntimeKey] as GizmoRuntime<NodeType>;
//   }
//
// cpp emission failed for @flighthq/gizmo/packages/gizmo/src/gizmoState.ts: the asserted row from
// flight::Ref<flight::types::EntityRuntime> to flight::Ref<GizmoRuntime<NodeType>> reads bounds, camera,
// cleanups, customPivotX, customPivotY, disposed, drag, features, handleRoot, handles, mode, nodeBounds,
// outline, outlineColor, outlineEnabled, outlinePoints, overlay, overlayRoot, pivot, pivotScreen, pivotWorld,
// scratchPoint, selection, signals, snapRotation, snapScale, snapTranslate, space, viewportHeight and
// viewportWidth, which the source type does not declare, and a structural owner binds the members and storage
// representations of the type the object was first reached as, so a member the source's own declaration lacks or
// carries differently has no compatible cell to answer the read. Reading the value back from retained storage
// does not recover a wider owner: an assertion cannot add those cells, change their representation, or prove
// which concrete owner was stored. Preserve the concrete owner in the source type through every storage,
// callback, and accessor boundary. For reusable scratch storage, retain indices, keys, or tokens into storage
// that remains typed as GizmoRuntime<NodeType>, rather than storing an erased reference and asserting it back;
// an identity map, tag, sort key, or registry key carried beside the reference identifies an entry but does not
// prove its owner. If heterogeneous storage is intentional, give it a checked recovery contract that validates
// and returns the exact owner before dispatch. Declare the source as a type that declares bounds, camera,
// cleanups, customPivotX, customPivotY, disposed, drag, features, handleRoot, handles, mode, nodeBounds,
// outline, outlineColor, outlineEnabled, outlinePoints, overlay, overlayRoot, pivot, pivotScreen, pivotWorld,
// scratchPoint, selection, signals, snapRotation, snapScale, snapTranslate, space, viewportHeight and
// viewportWidth -- construct GizmoRuntime<NodeType> explicitly, then type the retaining slot and every accessor
// result as GizmoRuntime<NodeType> wherever that concrete owner is known; changing only the accessor result
// cannot recover a wider owner after a base-typed slot erased it -- rather than asserting past
// flight::Ref<flight::types::EntityRuntime>. The compiler will not reinterpret the owner, cast it, copy or
// materialize a replacement, or add side storage.

template <typename NodeType>
inline flight::Ref<GizmoRuntime<NodeType>> get_gizmo_runtime(
    flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>> state) {
  const auto owner =
      state.template shared_exact_object<flight::types::GizmoState<NodeType>>();
  if (!owner || !owner->entity_runtime_key.has_value() ||
      !owner->entity_runtime_key.value()) {
    throw std::bad_cast();
  }
  return std::static_pointer_cast<GizmoRuntime<NodeType>>(
      owner->entity_runtime_key.value());
}


// NOT GENERATED: function disposeGizmoState -- source line 181
// refusal: cpp-presence-test-without-absence-storage [compiler-restriction]
//
// The source it stood for:
//   export function disposeGizmoState<NodeType extends HierarchyNodeAny>(state: GizmoState<NodeType>): void {
//     const runtime = getGizmoRuntime(state);
//     if (runtime.disposed) return;
//     finishGizmoTransform(runtime);
//     runtime.disposed = true;
//     for (let i = runtime.cleanups.length - 1; i >= 0; i--) runtime.cleanups[i]();
//     runtime.cleanups.length = 0;
//     const parent = getNodeParent(runtime.overlayRoot);
//     if (parent !== null) removeNodeChild(parent, runtime.overlayRoot);
//     runtime.camera = null;
//     runtime.features = null;
//     runtime.overlay = null;
//     runtime.selection = null;
//   }
//
// cpp emission failed for @flighthq/gizmo/packages/gizmo/src/gizmoState.ts: a presence test against null has no
// absence channel in the emitted C++ storage for identifier

template <typename NodeType>
inline void dispose_gizmo_state(
    flight::Ref<flight::types::GizmoState<NodeType>> state) {
  auto view = flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<
      flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>>(
      flight::StructuralRef<flight::RowWritable<
          flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>(state));
  auto runtime = get_gizmo_runtime<NodeType>(view);
  if (runtime->disposed) return;

  finish_gizmo_transform<NodeType>(runtime);
  runtime->disposed = true;
  for (std::size_t index = runtime->cleanups.size(); index > 0; --index) {
    runtime->cleanups.element(static_cast<double>(index - 1))();
  }
  runtime->cleanups.clear();

  if (const auto parent = get_gizmo_node_parent(runtime->overlay_root);
      parent.has_value()) {
    remove_gizmo_node_child(parent.value(), runtime->overlay_root);
  }
  runtime->camera = std::nullopt;
  runtime->features = std::nullopt;
  runtime->overlay = std::nullopt;
  runtime->selection = std::nullopt;
}

template <typename NodeType>
inline flight::types::GizmoMode get_gizmo_mode(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>> state) {
  return get_gizmo_runtime<NodeType>(state)->mode;
}

template <typename NodeType>
inline flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GizmoSignals>>>> get_gizmo_signals(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>> state) {
  return flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GizmoSignals>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::GizmoSignals>>>>(get_gizmo_runtime<NodeType>(state)->signals));
}

template <typename NodeType>
inline flight::types::GizmoSpace get_gizmo_space(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>> state) {
  return get_gizmo_runtime<NodeType>(state)->space;
}

template <typename NodeType>
inline void set_gizmo_custom_pivot(flight::Ref<flight::types::GizmoState<NodeType>> state, double x, double y) {
  auto runtime = get_gizmo_runtime<NodeType>(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>(state)));
  (runtime->custom_pivot_x = x);
  (runtime->custom_pivot_y = y);
}

template <typename NodeType>
inline void set_gizmo_mode(flight::Ref<flight::types::GizmoState<NodeType>> state, flight::types::GizmoMode mode) {
  auto runtime = get_gizmo_runtime<NodeType>(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>(state)));
  if ((runtime->mode == mode)) {
    return;
  }
  finish_gizmo_transform<NodeType>(runtime);
  (runtime->mode = mode);
}

template <typename NodeType>
inline void set_gizmo_pivot(flight::Ref<flight::types::GizmoState<NodeType>> state, flight::types::GizmoPivot pivot) {
  (get_gizmo_runtime<NodeType>(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>(state)))->pivot = pivot);
}

template <typename NodeType>
inline void set_gizmo_selection_outline_color(flight::Ref<flight::types::GizmoState<NodeType>> state, double color) {
  (get_gizmo_runtime<NodeType>(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>(state)))->outline_color = flight::unsigned_right_shift(color, 0.0));
}

template <typename NodeType>
inline void set_gizmo_selection_outline_enabled(flight::Ref<flight::types::GizmoState<NodeType>> state, bool enabled) {
  (get_gizmo_runtime<NodeType>(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>(state)))->outline_enabled = enabled);
}

template <typename NodeType>
inline void set_gizmo_space(flight::Ref<flight::types::GizmoState<NodeType>> state, flight::types::GizmoSpace space) {
  (get_gizmo_runtime<NodeType>(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>(state)))->space = space);
}

inline flight::types::GizmoTransformMode get_gizmo_transform_mode(flight::types::GizmoHandleKind kind) {
  if ((kind == flight::String("rotate"))) {
    return flight::String("rotate");
  }
  if ((((kind == flight::String("scale-x")) || (kind == flight::String("scale-y"))) || (kind == flight::String("scale-xy")))) {
    return flight::String("scale");
  }
  return flight::String("translate");
}

inline double normalize_degrees(double degrees) {
  double value = std::fmod(degrees, 360.0);
  if ((value > 180.0)) {
    (value -= 360.0);
  }
  else {
    if ((value < -180.0)) {
      (value += 360.0);
    }
  }
  return value;
}

inline double normalize_gizmo_snap(double value) {
  return (((value > 0.0) && std::isfinite(value)) ? value : 0.0);
}

template <typename NodeType>
inline void set_gizmo_snap_rotation(flight::Ref<flight::types::GizmoState<NodeType>> state, double degrees) {
  (get_gizmo_runtime<NodeType>(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>(state)))->snap_rotation = normalize_gizmo_snap(degrees));
}

template <typename NodeType>
inline void set_gizmo_snap_scale(flight::Ref<flight::types::GizmoState<NodeType>> state, double step) {
  (get_gizmo_runtime<NodeType>(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>(state)))->snap_scale = normalize_gizmo_snap(step));
}

template <typename NodeType>
inline void set_gizmo_snap_translate(flight::Ref<flight::types::GizmoState<NodeType>> state, double world_units) {
  (get_gizmo_runtime<NodeType>(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>(state)))->snap_translate = normalize_gizmo_snap(world_units));
}

template <typename NodeType>
inline void set_gizmo_pivot_to_bounds_center(flight::Ref<GizmoRuntime<NodeType>> runtime) {
  (runtime->pivot_world->x = (runtime->bounds->x + (runtime->bounds->width * 0.5)));
  (runtime->pivot_world->y = (runtime->bounds->y + (runtime->bounds->height * 0.5)));
}

template <typename NodeType>
inline void resolve_gizmo_pivot(flight::Ref<GizmoRuntime<NodeType>> runtime) {
  if (((runtime->mode != flight::String("rotate")) && (runtime->mode != flight::String("scale")))) {
    set_gizmo_pivot_to_bounds_center<NodeType>(runtime);
    return;
  }
  if ((runtime->pivot == flight::String("custom"))) {
    (runtime->pivot_world->x = runtime->custom_pivot_x);
    (runtime->pivot_world->y = runtime->custom_pivot_y);
    return;
  }
  if ((runtime->pivot == flight::String("origin"))) {
    runtime->features.value()->get_world_origin(runtime->pivot_world, flight::selection::get_active_node<NodeType>(runtime->selection.value()).value());
    return;
  }
  set_gizmo_pivot_to_bounds_center<NodeType>(runtime);
}

template <typename NodeType>
inline double resolve_gizmo_screen_rotation(flight::Ref<GizmoRuntime<NodeType>> runtime) {
  if ((runtime->space == flight::String("world"))) {
    return 0.0;
  }
  const double world_rotation = runtime->features.value()->get_world_rotation(flight::selection::get_active_node<NodeType>(runtime->selection.value()).value());
  return (world_rotation - (runtime->camera.value()->rotation * flight::math::rad_to_deg_flight_value_variable__u000052__u000041__u000044__u00005f__u000054__u00004f__u00005f__u000044__u000045__u000047__flight_source_ad5040a7d8c07bd3));
}

inline void set_gizmo_node_transform(flight::types::Node2D node, double x, double y, double rotation) {
  if ((((flight::row_get<flight::RowKey<"x">>(node) == x) && (flight::row_get<flight::RowKey<"y">>(node) == y)) && (flight::row_get<flight::RowKey<"rotation">>(node) == rotation))) {
    return;
  }
  flight::row_set<flight::RowKey<"x">>(node, x);
  flight::row_set<flight::RowKey<"y">>(node, y);
  flight::row_set<flight::RowKey<"rotation">>(node, rotation);
  auto runtime = get_gizmo_node_runtime(node);
  runtime->local_transform_id =
      flight::unsigned_right_shift(runtime->local_transform_id + 1.0, 0.0);
}

inline void set_gizmo_node_visible(flight::types::Node2D node, bool visible) {
  if ((flight::row_get<flight::RowKey<"visible">>(node) == visible)) {
    return;
  }
  flight::row_set<flight::RowKey<"visible">>(node, visible);
  auto runtime = get_gizmo_node_runtime(node);
  runtime->appearance_id =
      flight::unsigned_right_shift(runtime->appearance_id + 1.0, 0.0);
}

inline double snap_gizmo_delta(double value, double step) {
  return ((step == 0.0) ? value : flight::math::round_to(value, step));
}

inline double snap_gizmo_scale(double value, double step) {
  return ((step == 0.0) ? value : (1.0 + flight::math::round_to((value - 1.0), step)));
}

template <typename NodeType>
inline void start_gizmo_transform(flight::Ref<GizmoRuntime<NodeType>> runtime, flight::types::GizmoHandleKind handle, flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::PointerEventData>>>> data) {
  flight::types::GizmoTransformMode mode = get_gizmo_transform_mode(handle);
  if (((runtime->disposed || runtime->drag.has_value()) || (runtime->mode != mode))) {
    return;
  }
  std::optional<NodeType> active = flight::selection::get_active_node<NodeType>(runtime->selection.value());
  const double axis_rotation = (((runtime->space == flight::String("local")) && active.has_value()) ? runtime->features.value()->get_world_rotation(active.value()) : (runtime->camera.value()->rotation * flight::math::rad_to_deg_flight_value_variable__u000052__u000041__u000044__u00005f__u000054__u00004f__u00005f__u000044__u000045__u000047__flight_source_ad5040a7d8c07bd3));
  flight::camera::unproject_camera_2d_point(runtime->camera.value(), runtime->viewport_width, runtime->viewport_height, flight::row_get<flight::RowKey<"x">>(data), flight::row_get<flight::RowKey<"y">>(data), runtime->scratch_point);
  (runtime->drag = std::optional<flight::Ref<GizmoDrag>>{flight::make_ref<GizmoDrag>(GizmoDrag{.axis_rotation = axis_rotation, .handle = handle, .mode = mode, .pivot_world_x = runtime->pivot_world->x, .pivot_world_y = runtime->pivot_world->y, .pivot_screen_x = runtime->pivot_screen->x, .pivot_screen_y = runtime->pivot_screen->y, .pointer_id = flight::row_get<flight::RowKey<"pointerId">>(data), .screen_rotation = (flight::row_get<flight::RowKey<"rotation">>(runtime->handle_root) * flight::math::deg_to_rad_flight_value_variable__u000044__u000045__u000047__u00005f__u000054__u00004f__u00005f__u000052__u000041__u000044__flight_source_ad5040a7d8c07bd3), .start_screen_x = flight::row_get<flight::RowKey<"x">>(data), .start_screen_y = flight::row_get<flight::RowKey<"y">>(data), .start_world_x = runtime->scratch_point->x, .start_world_y = runtime->scratch_point->y})});
  flight::signals::emit_signal<std::function<void()>>(runtime->signals->on_transform_begin);
}

inline flight::String to_gizmo_handle_name(GizmoHandlePlacement placement) {
  if ((placement == flight::String("rotate"))) {
    return flight::String("Rotate");
  }
  if ((placement == flight::String("scale-east"))) {
    return flight::String("ScaleEast");
  }
  if ((placement == flight::String("scale-west"))) {
    return flight::String("ScaleWest");
  }
  if ((placement == flight::String("scale-north"))) {
    return flight::String("ScaleNorth");
  }
  if ((placement == flight::String("scale-south"))) {
    return flight::String("ScaleSouth");
  }
  if ((placement == flight::String("scale-northeast"))) {
    return flight::String("ScaleNortheast");
  }
  if ((placement == flight::String("scale-northwest"))) {
    return flight::String("ScaleNorthwest");
  }
  if ((placement == flight::String("scale-southeast"))) {
    return flight::String("ScaleSoutheast");
  }
  if ((placement == flight::String("scale-southwest"))) {
    return flight::String("ScaleSouthwest");
  }
  if ((placement == flight::String("translate-x"))) {
    return flight::String("TranslateX");
  }
  if ((placement == flight::String("translate-y"))) {
    return flight::String("TranslateY");
  }
  return flight::String("TranslateXY");
}

template <typename NodeType>
inline void update_gizmo_handle_visibility(flight::Ref<GizmoRuntime<NodeType>> runtime, flight::types::GizmoMode mode) {
  {
    double i = 0.0;
    while ((i < static_cast<double>(runtime->handles.size()))) {
      {
        auto handle = runtime->handles.element(i);
        set_gizmo_node_visible(flight::structural_ref_cast<flight::types::Node2D>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Shape>>>>(handle->node)), (get_gizmo_transform_mode(handle->kind) == mode));
      }
      (i += 1.0);
    }
  }
}

template <typename NodeType>
inline void update_gizmo_outline(flight::Ref<GizmoRuntime<NodeType>> runtime) {
  auto bounds = runtime->bounds;
  flight::Array<double> points = runtime->outline_points;
  flight::camera::project_camera_2d_point(runtime->camera.value(), runtime->viewport_width, runtime->viewport_height, bounds->x, bounds->y, runtime->scratch_point);
  (points.element(0.0) = runtime->scratch_point->x);
  (points.element(1.0) = runtime->scratch_point->y);
  flight::camera::project_camera_2d_point(runtime->camera.value(), runtime->viewport_width, runtime->viewport_height, (bounds->x + bounds->width), bounds->y, runtime->scratch_point);
  (points.element(2.0) = runtime->scratch_point->x);
  (points.element(3.0) = runtime->scratch_point->y);
  flight::camera::project_camera_2d_point(runtime->camera.value(), runtime->viewport_width, runtime->viewport_height, (bounds->x + bounds->width), (bounds->y + bounds->height), runtime->scratch_point);
  (points.element(4.0) = runtime->scratch_point->x);
  (points.element(5.0) = runtime->scratch_point->y);
  flight::camera::project_camera_2d_point(runtime->camera.value(), runtime->viewport_width, runtime->viewport_height, bounds->x, (bounds->y + bounds->height), runtime->scratch_point);
  (points.element(6.0) = runtime->scratch_point->x);
  (points.element(7.0) = runtime->scratch_point->y);
  flight::shape::clear_shape_commands(runtime->outline);
  append_gizmo_line_style(runtime->outline, 1.0, flight::bitwise_and(flight::unsigned_right_shift(runtime->outline_color, 8.0), 16777215.0), (flight::bitwise_and(runtime->outline_color, 255.0) / 255.0));
  append_gizmo_move_to(runtime->outline, points.element(0.0), points.element(1.0));
  append_gizmo_line_to(runtime->outline, points.element(2.0), points.element(3.0));
  append_gizmo_line_to(runtime->outline, points.element(4.0), points.element(5.0));
  append_gizmo_line_to(runtime->outline, points.element(6.0), points.element(7.0));
  append_gizmo_line_to(runtime->outline, points.element(0.0), points.element(1.0));
}

template <typename NodeType>
inline void update_gizmo_scale(flight::Ref<GizmoRuntime<NodeType>> runtime, flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<GizmoDrag>>>> drag, double screen_x, double screen_y, bool uniform) {
  const double cos = std::cos(flight::row_get<flight::RowKey<"screenRotation">>(drag));
  const double sin = std::sin(flight::row_get<flight::RowKey<"screenRotation">>(drag));
  const double start_x = (flight::row_get<flight::RowKey<"startScreenX">>(drag) - flight::row_get<flight::RowKey<"pivotScreenX">>(drag));
  const double start_y = (flight::row_get<flight::RowKey<"startScreenY">>(drag) - flight::row_get<flight::RowKey<"pivotScreenY">>(drag));
  const double current_x = (screen_x - flight::row_get<flight::RowKey<"pivotScreenX">>(drag));
  const double current_y = (screen_y - flight::row_get<flight::RowKey<"pivotScreenY">>(drag));
  const double start_local_x = ((start_x * cos) + (start_y * sin));
  const double start_local_y = ((-start_x * sin) + (start_y * cos));
  const double current_local_x = ((current_x * cos) + (current_y * sin));
  const double current_local_y = ((-current_x * sin) + (current_y * cos));
  double scale_x = 1.0;
  double scale_y = 1.0;
  if ((flight::row_get<flight::RowKey<"handle">>(drag) == flight::String("scale-x"))) {
    if ((std::abs(start_local_x) > std::numeric_limits<double>::epsilon())) {
      (scale_x = (current_local_x / start_local_x));
    }
    if (uniform) {
      (scale_y = scale_x);
    }
  }
  else {
    if ((flight::row_get<flight::RowKey<"handle">>(drag) == flight::String("scale-y"))) {
      if ((std::abs(start_local_y) > std::numeric_limits<double>::epsilon())) {
        (scale_y = (current_local_y / start_local_y));
      }
      if (uniform) {
        (scale_x = scale_y);
      }
    }
    else {
      const double start_length = std::hypot(start_local_x, start_local_y);
      const double current_length = std::hypot(current_local_x, current_local_y);
      const double uniform_2 = ((start_length > std::numeric_limits<double>::epsilon()) ? (current_length / start_length) : 1.0);
      (scale_x = uniform_2);
      (scale_y = uniform_2);
    }
  }
  flight::signals::emit_signal<std::function<void(double, double)>>(runtime->signals->on_scale, snap_gizmo_scale(scale_x, runtime->snap_scale), snap_gizmo_scale(scale_y, runtime->snap_scale));
}

template <typename NodeType>
inline void update_gizmo_translation(flight::Ref<GizmoRuntime<NodeType>> runtime, flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<GizmoDrag>>>> drag, double screen_x, double screen_y) {
  flight::camera::unproject_camera_2d_point(runtime->camera.value(), runtime->viewport_width, runtime->viewport_height, screen_x, screen_y, runtime->scratch_point);
  const double delta_x = (runtime->scratch_point->x - flight::row_get<flight::RowKey<"startWorldX">>(drag));
  const double delta_y = (runtime->scratch_point->y - flight::row_get<flight::RowKey<"startWorldY">>(drag));
  const double radians = (flight::row_get<flight::RowKey<"axisRotation">>(drag) * flight::math::deg_to_rad_flight_value_variable__u000044__u000045__u000047__u00005f__u000054__u00004f__u00005f__u000052__u000041__u000044__flight_source_ad5040a7d8c07bd3);
  const double cos = std::cos(radians);
  const double sin = std::sin(radians);
  double local_x = ((delta_x * cos) + (delta_y * sin));
  double local_y = ((-delta_x * sin) + (delta_y * cos));
  if ((flight::row_get<flight::RowKey<"handle">>(drag) == flight::String("translate-x"))) {
    (local_y = 0.0);
  }
  else {
    if ((flight::row_get<flight::RowKey<"handle">>(drag) == flight::String("translate-y"))) {
      (local_x = 0.0);
    }
  }
  if ((runtime->snap_translate != 0.0)) {
    const double pivot_local_x = ((flight::row_get<flight::RowKey<"pivotWorldX">>(drag) * cos) + (flight::row_get<flight::RowKey<"pivotWorldY">>(drag) * sin));
    const double pivot_local_y = ((-flight::row_get<flight::RowKey<"pivotWorldX">>(drag) * sin) + (flight::row_get<flight::RowKey<"pivotWorldY">>(drag) * cos));
    if ((flight::row_get<flight::RowKey<"handle">>(drag) != flight::String("translate-y"))) {
      (local_x = (flight::math::round_to((pivot_local_x + local_x), runtime->snap_translate) - pivot_local_x));
    }
    if ((flight::row_get<flight::RowKey<"handle">>(drag) != flight::String("translate-x"))) {
      (local_y = (flight::math::round_to((pivot_local_y + local_y), runtime->snap_translate) - pivot_local_y));
    }
  }
  flight::signals::emit_signal<std::function<void(double, double)>>(runtime->signals->on_translate, ((local_x * cos) - (local_y * sin)), ((local_x * sin) + (local_y * cos)));
}

template <typename NodeType>
inline void update_gizmo_transform(flight::Ref<GizmoRuntime<NodeType>> runtime, flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::PointerEventData>>>> data) {
  auto drag = runtime->drag;
  if ((!drag.has_value() || (drag.value()->pointer_id != flight::row_get<flight::RowKey<"pointerId">>(data)))) {
    return;
  }
  if ((drag.value()->mode == flight::String("translate"))) {
    update_gizmo_translation<NodeType>(runtime, flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<GizmoDrag>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<GizmoDrag>>>>(drag.value())), flight::row_get<flight::RowKey<"x">>(data), flight::row_get<flight::RowKey<"y">>(data));
  }
  else {
    if ((drag.value()->mode == flight::String("rotate"))) {
      const double start = std::atan2((drag.value()->start_screen_y - drag.value()->pivot_screen_y), (drag.value()->start_screen_x - drag.value()->pivot_screen_x));
      const double current = std::atan2((flight::row_get<flight::RowKey<"y">>(data) - drag.value()->pivot_screen_y), (flight::row_get<flight::RowKey<"x">>(data) - drag.value()->pivot_screen_x));
      const double degrees = normalize_degrees(((current - start) * flight::math::rad_to_deg_flight_value_variable__u000052__u000041__u000044__u00005f__u000054__u00004f__u00005f__u000044__u000045__u000047__flight_source_ad5040a7d8c07bd3));
      flight::signals::emit_signal<std::function<void(double)>>(runtime->signals->on_rotate, snap_gizmo_delta(degrees, runtime->snap_rotation));
    }
    else {
      update_gizmo_scale<NodeType>(runtime, flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<GizmoDrag>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<GizmoDrag>>>>(drag.value())), flight::row_get<flight::RowKey<"x">>(data), flight::row_get<flight::RowKey<"y">>(data), flight::row_get<flight::RowKey<"shiftKey">>(data));
    }
  }
}

template <typename NodeType>
inline void connect_gizmo_handle(flight::Ref<GizmoRuntime<NodeType>> runtime, flight::Ref<GizmoHandle> handle) {
  auto signals = enable_gizmo_interaction_signals(handle->node);
  GizmoPointerSlot on_pointer_down = GizmoPointerFunction(
      [=](flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::PointerEventData>>>> data) { return start_gizmo_transform<NodeType>(runtime, handle->kind, data); });
  GizmoPointerSlot on_pointer_move = GizmoPointerFunction(
      [=](flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::PointerEventData>>>> data) { return update_gizmo_transform<NodeType>(runtime, data); });
  GizmoPointerSlot on_pointer_end = GizmoPointerFunction(
      [=](flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::PointerEventData>>>> data) {
  if ((([&]() -> std::optional<double> { auto optional_chain_receiver = runtime->drag; if (!optional_chain_receiver.has_value()) return std::nullopt; return optional_chain_receiver.value()->pointer_id; }()) == flight::row_get<flight::RowKey<"pointerId">>(data))) {
    finish_gizmo_transform<NodeType>(runtime);
  }
});
  flight::signals::connect_signal<std::function<void(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::PointerEventData>>>>)>>(signals->on_pointer_down, on_pointer_down, std::nullopt);
  flight::signals::connect_signal<std::function<void(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::PointerEventData>>>>)>>(signals->on_pointer_move, on_pointer_move, std::nullopt);
  flight::signals::connect_signal<std::function<void(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::PointerEventData>>>>)>>(signals->on_pointer_up, on_pointer_end, std::nullopt);
  flight::signals::connect_signal<std::function<void(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::PointerEventData>>>>)>>(signals->on_pointer_cancel, on_pointer_end, std::nullopt);
  flight::signals::connect_signal<std::function<void(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::PointerEventData>>>>)>>(signals->on_release_outside, on_pointer_end, std::nullopt);
  runtime->cleanups.push([=]() {
  flight::signals::disconnect_signal<std::function<void(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::PointerEventData>>>>)>>(signals->on_pointer_down, on_pointer_down);
  flight::signals::disconnect_signal<std::function<void(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::PointerEventData>>>>)>>(signals->on_pointer_move, on_pointer_move);
  flight::signals::disconnect_signal<std::function<void(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::PointerEventData>>>>)>>(signals->on_pointer_up, on_pointer_end);
  flight::signals::disconnect_signal<std::function<void(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::PointerEventData>>>>)>>(signals->on_pointer_cancel, on_pointer_end);
  flight::signals::disconnect_signal<std::function<void(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::PointerEventData>>>>)>>(signals->on_release_outside, on_pointer_end);
});
}

template <typename NodeType>
inline void connect_gizmo_handles(flight::Ref<GizmoRuntime<NodeType>> runtime) {
  {
    double i = 0.0;
    while ((i < static_cast<double>(runtime->handles.size()))) {
      connect_gizmo_handle<NodeType>(runtime, runtime->handles.element(i));
      (i += 1.0);
    }
  }
}

inline const double arrow_size = 7.0;

inline const double center_color = 4367861.0;

inline const double handle_center_size = 7.0;

inline void draw_gizmo_axis(flight::Ref<flight::types::Shape> shape, double color, double end_x, double end_y) {
  append_gizmo_line_style(shape, 2.0, color, 1.0);
  append_gizmo_move_to(shape, ((end_x == 0.0) ? 0.0 : (flight::sign(end_x) * handle_center_size)), ((end_y == 0.0) ? 0.0 : (flight::sign(end_y) * handle_center_size)));
  append_gizmo_line_to(shape, end_x, end_y);
}

inline const double handle_length = 72.0;

inline const double hit_thickness = 9.0;

inline const double default_outline_color = 1149829119.0;

inline const double minimum_rotate_radius = 52.0;

inline const double rotate_padding = 20.0;

inline const double rotate_color = 16763432.0;

inline void draw_gizmo_rotate_handle(flight::Ref<flight::types::Shape> shape, double radius) {
  flight::shape::clear_shape_commands(shape);
  append_gizmo_line_style(shape, 3.0, rotate_color, 1.0);
  append_gizmo_circle(shape, 0.0, 0.0, radius);
  set_gizmo_shape_hit_area(
      shape, flight::geometry::create_rectangle(
                 -radius - 6.0, -radius - 6.0,
                 radius * 2.0 + 12.0, radius * 2.0 + 12.0));
}

template <typename NodeType>
inline void update_gizmo_handle_layout(flight::Ref<GizmoRuntime<NodeType>> runtime) {
  const double radians = (flight::row_get<flight::RowKey<"rotation">>(runtime->handle_root) * flight::math::deg_to_rad_flight_value_variable__u000044__u000045__u000047__u00005f__u000054__u00004f__u00005f__u000052__u000041__u000044__flight_source_ad5040a7d8c07bd3);
  const double cos = std::cos(radians);
  const double sin = std::sin(radians);
  double min_x = std::numeric_limits<double>::infinity();
  double min_y = std::numeric_limits<double>::infinity();
  double max_x = -std::numeric_limits<double>::infinity();
  double max_y = -std::numeric_limits<double>::infinity();
  {
    double i = 0.0;
    while ((i < static_cast<double>(runtime->outline_points.size()))) {
      {
        const double delta_x = (runtime->outline_points.element(i) - runtime->pivot_screen->x);
        const double delta_y = (runtime->outline_points.element((i + 1.0)) - runtime->pivot_screen->y);
        const double local_x = ((delta_x * cos) + (delta_y * sin));
        const double local_y = ((-delta_x * sin) + (delta_y * cos));
        (min_x = flight::minimum(min_x, local_x));
        (min_y = flight::minimum(min_y, local_y));
        (max_x = flight::maximum(max_x, local_x));
        (max_y = flight::maximum(max_y, local_y));
      }
      (i += 2.0);
    }
  }
  const double center_x = ((min_x + max_x) * 0.5);
  const double center_y = ((min_y + max_y) * 0.5);
  const double rotate_radius = flight::maximum(minimum_rotate_radius, (std::hypot(flight::maximum(std::abs(min_x), std::abs(max_x)), flight::maximum(std::abs(min_y), std::abs(max_y))) + rotate_padding));
  {
    double i_2 = 0.0;
    while ((i_2 < static_cast<double>(runtime->handles.size()))) {
      {
        auto handle = runtime->handles.element(i_2);
        if ((handle->placement == flight::String("rotate"))) {
          draw_gizmo_rotate_handle(handle->node, rotate_radius);
        }
        else {
          if ((handle->placement == flight::String("scale-east"))) {
            set_gizmo_node_transform(flight::structural_ref_cast<flight::types::Node2D>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Shape>>>>(handle->node)), max_x, center_y, 0.0);
          }
          else {
            if ((handle->placement == flight::String("scale-west"))) {
              set_gizmo_node_transform(flight::structural_ref_cast<flight::types::Node2D>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Shape>>>>(handle->node)), min_x, center_y, 0.0);
            }
            else {
              if ((handle->placement == flight::String("scale-north"))) {
                set_gizmo_node_transform(flight::structural_ref_cast<flight::types::Node2D>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Shape>>>>(handle->node)), center_x, min_y, 0.0);
              }
              else {
                if ((handle->placement == flight::String("scale-south"))) {
                  set_gizmo_node_transform(flight::structural_ref_cast<flight::types::Node2D>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Shape>>>>(handle->node)), center_x, max_y, 0.0);
                }
                else {
                  if ((handle->placement == flight::String("scale-northeast"))) {
                    set_gizmo_node_transform(flight::structural_ref_cast<flight::types::Node2D>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Shape>>>>(handle->node)), max_x, min_y, 0.0);
                  }
                  else {
                    if ((handle->placement == flight::String("scale-northwest"))) {
                      set_gizmo_node_transform(flight::structural_ref_cast<flight::types::Node2D>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Shape>>>>(handle->node)), min_x, min_y, 0.0);
                    }
                    else {
                      if ((handle->placement == flight::String("scale-southeast"))) {
                        set_gizmo_node_transform(flight::structural_ref_cast<flight::types::Node2D>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Shape>>>>(handle->node)), max_x, max_y, 0.0);
                      }
                      else {
                        if ((handle->placement == flight::String("scale-southwest"))) {
                          set_gizmo_node_transform(flight::structural_ref_cast<flight::types::Node2D>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Shape>>>>(handle->node)), min_x, max_y, 0.0);
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      (i_2 += 1.0);
    }
  }
}

template <typename NodeType>
inline void update_gizmo(flight::Ref<flight::types::GizmoState<NodeType>> state) {
  auto runtime = get_gizmo_runtime<NodeType>(flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::GizmoState<NodeType>>>>>(state)));
  if (runtime->disposed) {
    return;
  }
  auto selection = runtime->selection.value();
  flight::Array<NodeType> selected = flight::selection::get_selected_nodes<NodeType>(selection);
  if (!compute_gizmo_bounds<NodeType>(runtime, selected)) {
    finish_gizmo_transform<NodeType>(runtime);
    set_gizmo_node_visible(runtime->overlay_root, false);
    return;
  }
  set_gizmo_node_visible(runtime->overlay_root, true);
  resolve_gizmo_pivot<NodeType>(runtime);
  flight::camera::project_camera_2d_point(runtime->camera.value(), runtime->viewport_width, runtime->viewport_height, runtime->pivot_world->x, runtime->pivot_world->y, runtime->pivot_screen);
  set_gizmo_node_transform(runtime->handle_root, runtime->pivot_screen->x, runtime->pivot_screen->y, resolve_gizmo_screen_rotation<NodeType>(runtime));
  set_gizmo_node_visible(runtime->handle_root, (runtime->mode != flight::String("none")));
  update_gizmo_handle_visibility<NodeType>(runtime, runtime->mode);
  set_gizmo_node_visible(flight::structural_ref_cast<flight::types::Node2D>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::Shape>>>>(runtime->outline)), runtime->outline_enabled);
  update_gizmo_outline<NodeType>(runtime);
  update_gizmo_handle_layout<NodeType>(runtime);
}

inline const double scale_box_size = 11.0;

inline const double x_axis_color = 15684432.0;

inline const double y_axis_color = 6732650.0;

inline void draw_gizmo_handle(flight::Ref<flight::types::Shape> shape, flight::types::GizmoHandleKind kind) {
  if (kind == flight::String("rotate")) {
    draw_gizmo_rotate_handle(shape, minimum_rotate_radius);
    return;
  }
  if (kind == flight::String("translate-x")) {
    draw_gizmo_axis(shape, x_axis_color, handle_length, 0.0);
    append_gizmo_begin_fill(shape, x_axis_color);
    append_gizmo_polygon(shape, {handle_length, 0.0, handle_length - arrow_size,
                                 -arrow_size, handle_length - arrow_size, arrow_size});
    append_gizmo_end_fill(shape);
    set_gizmo_shape_hit_area(
        shape, flight::geometry::create_rectangle(
                   handle_center_size, -hit_thickness,
                   handle_length - handle_center_size, hit_thickness * 2.0));
    return;
  }
  if (kind == flight::String("translate-y")) {
    draw_gizmo_axis(shape, y_axis_color, 0.0, -handle_length);
    append_gizmo_begin_fill(shape, y_axis_color);
    append_gizmo_polygon(shape, {0.0, -handle_length, -arrow_size,
                                 -handle_length + arrow_size, arrow_size,
                                 -handle_length + arrow_size});
    append_gizmo_end_fill(shape);
    set_gizmo_shape_hit_area(
        shape, flight::geometry::create_rectangle(
                   -hit_thickness, -handle_length, hit_thickness * 2.0,
                   handle_length - handle_center_size));
    return;
  }
  if (kind == flight::String("translate-xy")) {
    append_gizmo_begin_fill(shape, center_color, 0.9);
    append_gizmo_rectangle(shape, -handle_center_size, -handle_center_size,
                           handle_center_size * 2.0, handle_center_size * 2.0);
    append_gizmo_end_fill(shape);
    set_gizmo_shape_hit_area(
        shape, flight::geometry::create_rectangle(
                   -handle_center_size, -handle_center_size,
                   handle_center_size * 2.0, handle_center_size * 2.0));
    return;
  }

  const double color = kind == flight::String("scale-y") ? y_axis_color :
                       kind == flight::String("scale-x") ? x_axis_color : center_color;
  const double alpha = kind == flight::String("scale-xy") ? 0.9 : 1.0;
  append_gizmo_begin_fill(shape, color, alpha);
  append_gizmo_rectangle(shape, -scale_box_size / 2.0, -scale_box_size / 2.0,
                         scale_box_size, scale_box_size);
  append_gizmo_end_fill(shape);
  set_gizmo_shape_hit_area(
      shape, flight::geometry::create_rectangle(
                 -hit_thickness, -hit_thickness,
                 hit_thickness * 2.0, hit_thickness * 2.0));
}

#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_GIZMO_ENTITY_RUNTIME_KEY_COMMANDS_9F7F305E70872542
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_GIZMO_ENTITY_RUNTIME_KEY_COMMANDS_9F7F305E70872542
struct entity_runtime_key_commands_9f7f305e70872542 : public flight::ReferenceEnabled {
  std::optional<flight::Ref<flight::types::EntityRuntime>> entity_runtime_key;
  std::optional<flight::Array<flight::types::ShapeCommandToken>> commands;
};
#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_GIZMO_ENTITY_RUNTIME_KEY_COMMANDS_9F7F305E70872542

#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_GIZMO_DATA_ENABLED_KIND_NAME_ENTITY_RUNTIME_KEY_ALPHA_VISIBLE_BLEND_MODE_CLIP_MATERIAL_MATERIAL_DATA_PIVOT_X_PIVOT_Y_ROTATION_SCALE_X_SCALE_Y_SKEW_X_SKEW_Y_X_Y_C68E4F1B5F29E4A7
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_GIZMO_DATA_ENABLED_KIND_NAME_ENTITY_RUNTIME_KEY_ALPHA_VISIBLE_BLEND_MODE_CLIP_MATERIAL_MATERIAL_DATA_PIVOT_X_PIVOT_Y_ROTATION_SCALE_X_SCALE_Y_SKEW_X_SKEW_Y_X_Y_C68E4F1B5F29E4A7
struct data_enabled_kind_name_entity_runtime_key_alpha_visible_blend_mode_clip_material_material_data_pivot_x_pivot_y_rotation_scale_x_scale_y_skew_x_skew_y_x_y_c68e4f1b5f29e4a7 : public flight::ReferenceEnabled {
  std::optional<flight::Ref<entity_runtime_key_commands_9f7f305e70872542>> data;
  std::optional<bool> enabled;
  std::optional<flight::String> kind;
  std::variant<flight::String, flight::Null, flight::Undefined> name = std::variant<flight::String, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>, flight::undefined};
  std::optional<std::shared_ptr<flight::types::NodeRuntime<flight::Ref<flight::types::Node2DTraits>>>> entity_runtime_key;
  std::optional<double> alpha;
  std::optional<bool> visible;
  std::variant<flight::String, flight::Null, flight::Undefined> blend_mode = std::variant<flight::String, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>, flight::undefined};
  std::variant<flight::Ref<flight::types::ClipRegion>, flight::Null, flight::Undefined> clip = std::variant<flight::Ref<flight::types::ClipRegion>, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>, flight::undefined};
  std::variant<flight::Ref<flight::types::Material2D>, flight::Null, flight::Undefined> material = std::variant<flight::Ref<flight::types::Material2D>, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>, flight::undefined};
  std::variant<flight::Ref<flight::types::MaterialData>, flight::Null, flight::Undefined> material_data = std::variant<flight::Ref<flight::types::MaterialData>, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>, flight::undefined};
  std::optional<double> pivot_x;
  std::optional<double> pivot_y;
  std::optional<double> rotation;
  std::optional<double> scale_x;
  std::optional<double> scale_y;
  std::optional<double> skew_x;
  std::optional<double> skew_y;
  std::optional<double> x;
  std::optional<double> y;
};
#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_GIZMO_DATA_ENABLED_KIND_NAME_ENTITY_RUNTIME_KEY_ALPHA_VISIBLE_BLEND_MODE_CLIP_MATERIAL_MATERIAL_DATA_PIVOT_X_PIVOT_Y_ROTATION_SCALE_X_SCALE_Y_SKEW_X_SKEW_Y_X_Y_C68E4F1B5F29E4A7

inline flight::Ref<GizmoHandle> create_gizmo_handle(flight::types::GizmoHandleKind kind, GizmoHandlePlacement placement) {
  auto node = flight::shape::create_shape();
  node->name = flight::String("Gizmo") + to_gizmo_handle_name(placement) +
               flight::String("Handle");
  flight::interaction::set_node_hit_test_enabled(node, true);
  flight::interaction::set_node_cursor(node, std::optional<flight::String>{get_gizmo_handle_cursor(kind)});
  draw_gizmo_handle(node, kind);
  return flight::make_ref<GizmoHandle>(GizmoHandle{.kind = kind, .node = node, .placement = placement});
}

inline flight::Array<flight::Ref<GizmoHandle>> create_gizmo_handles(flight::types::Node2D handle_root) {
  flight::Array<flight::Ref<GizmoHandle>> handles = flight::Array<flight::Ref<GizmoHandle>>{create_gizmo_handle(flight::String("rotate"), flight::String("rotate")), create_gizmo_handle(flight::String("translate-x"), flight::String("translate-x")), create_gizmo_handle(flight::String("translate-y"), flight::String("translate-y")), create_gizmo_handle(flight::String("translate-xy"), flight::String("translate-xy")), create_gizmo_handle(flight::String("scale-x"), flight::String("scale-east")), create_gizmo_handle(flight::String("scale-x"), flight::String("scale-west")), create_gizmo_handle(flight::String("scale-y"), flight::String("scale-north")), create_gizmo_handle(flight::String("scale-y"), flight::String("scale-south")), create_gizmo_handle(flight::String("scale-xy"), flight::String("scale-northeast")), create_gizmo_handle(flight::String("scale-xy"), flight::String("scale-northwest")), create_gizmo_handle(flight::String("scale-xy"), flight::String("scale-southeast")), create_gizmo_handle(flight::String("scale-xy"), flight::String("scale-southwest"))};
  {
    double i = 0.0;
    while ((i < static_cast<double>(handles.size()))) {
      add_gizmo_node_child(handle_root, gizmo_node_view(handles.element(i)->node));
      (i += 1.0);
    }
  }
  return handles;
}


// NOT GENERATED: function createGizmoState -- source line 124
// refusal: cpp-contextual-union-interface-heritage-carrier-unrepresented [target-runtime]
//
// The source it stood for:
//   export function createGizmoState<NodeType extends HierarchyNodeAny>(
//     options: Readonly<GizmoCreateOptions<NodeType>>,
//   ): GizmoState<NodeType> {
//     const state = {
//       [EntityRuntimeKey]: undefined,
//     } as GizmoState<NodeType>;
//     const overlayRoot = createDisplayObject({ name: 'GizmoRoot' });
//     const outline = createShape({ name: 'GizmoSelectionOutline' });
//     const handleRoot = createDisplayObject({ name: 'GizmoHandleRoot' });
//     const runtime: GizmoRuntime<NodeType> = {
//       binding: null,
//       bounds: createRectangle(),
//       camera: options.camera,
//       viewportHeight: options.viewportHeight,
//       viewportWidth: options.viewportWidth,
//       cleanups: [],
//       customPivotX: 0,
//       customPivotY: 0,
//       disposed: false,
//       drag: null,
//       features: options.features,
//       handleRoot,
//       handles: createGizmoHandles(handleRoot),
//       mode: 'translate',
//       nodeBounds: createRectangle(),
//       outline,
//       outlineColor: defaultOutlineColor,
//       outlineEnabled: true,
//       outlinePoints: new Array<number>(8).fill(Number.NaN),
//       overlay: options.overlayScene,
//       overlayRoot,
//       pivot: 'center',
//       pivotScreen: createVector2(),
//       pivotWorld: createVector2(),
//       snapRotation: 0,
//       snapScale: 0,
//       snapTranslate: 0,
//       space: 'world',
//       scratchPoint: createVector2(),
//       selection: options.selection,
//       signals: {
//         onRotate: createSignal(),
//         onScale: createSignal(),
//         onTransformBegin: createSignal(),
//         onTransformEnd: createSignal(),
//         onTranslate: createSignal(),
//       },
//     };
//     state[EntityRuntimeKey] = runtime;
//     addNodeChild(overlayRoot, outline);
//     addNodeChild(overlayRoot, handleRoot);
//     addNodeChild(options.overlayScene.root, overlayRoot);
//     connectGizmoHandles(runtime);
//     updateGizmo(state);
//     return state;
//   }
//
// cpp emission failed for @flighthq/gizmo/packages/gizmo/src/gizmoState.ts: contextual nullable interface
// conversion from GizmoRuntime to EntityRuntime follows TypeScript interface heritage, but flight-cpp stores
// those declarations in independent flight::Ref<GizmoRuntime<NodeType>> and
// flight::Ref<flight::types::EntityRuntime> owner types: each interface struct is emitted flattened, so the
// heritage is not a C++ base relation and no pointer cast exists to follow. A native pointer cast would claim
// C++ inheritance the emitted interface structs do not have, materializing the target would replace object
// identity, and a structural row would change the declared result carrier. Preserve the exact source owner in
// the declared result, or add an owner-preserving interface-heritage carrier to the runtime contract. Declaring
// the destination as the derived interface where the concrete type is known -- or converting where the type is
// still concrete rather than widening it through a nullable union -- avoids the boundary altogether; closing it
// at the carrier needs either that runtime carrier or interface structs emitted with the heritage as a C++ base
// relation.

template <typename NodeType>
inline flight::Ref<flight::types::GizmoState<NodeType>> create_gizmo_state(
    flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::GizmoCreateOptions<NodeType>>>>> options) {
  const auto exact_options = options.template shared_exact_object<
      flight::types::GizmoCreateOptions<NodeType>>();
  if (!exact_options) throw std::bad_cast();
  auto state = flight::make_ref<flight::types::GizmoState<NodeType>>();
  state->entity_runtime_key = std::nullopt;

  auto overlay_root = flight::scene2d::create_display_object();
  flight::row_set<flight::RowKey<"name">>(
      overlay_root, std::optional<flight::String>{flight::String("GizmoRoot")});
  auto outline = flight::shape::create_shape();
  outline->name = flight::String("GizmoSelectionOutline");
  auto handle_root = flight::scene2d::create_display_object();
  flight::row_set<flight::RowKey<"name">>(
      handle_root, std::optional<flight::String>{flight::String("GizmoHandleRoot")});

  auto runtime = flight::make_ref<GizmoRuntime<NodeType>>();
  runtime->binding = std::nullopt;
  runtime->uid = std::nullopt;
  runtime->bounds = flight::geometry::create_rectangle();
  runtime->camera = exact_options->camera;
  runtime->viewport_height = exact_options->viewport_height;
  runtime->viewport_width = exact_options->viewport_width;
  runtime->cleanups = {};
  runtime->custom_pivot_x = 0.0;
  runtime->custom_pivot_y = 0.0;
  runtime->disposed = false;
  runtime->drag = std::nullopt;
  runtime->features = exact_options->features;
  runtime->handle_root = handle_root;
  runtime->handles = create_gizmo_handles(handle_root);
  runtime->mode = flight::String("translate");
  runtime->node_bounds = flight::geometry::create_rectangle();
  runtime->outline = outline;
  runtime->outline_color = default_outline_color;
  runtime->outline_enabled = true;
  const double nan = std::numeric_limits<double>::quiet_NaN();
  runtime->outline_points = {nan, nan, nan, nan, nan, nan, nan, nan};
  runtime->overlay = exact_options->overlay_scene;
  runtime->overlay_root = overlay_root;
  runtime->pivot = flight::String("center");
  runtime->pivot_screen = flight::geometry::create_vector2();
  runtime->pivot_world = flight::geometry::create_vector2();
  runtime->snap_rotation = 0.0;
  runtime->snap_scale = 0.0;
  runtime->snap_translate = 0.0;
  runtime->space = flight::String("world");
  runtime->scratch_point = flight::geometry::create_vector2();
  runtime->selection = exact_options->selection;
  runtime->signals = flight::make_ref<flight::types::GizmoSignals>();
  runtime->signals->on_rotate =
      flight::signals::create_signal<std::function<void(double)>>();
  runtime->signals->on_scale =
      flight::signals::create_signal<std::function<void(double, double)>>();
  runtime->signals->on_transform_begin =
      flight::signals::create_signal<std::function<void()>>();
  runtime->signals->on_transform_end =
      flight::signals::create_signal<std::function<void()>>();
  runtime->signals->on_translate =
      flight::signals::create_signal<std::function<void(double, double)>>();

  state->entity_runtime_key =
      std::static_pointer_cast<flight::types::EntityRuntime>(runtime);
  add_gizmo_node_child(overlay_root, gizmo_node_view(outline));
  add_gizmo_node_child(overlay_root, handle_root);
  add_gizmo_node_child(exact_options->overlay_scene->root, overlay_root);
  connect_gizmo_handles<NodeType>(runtime);
  update_gizmo<NodeType>(state);
  return state;
}

} // namespace flight::gizmo
