#include <flight/runtime.hpp>

#include <cmath>
#include <iostream>
#include <limits>

int main() {
  flight::JsonArray observations;

  flight::ArrayBuffer buffer(8.0);
  flight::DataView view(buffer);
  view.set_uint32(1.0, 0x12345678, true);
  observations.push(view.get_uint32(1.0, true));

  observations.push(flight::TextDecoder().decode(flight::Uint8Array{0xE0, 0x80, 0x80}));
  observations.push(flight::String::from_code_point(0x41, 0x1F600));

  flight::Array<double> values{1.0, 4.0};
  const auto removed = values.splice(1, 0, 2.0, 3.0);
  flight::JsonArray spliced;
  for (const auto value : values) spliced.push(value);
  observations.push(std::move(spliced));
  observations.push(static_cast<double>(removed.size()));

  flight::RegExp expression("^flight$", "gi");
  auto expression_alias = expression;
  observations.push(expression.test("FLIGHT"));
  observations.push(expression_alias.test("flight"));
  observations.push(expression.test("flight"));

  observations.push(flight::Url("child", "https://example.test/base").protocol);
  observations.push(flight::parse_int("  -0x10tail"));
  observations.push(flight::to_number("0b101"));
  observations.push(flight::to_number("-0x10"));

  flight::Map<flight::String, double> target{{"first", 0.0}, {"retained", 3.0}};
  const flight::Map<flight::String, double> source{{"first", 1.0}, {"second", 2.0}};
  flight::object_assign(target, source);
  flight::JsonArray assigned;
  for (const auto& [key, value] : flight::object_entries(target)) {
    assigned.push(flight::JsonArray{key, value});
  }
  observations.push(std::move(assigned));

  const auto record_symbol = flight::Symbol::for_key("runtime-oracle-record");
  flight::Record<flight::PropertyKey, double> record;
  record.set(flight::String("later"), 1.0)
      .set(flight::String("10"), 10.0)
      .set(2.0, 2.0)
      .set(flight::String("01"), 1.0)
      .set(-0.0, 0.0)
      .set(4294967294.0, 4.0)
      .set(4294967295.0, 5.0)
      .set(record_symbol, 7.0)
      .set(flight::String("after"), 8.0)
      .set(flight::String("2"), 22.0);
  flight::JsonArray record_keys;
  for (const auto& key : flight::object_keys(record)) record_keys.push(key);
  observations.push(std::move(record_keys));
  flight::JsonArray record_entries;
  for (const auto& [key, value] : flight::object_entries(record)) {
    record_entries.push(flight::JsonArray{key, value});
  }
  observations.push(std::move(record_entries));
  observations.push(!record.get(flight::String("missing")).has_value() &&
                    flight::object_keys(record).size() == 8);

  observations.push(flight::Json::stringify(flight::Json::parse(
      R"({"name":"Flight","values":[null,-1.5e2,"\ud83d\ude00"]})")));
  observations.push(flight::IntlListFormat().format(flight::Array<flight::String>{"a", "b", "c"}));
  observations.push(flight::IntlPluralRules().select(1.0));
  observations.push(flight::IntlRelativeTimeFormat().format(-2.0, "day"));
  observations.push(static_cast<double>(flight::IntlCollator().compare("a", "b") < 0.0 ? -1 : 1));

  std::cout << flight::Json::stringify(observations).to_utf8() << '\n';
}
