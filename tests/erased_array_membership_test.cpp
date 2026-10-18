#include <flight/runtime.hpp>

#include <iostream>
#include <optional>

struct UnrepresentedElement { int value; };

int main() {
  int checks = 0;
  int failures = 0;
  const auto check = [&](bool ok) { ++checks; if (!ok) ++failures; };
  flight::Array<std::optional<double>> original{std::nullopt, 2.0};
  const flight::Any value(original);
  const auto has = [&](const char* key) { return flight::has_property(value, flight::String(key)); };
  check(has("length"));
  check(has("0") && has("1")); // An explicit undefined element is still an own index.
  check(!has("2"));
  for (const auto* key : {"01", "-0", "1.0", "4294967295", "missing"}) check(!has(key));
  for (const auto* key : {"map", "findLast", "toSorted", "with", "toString", "constructor", "hasOwnProperty"}) {
    check(has(key));
  }
  original.push(3.0);
  check(has("2"));
  original.clear();
  check(!has("0") && has("length"));

  const flight::Any json(flight::Json::parse(flight::String("[null,1]")));
  check(json.is_array());
  for (const auto* key : {"map", "length", "0", "1"}) check(flight::has_property(json, flight::String(key)));
  check(!flight::has_property(json, flight::String("2")));
  check(!flight::has_property(json, flight::String("missing")));

  // Presence reads no value, including values which cannot themselves be boxed as Any.
  check(flight::has_property(flight::Any(flight::Array<UnrepresentedElement>{{1}}), flight::String("0")));
  for (const auto& scalar : {flight::Any(flight::null), flight::Any(flight::undefined),
                             flight::Any(2.0), flight::Any(flight::String("str"))}) {
    bool threw = false;
    try { (void)flight::has_property(scalar, flight::String("0")); }
    catch (const flight::TypeError&) { threw = true; }
    check(threw);
  }
  bool typeof_refused = false;
  try { (void)flight::member_typeof(value, flight::String("length")); }
  catch (const flight::OutOfProfileError&) { typeof_refused = true; }
  check(typeof_refused);
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
