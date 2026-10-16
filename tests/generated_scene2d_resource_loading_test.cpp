// Executes the emitted orchestration against real declared owners and resolver sidecars.
#include <flight/scene2d_resources/load_scene_2d_audio_resources.hpp>

#include <flight/executor.hpp>
#include <iostream>
#include <optional>
#include <vector>

namespace {
namespace types = flight::types;
namespace resources = flight::scene2d_resources;
template <typename T>
using Readonly = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<T>>>>;
using AudioValue = std::optional<flight::Ref<types::AudioResource>>;
using AudioTask = flight::Task<AudioValue>;
using AudioProgress = Readonly<types::Scene2DAudioResourceLoadProgress>;
using Failure = Readonly<types::Scene2DResourceFailureNotice>;
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}
auto audio_reference(const char* uri) {
  auto reference = flight::make_ref<types::ExternalAudioResourceReference>();
  reference->uri = flight::String(uri);
  reference->kind = types::audio_resource_reference_kind->external;
  reference->state = types::resource_resolution_state->unresolved;
  reference->resource = flight::audio::create_audio_resource();
  return reference;
}
types::AudioResourceReference audio_variant(
    const flight::Ref<types::ExternalAudioResourceReference>& reference) {
  return types::AudioResourceReference{reference};
}

void audio_loading() {
  auto queue = std::make_shared<flight::QueueExecutor>();
  flight::ExecutorScope scope(queue);
  auto first = audio_reference("first");
  auto miss = audio_reference("miss");
  auto last = audio_reference("last");
  auto skipped = audio_reference("skipped");
  auto original_first_resource = first->resource;
  auto original_last_resource = last->resource;
  auto document = flight::make_ref<types::Scene2DDocument>();
  document->audio_resources = {audio_variant(first), audio_variant(miss),
                               audio_variant(last), audio_variant(skipped)};
  auto manifest_identity = document->audio_resources.identity();
  auto options = flight::make_ref<types::LoadScene2DAudioResourcesOptions>();
  options->select = [skipped](types::AudioResourceReference value) {
    return std::get<1>(value) != skipped;
  };
  std::vector<AudioTask::Resolver> resolve;
  std::vector<const void*> fetch_order;
  options->fetch = [&](Readonly<types::ExternalAudioResourceReference> value,
                       flight::AbortSignal) {
    fetch_order.push_back(value.shared_owner().get());
    return AudioTask::create([&](auto done) { resolve.push_back(done); });
  };
  std::vector<const void*> progress_order;
  std::vector<double> progress_counts;
  auto progress = flight::make_ref<types::Signal<std::function<void(AudioProgress)>>>();
  progress->emit = [&](AudioProgress value) {
    const auto reference = flight::row_get<flight::RowKey<"reference">>(value);
    progress_order.push_back(std::get<1>(reference).get());
    progress_counts.push_back(flight::row_get<flight::RowKey<"loaded">>(value));
    check(flight::row_get<flight::RowKey<"total">>(value) == 3.0,
          "audio progress counts only selected references");
  };
  options->progress = progress;
  int diagnostics = 0;
  resources::set_scene_2d_resource_failure_guard([&](Failure notice) {
    ++diagnostics;
    check(progress_order.size() == 3 &&
              flight::row_get<flight::RowKey<"unresolved">>(notice) == 1.0 &&
              flight::row_get<flight::RowKey<"reason">>(notice) ==
                  flight::String("audio-resources-unresolved"),
          "audio failure notice follows all progress and reports the miss");
  });
  auto task = resources::load_scene_2d_audio_resources(document, Readonly<types::LoadScene2DAudioResourcesOptions>(options));
  queue->run_until_idle();
  check(!task.is_ready() && resolve.size() == 3 && progress_order.empty(),
        "audio loader starts selected work and awaits its settlement");
  check(fetch_order == std::vector<const void*>{
              Readonly<types::ExternalAudioResourceReference>(first).identity(),
              Readonly<types::ExternalAudioResourceReference>(miss).identity(),
              Readonly<types::ExternalAudioResourceReference>(last).identity()},
        "audio fetch receives original selected reference owners in authored order");
  auto decoded = flight::audio::create_audio_resource(
      flight::AudioBuffer({.length = 4, .sample_rate = 8000}));
  resolve[2](AudioValue{decoded});
  queue->run_until_idle();
  check(!task.is_ready() && progress_order == std::vector<const void*>{last.get()},
        "audio progress follows completion while result waits for all work");
  resolve[0](AudioValue{decoded});
  resolve[1](AudioValue{});
  queue->run_until_idle();
  auto result = task.get();
  check(result->document == document &&
            document->audio_resources.identity() == manifest_identity,
        "audio result retains original document and manifest array");
  check(result->resolved.size() == 2 && result->unresolved.size() == 1 &&
            std::get<1>(result->resolved[0]) == first &&
            std::get<1>(result->resolved[1]) == last &&
            std::get<1>(result->unresolved[0]) == miss,
        "audio result partitions original reference owners in authored order");
  check(first->resource == original_first_resource &&
            last->resource == original_last_resource && first->resource != decoded &&
            first->resource->buffer->identity() == decoded->buffer->identity() &&
            last->resource->buffer->identity() == decoded->buffer->identity(),
        "audio resolution fills existing resource owners with the same buffer");
  check(miss->failure.has_value() &&
            miss->state == types::resource_resolution_state->failed &&
            skipped->state == types::resource_resolution_state->unresolved &&
            !skipped->resource->buffer.has_value(),
        "audio failure sidecar reaches original owner and skipped resource is untouched");
  check(progress_order == std::vector<const void*>{last.get(), first.get(), miss.get()} &&
            progress_counts == std::vector<double>{1, 2, 3} && diagnostics == 1,
        "audio progress uses completion order and monotonically counted finally blocks");
  result->resolved[0] = audio_variant(last);
  check(std::get<1>(document->audio_resources[0]) == first,
        "audio result collection is distinct from the document manifest");
  resources::set_scene_2d_resource_failure_guard(std::nullopt);
}

void audio_error_and_empty() {
  auto queue = std::make_shared<flight::QueueExecutor>();
  flight::ExecutorScope scope(queue);
  auto document = flight::make_ref<types::Scene2DDocument>();
  auto empty_a = resources::load_scene_2d_audio_resources(document).get();
  auto empty_b = resources::load_scene_2d_audio_resources(document).get();
  check(empty_a != empty_b && empty_a->document == document &&
            empty_a->resolved.empty() && empty_a->unresolved.empty() &&
            empty_a->resolved.identity() != empty_b->resolved.identity() &&
            empty_a->unresolved.identity() != empty_b->unresolved.identity(),
        "empty audio loads allocate independent declared result owners and arrays");
  auto reference = audio_reference("error");
  document->audio_resources = {audio_variant(reference)};
  auto options = flight::make_ref<types::LoadScene2DAudioResourcesOptions>();
  options->fetch = [](Readonly<types::ExternalAudioResourceReference>, flight::AbortSignal) {
    return AudioTask::ready(AudioValue{});
  };
  auto progress = flight::make_ref<types::Signal<std::function<void(AudioProgress)>>>();
  auto thrown = std::make_shared<int>(42);
  progress->emit = [thrown](AudioProgress) { throw thrown; };
  options->progress = progress;
  int diagnostics = 0;
  resources::set_scene_2d_resource_failure_guard([&](Failure) { ++diagnostics; });
  bool exact_throw = false;
  try {
    static_cast<void>(resources::load_scene_2d_audio_resources(document, Readonly<types::LoadScene2DAudioResourcesOptions>(options)).get());
  } catch (const std::shared_ptr<int>& caught) {
    exact_throw = caught == thrown;
  }
  check(exact_throw && diagnostics == 0 && reference->failure.has_value(),
        "audio progress exception retains its exact value and prevents post-await result reporting");
  resources::set_scene_2d_resource_failure_guard(std::nullopt);
}
} // namespace

int main() {
  audio_loading();
  audio_error_and_empty();
  if (failures == 0) std::cout << "Scene2D resource loading assertions passed\n";
  return failures == 0 ? 0 : 1;
}
