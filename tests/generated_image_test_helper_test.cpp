#include <flight/image/_internal_image_test_helper.hpp>

#include <iostream>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

} // namespace

int main() {
  using flight::types::HostImageDimensions;

  const auto dimensions = flight::make_ref<HostImageDimensions>(HostImageDimensions{});
  const auto source = flight::host_sdl::ImageSource::rgba8(
      2, 1, flight::Uint8ClampedArray{255, 0, 0, 255, 0, 255, 0, 255});

  flight::image::register_test_image_dimension_resolver();
  const bool resolved = flight::image::get_host_image_source_dimensions(source, dimensions);
  flight::image::unregister_test_image_dimension_resolver();

  return check(resolved, "registered test image resolver rejected a sized source") &&
                 check(dimensions->width == 2.0 && dimensions->height == 1.0,
                       "test image resolver reported incorrect dimensions") &&
                 check(!flight::image::has_host_image_dimension_resolver(),
                       "test image resolver remained registered after cleanup")
             ? 0
             : 1;
}
