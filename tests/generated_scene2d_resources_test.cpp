// Focused native parity for the independently usable @flighthq/scene2d-resources surface.
// The image/audio orchestration bodies remain compiler-refused; the default fetch sentinels they
// expose are still executable and pinned here alongside document loading, lookup, and lifecycle.
// Five measured one-at-a-time mutations produced 4/2/1/1/1 failures for append-instead-of-replace,
// skipped texture fan-out, omitted required-slot completeness, a corrupted diagnostic reason, and a
// zero-length unregister splice, respectively.
#include <flight/scene2d_resources/load_scene_2d_audio_resources.hpp>
#include <flight/scene2d_resources/load_scene_2d_image_resources.hpp>
#include <flight/scene2d_resources/scene_2d_document.hpp>
#include <flight/scene2d_resources/scene_2d_document_importer_registry.hpp>
#include <flight/scene2d_resources/scene_2d_document_source.hpp>
#include <flight/scene2d_resources/scene_2d_resource_diagnostics.hpp>

#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace resources = flight::scene2d_resources;
namespace types = flight::types;

using NodeTraits = flight::Ref<types::Node2DTraits>;
using Node = types::Node2D;
using ImportContextView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<types::Scene2DDocumentImportContext>>>>;
using RegistryView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<types::Scene2DDocumentImporterRegistry>>>>;
using FailureView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<types::Scene2DResourceFailureNotice>>>>;
using ExternalAudioView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<types::ExternalAudioResourceReference>>>>;
using ExternalImageView = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<types::ExternalImageResourceReference>>>>;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

Node create_test_node(const char* name) {
  using NativeNode = types::Node<NodeTraits>;
  using NativeView = flight::StructuralRef<
      flight::RowWritable<flight::RowOf<std::shared_ptr<NativeNode>>>>;
  auto native = std::make_shared<NativeNode>();
  native->data = std::nullopt;
  native->enabled = true;
  native->kind = types::display_object_kind;
  native->name = flight::String(name);
  native->entity_runtime_key = std::nullopt;
  return Node::from_owner(NativeView(native).shared_owner());
}

std::optional<std::optional<flight::String>> nullable_string(
    std::optional<flight::String> value) {
  return std::optional<std::optional<flight::String>>{std::move(value)};
}

std::optional<std::optional<double>> nullable_number(std::optional<double> value) {
  return std::optional<std::optional<double>>{value};
}

flight::Ref<types::ExternalImageResourceReference> image_reference(
    const flight::String& state) {
  auto value = flight::make_ref<types::ExternalImageResourceReference>();
  value->state = state;
  value->kind = types::image_resource_reference_kind->external;
  value->uri = flight::String("image.png");
  return value;
}

flight::Ref<types::ExternalAudioResourceReference> audio_reference(
    const flight::String& state) {
  auto value = flight::make_ref<types::ExternalAudioResourceReference>();
  value->state = state;
  value->kind = types::audio_resource_reference_kind->external;
  value->uri = flight::String("audio.mp3");
  return value;
}

void document_and_slot_construction() {
  const auto root = create_test_node("root");
  const auto target = create_test_node("before-slot-name");
  const auto slot = resources::create_scene_2d_slot_reference(
      flight::String("avatar"), target,
      nullable_string(flight::String("Game.Avatar")), false);
  flight::Array<flight::Ref<types::Scene2DSlotReference>> slots{slot};

  const auto image = image_reference(types::resource_resolution_state->resolved);
  const auto audio = audio_reference(types::resource_resolution_state->loading);
  flight::Array<types::ImageResourceReference> images{
      types::ImageResourceReference{std::in_place_type<flight::Ref<types::ExternalImageResourceReference>>, image}};
  flight::Array<types::AudioResourceReference> audio_values{
      types::AudioResourceReference{std::in_place_type<flight::Ref<types::ExternalAudioResourceReference>>, audio}};
  const auto document = resources::create_scene_2d_document(
      root, slots, nullable_string(flight::String("acme")),
      nullable_number(std::nullopt), images, audio_values);

  check(document->root == root && document->slots.identity() == slots.identity() &&
            document->image_resources.identity() == images.identity() &&
            document->audio_resources.identity() == audio_values.identity(),
        "document construction retains root and manifest array identity");
  check(document->source_kind ==
                std::optional<flight::String>{flight::String("acme")} &&
            !document->background_color.has_value(),
        "document construction preserves source kind and explicit null background");
  check(slot->target == target && slot->name == flight::String("avatar") &&
            slot->linkage ==
                std::optional<flight::String>{flight::String("Game.Avatar")} &&
            !slot->required && !slot->content.has_value() &&
            flight::row_get<flight::RowKey<"name">>(target) ==
                std::optional<flight::String>{flight::String("avatar")} &&
            !slot->entity_runtime_key.has_value(),
        "slot construction retains linkage, target identity, defaults, and entity-key absence");

  const auto empty_a = resources::create_scene_2d_document(create_test_node("empty-a"));
  const auto empty_b = resources::create_scene_2d_document(create_test_node("empty-b"));
  check(empty_a->slots.empty() && empty_a->image_resources.empty() &&
            empty_a->audio_resources.empty(),
        "document construction defaults each manifest collection to empty");
  check(empty_a->slots.identity() != empty_b->slots.identity() &&
            empty_a->image_resources.identity() != empty_b->image_resources.identity() &&
            empty_a->audio_resources.identity() != empty_b->audio_resources.identity(),
        "document defaults allocate independent manifest collections");
}

void importer_registry_and_document_loading() {
  const auto first_registry = resources::create_scene_2d_document_importer_registry();
  const auto isolated_registry = resources::create_scene_2d_document_importer_registry();
  check(first_registry->entries.empty() && isolated_registry->entries.empty() &&
            first_registry->entries.identity() != isolated_registry->entries.identity(),
        "importer registries start empty and isolated");

  int first_match_calls = 0;
  int second_match_calls = 0;
  const auto first_document =
      resources::create_scene_2d_document(create_test_node("first-document"));
  const auto replacement_document =
      resources::create_scene_2d_document(create_test_node("replacement-document"));
  resources::register_scene_2d_document_importer(
      first_registry, flight::String("first"),
      types::Scene2DDocumentImporterMatcher{
          [&](flight::Uint8Array, ImportContextView) {
            ++first_match_calls;
            return false;
          }},
      types::Scene2DDocumentImporter{
          [first_document](flight::Uint8Array, ImportContextView) {
            return std::optional<flight::Ref<types::Scene2DDocument>>{first_document};
          }});
  resources::register_scene_2d_document_importer(
      first_registry, flight::String("second"),
      types::Scene2DDocumentImporterMatcher{
          [&](flight::Uint8Array source, ImportContextView) {
            ++second_match_calls;
            return source.size() == 1 && source.get_index(0.0) == 42.0;
          }},
      types::Scene2DDocumentImporter{
          [first_document](flight::Uint8Array, ImportContextView) {
            return std::optional<flight::Ref<types::Scene2DDocument>>{first_document};
          }});
  const auto entries_identity = first_registry->entries.identity();
  const auto replaced_entry = first_registry->entries[0];
  resources::register_scene_2d_document_importer(
      first_registry, flight::String("first"),
      types::Scene2DDocumentImporterMatcher{
          [&](flight::Uint8Array source, ImportContextView) {
            ++first_match_calls;
            return source.size() == 1 && source.get_index(0.0) == 42.0;
          }},
      types::Scene2DDocumentImporter{
          [replacement_document](flight::Uint8Array, ImportContextView) {
            return std::optional<flight::Ref<types::Scene2DDocument>>{replacement_document};
          }});
  check(first_registry->entries.size() == 2 &&
            first_registry->entries.identity() == entries_identity &&
            first_registry->entries[0] != replaced_entry &&
            first_registry->entries[0]->kind == flight::String("first") &&
            first_registry->entries[1]->kind == flight::String("second"),
        "register replaces a kind in place without replacing or reordering the entries array");

  const auto imported = resources::create_scene_2d_document_from_bytes(
      flight::Uint8Array{42}, RegistryView(first_registry));
  check(imported ==
                std::optional<flight::Ref<types::Scene2DDocument>>{replacement_document} &&
            imported.value()->source_kind ==
                std::optional<flight::String>{flight::String("first")} &&
            first_match_calls == 1 && second_match_calls == 0,
        "byte lookup dispatches only the first matching importer and supplies its source kind");

  int fetch_calls = 0;
  types::Scene2DDocumentFetcher successful_fetch =
      [&](flight::String url, flight::AbortSignal, auto progress) {
        ++fetch_calls;
        check(url == flight::String("asset.scene") && !progress.has_value(),
              "document fetch receives the exact URL and absent progress channel");
        return flight::Task<std::optional<flight::Uint8Array>>::resolve(
            std::optional<flight::Uint8Array>{flight::Uint8Array{42}});
      };
  const auto loaded = resources::load_scene_2d_document_from_url(
                          flight::String("asset.scene"), RegistryView(first_registry),
                          successful_fetch)
                          .get();
  check(fetch_calls == 1 && loaded.has_value() && loaded.value() == replacement_document,
        "document loading fetches once and passes the bytes through importer lookup");

  check(resources::unregister_scene_2d_document_importer(
            first_registry, flight::String("first")) &&
            !resources::unregister_scene_2d_document_importer(
                first_registry, flight::String("first")) &&
            first_registry->entries.size() == 1 &&
            first_registry->entries[0]->kind == flight::String("second"),
        "unregister removes one existing kind, preserves order, and reports later absence");
}

void manifest_order_and_coverage() {
  const auto filled = resources::create_scene_2d_slot_reference(
      flight::String("filled"), create_test_node("filled-target"));
  filled->content = create_test_node("filled-content");
  const auto missing = resources::create_scene_2d_slot_reference(
      flight::String("missing"), create_test_node("missing-target"));
  const auto optional = resources::create_scene_2d_slot_reference(
      flight::String("optional"), create_test_node("optional-target"),
      nullable_string(std::nullopt), false);
  flight::Array<flight::Ref<types::Scene2DSlotReference>> slots{
      filled, missing, optional};
  const auto document = resources::create_scene_2d_document(
      create_test_node("coverage-root"), slots);
  const auto resolved_image = image_reference(types::resource_resolution_state->resolved);
  const auto failed_image = image_reference(types::resource_resolution_state->failed);
  const auto loading_audio = audio_reference(types::resource_resolution_state->loading);
  document->image_resources = flight::Array<types::ImageResourceReference>{
      types::ImageResourceReference{
          std::in_place_type<flight::Ref<types::ExternalImageResourceReference>>, resolved_image},
      types::ImageResourceReference{
          std::in_place_type<flight::Ref<types::ExternalImageResourceReference>>, failed_image}};
  document->audio_resources = flight::Array<types::AudioResourceReference>{
      types::AudioResourceReference{
          std::in_place_type<flight::Ref<types::ExternalAudioResourceReference>>, loading_audio}};
  check(document->slots.identity() == slots.identity() &&
            document->slots[0] == filled && document->slots[1] == missing &&
            document->slots[2] == optional,
        "slot manifests retain authored array identity and ordering");
  const auto coverage = resources::explain_scene_2d_resource_coverage(document);
  check(coverage->image_resources->resolved == 1.0 &&
            coverage->image_resources->total == 2.0 &&
            coverage->audio_resources->resolved == 0.0 &&
            coverage->audio_resources->total == 1.0 && !coverage->complete,
        "coverage counts every asset reference and observes unresolved required slots");

  const auto required_only = resources::create_scene_2d_slot_reference(
      flight::String("required-only"), create_test_node("required-only-target"));
  required_only->content = create_test_node("required-only-content");
  const auto optional_only = resources::create_scene_2d_slot_reference(
      flight::String("optional-only"), create_test_node("optional-only-target"),
      nullable_string(std::nullopt), false);
  const auto slot_document = resources::create_scene_2d_document(
      create_test_node("slot-coverage-root"),
      flight::Array<flight::Ref<types::Scene2DSlotReference>>{
          required_only, optional_only});
  const auto filled_slot_coverage =
      resources::explain_scene_2d_resource_coverage(slot_document);
  check(filled_slot_coverage->complete,
        "coverage ignores an unresolved optional slot when every required slot is filled");
  required_only->content = std::nullopt;
  const auto missing_slot_coverage =
      resources::explain_scene_2d_resource_coverage(slot_document);
  check(!missing_slot_coverage->complete,
        "coverage observes a required slot becoming unresolved");
  const auto empty = resources::explain_scene_2d_resource_coverage(
      resources::create_scene_2d_document(create_test_node("empty")));
  check(empty->complete && empty->image_resources->total == 0.0 &&
            empty->audio_resources->total == 0.0,
        "coverage reports a document with no resources as complete");
}

void image_texture_binding_and_invalidation() {
  const auto reference = image_reference(types::resource_resolution_state->resolved);
  const auto first = flight::texture::create_texture_2d(std::nullopt);
  const auto second = flight::texture::create_texture_2d(std::nullopt);
  const auto first_version = first->version;
  const auto second_version = second->version;
  reference->textures = flight::Array<types::Texture>{
      types::Texture{std::in_place_type<flight::Ref<types::Texture2D>>, first},
      types::Texture{std::in_place_type<flight::Ref<types::Texture2D>>, second}};

  const auto source = flight::make_ref<types::TextureSource>();
  source->kind = flight::String("image");
  source->version = 17.0;
  resources::bind_scene_2d_image_resource_textures(
      types::ImageResourceReference{
          std::in_place_type<flight::Ref<types::ExternalImageResourceReference>>, reference},
      source);

  check(first->source.has_value() && second->source.has_value() &&
            std::get<flight::Ref<types::TextureSource>>(first->source.value()) == source &&
            std::get<flight::Ref<types::TextureSource>>(second->source.value()) == source,
        "image binding fans the identical decoded source out to every attached texture");
  check(first->version == first_version + 1.0 &&
            second->version == second_version + 1.0,
        "image binding invalidates every attached texture exactly once");

  const auto unattached = image_reference(types::resource_resolution_state->resolved);
  resources::bind_scene_2d_image_resource_textures(
      types::ImageResourceReference{
          std::in_place_type<flight::Ref<types::ExternalImageResourceReference>>, unattached},
      source);
  check(!unattached->textures.has_value(),
        "image binding preserves an absent texture list without allocating it");
}

void diagnostics_malformed_inputs_and_default_fetches() {
  std::vector<std::pair<flight::String, flight::String>> notices;
  resources::set_scene_2d_resource_failure_guard(
      types::Scene2DResourceFailureGuard{[&](FailureView notice) {
        notices.emplace_back(notice->operation, notice->reason);
      }});

  const auto empty_registry = resources::create_scene_2d_document_importer_registry();
  check(!resources::create_scene_2d_document_from_bytes(
             flight::Uint8Array{0}, RegistryView(empty_registry))
             .has_value(),
        "malformed/unclaimed bytes return the documented null sentinel");
  resources::register_scene_2d_document_importer(
      empty_registry, flight::String("broken"),
      types::Scene2DDocumentImporterMatcher{
          [](flight::Uint8Array, ImportContextView) { return true; }},
      types::Scene2DDocumentImporter{
          [](flight::Uint8Array, ImportContextView) {
            return std::optional<flight::Ref<types::Scene2DDocument>>{};
          }});
  check(!resources::create_scene_2d_document_from_bytes(
             flight::Uint8Array{1}, RegistryView(empty_registry))
             .has_value(),
        "an importer rejection retains the null sentinel");

  types::Scene2DDocumentFetcher failing_fetch =
      [](flight::String, flight::AbortSignal, auto) {
        return flight::Task<std::optional<flight::Uint8Array>>::resolve(std::nullopt);
      };
  check(!resources::load_scene_2d_document_from_url(
             flight::String("missing.scene"), RegistryView(empty_registry), failing_fetch)
             .get()
             .has_value(),
        "a failed document fetch retains the null sentinel");

  check(notices ==
            std::vector<std::pair<flight::String, flight::String>>{
                {flight::String("createScene2DDocumentFromBytes"),
                 flight::String("document-importer-missing")},
                {flight::String("createScene2DDocumentFromBytes"),
                 flight::String("document-import-failed")},
                {flight::String("loadScene2DDocumentFromUrl"),
                 flight::String("document-fetch-failed")}},
        "failure diagnostics preserve exact operation-local reason order");

  const auto external_audio = audio_reference(types::resource_resolution_state->unresolved);
  const auto external_image = image_reference(types::resource_resolution_state->unresolved);
  check(!resources::reject_external_audio_resource(
              ExternalAudioView(external_audio), flight::AbortController().signal)
              .get()
              .has_value() &&
            !resources::reject_external_image_resource(
                ExternalImageView(external_image), flight::AbortController().signal)
                .get()
                .has_value(),
        "default external fetch seams accept the declared arguments and resolve to null");

  resources::set_scene_2d_resource_failure_guard(std::nullopt);
  const auto notice_count = notices.size();
  static_cast<void>(resources::create_scene_2d_document_from_bytes(
      flight::Uint8Array{0},
      RegistryView(resources::create_scene_2d_document_importer_registry())));
  check(notices.size() == notice_count,
        "clearing the diagnostic guard ends its observable lifecycle");
}

}  // namespace

int main() {
  document_and_slot_construction();
  importer_registry_and_document_loading();
  manifest_order_and_coverage();
  image_texture_binding_and_invalidation();
  diagnostics_malformed_inputs_and_default_fetches();
  resources::set_scene_2d_resource_failure_guard(std::nullopt);
  return failures == 0 ? 0 : 1;
}
