#include <flight/runtime.hpp>

#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int failures = 0;

class TestUnicodeService final : public flight::UnicodeService {
 public:
  [[nodiscard]] std::u16string lower(std::u16string_view value) const override {
    return value == u"\u00C4" ? u"\u00E4" : std::u16string(value);
  }

  [[nodiscard]] std::u16string upper(std::u16string_view value) const override {
    return value == u"\u00E4" ? u"\u00C4" : std::u16string(value);
  }
};

struct TestReference : public flight::ReferenceEnabled {
  int value;
};

struct TestEntity : public flight::ReferenceEnabled {
  std::optional<flight::Ref<TestReference>> entity_runtime_key;
};

struct TestImageFacet final {};
struct TestMenuFacet final {};
struct TestTrayCapabilities {
  bool image;
  bool menu;
};
struct TestImageHost {
  TestTrayCapabilities tray;
};
struct TestOptionalTrayHost {
  std::optional<TestTrayCapabilities> tray;
};

using TestImagePath = flight::MemberPath<
    []<typename Value>(Value& value) -> decltype((value.tray)) { return value.tray; },
    []<typename Value>(Value& value) -> decltype((value.image)) { return value.image; }>;
using TestMenuPath = flight::MemberPath<
    []<typename Value>(Value& value) -> decltype((value.tray)) { return value.tray; },
    []<typename Value>(Value& value) -> decltype((value.menu)) { return value.menu; }>;
using TestImageRule = flight::RequiredMemberFacet<TestImageFacet, TestImagePath>;
using TestMenuRule = flight::RequiredMemberFacet<TestMenuFacet, TestMenuPath>;
using TestImageCapabilities =
    flight::ConditionalFacetRef<TestReference, TestImageHost, TestImageRule, TestMenuRule>;
using TestOptionalCapabilities =
    flight::ConditionalFacetRef<TestReference, TestOptionalTrayHost, TestImageRule>;

static_assert(std::convertible_to<TestImageCapabilities, flight::FacetRef<TestReference, TestImageFacet>>);
static_assert(std::convertible_to<TestImageCapabilities, flight::FacetRef<TestReference, TestMenuFacet>>);
static_assert(!std::convertible_to<TestOptionalCapabilities,
                                   flight::FacetRef<TestReference, TestImageFacet>>);
static_assert(sizeof(flight::FacetRef<TestReference, TestImageFacet>) ==
              sizeof(flight::Ref<TestReference>));

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

FlightTask<int> doubled(FlightTask<int> input) {
  co_return (co_await input) * 2;
}

FlightTask<int> fails() {
  throw std::runtime_error("expected failure");
  co_return 0;
}

FlightTask<int> reuse(FlightTask<int> input) {
  const auto first = co_await input;
  const auto second = co_await input;
  co_return first + second;
}

FlightTask<void> set_flag(bool& flag) {
  flag = true;
  co_return;
}

void test_array() {
  const auto nan = std::numeric_limits<double>::quiet_NaN();
  flight::Array<double> values{nan, -0.0};
  auto alias = values;
  check(alias.push(4.0) == 3, "push returns the new array length");
  check(values.size() == 3, "array copies retain reference identity");
  check(values.length == 3, "array length property follows shared storage");
  check(values.includes(nan), "array includes uses SameValueZero for NaN");
  check(values.index_of(-0.0) == 1, "array index_of finds signed zero");

  auto independent = values.clone();
  independent.push(8.0);
  check(values.size() == 3 && independent.size() == 4, "clone creates independent storage");
  check(!values.at(20).has_value(), "out-of-range array access is absent");

  const auto mapped = values.map([](double value) { return std::isnan(value) ? 0.0 : value + 1.0; });
  check(mapped.size() == 3 && mapped[2] == 5.0, "array map creates transformed storage");
  const auto filtered = mapped.filter([](double value) { return value > 0.0; });
  check(filtered.size() == 2, "array filter preserves matching values");
  check(mapped.every([](double value, double index) { return value >= index; }),
        "array callbacks receive TypeScript numeric indexes");
  check(mapped.find_index([](double value) { return value == 5.0; }) == 2,
        "array find_index returns a numeric position sentinel");
  check(mapped.reduce([](double total, double value) { return total + value; }, 0.0) == 6.0,
        "array reduce preserves accumulator order");
  const auto tail = mapped.slice(-2);
  check(tail.size() == 2 && tail[0] == 1.0, "array slice normalizes negative boundaries");
  check(mapped.join(flight::String("|")) == flight::String("0|1|5"),
        "array join returns the semantic string type");
  check(flight::Array<double>{1.0e-6, 1.0e21}.join(flight::String(",")) ==
            flight::String("0.000001,1e+21"),
        "array join uses source numeric formatting");
  flight::Array<double> spliced{1.0, 4.0};
  const auto removed = spliced.splice(1, 0, 2.0, 3.0);
  check(removed.empty() && spliced.size() == 4 && spliced[1] == 2.0 && spliced[2] == 3.0,
        "array splice inserts forwarded values in source order");
}

void test_binary_data() {
  flight::ArrayBuffer buffer(16.0);
  flight::Uint8Array bytes(buffer);
  auto alias = bytes;
  auto tail = bytes.subarray(8);
  bytes[8] = 42;
  check(alias.buffer == buffer && tail.buffer == buffer && tail.byte_offset == 8 && tail.byte_length == 8 &&
            tail[0] == 42,
        "typed arrays retain shared ArrayBuffer storage and byte offsets");

  const flight::Uint8Array little_endian{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x3F};
  const flight::Uint8Array big_endian{0x3F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  check(flight::DataView(little_endian.buffer).get_float64(0.0, true) == 1.5 &&
            flight::DataView(big_endian.buffer).get_float64(0.0, false) == 1.5,
        "DataView decodes unaligned-independent float64 values in both byte orders");

  const flight::Uint8Array infinity_bytes{0x7F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  const flight::Uint8Array nan_bytes{0x7F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  check(std::isinf(flight::DataView(infinity_bytes.buffer).get_float64(0.0)) &&
            std::isnan(flight::DataView(nan_bytes.buffer).get_float64(0.0)),
        "DataView preserves IEEE infinity and NaN payload classes");

  flight::DataView writable(buffer);
  writable.set_uint32(0.0, 3735928559.0, true);
  writable.set_int16(4.0, -2.0, false);
  writable.set_float32(8.0, 1.25, true);
  check(writable.get_uint32(0.0, true) == 3735928559.0 && writable.get_int16(4.0) == -2.0 &&
            writable.get_float32(8.0, true) == 1.25,
        "DataView numeric setters share storage and preserve integer and float bit patterns");

  bool bounds_failed = false;
  try {
    static_cast<void>(flight::DataView(buffer, 9.0).get_float64(0.0));
  } catch (const std::range_error&) {
    bounds_failed = true;
  }
  check(bounds_failed, "DataView rejects reads beyond its declared view");
}

void test_contract() {
  check(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2",
        "runtime advertises the compiler contract it implements");
  check(flight::runtime_contract.cpp_abi == 1, "runtime C++ ABI is explicit");
  check(flight::runtime_contract.cpp_abi == flight::abi_version && FLIGHT_CPP_ABI_VERSION == 1,
        "runtime contract and public version header agree on the C++ ABI");
  check(flight::runtime_contract.task_contract == "flight-runtime-task-capability-abi/1",
        "task capability ABI is explicit");
  check(flight::runtime_capability_status("array") == flight::RuntimeCapabilityStatus::initial,
        "implemented capabilities are queryable");
  check(flight::runtime_capability_status("array-buffer") == flight::RuntimeCapabilityStatus::initial &&
            flight::runtime_capability_status("data-view") == flight::RuntimeCapabilityStatus::initial,
        "binary storage capabilities are advertised after their implementation lands");
  check(flight::runtime_capability_status("unicode-case-service") == flight::RuntimeCapabilityStatus::planned,
        "planned capabilities remain distinguishable");
  check(flight::runtime_capability_status("unknown") == flight::RuntimeCapabilityStatus::unavailable,
        "unknown capabilities are not treated as planned");
}

void test_error() {
  const flight::Error error(flight::String("expected"));
  check(error.message() == flight::String("expected") && std::string(error.what()) == "expected",
        "error preserves its semantic and native messages");
  check(flight::Error::name() == flight::String("Error"), "error exposes its source-language name");
}

void test_host() {
  const auto previous_executor = flight::current_executor();
  const auto executor = std::make_shared<flight::QueueExecutor>();
  const auto unicode = std::make_shared<TestUnicodeService>();
  {
    const flight::HostScope scope({.executor = executor, .unicode = unicode});
    check(flight::current_executor() == executor, "host scope installs its task executor");
    check(flight::String(u"\u00C4").to_lower() == flight::String(u"\u00E4"),
          "host scope installs its Unicode service");
  }
  check(flight::current_executor() == previous_executor, "host scope restores the previous executor");
}

void test_date() {
  const FlightDate epoch(0.0);
  check(epoch.get_time() == 0.0, "date retains epoch milliseconds");
  check(epoch.get_full_year() == 1970.0, "date exposes its UTC calendar year");
  check(epoch.get_month() == 0.0 && epoch.get_date() == 1.0 && epoch.get_day() == 4.0,
        "date exposes zero-based UTC month and Sunday-based weekday");
  check(epoch.get_hours() == 0.0 && epoch.get_minutes() == 0.0 && epoch.get_seconds() == 0.0 &&
            epoch.get_milliseconds() == 0.0,
        "date exposes UTC time fields");
  check(epoch.to_isostring() == "1970-01-01T00:00:00.000Z", "date formats an ISO UTC instant");
  check(std::isfinite(FlightDate::now()), "date now returns epoch milliseconds");
  check(FlightDate(1.9).get_time() == 1.0, "date applies integer TimeClip semantics");
  check(std::isnan(FlightDate(8.64e15 + 1.0).get_time()), "date clips instants outside the valid range");
}

void test_map() {
  const auto nan = std::numeric_limits<double>::quiet_NaN();
  flight::Map<double, std::string> values{{1.0, "one"}, {nan, "missing"}};
  values.set(nan, "nan").set(-0.0, "zero");
  check(values.size() == 3, "map overwrites SameValueZero keys without changing size");
  check(values.get(nan) == std::optional<std::string>("nan"), "map retrieves NaN keys");
  check(values.has(0.0), "map treats signed zero as the same key");

  auto alias = values;
  check(alias.erase(1.0), "map erase reports an existing key");
  check(!values.has(1.0), "map copies retain reference identity");

  const auto independent = values.clone();
  check(std::isnan(independent.begin()->first), "map iteration retains insertion order after erase");
  alias.clear();
  check(independent.size() == 2 && values.empty(), "map clone creates independent ordered storage");
  std::string visited;
  independent.for_each([&](const std::string& value, double key) {
    static_cast<void>(key);
    visited += value;
  });
  check(visited == "nanzero", "map for_each visits value then key in insertion order");
}

void test_new_runtime_services() {
  static_assert(std::same_as<flight::Ref<flight::Ref<TestReference>>, flight::Ref<TestReference>>);
  static_assert(std::same_as<flight::Ref<flight::String>, flight::String>);
  check(flight::String("frame=") + 12.0 == flight::String("frame=12"),
        "string concatenation converts emitted numeric operands");

  const auto json = flight::Json::stringify(flight::Array<double>{1.0, 2.0}, nullptr, 2.0);
  check(json == flight::String("[\n  1,\n  2\n]"),
        "JSON stringification covers semantic arrays and bounded indentation");

  auto key = flight::make_ref<TestReference>(TestReference{.value = 4});
  std::weak_ptr<TestReference> weak_key = key;
  flight::WeakMap<flight::Ref<TestReference>, flight::String> weak_map;
  weak_map.set(key, "retained value");
  check(weak_map.get(key) == std::optional<flight::String>("retained value") && weak_map.has(key),
        "WeakMap retrieves values by reference identity");
  key.reset();
  check(weak_key.expired(), "WeakMap does not retain its key");

  const auto ignored_arguments = flight::bind_callable_v1<std::function<void(double)>>([] {});
  ignored_arguments(3.0);
  check(flight::callable_signature_v1<std::function<void(double)>>::accepts<double>,
        "callable ABI binds source functions that ignore emitted arguments");

  auto facet_source = flight::make_ref<TestReference>(TestReference{.value = 7});
  auto conditional = flight::assume_conditional_facets<TestImageCapabilities>(facet_source);
  flight::FacetRef<TestReference, TestImageFacet> image_facet = conditional;
  image_facet->value = 8;
  check(facet_source->value == 8 && image_facet.shared_reference() == facet_source &&
            conditional.shared_reference() == facet_source,
        "conditional facets retain exactly one shared base referent");

  auto entity = flight::make_ref<TestEntity>();
  using EntityView = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestEntity>>>>;
  EntityView entity_view(entity);
  auto entity_runtime = flight::make_ref<TestReference>(TestReference{.value = 9});
  const auto entity_runtime_symbol = flight::Symbol::for_key("EntityRuntime");
  flight::row_set(entity_view, entity_runtime_symbol, entity_runtime);
  check(flight::row_get<std::optional<flight::Ref<TestReference>>>(entity_view, entity_runtime_symbol)
                .value()
                ->value == 9,
        "computed structural symbols share the generated Entity runtime slot");

  check(flight::parse_int("  -0x10tail") == -16.0 && flight::parse_int("101", 2.9) == 5.0 &&
            flight::parse_int("10", std::numeric_limits<double>::quiet_NaN()) == 10.0,
        "parse_int applies TypeScript sign, prefix, partial-parse, and radix coercion rules");
  check(flight::to_number(flight::String(" ")) == 0.0 &&
            flight::to_number(flight::String("0b101")) == 5.0 &&
            flight::to_number(flight::String("-1.25e2")) == -125.0 &&
            std::isnan(flight::to_number(flight::String("-0x10"))),
        "to_number handles empty, prefixed, decimal, and invalid signed-prefix inputs");

  flight::Map<flight::String, double> record{{"first", 1.0}, {"second", 2.0}};
  const auto keys = flight::object_keys(record);
  check(keys.size() == 2 && keys[0] == flight::String("first") && keys[1] == flight::String("second"),
        "object_keys preserves emitted key types and deterministic iteration order");

  const auto first_symbol = flight::Symbol::for_key("entity");
  const auto same_symbol = flight::Symbol::for_key("entity");
  check(first_symbol == same_symbol && first_symbol != flight::Symbol::for_key("other") &&
            flight::Symbol() != flight::Symbol(),
        "Symbol.for interns keys while direct symbols retain distinct identity");

  check(flight::Url("HTTP://example.test/path").protocol == flight::String("http:") &&
            flight::Url("child", "https://example.test/base").protocol == flight::String("https:"),
        "Url exposes normalized protocols for absolute and relative inputs");

  const auto decoded = flight::TextDecoder().decode(flight::Uint8Array{0x66, 0xF0, 0x9F, 0x98, 0x80});
  const auto malformed = flight::TextDecoder().decode(flight::Uint8Array{0xE0, 0x80, 0x80});
  const auto after_bom = flight::TextDecoder().decode(flight::Uint8Array{0xEF, 0xBB, 0xBF, 0x78});
  check(decoded == flight::String::from_utf8("f\xF0\x9F\x98\x80") &&
            malformed == flight::String(u"\uFFFD\uFFFD\uFFFD") && after_bom == flight::String("x") &&
            flight::TextDecoder().decode().empty(),
        "TextDecoder follows UTF-8 scalar and malformed-sequence replacement behavior");

  flight::RegExp expression("^flight$", "gi");
  auto expression_alias = expression;
  check(expression.test("FLIGHT") && !expression_alias.test("flight") && expression.test("flight"),
        "global RegExp copies share match position and reset it after failure");
  const auto captures = flight::RegExp("([a-z]+):(\\d+)").exec("id:42");
  check(captures.has_value() && captures->size() == 3 && (*captures)[1] == flight::String("id") &&
            (*captures)[2] == flight::String("42") && captures->index == 0.0,
        "RegExp exec returns the complete match, captures, and source index");
  check(flight::String("a1 b2").replace(flight::RegExp("([a-z])([0-9])", "g"),
                                         flight::String("$2$1")) == flight::String("1a 2b") &&
            flight::String("x=12").replace(
                flight::RegExp("([a-z]+)=([0-9]+)"),
                [](const flight::String&, const flight::String& name, const flight::String& value) {
                  return name.concat(flight::String(":"), value);
                }) == flight::String("x:12"),
        "RegExp global replacement expands captures and invokes replacement callbacks");

  check(flight::IntlCollator().compare("a", "b") < 0.0 &&
            flight::IntlListFormat().format(flight::Array<flight::String>{"a", "b", "c"}) ==
                flight::String("a, b, and c") &&
            flight::IntlNumberFormat().format(1.25) == flight::String("1.25") &&
            flight::IntlPluralRules().select(1.0) == flight::String("one") &&
            flight::IntlRelativeTimeFormat().format(-2.0, "day") == flight::String("2 days ago"),
        "internationalization types provide a deterministic locale-neutral baseline");
}

void test_presence_and_math() {
  flight::Presence<int> value = flight::undefined;
  check(std::holds_alternative<flight::Undefined>(value), "undefined is distinct from a value");
  value = flight::null;
  check(std::holds_alternative<flight::Null>(value), "null is distinct from undefined");
  check(flight::round(3.5) == 4.0 && flight::round(-3.5) == -3.0,
        "Math.round resolves half-integer ties toward positive infinity");
  check(flight::round(-3.5000000000000004) == -4.0 &&
            flight::round(-3.4999999999999996) == -3.0,
        "Math.round distinguishes values immediately around a negative half boundary");
  check(std::signbit(flight::round(-0.5)) && std::signbit(flight::round(-0.1)) &&
            std::signbit(flight::round(-0.0)),
        "Math.round preserves negative zero where JavaScript does");
  check(flight::round(std::numeric_limits<double>::infinity()) ==
            std::numeric_limits<double>::infinity() &&
            flight::round(-std::numeric_limits<double>::infinity()) ==
                -std::numeric_limits<double>::infinity(),
        "Math.round preserves infinities");
  check(std::isnan(flight::round(std::numeric_limits<double>::quiet_NaN())),
        "Math.round preserves NaN");
  check(std::signbit(flight::sign(-0.0)), "Math.sign preserves negative zero");
  check(std::isnan(flight::sign(std::numeric_limits<double>::quiet_NaN())),
        "Math.sign preserves NaN");
  check(flight::power(2.0, 10.0) == 1024.0 && flight::power(4.0, -0.5) == 0.5,
        "exponentiation uses the compiler runtime spelling");
  check(flight::minimum(3.0, 2.0) == 2.0 && flight::maximum(3.0, 2.0) == 3.0 &&
            std::signbit(flight::minimum(0.0, -0.0)) &&
            !std::signbit(flight::maximum(0.0, -0.0)) &&
            std::isnan(flight::minimum(std::numeric_limits<double>::quiet_NaN(), 1.0)),
        "Math minimum and maximum preserve NaN and signed-zero semantics");
  check(flight::minimum(flight::Array<double>{3.0, -2.0, 8.0}) == -2.0 &&
            flight::maximum(flight::Array<double>{3.0, -2.0, 8.0}) == 8.0 &&
            flight::minimum(flight::Array<double>{}) == std::numeric_limits<double>::infinity() &&
            flight::maximum(flight::Array<double>{}) == -std::numeric_limits<double>::infinity(),
        "spread minimum and maximum handle ranges and empty inputs");
  check(flight::is_integer(-0.0) && flight::is_integer(42.0) && !flight::is_integer(0.5) &&
            !flight::is_integer(std::numeric_limits<double>::infinity()) &&
            !flight::is_integer(std::numeric_limits<double>::quiet_NaN()),
        "Number.isInteger recognizes finite integral doubles");
  check(flight::bitwise_and(-1.0, 255.0) == 255.0 &&
            flight::bitwise_or(4294967301.0, 2.0) == 7.0 &&
            flight::bitwise_xor(15.0, 5.0) == 10.0 && flight::bitwise_not(0.0) == -1.0,
        "bitwise helpers apply JavaScript ToInt32 conversion");
  check(flight::left_shift(1073741824.0, 1.0) == -2147483648.0 &&
            flight::left_shift(1.0, 33.0) == 2.0 &&
            flight::signed_right_shift(-2147483648.0, 31.0) == -1.0,
        "signed shifts wrap results and mask their count to five bits");
  check(flight::unsigned_right_shift(-1.0, 0.0) == 4294967295.0 &&
            flight::unsigned_right_shift(-1.0, 1.0) == 2147483647.0 &&
            flight::unsigned_right_shift(std::numeric_limits<double>::quiet_NaN(), 4.0) == 0.0,
        "unsigned right shift returns the JavaScript uint32 number result");
  check(std::abs(flight::pi - std::acos(-1.0)) < 1.0e-15 &&
            std::abs(flight::e - std::exp(1.0)) < 1.0e-15,
        "Math constants are portable and retain double precision");
}

void test_reference() {
  auto value = flight::make_ref<TestReference>(TestReference{.value = 3});
  flight::Ref<flight::ReferenceEnabled> base = value;
  auto alias = value;
  alias->value = 7;
  check(value->value == 7 && base == value, "references preserve shared object identity and support upcasts");

  const auto cell = flight::make_binding_cell(2);
  auto captured = cell;
  captured.rebind(5);
  const auto updated = cell.update_binding([](int& current) {
    current *= 3;
    return current;
  });
  check(updated == 15 && captured.read_binding() == 15,
        "binding-cell copies share mutable closure storage");
}

void test_set() {
  const auto nan = std::numeric_limits<double>::quiet_NaN();
  flight::Set<double> values{nan, nan, -0.0};
  check(values.size() == 2, "set uses SameValueZero and suppresses duplicate insertion");
  auto alias = values;
  alias.add(4.0);
  check(values.has(4.0), "set copies retain reference identity");
  const auto independent = values.clone();
  std::size_t visits = 0;
  independent.for_each([&](double) { ++visits; });
  values.clear();
  check(independent.size() == 3 && values.empty() && visits == 3,
        "set clone and for_each preserve independent ordered storage");
}

void test_string() {
  const auto emoji = flight::String::from_utf8("\xF0\x9F\x98\x80");
  check(emoji.length() == 2, "string length counts UTF-16 code units");
  check(emoji.to_utf8() == "\xF0\x9F\x98\x80", "valid supplementary code points round-trip through UTF-8");
  check(emoji.at(-1).has_value(), "negative string indexing addresses code units from the end");
  check(emoji.slice(0, 1).to_utf8() == "\xEF\xBF\xBD",
        "an isolated surrogate encodes as the Unicode replacement character");

  const flight::String text(u"\u00A0 one,two \uFEFF");
  const auto pieces = text.trim().split(",");
  check(pieces.size() == 2 && pieces[0] == flight::String("one") && pieces[1] == flight::String("two"),
        "trim and split use TypeScript string boundaries");
  check(flight::String("prefix").starts_with("pre") && flight::String("prefix").ends_with("fix"),
        "string prefix and suffix checks accept UTF-8 boundaries");
  check(flight::String("ABC").to_lower() == flight::String("abc"),
        "ASCII case conversion is deterministic without a provider");
  check(flight::String("ab").pad_start(5, "01") == flight::String("010ab"),
        "string pad_start truncates repeated fill at UTF-16 boundaries");
  check(flight::String("ab").repeat(3) == flight::String("ababab"),
        "string repeat uses code-unit-preserving concatenation");
  check(flight::String("flight").substring(4, 1) == flight::String("lig"),
        "string substring clamps and swaps its boundaries");
  check(flight::String::from_char_code(65, 0xD83D, 0xDE00).length() == 3,
        "from_char_code preserves exact UTF-16 code units");
  check(flight::String::from_code_point(65, 0x1F600) == flight::String::from_utf8("A\xF0\x9F\x98\x80"),
        "from_code_point encodes basic and astral Unicode scalars");
  bool invalid_code_point = false;
  try {
    static_cast<void>(flight::String::from_code_point(0xD800));
  } catch (const std::range_error&) {
    invalid_code_point = true;
  }
  check(invalid_code_point, "from_code_point rejects surrogate values");
  check(flight::to_string(true) == flight::String("true") &&
            flight::String::from_number(-0.0) == flight::String("0") &&
            flight::String::from_number(1.0e-6) == flight::String("0.000001") &&
            flight::String::from_number(1.0e21) == flight::String("1e+21"),
        "source string coercion handles booleans and numeric format boundaries");

  bool provider_required = false;
  try {
    static_cast<void>(flight::String(u"\u00C4").to_lower());
  } catch (const flight::UnicodeServiceUnavailable&) {
    provider_required = true;
  }
  check(provider_required, "non-ASCII case conversion refuses a missing Unicode provider");

  const auto service = std::make_shared<TestUnicodeService>();
  {
    const flight::UnicodeServiceScope scope(service);
    check(flight::String(u"\u00C4").to_lower() == flight::String(u"\u00E4"),
          "Unicode case conversion delegates to the configured provider");
  }
}

void test_typed_array() {
  flight::Int16Array values{1, 2, 3};
  auto alias = values;
  check(alias == values && values.subarray(0) != values && values.slice(0) != values,
        "typed-array copies preserve identity while views and slices create objects");
  auto view = values.subarray(1);
  view[0] = 7;
  check(values[1] == 7 && view.size() == 2, "typed-array subarray shares its backing storage");
  auto copy = values.slice(1);
  copy[0] = 9;
  check(values[1] == 7 && copy[0] == 9, "typed-array slice copies its selected range");
  check(values.at(-1) == std::optional<std::int16_t>(3), "typed-array negative access addresses the tail");
  view.set(flight::Int16Array{8, 9});
  check(values[1] == 8 && values[2] == 9, "typed-array set snapshots and writes through a shared view");

  const flight::Uint8ClampedArray clamped{-1.0, 0.5, 1.5, 2.5, 300.0};
  check(static_cast<std::uint8_t>(clamped[0]) == 0 &&
            static_cast<std::uint8_t>(clamped[1]) == 0 &&
            static_cast<std::uint8_t>(clamped[2]) == 2 &&
            static_cast<std::uint8_t>(clamped[3]) == 2 &&
            static_cast<std::uint8_t>(clamped[4]) == 255,
        "Uint8ClampedArray uses saturating ties-to-even conversion");
}

void test_task() {
  const auto source = FlightTask<int>::ready(21);
  check(source.is_ready() && source.get() == 21, "ready task exposes its settled value");
  check(doubled(source).get() == 42, "task composes through co_await");
  check(reuse(source).get() == 42, "settled task supports repeated await");

  const auto joined = FlightTask<int>::join_all(
      std::vector<FlightTask<int>>{FlightTask<int>::ready(2), FlightTask<int>::ready(3)});
  check(joined.get() == std::vector<int>({2, 3}), "join_all preserves task order");

  bool flag = false;
  const auto completion = set_flag(flag);
  completion.get();
  check(flag, "void task completes normally");

  bool rejected = false;
  try {
    FlightTask<int>::reject(std::make_exception_ptr(std::runtime_error("expected failure"))).get();
  } catch (const std::runtime_error& error) {
    rejected = std::string(error.what()) == "expected failure";
  }
  check(rejected, "reject preserves the exact exception");

  bool thrown = false;
  try {
    fails().get();
  } catch (const std::runtime_error& error) {
    thrown = std::string(error.what()) == "expected failure";
  }
  check(thrown, "coroutine exceptions settle the task as rejected");

  const auto value_rejection = FlightTask<int>::reject(std::string("reason"));
  const auto settlement = value_rejection.settle();
  check(settlement.status == flight::TaskStatus::rejected && settlement.rejection->is<std::string>(),
        "task settlement retains a non-exception rejection value");

  const auto recovered = value_rejection.catch_error([](const flight::Rejection& reason) {
    return static_cast<int>(reason.as<std::string>().size());
  });
  check(recovered.get() == 6, "catch_error recovers an exact rejection value");

  bool continuation_called = false;
  const auto continued = source.then([&](int value) {
    continuation_called = true;
    return FlightTask<int>::ready(value + 1);
  });
  check(!continuation_called, "task continuations are queued rather than invoked inline");
  check(continued.get() == 22 && continuation_called, "then assimilates a returned task");

  bool cleanup_called = false;
  const auto finalized = continued.finally([&] {
    cleanup_called = true;
    return FlightTask<void>::ready();
  });
  check(finalized.get() == 22 && cleanup_called, "finally awaits cleanup and preserves fulfillment");

  const auto assimilated = FlightTask<int>::create([](auto resolve, auto reject) {
    static_cast<void>(reject);
    resolve(FlightTask<int>::ready(12));
  });
  check(assimilated.get() == 12, "task resolver assimilates another task");

  const auto resolve_only = FlightTask<int>::create([](auto resolve) { resolve(13); });
  check(resolve_only.get() == 13, "task construction accepts an omitted reject callback");

  check(flight::resolve_task(14).get() == 14 && flight::resolve_task<int>(15).get() == 15,
        "Promise resolve helpers support inferred and explicit result types");
  const auto all = flight::all_tasks(flight::Array{FlightTask<int>::ready(3), FlightTask<int>::ready(4)}).get();
  check(all.size() == 2 && all[0] == 3 && all[1] == 4,
        "Promise all helper bridges semantic arrays and tasks");

  const auto preserved_rejection = value_rejection.finally([] {});
  const auto preserved_settlement = preserved_rejection.settle();
  check(preserved_settlement.status == flight::TaskStatus::rejected &&
            preserved_settlement.rejection->is<std::string>(),
        "finally preserves an existing rejection");

  const auto cleanup_rejection = source.finally([] {
    return FlightTask<void>::reject(std::string("cleanup"));
  });
  const auto cleanup_settlement = cleanup_rejection.settle();
  check(cleanup_settlement.status == flight::TaskStatus::rejected &&
            cleanup_settlement.rejection->as<std::string>() == "cleanup",
        "finally replaces fulfillment with a cleanup rejection");

  const auto executor = std::make_shared<flight::QueueExecutor>();
  bool initializer_called = false;
  const auto created = FlightTask<int>::create(
      [&](auto resolve, auto reject) {
        static_cast<void>(reject);
        initializer_called = true;
        executor->post([resolve] {
          resolve(7);
          resolve(9);
        });
      },
      executor);
  check(initializer_called && created.status() == flight::TaskStatus::pending,
        "task executor runs synchronously while settlement remains queued");
  check(executor->run_until_idle() == 1 && created.get() == 7,
        "first settlement wins for executor-created tasks");
}

} // namespace

int main() {
  test_array();
  test_binary_data();
  test_contract();
  test_date();
  test_error();
  test_host();
  test_map();
  test_new_runtime_services();
  test_presence_and_math();
  test_reference();
  test_set();
  test_string();
  test_task();
  test_typed_array();
  return failures == 0 ? 0 : 1;
}
