// Pinned colladaXml.test.ts (Flight a627849): all fourteen cases, plus numeric/owner edges.
// Include the original public module path; no copied helper or replacement parser is used.
#include <flight/scene3d_formats/collada_xml.hpp>
#include <flight/xml/xml_parse.hpp>

#include <cmath>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace collada = flight::scene3d_formats;
using flight::String;
using Element = flight::Ref<flight::types::XmlElement>;

void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
Element parse(const char* text) {
  return flight::xml::parse_xml_document(String(text)).value();
}
void numbers_equal(const flight::Array<double>& actual, std::initializer_list<double> expected) {
  require(actual.size() == expected.size(), "wrong numeric array length");
  std::size_t index = 0;
  for (double value : expected) require(actual[index++] == value, "wrong parsed number/order");
}
}

int main() {
  int failures = 0;
  int cases = 0;
  const auto run = [&](const char* label, const auto& test) {
    ++cases;
    try { test(); }
    catch (const std::exception& error) {
      ++failures;
      std::cerr << "FAIL " << label << ": " << error.what() << '\n';
    }
  };
  const auto namespaced = parse(
      "<c:COLLADA xmlns:c=\"http://www.collada.org/2005/11/COLLADASchema\">"
      "<c:asset><c:up_axis>  Z_UP  </c:up_axis></c:asset>"
      "<c:library_geometries><c:geometry id=\"g1\"><c:mesh/></c:geometry>"
      "<c:geometry id=\"g2\"/></c:library_geometries></c:COLLADA>");
  const auto plain = parse("<COLLADA><asset><up_axis>Y_UP</up_axis></asset><node id=\"n\"/><node/></COLLADA>");

  run("child: plain and namespace", [&] {
    require(collada::collada_child(plain, String("asset")).has_value(), "plain child missing");
    require(collada::collada_child(namespaced, String("asset")).has_value(), "namespace child missing");
  });
  run("child: missing/no element", [&] {
    require(!collada::collada_child(plain, String("library_geometries")), "missing child became present");
    require(!collada::collada_child(std::nullopt, String("asset")), "absent element became present");
  });
  run("child: suffix boundary", [&] {
    require(!collada::collada_child(parse("<COLLADA><subasset/></COLLADA>"), String("asset")), "partial suffix matched");
  });
  run("children: document order and empty", [&] {
    const auto nodes = collada::collada_children(plain, String("node"));
    require(nodes.size() == 2 && nodes[0] == plain->children[1] && nodes[1] == plain->children[2], "child order/identity changed");
    require(collada::collada_children(collada::collada_child(namespaced, String("library_geometries")), String("geometry")).size() == 2,
            "namespace children missing");
    require(collada::collada_children(plain, String("geometry")).empty() && collada::collada_children(std::nullopt, String("node")).empty(),
            "empty child list changed");
  });
  run("descendants: depth and direct-child distinction", [&] {
    require(collada::collada_descendants(namespaced, String("geometry")).size() == 2, "descendant geometries missing");
    require(collada::collada_children(namespaced, String("geometry")).empty(), "child lookup recursed");
    require(collada::collada_descendants(namespaced, String("mesh")).size() == 1, "deep mesh missing");
  });
  run("id: present and missing", [&] {
    require(collada::collada_id_of(collada::collada_descendants(namespaced, String("geometry"))[0]) == std::optional<String>{String("g1")},
            "present id changed");
    require(!collada::collada_id_of(collada::collada_child(plain, String("asset")).value()), "missing id must be null");
  });
  run("local name: namespace and plain", [&] {
    require(collada::collada_local_name(namespaced) == String("COLLADA") && collada::collada_local_name(plain) == String("COLLADA"),
            "local name changed");
  });
  run("numbers: whitespace and absent element", [&] {
    const auto document = parse("<root><float_array>1 2.5  -3\n4</float_array></root>");
    numbers_equal(collada::collada_numbers(collada::collada_child(document, String("float_array"))), {1, 2.5, -3, 4});
    require(collada::collada_numbers(std::nullopt).empty(), "absent element must yield no numbers");
  });
  run("text: trim and missing child", [&] {
    require(collada::collada_text(collada::collada_child(namespaced, String("asset")), String("up_axis")) == std::optional<String>{String("Z_UP")},
            "namespace text changed");
    require(collada::collada_text(collada::collada_child(plain, String("asset")), String("up_axis")) == std::optional<String>{String("Y_UP")},
            "plain text changed");
    require(!collada::collada_text(collada::collada_child(plain, String("asset")), String("copyright")), "missing text must be null");
  });
  run("text: empty becomes null", [&] {
    require(!collada::collada_text(parse("<asset><copyright>   </copyright></asset>"), String("copyright")), "empty text must be null");
  });
  run("parse floats: sign/fraction/exponent", [&] {
    numbers_equal(collada::parse_collada_floats(String(" 1  -2.5\t3e2\n")), {1, -2.5, 300});
  });
  run("parse floats: skip nonnumeric", [&] {
    numbers_equal(collada::parse_collada_floats(String("1 x 2")), {1, 2});
  });
  run("parse floats: shipped whitespace zero", [&] {
    numbers_equal(collada::parse_collada_floats(String("   ")), {0});
  });
  run("walk: parent before children", [&] {
    std::vector<String> seen;
    collada::walk_collada_element(plain, [&](Element element) { seen.push_back(collada::collada_local_name(element)); });
    require(seen == std::vector<String>{String("COLLADA"), String("asset"), String("up_axis"), String("node"), String("node")},
            "walk order/coverage changed");
  });
  run("numeric edges: finite filter, zero and Number conversion", [&] {
    const auto values = collada::collada_numbers(parse("<n>-0 0 1e309 -Infinity NaN bad 4 0x10</n>"));
    numbers_equal(values, {-0.0, 0.0, 4.0, 16.0});
    require(std::signbit(values[0]) && !std::signbit(values[1]), "finite filter lost signed zero");
    require(collada::collada_numbers(parse("<n> \t </n>")).empty(), "colladaNumbers blank must stay empty");
    const auto parsed = collada::parse_collada_floats(String("-0 0 Infinity -Infinity NaN nope 1e309 3e1 0x10"));
    numbers_equal(parsed, {-0.0, 0.0, 30.0, 16.0});
    require(std::signbit(parsed[0]) && !std::signbit(parsed[1]), "parse floats lost signed zero");
    numbers_equal(collada::parse_collada_floats(String("")), {0});
  });
  run("owners: live attributes, exact references, callback exception", [&] {
    const auto nodes = collada::collada_children(plain, String("node"));
    nodes[0]->attributes.set(String("id"), String("updated"));
    require(collada::collada_id_of(plain->children[1]) == std::optional<String>{String("updated")}, "child owner was copied");
    const auto empty_id = parse("<node id=\"\"/>");
    require(collada::collada_id_of(empty_id) == std::optional<String>{String("")}, "empty id must stay present");
    bool threw = false;
    int visits = 0;
    try { collada::walk_collada_element(plain, [&](Element element) {
      ++visits;
      if (element == plain->children[0]) throw std::runtime_error("stop");
    }); } catch (const std::runtime_error&) { threw = true; }
    require(threw && visits == 2, "walk did not propagate callback exception in order");
    const auto multiple = parse("<a:b:mesh/>");
    require(collada::collada_local_name(multiple) == String("b:mesh"), "local name must strip only the first colon");
  });
  if (failures) return 1;
  std::cout << "Original Collada public header: " << cases << " cases passed (14 pinned, 2 numeric/owner edge groups), all nine helpers\n";
  return 0;
}
