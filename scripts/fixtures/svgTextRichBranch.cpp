// Compile/run the actual installed public SVG text handler. No refused-function substitutes.
#include <flight/scene2d_formats/svg_text_element.hpp>
#include <iostream>
namespace {
template<class T> auto read(const std::shared_ptr<T>& value) {
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<T>>>>(value);
}
}
int main() {
  using flight::String;
  namespace types = flight::types;
  namespace svg = flight::scene2d_formats;
  auto span = flight::make_ref<types::XmlElement>();
  span->name = "tspan"; span->attributes.set("fill", "#ff0000");
  span->text = "red"; span->content.push(String("red"));
  auto element = flight::make_ref<types::XmlElement>();
  element->name = "text"; element->attributes.set("id", "svg-text");
  element->children.push(span); element->text = "black red";
  element->content.push(String("black ")); element->content.push(span);
  auto context = flight::make_ref<types::SvgImportContext>();
  context->registry = read(svg::create_svg_registry());
  context->diagnostics = flight::Array<flight::Ref<types::ImportDiagnostic>>{};
  context->parent_by_element.set(span, element);
  auto input = flight::make_ref<types::SvgElementContext>();
  input->element = read(element); input->parent_style = read(svg::default_svg_style); input->import = context;
  const auto result = svg::svg_text_element_handler(input);
  const bool ok = result && flight::row_get<flight::RowKey<"kind">>(*result) == String("RichText");
  std::cout << "{\"publicSvgTextRichBranch\":" << (ok ? "true" : "false") << "}\n";
  return ok ? 0 : 1;
}
