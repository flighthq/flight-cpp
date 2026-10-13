// Native parity test for @flighthq/loader's callable emitted surface, at a627849.
//
// Only one module of this package emits: `load.ts`, whose `loadBytes`, `loadText` and private `requestOptions` are
// the whole of `flight/loader/load.hpp` -- the one loader header that compiles at this pin. `resourceLoader.ts` is
// refused (unsupported-ir: a reference assertion from flight::Ref<flight::types::ResourceLoader> targets a type the
// refinement representation does not carry), and `contract.ts` and `index.ts` are refused behind it, so
// `resource_loader.hpp` declares initialize_resource_loader and cannot compile. That refusal is the closed
// flattened-interface-refinement-identity decision, not a gap this test can close, and no loader override is added.
//
// Covered here, from packages/loader/src/load.test.ts and the two functions' own semantics: the payload and its
// domain (bytes as a Uint8Array over the response buffer, text as a string, and absence as null rather than an
// exception), the request each call sends (GET, with `arraybuffer` or `text` as its response type, and the url), the
// options it passes through (none when the caller gave none, the caller's signal when only that was given), the
// progress mapping's payload and order (a NetProgress re-emitted as a LoadProgress carrying the url argument and the
// event's own loaded/total/phase), and the failure domain of both functions: a non-ok response, a body of the wrong
// kind, and a rejected request all answer null instead of propagating.
//
// EXCLUDED, by the resourceLoader refusal: the whole of resourceLoader.test.ts -- createResourceLoader and its
// concurrency and ordering, queueResourceLoad priority and pause/resume, start/cancel/reset and dispose, the item
// signals, getResourceLoadItemStatus/getResourceLoadBytes/getResourceLoadCounts/getResourceLoadProgress, and
// enableResourceLoaderItemSignals. None of it is callable here: the module does not emit, and nothing in this file
// stands in for it.
#include <flight/loader/load.hpp>

#include <functional>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

namespace {

using flight::String;
using flight::to_string;
namespace loader = flight::loader;
namespace signals = flight::signals;
namespace types = flight::types;

using HostView = flight::Ref<types::HostNetCapability>;
using RequestView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::NetRequest>>>>;
using OptionsView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::NetRequestOptions>>>>;
using LoadOptionsView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::LoadOptions>>>>;
using LoadProgressRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::LoadProgress>>>>;
using NetProgressRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::NetProgress>>>>;
// Two signals meet here and are not interchangeable: the caller's progress slot carries LoadProgress and is what
// requestOptions fills, while the slot the mapper installs for the host carries NetProgress and is what a host emits.
using NetProgressSignal = flight::types::Signal<std::function<void(NetProgressRow)>>;
using LoadProgressSignal = flight::types::Signal<std::function<void(LoadProgressRow)>>;
using HostSender = std::function<flight::Task<flight::Ref<types::NetResponse>>(RequestView, std::optional<OptionsView>)>;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

std::string text(const String& value) { return value.to_utf8(); }

// The contract both functions state: every way a load can fail answers absence. A call that throws instead has not
// answered absent, so the failure cases assert the absence AND that nothing escaped.
template <typename Result>
bool answers_absent(flight::Task<Result> task) {
  try {
    return !task.get().has_value();
  } catch (...) {
    return false;
  }
}

flight::Uint8Array bytes(std::initializer_list<std::uint8_t> values) { return flight::Uint8Array(values); }

// The response a host answers with: a status, and a body of exactly the kind the caller should or should not accept.
flight::Ref<types::NetResponse> response_of(types::NetResponseBody body, double status = 200.0) {
  return flight::make_ref<types::NetResponse>(types::NetResponse{.status = status,
                                                                .status_text = String("OK"),
                                                                .ok = status == 200.0,
                                                                .headers = {},
                                                                .body = std::move(body),
                                                                .url = String("u")});
}

types::NetResponseBody buffer_body(const flight::Uint8Array& source) {
  return types::NetResponseBody{std::variant<flight::ArrayBuffer, flight::Blob, String, flight::Any>{
      flight::ArrayBuffer(source.buffer)}};
}

HostView host_of(HostSender send) {
  return flight::make_ref<types::HostNetCapability>(types::HostNetCapability{.send_net_request = std::move(send)});
}

// A host that answers every request the same way, so a call's own payload can be asserted without the request.
HostView answering(types::NetResponseBody body, double status = 200.0) {
  return host_of([body = std::move(body), status](RequestView, std::optional<OptionsView>) {
    return flight::resolve_task(response_of(body, status));
  });
}

LoadOptionsView options_with(std::optional<flight::AbortSignal> signal = std::nullopt,
                             std::optional<std::shared_ptr<LoadProgressSignal>> progress = std::nullopt) {
  return flight::make_structural_ref<flight::RowReadonly<flight::RowOf<flight::Ref<types::LoadOptions>>>>(
      flight::row_field<flight::RowKey<"signal">>(signal), flight::row_field<flight::RowKey<"progress">>(progress));
}

// loadBytes answers with the response body's bytes, and with null for every way the load can fail.
void test_load_bytes_payload_and_failure_domain() {
  const auto source = bytes({7, 8, 9});
  const auto loaded = loader::load_bytes(answering(buffer_body(source)), String("u")).get();
  check(loaded.has_value() && loaded->size() == 3 && (*loaded)[0] == 7 && (*loaded)[1] == 8 && (*loaded)[2] == 9,
        "loadBytes answers with the response body's bytes");

  // The returned array is a view over the buffer the response carried, the way `new Uint8Array(response.body)` is:
  // writing through it is visible in that buffer.
  if (loaded.has_value()) {
    (*loaded)[0] = 42;
    check(source.buffer.data() != nullptr && std::to_integer<int>(source.buffer.data()[0]) == 42,
          "the returned bytes are a view over the response buffer, not a copy");
  }

  // A body of the right kind does not save a response that is not ok: the ok test is what decides the answer.
  check(answers_absent(loader::load_bytes(answering(buffer_body(source), 404.0), String("u"))),
        "a non-ok response answers null even when its body is usable");
  check(answers_absent(loader::load_bytes(answering(types::NetResponseBody{String("text")}), String("u"))),
        "a body that is not an ArrayBuffer answers null");
  check(!loader::load_bytes(answering(types::NetResponseBody{}), String("u")).get().has_value(),
        "an absent body answers null");
  const auto rejecting = host_of([](RequestView, std::optional<OptionsView>) {
    return flight::Task<flight::Ref<types::NetResponse>>::reject(std::string("transport failed"));
  });
  check(answers_absent(loader::load_bytes(rejecting, String("u"))),
        "a rejected request answers null rather than propagating");
}

// loadText answers with the response body's text, and with null for the same failures.
void test_load_text_payload_and_failure_domain() {
  const auto loaded = loader::load_text(answering(types::NetResponseBody{String("ok")}), String("u")).get();
  check(loaded.has_value() && text(*loaded) == "ok", "loadText answers with the response body's text");

  check(answers_absent(loader::load_text(answering(types::NetResponseBody{String("ok")}, 500.0), String("u"))),
        "a non-ok response answers null even when its body is usable");
  check(answers_absent(loader::load_text(answering(buffer_body(bytes({1}))), String("u"))),
        "a body that is not a string answers null");

  // A body the host carried as an erased value still answers: the emitted test asks the erased value what it holds
  // rather than trusting the alternative the declaration names, so a string arriving as `any` is a string.
  const auto erased = loader::load_text(answering(types::NetResponseBody{flight::Any(String("erased"))}), String("u")).get();
  check(erased.has_value() && text(*erased) == "erased", "a string carried as an erased body still answers");
  const auto rejecting = host_of([](RequestView, std::optional<OptionsView>) {
    return flight::Task<flight::Ref<types::NetResponse>>::reject(std::string("transport failed"));
  });
  check(answers_absent(loader::load_text(rejecting, String("u"))),
        "a rejected request answers null rather than propagating");
}

// Both functions ask for one GET of the url, with the response type their own return domain needs.
void test_request_shape() {
  std::vector<std::string> seen;
  const auto recording = host_of([&](RequestView request, std::optional<OptionsView>) {
    seen.push_back(text(flight::row_get<flight::RowKey<"method">>(request)) + " " +
                   text(flight::row_get<flight::RowKey<"url">>(request)) + " " +
                   text(flight::row_get<flight::RowKey<"responseType">>(request).value()));
    return flight::resolve_task(response_of(types::NetResponseBody{String("ok")}));
  });
  static_cast<void>(loader::load_bytes(recording, String("bytes.bin")).get());
  static_cast<void>(loader::load_text(recording, String("text.txt")).get());

  check(seen.size() == 2 && seen[0] == "GET bytes.bin arraybuffer" && seen[1] == "GET text.txt text",
        "each load is one GET of its url, with the response type its domain needs");
}

// The options a call passes on: none when the caller gave none, the caller's own signal when only that was given.
void test_options_passthrough() {
  std::optional<OptionsView> seen;
  bool called = false;
  const auto recording = host_of([&](RequestView, std::optional<OptionsView> options) {
    called = true;
    seen = options;
    return flight::resolve_task(response_of(types::NetResponseBody{String("ok")}));
  });

  static_cast<void>(loader::load_bytes(recording, String("u")).get());
  check(called && !seen.has_value(), "a call with no options passes none on");

  const auto signal = flight::AbortSignal();
  static_cast<void>(loader::load_bytes(recording, String("u"), options_with(signal)).get());
  check(seen.has_value() && flight::row_get<flight::RowKey<"signal">>(seen.value()).has_value(),
        "a call given a signal passes one on");
  check(seen.has_value() && !flight::row_get<flight::RowKey<"progress">>(seen.value()).has_value(),
        "and passes no progress signal when the caller asked for no progress");
}

// The progress the caller asked for: a NetProgress re-emitted as a LoadProgress, carrying the url being loaded and
// the event's own loaded, total and phase, in the order the events arrive.
void test_progress_mapping() {
  auto consumer = signals::create_signal<std::function<void(LoadProgressRow)>>();
  std::vector<std::string> delivered;
  signals::connect_signal<std::function<void(LoadProgressRow)>>(
      consumer,
      [&](LoadProgressRow event) {
        delivered.push_back(text(flight::row_get<flight::RowKey<"url">>(event)) + " " +
                            text(to_string(flight::row_get<flight::RowKey<"loaded">>(event))) + "/" +
                            text(to_string(flight::row_get<flight::RowKey<"total">>(event))) + " " +
                            text(flight::row_get<flight::RowKey<"phase">>(event)));
      },
      std::nullopt);

  // A host reports progress through the slot the caller engaged, and only when the options carry one -- which is
  // exactly the state requestOptions produces when the caller asked for progress and produces nothing of when they
  // did not.
  const auto emitting = host_of([&](RequestView, std::optional<OptionsView> options) {
    const auto event = [](double loaded, double total, const char* phase) {
      return flight::make_ref<types::NetProgress>(
          types::NetProgress{.loaded = loaded, .total = total, .phase = String(phase)});
    };
    if (options.has_value()) {
      const auto progress = flight::row_get<flight::RowKey<"progress">>(options.value());
      if (progress.has_value()) {
        static_cast<void>(signals::emit_signal<std::function<void(NetProgressRow)>>(progress.value(), event(2.0, 4.0, "body")));
        static_cast<void>(signals::emit_signal<std::function<void(NetProgressRow)>>(progress.value(), event(4.0, 4.0, "done")));
      }
    }
    return flight::resolve_task(response_of(types::NetResponseBody{String("ok")}));
  });

  static_cast<void>(loader::load_bytes(emitting, String("asset.bin"), options_with()).get());
  check(delivered.empty(), "a caller that asked for no progress receives nothing, and no mapping is installed");

  const std::shared_ptr<LoadProgressSignal> engaged = consumer;
  static_cast<void>(loader::load_text(emitting, String("asset.txt"), options_with(std::nullopt, engaged)).get());
  check(delivered.size() == 2, "each net progress event becomes one load progress event");
  check(delivered.size() == 2 && delivered[0] == "asset.txt 2/4 body" && delivered[1] == "asset.txt 4/4 done",
        "and carries the url being loaded with the event's own loaded, total and phase, in order");
}

}  // namespace

int main() {
  test_load_bytes_payload_and_failure_domain();
  test_load_text_payload_and_failure_domain();
  test_request_shape();
  test_options_passthrough();
  test_progress_mapping();
  if (failures == 0) std::cout << "generated loader: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
