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
  auto light = flight::lighting::create_ambient_light(OptionsView(options));
  if (light->kind != flight::types::ambient_light_kind || light->color != 0xffc080ff ||
      light->intensity != 2.0 || !light->enabled) {
    return 1;
  }

  std::cout << "Flight SDK math: midpoint=" << midpoint << ", remapped=" << remapped
            << ", smoothStep=" << eased << "; ambientLight=" << light->intensity << '\n';
  return 0;
}
