#include <flight/xml/contract.hpp>

#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// Native port of packages/xml/src/xmlParse.test.ts and xmlQuery.test.ts (a627849), every case, against the
// generated headers (no override).

namespace {

using flight::String;
using flight::types::XmlElement;
namespace xml = flight::xml;
using Element = flight::Ref<XmlElement>;
using Attributes = flight::Record<String, String>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

std::string repeat(const std::string& value, int count) {
  std::string out;
  for (int i = 0; i < count; ++i) out += value;
  return out;
}

String attr(const Attributes& attributes, const char* key) {
  const auto value = attributes.get(String(key));
  return value.has_value() ? *value : String("<absent>");
}

bool attributes_equal(const Attributes& attributes, const std::vector<std::pair<const char*, const char*>>& expected) {
  if (attributes.size() != expected.size()) return false;
  for (const auto& [key, value] : expected) {
    const auto actual = attributes.get(String(key));
    if (!actual.has_value() || *actual != String(value)) return false;
  }
  return true;
}

std::optional<Element> parse(const std::string& source) { return xml::parse_xml_document(String(source.c_str())); }

std::string doctype(const std::string& subset, const std::string& body) {
  return "<?xml version=\"1.0\"?><!DOCTYPE root [" + subset + "]><root>" + body + "</root>";
}

void test_parse_xml_attributes() {
  const auto quoted = xml::parse_xml_attributes(String("name=\"hero\" x=\"10\" y=\"20\""));
  check(attr(quoted, "name") == String("hero") && attr(quoted, "x") == String("10") && attr(quoted, "y") == String("20"), "parseXmlAttributes parses double-quoted attributes");
  const auto single = xml::parse_xml_attributes(String("name='hero' x='10'"));
  check(attr(single, "name") == String("hero") && attr(single, "x") == String("10"), "parseXmlAttributes parses single-quoted attributes");
  check(attr(xml::parse_xml_attributes(String("name=\"a&amp;b\"")), "name") == String("a&b"), "parseXmlAttributes decodes &amp;");
  check(attr(xml::parse_xml_attributes(String("name=\"a&lt;b&gt;c\"")), "name") == String("a<b>c"), "parseXmlAttributes decodes &lt; and &gt;");
  check(attr(xml::parse_xml_attributes(String("name='say &quot;hi&quot;'")), "name") == String("say \"hi\""), "parseXmlAttributes decodes &quot;");
  check(attr(xml::parse_xml_attributes(String("name=\"it&apos;s\"")), "name") == String("it's"), "parseXmlAttributes decodes &apos;");
  check(attr(xml::parse_xml_attributes(String("name=\"&#65;\"")), "name") == String("A"), "parseXmlAttributes decodes numeric character references");
  bool threw = false;
  Attributes out_of_range;
  try {
    out_of_range = xml::parse_xml_attributes(String("decimal=\"&#1114112;\" hex=\"&#x110000;\""));
  } catch (...) {
    threw = true;
  }
  check(!threw && attributes_equal(out_of_range, {{"decimal", "&#1114112;"}, {"hex", "&#x110000;"}}),
        "parseXmlAttributes preserves out-of-range numeric references without throwing");
  check(xml::parse_xml_attributes(String("")).size() == 0, "parseXmlAttributes returns an empty object for an empty string");
}

void test_parse_xml_document() {
  {
    const auto root = parse("<TextureAtlas imagePath=\"sheet.png\"></TextureAtlas>");
    check(root.has_value() && (*root)->name == String("TextureAtlas") && attr((*root)->attributes, "imagePath") == String("sheet.png") &&
              (*root)->children.size() == 0,
          "parseXmlDocument returns the first top-level element as the root");
  }
  {
    const auto root = parse("<sub x=\"10\" y=\"20\" width=\"32\" height=\"64\"/>");
    check(root.has_value() && attributes_equal((*root)->attributes, {{"x", "10"}, {"y", "20"}, {"width", "32"}, {"height", "64"}}),
          "parseXmlDocument parses multiple attributes on a single element");
  }
  {
    const auto root = parse("<node a='single' b=\"double\"/>");
    check(root.has_value() && attributes_equal((*root)->attributes, {{"a", "single"}, {"b", "double"}}),
          "parseXmlDocument accepts both single and double quoted attribute values");
  }
  {
    const auto root = parse("<TextureAtlas imagePath=\"s.png\"><SubTexture name=\"a\" x=\"0\" y=\"0\"/><SubTexture name=\"b\" x=\"8\" y=\"8\"/></TextureAtlas>");
    check(root.has_value() && (*root)->name == String("TextureAtlas") && (*root)->children.size() == 2 &&
              (*root)->children[0]->name == String("SubTexture") && attr((*root)->children[0]->attributes, "name") == String("a") &&
              attr((*root)->children[1]->attributes, "name") == String("b"),
          "parseXmlDocument nests children under their parent open tag");
  }
  {
    const auto root = parse("<root><leaf/><sibling/></root>");
    check(root.has_value() && (*root)->children.size() == 2 && (*root)->children[0]->name == String("leaf") && (*root)->children[1]->name == String("sibling"),
          "parseXmlDocument treats self-closing tags as leaves rather than opening a scope");
  }
  {
    const auto root = parse("<a><b><c value=\"deep\"/></b></a>");
    check(root.has_value() && (*root)->name == String("a") && (*root)->children[0]->name == String("b") &&
              (*root)->children[0]->children[0]->name == String("c") && attr((*root)->children[0]->children[0]->attributes, "value") == String("deep"),
          "parseXmlDocument supports deep nesting across multiple levels");
  }
  check(parse(repeat("<node>", 128) + "<leaf/>" + repeat("</node>", 128)).has_value(), "parseXmlDocument accepts element nesting with broad margin");
  {
    bool threw = false;
    std::optional<Element> root;
    try {
      root = parse(repeat("<node>", 300) + "<leaf/>" + repeat("</node>", 300));
    } catch (...) {
      threw = true;
    }
    check(!threw && !root.has_value(), "parseXmlDocument returns null instead of exhausting the stack on excessive nesting");
  }
  check(!parse("").has_value() && !parse("   just text, no tags   ").has_value(), "parseXmlDocument returns null when the input contains no recognizable element");
  {
    const auto root = parse("<a></a></b>");
    check(root.has_value() && (*root)->name == String("a"), "parseXmlDocument tolerates unbalanced close tags without underflowing the stack");
  }
  {
    const auto root = parse("<first/><second/>");
    check(root.has_value() && (*root)->name == String("first"), "parseXmlDocument returns the first element when several share the top level");
  }
  {
    const auto root = parse("<my-tag.v2 data_key=\"x\"/>");
    check(root.has_value() && (*root)->name == String("my-tag.v2") && attr((*root)->attributes, "data_key") == String("x"),
          "parseXmlDocument allows dots, hyphens, and underscores in tag and attribute names");
  }
  {
    const auto root = parse("<root><!-- This is a comment --><child/></root>");
    check(root.has_value() && (*root)->children.size() == 1 && (*root)->children[0]->name == String("child"), "parseXmlDocument strips XML comments");
  }
  {
    const auto root = parse("<?xml version=\"1.0\" encoding=\"UTF-8\"?><root/>");
    check(root.has_value() && (*root)->name == String("root"), "parseXmlDocument strips the XML declaration");
  }
  {
    const auto root = parse("<root><key>hello</key></root>");
    check(root.has_value() && (*root)->children[0]->text == String("hello"), "parseXmlDocument parses text content");
  }
  {
    const auto root = parse("<text>Hello <tspan>world</tspan>!</text>");
    const auto& content = (*root)->content;
    check(root.has_value() && content.size() == 3 && std::get<String>(content[0]) == String("Hello ") &&
              std::get<Element>(content[1])->name == String("tspan") && std::get<String>(content[2]) == String("!") && (*root)->text == String("Hello!"),
          "parseXmlDocument preserves the order of mixed text and child elements");
  }
  {
    const auto root = parse("<root><![CDATA[<tag>&amp;<!-- literal -->]]><child/>tail</root>");
    check(root.has_value() && (*root)->children.size() == 1 && (*root)->children[0]->name == String("child") &&
              std::get<String>((*root)->content[0]) == String("<tag>&amp;<!-- literal -->") && (*root)->text == String("<tag>&amp;<!-- literal -->tail"),
          "parseXmlDocument preserves markup and comment syntax inside CDATA as text");
  }
  {
    const auto root = parse("<node attr=\"a>b\"/>");
    check(root.has_value() && (*root)->name == String("node") && attr((*root)->attributes, "attr") == String("a>b") && (*root)->children.size() == 0,
          "parseXmlDocument keeps a > inside a quoted attribute value as data");
  }
  {
    const auto root = parse("<root><child path='x>y'/></root>");
    check(root.has_value() && (*root)->children.size() == 1 && attr((*root)->children[0]->attributes, "path") == String("x>y"),
          "parseXmlDocument keeps a > inside a single-quoted attribute value and still parses children");
  }
  {
    const auto root = parse(
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\" [ <!ENTITY x \"y\"> ]><plist version=\"1.0\"/>");
    check(root.has_value() && (*root)->name == String("plist") && attr((*root)->attributes, "version") == String("1.0"),
          "parseXmlDocument strips a DOCTYPE that carries an internal subset");
  }
  {
    const auto root = parse("<!DOCTYPE root [<!ENTITY tricky \"a ] > b\"><!ELEMENT root (#PCDATA)>]><root value=\"ok\"/>");
    check(root.has_value() && (*root)->name == String("root") && attr((*root)->attributes, "value") == String("ok"),
          "parseXmlDocument strips an internal subset when an entity value contains ] and >");
  }
  {
    const auto root = parse("<!DOCTYPE root SYSTEM \"https://example.invalid/a>b.dtd\"><root/>");
    check(root.has_value() && (*root)->name == String("root"), "parseXmlDocument strips a DOCTYPE when its external identifier contains >");
  }
}

void test_internal_dtd_entities() {
  {
    const auto root = parse(doctype("<!ENTITY dot \"<circle r='4'/>\">", "&dot;"));
    check(root.has_value() && (*root)->children.size() == 1 && (*root)->children[0]->name == String("circle") &&
              attr((*root)->children[0]->attributes, "r") == String("4"),
          "internal DTD: expands an entity whose replacement is an element");
  }
  {
    const auto root = parse(doctype("<!ENTITY dot \"<circle/>\">", "&dot;&dot;&dot;"));
    check(root.has_value() && (*root)->children.size() == 3, "internal DTD: expands the same entity at every reference");
  }
  {
    // Upstream builds this with `.replace(']', '>]')`, which closes the declaration.
    auto source = doctype("<!ENTITY dot '<circle/>'", "&dot;");
    source.replace(source.find(']'), 1, ">]");
    const auto root = parse(source);
    check(root.has_value() && (*root)->children.size() >= 1 && (*root)->children[0]->name == String("circle"), "internal DTD: expands an entity declared with single quotes");
  }
  {
    const auto root = parse(doctype("<!ENTITY inner \"<circle/>\"><!ENTITY outer \"<g>&inner;</g>\">", "&outer;"));
    check(root.has_value() && (*root)->children[0]->name == String("g") && (*root)->children[0]->children[0]->name == String("circle"),
          "internal DTD: expands an entity that references another entity");
  }
  {
    const auto root = parse(doctype("<!ENTITY w \"40\">", "<rect width=\"&w;\"/>"));
    check(root.has_value() && attr((*root)->children[0]->attributes, "width") == String("40"), "internal DTD: expands an entity used as an attribute value");
  }
  {
    const auto root = parse(doctype("<!ENTITY dot \"<circle/>\">", "&missing;"));
    check(root.has_value() && (*root)->text == String("&missing;"), "internal DTD: leaves an undeclared reference alone rather than dropping it");
  }
  {
    const auto root = parse(doctype("<!ENTITY dot \"<circle/>\">", "a &amp; b"));
    check(root.has_value() && (*root)->text == String("a & b"), "internal DTD: still decodes the predefined entities");
  }
  for (const auto& [form, subset] : std::vector<std::pair<std::string, std::string>>{
           {"external SYSTEM", "<!ENTITY x SYSTEM \"/etc/passwd\">"},
           {"external PUBLIC", "<!ENTITY x PUBLIC \"-//X//EN\" \"http://example.test/x\">"},
           {"parameter", "<!ENTITY % x \"<circle/>\">"},
       }) {
    const auto root = parse(doctype(subset, "&x;"));
    check(root.has_value() && (*root)->children.size() == 0 && (*root)->text == String("&x;"), "internal DTD: does not honor an " + form + " entity");
  }
  {
    std::string subset = "<!ENTITY e0 \"aaaaaaaaaa\">";
    for (int level = 1; level <= 6; ++level) subset += "<!ENTITY e" + std::to_string(level) + " \"" + repeat("&e" + std::to_string(level - 1) + ";", 10) + "\">";
    const auto root = parse(doctype(subset, "&e6;"));
    check(root.has_value() && (*root)->text.length() < 200000, "internal DTD: does not expand a nested entity without bound");
  }
  {
    const auto root = parse(doctype("<!ENTITY loop \"&loop;\">", "&loop;"));
    check(root.has_value(), "internal DTD: terminates on an entity that references itself");
  }
}

void test_xml_query() {
  {
    const auto root = parse("<node name=\"hero\" x=\"10\"/>");
    check(xml::get_xml_element_attribute(*root, String("name")) == String("hero"), "getXmlElementAttribute returns the attribute value when present");
  }
  {
    const auto root = parse("<node name=\"hero\"/>");
    check(!xml::get_xml_element_attribute(*root, String("missing")).has_value(), "getXmlElementAttribute returns null when the attribute is absent");
  }
  {
    const auto root = parse("<node label=\"\"/>");
    const auto label = xml::get_xml_element_attribute(*root, String("label"));
    check(label.has_value() && *label == String(""), "getXmlElementAttribute returns an empty-string attribute rather than null");
  }
  {
    const auto root = parse("<node x=\"10\" scale=\"1.5\"/>");
    check(xml::get_xml_element_attribute_number(*root, String("x")) == 10.0 && xml::get_xml_element_attribute_number(*root, String("scale")) == 1.5,
          "getXmlElementAttributeNumber parses a numeric attribute");
  }
  {
    const auto root = parse("<node x=\"10\"/>");
    check(!xml::get_xml_element_attribute_number(*root, String("missing")).has_value(), "getXmlElementAttributeNumber returns null when the attribute is absent");
  }
  {
    const auto root = parse("<node x=\"abc\" y=\"\"/>");
    check(!xml::get_xml_element_attribute_number(*root, String("x")).has_value() && !xml::get_xml_element_attribute_number(*root, String("y")).has_value(),
          "getXmlElementAttributeNumber returns null when the value is not a finite number");
  }
  {
    const auto root = parse("<root><a id=\"1\"/><b/><a id=\"2\"/></root>");
    const auto first = xml::get_xml_element_child_by_name(*root, String("a"));
    check(first.has_value() && attr((*first)->attributes, "id") == String("1"), "getXmlElementChildByName returns the first matching child element");
    const auto all = xml::get_xml_element_children_by_name(*root, String("a"));
    check(all.size() == 2 && attr(all[0]->attributes, "id") == String("1") && attr(all[1]->attributes, "id") == String("2"),
          "getXmlElementChildrenByName returns all matching children in document order");
  }
  {
    const auto root = parse("<root><a/></root>");
    check(!xml::get_xml_element_child_by_name(*root, String("z")).has_value(), "getXmlElementChildByName returns null when no child matches");
    check(xml::get_xml_element_children_by_name(*root, String("z")).size() == 0, "getXmlElementChildrenByName returns an empty array when no child matches");
  }
}

}  // namespace

int main() {
  test_parse_xml_attributes();
  test_parse_xml_document();
  test_internal_dtd_entities();
  test_xml_query();
  return failures == 0 ? 0 : 1;
}
