#include <flight/scene2d/display_object.hpp>

#include <iostream>

// Mutation evidence (2026-10-13): focused assertions killed discarding the
// supplied alpha option and suppressing the clip appearance-revision increment.

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

} // namespace

int main() {
  using flight::RowKey;
  using flight::String;
  using flight::scene2d::Node2DOptions;

  const auto first = flight::scene2d::create_display_object();
  const auto second = flight::scene2d::create_display_object();
  const auto first_runtime = flight::scene2d::get_display_object_runtime(first);
  const auto second_runtime = flight::scene2d::get_display_object_runtime(second);
  if (!check(first.identity() != second.identity(),
             "display object construction reused node identity") ||
      !check(first_runtime && second_runtime && first_runtime != second_runtime,
             "display object construction reused or omitted runtime identity") ||
      !check(first_runtime->traits.has_value() &&
                 first_runtime->traits.value() == flight::types::node2_dtraits_key,
             "display object runtime lost the Node2D trait key") ||
      !check(first_runtime->appearance_id == 0.0 &&
                 first_runtime->local_transform_id == 0.0 &&
                 first_runtime->bounds_using_local_bounds_id == -1.0,
             "display object runtime defaults drifted") ||
      !check(flight::row_get<RowKey<"enabled">>(first) &&
                 flight::row_get<RowKey<"kind">>(first) ==
                     flight::types::display_object_kind &&
                 !flight::row_get<RowKey<"name">>(first).has_value() &&
                 flight::row_get<RowKey<"alpha">>(first) == 1.0 &&
                 flight::row_get<RowKey<"visible">>(first) &&
                 flight::row_get<RowKey<"scaleX">>(first) == 1.0 &&
                 flight::row_get<RowKey<"scaleY">>(first) == 1.0,
             "display object field defaults drifted")) {
    return 1;
  }

  using OptionsSchema = flight::RowReadonly<flight::RowPartial<
      flight::RowOf<flight::Ref<flight::types::Node2DTraits>>>>;
  const auto option_row = flight::make_structural_ref<OptionsSchema>(
      flight::row_field<RowKey<"alpha">>(2.0),
      flight::row_field<RowKey<"blendMode">>(String("darken")),
      flight::row_field<RowKey<"name">>(String("foo")),
      flight::row_field<RowKey<"rotation">>(45.0),
      flight::row_field<RowKey<"scaleX">>(2.0),
      flight::row_field<RowKey<"scaleY">>(3.0),
      flight::row_field<RowKey<"visible">>(false),
      flight::row_field<RowKey<"x">>(100.0),
      flight::row_field<RowKey<"y">>(200.0));
  const Node2DOptions options{option_row};
  const auto configured = flight::scene2d::create_display_object(options);
  if (!check(configured.identity() != option_row.identity(),
             "display object aliased its option row") ||
      !check(flight::row_get<RowKey<"alpha">>(configured) == 2.0 &&
                 flight::row_get<RowKey<"blendMode">>(configured) == String("darken") &&
                 flight::row_get<RowKey<"name">>(configured) == String("foo") &&
                 flight::row_get<RowKey<"rotation">>(configured) == 45.0 &&
                 flight::row_get<RowKey<"scaleX">>(configured) == 2.0 &&
                 flight::row_get<RowKey<"scaleY">>(configured) == 3.0 &&
                 !flight::row_get<RowKey<"visible">>(configured) &&
                 flight::row_get<RowKey<"x">>(configured) == 100.0 &&
                 flight::row_get<RowKey<"y">>(configured) == 200.0,
             "display object discarded predefined values")) {
    return 1;
  }

  const auto clip = flight::make_ref<flight::types::ClipRegion>();
  const double appearance_before = first_runtime->appearance_id;
  flight::scene2d::set_node2_dclip(first, clip);
  const auto stored_clip = flight::row_get<RowKey<"clip">>(first);
  if (!check(stored_clip.has_value() && stored_clip.value() == clip,
             "clip assignment lost the supplied reference") ||
      !check(first_runtime->appearance_id == appearance_before + 1.0,
             "clip assignment did not invalidate appearance exactly once")) {
    return 1;
  }
  flight::scene2d::set_node2_dclip(first, std::nullopt);
  if (!check(!flight::row_get<RowKey<"clip">>(first).has_value(),
             "clip clear did not restore absence") ||
      !check(first_runtime->appearance_id == appearance_before + 2.0,
             "clip clear did not invalidate appearance exactly once")) {
    return 1;
  }

  const auto any_runtime = flight::node::create_node_runtime<flight::Any>();
  any_runtime->traits = flight::types::node2_dtraits_key;
  const flight::types::NodeAny any_node =
      flight::make_ref<flight::types::Node<flight::Any>>(
          flight::types::Node<flight::Any>{
              .enabled = true,
              .kind = flight::types::display_object_kind,
              .entity_runtime_key = any_runtime,
          });
  if (!check(flight::scene2d::is_node2_d(any_node),
             "NodeAny kind test rejected the Node2D trait key") ||
      !check(flight::scene2d::is_node2_d(first),
             "constructed display object was not recognized as Node2D")) {
    return 1;
  }

  bool methods_failed_loudly = false;
  bool factories_failed_loudly = false;
  try {
    static_cast<void>(flight::scene2d::create_node2_d_runtime(1));
  } catch (const flight::Error& error) {
    methods_failed_loudly =
        error.message() == String(
            "createNode2DRuntime methods require the unavailable exact Node2DRuntime owner");
  }
  try {
    static_cast<void>(flight::scene2d::create_node2_d<
        flight::scene2d::Node2DRuntimeOwner>(
        flight::types::display_object_kind, Node2DOptions{}, nullptr, nullptr));
  } catch (const flight::Error& error) {
    factories_failed_loudly =
        error.message() == String(
            "createNode2D custom data/runtime factories require the unavailable exact Node2DRuntime owner");
  }
  if (!check(methods_failed_loudly && factories_failed_loudly,
             "unavailable exact-runtime paths did not fail at their boundary")) {
    return 1;
  }

  return 0;
}
