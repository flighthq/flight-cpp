#include <flight/log/contract.hpp>
#include <flight/snapshot/contract.hpp>

#include <cmath>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using flight::String;
using flight::snapshot::SnapshotValue;
using Value = SnapshotValue;
using Entry = SnapshotValue::ObjectEntry;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << message << '\n';
  ++failures;
}

Value object(std::initializer_list<Entry> entries = {}) {
  return Value::object(entries);
}

Value array(std::initializer_list<Value> values = {}) {
  return Value::array(values);
}

const Value& field(const Value& value, const char* key) {
  const auto* result = value.object_get(String(key));
  if (result == nullptr) throw std::logic_error("missing snapshot test field");
  return *result;
}

Value& mutable_field(Value& value, const char* key) {
  auto* result = value.object_get_mutable(String(key));
  if (result == nullptr) throw std::logic_error("missing mutable snapshot test field");
  return *result;
}

double number(const Value& value, const char* key) {
  return field(value, key).as_number();
}

String string(const Value& value, const char* key) {
  return field(value, key).as_string();
}

void reset_guards_and_log() {
  flight::snapshot::disable_snapshot_guards();
  flight::log::clear_log_sinks();
  flight::log::clear_log_once_keys();
}

std::vector<std::string> capture_guard_messages(const Value& source, bool enabled = true) {
  reset_guards_and_log();
  std::vector<std::string> messages;
  flight::types::LogSink sink = [&](flight::Ref<flight::types::LogEntry> entry) {
    const auto* data = std::get_if<flight::Record<String, flight::Any>>(&entry->data);
    if (data == nullptr) return;
    const auto message = data->get(String("message"));
    if (message.has_value() && message->kind() == flight::AnyKind::string) {
      messages.push_back(message->as_string().to_utf8());
    }
  };
  flight::log::set_log_sink(sink);
  if (enabled) flight::snapshot::enable_snapshot_guards();
  static_cast<void>(flight::snapshot::capture_snapshot(source));
  reset_guards_and_log();
  return messages;
}

bool contains_message(const std::vector<std::string>& messages, const std::string& text) {
  for (const auto& message : messages) {
    if (message.find(text) != std::string::npos) return true;
  }
  return false;
}

void test_capture_snapshot() {
  // Ports all 12 captureSnapshot.test.ts declarations: deep equality, recursive freeze,
  // source independence, nested array identity, empty/null/undefined, both cycle shapes, shared
  // subtrees, guard source identity/pre-freeze timing, and clearing the guard.
  auto source = object({
      {String("x"), 1},
      {String("nested"), object({{String("y"), 2}})},
      {String("items"), array({3, 4})},
  });
  const auto snapshot = flight::snapshot::capture_snapshot(source);
  check(flight::snapshot::equals_snapshot(snapshot, source),
        "capture did not return a deep-equal copy");
  check(snapshot.is_frozen() && field(snapshot, "nested").is_frozen() &&
            field(snapshot, "items").is_frozen(),
        "capture did not deep-freeze every nested container");
  check(!snapshot.same_identity(source) &&
            !field(snapshot, "nested").same_identity(field(source, "nested")) &&
            !field(snapshot, "items").same_identity(field(source, "items")),
        "capture retained a source container identity");

  source.object_set(String("x"), 99);
  mutable_field(source, "nested").object_set(String("y"), 99);
  mutable_field(source, "items").array_push(5);
  check(number(snapshot, "x") == 1 && number(field(snapshot, "nested"), "y") == 2 &&
            field(snapshot, "items").size() == 2,
        "source mutation changed a captured snapshot");

  const auto empty = flight::snapshot::capture_snapshot(object());
  check(empty.is_object() && empty.size() == 0 && empty.is_frozen(),
        "empty object capture changed shape or freeze state");
  const auto absent = flight::snapshot::capture_snapshot(object({
      {String("a"), flight::null},
      {String("b"), flight::undefined},
      {String("c"), 1},
  }));
  check(field(absent, "a").kind() == Value::Kind::null &&
            field(absent, "b").kind() == Value::Kind::undefined &&
            number(absent, "c") == 1,
        "capture collapsed null, undefined, or a numeric leaf");

  auto self = object({{String("name"), String("root")}});
  self.object_set(String("self"), self);
  const auto self_snapshot = flight::snapshot::capture_snapshot(self);
  check(self_snapshot.is_frozen() && field(self_snapshot, "self").same_identity(self_snapshot),
        "self cycle was not cloned and frozen with one identity");

  auto a = object({{String("tag"), String("a")}});
  auto b = object({{String("tag"), String("b")}, {String("a"), a}});
  a.object_set(String("b"), b);
  const auto mutual = flight::snapshot::capture_snapshot(a);
  check(field(field(mutual, "b"), "a").same_identity(mutual) &&
            field(mutual, "b").is_frozen(),
        "mutual cycle did not preserve identity and freeze state");

  auto shared = object({{String("v"), 1}});
  auto diamond_source = object({{String("left"), shared}, {String("right"), shared}});
  const auto diamond = flight::snapshot::capture_snapshot(diamond_source);
  check(field(diamond, "left").same_identity(field(diamond, "right")) &&
            field(diamond, "left").is_frozen() &&
            !field(diamond, "left").same_identity(shared),
        "shared subtree was duplicated, left mutable, or aliased to the source");

  const void* guarded_identity = nullptr;
  bool guarded_source_was_frozen = true;
  flight::snapshot::set_snapshot_capture_guard(
      flight::snapshot::SnapshotCaptureGuard{[&](const Value& guarded) {
        guarded_identity = guarded.identity();
        guarded_source_was_frozen = guarded.is_frozen();
      }});
  auto guarded = object({{String("hp"), 1}});
  static_cast<void>(flight::snapshot::capture_snapshot(guarded));
  check(guarded_identity == guarded.identity(), "capture guard did not receive the source identity");
  check(!guarded_source_was_frozen, "capture guard ran against the frozen clone");

  int guard_calls = 0;
  flight::snapshot::set_snapshot_capture_guard(
      flight::snapshot::SnapshotCaptureGuard{[&](const Value&) { ++guard_calls; }});
  static_cast<void>(flight::snapshot::capture_snapshot(object({{String("a"), 1}})));
  flight::snapshot::set_snapshot_capture_guard(std::nullopt);
  static_cast<void>(flight::snapshot::capture_snapshot(object({{String("a"), 2}})));
  check(guard_calls == 1, "clearing the capture guard did not stop later calls");
}

void test_snapshot_guards() {
  // Ports all five enableSnapshotGuards.test.ts declarations.
  auto disabled_messages = capture_guard_messages(
      object({{String("when"), Value::non_plain(String("Date"))}}), false);
  check(disabled_messages.empty(), "disabled snapshot guards emitted a warning");

  const auto plain_messages = capture_guard_messages(object({
      {String("hp"), 1},
      {String("items"), array({object({{String("id"), String("a")}})})},
      {String("nested"), object({{String("deep"), object({{String("ok"), true}})}})},
  }));
  check(plain_messages.empty(), "plain acyclic snapshot source emitted a warning");

  const auto map_messages = capture_guard_messages(
      object({{String("byId"), Value::non_plain(String("Map"))}}));
  check(contains_message(map_messages, "Map"), "non-plain Map warning did not name Map");

  auto cyclic = object({{String("name"), String("root")}});
  cyclic.object_set(String("self"), cyclic);
  const auto cycle_messages = capture_guard_messages(cyclic);
  check(contains_message(cycle_messages, "cycle"), "cycle warning did not name the cycle");

  bool terminated = true;
  try {
    auto first = object();
    auto second = object({{String("a"), first}});
    first.object_set(String("b"), second);
    static_cast<void>(capture_guard_messages(first));
  } catch (...) {
    terminated = false;
  }
  check(terminated, "snapshot guard did not terminate on a mutual cycle");
}

void test_equals_snapshot() {
  // Ports all 11 equalsSnapshot.test.ts declarations, including exact strict-equality leaf rules.
  const auto equal_a = flight::snapshot::capture_snapshot(object({
      {String("x"), 1},
      {String("nested"), object({{String("y"), 2}})},
      {String("items"), array({3, 4})},
  }));
  const auto equal_b = flight::snapshot::capture_snapshot(object({
      {String("items"), array({3, 4})},
      {String("nested"), object({{String("y"), 2}})},
      {String("x"), 1},
  }));
  check(flight::snapshot::equals_snapshot(equal_a, equal_b),
        "deep-equal snapshots or reordered object keys compared unequal");

  check(!flight::snapshot::equals_snapshot(
            flight::snapshot::capture_snapshot(object({{String("x"), 1}, {String("nested"), object({{String("y"), 2}})}})),
            flight::snapshot::capture_snapshot(object({{String("x"), 1}, {String("nested"), object({{String("y"), 3}})}}))),
        "differing leaf compared equal");
  check(!flight::snapshot::equals_snapshot(
            flight::snapshot::capture_snapshot(object({{String("x"), 1}})),
            flight::snapshot::capture_snapshot(object({{String("x"), 1}, {String("y"), 2}}))),
        "differing object shape compared equal");
  check(!flight::snapshot::equals_snapshot(
            flight::snapshot::capture_snapshot(object({{String("items"), array({1, 2})}})),
            flight::snapshot::capture_snapshot(object({{String("items"), array({1, 2, 3})}}))),
        "differing array length compared equal");
  check(flight::snapshot::equals_snapshot(
            flight::snapshot::capture_snapshot(object()),
            flight::snapshot::capture_snapshot(object())),
        "two empty objects compared unequal");
  check(!flight::snapshot::equals_snapshot(
            flight::snapshot::capture_snapshot(array({1, 2})),
            flight::snapshot::capture_snapshot(object({{String("0"), 1}, {String("1"), 2}}))),
        "array compared equal to indexed object");

  auto deep_one = object({{String("level1"), object({{String("level2"), object({
      {String("level3"), object({{String("value"), 1}})},
  })}})}});
  auto deep_two = object({{String("level1"), object({{String("level2"), object({
      {String("level3"), object({{String("value"), 2}})},
  })}})}});
  auto deep_same = object({{String("level1"), object({{String("level2"), object({
      {String("level3"), object({{String("value"), 1}})},
  })}})}});
  check(!flight::snapshot::equals_snapshot(
            flight::snapshot::capture_snapshot(deep_one),
            flight::snapshot::capture_snapshot(deep_two)),
        "three-level differing objects compared equal");
  check(flight::snapshot::equals_snapshot(
            flight::snapshot::capture_snapshot(deep_one),
            flight::snapshot::capture_snapshot(deep_same)),
        "three-level equal objects compared unequal");
  check(!flight::snapshot::equals_snapshot(
            flight::snapshot::capture_snapshot(object({{String("tag"), flight::null}})),
            flight::snapshot::capture_snapshot(object({{String("tag"), 0}}))),
        "null leaf compared equal to zero");
  check(!flight::snapshot::equals_snapshot(
            flight::snapshot::capture_snapshot(object({{String("v"), std::nan("")}})),
            flight::snapshot::capture_snapshot(object({{String("v"), std::nan("")}}))),
        "two NaN leaves compared equal");
  check(flight::snapshot::equals_snapshot(
            flight::snapshot::capture_snapshot(object({{String("v"), -0.0}})),
            flight::snapshot::capture_snapshot(object({{String("v"), 0.0}}))),
        "negative and positive zero compared unequal");
}

void test_interpolate_snapshots() {
  // Ports all 14 interpolateSnapshots.test.ts declarations.
  const auto a = flight::snapshot::capture_snapshot(
      object({{String("x"), 0}, {String("y"), 10}, {String("name"), String("a")}}));
  const auto b = flight::snapshot::capture_snapshot(
      object({{String("x"), 10}, {String("y"), 0}, {String("name"), String("b")}}));
  auto out = object({{String("x"), 0}, {String("y"), 0}, {String("name"), String()}});
  flight::snapshot::interpolate_snapshots(a, b, 0.5, out);
  check(number(out, "x") == 5 && number(out, "y") == 5 && string(out, "name") == String("b"),
        "half interpolation did not lerp numerics and snap string");

  auto at_start = object({{String("x"), -1}, {String("y"), -1}});
  auto at_end = object({{String("x"), -1}, {String("y"), -1}});
  flight::snapshot::interpolate_snapshots(a, b, 0, at_start);
  flight::snapshot::interpolate_snapshots(a, b, 1, at_end);
  check(number(at_start, "x") == 0 && number(at_start, "y") == 10 &&
            number(at_end, "x") == 10 && number(at_end, "y") == 0,
        "interpolation endpoints were not exact");

  const auto nested_a = flight::snapshot::capture_snapshot(object({
      {String("pos"), object({{String("x"), 0}, {String("y"), 0}})},
      {String("hp"), 100},
  }));
  const auto nested_b = flight::snapshot::capture_snapshot(object({
      {String("pos"), object({{String("x"), 100}, {String("y"), 200}})},
      {String("hp"), 0},
  }));
  auto nested_out = object({
      {String("pos"), object({{String("x"), 0}, {String("y"), 0}})},
      {String("hp"), 0},
  });
  flight::snapshot::interpolate_snapshots(
      nested_a, nested_b, 0.5, nested_out,
      flight::Array<String>{String("pos.x")});
  check(number(field(nested_out, "pos"), "x") == 50 &&
            number(field(nested_out, "pos"), "y") == 200 && number(nested_out, "hp") == 0,
        "nested dotted schema did not interpolate only pos.x");

  auto selected_out = object({{String("x"), 0}, {String("y"), 0}});
  flight::snapshot::interpolate_snapshots(
      a, b, 0.5, selected_out, flight::Array<String>{String("x")});
  check(number(selected_out, "x") == 5 && number(selected_out, "y") == 0,
        "root schema did not snap the unlisted numeric path");

  const auto quarter_a = flight::snapshot::capture_snapshot(
      object({{String("pos"), object({{String("x"), 0}, {String("y"), 0}})}}));
  const auto quarter_b = flight::snapshot::capture_snapshot(
      object({{String("pos"), object({{String("x"), 100}, {String("y"), 40}})}}));
  auto quarter_out = object({{String("pos"), object({{String("x"), 0}, {String("y"), 0}})}});
  flight::snapshot::interpolate_snapshots(quarter_a, quarter_b, 0.25, quarter_out);
  check(number(field(quarter_out, "pos"), "x") == 25 &&
            number(field(quarter_out, "pos"), "y") == 10,
        "nested numeric interpolation was not exact");

  const auto shape_a = flight::snapshot::capture_snapshot(
      object({{String("x"), 10}, {String("onlyA"), 99}}));
  const auto shape_b = flight::snapshot::capture_snapshot(
      object({{String("x"), 20}, {String("onlyB"), String("hello")}}));
  auto shape_out = object({{String("x"), 0}});
  flight::snapshot::interpolate_snapshots(shape_a, shape_b, 0.5, shape_out);
  check(number(shape_out, "x") == 15 && string(shape_out, "onlyB") == String("hello") &&
            shape_out.object_get(String("onlyA")) == nullptr,
        "destination-shape interpolation mishandled a-only or b-only keys");

  const auto clamp_a = flight::snapshot::capture_snapshot(object({{String("x"), 0}}));
  const auto clamp_b = flight::snapshot::capture_snapshot(object({{String("x"), 10}}));
  auto under = object({{String("x"), -1}});
  auto over = object({{String("x"), -1}});
  flight::snapshot::interpolate_snapshots(clamp_a, clamp_b, -5, under);
  flight::snapshot::interpolate_snapshots(clamp_a, clamp_b, 5, over);
  check(number(under, "x") == 0 && number(over, "x") == 10,
        "interpolation did not clamp t to both bounds");

  const auto arrays_a = flight::snapshot::capture_snapshot(
      object({{String("points"), array({0, 100})}}));
  const auto arrays_b = flight::snapshot::capture_snapshot(
      object({{String("points"), array({10, 0})}}));
  auto arrays_out = object({{String("points"), array({0, 0})}});
  flight::snapshot::interpolate_snapshots(arrays_a, arrays_b, 0.5, arrays_out);
  check(field(arrays_out, "points").array_at(0).as_number() == 5 &&
            field(arrays_out, "points").array_at(1).as_number() == 50,
        "array numeric interpolation changed positional ordering");

  const auto type_a = flight::snapshot::capture_snapshot(object({{String("x"), 5}}));
  const auto type_b = flight::snapshot::capture_snapshot(object({{String("x"), String("hello")}}));
  auto type_out = object({{String("x"), 0}});
  flight::snapshot::interpolate_snapshots(type_a, type_b, 0.5, type_out);
  check(string(type_out, "x") == String("hello"),
        "number-to-non-number interpolation did not snap to b");

  const auto null_a = flight::snapshot::capture_snapshot(
      object({{String("value"), 1}, {String("tag"), flight::null}}));
  const auto null_b = flight::snapshot::capture_snapshot(
      object({{String("value"), 3}, {String("tag"), flight::null}}));
  auto null_out = object({{String("value"), 0}, {String("tag"), 7}});
  flight::snapshot::interpolate_snapshots(null_a, null_b, 0.5, null_out);
  check(number(null_out, "value") == 2 && field(null_out, "tag").kind() == Value::Kind::null,
        "interpolation did not snap a null field");

  const auto players_a = flight::snapshot::capture_snapshot(object({
      {String("players"), array({object({{String("health"), 0}}), object({{String("health"), 0}})})},
  }));
  const auto players_b = flight::snapshot::capture_snapshot(object({
      {String("players"), array({object({{String("health"), 100}}), object({{String("health"), 100}})})},
  }));
  auto players_out = object({
      {String("players"), array({object({{String("health"), 0}}), object({{String("health"), 0}})})},
  });
  flight::snapshot::interpolate_snapshots(
      players_a, players_b, 0.5, players_out,
      flight::Array<String>{String("players.0.health")});
  check(number(field(players_out, "players").array_at(0), "health") == 50 &&
            number(field(players_out, "players").array_at(1), "health") == 100,
        "array-indexed schema path did not select exactly one element");

  auto empty_schema_out = object({{String("hp"), 0}});
  flight::snapshot::interpolate_snapshots(
      flight::snapshot::capture_snapshot(object({{String("hp"), 0}})),
      flight::snapshot::capture_snapshot(object({{String("hp"), 100}})),
      0.5, empty_schema_out, flight::Array<String>{});
  check(number(empty_schema_out, "hp") == 100,
        "empty schema was collapsed into no-schema interpolation");

  auto no_schema_out = object({{String("hp"), 0}, {String("mp"), 0}});
  flight::snapshot::interpolate_snapshots(
      flight::snapshot::capture_snapshot(object({{String("hp"), 0}, {String("mp"), 0}})),
      flight::snapshot::capture_snapshot(object({{String("hp"), 100}, {String("mp"), 50}})),
      0.5, no_schema_out);
  check(number(no_schema_out, "hp") == 50 && number(no_schema_out, "mp") == 25,
        "absent schema did not interpolate every numeric leaf");

  const auto lifecycle_a = flight::snapshot::capture_snapshot(object({
      {String("pos"), object({{String("x"), 0}, {String("y"), 0}})},
      {String("tag"), String("a")},
  }));
  const auto lifecycle_b = flight::snapshot::capture_snapshot(object({
      {String("pos"), object({{String("x"), 10}, {String("y"), 20}})},
      {String("tag"), String("b")},
  }));
  auto live = object({
      {String("pos"), object({{String("x"), 0}, {String("y"), 0}})},
      {String("tag"), String("a")},
  });
  flight::snapshot::interpolate_snapshots(lifecycle_a, lifecycle_b, 0.5, live);
  check(number(field(live, "pos"), "x") == 5 && number(field(live, "pos"), "y") == 10 &&
            string(live, "tag") == String("b"),
        "lifecycle interpolation produced the wrong live state");
  flight::snapshot::restore_snapshot(lifecycle_a, live);
  check(number(field(live, "pos"), "x") == 0 && number(field(live, "pos"), "y") == 0 &&
            string(live, "tag") == String("a"),
        "interpolated output could not be restored in place");
}

void test_restore_snapshot() {
  // Ports all 13 restoreSnapshot.test.ts declarations.
  const auto basic = flight::snapshot::capture_snapshot(
      object({{String("x"), 1}, {String("y"), 2}}));
  auto basic_target = object({{String("x"), 0}, {String("y"), 0}});
  flight::snapshot::restore_snapshot(basic, basic_target);
  check(number(basic_target, "x") == 1 && number(basic_target, "y") == 2,
        "restore did not write a snapshot into the live target");

  const auto nested = flight::snapshot::capture_snapshot(object({
      {String("nested"), object({{String("a"), 1}})},
      {String("items"), array({1, 2, 3})},
  }));
  auto nested_target = object({
      {String("nested"), object({{String("a"), 0}})},
      {String("items"), array({0})},
  });
  flight::snapshot::restore_snapshot(nested, nested_target);
  check(number(field(nested_target, "nested"), "a") == 1 &&
            field(nested_target, "items").size() == 3 &&
            field(nested_target, "items").array_at(2).as_number() == 3,
        "restore did not update nested objects and arrays");

  const auto primitive = flight::snapshot::capture_snapshot(Value(42));
  auto primitive_target = object({{String("x"), 1}});
  flight::snapshot::restore_snapshot(primitive, primitive_target);
  check(number(primitive_target, "x") == 1,
        "primitive top-level snapshot was not a no-op");

  auto identity_target = object({{String("x"), 0}});
  const void* target_identity = identity_target.identity();
  flight::snapshot::restore_snapshot(
      flight::snapshot::capture_snapshot(object({{String("x"), 1}})), identity_target);
  check(identity_target.identity() == target_identity,
        "restore replaced the top-level target identity");

  auto nested_identity_target = object({{String("nested"), object({{String("a"), 0}})}});
  const void* nested_identity = field(nested_identity_target, "nested").identity();
  flight::snapshot::restore_snapshot(
      flight::snapshot::capture_snapshot(object({{String("nested"), object({{String("a"), 1}})}})),
      nested_identity_target);
  check(field(nested_identity_target, "nested").identity() == nested_identity &&
            number(field(nested_identity_target, "nested"), "a") == 1,
        "restore replaced a compatible nested container");

  const auto frozen = flight::snapshot::capture_snapshot(
      object({{String("nested"), object({{String("a"), 1}})}}));
  auto mutable_target = object({{String("nested"), object({{String("a"), 0}})}});
  flight::snapshot::restore_snapshot(frozen, mutable_target);
  mutable_field(mutable_target, "nested").object_set(String("a"), 42);
  check(number(field(mutable_target, "nested"), "a") == 42 &&
            number(field(frozen, "nested"), "a") == 1,
        "restored target was frozen or aliased the snapshot");

  auto extras = object({{String("x"), 0}, {String("y"), 5}, {String("z"), 99}});
  flight::snapshot::restore_snapshot(
      flight::snapshot::capture_snapshot(object({{String("x"), 10}})), extras);
  check(number(extras, "x") == 10 && number(extras, "y") == 5 && number(extras, "z") == 99,
        "restore removed target-only keys");

  auto object_to_array = object({{String("data"), object({{String("a"), 1}})}});
  flight::snapshot::restore_snapshot(
      flight::snapshot::capture_snapshot(object({{String("data"), array({1, 2, 3})}})),
      object_to_array);
  check(field(object_to_array, "data").is_array() && field(object_to_array, "data").size() == 3,
        "restore did not replace object with array");

  auto array_to_object = object({{String("data"), array({1, 2, 3})}});
  flight::snapshot::restore_snapshot(
      flight::snapshot::capture_snapshot(object({{String("data"), object({{String("a"), 1}})}})),
      array_to_object);
  check(field(array_to_object, "data").is_object() &&
            number(field(array_to_object, "data"), "a") == 1,
        "restore did not replace array with object");

  auto resized = object({{String("items"), array({9, 9, 9, 9})}});
  flight::snapshot::restore_snapshot(
      flight::snapshot::capture_snapshot(object({{String("items"), array({1, 2})}})), resized);
  check(field(resized, "items").size() == 2 &&
            field(resized, "items").array_at(0).as_number() == 1 &&
            field(resized, "items").array_at(1).as_number() == 2,
        "restore did not resize a numeric array to the snapshot length");

  auto shrink = object({{String("items"), array({
      object({{String("id"), String("x")}}),
      object({{String("id"), String("y")}}),
      object({{String("id"), String("z")}}),
  })}});
  flight::snapshot::restore_snapshot(
      flight::snapshot::capture_snapshot(object({{String("items"), array({
          object({{String("id"), String("a")}}),
      })}})), shrink);
  check(field(shrink, "items").size() == 1 &&
            string(field(shrink, "items").array_at(0), "id") == String("a"),
        "restore left stale trailing entries after an array shrink");

  auto grow = object({{String("items"), array({object({{String("id"), String("x")}})})}});
  flight::snapshot::restore_snapshot(
      flight::snapshot::capture_snapshot(object({{String("items"), array({
          object({{String("id"), String("a")}}),
          object({{String("id"), String("b")}}),
          object({{String("id"), String("c")}}),
      })}})), grow);
  check(field(grow, "items").size() == 3 &&
            string(field(grow, "items").array_at(2), "id") == String("c"),
        "restore did not grow an array in source order");

  auto round_trip = object({
      {String("hp"), 10},
      {String("items"), array({object({{String("id"), String("a")}})})},
  });
  const auto round_snapshot = flight::snapshot::capture_snapshot(round_trip);
  round_trip.object_set(String("hp"), 99);
  mutable_field(round_trip, "items").array_push(object({{String("id"), String("b")}}));
  flight::snapshot::restore_snapshot(round_snapshot, round_trip);
  mutable_field(round_trip, "items").array_at_mutable(0).object_set(String("id"), String("mutated"));
  check(number(round_trip, "hp") == 10 && field(round_trip, "items").size() == 1 &&
            string(field(round_snapshot, "items").array_at(0), "id") == String("a"),
        "capture/mutate/restore round trip aliased or retained a later mutation");
}

void test_native_identity_ordering_and_failures() {
  // Snapshot has no dimension or pixel/byte layout surface. Its relevant layout contract is the
  // ordered array/object graph, covered here with exact key and element ordering.
  const auto ordered = flight::snapshot::capture_snapshot(object({
      {String("z"), 1}, {String("a"), 2}, {String("m"), 3},
  }));
  check(ordered.object_entries().size() == 3 &&
            ordered.object_entries()[0].first == String("z") &&
            ordered.object_entries()[1].first == String("a") &&
            ordered.object_entries()[2].first == String("m"),
        "capture changed insertion ordering of object fields");

  bool frozen_write_failed = false;
  try {
    auto alias = ordered;
    alias.object_set(String("z"), 9);
  } catch (const std::logic_error&) {
    frozen_write_failed = true;
  }
  check(frozen_write_failed, "a frozen snapshot alias accepted a write");

  bool clone_failed = false;
  try {
    static_cast<void>(flight::snapshot::capture_snapshot(
        Value::non_plain(String("Function"), false)));
  } catch (const flight::DataCloneError&) {
    clone_failed = true;
  }
  check(clone_failed, "uncloneable snapshot input did not throw DataCloneError");

  const auto first_map = flight::snapshot::capture_snapshot(Value::non_plain(String("Map")));
  const auto second_map = flight::snapshot::capture_snapshot(Value::non_plain(String("Map")));
  check(!flight::snapshot::equals_snapshot(first_map, second_map) &&
            flight::snapshot::equals_snapshot(first_map, first_map),
        "non-plain strict comparison lost reference identity semantics");
}

}  // namespace

int main() {
  reset_guards_and_log();
  test_capture_snapshot();
  test_snapshot_guards();
  test_equals_snapshot();
  test_interpolate_snapshots();
  test_restore_snapshot();
  test_native_identity_ordering_and_failures();
  reset_guards_and_log();
  return failures == 0 ? 0 : 1;
}
