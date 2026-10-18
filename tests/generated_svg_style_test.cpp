#include <flight/scene2d_formats/svg_style.hpp>

#include <iostream>
#include <optional>
#include <utility>
#include <vector>

// Source cases from svgStyle.test.ts at Flight a627849, plus scalar and record boundaries.
// Includes the original whole public header. Refused collector/cascade bodies are not supplied here.
namespace {
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL " << message << '\n'; ++failures; }
}
using flight::String;
void test_winding() {
  struct Case { std::optional<String> value; String fallback; String expected; };
  const std::vector<Case> cases{
    {std::nullopt, String("nonZero"), String("nonZero")},
    {String("evenodd"), String("nonZero"), String("evenOdd")},
    {String("nonzero"), String("nonZero"), String("nonZero")},
    {String("bogus"), String("nonZero"), String("nonZero")},
    {String(""), String("nonZero"), String("nonZero")},
    {String("EvenOdd"), String("nonZero"), String("nonZero")},
    {String("evenOdd"), String("nonZero"), String("nonZero")},
    {String("NONZERO"), String("nonZero"), String("nonZero")},
    {std::nullopt, String("evenOdd"), String("evenOdd")},
    {String("evenodd"), String("evenOdd"), String("evenOdd")},
    {String("nonzero"), String("evenOdd"), String("nonZero")},
    {String("bogus"), String("evenOdd"), String("evenOdd")},
    {String(""), String("evenOdd"), String("evenOdd")},
    {String("EvenOdd"), String("evenOdd"), String("evenOdd")},
    {String("evenOdd"), String("evenOdd"), String("evenOdd")},
    {String("NONZERO"), String("evenOdd"), String("evenOdd")},
  };
  for (const auto& item : cases) {
    check(flight::scene2d_formats::resolve_svg_winding(item.value, item.fallback) == item.expected,
          "SVG winding keeps exact spelling and caller fallback");
  }
}

void test_selector_specificity() {
  const std::vector<std::pair<String, double>> cases{
    {String(""), 0.0},
    {String("rect"), 1.0},
    {String("_tag"), 1.0},
    {String("1rect"), 0.0},
    {String("RECT"), 1.0},
    {String("svg:rect"), 1.0},
    {String("#a"), 100.0},
    {String("#a#b"), 200.0},
    {String(".a"), 10.0},
    {String(".a.b"), 20.0},
    {String("rect.a#b"), 111.0},
    {String(".a#b#c"), 210.0},
    {String("rect.x.y#id"), 121.0},
    {String("-tag"), 0.0},
    {String("é"), 0.0},
    {String("😀.a"), 10.0},
    {String("a\nb"), 1.0},
    {String(".under_score"), 10.0},
    {String("a-b#x-2.c_d"), 111.0},
    {String(".#"), 0.0},
    {String("#"), 0.0},
    {String(" .a"), 10.0},
    {String("[name]"), 0.0},
    {String("a:before"), 1.0},
    {String("a > b"), 1.0},
  };
  for (const auto& [selector, expected] : cases) {
    check(flight::scene2d_formats::get_css_selector_specificity(selector) == expected,
          "selector score retains all global id/class counts and type weight");
  }
}

void test_declarations() {
  struct Case { String input; std::vector<std::pair<String, String>> expected; };
  const std::vector<Case> cases{
    {String("fill: red; stroke : blue ;"), {{String("fill"), String("red")}, {String("stroke"), String("blue")}}},
    {String(""), {}},
    {String("no colon; :bad;   ;fill:"), {{String("fill"), String("")}}},
    {String("fill:red;fill:blue;stroke:black"), {{String("fill"), String("blue")}, {String("stroke"), String("black")}}},
    {String("x:url(a:b); y: a:b:c"), {{String("x"), String("url(a:b)")}, {String("y"), String("a:b:c")}}},
    {String("x: red !important  ; y:blue!important; z:green !IMPORTANT"), {{String("x"), String("red")}, {String("y"), String("blue")}, {String("z"), String("green !IMPORTANT")}}},
    {String("2:b;1:a;x: first; x: last;0:z"), {{String("0"), String("z")}, {String("1"), String("a")}, {String("2"), String("b")}, {String("x"), String("last")}}},
    {String("__proto__:ignored;constructor:kept;toString:also-kept"), {{String("constructor"), String("kept")}, {String("toString"), String("also-kept")}}},
    {String("--vendor: x; custom-😀: y"), {{String("--vendor"), String("x")}, {String("custom-😀"), String("y")}}},
    {String("fill:   ;x: 1!important trailing"), {{String("fill"), String("")}, {String("x"), String("1!important trailing")}}},
  };
  for (const auto& item : cases) {
    const auto actual = flight::scene2d_formats::parse_style_declarations(item.input);
    check(actual.size() == item.expected.size(), "declarations retain own-key count");
    std::size_t index = 0;
    for (const auto& [key, value] : actual) {
      check(index < item.expected.size() && key == item.expected[index].first &&
                value == item.expected[index].second,
            "declarations retain exact names/values, overwrites and own-key order");
      ++index;
    }
  }
  const auto first = flight::scene2d_formats::parse_style_declarations(String("fill: red"));
  const auto second = flight::scene2d_formats::parse_style_declarations(String("fill: red"));
  check(first.identity() != second.identity(), "each declaration parse returns fresh storage");
  const auto alias = first;
  alias.set(String("fill"), String("blue"));
  check(first.get(String("fill")) == String("blue") && second.get(String("fill")) == String("red"),
        "result aliases share storage without changing a separate parse");
}

} // namespace

int main() {
  test_winding();
  test_selector_specificity();
  test_declarations();
  if (failures != 0) return 1;
  std::cout << "SVG original-header helpers match 16 winding, 25 selector and 10 declaration source cases\n";
}
