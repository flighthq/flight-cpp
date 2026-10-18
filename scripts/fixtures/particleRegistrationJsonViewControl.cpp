// Existing runtime capability control for a scope proposal; no production helper ABI is added.
#include <flight/runtime.hpp>
#include <iostream>
#include <optional>
#include <type_traits>

using RecordResult = std::optional<flight::Record<flight::String, flight::Any>>;
static_assert(!std::is_constructible_v<RecordResult, flight::NamedProperties>);
static_assert(std::is_constructible_v<std::optional<flight::NamedProperties>, flight::NamedProperties>);
int main() {
  int failures = 0;
  auto check = [&](bool value, const char* message) { if (!value) { ++failures; std::cerr << message << '\n'; } };
  for (const auto& input : {"null", "0", "true", "\"text\"", "[]", "{}"}) {
    const auto value = flight::Json::parse(input);
    check((value.kind() == flight::JsonValue::Kind::object) == (flight::String(input) == "{}"),
          "existing JSON storage distinguishes the original helper's complete root domain");
  }
  std::optional<flight::NamedProperties> retained;
  std::weak_ptr<flight::JsonObject> lifetime;
  {
    auto value = flight::Json::parse("{\"2\":2,\"1\":1,\"marker\":3,\"__proto__\":4,\"present\":null}");
    lifetime = value.object_owner();
    auto view = flight::named_properties(value);
    check(view.shared_owner()->native_object().get() == value.object_owner().get(),
          "existing read view retains the exact original parsed JSON owner");
    check(view.keys() == std::vector<flight::String>{"1", "2", "marker", "__proto__", "present"},
          "existing JSON view keeps own key order and proto spelling");
    check(view.has("present") && view.get("present").is_null() && !view.has("missing") && view.get("missing").is_undefined(),
          "existing view preserves null and absent properties separately");
    value.as_object().set("marker", flight::JsonValue(7));
    check(view.get("marker").as_number() == 7, "existing view observes original owner mutations");
    retained = view;
  }
  check(!lifetime.expired() && retained->get("marker").as_number() == 7,
        "optional view can retain original parsed owner after local scope");
  retained.reset();
  check(lifetime.expired(), "last view releases parsed owner");
  return failures ? 1 : 0;
}
