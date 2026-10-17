// Faithful subset of the generated partial shape module. The flattened C++
// owners cannot represent ShapeRuntime or recover MorphShape from Shape, and
// there is no emitted owner for PartialNode<Shape>; those branches remain
// explicit override debt in overrides/manifest.json.
#pragma once

#include <optional>
#include <variant>
#include <flight/runtime.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/shape/shape_bounds.hpp>
#include <flight/types/shape.hpp>

namespace flight::shape {

inline flight::Ref<flight::types::ShapeData> create_shape_data() {
  return flight::make_ref<flight::types::ShapeData>(flight::types::ShapeData{
      .entity_runtime_key = std::nullopt,
      .commands = {},
  });
}

inline flight::Ref<flight::types::ShapeData> create_shape_data(std::nullopt_t) {
  return create_shape_data();
}

inline flight::Ref<flight::types::Shape> make_shape() {
  return flight::make_ref<flight::types::Shape>(flight::types::Shape{
      .data = create_shape_data(),
      .enabled = true,
      .kind = flight::types::shape_kind,
      .name = std::nullopt,
      .entity_runtime_key = flight::scene2d::create_node_2d_runtime(),
      .alpha = 1.0,
      .visible = true,
      .blend_mode = std::nullopt,
      .clip = std::nullopt,
      .material = std::nullopt,
      .material_data = std::nullopt,
      .pivot_x = 0.0,
      .pivot_y = 0.0,
      .rotation = 0.0,
      .scale_x = 1.0,
      .scale_y = 1.0,
      .skew_x = 0.0,
      .skew_y = 0.0,
      .x = 0.0,
      .y = 0.0,
  });
}

inline flight::Ref<flight::types::Shape> create_shape() {
  return make_shape();
}

inline flight::Ref<flight::types::Shape> create_shape(std::nullopt_t) {
  return make_shape();
}

inline void clear_shape_commands(const flight::Ref<flight::types::Shape>& shape) {
  shape->data->commands.clear();
  flight::node::invalidate_content(shape);
}

inline void copy_shape_commands(
    const flight::Ref<flight::types::Shape>& out,
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Shape>>>> source) {
  auto& commands = out->data->commands;
  const auto& source_commands = flight::row_get<flight::RowKey<"data">>(source)->commands;
  if (commands.identity() != source_commands.identity()) {
    commands.resize(source_commands.size());
    for (std::size_t index = 0; index < source_commands.size(); ++index) {
      commands[index] = source_commands[index];
    }
  }
  flight::node::invalidate_content(out);
}


// Keep the native reference entry point while sharing the emitted readonly source contract.
inline void copy_shape_commands(const flight::Ref<flight::types::Shape>& out,
                                const flight::Ref<flight::types::Shape>& source) {
  copy_shape_commands(out, flight::StructuralRef<flight::RowReadonly<
                               flight::RowOf<flight::Ref<flight::types::Shape>>>>{source});
}

inline double get_shape_command_count(const flight::Ref<flight::types::Shape>& source) {
  const auto& commands = source->data->commands;
  double count = 0.0;
  std::size_t index = 0;
  while (index + 1 < commands.size()) {
    const auto& argument_count = commands.element(static_cast<double>(index + 1));
    if (!argument_count.has_value() ||
        !std::holds_alternative<double>(argument_count.value())) break;
    const auto width = std::get<double>(argument_count.value());
    if (width < 0.0) break;
    index += static_cast<std::size_t>(width) + 2;
    count += 1.0;
  }
  return count;
}

inline bool is_shape_empty(const flight::Ref<flight::types::Shape>& source) {
  return source->data->commands.empty();
}

} // namespace flight::shape
