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
  for (const auto value : {Any(), Any(flight::null), Any(false), Any(0), Any(-0.0),
                           Any(std::numeric_limits<double>::quiet_NaN()), Any(String())}) {
    check(!flight::to_boolean(value) && !flight::to_boolean(std::optional<Any>(value)),
          "shared truthiness preserves erased falsy primitive values");
  }
  for (const auto value : {Any(true), Any(1), Any(String("s")), Any(flight::Json::parse("{}")),
                           Any(flight::Json::parse("[]"))}) {
    check(flight::to_boolean(value), "shared truthiness observes erased truthy primitives and empty objects");
  }
  auto doc = flight::Json::parse(R"({"bones":[null,2,{"x":1}],"obj":{"n":1},"null":null,"false":false,"zero":0,"str":"s","2":2,"1":1,"01":3,"4294967295":4})");
  auto alias = doc;
  check(!flight::is_array(doc), "parsed objects are not arrays");
  for (const auto* text : {"null", "true", "0", "\"s\"", "{}"}) {
    check(!flight::is_array(flight::Json::parse(text)), "JSON non-arrays keep the array predicate false");
  }
  const auto parsed_array = flight::Json::parse("[null,1]");
  check(flight::is_array(parsed_array) && flight::is_array(parsed_array.as_array()) &&
        flight::is_array(std::optional<JsonValue>(parsed_array)) &&
        !flight::is_array(std::optional<JsonValue>()) &&
        flight::is_array(Any(parsed_array)), "array predicate agrees across parsed, optional and erased storage");
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
  check(flight::read_json_data_property(object, "n").as_number() == 8 &&
        flight::read_json_data_property(object, "added").as_number() == 9 &&
        flight::read_json_data_property(object, "absent").is_undefined(),
        "JSON data reads retain the live owner and missing-key value");
  for (const auto nullish : {Any(flight::null), Any(flight::undefined)}) {
    bool type_error = false;
    try { static_cast<void>(flight::read_json_data_property(nullish, "x")); }
    catch (const flight::TypeError&) { type_error = true; }
    check(type_error, "direct nullish JSON data reads throw TypeError");
  }
  for (const auto primitive : {Any(0), Any(false), Any(String("s")), Any(String())}) {
    check(flight::read_json_data_property(primitive, "value").is_undefined(),
          "primitive JSON data fields read undefined without fictitious object recovery");
  }
  const Any string_units(String::from_char_code(0xD800, u'x', 0xDC00));
  check(flight::read_json_data_property(string_units, "length").as_number() == 3 &&
        flight::read_json_data_property(string_units, "0").as_string() == String::from_char_code(0xD800) &&
        flight::read_json_data_property(string_units, "2").as_string() == String::from_char_code(0xDC00),
        "boxed JSON strings expose exact UTF16 own indices and length");
  for (const auto* key : {"", "3", "01", "-0", "-1", "1.0", "999999999999999999999999999"}) {
    check(flight::read_json_data_property(string_units, key).is_undefined(),
          "noncanonical and out-of-range boxed string indices are absent");
  }
  check(flight::read_json_data_property(Any(String()), "0").is_undefined(),
        "empty string has no own index zero");

  const auto array = view.get("bones");
  check(flight::read_json_data_property(array, "0").is_null() &&
        flight::read_json_data_property(array, "length").as_number() == 3,
        "JSON data array reads use the original array storage");
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
  check(array_view.get(-0.0).is_null() && array_view.has(1.0) &&
        array_view.get(std::variant<String, double>(1.0)).as_number() == 2 &&
        array_view.get(std::variant<String, double>(String("1"))).as_number() == 2,
        "numeric and mixed string/number keys retain JavaScript property spelling");
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

  // These are valid JSON inputs to the original counter, even when a number is not finite
  // or a key cannot be represented as a Unicode scalar value in UTF-8.
  const auto high = String::from_char_code(0xD800);
  const auto other_high = String::from_char_code(0xD801);
  const auto low = String::from_char_code(0xDC00);
  const auto replacement = String::from_char_code(0xFFFD);
  auto unicode = flight::Json::parse(
      R"({"\ud800":1,"\ud801":2,"\udc00":3,"\ufffd":4,"\ud83d\ude00":5,"\u0000":6})");
  const auto unicode_view = flight::named_properties(unicode);
  check(unicode_view.keys() == std::vector<String>{high, other_high, low, replacement,
        String::from_char_code(0xD83D, 0xDE00), String::from_char_code(0)},
        "JSON enumeration retains distinct surrogate and embedded-NUL keys in insertion order");
  check(unicode_view.get(high).as_number() == 1 && unicode_view.get(other_high).as_number() == 2 &&
        unicode_view.get(low).as_number() == 3 && unicode_view.get(replacement).as_number() == 4,
        "exact UTF16 property reads never alias lone surrogates to the replacement character");
  unicode.as_object().set(high, 7);
  check(unicode_view.get(high).as_number() == 7 && !unicode_view.set(high, 8),
        "exact UTF16 reads remain live and readonly");
  const String literal_surrogate(std::u16string{u'"', char16_t(0xD800), u'"'});
  check(flight::Json::parse(literal_surrogate).as_string() == high &&
        flight::Json::parse(R"("\ud800x\udc00")").as_string() ==
            String(std::u16string{char16_t(0xD800), u'x', char16_t(0xDC00)}),
        "escaped and literal UTF16 units retain the full JavaScript string domain");
  flight::Record<String, Any> unicode_record;
  unicode_record.set(high, 1).set(replacement, 2);
  flight::NamedProperties unicode_record_view = unicode_record;
  check(unicode_record_view.keys() == std::vector<String>{high, replacement} &&
        unicode_record_view.get(high).as_number() == 1 && unicode_record_view.get(replacement).as_number() == 2,
        "native Record bridging retains exact dynamic UTF16 keys");
  auto declared = std::make_shared<flight::RowOwner>();
  declared->set_named_value(replacement.to_utf8(), Any(3));
  const flight::NamedProperties declared_view(declared);
  check(!declared_view.has(high) && declared_view.get(high).is_undefined() && !declared_view.set(high, 4) &&
        declared_view.get(replacement).as_number() == 3,
        "unrepresentable native names cannot read or write a different declared property");

  const auto infinity = std::numeric_limits<double>::infinity();
  for (const auto& [text, expected] : std::vector<std::pair<const char*, double>>{
      {"1e400", infinity}, {"1.8e308", infinity}, {"-1e400", -infinity},
      {"1e-400", 0.0}, {"-1e-400", -0.0}, {"-0e9999999999999999999999", -0.0},
      {"1e9999999999999999999999", infinity}, {"1e-9999999999999999999999", 0.0},
      {"5e-324", std::numeric_limits<double>::denorm_min()}}) {
    const auto actual = flight::Json::parse(text).as_number();
    check(actual == expected && std::signbit(actual) == std::signbit(expected),
          "valid JSON decimals retain overflow, underflow, rounding and signed-zero semantics");
  }
  for (const auto* text : {"01", "1.", "1e", "-", "+1", "NaN", "Infinity", R"("\uD80")", R"("\x00")"}) {
    bool syntax_error = false;
    try { static_cast<void>(flight::Json::parse(text)); }
    catch (const flight::JsonSyntaxError&) { syntax_error = true; }
    check(syntax_error, "malformed JSON still throws the syntax-only error");
  }
  return failures == 0 ? 0 : 1;
}
