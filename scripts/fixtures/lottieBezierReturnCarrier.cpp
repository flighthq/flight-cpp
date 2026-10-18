// Existing-family component counterexamples; this does not implement the Bezier functions.
#include <flight/json_scalar.hpp>
#include <functional>
#include <iostream>
#include <type_traits>

using Cells = flight::Array<flight::JsonScalarCell>;
static_assert(!std::is_constructible_v<flight::Array<double>, Cells>);

int main() {
  int checks = 0, failures = 0;
  auto check = [&](bool value) { ++checks; if (!value) { ++failures; std::cerr << "failed " << checks << '\n'; } };
  const auto project = [](const flight::Any& value) { return flight::JsonScalarCell(value); };
  const auto source = flight::Any(flight::Json::parse(R"(["x",2])"));
  const auto view = flight::SequenceView<flight::JsonScalarCell>::from_erased_array(source, project);
  Cells returned{view[0], view[1]};
  std::function<Cells()> callback = [returned] { return returned; };
  const auto escaped = callback();
  check(escaped.identity() == returned.identity());
  check(escaped[0].raw().as_string() == flight::String("x") && escaped[1].as_native_number() == 2);
  bool native_rejected = false;
  try { (void)escaped[0].as_native_number(); } catch (const flight::NativeScalarEscapeError&) { native_rejected = true; }
  check(native_rejected);

  std::weak_ptr<flight::JsonObject> lifetime;
  Cells object_result;
  {
    const auto input = flight::Json::parse(R"([{"value":1},2])");
    const auto object = input.as_array()[0].object_owner(); lifetime = object;
    const auto raw = flight::Any(input);
    const auto sequence = flight::SequenceView<flight::JsonScalarCell>::from_erased_array(raw, project);
    object_result.push(sequence[0]);
    check(object_result[0].raw().identity() == flight::Any(input.as_array()[0]).identity());
    object->set("value", flight::JsonValue(9.0));
    check(flight::read_json_scalar_property(object_result[0].raw(), "value").as_native_number() == 9);
  }
  check(!lifetime.expired()); object_result = Cells{}; check(lifetime.expired());

  bool string_rejected_at_iteration = false;
  const auto string_view = flight::SequenceView<flight::JsonScalarCell>::from_erased_array(flight::Any(flight::String("ab")), project);
  try { (void)string_view.size(); } catch (const flight::TypeError&) { string_rejected_at_iteration = true; }
  check(string_rejected_at_iteration);

  flight::Array<double> snapshot_source{1}; const flight::SequenceView<double> snapshot_view(snapshot_source);
  int snapshot_visits = 0;
  for (const auto value : snapshot_view) { ++snapshot_visits; if (value == 1) snapshot_source.push(2); }
  check(snapshot_visits == 1);
  flight::Array<double> live_source{1}; int live_visits = 0;
  for (const auto value : live_source.values()) { ++live_visits; if (value == 1) live_source.push(2); }
  check(live_visits == 2);
  std::cout << "{\"checks\":" << checks << ",\"failures\":" << failures
            << ",\"snapshotVisits\":" << snapshot_visits << ",\"liveVisits\":" << live_visits
            << ",\"nativeNumberRejectsString\":" << (native_rejected ? "true" : "false")
            << ",\"arrayViewRejectsStringIterator\":" << (string_rejected_at_iteration ? "true" : "false") << "}\n";
  return failures ? 1 : 0;
}
