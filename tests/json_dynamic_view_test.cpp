#include <flight/runtime.hpp>

#include <iostream>
#include <limits>
#include <memory>
#include <vector>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
}

int main() {
  using flight::Any;
  using flight::JsonValue;
  using flight::String;
  auto doc = flight::Json::parse(R"({"bones":[null,2,{"x":1}],"obj":{"n":1},"null":null,"false":false,"zero":0,"str":"s","2":2,"1":1,"01":3,"4294967295":4})");
  auto alias = doc;
  auto view = flight::named_properties(doc);
  check(view.shared_owner()->native_object().get() == doc.object_owner().get(), "view retains parsed root identity");
  check(view.shared_owner() == flight::named_properties(alias).shared_owner(), "root aliases share their canonical row owner");
  const auto keys = view.keys();
  check(keys[0] == String("1") && keys[1] == String("2") && keys[2] == String("bones") &&
        keys[keys.size() - 2] == String("01") && keys.back() == String("4294967295"),
        "integer keys precede insertion-ordered non-index names");
  check(!view.has("missing") && view.get("missing").is_undefined(), "missing key reads undefined");
  check(view.has("null") && view.get("null").is_null(), "present null remains null");
  check(!view.get("false").as_boolean() && view.get("zero").as_number() == 0 &&
        view.get("str").as_string() == String("s"), "falsy and string primitives retain their kinds");
  const auto object = view.get("obj");
  check(object.object_if<flight::JsonObject>() == doc.as_object().get("obj")->object_owner(),
        "exact erased object recovery returns the original parsed owner");
  check(object.strict_equals(view.get("obj")) && object.identity() == doc.as_object().get("obj")->object_owner().get(),
        "repeated nested object reads retain parsed identity");
  check(!object.strict_equals(Any(flight::Json::parse("{\"n\":1}"))), "equal-looking parses have distinct object identity");
  auto object_view = flight::named_properties(object);
  alias.as_object().get("obj")->as_object().set("n", 8).set("added", 9);
  check(object_view.get("n").as_number() == 8 && object_view.has("added"), "nested view reads live owner changes");
  check(!object_view.set("n", Any(3)) && object_view.get("n").as_number() == 8, "JSON read view never materializes mutable cells");

  const auto array = view.get("bones");
  check(array.is_array() && array.type_of() == String("object") && array.array_size() == 3,
        "JSON array keeps array and typeof identity");
  check(array.identity() == doc.as_object().get("bones")->array_owner().get() && array.strict_equals(view.get("bones")),
        "array reads retain their original storage identity");
  check(array.array_get(0)->is_null() && array.array_get(1)->as_number() == 2,
        "erased element reads retain null and primitive values");
  check(array.array_get(2)->identity() == doc.as_object().get("bones")->as_array()[2].object_owner().get(),
        "erased array object element keeps identity");
  check(flight::Json::stringify(array) == flight::Json::stringify(doc.as_object().get("bones")->as_array()) &&
        flight::Json::stringify(object) == flight::Json::stringify(doc.as_object().get("obj")->as_object()),
        "boxed JSON containers serialize their retained contents");
  for (const auto index : {-1.0, 0.5, 3.0, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN()}) {
    check(!array.array_get(index), "invalid or missing array index stays absent");
  }
  auto array_view = flight::named_properties(array);
  check(array_view.keys() == std::vector<String>{"0", "1", "2"} && array_view.has("length") &&
        array_view.get("length").as_number() == 3 && !array_view.has("01"),
        "array own indices enumerate while length is nonenumerable");
  alias.as_object().get("bones")->as_array().push(4);
  check(array.array_size() == 4 && array_view.keys().size() == 4 && array_view.get("3").as_number() == 4,
        "retained array and named views observe growth without stale cells");
  check(Any(JsonValue(flight::null)).is_null() && Any(JsonValue(false)).kind() == flight::AnyKind::boolean &&
        Any(JsonValue(0)).kind() == flight::AnyKind::number && Any(JsonValue("s")).kind() == flight::AnyKind::string,
        "boxing covers the complete JSON primitive domain without coercion");

  flight::NamedProperties retained;
  std::weak_ptr<flight::JsonObject> lifetime;
  {
    auto temporary = flight::Json::parse("{\"retained\":7}");
    lifetime = temporary.object_owner();
    retained = flight::named_properties(temporary);
  }
  check(!lifetime.expired() && retained.get("retained").as_number() == 7, "view owns parsed container lifetime");
  retained = {};
  check(lifetime.expired(), "canonical row metadata does not keep the parsed owner alive");

  flight::Record<String, Any> record;
  record.set("undefined", flight::undefined).set("null", flight::null).set("2", 2).set("1", 1);
  flight::NamedProperties record_view = record;
  check(record_view.shared_owner()->native_object().get() == record.identity(), "readonly Record bridge keeps native storage identity");
  check(record_view.has("undefined") && record_view.get("undefined").is_undefined() &&
        !record_view.has("missing") && record_view.get("missing").is_undefined(),
        "readonly Record separates missing keys from present undefined");
  record.set("later", 5); record.erase(String("null"));
  check(record_view.get("later").as_number() == 5 && !record_view.has("null") &&
        record_view.keys() == std::vector<String>{"1", "2", "undefined", "later"},
        "readonly Record bridge observes insert, erase, and integer ordering");
  return failures == 0 ? 0 : 1;
}
