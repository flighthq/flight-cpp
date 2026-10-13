// Re-derived from the generated log module at a627849 (flight/log/log.hpp, 2/3 modules emitted: the emitter now
// generates every function but _writeConsoleCaptureEntry). The public surface is the one this override already
// gave its 62 consumers -- the data and sink spellings below are the same types the new emission names -- and the
// functions the emission's bodies cannot express in C++ are supplied from the source. See overrides/manifest.json
// (log-module-qualification-and-operators) for each change, why, and what remains.
//
// Still not supplied, by decision rather than gap: createConsoleCaptureSink and createConsoleLogSink write an
// object argument to a variadic console method, and choosing that output format is an open user decision
// (docs/compiler-requests/console-variadic-arguments.md).
#pragma once

#include <cmath>
#include <coroutine>
#include <cstddef>
#include <flight/any.hpp>
#include <flight/boolean.hpp>
#include <flight/error.hpp>
#include <flight/json.hpp>
#include <flight/number.hpp>
#include <flight/host/console.hpp>
#include <flight/host/performance.hpp>
#include <flight/host/timers.hpp>
#include <flight/object.hpp>
#include <flight/signals/emitter.hpp>
#include <flight/signals/signal.hpp>
#include <flight/record.hpp>
#include <flight/structural_ref.hpp>
#include <flight/weak_map.hpp>
#include <functional>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <variant>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

#include <flight/types/log.hpp>
#include <flight/types/log_signals.hpp>
#include <flight/types/buffered_log_sink.hpp>
#include <flight/types/memory_log_sink.hpp>
#include <flight/types/file_log_sink.hpp>
#include <flight/types/rate_limited_log_sink.hpp>

namespace flight::log {

// OVERRIDE: names the emitter leaves unqualified in this namespace.
using flight::types::BufferedLogSink;
using flight::types::FileLogSink;
using flight::types::LogContext;
using flight::types::LogFormatter;
using flight::types::LogSignals;
using flight::types::LogTimer;
using flight::types::LogData;

// STUB SUPPORT for this override. Three functions below were never generated, and their return types
// differ per call site, so there is no single signature to declare. This stands in for the value: it
// converts to anything and throws when it does, which names the gap at the moment it would have mattered
// rather than letting a wrong value travel.
struct NotImplementedInThisProfile final {
  const char* subject;
  template <typename Wanted>
  [[noreturn]] operator Wanted() const {
    throw flight::Error(flight::String("flight::log::") + flight::String(subject) +
                        flight::String(" was not generated for this profile"));
  }
};

// OVERRIDE: initializeLogEntity, which the emission spells as a cast of an anonymous object literal to the entity
// type it is building -- a different C++ type, so no conversion exists (log-native-compile-defects.md, row 12).
// The source marks the object it is given as an entity (its runtime cell present and undefined) and returns that
// same object. Every caller here builds the entity's own nominal type directly, which is equivalent: the literal
// is never observable except as the returned entity.
template <typename Pointer>
[[nodiscard]] inline Pointer initialize_log_entity(Pointer entity) {
  entity->entity_runtime_key = std::nullopt;
  return entity;
}

[[nodiscard]] inline NotImplementedInThisProfile create_json_log_formatter() {
  return NotImplementedInThisProfile{"create_json_log_formatter"};
}

// emit_signal belongs to @flighthq/signals and is written here unqualified.
using flight::signals::emit_signal;
using flight::types::LogDataProvider;
using flight::types::LogLevel;
using flight::types::MemoryLogSink;
using flight::types::RateLimitedLogSink;
using flight::types::FileLogSinkDestroyOutcome;

using flight::types::LogTransport;

using flight::types::LogSpan;

using flight::types::LogSink;

using flight::types::LogEntry;

struct BufferedLogSinkState;
struct MemoryLogSinkState;
struct FileLogSinkState;

inline flight::Ref<LogSink> create_fanout_log_sink(flight::Array<flight::Ref<LogSink>> sinks_2) {
  flight::Array<flight::Ref<LogSink>> list = sinks_2.slice();
  return [=](flight::Ref<LogEntry> entry) {
  for (auto s : list) {
    s(entry);
  }
};
}

inline flight::Ref<LogSink> create_filter_log_sink(flight::Ref<LogSink> target, std::function<bool(flight::Ref<LogEntry>)> predicate) {
  return [=](flight::Ref<LogEntry> entry) {
  if (predicate(entry)) {
    target(entry);
  }
};
}

inline flight::Ref<LogSink> create_sampled_log_sink(flight::Ref<LogSink> target, double rate) {
  if ((rate <= 1.0)) {
    return [=](flight::Ref<LogEntry> entry) { return target(entry); };
  }
  const auto counter_capture = flight::make_binding_cell(double{0.0});
  return [=](flight::Ref<LogEntry> entry) {
  counter_capture.rebind(std::fmod((counter_capture.read_binding() + 1.0), rate));
  if ((counter_capture.read_binding() == 0.0)) {
    target(entry);
  }
};
}


// OVERRIDE: serializeLogError. The emission reads `!value.error_if() != nullptr` (negation precedence lost) and
// writes `result.get(k).value() = v` for a record element store (log-native-compile-defects.md, rows 1-3).
// The source:
//   if (!(value instanceof Error)) return { value: String(value) };
//   const result = { name: value.name, message: value.message };
//   if (value.stack !== undefined) result.stack = value.stack;
//   if (value.cause !== undefined) result.cause = serializeLogError(value.cause);
// error_if() is the checked Error view, which accepts subclasses and keeps their names; String(value) is the
// runtime's explicit conversion, which throws for a non-Error object rather than inventing "[object Object]".
// Key order is the source's: name, message, then stack and cause when present.
inline flight::Record<flight::String, flight::Any> serialize_log_error(flight::Any value) {
  const auto error = value.error_if();
  if (error == nullptr) {
    return {{flight::String("value"), flight::Any(flight::to_string(value))}};
  }
  flight::Record<flight::String, flight::Any> result = {{flight::String("name"), flight::Any(error->name())},
                                                        {flight::String("message"), flight::Any(error->message())}};
  if (error->stack().has_value()) {
    result.set(flight::String("stack"), flight::Any(error->stack()));
  }
  const flight::Any cause = error->cause().value_or(flight::Any());
  if (!cause.is_undefined()) {
    // A nested record travels in an Any as an external, the runtime's carrier for a Record value.
    result.set(flight::String("cause"), flight::Any::external(serialize_log_error(cause)));
  }
  return result;
}

struct BufferedLogSinkState : public flight::ReferenceEnabled {
  flight::Array<flight::Ref<LogEntry>> buf;
  std::function<void()> flush;
  std::optional<flight::host::TimerHandle> timer;
};

struct MemoryLogSinkState : public flight::ReferenceEnabled {
  flight::Array<flight::Ref<LogEntry>> buf;
  double head;
};

struct FileLogSinkState : public flight::ReferenceEnabled {
  std::optional<flight::Task<flight::Ref<FileLogSinkDestroyOutcome>>> destroy_promise;
  flight::String status;
  std::optional<flight::Ref<LogTransport>> transport;
};

inline flight::WeakMap<flight::Ref<BufferedLogSink>, flight::Ref<BufferedLogSinkState>> buffered_sink_states = flight::WeakMap<flight::Ref<BufferedLogSink>, flight::Ref<BufferedLogSinkState>>();

inline void dispose_log_sink(flight::Ref<BufferedLogSink> handle) {
  auto state = buffered_sink_states.get(handle);
  if (!state.has_value()) {
    return;
  }
  if (state.value()->timer.has_value()) {
    flight::host::clear_interval(state.value()->timer.value());
  }
  state.value()->flush();
  (state.value()->timer = std::nullopt);
}

inline void flush_log_sink(flight::Ref<BufferedLogSink> handle) {
  auto state = buffered_sink_states.get(handle);
  if (flight::to_boolean(state)) {
    state.value()->flush();
  }
}

inline flight::WeakMap<flight::Ref<FileLogSink>, flight::Ref<FileLogSinkState>> file_log_sink_states = flight::WeakMap<flight::Ref<FileLogSink>, flight::Ref<FileLogSinkState>>();

inline flight::WeakMap<flight::Ref<MemoryLogSink>, flight::Ref<MemoryLogSinkState>> memory_sink_states = flight::WeakMap<flight::Ref<MemoryLogSink>, flight::Ref<MemoryLogSinkState>>();

inline void clear_memory_log_sink(flight::Ref<MemoryLogSink> handle) {
  auto state = memory_sink_states.get(handle);
  if (!state.has_value()) {
    return;
  }
  ([&]() { auto&& assignment_receiver = state.value()->buf; const auto assignment_value = 0.0; assignment_receiver.resize(assignment_value); return assignment_value; }());
  (state.value()->head = 0.0);
}

inline flight::Array<flight::Ref<LogEntry>> get_memory_log_sink_entries(flight::Ref<MemoryLogSink> handle) {
  auto state = memory_sink_states.get(handle);
  if (!state.has_value()) {
    return flight::Array<flight::Ref<LogEntry>>{};
  }
  auto object_pattern_value = state.value();
  flight::Array<flight::Ref<LogEntry>> buf = object_pattern_value->buf;
  const double head = object_pattern_value->head;
  if ((head == 0.0)) {
    return buf.slice();
  }
  return ([&]() { flight::Array<flight::Ref<LogEntry>> array_spread_result; for (const auto& array_spread_item : buf.slice(head)) { array_spread_result.push(array_spread_item); } for (const auto& array_spread_item : buf.slice(0.0, head)) { array_spread_result.push(array_spread_item); } return array_spread_result; }());
}

// OVERRIDE (representation): TypeScript's `Record<LogLevel, string>` is keyed by a NUMBER at runtime --
// LogLevel is a numeric enum, and `_levelNames[level]` is a numeric index. The emitter kept the enum as
// the C++ key type, and flight::Record only admits the PropertyKey domain (String, double, Symbol), so
// `Record<LogLevel, String>` fails its own static_assert: "Flight Record key is not a PropertyKey
// domain". Keyed by double, with every enum key and every lookup converted at the boundary, the table
// holds exactly the entries and answers exactly the queries the TypeScript does. This is the same class
// of correction as the LogLevel work above, in the one place the enum is used as a key rather than a value.
inline flight::Record<double, flight::String> console_methods = ([]() { flight::Record<double, flight::String> record_construction = {}; const double record_construction_key = static_cast<double>(flight::types::LogLevel::None); auto record_construction_value = flight::String("log"); record_construction.set(record_construction_key, record_construction_value); const double record_construction_key_2 = static_cast<double>(flight::types::LogLevel::Error); auto record_construction_value_2 = flight::String("error"); record_construction.set(record_construction_key_2, record_construction_value_2); const double record_construction_key_3 = static_cast<double>(flight::types::LogLevel::Warn); auto record_construction_value_3 = flight::String("warn"); record_construction.set(record_construction_key_3, record_construction_value_3); const double record_construction_key_4 = static_cast<double>(flight::types::LogLevel::Info); auto record_construction_value_4 = flight::String("info"); record_construction.set(record_construction_key_4, record_construction_value_4); const double record_construction_key_5 = static_cast<double>(flight::types::LogLevel::Debug); auto record_construction_value_5 = flight::String("debug"); record_construction.set(record_construction_key_5, record_construction_value_5); const double record_construction_key_6 = static_cast<double>(flight::types::LogLevel::Verbose); auto record_construction_value_6 = flight::String("log"); record_construction.set(record_construction_key_6, record_construction_value_6); return record_construction; }());

inline flight::Record<double, flight::String> level_names = ([]() { flight::Record<double, flight::String> record_construction_2 = {}; const double record_construction_key_7 = static_cast<double>(flight::types::LogLevel::None); auto record_construction_value_7 = flight::String("none"); record_construction_2.set(record_construction_key_7, record_construction_value_7); const double record_construction_key_8 = static_cast<double>(flight::types::LogLevel::Error); auto record_construction_value_8 = flight::String("error"); record_construction_2.set(record_construction_key_8, record_construction_value_8); const double record_construction_key_9 = static_cast<double>(flight::types::LogLevel::Warn); auto record_construction_value_9 = flight::String("warn"); record_construction_2.set(record_construction_key_9, record_construction_value_9); const double record_construction_key_10 = static_cast<double>(flight::types::LogLevel::Info); auto record_construction_value_10 = flight::String("info"); record_construction_2.set(record_construction_key_10, record_construction_value_10); const double record_construction_key_11 = static_cast<double>(flight::types::LogLevel::Debug); auto record_construction_value_11 = flight::String("debug"); record_construction_2.set(record_construction_key_11, record_construction_value_11); const double record_construction_key_12 = static_cast<double>(flight::types::LogLevel::Verbose); auto record_construction_value_12 = flight::String("verbose"); record_construction_2.set(record_construction_key_12, record_construction_value_12); return record_construction_2; }());

inline flight::String get_log_level_name(flight::types::LogLevel level_2) {
  return ([&]() -> flight::String { auto nullish_coalesce_left = level_names.get(static_cast<double>(level_2)); if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return flight::String("unknown"); }());
}

inline flight::Map<flight::String, flight::types::LogLevel> level_by_name = flight::Map<flight::String, flight::types::LogLevel>({{flight::String("none"), flight::types::LogLevel::None}, {flight::String("error"), flight::types::LogLevel::Error}, {flight::String("warn"), flight::types::LogLevel::Warn}, {flight::String("info"), flight::types::LogLevel::Info}, {flight::String("debug"), flight::types::LogLevel::Debug}, {flight::String("verbose"), flight::types::LogLevel::Verbose}});

inline std::optional<flight::types::LogLevel> parse_log_level(flight::String name) {
  return level_by_name.get(name.to_lower());
}

inline flight::Map<flight::String, flight::types::LogLevel> channel_levels = flight::Map<flight::String, flight::types::LogLevel>();

inline void clear_log_channel_level(flight::String channel) {
  channel_levels.erase(channel);
}

inline void clear_log_channel_levels() {
  channel_levels.clear();
}

inline std::optional<flight::types::LogLevel> get_log_channel_level(flight::String channel) {
  return channel_levels.get(channel);
}

inline void set_log_channel_level(flight::String channel, flight::types::LogLevel level_2) {
  channel_levels.set(channel, level_2);
}

inline flight::Set<flight::String> once_keys = flight::Set<flight::String>();

inline void clear_log_once_keys() {
  once_keys.clear();
}

inline flight::Array<flight::String> redaction_paths = flight::Array<flight::String>{};

inline void clear_log_redaction_paths() {
  ([&]() { auto&& assignment_receiver = redaction_paths; const auto assignment_value = 0.0; assignment_receiver.resize(assignment_value); return assignment_value; }());
}

inline void set_log_redaction_paths(flight::Array<flight::String> paths) {
  ([&]() { auto&& assignment_receiver = redaction_paths; const auto assignment_value = 0.0; assignment_receiver.resize(assignment_value); return assignment_value; }());
  for (auto p : paths) {
    redaction_paths.push(p);
  }
}

inline flight::Map<flight::String, std::function<flight::Record<flight::String, flight::Any>(flight::Any)>> serializers = flight::Map<flight::String, std::function<flight::Record<flight::String, flight::Any>(flight::Any)>>();

inline void clear_log_serializers() {
  serializers.clear();
}

inline void register_log_serializer(flight::String kind, std::function<flight::Record<flight::String, flight::Any>(flight::Any)> fn) {
  serializers.set(kind, fn);
}

inline flight::Array<flight::Ref<LogSink>> sinks = flight::Array<flight::Ref<LogSink>>{};

inline void add_log_sink(flight::Ref<LogSink> sink) {
  if (sinks.includes(sink)) return;
  sinks.push(sink);
}

inline void clear_log_sinks() {
  ([&]() { auto&& assignment_receiver = sinks; const auto assignment_value = 0.0; assignment_receiver.resize(assignment_value); return assignment_value; }());
}

inline bool remove_log_sink(flight::Ref<LogSink> sink) {
  const double idx = sinks.index_of(sink);
  if (idx < 0.0) return false;
  static_cast<void>(sinks.splice(idx, 1.0));
  return true;
}

inline void set_log_sink(std::optional<flight::Ref<LogSink>> sink) {
  ([&]() { auto&& assignment_receiver = sinks; const auto assignment_value = 0.0; assignment_receiver.resize(assignment_value); return assignment_value; }());
  if (sink.has_value()) {
    sinks.push(sink.value());
  }
}

inline flight::Array<flight::Ref<LogSpan>> span_stack = flight::Array<flight::Ref<LogSpan>>{};

inline void enter_log_span(flight::Ref<LogSpan> span) {
  span_stack.push(span);
}

inline void exit_log_span(flight::Ref<LogSpan> span) {
  const double idx = span_stack.index_of(span);
  if ((idx >= 0.0)) {
    span_stack.splice(idx, 1.0);
  }
}

inline flight::types::LogLevel console_level = flight::types::LogLevel::Info;

inline flight::types::LogLevel get_log_console_level() {
  return console_level;
}

inline void set_log_console_level(flight::types::LogLevel level_2) {
  (console_level = level_2);
}

inline double group_depth = 0.0;

inline void clear_log_groups() {
  (group_depth = 0.0);
}

inline flight::types::LogLevel level = flight::types::LogLevel::Verbose;

inline flight::types::LogLevel get_log_level() {
  return level;
}

inline void set_log_level(flight::types::LogLevel level_2) {
  (level = level_2);
}

inline std::optional<flight::Ref<LogSignals>> log_signals = std::nullopt;


// OVERRIDE: _operationFailureMessage -- `error instanceof Error ? error.message : String(error)`, through the
// same checked Error view and explicit conversion as serializeLogError (log-native-compile-defects.md, row 2).
inline flight::String operation_failure_message(flight::Any error) {
  const auto view = error.error_if();
  return view != nullptr ? view->message() : flight::to_string(error);
}

// NOT GENERATED: function _destroyFileLogSinkState -- source line 678
//
// The source it stood for:
//   async function _destroyFileLogSinkState(state: FileLogSinkState): Promise<FileLogSinkDestroyOutcome> {
//     const transport = state.transport!;
//     let flush: LogTransportFlushOutcome;
//     let destroy: LogTransportDestroyOutcome;
//   
//     try {
//       flush = await transport.flush();
//     } catch (error) {
//       flush = { message: _operationFailureMessage(error), reason: 'operation-failed' };
//     }
//   
//     try {
//       destroy = await transport.destroy();
//     } catch (error) {
//       destroy = { message: _operationFailureMessage(error), reason: 'operation-failed' };
//     }
//   
//     state.transport = null;
//     state.status = 'destroyed';
//     return {
//       destroy,
//       flush,
//       reason:
//         flush.reason === 'operation-failed' || destroy.reason === 'operation-failed' ? 'operation-failed' : 'destroyed',
//     };
//   }
//   
// cpp emission failed for @flighthq/log/packages/log/src/log.ts: union member test requires a C++ variant
// binding

inline flight::Task<flight::Ref<FileLogSinkDestroyOutcome>> destroy_file_log_sink(flight::Ref<FileLogSink> handle) {
  // STUBBED in this override. The emitter produced a body for this function that is not valid C++,
  // and recovering what it meant requires re-deriving the expression from the TypeScript. Throwing
  // names the gap at the one moment it could matter instead of logging nothing and looking fine.
  throw flight::Error(flight::String("flight::log::destroy_file_log_sink is not implemented in this profile: the emitter produced invalid C++ for it"));
}

inline flight::Record<flight::String, flight::Any> apply_serializers(flight::Record<flight::String, flight::Any> data) {
  // STUBBED in this override. The emitter produced a body for this function that is not valid C++,
  // and recovering what it meant requires re-deriving the expression from the TypeScript. Throwing
  // names the gap at the one moment it could matter instead of logging nothing and looking fine.
  throw flight::Error(flight::String("flight::log::apply_serializers is not implemented in this profile: the emitter produced invalid C++ for it"));
}

inline void redact_path(flight::Record<flight::String, flight::Any> obj, flight::Array<flight::String> parts, double idx) {
  // STUBBED in this override. The emitter produced a body for this function that is not valid C++,
  // and recovering what it meant requires re-deriving the expression from the TypeScript. Throwing
  // names the gap at the one moment it could matter instead of logging nothing and looking fine.
  throw flight::Error(flight::String("flight::log::redact_path is not implemented in this profile: the emitter produced invalid C++ for it"));
}

inline flight::Record<flight::String, flight::Any> apply_redaction(flight::Record<flight::String, flight::Any> data) {
  auto result = ([&]() { flight::Record<flight::String, flight::Any> record_construction_3 = {}; auto&& record_spread_source = data; for (const auto& record_spread_entry : record_spread_source) { record_construction_3.set(record_spread_entry.first, record_spread_entry.second); } return record_construction_3; }());
  for (auto path : redaction_paths) {
    flight::Array<flight::String> parts = path.split(flight::String("."));
    redact_path(result, parts, 0.0);
  }
  return result;
}

inline void emit_to_sinks(flight::Ref<LogEntry> entry) {
  for (auto sink : sinks) {
    sink(entry);
  }
  if (log_signals.has_value()) {
    emit_signal(log_signals.value()->on_log_entry, entry);
    if ((entry->level == flight::types::LogLevel::Error)) {
      emit_signal(log_signals.value()->on_log_error, entry);
    }
  }
}


// OVERRIDE: _mergeContextFields and _mergeSpanFields. The emission writes each object spread as a designated
// initializer -- `return {.msg = data};` for string data and `return {};` for record data, dropping every spread
// field -- which does not compile, and would be wrong if it did (log-native-compile-defects.md, row 8). The spreads
// are supplied with flight::object_assign, which is Object.assign over flight::Record: own keys in insertion order,
// a key already present keeping its position and taking the later value -- exactly `{ ...a, ...b }`. A key present
// with the value undefined is copied as present, as a spread copies it; only a missing key stays missing.
//
//   function _mergeContextFields(context, data) {
//     const { fields } = context;
//     if (Object.keys(fields).length === 0) return data;
//     if (typeof data === 'string') return { msg: data, ...fields };
//     return { ...fields, ...(data as Record<string, unknown>) };
//   }
//
// Data fields win over context fields; a string becomes { msg, ...fields }, so a field named msg wins over it.
// With no fields, the data is returned as given -- a record keeps its identity.
inline LogData merge_context_fields(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<LogContext>>>> context, LogData data) {
  const flight::Record<flight::String, flight::Any> fields = flight::row_get<flight::RowKey<"fields">>(context);
  if (fields.size() == 0) {
    return data;
  }
  flight::Record<flight::String, flight::Any> merged;
  if (const auto* text = std::get_if<flight::String>(&data)) {
    merged.set(flight::String("msg"), flight::Any(*text));
    flight::object_assign(merged, fields);
  } else {
    flight::object_assign(merged, fields, std::get<flight::Record<flight::String, flight::Any>>(data));
  }
  return LogData{std::in_place_index<0>, merged};
}

//   function _mergeSpanFields(data, _channel) {
//     if (_spanStack.length === 0) return data;
//     const spanFields = {};
//     for (const span of _spanStack) Object.assign(spanFields, span.fields);
//     if (Object.keys(spanFields).length === 0) return data;
//     if (typeof data === 'string') return { msg: data, ...spanFields };
//     return { ...spanFields, ...(data as Record<string, unknown>) };
//   }
//
// Spans are folded oldest-first, so a newer span wins on a shared key; data fields win over every span.
inline LogData merge_span_fields(LogData data, std::optional<flight::String> channel) {
  static_cast<void>(channel);
  if (span_stack.size() == 0) {
    return data;
  }
  flight::Record<flight::String, flight::Any> span_fields;
  for (const auto& span : span_stack) {
    flight::object_assign(span_fields, span->fields);
  }
  if (span_fields.size() == 0) {
    return data;
  }
  flight::Record<flight::String, flight::Any> merged;
  if (const auto* text = std::get_if<flight::String>(&data)) {
    merged.set(flight::String("msg"), flight::Any(*text));
    flight::object_assign(merged, span_fields);
  } else {
    flight::object_assign(merged, span_fields, std::get<flight::Record<flight::String, flight::Any>>(data));
  }
  return LogData{std::in_place_index<0>, merged};
}

inline bool passes_level_gate(flight::types::LogLevel level_2, std::optional<flight::String> channel) {
  if (((static_cast<double>(sinks.size()) == 0.0) && !log_signals.has_value())) {
    return false;
  }
  flight::Ref<LogLevel> gate = ((channel.has_value() && channel_levels.has(channel.value())) ? channel_levels.get(channel.value()).value() : level);
  return ((level_2 <= gate) && (level_2 != flight::types::LogLevel::None));
}

// a627849 emission, carried verbatim: the beginLogGroup body is valid C++ at this pin.
inline void begin_log_group(flight::String label, std::optional<std::optional<flight::String>> channel = std::nullopt) {
  channel = channel.value_or(std::nullopt);
  group_depth++;
  if (!passes_level_gate(flight::types::LogLevel::Debug, channel.value())) {
    return;
  }
  emit_to_sinks(flight::make_ref<flight::types::LogEntry>(flight::types::LogEntry{.level = flight::types::LogLevel::Debug, .channel = channel.value(), .data = std::variant<flight::Record<flight::String, flight::Any>, flight::String>{std::in_place_type<flight::Record<flight::String, flight::Any>>, flight::Record<flight::String, flight::Any>{{flight::String("msg"), label}, {flight::String("group"), flight::String("begin")}, {flight::String("depth"), group_depth}}}}));
}

// a627849 emission, carried verbatim: the endLogGroup body is valid C++ at this pin.
inline void end_log_group(std::optional<std::optional<flight::String>> channel = std::nullopt) {
  channel = channel.value_or(std::nullopt);
  if ((group_depth <= 0.0)) {
    return;
  }
  group_depth--;
  if (!passes_level_gate(flight::types::LogLevel::Debug, channel.value())) {
    return;
  }
  emit_to_sinks(flight::make_ref<flight::types::LogEntry>(flight::types::LogEntry{.level = flight::types::LogLevel::Debug, .channel = channel.value(), .data = std::variant<flight::Record<flight::String, flight::Any>, flight::String>{std::in_place_type<flight::Record<flight::String, flight::Any>>, flight::Record<flight::String, flight::Any>{{flight::String("group"), flight::String("end")}, {flight::String("depth"), (group_depth + 1.0)}}}}));
}

inline void log(flight::types::LogLevel level_2, std::variant<flight::Ref<LogDataProvider>, flight::Ref<LogData>> data, std::optional<std::optional<flight::String>> channel = std::nullopt) {
  channel = channel.value_or(std::nullopt);
  if (!passes_level_gate(level_2, channel.value())) {
    return;
  }
  flight::Ref<LogData> resolved = (data.index() == 0 ? std::get<0>(data)() : std::get<1>(data));
  flight::Ref<LogEntry> entry = flight::make_ref<LogEntry>(LogEntry{.level = level_2, .channel = channel.value(), .data = merge_span_fields(resolved, channel.value())});
  emit_to_sinks(entry);
}

inline void log_assert(bool condition, std::variant<flight::Ref<LogDataProvider>, flight::Ref<LogData>> data, std::optional<std::optional<flight::String>> channel = std::nullopt) {
  channel = channel.value_or(std::nullopt);
  if (condition) {
    return;
  }
  if (!passes_level_gate(flight::types::LogLevel::Error, channel.value())) {
    return;
  }
  flight::Ref<LogData> resolved = (data.index() == 0 ? std::get<0>(data)() : std::get<1>(data));
  emit_to_sinks(flight::make_ref<LogEntry>(LogEntry{.level = flight::types::LogLevel::Error, .channel = channel.value(), .data = merge_span_fields(resolved, channel.value())}));
}

inline void log_debug(std::variant<flight::Ref<LogDataProvider>, flight::Ref<LogData>> data, std::optional<std::optional<flight::String>> channel = std::nullopt) {
  channel = channel.value_or(std::nullopt);
  if (!passes_level_gate(flight::types::LogLevel::Debug, channel.value())) {
    return;
  }
  flight::Ref<LogData> resolved = (data.index() == 0 ? std::get<0>(data)() : std::get<1>(data));
  emit_to_sinks(flight::make_ref<LogEntry>(LogEntry{.level = flight::types::LogLevel::Debug, .channel = channel.value(), .data = merge_span_fields(resolved, channel.value())}));
}

inline void log_debug(flight::Record<flight::String, flight::Any> data,
                      std::optional<std::optional<flight::String>> channel = std::nullopt) {
  using Input = std::variant<flight::Ref<LogDataProvider>, flight::Ref<LogData>>;
  log_debug(
      Input{std::in_place_index<1>,
            flight::types::LogData{std::in_place_index<0>, std::move(data)}},
      channel);
}


// OVERRIDE: logDebugWith, which the earlier emission refused and the a627849 emission produces. Its body is carried
// over the module's own data carrier: the context's channel gates and labels the entry, and the data is
// span-merged, then context-merged (context fields lose to data, and to spans' fields only where data does not
// set them), exactly as the source composes _mergeContextFields(context, _mergeSpanFields(resolved, channel)).
inline void log_debug_with(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<LogContext>>>> context, std::variant<flight::Ref<LogDataProvider>, flight::Ref<LogData>> data) {
  const std::optional<flight::String> channel = flight::row_get<flight::RowKey<"channel">>(context);
  if (!passes_level_gate(flight::types::LogLevel::Debug, channel)) {
    return;
  }
  flight::Ref<LogData> resolved = (data.index() == 0 ? std::get<0>(data)() : std::get<1>(data));
  emit_to_sinks(flight::make_ref<LogEntry>(LogEntry{.level = flight::types::LogLevel::Debug, .channel = channel, .data = merge_context_fields(context, merge_span_fields(resolved, channel))}));
}

inline void log_error(std::variant<flight::Ref<LogDataProvider>, flight::Ref<LogData>> data, std::optional<std::optional<flight::String>> channel = std::nullopt) {
  channel = channel.value_or(std::nullopt);
  if (!passes_level_gate(flight::types::LogLevel::Error, channel.value())) {
    return;
  }
  flight::Ref<LogData> resolved = (data.index() == 0 ? std::get<0>(data)() : std::get<1>(data));
  emit_to_sinks(flight::make_ref<LogEntry>(LogEntry{.level = flight::types::LogLevel::Error, .channel = channel.value(), .data = merge_span_fields(resolved, channel.value())}));
}


// OVERRIDE: logErrorWith, which the earlier emission refused and the a627849 emission produces. Its body is carried
// over the module's own data carrier: the context's channel gates and labels the entry, and the data is
// span-merged, then context-merged (context fields lose to data, and to spans' fields only where data does not
// set them), exactly as the source composes _mergeContextFields(context, _mergeSpanFields(resolved, channel)).
inline void log_error_with(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<LogContext>>>> context, std::variant<flight::Ref<LogDataProvider>, flight::Ref<LogData>> data) {
  const std::optional<flight::String> channel = flight::row_get<flight::RowKey<"channel">>(context);
  if (!passes_level_gate(flight::types::LogLevel::Error, channel)) {
    return;
  }
  flight::Ref<LogData> resolved = (data.index() == 0 ? std::get<0>(data)() : std::get<1>(data));
  emit_to_sinks(flight::make_ref<LogEntry>(LogEntry{.level = flight::types::LogLevel::Error, .channel = channel, .data = merge_context_fields(context, merge_span_fields(resolved, channel))}));
}

inline void log_info(std::variant<flight::Ref<LogDataProvider>, flight::Ref<LogData>> data, std::optional<std::optional<flight::String>> channel = std::nullopt) {
  channel = channel.value_or(std::nullopt);
  if (!passes_level_gate(flight::types::LogLevel::Info, channel.value())) {
    return;
  }
  flight::Ref<LogData> resolved = (data.index() == 0 ? std::get<0>(data)() : std::get<1>(data));
  emit_to_sinks(flight::make_ref<LogEntry>(LogEntry{.level = flight::types::LogLevel::Info, .channel = channel.value(), .data = merge_span_fields(resolved, channel.value())}));
}


// OVERRIDE: logInfoWith, which the earlier emission refused and the a627849 emission produces. Its body is carried
// over the module's own data carrier: the context's channel gates and labels the entry, and the data is
// span-merged, then context-merged (context fields lose to data, and to spans' fields only where data does not
// set them), exactly as the source composes _mergeContextFields(context, _mergeSpanFields(resolved, channel)).
inline void log_info_with(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<LogContext>>>> context, std::variant<flight::Ref<LogDataProvider>, flight::Ref<LogData>> data) {
  const std::optional<flight::String> channel = flight::row_get<flight::RowKey<"channel">>(context);
  if (!passes_level_gate(flight::types::LogLevel::Info, channel)) {
    return;
  }
  flight::Ref<LogData> resolved = (data.index() == 0 ? std::get<0>(data)() : std::get<1>(data));
  emit_to_sinks(flight::make_ref<LogEntry>(LogEntry{.level = flight::types::LogLevel::Info, .channel = channel, .data = merge_context_fields(context, merge_span_fields(resolved, channel))}));
}

inline bool log_once(flight::String key, flight::types::LogLevel level_2, std::variant<flight::Ref<LogDataProvider>, flight::Ref<LogData>> data, std::optional<std::optional<flight::String>> channel = std::nullopt) {
  channel = channel.value_or(std::nullopt);
  if (once_keys.has(key)) {
    return false;
  }
  once_keys.add(key);
  log(level_2, data, channel.value());
  return true;
}

inline bool log_once(flight::String key, flight::types::LogLevel level,
                     flight::Record<flight::String, flight::Any> data,
                     std::optional<std::optional<flight::String>> channel = std::nullopt) {
  using Input = std::variant<flight::Ref<LogDataProvider>, flight::Ref<LogData>>;
  return log_once(
      std::move(key), level,
      Input{std::in_place_index<1>,
            flight::types::LogData{std::in_place_index<0>, std::move(data)}},
      channel);
}

inline void log_verbose(std::variant<flight::Ref<LogDataProvider>, flight::Ref<LogData>> data, std::optional<std::optional<flight::String>> channel = std::nullopt) {
  channel = channel.value_or(std::nullopt);
  if (!passes_level_gate(flight::types::LogLevel::Verbose, channel.value())) {
    return;
  }
  flight::Ref<LogData> resolved = (data.index() == 0 ? std::get<0>(data)() : std::get<1>(data));
  emit_to_sinks(flight::make_ref<LogEntry>(LogEntry{.level = flight::types::LogLevel::Verbose, .channel = channel.value(), .data = merge_span_fields(resolved, channel.value())}));
}


// OVERRIDE: logVerboseWith, which the earlier emission refused and the a627849 emission produces. Its body is carried
// over the module's own data carrier: the context's channel gates and labels the entry, and the data is
// span-merged, then context-merged (context fields lose to data, and to spans' fields only where data does not
// set them), exactly as the source composes _mergeContextFields(context, _mergeSpanFields(resolved, channel)).
inline void log_verbose_with(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<LogContext>>>> context, std::variant<flight::Ref<LogDataProvider>, flight::Ref<LogData>> data) {
  const std::optional<flight::String> channel = flight::row_get<flight::RowKey<"channel">>(context);
  if (!passes_level_gate(flight::types::LogLevel::Verbose, channel)) {
    return;
  }
  flight::Ref<LogData> resolved = (data.index() == 0 ? std::get<0>(data)() : std::get<1>(data));
  emit_to_sinks(flight::make_ref<LogEntry>(LogEntry{.level = flight::types::LogLevel::Verbose, .channel = channel, .data = merge_context_fields(context, merge_span_fields(resolved, channel))}));
}

inline void log_warn(std::variant<flight::Ref<LogDataProvider>, flight::Ref<LogData>> data, std::optional<std::optional<flight::String>> channel = std::nullopt) {
  channel = channel.value_or(std::nullopt);
  if (!passes_level_gate(flight::types::LogLevel::Warn, channel.value())) {
    return;
  }
  flight::Ref<LogData> resolved = (data.index() == 0 ? std::get<0>(data)() : std::get<1>(data));
  emit_to_sinks(flight::make_ref<LogEntry>(LogEntry{.level = flight::types::LogLevel::Warn, .channel = channel.value(), .data = merge_span_fields(resolved, channel.value())}));
}


// OVERRIDE: logWarnWith, which the earlier emission refused and the a627849 emission produces. Its body is carried
// over the module's own data carrier: the context's channel gates and labels the entry, and the data is
// span-merged, then context-merged (context fields lose to data, and to spans' fields only where data does not
// set them), exactly as the source composes _mergeContextFields(context, _mergeSpanFields(resolved, channel)).
inline void log_warn_with(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<LogContext>>>> context, std::variant<flight::Ref<LogDataProvider>, flight::Ref<LogData>> data) {
  const std::optional<flight::String> channel = flight::row_get<flight::RowKey<"channel">>(context);
  if (!passes_level_gate(flight::types::LogLevel::Warn, channel)) {
    return;
  }
  flight::Ref<LogData> resolved = (data.index() == 0 ? std::get<0>(data)() : std::get<1>(data));
  emit_to_sinks(flight::make_ref<LogEntry>(LogEntry{.level = flight::types::LogLevel::Warn, .channel = channel, .data = merge_context_fields(context, merge_span_fields(resolved, channel))}));
}


// OVERRIDE: logWith, which the earlier emission refused and the a627849 emission produces. Its body is carried
// over the module's own data carrier: the context's channel gates and labels the entry, and the data is
// span-merged, then context-merged (context fields lose to data, and to spans' fields only where data does not
// set them), exactly as the source composes _mergeContextFields(context, _mergeSpanFields(resolved, channel)).
inline void log_with(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<LogContext>>>> context, flight::types::LogLevel level_2, std::variant<flight::Ref<LogDataProvider>, flight::Ref<LogData>> data) {
  const std::optional<flight::String> channel = flight::row_get<flight::RowKey<"channel">>(context);
  if (!passes_level_gate(level_2, channel)) {
    return;
  }
  flight::Ref<LogData> resolved = (data.index() == 0 ? std::get<0>(data)() : std::get<1>(data));
  emit_to_sinks(flight::make_ref<LogEntry>(LogEntry{.level = level_2, .channel = channel, .data = merge_context_fields(context, merge_span_fields(resolved, channel))}));
}

inline double timestamp() {
  return (true ? flight::host::performance_now() : flight::Date::now());
}


// NOT GENERATED: function createJsonLogFormatter -- source line 195
// refusal: cpp-contextual-union-missing-expression-type:optionalSingle [compiler-restriction]
//
// The source it stood for:
//   export function createJsonLogFormatter(): LogFormatter {
//     return (entry: Readonly<LogEntry>): string => {
//       const { level, channel, data } = entry;
//       const serialized = _applySerializers(typeof data === 'string' ? { msg: data } : (data as Record<string, unknown>));
//       const redacted = _redactionPaths.length > 0 ? _applyRedaction(serialized) : serialized;
//       return JSON.stringify({
//         __flight: true,
//         t: _timestamp(),
//         level: _levelNames[level],
//         channel,
//         data: redacted,
//       });
//     };
//   }
//   
//   // Creates a bound logging context with a channel and optional base fields. Pass the context to
//   // logWith / logErrorWith / etc. to have the channel and fields merged into every entry.
// cpp emission failed for @flighthq/log/packages/log/src/log.ts: contextual optionalSingle construction requires
// expression type evidence

#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_FORMATTER_C3F72D72FEB278C6
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_FORMATTER_C3F72D72FEB278C6
struct formatter_c3f72d72feb278c6 : public flight::ReferenceEnabled {
  std::optional<flight::Ref<LogFormatter>> formatter;
};
#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_FORMATTER_C3F72D72FEB278C6

inline flight::Ref<FileLogSink> create_file_log_sink(flight::Ref<LogTransport> transport, std::optional<flight::Ref<formatter_c3f72d72feb278c6>> options = std::nullopt) {
  // STUBBED: the emitted body reads a `handle` that is never declared in its scope -- the declaration
  // belonged to a part of this module the emitter did not generate.
  throw flight::Error(flight::String("flight::log::create_file_log_sink is not implemented in this profile: the emitter produced invalid C++ for it"));
}

#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_INDENT_GROUPS_LEVEL_PREFIX_TIMESTAMP_85EF46F909E535F5
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_INDENT_GROUPS_LEVEL_PREFIX_TIMESTAMP_85EF46F909E535F5
struct indent_groups_level_prefix_timestamp_85ef46f909e535f5 : public flight::ReferenceEnabled {
  std::optional<bool> indent_groups;
  std::optional<bool> level_prefix;
  std::optional<bool> timestamp;
};
#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_INDENT_GROUPS_LEVEL_PREFIX_TIMESTAMP_85EF46F909E535F5


// OVERRIDE: createTextLogFormatter, emitted at a627849 and carried with one correction. The emission tests
// `options.indentGroups && _groupDepth > 0` as to_boolean(optional<bool> && bool), where C++ converts the
// optional by PRESENCE, so `indentGroups: false` would indent (docs/compiler-requests/logical-and-optional-operand-truthiness.md).
// Each operand is tested for its JavaScript truthiness here. Otherwise the emitted body: optional timestamp,
// optional level name (unknown for a level outside the table), the channel or [flight], two spaces per open
// group, then the string or the JSON of the record, joined by single spaces.
inline flight::Ref<LogFormatter> create_text_log_formatter(std::optional<flight::Ref<indent_groups_level_prefix_timestamp_85ef46f909e535f5>> options = std::nullopt) {
  const auto opts = options.value_or(flight::make_ref<indent_groups_level_prefix_timestamp_85ef46f909e535f5>(indent_groups_level_prefix_timestamp_85ef46f909e535f5{}));
  return [opts](flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<LogEntry>>>> entry) -> flight::String {
    const flight::types::LogLevel level_2 = flight::row_get<flight::RowKey<"level">>(entry);
    const std::optional<flight::String> channel = flight::row_get<flight::RowKey<"channel">>(entry);
    const LogData data = flight::row_get<flight::RowKey<"data">>(entry);
    flight::Array<flight::String> parts;
    if (flight::to_boolean(opts->timestamp)) {
      parts.push(flight::String("t=") + flight::number_to_fixed(timestamp(), 2.0));
    }
    if (flight::to_boolean(opts->level_prefix)) {
      parts.push(level_names.get(static_cast<double>(level_2)).value_or(flight::String("unknown")));
    }
    parts.push(channel.has_value() ? flight::String("[") + channel.value() + flight::String("]") : flight::String("[flight]"));
    if (flight::to_boolean(opts->indent_groups) && (group_depth > 0.0)) {
      parts.push(flight::String("  ").repeat(group_depth));
    }
    if (const auto* text = std::get_if<flight::String>(&data)) {
      parts.push(*text);
    } else {
      parts.push(flight::Json::stringify(std::get<0>(data), std::nullopt, std::nullopt));
    }
    return parts.join(flight::String(" "));
  };
}



inline double end_log_timer(flight::Ref<LogTimer> timer) {
  const double elapsed = timestamp() - timer->started_at;
  flight::Record<flight::String, flight::Any> fields{
      {flight::String("label"), flight::Any(timer->label)},
      {flight::String("elapsedMs"), flight::Any(elapsed)},
  };
  log_debug(std::move(fields), std::optional<std::optional<flight::String>>{timer->channel});
  return elapsed;
}

inline flight::Ref<LogTimer> start_log_timer(flight::String label, std::optional<std::optional<flight::String>> channel = std::nullopt) {
  channel = channel.value_or(std::nullopt);
  return flight::make_ref<LogTimer>(LogTimer{.label = label, .channel = channel.value(), .started_at = timestamp()});
}



#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_SIZE_INTERVAL_MS_014EB73ADB446976
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_SIZE_INTERVAL_MS_014EB73ADB446976
struct size_interval_ms_014eb73adb446976 : public flight::ReferenceEnabled {
  std::optional<double> size;
  std::optional<double> interval_ms;
};
#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_SIZE_INTERVAL_MS_014EB73ADB446976


// OVERRIDE: createBufferedLogSink. Emitted at a627849 with the source's closures over `handle`, the const they
// precede: legal in JavaScript, where they run later, and an undeclared name in C++ (log-native-compile-defects.md,
// row 10). The closures here hold the sink's state directly instead of looking it up by handle; the state is set
// once per handle and never replaced, so every lookup the source makes answers this same object. Ownership follows
// JavaScript reachability without a cycle: the sink and a running interval hold the state, as their closures keep
// it alive in JavaScript, and the state's own flush holds it weakly, which is the reference a collector would
// break. Entries are buffered as fresh copies of { level, channel, data } and delivered to the target in order:
// on reaching `size`, on each `intervalMs` tick, on flushLogSink, and on disposeLogSink. The host always has an
// interval timer, so `typeof setInterval !== 'undefined'` is true here.
inline flight::Ref<BufferedLogSink> create_buffered_log_sink(flight::Ref<LogSink> target, std::optional<flight::Ref<size_interval_ms_014eb73adb446976>> options = std::nullopt) {
  const auto opts = options.value_or(flight::make_ref<size_interval_ms_014eb73adb446976>(size_interval_ms_014eb73adb446976{}));
  const double size = opts->size.value_or(100.0);
  const double interval_ms = opts->interval_ms.value_or(1000.0);
  auto state = flight::make_ref<BufferedLogSinkState>(BufferedLogSinkState{.buf = flight::Array<flight::Ref<LogEntry>>{}, .flush = {}, .timer = std::nullopt});
  const std::weak_ptr<BufferedLogSinkState> weak_state = state;
  state->flush = [weak_state, target]() {
    const auto held = weak_state.lock();
    if (!held || held->buf.size() == 0) {
      return;
    }
    const flight::Array<flight::Ref<LogEntry>> batch = held->buf.splice(0.0);
    for (const auto& entry : batch) {
      target(entry);
    }
  };
  flight::Ref<LogSink> sink = [state, size](flight::Ref<LogEntry> entry) {
    state->buf.push(flight::make_ref<LogEntry>(LogEntry{.level = entry->level, .channel = entry->channel, .data = entry->data}));
    if (static_cast<double>(state->buf.size()) >= size) {
      state->flush();
    }
  };
  auto handle = initialize_log_entity(flight::make_ref<BufferedLogSink>(BufferedLogSink{.sink = sink}));
  if (interval_ms > 0.0) {
    state->timer = flight::host::set_interval([state]() { state->flush(); }, interval_ms);
  }
  buffered_sink_states.set(handle, state);
  return handle;
}


// OVERRIDE: createChildLogContext. Emitted at a627849 as `auto merged = {};` -- the spread dropped -- and a cast
// of an anonymous literal to LogContext (log-native-compile-defects.md, rows 8 and 12).
//   const merged = { ...parent.fields, ...fields };
//   return initializeLogEntity({ channel: channel !== undefined ? channel : parent.channel, fields: merged });
// The child's fields win; `channel` absent (undefined) inherits the parent's, while an explicit null clears it --
// the outer optional is the undefined state, the inner one null.
inline flight::Ref<LogContext> create_child_log_context(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<LogContext>>>> parent, flight::Record<flight::String, flight::Any> fields, std::optional<std::optional<flight::String>> channel = std::nullopt) {
  flight::Record<flight::String, flight::Any> merged;
  flight::object_assign(merged, flight::row_get<flight::RowKey<"fields">>(parent), fields);
  return initialize_log_entity(flight::make_ref<LogContext>(LogContext{
      .channel = channel.has_value() ? channel.value() : flight::row_get<flight::RowKey<"channel">>(parent), .fields = merged}));
}

#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_CHANNEL_FIELDS_1F56163CB6145760
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_CHANNEL_FIELDS_1F56163CB6145760
struct channel_fields_1f56163cb6145760 : public flight::ReferenceEnabled {
  std::optional<flight::String> channel;
  flight::Record<flight::String, flight::Any> fields;
};
#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_CHANNEL_FIELDS_1F56163CB6145760

inline flight::Ref<LogContext> create_log_context(std::optional<flight::String> channel, std::optional<flight::Record<flight::String, flight::Any>> fields = std::nullopt) {
  fields = fields.value_or(flight::Record<flight::String, flight::Any>{});
  return initialize_log_entity(flight::make_ref<LogContext>(LogContext{.channel = channel, .fields = fields.value()}));
}

#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_NAME_FIELDS_CHANNEL_2C921024833B1D00
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_NAME_FIELDS_CHANNEL_2C921024833B1D00
struct name_fields_channel_2c921024833b1d00 : public flight::ReferenceEnabled {
  flight::String name;
  flight::Record<flight::String, flight::Any> fields;
  std::optional<flight::String> channel;
};
#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_NAME_FIELDS_CHANNEL_2C921024833B1D00

inline flight::Ref<LogSpan> create_log_span(flight::String name, std::optional<flight::Record<flight::String, flight::Any>> fields = std::nullopt, std::optional<std::optional<flight::String>> channel = std::nullopt) {
  fields = fields.value_or(flight::Record<flight::String, flight::Any>{});
  channel = channel.value_or(std::nullopt);
  return initialize_log_entity(flight::make_ref<LogSpan>(LogSpan{.name = name, .fields = fields.value(), .channel = channel.value()}));
}

#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_SINK_65EED447EB7D5CB3
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_SINK_65EED447EB7D5CB3
struct sink_65eed447eb7d5cb3 : public flight::ReferenceEnabled {
  flight::Ref<LogSink> sink;
};
#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_SINK_65EED447EB7D5CB3

inline flight::Ref<MemoryLogSink> create_memory_log_sink(double capacity) {
  flight::Ref<MemoryLogSinkState> state = flight::make_ref<MemoryLogSinkState>(MemoryLogSinkState{.buf = flight::Array<flight::Ref<LogEntry>>{}, .head = 0.0});
  flight::Ref<LogSink> sink = [=](flight::Ref<LogEntry> entry) {
  flight::Ref<LogEntry> stored = flight::make_ref<LogEntry>(LogEntry{.level = entry->level, .channel = entry->channel, .data = entry->data});
  if ((static_cast<double>(state->buf.size()) < capacity)) {
    state->buf.push(stored);
  }
  else {
    (state->buf.element(state->head) = stored);
    (state->head = std::fmod((state->head + 1.0), capacity));
  }
};
  flight::Ref<MemoryLogSink> handle = initialize_log_entity(flight::make_ref<MemoryLogSink>(MemoryLogSink{.sink = sink}));
  memory_sink_states.set(handle, state);
  return handle;
}

#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_PER_CHANNEL_MAX_PER_INTERVAL_INTERVAL_MS_B055D1B6AF9DA40A
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_PER_CHANNEL_MAX_PER_INTERVAL_INTERVAL_MS_B055D1B6AF9DA40A
struct per_channel_max_per_interval_interval_ms_b055d1b6af9da40a : public flight::ReferenceEnabled {
  std::optional<bool> per_channel;
  double max_per_interval;
  double interval_ms;
};
#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_LOG_PER_CHANNEL_MAX_PER_INTERVAL_INTERVAL_MS_B055D1B6AF9DA40A


// OVERRIDE: createRateLimitedLogSink, emitted at a627849; its sink and handle are carried over this module's
// LogSink and entity spellings. Each entry spends one unit of the current window's budget for its key -- the
// entry's channel when perChannel, otherwise one shared key -- and is dropped when the budget is spent. A window
// opens at creation and is replaced, with every count cleared, by the first entry at or after `intervalMs`.
// The counts map is shared by the sink's copies, as the closure's one Map is in JavaScript.
inline flight::Ref<RateLimitedLogSink> create_rate_limited_log_sink(flight::Ref<LogSink> target, flight::Ref<per_channel_max_per_interval_interval_ms_b055d1b6af9da40a> options) {
  const bool per_channel = options->per_channel.value_or(false);
  const double max_per_interval = options->max_per_interval;
  const double interval_ms = options->interval_ms;
  auto counts = flight::Map<std::optional<flight::String>, double>();
  auto window_start = std::make_shared<double>(timestamp());
  flight::Ref<LogSink> sink = [=](flight::Ref<LogEntry> entry) {
    const double now = timestamp();
    if ((now - *window_start) >= interval_ms) {
      counts.clear();
      *window_start = now;
    }
    const std::optional<flight::String> key = per_channel ? entry->channel : std::nullopt;
    const double current = counts.get(key).value_or(0.0);
    if (current >= max_per_interval) {
      return;
    }
    counts.set(key, current + 1.0);
    target(entry);
  };
  return initialize_log_entity(flight::make_ref<RateLimitedLogSink>(RateLimitedLogSink{.sink = sink}));
}


// OVERRIDE: enableLogSignals. The emission builds the group through an anonymous literal cast to LogSignals and
// an initializer list inside `?:` (log-native-compile-defects.md, rows 12 and 13). The source creates one entity
// with an onLogEntry and an onLogError signal on first call and returns that same entity on every later call;
// _emitToSinks then emits every entry on onLogEntry and Error entries on onLogError too.
inline flight::Ref<LogSignals> enable_log_signals() {
  if (log_signals.has_value()) {
    return log_signals.value();
  }
  using Handler = std::function<void(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<LogEntry>>>>)>;
  log_signals = initialize_log_entity(flight::make_ref<LogSignals>(
      LogSignals{.on_log_entry = flight::signals::create_signal<Handler>(), .on_log_error = flight::signals::create_signal<Handler>()}));
  return log_signals.value();
}


// NOT GENERATED: variable (binding) -- source line 802
// refusal: cpp-contextual-union-missing-expression-type:optionalSingle [compiler-restriction]
//
// The source it stood for:
//   const _defaultJsonFormatter: LogFormatter = (entry: Readonly<LogEntry>): string => {
//     const { level, channel, data } = entry;
//     return JSON.stringify({
//       __flight: true,
//       t: _timestamp(),
//       level: _levelNames[level],
//       channel,
//       data: typeof data === 'string' ? { msg: data } : data,
//     });
//   };
//   
//   /* eslint-disable no-console -- this is the console-capture sink; writing to the console is its job */
// cpp emission failed for @flighthq/log/packages/log/src/log.ts: contextual optionalSingle construction requires
// expression type evidence


// NOT GENERATED: function _writeConsoleCaptureEntry -- source line 814
// refusal: cpp-contextual-union-missing-expression-type:optionalSingle [compiler-restriction]
//
// The source it stood for:
//   function _writeConsoleCaptureEntry(entry: Readonly<LogEntry>, envelopeFormatter: LogFormatter): void {
//     if (typeof console === 'undefined') return;
//     const { level, channel } = entry;
//     // The capture record: every level, as a tagged JSON line the collector parses.
//     console.debug(envelopeFormatter(entry));
//     // The human-readable subset.
//     if (level !== LogLevel.None && _consoleLevel >= level) {
//       const method = _consoleMethods[level];
//       const prefix = channel !== null ? `[${channel}]` : '[flight]';
//       const { data } = entry;
//       if (typeof data === 'string') console[method](`${prefix} ${data}`);
//       else console[method](prefix, data);
//     }
//   }
//   
// cpp emission failed for @flighthq/log/packages/log/src/log.ts: contextual optionalSingle construction requires
// expression type evidence

inline flight::Ref<LogSink> create_console_capture_sink(std::optional<flight::Ref<formatter_c3f72d72feb278c6>> options = std::nullopt) {
  // STUBBED in this override. The emitter produced a body for this function that is not valid C++,
  // and recovering what it meant requires re-deriving the expression from the TypeScript. Throwing
  // names the gap at the one moment it could matter instead of logging nothing and looking fine.
  throw flight::Error(flight::String("flight::log::create_console_capture_sink is not implemented in this profile: the emitter produced invalid C++ for it"));
}


// NOT GENERATED: function _writeConsoleLogEntry -- source line 829
// refusal: cpp-closed-key-access-without-object-storage [compiler-restriction]
//
// The source it stood for:
//   function _writeConsoleLogEntry(entry: Readonly<LogEntry>, formatter: LogFormatter): void {
//     if (typeof console === 'undefined') return;
//     console[_consoleMethods[entry.level]](formatter(entry));
//   }
//   /* eslint-enable no-console */
//   
// cpp emission failed for @flighthq/log/packages/log/src/log.ts: closed-key element access over an unevidenced
// receiver requires represented object storage: the finite key set proves which members may be selected, but the
// receiver does not retain one exact flight::Ref owner whose shape defines those members. Accept a concrete
// declared object type at this boundary (or a Record when keyed storage is intended); the compiler will not
// cast, copy, or materialize an owner to make the access possible

inline flight::Ref<LogSink> create_console_log_sink(std::optional<flight::Ref<formatter_c3f72d72feb278c6>> options = std::nullopt) {
  // STUBBED in this override. The emitter produced a body for this function that is not valid C++,
  // and recovering what it meant requires re-deriving the expression from the TypeScript. Throwing
  // names the gap at the one moment it could matter instead of logging nothing and looking fine.
  throw flight::Error(flight::String("flight::log::create_console_log_sink is not implemented in this profile: the emitter produced invalid C++ for it"));
}

} // namespace flight::log
