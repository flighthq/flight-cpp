#include <flight/net/contract.hpp>

#include <functional>
#include <iostream>
#include <string>
#include <vector>

// Native port of packages/net/src/net.test.ts, netForm.test.ts and enableNetGuards.test.ts (a627849),
// limited to cases whose code paths are callable and reach no stub.
//
// Not ported:
// - enableNetGuards' three warning cases. A guard warning goes through log_once -> log ->
//   merge_span_fields, a NotImplementedInThisProfile stub in the log override, so it throws instead
//   of logging; see that override's `remaining`.
// - the 'R3 boundary' case, which reflects over the module's runtime export object. C++ has no such
//   object to enumerate.

namespace {

using flight::String;
using namespace flight::types;
using RequestView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<NetRequest>>>>;
using OptionsView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<NetRequestOptions>>>>;
using ResponseView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<NetResponse>>>>;
using HostView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<HostNetCapability>>>>;
using NoticeView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<NetGuardNotice>>>>;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

flight::Ref<NetResponse> stub_response(NetResponseBody body, double status = 200.0, const char* status_text = "OK") {
  return flight::make_ref<NetResponse>(NetResponse{
      .status = status, .status_text = String(status_text), .ok = status == 200.0, .headers = {}, .body = std::move(body), .url = String("u")});
}

flight::Ref<NetRequest> request(const char* method, const char* url, std::optional<NetBody> body = std::nullopt,
                                std::optional<double> timeout_ms = std::nullopt) {
  return flight::make_ref<NetRequest>(NetRequest{.method = String(method), .url = String(url), .headers = std::nullopt, .body = std::move(body),
                                                 .response_type = std::nullopt, .timeout_ms = timeout_ms, .credentials = std::nullopt,
                                                 .redirect = std::nullopt});
}

flight::Ref<HostNetCapability> host_of(std::function<flight::Task<flight::Ref<NetResponse>>(RequestView, std::optional<OptionsView>)> send) {
  return flight::make_ref<HostNetCapability>(HostNetCapability{.send_net_request = std::move(send)});
}

void test_explain_net_response() {
  const auto aborted = flight::net::explain_net_response(stub_response(std::nullopt, 0.0, "aborted"));
  const auto timeout = flight::net::explain_net_response(stub_response(std::nullopt, 0.0, "timeout"));
  check(aborted.has_value() && aborted.value()->reason == String("aborted") && aborted.value()->status == 0.0 && timeout.has_value() &&
            timeout.value()->reason == String("timeout") && timeout.value()->status == 0.0,
        "explainNetResponse derives specific abort and timeout reasons from transport status text");
  const auto transport = flight::net::explain_net_response(stub_response(std::nullopt, 0.0, "Failed to fetch"));
  const auto decode = flight::net::explain_net_response(stub_response(std::nullopt));
  check(transport.has_value() && transport.value()->reason == String("transport-failure") &&
            transport.value()->status_text == String("Failed to fetch") && decode.has_value() &&
            decode.value()->reason == String("decode-failure") && decode.value()->status == 200.0,
        "explainNetResponse distinguishes generic transport and decode failures");
  check(!flight::net::explain_net_response(stub_response(NetResponseBody{String("ok")})).has_value(),
        "explainNetResponse returns null when the response carries no failure sentinel");
}

void test_send_net_request() {
  std::optional<RequestView> received_request;
  std::optional<std::optional<OptionsView>> received_options;
  const auto host = host_of([&](RequestView sent, std::optional<OptionsView> options) {
    received_request = sent;
    received_options = options;
    return flight::resolve_task(stub_response(std::nullopt));
  });
  const auto sent = request("GET", "https://example.test");
  const auto options = flight::make_ref<NetRequestOptions>(NetRequestOptions{.progress = std::nullopt, .signal = std::nullopt});
  static_cast<void>(flight::net::send_net_request(host, sent, OptionsView(options)).get());
  check(received_request.has_value() && received_request.value() == RequestView(sent) && received_options.has_value() &&
            received_options->has_value() && received_options->value() == OptionsView(options),
        "sendNetRequest dispatches through the host backend and passes the same request and options");

  const auto response = stub_response(std::nullopt);
  const auto passthrough = host_of([&](RequestView, std::optional<OptionsView>) { return flight::resolve_task(response); });
  check(flight::net::send_net_request(passthrough, request("GET", "u")).get() == response,
        "sendNetRequest passes the response through unchanged");
}

void test_set_net_guard() {
  std::vector<std::string> reasons;
  flight::net::set_net_guard(NetGuard([&](NoticeView notice) { reasons.push_back(std::string(flight::row_get<flight::RowKey<"reason">>(notice).to_utf8())); }));
  const auto host = host_of([](RequestView, std::optional<OptionsView>) { return flight::resolve_task(stub_response(std::nullopt)); });
  static_cast<void>(flight::net::send_net_request(host, request("get", "u", NetBody{String("x")}, -1.0)).get());
  flight::net::set_net_guard(std::nullopt);
  static_cast<void>(flight::net::send_net_request(host, request("HEAD", "v", NetBody{String("x")})).get());
  check(reasons == std::vector<std::string>({"body-on-get-or-head", "negative-timeout"}),
        "setNetGuard installs and removes the core request notice hook");
}

void test_net_form() {
  flight::Record<String, String> fields;
  fields.set(String("query"), String("flight sdk"));
  fields.set(String("redirect"), String("https://example.test/a?x=1&y=2"));
  check(flight::net::format_net_form_body(fields) == String("query=flight+sdk&redirect=https%3A%2F%2Fexample.test%2Fa%3Fx%3D1%26y%3D2"),
        "formatNetFormBody encodes fields in insertion order with HTML form space semantics");
  flight::Record<String, String> unsafe;
  unsafe.set(String("!~'()"), String("café"));
  check(flight::net::format_net_form_body(unsafe) == String("%21%7E%27%28%29=caf%C3%A9"),
        "formatNetFormBody encodes form-unsafe punctuation and unicode");
  check(flight::net::net_form_content_type == String("application/x-www-form-urlencoded;charset=UTF-8"),
        "NetFormContentType declares the encoding produced by formatNetFormBody");
}

void test_guard_enablement() {
  flight::net::enable_net_guards();
  const bool enabled = flight::net::are_net_guards_enabled();
  flight::net::disable_net_guards();
  check(enabled && !flight::net::are_net_guards_enabled(), "areNetGuardsEnabled reports whether the global guard hook is installed");

  // 'restores silent request delegation'. Upstream captures with createMemoryLogSink, which throws here
  // (initialize_log_entity stub), so a plain registered sink observes the same thing: no entries.
  int entries = 0;
  LogSink recording = [&](flight::Ref<LogEntry>) { ++entries; };
  flight::log::add_log_sink(recording);
  flight::net::enable_net_guards();
  flight::net::disable_net_guards();
  const auto host = host_of([](RequestView, std::optional<OptionsView>) { return flight::resolve_task(stub_response(NetResponseBody{String("")})); });
  bool threw = false;
  try {
    static_cast<void>(flight::net::send_net_request(host, request("GET", "u", NetBody{String("ignored")}, -1.0)).get());
  } catch (...) {
    threw = true;
  }
  flight::log::remove_log_sink(recording);
  check(!threw && entries == 0, "disableNetGuards restores silent request delegation");
}

}  // namespace

int main() {
  test_explain_net_response();
  test_send_net_request();
  test_set_net_guard();
  test_net_form();
  test_guard_enablement();
  return failures == 0 ? 0 : 1;
}
