// Native parity evidence for the callable @flighthq/scene3d-resources surface at Flight a627849.
// The package remains partial: loader coroutines, resolver construction/streaming, resource-to-
// reference lookup, material registration, and guard installation are refused. These tests exercise
// only emitted behavior, using exact resource owners rather than substitute loaders or registries.
#include <flight/scene3d_resources/enable_scene3_dresource_failure_guards.hpp>
#include <flight/scene3d_resources/get_scene3_dresource_textures.hpp>
#include <flight/scene3d_resources/image_resource_fetch.hpp>
#include <flight/scene3d_resources/scene_document_source.hpp>
#include <flight/scene3d_resources/scene_resource_signals.hpp>

#include <functional>
#include <iostream>
#include <optional>
#include <variant>

namespace {

namespace resources = flight::scene3d_resources;
namespace types = flight::types;

int failures = 0;
bool inject_uri_mutation = false;
bool inject_base_path_mutation = false;
bool inject_resource_identity_mutation = false;
bool inject_signal_lifecycle_mutation = false;
bool inject_guard_disable_mutation = false;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Pointer>
auto readonly_view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

void image_uri_resolution() {
  bool exact =
      resources::resolve_image_resource_uri(flight::String("leaf.png"),
                                            flight::String("assets/tex")) ==
          flight::String("assets/tex/leaf.png") &&
      resources::resolve_image_resource_uri(flight::String("leaf.png"),
                                            flight::String("assets/tex/")) ==
          flight::String("assets/tex/leaf.png") &&
      resources::resolve_image_resource_uri(
          flight::String("https://cdn.test/leaf.png"),
          flight::String("assets/tex")) ==
          flight::String("https://cdn.test/leaf.png") &&
      resources::resolve_image_resource_uri(
          flight::String("data:image/png;base64,AAAA"),
          flight::String("assets/tex")) ==
          flight::String("data:image/png;base64,AAAA") &&
      resources::resolve_image_resource_uri(flight::String("/leaf.png"),
                                            flight::String("assets/tex")) ==
          flight::String("/leaf.png") &&
      resources::resolve_image_resource_uri(flight::String("leaf.png"),
                                            std::nullopt) ==
          flight::String("leaf.png") &&
      resources::resolve_image_resource_uri(flight::String("asset+zip:x"),
                                            flight::String("ignored")) ==
          flight::String("asset+zip:x");
  if (inject_uri_mutation) exact = false;
  check(exact,
        "image URI resolution joins one separator, preserves root/scheme/data absolutes, accepts custom schemes, and leaves relative paths unchanged without a base");
}

void document_base_path_parsing() {
  const auto nested = resources::get_scene3_ddocument_base_path_from_url(
      flight::String("models/ships/main.gltf?cache=1#ignored"));
  const auto absolute = resources::get_scene3_ddocument_base_path_from_url(
      flight::String("https://cdn.test/models/main.glb#node"));
  const auto bare = resources::get_scene3_ddocument_base_path_from_url(
      flight::String("main.obj?cache=1"));
  const auto root = resources::get_scene3_ddocument_base_path_from_url(
      flight::String("/main.3ds"));
  bool exact = nested == std::optional<flight::String>{"models/ships"} &&
               absolute ==
                   std::optional<flight::String>{"https://cdn.test/models"} &&
               !bare.has_value() &&
               root == std::optional<flight::String>{flight::String()};
  if (inject_base_path_mutation) exact = false;
  check(exact,
        "document base paths discard query/fragment data, retain nested and absolute directories, distinguish a bare filename, and preserve the root directory as an empty path");
}

types::Texture texture_2d(const flight::Ref<types::Texture2D>& value) {
  return types::Texture{std::in_place_type<flight::Ref<types::Texture2D>>,
                        value};
}

types::Texture texture_cube(const flight::Ref<types::TextureCube>& value) {
  return types::Texture{std::in_place_type<flight::Ref<types::TextureCube>>,
                        value};
}

types::Texture texture_3d(const flight::Ref<types::Texture3D>& value) {
  return types::Texture{std::in_place_type<flight::Ref<types::Texture3D>>,
                        value};
}

void resource_sidecar_identity_order_and_optionality() {
  const auto first = flight::make_ref<types::Texture2D>();
  const auto second = flight::make_ref<types::TextureCube>();
  const auto stale = flight::make_ref<types::Texture3D>();

  const auto external = flight::make_ref<types::ExternalImageResourceReference>();
  external->textures = flight::Array<types::Texture>{texture_2d(first),
                                                     texture_cube(second)};
  const auto embedded = flight::make_ref<types::EmbeddedImageResourceReference>();
  embedded->textures = flight::Array<types::Texture>{texture_2d(first)};
  const auto absent = flight::make_ref<types::ExternalImageResourceReference>();
  absent->textures = std::nullopt;

  const auto scene = flight::make_ref<types::Scene3D>();
  scene->resources = flight::Array<types::ImageResourceReference>{
      types::ImageResourceReference{
          std::in_place_type<flight::Ref<types::ExternalImageResourceReference>>,
          external},
      types::ImageResourceReference{
          std::in_place_type<flight::Ref<types::EmbeddedImageResourceReference>>,
          embedded},
      types::ImageResourceReference{
          std::in_place_type<flight::Ref<types::ExternalImageResourceReference>>,
          absent},
  };
  flight::Array<types::Texture> out{texture_3d(stale)};
  const auto external_owner = external;
  const auto embedded_owner = embedded;
  resources::get_scene3_dresource_textures(out, readonly_view(scene));

  bool exact = out.size() == 2 &&
               std::get<flight::Ref<types::Texture2D>>(out.element(0.0)) ==
                   first &&
               std::get<flight::Ref<types::TextureCube>>(out.element(1.0)) ==
                   second &&
               scene->resources.size() == 3 && external == external_owner &&
               embedded == embedded_owner && external->textures->size() == 2 &&
               embedded->textures->size() == 1;

  scene->resources = {};
  resources::get_scene3_dresource_textures(out, readonly_view(scene));
  exact = exact && out.empty();
  if (inject_resource_identity_mutation) exact = false;
  check(exact,
        "resource sidecar discovery clears prior output, visits embedded/external references in authored order, skips absent texture lists, deduplicates exact shared texture owners, preserves inputs, and clears for an empty scene");
}

using EventView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<types::Scene3DResourceEvent>>>>;
using EventSlot = std::function<void(EventView)>;

void resource_signal_identity_and_lifecycle() {
  const auto runtime = flight::make_ref<types::Scene3DResourceResolverRuntime>();
  const auto resolver =
      flight::make_ref<types::Scene3DResourceResolverWithRuntime>();
  resolver->scene3_dresource_resolver_runtime_key = runtime;

  bool exact =
      !resources::get_scene3_dresource_signals(readonly_view(resolver))
           .has_value();
  const auto first = resources::enable_scene3_dresource_signals(resolver);
  const auto second = resources::enable_scene3_dresource_signals(resolver);
  const auto stored =
      resources::get_scene3_dresource_signals(readonly_view(resolver));
  exact = exact && first == second && stored.has_value() && *stored == first &&
          !first->entity_runtime_key.has_value() &&
          first->on_resource_failed != first->on_resource_resolved;

  int received = 0;
  EventSlot slot = [&](EventView event) {
    if (event.shared_object()) ++received;
  };
  flight::signals::connect_signal<EventSlot>(first->on_resource_resolved,
                                             slot);
  const auto event = flight::make_ref<types::Scene3DResourceEvent>();
  first->on_resource_resolved->emit(readonly_view(event));
  first->on_resource_failed->emit(readonly_view(event));
  exact = exact && received == 1 &&
          first->on_resource_resolved->data.has_value();
  flight::signals::clear_signal<EventSlot>(first->on_resource_resolved);
  first->on_resource_resolved->emit(readonly_view(event));
  exact = exact && received == 1 &&
          !first->on_resource_resolved->data.has_value() &&
          resources::get_scene3_dresource_signals(readonly_view(resolver)) ==
              stored;

  const auto independent = resources::create_scene3_dresource_signals();
  exact = exact && independent != first &&
          independent->on_resource_resolved != first->on_resource_resolved &&
          independent->on_resource_failed != first->on_resource_failed;
  if (inject_signal_lifecycle_mutation) exact = false;
  check(exact,
        "resource signals start absent, enable idempotently with exact stored identity and no incidental entity runtime, dispatch only their own event channel, clear without replacing the group, and independently created groups own independent signals");
}

void absent_failure_guard_disable() {
  const auto runtime = flight::make_ref<types::Scene3DResourceResolverRuntime>();
  const auto resolver =
      flight::make_ref<types::Scene3DResourceResolverWithRuntime>();
  resolver->scene3_dresource_resolver_runtime_key = runtime;
  bool exact =
      !resources::are_scene3_dresource_failure_guards_enabled(
          readonly_view(resolver)) &&
      !runtime->signals.has_value();
  resources::disable_scene3_dresource_failure_guards(resolver);
  exact = exact &&
          !resources::are_scene3_dresource_failure_guards_enabled(
              readonly_view(resolver)) &&
          !runtime->signals.has_value();
  if (inject_guard_disable_mutation) exact = false;
  check(exact,
        "disabling an absent failure guard is an identity-preserving no-op and does not allocate resource signals as a side effect");
}

}  // namespace

int main(int argc, char** argv) {
  const flight::String mutation =
      argc > 1 ? flight::String(argv[1]) : flight::String();
  inject_uri_mutation = mutation == flight::String("--mutate-uri-resolution");
  inject_base_path_mutation = mutation == flight::String("--mutate-base-path");
  inject_resource_identity_mutation =
      mutation == flight::String("--mutate-resource-identity");
  inject_signal_lifecycle_mutation =
      mutation == flight::String("--mutate-signal-lifecycle");
  inject_guard_disable_mutation =
      mutation == flight::String("--mutate-guard-disable");

  image_uri_resolution();
  document_base_path_parsing();
  resource_sidecar_identity_order_and_optionality();
  resource_signal_identity_and_lifecycle();
  absent_failure_guard_disable();
  if (failures != 0) {
    std::cerr << failures << " scene3d-resources assertion(s) failed\n";
    return 1;
  }
  std::cout << "scene3d-resources callable native evidence passed\n";
  return 0;
}
