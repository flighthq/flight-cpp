// Expectations from untouched Flight a62784923f3be814463286c9fb28edfe6ab15789 collectCssRules.
#include <flight/scene2d_formats/svg_style.hpp>
#include <iostream>
#include <vector>

namespace {
using flight::String;
namespace types = flight::types;
namespace svg = flight::scene2d_formats;
int failures = 0;
int cases = 0;
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "case " << cases << ": " << message << '\n'; ++failures; }
}
String units(std::initializer_list<char16_t> value) { return String(std::u16string(value)); }
auto element(String name, String text = {}) {
  auto result = flight::make_ref<types::XmlElement>();
  result->name = name; result->text = text; return result;
}
auto collect(flight::Ref<types::XmlElement> root) {
  return svg::collect_css_rules(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::XmlElement>>>>(root));
}
struct Expected { String selector; double specificity; flight::Record<String,String> declarations; };
void expect(String text, std::initializer_list<Expected> expected) {
  ++cases;
  const auto root = element("style", text);
  const auto actual = collect(root);
  check(actual.size() == expected.size(), "rule count");
  std::size_t index = 0;
  for (const auto& want : expected) {
    if (index >= actual.size()) break;
    const auto got = actual[index];
    check(got->selector == want.selector && got->specificity == want.specificity &&
          got->order == static_cast<double>(index), "selector/specificity/order");
    const auto keys = flight::object_keys(got->declarations);
    const auto wanted_keys = flight::object_keys(want.declarations);
    check(keys.size() == wanted_keys.size(), "declaration count");
    for (std::size_t key = 0; key < wanted_keys.size(); ++key) {
      check(key < keys.size() && keys[key] == wanted_keys[key], "declaration own-key order");
      check(got->declarations.get(wanted_keys[key]) == want.declarations.get(wanted_keys[key]), "declaration value");
    }
    ++index;
  }
  check(root->text == text && root->children.empty(), "original source owner untouched");
}
}
int main() {
  expect(String(""), {});
  expect(String("rect{}"), {{String("rect"),1,flight::Record<String,String>{}}});
  expect(String("rect{fill:red}"), {{String("rect"),1,flight::Record<String,String>{{String("fill"),String("red")}}}});
  expect(String("rect,.a,#b{fill:red;stroke:blue}"), {{String("rect"),1,flight::Record<String,String>{{String("fill"),String("red")},{String("stroke"),String("blue")}}},{String(".a"),10,flight::Record<String,String>{{String("fill"),String("red")},{String("stroke"),String("blue")}}},{String("#b"),100,flight::Record<String,String>{{String("fill"),String("red")},{String("stroke"),String("blue")}}}});
  expect(String("rect{} .a{stroke:blue} #b{fill:green}"), {{String("rect"),1,flight::Record<String,String>{}},{String(".a"),10,flight::Record<String,String>{{String("stroke"),String("blue")}}},{String("#b"),100,flight::Record<String,String>{{String("fill"),String("green")}}}});
  expect(String("/* skip .bad{fill:no} */ rect{fill:red}"), {{String("rect"),1,flight::Record<String,String>{{String("fill"),String("red")}}}});
  expect(String("rect{fill:red;fill:blue;; invalid ; :lost;stroke: green !important}"), {{String("rect"),1,flight::Record<String,String>{{String("fill"),String("blue")},{String("stroke"),String("green")}}}});
  expect(String("rect > path{fill:no}.a:hover{fill:no}.b[attr]{fill:no}.c + .d{fill:no}.e~.f{fill:no}"), {});
  expect(String(" , ,rect,,.a, {x:1}"), {{String("rect"),1,flight::Record<String,String>{{String("x"),String("1")}}},{String(".a"),10,flight::Record<String,String>{{String("x"),String("1")}}}});
  expect(String("RECT { FILL : RED ; zero: ; x:y:z }"), {{String("RECT"),1,flight::Record<String,String>{{String("FILL"),String("RED")},{String("zero"),String("")},{String("x"),String("y:z")}}}});
  expect(String("rect{color:a/*mid*/b} /* multiline\r\n */ #x{color:c}"), {{String("rect"),1,flight::Record<String,String>{{String("color"),String("ab")}}},{String("#x"),100,flight::Record<String,String>{{String("color"),String("c")}}}});
  expect(String("{x:y}rect{{x:y}}tail{z:w}"), {{String("tail"),1,flight::Record<String,String>{{String("z"),String("w")}}}});
  expect(String("unterminated{color:red"), {});
  expect(String("rect{color:red} trailing"), {{String("rect"),1,flight::Record<String,String>{{String("color"),String("red")}}}});
  expect(String("rect{}rect{}"), {{String("rect"),1,flight::Record<String,String>{}},{String("rect"),1,flight::Record<String,String>{}}});
  expect(String("#a.b#c.d{fill:blue}"), {{String("#a.b#c.d"),220,flight::Record<String,String>{{String("fill"),String("blue")}}}});
  expect(String("*{fill:red}.a.b{fill:blue}"), {{String("*"),0,flight::Record<String,String>{{String("fill"),String("red")}}},{String(".a.b"),20,flight::Record<String,String>{{String("fill"),String("blue")}}}});
  expect(String("svg:rect{fill:red} .ok{fill:blue}"), {{String(".ok"),10,flight::Record<String,String>{{String("fill"),String("blue")}}}});
  expect(String("rect{a:!important;b:x!important;c:y !IMPORTANT}"), {{String("rect"),1,flight::Record<String,String>{{String("a"),String("")},{String("b"),String("x")},{String("c"),String("y !IMPORTANT")}}}});
  expect(String("rect{1:a;0:b;2:c;__proto__:x;constructor:y;toString:z}"), {{String("rect"),1,flight::Record<String,String>{{String("0"),String("b")},{String("1"),String("a")},{String("2"),String("c")},{String("constructor"),String("y")},{String("toString"),String("z")}}}});
  expect(String(" rect {font-family:é😀} .a .b{x:no}"), {{String("rect"),1,flight::Record<String,String>{{String("font-family"),String("é😀")}}}});
  expect(String(" rect {a:one !important}"), {{String("rect"),1,flight::Record<String,String>{{String("a"),String("one")}}}});
  expect(units({0x72,0x65,0x63,0x74,0x7b,0x63,0x6f,0x6c,0x6f,0x72,0x3a,0xd800,0x7d}), {{String("rect"),1,flight::Record<String,String>{{String("color"),units({0xd800})}}}});
  expect(units({0xd800,0x7b,0x63,0x6f,0x6c,0x6f,0x72,0x3a,0xdc00,0x7d}), {{units({0xd800}),0,flight::Record<String,String>{{String("color"),units({0xdc00})}}}});
  expect(units({0xd83d,0xde00,0x7b,0x66,0x6f,0x6e,0x74,0x2d,0x66,0x61,0x6d,0x69,0x6c,0x79,0x3a,0xe9,0xd800,0xd83d,0xde00,0xdc00,0x7d}), {{String("😀"),0,flight::Record<String,String>{{String("font-family"),units({0xe9,0xd800,0xd83d,0xde00,0xdc00})}}}});
  expect(units({0x72,0x65,0x63,0x74,0x7b,0x61,0x3a,0x0,0x3b,0x62,0x3a,0xd800,0x7d}), {{String("rect"),1,flight::Record<String,String>{{String("a"),units({0x0})},{String("b"),units({0xd800})}}}});
  expect(units({0x2f,0x2a,0x20,0xd800,0xd83d,0xde00,0xdc00,0x20,0x2a,0x2f,0x72,0x65,0x63,0x74,0x7b,0x61,0x3a,0x67,0x6f,0x6f,0x64,0x7d}), {{String("rect"),1,flight::Record<String,String>{{String("a"),String("good")}}}});
  expect(String("[bad]{}:bad{}good{}"), {{String("good"),1,flight::Record<String,String>{}}});
  expect(String("rect{a:x !important !important}"), {{String("rect"),1,flight::Record<String,String>{{String("a"),String("x !important")}}}});
  expect(String(".a{padding:1; color :  blue }"), {{String(".a"),10,flight::Record<String,String>{{String("padding"),String("1")},{String("color"),String("blue")}}}});
  expect(String("a\\b{x:y}"), {{String("a\\b"),1,flight::Record<String,String>{{String("x"),String("y")}}}});
  expect(String("a\n b{x:y}"), {});

  auto root = element("svg");
  auto first = element("svg:style", "rect,.a{fill:red}.b{fill:blue}");
  auto nested = element("g"); nested->children.push(element("style", "#id{color:green}"));
  root->children = {element("STYLE", ".ignored{x:y}"), first, nested};
  auto rules = collect(root);
  check(rules.size() == 4 && rules[0]->selector == String("rect") && rules[3]->selector == String("#id") &&
        rules[3]->order == 3, "namespaced style and depth-first traversal order");
  check(rules[0] != rules[1] && rules[0]->declarations.identity() == rules[1]->declarations.identity() &&
        rules[1]->declarations.identity() != rules[2]->declarations.identity(), "authored rule and shared declaration owners");
  rules[0]->declarations.set("fill", "changed");
  check(rules[1]->declarations.get(String("fill")) == String("changed") &&
        rules[2]->declarations.get(String("fill")) == String("blue"), "live shared declarations within one matched block");
  auto again = collect(root);
  check(again.identity() != rules.identity() && again[0] != rules[0] &&
        again[0]->declarations.identity() != rules[0]->declarations.identity() &&
        again[0]->declarations.get(String("fill")) == String("red"), "fresh call owners and unchanged input");
  first->text = "path{}";
  auto changed = collect(root);
  check(changed.size() == 2 && changed[0]->selector == String("path") && changed[1]->order == 1,
        "later collection sees live original tree mutation");
  std::cout << cases << " source collector cases and owner checks\n";
  return failures == 0 ? 0 : 1;
}
