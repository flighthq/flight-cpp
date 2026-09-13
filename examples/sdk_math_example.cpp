#include <flight/lighting/ambient_light.hpp>
#include <flight/math/interpolation.hpp>

#include <cmath>
#include <iostream>

int main() {
  const double midpoint = flight::math::lerp(10.0, 20.0, 0.5);
  const double remapped = flight::math::remap(0.25, 0.0, 1.0, -1.0, 1.0);
  const double eased = flight::math::smooth_step(0.0, 1.0, 0.5);
  if (midpoint != 15.0 || remapped != -0.5 || std::abs(eased - 0.5) > 1.0e-12) return 1;

  auto options = flight::make_ref<flight::types::AmbientLightOptions>();
  options->color = 0xffc080ff;
  options->intensity = 2.0;
  using OptionsView = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::AmbientLightOptions>>>>;
  using WritableOptionsView = flight::StructuralRef<
      flight::RowWritable<flight::RowOf<flight::Ref<flight::types::AmbientLightOptions>>>>;
  using PartialOptionsView = flight::StructuralRef<
      flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<flight::types::AmbientLightOptions>>>>>;
  const OptionsView readonly_options(options);
  const WritableOptionsView writable_options(options);
  flight::row_set<flight::RowKey<"intensity">>(writable_options, std::optional<double>{3.0});
  const auto symbol_color = flight::Symbol::for_key("color");
  flight::row_set(writable_options, symbol_color, 7);
  const PartialOptionsView partial_options = readonly_options;
  if (flight::row_get<flight::RowKey<"intensity">>(readonly_options) != std::optional<double>{3.0} ||
      flight::row_get<flight::RowKey<"color">>(partial_options) != std::optional<double>{0xffc080ff} ||
      !flight::row_has<flight::RowKey<"color">>(partial_options) ||
      flight::row_get<int>(writable_options, symbol_color) != 7) {
    return 1;
  }

  using MergedOptions = flight::StructuralRef<flight::RowMerge<
      flight::RowOf<flight::Ref<flight::types::AmbientLightOptions>>,
      flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::AmbientLightOptions>>>>>;
  const MergedOptions merged_options = readonly_options;
  if (flight::row_get<flight::RowKey<"intensity">>(merged_options) != std::optional<double>{3.0}) return 1;

  auto light = flight::lighting::create_ambient_light(readonly_options);
  if (light->kind != flight::types::ambient_light_kind || light->color != 0xffc080ff ||
      light->intensity != 3.0 || !light->enabled) {
    return 1;
  }

  std::cout << "Flight SDK math: midpoint=" << midpoint << ", remapped=" << remapped
            << ", smoothStep=" << eased << "; ambientLight=" << light->intensity << '\n';
  return 0;
}
