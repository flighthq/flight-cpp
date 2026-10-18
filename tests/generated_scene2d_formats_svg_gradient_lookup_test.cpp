// Original public gradient lookup through the complete emitted module header.
#include <flight/scene2d_formats/svg_gradient.hpp>

#include <iostream>
#include <utility>
#include <vector>

namespace {
using flight::String;
namespace types = flight::types;
namespace svg = flight::scene2d_formats;
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main() {
  auto context = flight::make_ref<types::SvgImportContext>();
  auto gradient = flight::make_ref<types::SvgGradient>();
  auto color = flight::make_ref<types::SvgColor>(types::SvgColor{.alpha = 0.5, .rgb = 12});
  auto stop = flight::make_ref<types::SvgGradientStop>(types::SvgGradientStop{.color = color, .offset = 0.25});
  gradient->stops.push(stop);
  context->gradients_by_id.set(String("paint"), gradient);
  context->gradients_by_id.set(String("é😀"), gradient);
  flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::SvgImportContext>>>> view(context);
  const std::vector<std::pair<String, bool>> cases{
    {String("url(#paint)"), true}, {String(" url(#paint) "), true},
    {String("url( '#paint' )"), true}, {String("url(\"#paint\")"), true},
    {String("url(\n#paint\t)"), true}, {String("url(#é😀)"), true},
    {String("url(#missing)"), false}, {String("none"), false}, {String(""), false},
    {String("#paint"), false}, {String("URL(#paint)"), false}, {String("url(#)"), false},
    {String("url(#paint) trailing"), false}, {String("url(#pa int)"), false},
  };
  for (const auto& [text, expected] : cases) {
    auto result = svg::resolve_svg_gradient(text, view);
    check(result.has_value() == expected && (!result || result.value() == gradient),
          "lookup preserves exact URL syntax, missing results and cached gradient owner");
  }
  auto held = svg::resolve_svg_gradient(String("url(#paint)"), view).value();
  color->rgb = 99;
  check(held->stops[0] == stop && held->stops[0]->color == color && held->stops[0]->color->rgb == 99,
        "lookup retains the original array elements, color owner and live cells");
  auto replacement = flight::make_ref<types::SvgGradient>();
  context->gradients_by_id.set(String("paint"), replacement);
  check(svg::resolve_svg_gradient(String("url(#paint)"), view).value() == replacement && held == gradient,
        "later lookup sees replacement without disturbing a previously returned owner");
  context->gradients_by_id.erase(String("paint"));
  check(!svg::resolve_svg_gradient(String("url(#paint)"), view), "deleted entry remains absent");
  if (!failures) std::cout << "Original resolveSvgGradient: 14 syntax cases and owner/live-map controls passed\n";
  return failures ? 1 : 0;
}
