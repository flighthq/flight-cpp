#include <flight/log/log.hpp>

#include <concepts>
#include <iostream>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

flight::Ref<flight::types::LogEntry> make_entry() {
  return flight::make_ref<flight::types::LogEntry>(flight::types::LogEntry{
      .level = flight::types::LogLevel::Info,
      .channel = std::nullopt,
      .data = flight::String("identity test"),
  });
}

} // namespace

static_assert(std::same_as<
              flight::types::LogSink,
              flight::Function<void(flight::Ref<flight::types::LogEntry>)>>);

int main() {
  flight::log::clear_log_sinks();
  flight::log::log_signals = std::nullopt;

  int copied_calls = 0;
  flight::types::LogSink sink =
      [&](flight::Ref<flight::types::LogEntry>) { ++copied_calls; };
  const flight::types::LogSink copied_sink = sink;
  if (!check(sink == copied_sink && sink.identity() == copied_sink.identity(),
             "copying a log sink did not preserve callable identity")) {
    return 1;
  }

  flight::log::add_log_sink(sink);
  flight::log::add_log_sink(copied_sink);
  if (!check(flight::log::sinks.size() == 1,
             "adding the same log sink twice was not idempotent")) {
    return 1;
  }
  flight::log::emit_to_sinks(make_entry());
  if (!check(copied_calls == 1, "an idempotently registered sink fired more than once")) {
    return 1;
  }
  if (!check(flight::log::remove_log_sink(copied_sink),
             "a copied log sink did not remove the registered callable") ||
      !check(!flight::log::remove_log_sink(sink),
             "removing an absent log sink did not report false")) {
    return 1;
  }
  flight::log::emit_to_sinks(make_entry());
  if (!check(copied_calls == 1, "a removed log sink still received entries")) return 1;

  int distinct_calls = 0;
  auto implementation =
      [&](flight::Ref<flight::types::LogEntry>) { ++distinct_calls; };
  flight::types::LogSink first = implementation;
  flight::types::LogSink second = implementation;
  if (!check(first != second && first.identity() != second.identity(),
             "separately constructed log sinks unexpectedly shared identity")) {
    return 1;
  }
  flight::log::add_log_sink(first);
  flight::log::add_log_sink(second);
  flight::log::emit_to_sinks(make_entry());
  if (!check(flight::log::sinks.size() == 2 && distinct_calls == 2,
             "distinct log sink objects were incorrectly deduplicated")) {
    return 1;
  }

  auto handle = flight::make_ref<flight::types::MemoryLogSink>();
  handle->sink = sink;
  flight::log::set_log_sink(handle->sink);
  if (!check(flight::log::sinks.size() == 1 &&
                 flight::log::sinks.element(0.0).identity() == sink.identity(),
             "a generated handle or sink registry lost callable identity") ||
      !check(flight::log::remove_log_sink(copied_sink),
             "a callback copied through a generated handle was not removable")) {
    return 1;
  }

  flight::log::clear_log_sinks();
  return 0;
}
