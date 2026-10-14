// Focused parity for @flighthq/effects-wgpu's wgpuEffectGradientRamp.ts at Flight a627849.
// CPU ramp construction remains emitted code. The SDL host profile cannot upload a WebGPU texture,
// so the narrow override evaluates that CPU work and then fails without fabricating or caching a
// texture. WebGPU texture creation and queue upload remain explicit host-profile debt.
//
// MUTATION SENSITIVITY, measured 2026-10-14 against isolated one-at-a-time override changes:
// returning a fabricated default texture instead of throwing produced six failures, and skipping
// CPU ramp construction before the profile boundary produced one. Each mutant was restored after
// the measurement.
#include <flight/effects_wgpu/wgpu_effect_gradient_ramp.hpp>

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

namespace effects_wgpu = flight::effects_wgpu;
namespace types = flight::types;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void check_pixel(const flight::Uint8ClampedArray& data, std::size_t pixel,
                 unsigned red, unsigned green, unsigned blue, unsigned alpha,
                 const std::string& label) {
  const std::size_t offset = pixel * 4;
  check(data[offset] == red && data[offset + 1] == green &&
            data[offset + 2] == blue && data[offset + 3] == alpha,
        label);
}

template <typename Function>
bool throws_profile_error(Function&& function) {
  try {
    function();
  } catch (const flight::Error& error) {
    return std::string(error.what()) ==
           "effects-wgpu: gradient texture upload is unavailable in this profile";
  } catch (...) {
  }
  return false;
}

void cpu_ramp_construction() {
  const auto empty = effects_wgpu::build_ramp_data({}, {}, {});
  bool all_zero = empty.size() == 256 * 4;
  for (const auto value : empty) all_zero = all_zero && value == 0;
  check(all_zero, "an empty gradient produces one zeroed 256-pixel ramp");

  const auto ramp = effects_wgpu::build_ramp_data(
      {0xff0000, 0x0000ff}, {0.0, 1.0}, {0.0, 255.0});
  check_pixel(ramp, 0, 255, 0, 0, 0,
              "the first stop supplies the first ramp pixel");
  check_pixel(ramp, 127, 128, 0, 127, 127,
              "the lower midpoint uses source rounding and interpolation");
  check_pixel(ramp, 128, 127, 0, 128, 128,
              "the upper midpoint uses source rounding and interpolation");
  check_pixel(ramp, 255, 0, 0, 255, 255,
              "the final stop supplies the final ramp pixel");

  const auto clamped = effects_wgpu::build_ramp_data(
      {0x112233, 0xaabbcc}, {0.25, 0.75}, {64.0, 192.0});
  check_pixel(clamped, 0, 0x11, 0x22, 0x33, 64,
              "pixels before the first ratio use the first stop");
  check_pixel(clamped, 255, 0xaa, 0xbb, 0xcc, 191,
              "pixels after the last ratio use the final stop");
}

void profile_boundary_and_cache_identity() {
  const auto first_state = flight::make_ref<types::WgpuRenderState>();
  const auto second_state = flight::make_ref<types::WgpuRenderState>();
  const flight::Array<double> colors{0x102030, 0x405060};
  const flight::Array<double> alphas{0.25, 0.75};
  const flight::Array<double> ratios{0.0, 255.0};

  check(throws_profile_error([&] {
          (void)effects_wgpu::create_wgpu_effect_gradient_ramp_texture(
              first_state, colors, alphas, ratios);
        }),
        "the upload helper fails with the exact profile boundary");

  bool source_range_error = false;
  try {
    (void)effects_wgpu::create_wgpu_effect_gradient_ramp_texture(
        first_state, {0xff0000}, {}, {0.0});
  } catch (const std::range_error&) {
    source_range_error = true;
  } catch (...) {
  }
  check(source_range_error,
        "CPU ramp evaluation precedes the unavailable upload boundary");

  check(throws_profile_error([&] {
          (void)effects_wgpu::get_wgpu_effect_gradient_ramp_texture(
              first_state, colors, alphas, ratios);
        }),
        "the public cache entry point propagates the profile boundary");
  auto first_cache = effects_wgpu::ramp_caches.get(first_state);
  check(first_cache.has_value() && first_cache->empty(),
        "a failed upload creates no fabricated texture cache entry");

  first_cache->set(flight::String("first-state-only"),
                   flight::host_sdl::WgpuTexture{});
  check(!effects_wgpu::ramp_caches.get(second_state).has_value(),
        "render-state cache keys retain reference identity");
  check(throws_profile_error([&] {
          (void)effects_wgpu::get_wgpu_effect_gradient_ramp_texture(
              second_state, colors, alphas, ratios);
        }),
        "a distinct render state reaches the same explicit boundary");

  const auto first_after = effects_wgpu::ramp_caches.get(first_state);
  const auto second_after = effects_wgpu::ramp_caches.get(second_state);
  check(first_after.has_value() && first_after->size() == 1 &&
            first_after->has(flight::String("first-state-only")),
        "a second state does not alias the first state's cache");
  check(second_after.has_value() && second_after->empty(),
        "the second failed upload also leaves its cache empty");
}

}  // namespace

int main() {
  cpu_ramp_construction();
  profile_boundary_and_cache_identity();
  return failures == 0 ? 0 : 1;
}
