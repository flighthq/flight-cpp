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

  // mergeSpanFields PRECEDENCE, added for a MEASURED HOLE: a mutation that inverted the record arm's
  // data-wins rule SURVIVED the whole suite, because no case gave a span field and a data field the same
  // key. The rule is the source's -- `{ ...spanFields, ...data }` assigns data AFTER the span fields -- and
  // within the span fields a LATER span wins, because they are accumulated oldest-first.
  {
    flight::Record<flight::String, flight::Any> first = {};
    first.set(flight::String("shared"), flight::Any(flight::String("first-span")));
    first.set(flight::String("only-first"), flight::Any(flight::String("first")));
    flight::Record<flight::String, flight::Any> second = {};
    second.set(flight::String("shared"), flight::Any(flight::String("second-span")));
    second.set(flight::String("only-second"), flight::Any(flight::String("second")));
    auto data = flight::Record<flight::String, flight::Any>{};
    data.set(flight::String("shared"), flight::Any(flight::String("data")));

    const auto span_a = flight::log::create_log_span(flight::String("a"), first);
    const auto span_b = flight::log::create_log_span(flight::String("b"), second);
    flight::log::enter_log_span(span_a);
    flight::log::enter_log_span(span_b);
    const auto merged = flight::log::merge_span_fields(
        flight::types::LogData{std::in_place_type<flight::Record<flight::String, flight::Any>>, data},
        std::nullopt);
    flight::log::exit_log_span(span_b);
    flight::log::exit_log_span(span_a);

    const auto merged_fields = std::get<flight::Record<flight::String, flight::Any>>(merged);
    const auto shared = merged_fields.get(flight::String("shared"));
    if (!check(shared.has_value() && shared.value().as_string() == flight::String("data"),
               "mergeSpanFields: a DATA field wins over a span field on the same key") ||
        !check(merged_fields.get(flight::String("only-first")).has_value() &&
                   merged_fields.get(flight::String("only-second")).has_value(),
               "and every span's own fields survive the merge")) {
      return 1;
    }
  }

  // createChildLogContext: the parent's fields are merged FIRST and the caller's win on collision, and the
  // channel falls back to the parent's only when the caller omits it. Added for a MEASURED hole -- the
  // emitted body declared an empty record and dropped the merge entirely, so every child context carried no
  // fields, and this function was covered by no test at all.
  {
    flight::Record<flight::String, flight::Any> parent_fields = {};
    parent_fields.set(flight::String("a"), flight::Any(flight::String("parent")));
    parent_fields.set(flight::String("b"), flight::Any(flight::String("parent")));
    const auto parent = flight::log::create_log_context(flight::String("parent-channel"), parent_fields);

    flight::Record<flight::String, flight::Any> child_fields = {};
    child_fields.set(flight::String("b"), flight::Any(flight::String("child")));
    child_fields.set(flight::String("c"), flight::Any(flight::String("child")));
    const auto child = flight::log::create_child_log_context(parent, child_fields);

    if (!check(child->fields.get(flight::String("a")).has_value() &&
                   child->fields.get(flight::String("a")).value().as_string() == flight::String("parent"),
               "createChildLogContext keeps the parent's fields the caller did not override") ||
        !check(child->fields.get(flight::String("b")).has_value() &&
                   child->fields.get(flight::String("b")).value().as_string() == flight::String("child"),
               "and the caller's field wins on collision") ||
        !check(child->fields.get(flight::String("c")).has_value(),
               "and the caller's own new field is present") ||
        !check(child->channel.has_value() && child->channel.value() == flight::String("parent-channel"),
               "and the channel is inherited when the caller omits it") ||
        !check(parent->fields.get(flight::String("b")).has_value() &&
                   parent->fields.get(flight::String("b")).value().as_string() == flight::String("parent"),
               "and the PARENT's own fields are not mutated")) {
      return 1;
    }
  }

  flight::log::clear_log_sinks();
  return 0;
}
