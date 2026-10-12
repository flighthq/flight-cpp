#include <flight/log/log.hpp>

#include <functional>
#include <iostream>
#include <string>
#include <variant>

// Native port of the parts of packages/log/src/log.test.ts (a627849) whose code paths exist in the
// log-module-qualification-and-operators override WITHOUT reaching one of its throwing stubs.
//
// Deliberately NOT ported: every case that needs an entry to pass the level gate through log() or a
// severity wrapper. Those reach merge_span_fields, a NotImplementedInThisProfile stub, so they throw;
// see that override's `remaining`. Also not ported: createMemoryLogSink, createLogContext and createLogSpan,
// which construct through initialize_log_entity, another NotImplementedInThisProfile stub; and every
// function the override does not define (its header lists them, e.g. the *With wrappers, the formatters,
// createBufferedLogSink, createRateLimitedLogSink, serializeLogError, enableLogSignals). Sink combinators are therefore driven through emit_to_sinks, the step
// log() performs after building the entry, which is how generated_log_test.cpp exercises sinks too.

namespace {

using flight::String;
using flight::types::LogEntry;
using flight::types::LogLevel;
namespace log = flight::log;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

// The suite's beforeEach.
void reset() {
  log::clear_log_once_keys();
  log::clear_log_sinks();
  log::clear_log_channel_levels();
  log::set_log_console_level(LogLevel::Info);
  log::set_log_level(LogLevel::Verbose);
}

flight::Ref<LogEntry> entry(LogLevel level, const char* text) {
  return flight::make_ref<LogEntry>(LogEntry{.level = level, .channel = std::nullopt, .data = flight::types::LogData{String(text)}});
}

String text_of(const flight::Ref<LogEntry>& value) { return std::get<String>(value->data); }

void test_levels() {
  reset();
  log::set_log_level(LogLevel::Error);
  check(log::get_log_level() == LogLevel::Error, "getLogLevel returns the current global emit level");

  reset();
  log::set_log_console_level(LogLevel::Verbose);
  check(log::get_log_console_level() == LogLevel::Verbose, "getLogConsoleLevel returns the current threshold");

  check(log::get_log_level_name(LogLevel::None) == String("none") && log::get_log_level_name(LogLevel::Error) == String("error") &&
            log::get_log_level_name(LogLevel::Warn) == String("warn") && log::get_log_level_name(LogLevel::Info) == String("info") &&
            log::get_log_level_name(LogLevel::Debug) == String("debug") && log::get_log_level_name(LogLevel::Verbose) == String("verbose"),
        "getLogLevelName returns the canonical lowercase name for each level");
  check(log::get_log_level_name(static_cast<LogLevel>(99)) == String("unknown"),
        "getLogLevelName returns unknown for an unrecognized numeric level");

  check(log::parse_log_level(String("error")) == LogLevel::Error && log::parse_log_level(String("warn")) == LogLevel::Warn &&
            log::parse_log_level(String("info")) == LogLevel::Info && log::parse_log_level(String("debug")) == LogLevel::Debug &&
            log::parse_log_level(String("verbose")) == LogLevel::Verbose && log::parse_log_level(String("none")) == LogLevel::None,
        "parseLogLevel parses canonical names to their LogLevel values");
  check(log::parse_log_level(String("ERROR")) == LogLevel::Error && log::parse_log_level(String("Warn")) == LogLevel::Warn,
        "parseLogLevel is case-insensitive");
  check(!log::parse_log_level(String("unknown")).has_value() && !log::parse_log_level(String("")).has_value(),
        "parseLogLevel returns null for unknown names");
}

void test_channel_levels() {
  reset();
  check(!log::get_log_channel_level(String("unknown")).has_value(), "getLogChannelLevel returns null when no channel level is set");
  log::set_log_channel_level(String("render"), LogLevel::Error);
  check(log::get_log_channel_level(String("render")) == LogLevel::Error, "getLogChannelLevel returns the set level for a known channel");

  reset();
  log::set_log_channel_level(String("render"), LogLevel::Error);
  log::set_log_channel_level(String("input"), LogLevel::Warn);
  log::clear_log_channel_level(String("render"));
  check(!log::get_log_channel_level(String("render")).has_value() && log::get_log_channel_level(String("input")) == LogLevel::Warn,
        "clearLogChannelLevel removes only the named channel override");

  reset();
  log::set_log_channel_level(String("render"), LogLevel::Error);
  log::clear_log_channel_levels();
  check(!log::get_log_channel_level(String("render")).has_value(), "clearLogChannelLevels removes all per-channel level overrides");
}

// The suppressed halves of the upstream gate cases. A suppressed call returns before merge_span_fields,
// so it is the real code path; the emitted halves are the stub-blocked ones and are not asserted.
void test_gates() {
  int sink_calls = 0;
  flight::types::LogSink recording = [&](flight::Ref<LogEntry>) { ++sink_calls; };

  reset();
  log::add_log_sink(recording);
  log::set_log_level(LogLevel::Error);
  log::log(LogLevel::Info, flight::make_ref<flight::types::LogData>(flight::types::LogData{String("suppressed")}));
  check(sink_calls == 0, "setLogLevel suppresses entries below the global level");

  reset();
  log::add_log_sink(recording);
  log::set_log_level(LogLevel::None);
  log::log(LogLevel::None, flight::make_ref<flight::types::LogData>(flight::types::LogData{String("should not emit")}));
  check(sink_calls == 0, "LogLevel.None always suppresses (never-emit guard)");

  reset();
  log::add_log_sink(recording);
  log::set_log_channel_level(String("render"), LogLevel::Error);
  log::log(LogLevel::Info, flight::make_ref<flight::types::LogData>(flight::types::LogData{String("info on render channel")}),
           std::optional<String>(String("render")));
  check(sink_calls == 0, "setLogChannelLevel suppresses entries below the channel level");

  // 'does not evaluate any provider when its level is suppressed', over the severity wrappers this override
  // has; the *With variants are absent from it (its header lists them as not generated).
  reset();
  log::add_log_sink(recording);
  log::set_log_level(LogLevel::None);
  int provider_calls = 0;
  const auto provider = [&]() {
    return flight::make_ref<flight::types::LogDataProvider>([&]() -> flight::types::LogData {
      ++provider_calls;
      return String("value");
    });
  };
  log::log_assert(false, provider());
  log::log_debug(provider());
  log::log_error(provider());
  log::log_info(provider());
  log::log_verbose(provider());
  log::log_warn(provider());
  check(provider_calls == 0 && sink_calls == 0, "severity wrappers do not evaluate any provider when their level is suppressed");
}

// Sink combinators, driven through emit_to_sinks with the upstream inputs and expected counts.
void test_sink_combinators() {
  reset();
  int a = 0;
  int b = 0;
  auto fanout = log::create_fanout_log_sink(flight::Array<flight::Ref<flight::types::LogSink>>{
      flight::types::LogSink([&](flight::Ref<LogEntry>) { ++a; }), flight::types::LogSink([&](flight::Ref<LogEntry>) { ++b; })});
  log::add_log_sink(fanout);
  log::emit_to_sinks(entry(LogLevel::Info, "hello"));
  check(a == 1 && b == 1, "createFanoutLogSink forwards each entry to all component sinks");

  reset();
  flight::Array<flight::Ref<LogEntry>> kept;
  auto filtered = log::create_filter_log_sink(flight::types::LogSink([&](flight::Ref<LogEntry> value) { kept.push(value); }),
                                              [](flight::Ref<LogEntry> value) { return value->level == LogLevel::Error; });
  log::add_log_sink(filtered);
  log::emit_to_sinks(entry(LogLevel::Info, "skip"));
  log::emit_to_sinks(entry(LogLevel::Error, "keep"));
  check(kept.size() == 1 && kept[0]->level == LogLevel::Error, "createFilterLogSink only forwards entries matching the predicate");

  reset();
  int sampled_calls = 0;
  auto sampled = log::create_sampled_log_sink(flight::types::LogSink([&](flight::Ref<LogEntry>) { ++sampled_calls; }), 3.0);
  log::add_log_sink(sampled);
  for (int index = 0; index < 9; ++index) log::emit_to_sinks(entry(LogLevel::Info, "msg"));
  check(sampled_calls == 3, "createSampledLogSink forwards 1-in-N entries");

  reset();
  flight::Array<flight::Ref<LogEntry>> passed;
  auto every = log::create_sampled_log_sink(flight::types::LogSink([&](flight::Ref<LogEntry> value) { passed.push(value); }), 1.0);
  log::add_log_sink(every);
  log::emit_to_sinks(entry(LogLevel::Info, "a"));
  log::emit_to_sinks(entry(LogLevel::Info, "b"));
  check(passed.size() == 2 && text_of(passed[0]) == String("a") && text_of(passed[1]) == String("b"),
        "createSampledLogSink passes all entries through when rate is 1");
}

void test_timer() {
  reset();
  const auto timer = log::start_log_timer(String("render"), std::optional<String>(String("perf")));
  check(timer->label == String("render") && timer->channel == String("perf") && timer->started_at >= 0.0,
        "startLogTimer returns a LogTimer with the given label and channel");
}

}  // namespace

int main() {
  test_levels();
  test_channel_levels();
  test_gates();
  test_sink_combinators();
  test_timer();
  return failures == 0 ? 0 : 1;
}
