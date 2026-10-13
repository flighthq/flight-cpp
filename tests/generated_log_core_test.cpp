// The @flighthq/log core at a627849 -- the a627849 emission carried by the log-module-qualification-and-operators
// override, with merge_span_fields, _mergeContextFields, initializeLogEntity, the log*With family,
// createChildLogContext, the buffered, rate-limited and file sinks, the text and JSON formatters, serializers,
// redaction, serializeLogError, log groups and signals supplied -- exercised natively. Ported from packages/log/src/log.test.ts:
// - every level function's entry shape, the lazy provider and its gate, log_once and log_assert;
// - span merge precedence: newer spans over older, data over spans, string data as { msg, ...fields };
// - contexts and child contexts, and the log*With composition of context over span over data;
// - the memory, buffered and rate-limited sinks, groups and signals; the text formatter; serializeLogError;
// - the JSON envelope, serializers and redaction; the file sink and its flush-then-destroy teardown.
//
// Additions the source's semantics imply but upstream does not assert, each marked: one entry object reaches every
// sink; merged keys keep JavaScript's spread order; a field present with the value undefined overrides and stays
// present, distinct from a missing one; a span or context field named msg wins over string data; data is returned
// as the same record when nothing merges; a child context's null channel clears where an absent one inherits;
// `indentGroups: false` does not indent; serializeLogError keeps a subclass's name; an absent channel or unknown
// level is omitted from the JSON envelope; a teardown is failed by an outcome's reason, not its shape; a thrown Error
// and a rejected value become operation-failed messages; a repeat destroy shares the first teardown and spreads its
// outcomes; a destroyed handle releases its transport.
//
// Not natively evidenced, and why: upstream's "extracts ... stack" case -- native errors carry no stack, so a
// serialized error has none (the "omits the stack" case is asserted); the buffered sink's "works without an
// interval implementation" -- the host always provides intervals. Timing uses the real host clock with margins
// far wider than the intervals involved.
#include <flight/log/log.hpp>
#include <flight/signals/slot.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

namespace lg = flight::log;
using flight::Any;
using flight::Ref;
using flight::String;
using flight::types::LogEntry;
using flight::types::LogLevel;
using Fields = flight::Record<String, Any>;
using Data = std::variant<Ref<flight::types::LogDataProvider>, Ref<flight::types::LogData>>;
using Channel = std::optional<std::optional<String>>;

Data text(const char* value) { return Data{std::in_place_index<1>, flight::types::LogData{std::in_place_index<1>, String(value)}}; }
Data record(Fields fields) { return Data{std::in_place_index<1>, flight::types::LogData{std::in_place_index<0>, std::move(fields)}}; }
Channel channel(const char* name) { return Channel{std::optional<String>{String(name)}}; }

void reset() {
  lg::clear_log_groups();
  lg::clear_log_once_keys();
  lg::clear_log_redaction_paths();
  lg::clear_log_serializers();
  lg::clear_log_sinks();
  lg::clear_log_channel_levels();
  lg::set_log_console_level(LogLevel::Info);
  lg::set_log_level(LogLevel::Verbose);
}

// upstream's recordingSink: installs a sink that records what it receives.
struct Recording {
  std::vector<Ref<LogEntry>> entries;
  Ref<lg::LogSink> sink;
};

Recording& recording() {
  static std::vector<std::unique_ptr<Recording>> all;
  all.push_back(std::make_unique<Recording>());
  auto* out = all.back().get();
  out->sink = Ref<lg::LogSink>([out](Ref<LogEntry> entry) { out->entries.push_back(entry); });
  lg::add_log_sink(out->sink);
  return *out;
}

bool is_text(const Ref<LogEntry>& entry, const char* value) {
  const auto* t = std::get_if<String>(&entry->data);
  return t != nullptr && *t == String(value);
}

// A failed expectation must fail its check, not crash the run: a non-record entry reads as an empty record.
const Fields* fields_of(const Ref<LogEntry>& entry) {
  static const Fields empty;
  const auto* fields = std::get_if<Fields>(&entry->data);
  return fields != nullptr ? fields : &empty;
}

std::vector<std::string> keys(const Fields& f) {
  std::vector<std::string> out;
  for (const auto& key : flight::object_keys(f)) out.push_back(key.to_utf8());
  return out;
}

double num(const Fields& f, const char* key) {
  const auto value = f.get(String(key));
  return value.has_value() && value->kind() == flight::AnyKind::number ? value->as_number() : -1e300;
}
std::string str(const Fields& f, const char* key) {
  const auto value = f.get(String(key));
  return value.has_value() && value->kind() == flight::AnyKind::string ? value->as_string().to_utf8() : std::string("<absent>");
}

void level_functions() {
  reset();
  {
    auto& r = recording();
    lg::log(LogLevel::Warn, record(Fields{{String("k"), Any(1.0)}}), channel("shader"));
    lg::log(LogLevel::Info, text("x"));
    check(r.entries.size() == 2 && r.entries[0]->level == LogLevel::Warn && r.entries[0]->channel == String("shader") &&
              num(*fields_of(r.entries[0]), "k") == 1 && !r.entries[1]->channel.has_value(),
          "log forwards level, channel and data, with the channel defaulting to null");
  }
  reset();
  {
    int calls = 0;
    lg::log(LogLevel::Info, Data{std::in_place_index<0>, [&calls] {
                                   ++calls;
                                   return flight::types::LogData{std::in_place_index<1>, String("x")};
                                 }});
    check(calls == 0, "with no sink installed nothing is emitted and the provider is not called");
    auto& r = recording();
    lg::log(LogLevel::Info, Data{std::in_place_index<0>, [&calls] {
                                   ++calls;
                                   return flight::types::LogData{std::in_place_index<1>, String("lazy-value")};
                                 }});
    check(calls == 1 && is_text(r.entries[0], "lazy-value"), "a lazy provider is called once and its data emitted");
    lg::set_log_level(LogLevel::Error);
    lg::log(LogLevel::Verbose, Data{std::in_place_index<0>, [&calls] {
                                      ++calls;
                                      return flight::types::LogData{std::in_place_index<1>, String("no")};
                                    }});
    check(calls == 1 && r.entries.size() == 1, "a suppressed level does not call the provider");
  }
  reset();
  {
    auto& a = recording();
    auto& b = recording();
    lg::log(LogLevel::Info, text("multi"));
    check(a.entries.size() == 1 && b.entries.size() == 1, "every sink receives the entry");
    // Addition: _emitToSinks hands every sink the one entry object.
    check(a.entries[0] == b.entries[0], "every sink receives the same entry object");
  }
  reset();
  {
    auto& r = recording();
    lg::log_debug(text("d"), channel("chan"));
    lg::log_info(text("i"));
    lg::log_warn(text("w"));
    lg::log_error(text("e"));
    lg::log_verbose(text("v"));
    check(r.entries.size() == 5 && r.entries[0]->level == LogLevel::Debug && r.entries[0]->channel == String("chan") &&
              is_text(r.entries[0], "d") && r.entries[1]->level == LogLevel::Info && r.entries[2]->level == LogLevel::Warn &&
              r.entries[3]->level == LogLevel::Error && r.entries[4]->level == LogLevel::Verbose && is_text(r.entries[4], "v"),
          "each level function emits at its level with its data and channel");
    lg::log_debug(Data{std::in_place_index<0>, [] { return flight::types::LogData{std::in_place_index<1>, String("lazy")}; }}, Channel{std::optional<String>{}});
    check(is_text(r.entries.back(), "lazy") && !r.entries.back()->channel.has_value(), "a level function accepts a lazy provider");
  }
  reset();
  {
    auto& r = recording();
    lg::log_assert(true, text("should not emit"));
    lg::log_assert(false, text("assertion failed"));
    check(r.entries.size() == 1 && r.entries[0]->level == LogLevel::Error && is_text(r.entries[0], "assertion failed"),
          "log_assert emits an Error entry only when the condition is false");
  }
  reset();
  {
    auto& r = recording();
    const bool first = lg::log_once(String("key1"), LogLevel::Warn, text("first"));
    const bool second = lg::log_once(String("key1"), LogLevel::Warn, text("second"));
    lg::log_once(String("key2"), LogLevel::Info, text("other"));
    check(first && !second && r.entries.size() == 2 && is_text(r.entries[0], "first") && is_text(r.entries[1], "other"),
          "log_once emits a key once, returns true then false, and keys are independent");
    lg::clear_log_once_keys();
    check(lg::log_once(String("key1"), LogLevel::Warn, text("again")) && r.entries.size() == 3, "clearing once-keys lets a key fire again");
  }
  reset();
  {
    auto& r = recording();
    lg::set_log_level(LogLevel::Info);
    lg::log_debug(text("suppressed"));
    lg::log(LogLevel::None, text("never"));
    check(r.entries.empty(), "levels finer than the global level, and None, are suppressed");
  }
}

void spans() {
  reset();
  {
    auto& r = recording();
    auto span = lg::create_log_span(String("request"), Fields{{String("reqId"), Any(String("abc"))}});
    lg::enter_log_span(span);
    lg::log(LogLevel::Info, text("inside span"));
    lg::exit_log_span(span);
    lg::log(LogLevel::Info, text("outside span"));
    const auto* merged = fields_of(r.entries[0]);
    check(merged != nullptr && str(*merged, "reqId") == "abc" && str(*merged, "msg") == "inside span" &&
              keys(*merged) == std::vector<std::string>{"msg", "reqId"},
          "an active span's fields merge into string data as { msg, ...fields }");
    check(is_text(r.entries[1], "outside span"), "nothing merges after the span exits");
  }
  reset();
  {
    auto& r = recording();
    auto outer = lg::create_log_span(String("outer"), Fields{{String("x"), Any(1.0)}, {String("y"), Any(10.0)}});
    auto inner = lg::create_log_span(String("inner"), Fields{{String("x"), Any(2.0)}});
    lg::enter_log_span(outer);
    lg::enter_log_span(inner);
    lg::log(LogLevel::Info, text("nested"));
    lg::exit_log_span(inner);
    lg::exit_log_span(outer);
    const auto& merged = *fields_of(r.entries[0]);
    check(num(merged, "x") == 2 && num(merged, "y") == 10 && keys(merged) == std::vector<std::string>{"msg", "x", "y"},
          "a newer span wins a shared key, which keeps its first position");
  }
  reset();
  {
    auto& r = recording();
    auto span = lg::create_log_span(String("empty"));
    lg::enter_log_span(span);
    Fields data{{String("k"), Any(1.0)}};
    lg::log(LogLevel::Info, text("unchanged"));
    lg::log(LogLevel::Info, record(data));
    lg::exit_log_span(span);
    check(is_text(r.entries[0], "unchanged"), "spans without fields leave the data unchanged");
    // Addition: with nothing to merge, the record is the caller's own, not a copy.
    check(fields_of(r.entries[1])->identity() == data.identity(), "and a record keeps its identity");
  }
  reset();
  {
    auto& r = recording();
    auto span = lg::create_log_span(String("record"), Fields{{String("inherited"), Any(1.0)}, {String("winner"), Any(String("span"))}});
    lg::enter_log_span(span);
    lg::log(LogLevel::Info, record(Fields{{String("direct"), Any(2.0)}, {String("winner"), Any(String("data"))}}));
    lg::exit_log_span(span);
    const auto& merged = *fields_of(r.entries[0]);
    check(num(merged, "direct") == 2 && num(merged, "inherited") == 1 && str(merged, "winner") == "data" &&
              keys(merged) == std::vector<std::string>{"inherited", "winner", "direct"},
          "record data wins over span fields, in spread order");
  }
  reset();
  {
    // Addition: a present undefined overrides, and stays present, distinct from a missing key.
    auto& r = recording();
    auto span = lg::create_log_span(String("s"), Fields{{String("x"), Any(1.0)}, {String("gone"), Any()}});
    lg::enter_log_span(span);
    lg::log(LogLevel::Info, record(Fields{{String("x"), Any()}}));
    lg::exit_log_span(span);
    const auto& merged = *fields_of(r.entries[0]);
    check(merged.has(String("x")) && merged.get(String("x")).value().is_undefined() && merged.has(String("gone")) &&
              merged.get(String("gone")).value().is_undefined() && !merged.has(String("missing")),
          "an undefined data field overrides a span value and stays present; a span's undefined field is copied too");
  }
  reset();
  {
    // Addition: { msg: data, ...spanFields } -- a span field named msg wins over the string.
    auto& r = recording();
    auto span = lg::create_log_span(String("s"), Fields{{String("msg"), Any(String("from span"))}});
    lg::enter_log_span(span);
    lg::log(LogLevel::Info, text("from data"));
    lg::exit_log_span(span);
    check(str(*fields_of(r.entries[0]), "msg") == "from span", "a span field named msg wins over string data");
  }
  reset();
  {
    auto& r = recording();
    auto phantom = lg::create_log_span(String("phantom"), Fields{{String("z"), Any(1.0)}});
    lg::exit_log_span(phantom);
    auto a = lg::create_log_span(String("a"), Fields{{String("a"), Any(1.0)}});
    auto b = lg::create_log_span(String("b"), Fields{{String("b"), Any(2.0)}});
    lg::enter_log_span(a);
    lg::enter_log_span(b);
    lg::exit_log_span(a);
    lg::log(LogLevel::Info, text("after-a-exit"));
    lg::exit_log_span(b);
    const auto& merged = *fields_of(r.entries[0]);
    check(num(merged, "b") == 2 && !merged.has(String("a")) && !merged.has(String("z")),
          "exiting an absent span is a no-op, and spans exit by identity, out of order");
  }
  {
    auto span = lg::create_log_span(String("op"));
    auto full = lg::create_log_span(String("render-frame"), Fields{{String("frame"), Any(1.0)}}, channel("perf"));
    check(span->name == String("op") && span->fields.size() == 0 && !span->channel.has_value() && !span->entity_runtime_key.has_value() &&
              full->channel == String("perf") && num(full->fields, "frame") == 1,
          "createLogSpan defaults fields to {} and channel to null, as an entity");
  }
}

void contexts() {
  reset();
  {
    auto ctx = lg::create_log_context(String("render"), Fields{{String("version"), Any(1.0)}});
    auto bare = lg::create_log_context(String("ch"));
    check(ctx->channel == String("render") && num(ctx->fields, "version") == 1 && bare->fields.size() == 0 &&
              !ctx->entity_runtime_key.has_value(),
          "createLogContext keeps the channel and fields, defaulting fields to {}");
    Fields shared{{String("a"), Any(1.0)}};
    auto byReference = lg::create_log_context(std::nullopt, shared);
    shared.set(String("later"), Any(2.0));
    check(byReference->fields.has(String("later")), "a context holds the caller's fields object, not a copy");
  }
  {
    auto parent = lg::create_log_context(String("chan"), Fields{{String("a"), Any(1.0)}, {String("b"), Any(2.0)}});
    auto child = lg::create_child_log_context(view(parent), Fields{{String("b"), Any(99.0)}, {String("c"), Any(3.0)}});
    check(num(child->fields, "a") == 1 && num(child->fields, "b") == 99 && num(child->fields, "c") == 3 &&
              keys(child->fields) == std::vector<std::string>{"a", "b", "c"} && num(parent->fields, "b") == 2,
          "a child merges parent and child fields, child winning, without touching the parent");
    auto inherit = lg::create_child_log_context(view(lg::create_log_context(String("parent"))), Fields{});
    auto named = lg::create_child_log_context(view(lg::create_log_context(String("parent"))), Fields{}, channel("child"));
    check(inherit->channel == String("parent") && named->channel == String("child"), "a child inherits the channel unless one is given");
    // Addition: `channel !== undefined` -- an explicit null clears the parent's channel.
    auto cleared = lg::create_child_log_context(view(lg::create_log_context(String("parent"))), Fields{}, Channel{std::optional<String>{}});
    check(!cleared->channel.has_value(), "an explicit null channel clears rather than inherits");
  }
  reset();
  {
    auto& r = recording();
    auto ctx = lg::create_log_context(String("ch"), Fields{{String("reqId"), Any(String("abc"))}});
    lg::log_debug_with(view(ctx), text("msg"));
    lg::log_error_with(view(ctx), text("fail"));
    lg::log_info_with(view(ctx), record(Fields{{String("extra"), Any(true)}}));
    lg::log_verbose_with(view(ctx), text("trace"));
    lg::log_warn_with(view(ctx), text("warning"));
    const LogLevel levels[] = {LogLevel::Debug, LogLevel::Error, LogLevel::Info, LogLevel::Verbose, LogLevel::Warn};
    bool shapes = r.entries.size() == 5;
    for (std::size_t i = 0; shapes && i < 5; ++i) shapes = r.entries[i]->level == levels[i] && r.entries[i]->channel == String("ch");
    check(shapes && keys(*fields_of(r.entries[0])) == std::vector<std::string>{"msg", "reqId"} && str(*fields_of(r.entries[0]), "msg") == "msg" &&
              keys(*fields_of(r.entries[2])) == std::vector<std::string>{"reqId", "extra"},
          "every *With level emits on the context's channel: string data as { msg, ...fields }, record data as { ...fields, ...data }");
  }
  reset();
  {
    auto& r = recording();
    lg::log_with(view(lg::create_log_context(String("render"), Fields{{String("frame"), Any(1.0)}})), LogLevel::Info, text("update"));
    lg::log_with(view(lg::create_log_context(std::nullopt, Fields{{String("base"), Any(1.0)}})), LogLevel::Debug,
                 record(Fields{{String("extra"), Any(2.0)}}));
    lg::log_with(view(lg::create_log_context(std::nullopt)), LogLevel::Info,
                 Data{std::in_place_index<0>, [] { return flight::types::LogData{std::in_place_index<1>, String("lazy")}; }});
    check(r.entries[0]->level == LogLevel::Info && r.entries[0]->channel == String("render") && str(*fields_of(r.entries[0]), "msg") == "update" &&
              num(*fields_of(r.entries[0]), "frame") == 1 && keys(*fields_of(r.entries[1])) == std::vector<std::string>{"base", "extra"} &&
              is_text(r.entries[2], "lazy"),
          "logWith emits at its level on the context's channel, merging fields and accepting a provider");
  }
  reset();
  {
    // Context over span over data: _mergeContextFields(context, _mergeSpanFields(resolved, channel)).
    auto& r = recording();
    auto span = lg::create_log_span(String("s"), Fields{{String("shared"), Any(String("span"))}, {String("s"), Any(1.0)}});
    lg::enter_log_span(span);
    lg::log_info_with(view(lg::create_log_context(std::nullopt, Fields{{String("shared"), Any(String("ctx"))}, {String("c"), Any(2.0)}})),
                      text("t"));
    lg::exit_log_span(span);
    const auto& merged = *fields_of(r.entries[0]);
    check(str(merged, "shared") == "span" && keys(merged) == std::vector<std::string>{"shared", "c", "msg", "s"},
          "span fields win over context fields, and data over both, in spread order");
  }
  reset();
  {
    auto& r = recording();
    lg::set_log_channel_level(String("quiet"), LogLevel::Error);
    lg::log_info_with(view(lg::create_log_context(String("quiet"), Fields{{String("k"), Any(1.0)}})), text("x"));
    check(r.entries.empty(), "the context's channel level gates its entries");
  }
}

void memory_sink() {
  reset();
  {
    auto handle = lg::create_memory_log_sink(2);
    lg::add_log_sink(handle->sink);
    auto& r = recording();
    lg::log(LogLevel::Info, text("first"));
    lg::log(LogLevel::Info, text("second"));
    lg::log(LogLevel::Info, text("third"));
    auto entries = lg::get_memory_log_sink_entries(handle);
    check(entries.size() == 2 && is_text(entries.element(0.0), "second") && is_text(entries.element(1.0), "third"),
          "the ring buffer keeps the newest entries, oldest first");
    check(entries.element(1.0) != r.entries[2] && entries.element(1.0)->level == r.entries[2]->level,
          "stored entries are copies of { level, channel, data }");
    lg::clear_memory_log_sink(handle);
    check(lg::get_memory_log_sink_entries(handle).size() == 0 && !handle->entity_runtime_key.has_value(), "clearing empties it");
  }
  reset();
  {
    auto handle = lg::create_memory_log_sink(3);
    lg::add_log_sink(handle->sink);
    for (int i = 0; i < 5; ++i) lg::log(LogLevel::Info, text(("msg" + std::to_string(i)).c_str()));
    auto entries = lg::get_memory_log_sink_entries(handle);
    check(entries.size() == 3 && is_text(entries.element(0.0), "msg2") && is_text(entries.element(2.0), "msg4"), "oldest-first after wrapping");
    auto unknown = flight::make_ref<flight::types::MemoryLogSink>();
    lg::clear_memory_log_sink(unknown);
    check(lg::get_memory_log_sink_entries(unknown).size() == 0, "an unknown handle has no entries");
  }
}

Ref<lg::LogSink> collector(std::vector<Ref<LogEntry>>& out) {
  return Ref<lg::LogSink>([&out](Ref<LogEntry> entry) { out.push_back(entry); });
}

void buffered_sink() {
  using Options = lg::size_interval_ms_014eb73adb446976;
  const auto options = [](std::optional<double> size, std::optional<double> interval) {
    return flight::make_ref<Options>(Options{.size = size, .interval_ms = interval});
  };
  reset();
  {
    std::vector<Ref<LogEntry>> forwarded;
    auto handle = lg::create_buffered_log_sink(collector(forwarded));
    handle->sink(flight::make_ref<LogEntry>(LogEntry{.level = LogLevel::Info, .channel = std::nullopt, .data = flight::types::LogData{std::in_place_index<1>, String("queued")}}));
    check(forwarded.empty(), "with the defaults (size 100, interval 1000) an entry waits");
    lg::flush_log_sink(handle);
    check(forwarded.size() == 1 && is_text(forwarded[0], "queued"), "flushLogSink delivers it");
    lg::dispose_log_sink(handle);
  }
  reset();
  {
    std::vector<Ref<LogEntry>> forwarded;
    auto handle = lg::create_buffered_log_sink(collector(forwarded), options(100, 0));
    lg::flush_log_sink(handle);
    check(forwarded.empty(), "flushing an empty buffer delivers nothing");
    lg::add_log_sink(handle->sink);
    auto& r = recording();
    lg::log(LogLevel::Info, text("queued"));
    check(forwarded.empty(), "entries are held until flushed");
    lg::flush_log_sink(handle);
    check(forwarded.size() == 1 && forwarded[0] != r.entries[0] && is_text(forwarded[0], "queued"), "and forwarded as copies");
  }
  reset();
  {
    std::vector<Ref<LogEntry>> forwarded;
    auto handle = lg::create_buffered_log_sink(collector(forwarded), options(2, 0));
    lg::add_log_sink(handle->sink);
    lg::log(LogLevel::Info, text("a"));
    const auto beforeSize = forwarded.size();
    lg::log(LogLevel::Info, text("b"));
    check(beforeSize == 0 && forwarded.size() == 2 && is_text(forwarded[0], "a") && is_text(forwarded[1], "b"),
          "nothing is forwarded below size, and reaching size flushes everything, in order");
  }
  reset();
  {
    std::vector<Ref<LogEntry>> forwarded;
    auto handle = lg::create_buffered_log_sink(collector(forwarded), options(100, 0));
    lg::add_log_sink(handle->sink);
    lg::log(LogLevel::Info, text("pending"));
    lg::dispose_log_sink(handle);
    check(forwarded.size() == 1, "disposeLogSink flushes");
    lg::dispose_log_sink(flight::make_ref<flight::types::BufferedLogSink>());
    lg::flush_log_sink(flight::make_ref<flight::types::BufferedLogSink>());
    check(true, "dispose and flush ignore an unknown handle");
  }
  reset();
  {
    std::vector<Ref<LogEntry>> forwarded;
    auto handle = lg::create_buffered_log_sink(collector(forwarded), options(100, 5));
    lg::add_log_sink(handle->sink);
    lg::log(LogLevel::Info, text("ticked"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    static_cast<void>(flight::host::pump_timers());
    check(forwarded.size() == 1, "the interval delivers buffered entries");
    lg::dispose_log_sink(handle);
    lg::log(LogLevel::Info, text("after dispose"));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    static_cast<void>(flight::host::pump_timers());
    check(forwarded.size() == 1, "disposing stops the interval");
  }
}

void rate_limited_sink() {
  using Options = lg::per_channel_max_per_interval_interval_ms_b055d1b6af9da40a;
  reset();
  {
    std::vector<Ref<LogEntry>> forwarded;
    auto handle = lg::create_rate_limited_log_sink(collector(forwarded), flight::make_ref<Options>(Options{.max_per_interval = 2, .interval_ms = 1e9}));
    lg::add_log_sink(handle->sink);
    lg::log(LogLevel::Info, text("a"));
    lg::log(LogLevel::Info, text("b"));
    lg::log(LogLevel::Info, text("c"));
    check(forwarded.size() == 2 && !handle->entity_runtime_key.has_value(), "entries beyond the budget are dropped");
  }
  reset();
  {
    std::vector<Ref<LogEntry>> forwarded;
    auto handle = lg::create_rate_limited_log_sink(collector(forwarded),
                                                   flight::make_ref<Options>(Options{.per_channel = true, .max_per_interval = 1, .interval_ms = 1e9}));
    lg::add_log_sink(handle->sink);
    lg::log(LogLevel::Info, text("a"), channel("ch1"));
    lg::log(LogLevel::Info, text("b"), channel("ch1"));
    lg::log(LogLevel::Info, text("c"), channel("ch2"));
    // Addition: the null channel and the empty channel are different keys.
    lg::log(LogLevel::Info, text("d"));
    lg::log(LogLevel::Info, text("e"), channel(""));
    check(forwarded.size() == 4 && forwarded[0]->channel == String("ch1") && forwarded[1]->channel == String("ch2") &&
              !forwarded[2]->channel.has_value() && forwarded[3]->channel == String(""),
          "perChannel keeps a budget per channel, null and '' apart");
  }
  reset();
  {
    std::vector<Ref<LogEntry>> forwarded;
    auto handle = lg::create_rate_limited_log_sink(collector(forwarded), flight::make_ref<Options>(Options{.max_per_interval = 1, .interval_ms = 20}));
    lg::add_log_sink(handle->sink);
    lg::log(LogLevel::Info, text("first"));
    lg::log(LogLevel::Info, text("suppressed"));
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    lg::log(LogLevel::Info, text("next-window"));
    check(forwarded.size() == 2 && is_text(forwarded[0], "first") && is_text(forwarded[1], "next-window"), "the budget resets after the interval");
  }
}

void text_formatter() {
  using Options = lg::indent_groups_level_prefix_timestamp_85ef46f909e535f5;
  const auto entry = [](LogLevel level, std::optional<String> ch, flight::types::LogData data) {
    return view(flight::make_ref<LogEntry>(LogEntry{.level = level, .channel = ch, .data = std::move(data)}));
  };
  const auto textData = [](const char* t) { return flight::types::LogData{std::in_place_index<1>, String(t)}; };
  reset();
  auto plain = lg::create_text_log_formatter();
  check(plain(entry(LogLevel::Warn, String("batch"), textData("test"))) == String("[batch] test"), "channel then message");
  auto prefixed = lg::create_text_log_formatter(flight::make_ref<Options>(Options{.level_prefix = true}));
  check(prefixed(entry(LogLevel::Error, String("ch"), textData("boom"))) == String("error [ch] boom"), "the level prefix");
  check(prefixed(entry(static_cast<LogLevel>(99), std::nullopt, textData("message"))) == String("unknown [flight] message"),
        "an unknown level is labelled unknown, and no channel is [flight]");
  check(plain(entry(LogLevel::Info, std::nullopt, flight::types::LogData{std::in_place_index<0>, Fields{{String("value"), Any(3.0)}}})) ==
            String("[flight] {\"value\":3}"),
        "record data is JSON");
  const auto stamped = lg::create_text_log_formatter(flight::make_ref<Options>(Options{.timestamp = true}))(entry(LogLevel::Info, std::nullopt, textData("x"))).to_utf8();
  const auto space = stamped.find(' ');
  const auto dot = stamped.find('.');
  check(stamped.rfind("t=", 0) == 0 && dot != std::string::npos && dot < space && space - dot == 3 && stamped.substr(space) == " [flight] x",
        "the timestamp prefix is t= with two decimals");
  lg::begin_log_group(String("outer"));
  auto indented = lg::create_text_log_formatter(flight::make_ref<Options>(Options{.indent_groups = true}));
  check(indented(entry(LogLevel::Info, std::nullopt, textData("inside"))) == String("[flight]    inside"), "an open group indents");
  // Addition: indentGroups is tested for its value, not its presence.
  auto notIndented = lg::create_text_log_formatter(flight::make_ref<Options>(Options{.indent_groups = false}));
  check(notIndented(entry(LogLevel::Info, std::nullopt, textData("inside"))) == String("[flight] inside"), "indentGroups: false does not indent");
  lg::clear_log_groups();
}

void serialize_errors() {
  {
    auto result = lg::serialize_log_error(Any::object(flight::make_ref<flight::Error>(String("without stack"))));
    check(str(result, "name") == "Error" && str(result, "message") == "without stack" && keys(result) == std::vector<std::string>{"name", "message"},
          "an Error serializes to name and message, omitting the stack it does not have");
  }
  {
    auto inner = flight::make_ref<flight::Error>(String("inner"));
    auto outer = flight::make_ref<flight::Error>(String("outer"), flight::ErrorOptions{.cause = Any::object(inner)});
    auto result = lg::serialize_log_error(Any::object(outer));
    const auto stored = result.get(String("cause"));
    const auto* cause = stored.has_value() ? stored->external_if<Fields>() : nullptr;
    check(cause != nullptr && str(*cause, "message") == "inner" && keys(result) == std::vector<std::string>{"name", "message", "cause"},
          "the cause chain is serialized recursively");
  }
  {
    // Addition: Error.name is the instance's, so a subclass keeps its own.
    auto result = lg::serialize_log_error(Any::object(flight::make_ref<flight::RangeError>(String("out"))));
    check(str(result, "name") == "RangeError", "a subclass keeps its name");
  }
  check(str(lg::serialize_log_error(Any(String("oops"))), "value") == "oops" && str(lg::serialize_log_error(Any(42.0)), "value") == "42" &&
            str(lg::serialize_log_error(Any()), "value") == "undefined",
        "a non-Error value is wrapped as { value: String(value) }");
}

void groups_and_signals() {
  reset();
  {
    auto& r = recording();
    lg::begin_log_group(String("outer"));
    lg::begin_log_group(String("inner"), channel("g"));
    lg::end_log_group();
    lg::end_log_group();
    lg::end_log_group();
    check(r.entries.size() == 4 && r.entries[0]->level == LogLevel::Debug && str(*fields_of(r.entries[0]), "group") == "begin" &&
              num(*fields_of(r.entries[0]), "depth") == 1 && str(*fields_of(r.entries[0]), "msg") == "outer" &&
              num(*fields_of(r.entries[1]), "depth") == 2 && r.entries[1]->channel == String("g") &&
              str(*fields_of(r.entries[2]), "group") == "end" && num(*fields_of(r.entries[2]), "depth") == 2 && num(*fields_of(r.entries[3]), "depth") == 1,
          "groups emit Debug begin/end entries with their depth, and an end with none open is a no-op");
    lg::set_log_level(LogLevel::Info);
    lg::begin_log_group(String("quiet"));
    check(r.entries.size() == 4, "a suppressed begin tracks depth without emitting");
    lg::set_log_level(LogLevel::Verbose);
    lg::end_log_group();
    check(r.entries.size() == 5 && num(*fields_of(r.entries[4]), "depth") == 1, "and the matching end closes it");
  }
  reset();
  {
    auto signals = lg::enable_log_signals();
    check(lg::enable_log_signals() == signals && !signals->entity_runtime_key.has_value(), "enableLogSignals returns one entity");
    using Row = flight::StructuralRef<flight::RowReadonly<flight::RowOf<Ref<LogEntry>>>>;
    int all = 0, errors = 0;
    flight::signals::connect_signal(signals->on_log_entry, std::function<void(Row)>([&](Row) { ++all; }), std::nullopt);
    flight::signals::connect_signal(signals->on_log_error, std::function<void(Row)>([&](Row) { ++errors; }), std::nullopt);
    lg::log(LogLevel::Info, text("x"));
    lg::log(LogLevel::Error, text("e"));
    check(all == 2 && errors == 1, "onLogEntry fires for every entry, onLogError for errors only, even with no sink installed");
  }
}

}  // namespace

// The JSON envelope without its clock reading: `"t":<number>,` is removed so the rest compares exactly.
std::string without_t(const String& line) {
  std::string out = line.to_utf8();
  const auto at = out.find("\"t\":");
  if (at == std::string::npos) return out;
  const auto end = out.find(',', at);
  return end == std::string::npos ? out : out.erase(at, end - at + 1);
}

Any nested(Fields fields) { return Any::external(std::move(fields)); }

void json_formatter() {
  const auto entry = [](LogLevel level, std::optional<String> ch, flight::types::LogData data) {
    return flight::make_ref<LogEntry>(LogEntry{.level = level, .channel = ch, .data = std::move(data)});
  };
  const auto data = [](Fields f) { return flight::types::LogData{std::in_place_index<0>, std::move(f)}; };
  reset();
  auto fmt = lg::create_json_log_formatter();
  {
    const String line = fmt(entry(LogLevel::Info, String("test"), flight::types::LogData{std::in_place_index<1>, String("msg")}));
    check(without_t(line) == R"({"__flight":true,"level":"info","channel":"test","data":{"msg":"msg"}})",
          "produces a __flight JSON envelope, in the literal's key order");
    check(line.to_utf8().find("\"t\":") == 17, "the envelope carries the timestamp as t, after __flight");
  }
  // Addition: an absent channel and an unknown level are undefined, and JSON.stringify omits them.
  check(without_t(fmt(entry(static_cast<LogLevel>(99), std::nullopt, data(Fields{{String("a"), Any(1.0)}})))) ==
            R"({"__flight":true,"data":{"a":1}})",
        "an absent channel and an unknown level are omitted, not written as null");

  reset();
  lg::register_log_serializer(String("acme.Widget"), [](Any value) {
    const auto* source = value.external_if<Fields>();
    Fields out;
    out.set(String("serialized"), Any(true));
    out.set(String("id"), source != nullptr ? source->get(String("id")).value_or(Any()) : Any());
    return out;
  });
  check(without_t(fmt(entry(LogLevel::Info, std::nullopt,
                            data(Fields{{String("widget"), nested(Fields{{String("__kind"), Any(String("acme.Widget"))}, {String("id"), Any(42.0)}})}})))) ==
            R"({"__flight":true,"level":"info","data":{"widget":{"serialized":true,"id":42}}})",
        "applies registered serializers to values with matching __kind");
  {
    lg::register_log_serializer(String("acme.Known"), [](Any) { return Fields{{String("serialized"), Any(true)}}; });
    Fields input{{String("missingKind"), nested(Fields{{String("value"), Any(1.0)}})},
                 {String("nullValue"), Any(nullptr)},
                 {String("numericKind"), nested(Fields{{String("__kind"), Any(7.0)}, {String("value"), Any(2.0)}})},
                 {String("primitive"), Any(3.0)},
                 {String("kindString"), Any(String("acme.Known"))},
                 {String("unknownKind"), nested(Fields{{String("__kind"), Any(String("acme.Unknown"))}, {String("value"), Any(4.0)}})}};
    check(without_t(fmt(entry(LogLevel::Info, std::nullopt, data(input)))) ==
              R"({"__flight":true,"level":"info","data":{"missingKind":{"value":1},"nullValue":null,"numericKind":{"__kind":7,"value":2},"primitive":3,"kindString":"acme.Known","unknownKind":{"__kind":"acme.Unknown","value":4}}})",
          "preserves values that do not match a registered serializer, in order");
  }
  lg::clear_log_serializers();
  check(without_t(fmt(entry(LogLevel::Info, std::nullopt, data(Fields{{String("obj"), nested(Fields{{String("__kind"), Any(String("acme.Widget"))}})}})))) ==
            R"({"__flight":true,"level":"info","data":{"obj":{"__kind":"acme.Widget"}}})",
        "clearLogSerializers removes every registration");

  reset();
  lg::set_log_redaction_paths(flight::Array<String>{String("credentials.token"), String("password")});
  {
    Fields credentials{{String("token"), Any(String("secret"))}, {String("user"), Any(String("alice"))}};
    Fields original{{String("credentials"), nested(credentials)}, {String("password"), Any(String("pw"))}};
    check(without_t(fmt(entry(LogLevel::Info, std::nullopt, data(original)))) ==
              R"({"__flight":true,"level":"info","data":{"credentials":{"token":"[REDACTED]","user":"alice"},"password":"[REDACTED]"}})",
          "applies redaction paths to top-level and nested fields");
    check(str(original, "password") == "pw" && str(credentials, "token") == "secret" &&
              original.get(String("credentials"))->external_if<Fields>() != nullptr &&
              str(*original.get(String("credentials"))->external_if<Fields>(), "token") == "secret",
          "redaction does not mutate the caller's record or its nested records (alias-safe)");
  }
  lg::set_log_redaction_paths(flight::Array<String>{String("missing.secret"), String("nullValue.secret"), String("primitive.secret"), String("items.secret")});
  {
    Fields input{{String("items"), Any(flight::Array<Any>{Any(String("secret"))})}, {String("nullValue"), Any(nullptr)}, {String("primitive"), Any(7.0)}};
    std::string line;
    try {
      line = without_t(fmt(entry(LogLevel::Info, std::nullopt, data(input))));
    } catch (const std::exception&) {
      line = "<threw>";
    }
    check(line == R"({"__flight":true,"level":"info","data":{"items":["secret"],"nullValue":null,"primitive":7}})",
          "leaves absent and non-record path segments unchanged");
  }
  lg::clear_log_redaction_paths();
  check(without_t(fmt(entry(LogLevel::Info, std::nullopt, data(Fields{{String("token"), Any(String("secret"))}})))) ==
            R"({"__flight":true,"level":"info","data":{"token":"secret"}})",
        "clearLogRedactionPaths stops redaction");
}

// upstream's createTestLogTransport: every operation is recorded and overridable.
struct TestTransport {
  std::vector<std::string> lines;
  std::vector<std::string> events;
  flight::types::LogTransportWriteOutcome write_outcome{flight::make_ref<flight::types::reason_43a745d20647bfb6>(flight::types::reason_43a745d20647bfb6{.reason = String("accepted")})};
  std::function<flight::Task<flight::types::LogTransportFlushOutcome>()> flush;
  std::function<flight::Task<flight::types::LogTransportDestroyOutcome>()> destroy;
};

using FlushOutcome = flight::types::LogTransportFlushOutcome;
using DestroyOutcome = flight::types::LogTransportDestroyOutcome;
using Plain = flight::types::reason_43a745d20647bfb6;
using Failed = flight::types::reason_message_d46086b32265b61f;
using Drained = flight::types::reason_delivery_3724413a85240c26;

Ref<flight::types::LogTransport> transport_of(TestTransport& t) {
  auto* p = &t;
  if (!t.flush) t.flush = [p] { p->events.push_back("flush"); return flight::Task<FlushOutcome>::resolve(FlushOutcome{flight::make_ref<Drained>(Drained{.reason = String("drained"), .delivery = String("operating-system")})}); };
  if (!t.destroy) t.destroy = [p] { p->events.push_back("destroy"); return flight::Task<DestroyOutcome>::resolve(DestroyOutcome{flight::make_ref<Plain>(Plain{.reason = String("destroyed")})}); };
  return flight::make_ref<flight::types::LogTransport>(flight::types::LogTransport{
      .write = [p](String line) { p->lines.push_back(line.to_utf8()); return p->write_outcome; },
      .flush = [p] { return p->flush(); },
      .destroy = [p] { return p->destroy(); }});
}

template <typename Outcome>
std::string reason_of(const Outcome& outcome) {
  return std::visit([](const auto& v) { return v->reason.to_utf8(); }, outcome);
}

std::string message_of(const FlushOutcome& outcome) {
  const auto* failed = std::get_if<Ref<Failed>>(&outcome);
  return failed != nullptr ? (*failed)->message.to_utf8() : std::string("<none>");
}
std::string message_of_destroy(const DestroyOutcome& outcome) {
  const auto* failed = std::get_if<Ref<Failed>>(&outcome);
  return failed != nullptr ? (*failed)->message.to_utf8() : std::string("<none>");
}

void file_sink() {
  const auto executor = std::make_shared<flight::QueueExecutor>();
  const flight::ExecutorScope scope(executor);
  using FormatterOptions = lg::formatter_c3f72d72feb278c6;
  reset();
  for (const char* admission : {"accepted", "backpressure", "operation-failed", "destroyed"}) {
    TestTransport t;
    t.write_outcome = flight::types::LogTransportWriteOutcome{flight::make_ref<Plain>(Plain{.reason = String(admission)})};
    auto handle = lg::create_file_log_sink(transport_of(t));
    lg::add_log_sink(handle->sink);
    lg::log(LogLevel::Info, text("file-entry"));
    check(t.lines.size() == 1 && without_t(String(t.lines[0].c_str())) == std::string(R"({"__flight":true,"level":"info","data":{"msg":"file-entry"}})") + "\n",
          std::string("offers one complete JSON line when admission reports ") + admission);
    lg::clear_log_sinks();
  }
  {
    TestTransport t;
    auto handle = lg::create_file_log_sink(transport_of(t));
    check(!handle->entity_runtime_key.has_value(), "creates an Entity handle with an undefined runtime key");
  }
  {
    TestTransport t;
    auto handle = lg::create_file_log_sink(transport_of(t), flight::make_ref<FormatterOptions>(FormatterOptions{.formatter = [](auto) { return String("custom"); }}));
    lg::add_log_sink(handle->sink);
    lg::log(LogLevel::Info, text("x"));
    check(t.lines == std::vector<std::string>{"custom\n"}, "accepts a custom formatter");
    lg::clear_log_sinks();
  }
  {
    TestTransport a, b;
    auto first = lg::create_file_log_sink(transport_of(a), flight::make_ref<FormatterOptions>(FormatterOptions{.formatter = [](auto) { return String("first"); }}));
    auto second = lg::create_file_log_sink(transport_of(b), flight::make_ref<FormatterOptions>(FormatterOptions{.formatter = [](auto) { return String("second"); }}));
    const auto e = flight::make_ref<LogEntry>(LogEntry{.level = LogLevel::Info, .channel = std::nullopt, .data = flight::types::LogData{std::in_place_index<1>, String("entry")}});
    first->sink(e);
    second->sink(e);
    check(a.lines == std::vector<std::string>{"first\n"} && b.lines == std::vector<std::string>{"second\n"}, "pins each handle to its injected transport");
  }

  // destroyFileLogSink: flush is awaited before destroy, and both outcomes are reported.
  {
    TestTransport t;
    std::optional<flight::Task<FlushOutcome>::Resolver> finish;
    auto pending = flight::Task<FlushOutcome>::create([&finish](auto resolve) { finish.emplace(resolve); });
    t.flush = [&t, pending] { t.events.push_back("flush-start"); return pending; };
    auto handle = lg::create_file_log_sink(transport_of(t));
    lg::add_log_sink(handle->sink);
    auto closing = lg::destroy_file_log_sink(handle);
    executor->run_until_idle();
    check(t.events == std::vector<std::string>{"flush-start"} && !closing.is_ready(), "destroy waits for flush to settle");
    lg::log(LogLevel::Info, text("late"));
    handle->sink(flight::make_ref<LogEntry>(LogEntry{.level = LogLevel::Info, .channel = std::nullopt, .data = flight::types::LogData{std::in_place_index<1>, String("direct")}}));
    check(t.lines.empty(), "a destroying sink is removed and writes nothing, even when called directly");
    const FlushOutcome drained{flight::make_ref<Drained>(Drained{.reason = String("drained"), .delivery = String("operating-system")})};
    (*finish)(drained);
    const auto outcome = closing.get();
    check(t.events == std::vector<std::string>{"flush-start", "destroy"}, "destroy runs after flush");
    check(outcome->reason == String("destroyed") && reason_of(outcome->flush) == "drained" && reason_of(outcome->destroy) == "destroyed" &&
              std::get_if<Ref<Drained>>(&outcome->flush) != nullptr && std::get<Ref<Drained>>(outcome->flush) == std::get<Ref<Drained>>(drained),
          "the outcome carries the transport's own flush and destroy outcomes");

    // Addition: a repeat call shares the first teardown and spreads its outcomes under already-destroyed.
    std::optional<Ref<flight::types::FileLogSinkDestroyOutcome>> again;
    try {
      again = lg::destroy_file_log_sink(handle).get();
    } catch (const std::exception&) {
    }
    const auto same = [](const auto& left, const auto& right) {
      return left.index() == right.index() && std::visit([&right](const auto& l) { return std::get_if<std::remove_cvref_t<decltype(l)>>(&right) != nullptr && *std::get_if<std::remove_cvref_t<decltype(l)>>(&right) == l; }, left);
    };
    check(again.has_value() && (*again)->reason == String("already-destroyed") && *again != outcome &&
              same((*again)->flush, outcome->flush) && same((*again)->destroy, outcome->destroy),
          "a repeat destroy returns a new outcome with the first teardown's flush and destroy");
    check(t.events.size() == 2, "a repeat destroy does not flush or destroy the transport again");
    check(!lg::remove_log_sink(handle->sink), "destroy unregisters the sink, so a later removal finds nothing");
  }
  {
    TestTransport t;
    t.flush = [&t] { t.events.push_back("flush"); return flight::Task<FlushOutcome>::resolve(FlushOutcome{flight::make_ref<Failed>(Failed{.reason = String("operation-failed"), .message = String("disk full")})}); };
    auto outcome = lg::destroy_file_log_sink(lg::create_file_log_sink(transport_of(t))).get();
    check(outcome->reason == String("operation-failed") && message_of(outcome->flush) == "disk full" && reason_of(outcome->destroy) == "destroyed" &&
              t.events == std::vector<std::string>{"flush", "destroy"},
          "attempts destroy and preserves both outcomes when flush reports failure");
  }
  // Addition: a destroy that reports a message-bearing outcome is classified by its reason, not its shape.
  {
    TestTransport t;
    t.destroy = [] { return flight::Task<DestroyOutcome>::resolve(DestroyOutcome{flight::make_ref<Failed>(Failed{.reason = String("closed"), .message = String("ok")})}); };
    auto outcome = lg::destroy_file_log_sink(lg::create_file_log_sink(transport_of(t))).get();
    check(outcome->reason == String("destroyed"), "only an operation-failed reason fails the teardown");
  }
  {
    TestTransport t;
    t.flush = []() -> flight::Task<FlushOutcome> { throw flight::Error(String("flush threw")); };
    t.destroy = [] { return flight::Task<DestroyOutcome>::reject(Any(String("destroy rejected"))); };
    auto outcome = lg::destroy_file_log_sink(lg::create_file_log_sink(transport_of(t))).get();
    check(outcome->reason == String("operation-failed") && reason_of(outcome->flush) == "operation-failed" && message_of(outcome->flush) == "flush threw" &&
              reason_of(outcome->destroy) == "operation-failed" && message_of_destroy(outcome->destroy) == "destroy rejected",
          "a thrown Error and a rejected value both become operation-failed with String(reason) as the message");
  }
  {
    TestTransport a, b;
    auto first = lg::create_file_log_sink(transport_of(a));
    auto second = lg::create_file_log_sink(transport_of(b));
    lg::add_log_sink(first->sink);
    lg::add_log_sink(second->sink);
    static_cast<void>(lg::destroy_file_log_sink(first).get());
    lg::log(LogLevel::Info, text("after"));
    check(a.lines.empty() && b.lines.size() == 1, "keeps a successor destination live when an older handle is destroyed");
    lg::clear_log_sinks();
  }
  // Addition: a destroyed handle no longer holds its transport (the source sets it to null).
  {
    TestTransport t;
    auto token = std::make_shared<int>(0);
    auto transport = transport_of(t);
    transport->write = [token](String) { return flight::types::LogTransportWriteOutcome{flight::make_ref<Plain>(Plain{.reason = String("accepted")})}; };
    auto handle = lg::create_file_log_sink(transport);
    transport = flight::make_ref<flight::types::LogTransport>(flight::types::LogTransport{});
    const auto held = token.use_count();
    static_cast<void>(lg::destroy_file_log_sink(handle).get());
    check(held == 2 && token.use_count() == 1, "a destroyed handle releases its transport");
  }
  {
    bool threw = false;
    try {
      static_cast<void>(lg::destroy_file_log_sink(flight::make_ref<flight::types::FileLogSink>(flight::types::FileLogSink{})).get());
    } catch (const flight::Error& error) {
      threw = error.message() == String("destroyFileLogSink received an unknown handle");
    }
    check(threw, "an unknown handle throws");
  }
}

int main() {
  level_functions();
  spans();
  contexts();
  memory_sink();
  buffered_sink();
  rate_limited_sink();
  text_formatter();
  serialize_errors();
  groups_and_signals();
  json_formatter();
  file_sink();
  return failures == 0 ? 0 : 1;
}
