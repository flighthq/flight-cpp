#include <flight/scene3d/scene_node.hpp>

#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

struct NodeOptions : public flight::ReferenceEnabled {
  std::optional<double> alpha;
  std::optional<bool> enabled;
  std::variant<flight::String, flight::Null, flight::Undefined> name{
      std::in_place_type<flight::Undefined>, flight::undefined};
  std::optional<bool> visible;
};

} // namespace

int main() {
  namespace scene3d = flight::scene3d;
  namespace types = flight::types;

  const auto first = scene3d::create_node3_d();
  const auto second = scene3d::create_node3_d();
  check(flight::row_get<flight::RowKey<"kind">>(first) == types::node3_dkind,
        "Node3D construction retains the upstream default kind");
  check(flight::row_get<flight::RowKey<"enabled">>(first) &&
            flight::row_get<flight::RowKey<"visible">>(first) &&
            flight::row_get<flight::RowKey<"alpha">>(first) == 1.0,
        "Node3D construction applies the upstream appearance defaults");
  check(flight::row_get<flight::RowKey<"position">>(first)->x == 0.0 &&
            flight::row_get<flight::RowKey<"position">>(first)->y == 0.0 &&
            flight::row_get<flight::RowKey<"position">>(first)->z == 0.0 &&
            flight::row_get<flight::RowKey<"rotation">>(first)->w == 1.0 &&
            flight::row_get<flight::RowKey<"scale">>(first)->x == 1.0 &&
            flight::row_get<flight::RowKey<"scale">>(first)->y == 1.0 &&
            flight::row_get<flight::RowKey<"scale">>(first)->z == 1.0,
        "Node3D construction initializes identity transform storage");
  check(flight::row_get<flight::RowKey<"position">>(first) !=
            flight::row_get<flight::RowKey<"position">>(second),
        "each Node3D owns independent transform values");

  auto options = flight::make_ref<NodeOptions>();
  options->alpha = 0.25;
  options->enabled = false;
  options->name = flight::String("named");
  options->visible = false;
  const auto configured = scene3d::create_node3_d(flight::String("Configured"), options);
  check(flight::row_get<flight::RowKey<"kind">>(configured) ==
            flight::String("Configured") &&
            !flight::row_get<flight::RowKey<"enabled">>(configured) &&
            !flight::row_get<flight::RowKey<"visible">>(configured) &&
            flight::row_get<flight::RowKey<"alpha">>(configured) == 0.25 &&
            flight::row_get<flight::RowKey<"name">>(configured) ==
                std::optional<flight::String>{flight::String("named")},
        "Node3D construction forwards the complete declared option surface");

  const auto runtime = scene3d::get_node3_druntime(configured);
  check(runtime->traits == std::optional<flight::Symbol>{types::node3_dtraits_key},
        "Node3D runtime carries the upstream trait key");
  check(runtime->appearance_id == 0.0 && runtime->local_transform_id == 0.0 &&
            runtime->world_alpha_using_appearance_id == -1.0 &&
            runtime->world_alpha_using_parent_appearance_id == -1.0 &&
            !runtime->local_matrix4.has_value() &&
            !runtime->world_matrix4.has_value(),
        "Node3D runtime initializes base and 3D cache revisions");

  const auto signals = scene3d::enable_node3_dsignals(configured);
  check(signals != nullptr &&
            scene3d::get_node3_dsignals(configured) ==
                std::optional<flight::Ref<types::NodeSignals>>{signals} &&
            scene3d::enable_node3_dsignals(configured) == signals,
        "Node3D signals are allocated lazily and retain identity");

  return failures == 0 ? 0 : 1;
}
