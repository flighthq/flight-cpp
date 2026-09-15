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

  observations.push(flight::String("abc").replace(
      flight::RegExp("(a)(b)?(z)?"), flight::String("$$|$&|$`|$'|$1|$2|$3|$12")));
  observations.push(flight::String("-").replace(flight::RegExp("(?:)", "g"), flight::String("_")));
  flight::JsonArray global_matches;
  const auto matched_digits = flight::String("a1b2").match(flight::RegExp("[0-9]", "g"));
  for (const auto& match : *matched_digits) {
    global_matches.push(match);
  }
  observations.push(std::move(global_matches));
  observations.push(flight::String("b").replace(
      flight::RegExp("(a)?b"),
      [](const flight::String&, const std::optional<flight::String>& capture,
         double offset, const flight::String& input) {
        return (capture ? *capture : flight::String("undefined")) + flight::String(":") +
               flight::String::from_number(offset) + flight::String(":") + input;
      }));
  observations.push(flight::String::from_utf8("\xC3\xA9" "0x0").replace(
      flight::RegExp("[0-9]", "g"),
      [](const flight::String&, double offset) {
        return flight::String::from_number(offset);
      }));

  observations.push(flight::Url("child", "https://example.test/base").protocol);
  observations.push(flight::parse_int("  -0x10tail"));
  observations.push(flight::parse_float("  -1.25e2tail"));
  observations.push(flight::parse_float("1e+"));
  observations.push(std::isinf(flight::parse_float("Infinityrest")));
  observations.push(flight::is_safe_integer(9007199254740991.0));
  observations.push(flight::is_safe_integer(9007199254740992.0));
  observations.push(flight::to_number("0b101"));
  observations.push(flight::to_number("-0x10"));
  observations.push(flight::JsonArray{
      flight::number_to_string(255.0, 16.0),
      flight::number_to_string(-10.5, 2.0),
      flight::number_to_string(0.2, 3.0),
      flight::number_to_string(1.2184253536972378e19, 10.0),
  });
  bool rejected_radix = false;
  try {
    static_cast<void>(flight::number_to_string(1.0, 37.0));
  } catch (const std::range_error&) {
    rejected_radix = true;
  }
  observations.push(rejected_radix);
  observations.push(flight::String("ab").pad_end(5, "01"));
  observations.push(flight::fround(1.337));
  observations.push(flight::fround(16777217.0));
  observations.push(std::signbit(flight::fround(-0.0)));
  observations.push(std::isfinite(flight::fround(3.4028235677973362e38)));
  observations.push(std::isfinite(flight::fround(3.4028235677973366e38)));
  observations.push(flight::Uint16Array::bytes_per_element);
  observations.push(flight::Uint32Array::bytes_per_element);

  observations.push(flight::JsonArray{
      flight::to_boolean(flight::String()),
      flight::to_boolean(flight::String("0")),
      flight::to_boolean(0.0),
      flight::to_boolean(-0.0),
      flight::to_boolean(std::numeric_limits<double>::quiet_NaN()),
      flight::to_boolean(std::numeric_limits<double>::infinity()),
      flight::to_boolean(flight::null),
      flight::to_boolean(flight::undefined),
      flight::to_boolean(flight::Array<double>{}),
      flight::to_boolean(flight::Record<flight::String, double>{}),
  });

  const flight::Set<double> iterable{3.0, 1.0, 4.0};
  flight::JsonArray array_from_values;
  for (const auto value : flight::array_from(iterable, [](double value, double index) {
         return value + index;
       })) {
    array_from_values.push(value);
  }
  observations.push(std::move(array_from_values));

  flight::JsonArray uint32_values;
  for (const auto value : flight::Uint32Array::from(
           flight::Array<double>{-1.0, 4294967297.0,
                                 std::numeric_limits<double>::quiet_NaN()})) {
    uint32_values.push(static_cast<double>(value));
  }
  observations.push(std::move(uint32_values));

  flight::JsonArray int8_values;
  for (const auto value : flight::Int8Array::from(
           flight::Array<double>{127.0, 128.0, 255.0, 256.0, -129.0})) {
    int8_values.push(static_cast<double>(value));
  }
  observations.push(std::move(int8_values));

  const auto converted_view = flight::Uint8Array::from(flight::Array<double>{1.0, 2.0});
  observations.push(flight::is_array_buffer_view(converted_view));
  observations.push(flight::is_array_buffer_view(converted_view.buffer));

  const flight::RangeError range_error("outside");
  observations.push(flight::JsonArray{flight::RangeError::name(), range_error.message()});
  const flight::TypeError type_error("wrong type");
  observations.push(flight::JsonArray{flight::TypeError::name(), type_error.message()});

  const auto settlements = flight::all_settled_tasks(flight::Array{
      flight::Task<double>::ready(3.0),
      flight::Task<double>::reject(flight::String("bad"))}).get();
  flight::JsonArray settlement_values;
  settlement_values.push(flight::JsonArray{flight::String("fulfilled"), *settlements[0].value});
  settlement_values.push(flight::JsonArray{
      flight::String("rejected"), settlements[1].rejection->as<flight::String>()});
  observations.push(std::move(settlement_values));

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
  flight::JsonArray record_values;
  for (const auto& value : flight::object_values(record)) record_values.push(value);
  observations.push(std::move(record_values));
  observations.push(!record.get(flight::String("missing")).has_value() &&
                    flight::object_keys(record).size() == 8);

  const flight::Symbol described_symbol("runtime-oracle-record");
  const flight::Symbol second_described_symbol("runtime-oracle-record");
  observations.push(flight::JsonArray{
      described_symbol != second_described_symbol,
      described_symbol != record_symbol,
      described_symbol.key(),
  });

  auto weak_set_key = flight::make_ref<flight::ReferenceEnabled>();
  flight::WeakSet<flight::Ref<flight::ReferenceEnabled>> weak_set;
  auto weak_set_alias = weak_set;
  observations.push(&weak_set.add(weak_set_key) == &weak_set && weak_set_alias.has(weak_set_key));
  observations.push(weak_set_alias.delete_key(weak_set_key) && !weak_set.has(weak_set_key));

  observations.push(flight::Json::stringify(flight::Json::parse(
      R"({"name":"Flight","values":[null,-1.5e2,"\ud83d\ude00"]})")));
  observations.push(flight::IntlListFormat().format(flight::Array<flight::String>{"a", "b", "c"}));
  observations.push(flight::IntlPluralRules().select(1.0));
  observations.push(flight::IntlRelativeTimeFormat().format(-2.0, "day"));
  observations.push(static_cast<double>(flight::IntlCollator().compare("a", "b") < 0.0 ? -1 : 1));

  std::cout << flight::Json::stringify(observations).to_utf8() << '\n';
}
