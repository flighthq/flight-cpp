#include <flight/render/explain_scene2_dcoverage.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

namespace {

using flight::Array;
using flight::Ref;
using flight::String;
using flight::types::CatalogEntry;
using flight::types::CatalogRegistration;
using flight::types::RenderRegistryTable;
using flight::types::SceneCoverageEntry;

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

template <typename Entry>
std::shared_ptr<Entry> arm(const SceneCoverageEntry& entry, const char* message) {
  const auto* selected = std::get_if<Ref<Entry>>(&entry);
  if (!check(selected != nullptr && static_cast<bool>(*selected), message)) return {};
  return *selected;
}

template <typename Entry>
bool check_base(const std::shared_ptr<Entry>& entry, const String& facet,
                const String& kind,
                RenderRegistryTable registry, const String& coverage,
                const char* message) {
  return check(entry && entry->facet == facet && entry->kind == kind &&
                   entry->registry == registry && entry->coverage == coverage,
               message);
}

Ref<CatalogEntry> catalog_entry(const String& kind, RenderRegistryTable registry,
                                const String& module, const String& registrar) {
  auto registration = flight::make_ref<CatalogRegistration>(CatalogRegistration{
      .module = module,
      .registrar = registrar,
  });
  return flight::make_ref<CatalogEntry>(CatalogEntry{
      .kind = kind,
      .registrations = Array<Ref<CatalogRegistration>>{registration},
      .registry = registry,
  });
}

using UsageView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<Ref<flight::types::Scene2DKindUsage>>>>;

UsageView usage(Array<String> node_kinds, Array<String> shape_command_keys) {
  auto value = flight::make_ref<flight::types::Scene2DKindUsage>(
      flight::types::Scene2DKindUsage{
          .blend_modes = {},
          .material_kinds = {},
          .node_kinds = std::move(node_kinds),
          .shape_command_keys = std::move(shape_command_keys),
      });
  return UsageView{value};
}

Ref<flight::types::RenderState> state_with(
    Array<String> node_kinds, std::optional<Array<String>> shape_command_keys) {
  auto registries = flight::make_ref<flight::types::RenderRegistries>();
  for (const auto& kind : node_kinds) {
    registries->node_renderers.set(kind, Ref<flight::types::NodeRenderer>{});
  }
  if (shape_command_keys.has_value()) {
    flight::Map<String,
                Ref<flight::types::CanvasShapeCommand<flight::types::ShapeCommandKey>>>
        commands;
    for (const auto& kind : shape_command_keys.value()) {
      commands.set(kind, {});
    }
    registries->canvas_shape_commands = commands;
  }

  auto runtime = flight::make_ref<flight::types::RenderStateRuntime>();
  runtime->registries = registries;
  auto state = flight::make_ref<flight::types::RenderState>();
  state->entity_runtime_key = runtime;
  return state;
}

bool check_shortfall_arms() {
  const auto facet = flight::types::requirement_facet->scene_node_kind;
  constexpr auto registry = RenderRegistryTable::NodeRenderer;

  auto unavailable = arm<flight::types::UnavailableSceneCoverageEntry>(
      flight::render::create_shortfall_entry(std::nullopt, false, facet,
                                             String("missing"), registry),
      "ordinary catalog miss did not select unavailable entry");
  if (!check_base(unavailable, facet, String("missing"), registry,
                  flight::types::scene_coverage->unavailable,
                  "unavailable entry lost its base fields")) {
    return false;
  }

  auto fallback_unavailable =
      arm<flight::types::FallbackUnavailableSceneCoverageEntry>(
          flight::render::create_shortfall_entry(
              std::nullopt, true, facet, String("missing"), registry),
          "fallback catalog miss did not select fallback-unavailable entry");
  if (!check_base(fallback_unavailable, facet, String("missing"), registry,
                  flight::types::scene_coverage->fallback_unavailable,
                  "fallback-unavailable entry lost its base fields")) {
    return false;
  }

  Array<Ref<CatalogEntry>> catalog{
      catalog_entry(String("other"), registry, String("ignored"),
                    String("ignoredRegistrar")),
      catalog_entry(String("registered"), registry, String("render-module"),
                    String("registerRenderer")),
      catalog_entry(String("registered"), registry, String("later-module"),
                    String("laterRegistrar")),
  };
  auto unregistered = arm<flight::types::UnregisteredSceneCoverageEntry>(
      flight::render::create_shortfall_entry(
          std::optional<Array<Ref<CatalogEntry>>>{catalog}, false, facet,
          String("registered"), registry),
      "catalog match did not select unregistered entry");
  if (!check_base(unregistered, facet, String("registered"), registry,
                  flight::types::scene_coverage->unregistered,
                  "unregistered entry lost its base fields") ||
      !check(unregistered->module == String("render-module") &&
                 unregistered->registrar == String("registerRenderer"),
             "unregistered entry did not retain the first catalog remedy")) {
    return false;
  }

  auto fallback_remediable =
      arm<flight::types::FallbackRemediableSceneCoverageEntry>(
          flight::render::create_shortfall_entry(
              std::optional<Array<Ref<CatalogEntry>>>{catalog}, true, facet,
              String("registered"), registry),
          "fallback catalog match did not select fallback-remediable entry");
  return check_base(fallback_remediable, facet, String("registered"), registry,
                    flight::types::scene_coverage->fallback_remediable,
                    "fallback-remediable entry lost its base fields") &&
         check(fallback_remediable->module == String("render-module") &&
                   fallback_remediable->registrar == String("registerRenderer"),
               "fallback-remediable entry lost its catalog remedy");
}

bool check_satisfied_registry_paths() {
  auto state = state_with(Array<String>{String("sprite")},
                          Array<String>{String("moveTo")});
  Array<SceneCoverageEntry> out;
  const bool found = flight::render::collect_scene2_dcoverage_gaps(
      std::optional<Array<SceneCoverageEntry>>{out}, state,
      usage(Array<String>{String("sprite")}, Array<String>{String("moveTo")}),
      false, std::nullopt);
  if (!check(!found && out.size() == 2,
             "present renderer and command did not yield two satisfied entries")) {
    return false;
  }

  auto node = arm<flight::types::SatisfiedSceneCoverageEntry>(
      out[0], "node renderer presence did not select satisfied entry");
  auto command = arm<flight::types::SatisfiedSceneCoverageEntry>(
      out[1], "shape command presence did not select satisfied entry");
  return check_base(node, flight::types::requirement_facet->scene_node_kind,
                    String("sprite"), RenderRegistryTable::NodeRenderer,
                    flight::types::scene_coverage->satisfied,
                    "node satisfied entry lost its fields") &&
         check_base(command,
                    flight::types::requirement_facet->scene_shape_command,
                    String("moveTo"), RenderRegistryTable::ShapeCommandHandler,
                    flight::types::scene_coverage->satisfied,
                    "shape-command satisfied entry lost its fields") &&
         check(flight::render::has_scene2_dcoverage(
                   state, usage(Array<String>{String("sprite")},
                                Array<String>{String("moveTo")})),
               "has-coverage rejected present renderer and command");
}

bool check_order_clear_and_early_exit() {
  auto state = state_with(Array<String>{String("sprite")}, std::nullopt);
  Array<SceneCoverageEntry> out{
      flight::render::create_shortfall_entry(
          std::nullopt, false, flight::types::requirement_facet->scene_node_kind,
          String("stale"), RenderRegistryTable::NodeRenderer),
  };
  Array<Ref<CatalogEntry>> catalog{catalog_entry(
      String("mesh"), RenderRegistryTable::NodeRenderer,
      String("mesh-module"), String("registerMesh"))};
  flight::render::explain_scene2_dcoverage(
      out, state,
      usage(Array<String>{String("sprite"), String("mesh")},
            Array<String>{String("lineTo")}),
      catalog);
  if (!check(out.size() == 3,
             "explanation did not clear stale output before ordered writes")) {
    return false;
  }
  auto satisfied = arm<flight::types::SatisfiedSceneCoverageEntry>(
      out[0], "ordered explanation did not begin with present node kind");
  auto remediable = arm<flight::types::UnregisteredSceneCoverageEntry>(
      out[1], "ordered explanation did not retain catalog-remediable node gap");
  auto unavailable = arm<flight::types::UnavailableSceneCoverageEntry>(
      out[2], "ordered explanation did not finish with unavailable command gap");
  if (!check_base(satisfied,
                  flight::types::requirement_facet->scene_node_kind,
                  String("sprite"), RenderRegistryTable::NodeRenderer,
                  flight::types::scene_coverage->satisfied,
                  "ordered satisfied node entry lost its fields") ||
      !check_base(remediable,
                  flight::types::requirement_facet->scene_node_kind,
                  String("mesh"), RenderRegistryTable::NodeRenderer,
                  flight::types::scene_coverage->unregistered,
                  "ordered remediable node entry lost its fields") ||
      !check(remediable->module == String("mesh-module") &&
                 remediable->registrar == String("registerMesh"),
             "ordered remediable node entry lost its remedy") ||
      !check_base(unavailable,
                  flight::types::requirement_facet->scene_shape_command,
                  String("lineTo"), RenderRegistryTable::ShapeCommandHandler,
                  flight::types::scene_coverage->unavailable,
                  "ordered unavailable command entry lost its fields")) {
    return false;
  }

  Array<SceneCoverageEntry> untouched;
  const bool stopped = flight::render::collect_scene2_dcoverage_gaps(
      std::optional<Array<SceneCoverageEntry>>{untouched}, state,
      usage(Array<String>{String("missing"), String("sprite")},
            Array<String>{String("lineTo")}),
      true, std::nullopt);
  return check(stopped && untouched.empty(),
               "stop-at-first wrote a gap or failed to stop") &&
         check(!flight::render::has_scene2_dcoverage(
                   state, usage(Array<String>{String("missing")}, {})),
               "has-coverage accepted a missing node renderer") &&
         check(!flight::render::has_scene2_dcoverage(
                   state, usage({}, Array<String>{String("lineTo")})),
               "has-coverage accepted an absent shape-command map");
}

}  // namespace

int main() {
  if (!check_shortfall_arms()) return 1;
  if (!check_satisfied_registry_paths()) return 1;
  if (!check_order_clear_and_early_exit()) return 1;
  return 0;
}
