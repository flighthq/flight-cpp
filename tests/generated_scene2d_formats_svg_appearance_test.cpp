// Calls the original complete public SVG document header and its real style/transform helpers.
#include <flight/scene2d_formats/svg_document.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <type_traits>

namespace {
namespace types = flight::types;
namespace svg = flight::scene2d_formats;
using flight::String;
using MatrixRead = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::Matrix>>>>;
using MatrixWrite = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<types::Matrix>>>>;
using MatrixArgument = std::optional<std::optional<MatrixRead>>;
static_assert(!std::is_constructible_v<MatrixWrite, MatrixRead>);
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
template<class T> auto readonly(const std::shared_ptr<T>& owner) {
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<T>>>>(owner);
}
void transform(const types::Node2D& target, double x, double y, double sx, double sy) {
  check(std::abs(flight::row_get<flight::RowKey<"x">>(target) - x) < 1e-10 &&
        std::abs(flight::row_get<flight::RowKey<"y">>(target) - y) < 1e-10 &&
        std::abs(flight::row_get<flight::RowKey<"scaleX">>(target) - sx) < 1e-10 &&
        std::abs(flight::row_get<flight::RowKey<"scaleY">>(target) - sy) < 1e-10,
        "original transform helpers apply the expected ordered matrix product");
}
}

int main() {
  auto target = flight::scene2d::create_display_object();
  const auto target_owner = target.shared_owner();
  auto element = flight::make_ref<types::XmlElement>();
  element->name = String("rect");
  element->attributes.set(String("id"), String("source-id"));
  element->attributes.set(String("opacity"), String("0.25"));
  element->attributes.set(String("visibility"), String("hidden"));
  auto context = flight::make_ref<types::SvgImportContext>();
  context->diagnostics = flight::Array<flight::Ref<types::ImportDiagnostic>>{};
  auto parent = readonly(svg::default_svg_style);
  auto source = readonly(element);

  // With neither transform, omitted and explicit undefined/null keep existing target coordinates.
  flight::row_set<flight::RowKey<"x">>(target, 81.0);
  flight::row_set<flight::RowKey<"y">>(target, 82.0);
  flight::row_set<flight::RowKey<"scaleX">>(target, 4.0);
  flight::row_set<flight::RowKey<"scaleY">>(target, 5.0);
  auto style = svg::apply_svg_element_appearance(target, source, parent, context);
  transform(target, 81, 82, 4, 5);
  svg::apply_svg_element_appearance(target, source, parent, context, std::nullopt);
  transform(target, 81, 82, 4, 5);
  const MatrixArgument null(std::in_place, std::nullopt);
  svg::apply_svg_element_appearance(target, source, parent, context, null);
  transform(target, 81, 82, 4, 5);
  check(style != svg::default_svg_style && style->opacity == 0.25 && svg::default_svg_style->opacity == 1,
        "the original cascade returns its authored style without changing the parent");
  check(flight::row_get<flight::RowKey<"alpha">>(target) == 0.25 &&
        !flight::row_get<flight::RowKey<"visible">>(target) &&
        flight::row_get<flight::RowKey<"name">>(target) == String("source-id"),
        "appearance writes opacity, visibility and name on the original target");

  std::weak_ptr<types::Matrix> weak;
  {
    auto geometry = flight::geometry::create_matrix(2, 0, 0, 3, 5, 7);
    weak = geometry;
    MatrixRead view(geometry);
    const auto row_owner = view.shared_owner();
    MatrixArgument present(std::in_place, std::in_place, view);
    svg::apply_svg_element_appearance(target, source, parent, context, present);
    transform(target, 5, 7, 2, 3);

    element->attributes.set(String("transform"), String("translate(10 20)"));
    svg::apply_svg_element_appearance(target, source, parent, context, present);
    transform(target, 15, 27, 2, 3);
    check(geometry->a == 2 && geometry->d == 3 && geometry->tx == 5 && geometry->ty == 7,
          "matrix multiplication does not mutate the readonly geometry input");
    geometry->a = 4; geometry->d = 6; geometry->tx = 9; geometry->ty = 11;
    svg::apply_svg_element_appearance(target, source, parent, context, present);
    transform(target, 19, 31, 4, 6);
    const auto exact = present.value().value().shared_object();
    check(present.value().value().shared_owner() == row_owner && exact.get() == geometry.get() &&
          !exact.owner_before(geometry) && !geometry.owner_before(exact),
          "the retained argument keeps the original row, native owner and control block");

    for (const auto& argument : {MatrixArgument{}, null}) {
      svg::apply_svg_element_appearance(target, source, parent, context, argument, true);
      transform(target, 10, 20, 1, 1);
      check(flight::row_get<flight::RowKey<"visible">>(target),
            "the authored visual-descendants flag preserves visibility semantics");
    }
    svg::apply_svg_element_appearance(target, source, parent, context);
    transform(target, 10, 20, 1, 1);
    element->attributes.set(String("transform"), String("matrix(1 0 0 1 10 20) scale(2 3)"));
    svg::apply_svg_element_appearance(target, source, parent, context, present);
    transform(target, 28, 53, 8, 18);
  }
  check(weak.expired(), "appearance retains no extra geometry owner after arguments leave scope");
  check(target.shared_owner() == target_owner, "all changes retain the original target row owner");
  check(context->diagnostics->size() == 0, "supported appearance and transforms add no diagnostics");
  if (!failures) std::cout << "Original applySvgElementAppearance: defaults/null/undefined, author/geometry composition, live readonly Matrix and target/style identity passed\n";
  return failures ? 1 : 0;
}
