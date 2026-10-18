// Exercise the original normally emitted public counter. Expected ordered entries were
// checked against the untouched pinned TypeScript body and its actual registry constants.
#include <flight/skeleton2d_formats/spine_json_section_counts.hpp>

#include <iostream>
#include <optional>
#include <utility>
#include <vector>

namespace {
using flight::String;
using Entries = std::vector<std::pair<String, double>>;
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
}

int main() {
  namespace formats = flight::skeleton2d_formats;
  struct Case { const char* json; std::optional<Entries> expected; };
  const std::vector<Case> cases{
    {R"json()json", std::nullopt},
    {R"json({)json", std::nullopt},
    {R"json([1,])json", std::nullopt},
    {R"json({}x)json", std::nullopt},
    {R"json(null)json", std::nullopt},
    {R"json(false)json", std::nullopt},
    {R"json(0)json", std::nullopt},
    {R"json("value")json", std::nullopt},
    {R"json([])json", std::nullopt},
    {R"json({})json", std::nullopt},
    {R"json({"bones":[]})json", Entries{}},
    {R"json({"skeleton":{}})json", Entries{}},
    {R"json({"skeleton":[]})json", Entries{}},
    {R"json({"bones":[null,1,{}],"slots":{"2":{},"1":null},"skins":{"default":{}},"events":[1],"ik":{"a":1},"path":[1,2],"transform":{"a":1,"b":2}})json", Entries{{String("bones"), 3.0}, {String("slots"), 2.0}, {String("skins"), 1.0}, {String("events"), 1.0}, {String("ikConstraints"), 1.0}, {String("pathConstraints"), 2.0}, {String("transformConstraints"), 2.0}}},
    {R"json({"bones":[],"armature":[]})json", std::nullopt},
    {R"json({"bones":[],"armature":{}})json", Entries{}},
    {R"json({"skeleton":null})json", std::nullopt},
    {R"json({"bones":[],"animations":{"a":{"drawOrder":null,"draworder":[1,2]}}})json", Entries{{String("animations"), 1.0}, {String("drawOrder"), 2.0}}},
    {R"json({"bones":[],"animations":{"a":{"draworder":[1,2]}}})json", Entries{{String("animations"), 1.0}, {String("drawOrder"), 2.0}}},
    {R"json({"bones":[],"animations":{"a":{"drawOrder":false,"draworder":[1,2]}}})json", Entries{{String("animations"), 1.0}}},
    {R"json({"bones":[],"animations":{"a":{"drawOrder":0,"draworder":[1,2]}}})json", Entries{{String("animations"), 1.0}}},
    {R"json({"bones":[],"animations":{"a":{"drawOrder":[],"draworder":[1,2]}}})json", Entries{{String("animations"), 1.0}}},
    {R"json({"bones":[],"animations":[null,{},3,{"bones":[1,2],"slots":{"z":0},"draworder":[1]}]})json", Entries{{String("animations"), 4.0}, {String("bone"), 2.0}, {String("slot"), 1.0}, {String("drawOrder"), 1.0}}},
    {R"json({"bones":[],"events":1e400})json", Entries{}},
    {R"json({"bones":[],"events":1e-400})json", Entries{}},
    {R"json({"bones":[],"slots":{"\ud800":1,"\ud801":2}})json", Entries{{String("slots"), 2.0}}},
    {R"json({"bones":[],"slots":{"\ud800":1,"�":2}})json", Entries{{String("slots"), 2.0}}},
    {R"json({"bones":[],"animations":{"\ud800":{"draworder":[1]},"\ud801":{"draworder":[1,2]}}})json", Entries{{String("animations"), 2.0}, {String("drawOrder"), 3.0}}},
  };
  for (std::size_t index = 0; index < cases.size(); ++index) {
    const auto& test = cases[index];
    const auto actual = formats::collect_spine_json_section_counts(String(test.json));
    bool matches = actual.has_value() == test.expected.has_value();
    if (actual && test.expected) matches = Entries(actual->begin(), actual->end()) == *test.expected;
    if (!matches) { ++failures; std::cerr << "FAIL: original source boundary case " << index << '\n'; }
  }
  auto first = formats::collect_spine_json_section_counts(R"({"bones":[]})");
  auto second = formats::collect_spine_json_section_counts(R"({"bones":[]})");
  check(first.has_value() && second.has_value(), "an empty valid document returns a present empty Map");
  if (first && second) {
    first->set("mutation", 1);
    check(!second->has("mutation"), "each valid invocation returns its own fresh counts owner");
  }
  std::u16string literal = u"{\"bones\":[],\"slots\":{\"";
  literal.push_back(char16_t(0xD800));
  literal += u"\":1,\"";
  literal.push_back(char16_t(0xD801));
  literal += u"\":2,\"";
  literal.push_back(char16_t(0xFFFD));
  literal += u"\":3}}";
  const auto unicode = formats::collect_spine_json_section_counts(String(std::move(literal)));
  check(unicode && unicode->get("slots").value_or(-1.0) == 3,
        "distinct literal surrogate keys and U+FFFD retain the source String domain");
  return failures == 0 ? 0 : 1;
}
