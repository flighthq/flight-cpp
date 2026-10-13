// Native port of all 89 test declarations (94 concrete parameterized cases) from
// packages/color/src/*.test.ts at Flight a627849, plus byte-layout, identity, transfer-threshold,
// aliasing, and unclamped-range evidence at the generated C++ boundary.
// Mutation check (2026-10-12): copied return storage, a wrong red-byte shift, a removed interpolation
// clamp, a removed gamut clamp, the wrong sRGB exponent, and a broken alpha-zero guard all failed.
#include <flight/color/contract.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <string_view>

namespace {

namespace color = flight::color;

int failures = 0;

void check(bool condition, std::string_view message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

// Vitest's toBeCloseTo(expected, digits) accepts an absolute error below 10^-digits / 2.
bool close(double actual, double expected, int digits) {
  return std::abs(actual - expected) < std::pow(10.0, -digits) / 2.0;
}

double packed(std::uint32_t value) {
  return static_cast<double>(value);
}

std::array<std::uint8_t, 4> packed_bytes(double value) {
  const auto word = static_cast<std::uint32_t>(value);
  return {
      static_cast<std::uint8_t>(word >> 24U),
      static_cast<std::uint8_t>(word >> 16U),
      static_cast<std::uint8_t>(word >> 8U),
      static_cast<std::uint8_t>(word),
  };
}

bool array_is(const flight::Array<double>& actual, std::initializer_list<double> expected) {
  if (actual.size() != expected.size()) return false;
  std::size_t index = 0;
  for (const double value : expected) {
    if (actual.element(static_cast<double>(index)) != value) return false;
    ++index;
  }
  return true;
}

void test_kelvin() {
  check(static_cast<std::uint32_t>(color::color_from_kelvin(3000.0)) % 256U == 0xffU &&
            static_cast<std::uint32_t>(color::color_from_kelvin(6500.0)) % 256U == 0xffU &&
            static_cast<std::uint32_t>(color::color_from_kelvin(10000.0)) % 256U == 0xffU,
        "Kelvin colors are always fully opaque");

  const auto daylight = packed_bytes(color::color_from_kelvin(6500.0));
  check(daylight[0] > 240U && daylight[1] > 240U && daylight[2] > 230U,
        "D65 daylight is close to neutral white");

  const auto warm = packed_bytes(color::color_from_kelvin(2000.0));
  const auto cool = packed_bytes(color::color_from_kelvin(10000.0));
  check(warm[0] >= cool[0] && warm[2] <= cool[2],
        "lower Kelvin is warmer with no less red and no more blue");
  check(color::color_from_kelvin(500.0) == color::color_from_kelvin(1000.0),
        "Kelvin clamps below 1000 K");
  check(color::color_from_kelvin(50000.0) == color::color_from_kelvin(40000.0),
        "Kelvin clamps above 40000 K");

  const double sample = color::color_from_kelvin(5500.0);
  check(sample >= 0.0 && sample <= packed(0xffffffffU) && std::trunc(sample) == sample,
        "Kelvin returns an unsigned packed 32-bit integer");
}

void test_hsl() {
  const auto allocated = color::allocate_hsl_color();
  const auto separately_allocated = color::allocate_hsl_color();
  check(array_is(allocated, {0.0, 0.0, 0.0}), "allocateHslColor returns three zero components");
  check(allocated.identity() != separately_allocated.identity(),
        "allocateHslColor returns fresh storage");

  flight::Array<double> rgb{0.0, 0.0, 0.0, 1.0};
  color::hsl_to_rgb(rgb, 0.0, 0.0, 0.5);
  check(close(rgb.element(0.0), 0.5, 2) && close(rgb.element(1.0), 0.5, 2) &&
            close(rgb.element(2.0), 0.5, 2),
        "HSL saturation zero produces achromatic RGB");

  color::hsl_to_rgb(rgb, 0.0, 1.0, 0.5);
  check(close(rgb.element(0.0), 1.0, 5) && close(rgb.element(1.0), 0.0, 5) &&
            close(rgb.element(2.0), 0.0, 5),
        "HSL primary red converts to RGB");

  flight::Array<double> alpha_guard{0.0, 0.0, 0.0, 0.75};
  color::hsl_to_rgb(alpha_guard, 120.0, 1.0, 0.5);
  check(alpha_guard.element(3.0) == 0.75, "HSL conversion leaves alpha untouched");

  auto hsl = color::allocate_hsl_color();
  color::rgb_to_hsl(hsl, packed(0xff0000ffU));
  check(close(hsl.element(0.0), 0.0, 3) && close(hsl.element(1.0), 1.0, 5) &&
            close(hsl.element(2.0), 0.5, 5),
        "packed red converts to HSL 0,1,0.5");
  color::rgb_to_hsl(hsl, packed(0xffffffffU));
  check(hsl.element(0.0) == 0.0 && hsl.element(1.0) == 0.0 &&
            close(hsl.element(2.0), 1.0, 5),
        "packed white converts to achromatic HSL white");
  color::rgb_to_hsl(hsl, packed(0x000000ffU));
  check(hsl.element(0.0) == 0.0 && hsl.element(1.0) == 0.0 &&
            close(hsl.element(2.0), 0.0, 5),
        "packed black converts to the HSL origin");
  color::rgb_to_hsl(hsl, packed(0x00ff00ffU));
  check(close(hsl.element(0.0), 120.0, 3) && close(hsl.element(1.0), 1.0, 5) &&
            close(hsl.element(2.0), 0.5, 5),
        "green-dominant packed color takes the HSL green branch");
  color::rgb_to_hsl(hsl, packed(0xff00ffffU));
  check(close(hsl.element(0.0), 300.0, 3) && close(hsl.element(1.0), 1.0, 5) &&
            close(hsl.element(2.0), 0.5, 5),
        "red-dominant HSL hue wraps when blue exceeds green");
  color::rgb_to_hsl(hsl, packed(0xff8080ffU));
  check(close(hsl.element(0.0), 0.0, 3) && close(hsl.element(1.0), 1.0, 5) &&
            close(hsl.element(2.0), 0.75098, 5),
        "HSL conversion uses the high-lightness saturation formula");

  const auto returned = color::rgb_to_hsl(hsl, packed(0xff0000ffU));
  check(returned.identity() == hsl.identity(), "rgbToHsl returns the exact out array");

  color::rgb_to_hsl(hsl, packed(0x3c8ab5ffU));
  flight::Array<double> roundtrip{0.0, 0.0, 0.0, 1.0};
  color::hsl_to_rgb(roundtrip, hsl.element(0.0), hsl.element(1.0), hsl.element(2.0));
  const auto repacked = packed_bytes(color::pack_color(
      roundtrip.element(0.0), roundtrip.element(1.0), roundtrip.element(2.0), 1.0));
  const auto original = packed_bytes(packed(0x3c8ab5ffU));
  check(std::abs(static_cast<int>(repacked[0]) - static_cast<int>(original[0])) <= 1 &&
            std::abs(static_cast<int>(repacked[1]) - static_cast<int>(original[1])) <= 1 &&
            std::abs(static_cast<int>(repacked[2]) - static_cast<int>(original[2])) <= 1,
        "packed RGB round-trips through HSL within one byte LSB");

  flight::Array<double> wrapped{0.0, 0.0, 0.0, 1.0};
  color::hsl_to_rgb(wrapped, -60.0, 1.0, 0.5);
  check(close(wrapped.element(0.0), 1.0, 8) && close(wrapped.element(1.0), 0.0, 8) &&
            close(wrapped.element(2.0), 1.0, 8),
        "HSL hue normalization wraps negative input");
}

void test_hsv() {
  const auto allocated = color::allocate_hsv_color();
  const auto separately_allocated = color::allocate_hsv_color();
  check(array_is(allocated, {0.0, 0.0, 0.0}), "allocateHsvColor returns three zero components");
  check(allocated.identity() != separately_allocated.identity(),
        "allocateHsvColor returns fresh storage");

  flight::Array<double> rgb{0.0, 0.0, 0.0, 1.0};
  color::hsv_to_rgb(rgb, 0.0, 0.0, 0.6);
  check(close(rgb.element(0.0), 0.6, 2) && close(rgb.element(1.0), 0.6, 2) &&
            close(rgb.element(2.0), 0.6, 2),
        "HSV saturation zero produces achromatic RGB");
  color::hsv_to_rgb(rgb, 0.0, 1.0, 1.0);
  check(close(rgb.element(0.0), 1.0, 5) && close(rgb.element(1.0), 0.0, 5) &&
            close(rgb.element(2.0), 0.0, 5),
        "HSV zero hue produces red");
  color::hsv_to_rgb(rgb, 120.0, 1.0, 1.0);
  check(close(rgb.element(0.0), 0.0, 5) && close(rgb.element(1.0), 1.0, 5) &&
            close(rgb.element(2.0), 0.0, 5),
        "HSV 120 degree hue produces green");

  struct SectorCase {
    double hue;
    std::array<double, 3> expected;
  };
  for (const auto& sector : std::array{
           SectorCase{90.0, {0.5, 1.0, 0.0}},
           SectorCase{210.0, {0.0, 0.5, 1.0}},
           SectorCase{270.0, {0.5, 0.0, 1.0}},
           SectorCase{330.0, {1.0, 0.0, 0.5}},
       }) {
    flight::Array<double> actual{0.0, 0.0, 0.0, 0.75};
    color::hsv_to_rgb(actual, sector.hue, 1.0, 1.0);
    check(close(actual.element(0.0), sector.expected[0], 5) &&
              close(actual.element(1.0), sector.expected[1], 5) &&
              close(actual.element(2.0), sector.expected[2], 5) && actual.element(3.0) == 0.75,
          "HSV color-wheel sector and untouched alpha match upstream");
  }

  auto hsv = color::allocate_hsv_color();
  color::rgb_to_hsv(hsv, packed(0xff0000ffU));
  check(close(hsv.element(0.0), 0.0, 3) && close(hsv.element(1.0), 1.0, 5) &&
            close(hsv.element(2.0), 1.0, 5),
        "packed red converts to HSV 0,1,1");
  color::rgb_to_hsv(hsv, packed(0x000000ffU));
  check(hsv.element(1.0) == 0.0 && close(hsv.element(2.0), 0.0, 5),
        "packed black converts to zero HSV saturation and value");
  color::rgb_to_hsv(hsv, packed(0xffffffffU));
  check(hsv.element(1.0) == 0.0 && close(hsv.element(2.0), 1.0, 5),
        "packed white converts to zero saturation and full value");

  struct HueCase {
    std::uint32_t packed_color;
    double expected;
  };
  for (const auto& hue : std::array{
           HueCase{0x00ff00ffU, 120.0},
           HueCase{0x0000ffffU, 240.0},
           HueCase{0xff00ffffU, 300.0},
       }) {
    color::rgb_to_hsv(hsv, packed(hue.packed_color));
    check(close(hsv.element(0.0), hue.expected, 3) && close(hsv.element(1.0), 1.0, 5) &&
              close(hsv.element(2.0), 1.0, 5),
          "packed primary hue converts through the correct HSV branch");
  }
  const auto returned = color::rgb_to_hsv(hsv, packed(0xff0000ffU));
  check(returned.identity() == hsv.identity(), "rgbToHsv returns the exact out array");

  flight::Array<double> wrapped{0.0, 0.0, 0.0, 0.25};
  color::hsv_to_rgb(wrapped, -30.0, 1.0, 1.0);
  check(close(wrapped.element(0.0), 1.0, 8) && close(wrapped.element(1.0), 0.0, 8) &&
            close(wrapped.element(2.0), 0.5, 8) && wrapped.element(3.0) == 0.25,
        "HSV hue normalization wraps negative input without touching alpha");
}

void test_interpolation() {
  check(color::lerp_color(packed(0xff0000ffU), packed(0x0000ffffU), 0.0) ==
            packed(0xff0000ffU),
        "packed interpolation returns start at zero");
  check(color::lerp_color(packed(0xff0000ffU), packed(0x0000ffffU), 1.0) ==
            packed(0x0000ffffU),
        "packed interpolation returns end at one");
  check(color::lerp_color(packed(0xff0000ffU), packed(0x0000ffffU), -1.0) ==
            packed(0xff0000ffU),
        "packed interpolation clamps below zero");
  check(color::lerp_color(packed(0xff0000ffU), packed(0x0000ffffU), 2.0) ==
            packed(0x0000ffffU),
        "packed interpolation clamps above one");
  check(color::lerp_color(packed(0x40608020U), packed(0x80a0c0e0U), -1.0) ==
            packed(0x40608020U),
        "packed interpolation clamps below zero before non-extreme channel interpolation");
  check(color::lerp_color(packed(0x40608020U), packed(0x80a0c0e0U), 2.0) ==
            packed(0x80a0c0e0U),
        "packed interpolation clamps above one before non-extreme channel interpolation");
  check(color::lerp_color(packed(0x000000ffU), packed(0xffffffffU), 0.5) ==
            packed(0xbcbcbcffU),
        "gamma-correct packed midpoint has exact sRGB bytes");
  check(color::lerp_color(packed(0xff000000U), packed(0xff0000ffU), 0.5) ==
            packed(0xff000080U),
        "packed interpolation keeps RGB in linear space and alpha as exact linear coverage");

  flight::Array<double> start{1.0, 0.0, 0.0, 1.0};
  const flight::Array<double> end{0.0, 0.0, 1.0, 1.0};
  auto out = color::allocate_linear_color();
  color::lerp_linear_color(out, start, end, 0.0);
  check(array_is(out, {1.0, 0.0, 0.0, 1.0}), "linear interpolation returns start at zero");
  color::lerp_linear_color(out, start, end, 1.0);
  check(array_is(out, {0.0, 0.0, 1.0, 1.0}), "linear interpolation returns end at one");
  const auto returned = color::lerp_linear_color(out, start, end, 0.5);
  check(returned.identity() == out.identity(), "linear interpolation returns the exact out array");

  const void* start_identity = start.identity();
  color::lerp_linear_color(start, start, end, 0.5);
  check(start.identity() == start_identity && start.element(0.0) == 0.5 &&
            start.element(1.0) == 0.0 && start.element(2.0) == 0.5 && start.element(3.0) == 1.0,
        "linear interpolation is alias-safe when out and start share storage");

  color::lerp_linear_color(out, start, end, -100.0);
  check(array_is(out, {0.5, 0.0, 0.5, 1.0}), "linear interpolation clamps extreme negative t");
  color::lerp_linear_color(out, start, end, 100.0);
  check(array_is(out, {0.0, 0.0, 1.0, 1.0}), "linear interpolation clamps extreme positive t");
}

void test_luminance() {
  check(close(color::get_color_contrast_ratio(packed(0xffffffffU), packed(0x000000ffU)), 21.0, 0),
        "black-on-white contrast is 21");
  check(close(color::get_color_contrast_ratio(packed(0x808080ffU), packed(0x808080ffU)), 1.0, 3),
        "identical colors have contrast one");
  const double ratio_forward =
      color::get_color_contrast_ratio(packed(0xffffffffU), packed(0x808080ffU));
  const double ratio_reverse =
      color::get_color_contrast_ratio(packed(0x808080ffU), packed(0xffffffffU));
  check(close(ratio_forward, ratio_reverse, 8), "contrast ratio is symmetric");

  check(close(color::get_color_luminance(packed(0xffffffffU)), 1.0, 5),
        "white luminance is one");
  check(close(color::get_color_luminance(packed(0x000000ffU)), 0.0, 5),
        "black luminance is zero");
  check(color::get_color_luminance(packed(0xffffffffU)) ==
            color::get_color_luminance(packed(0xffffff00U)),
        "luminance ignores alpha exactly");
  check(close(color::get_color_luminance(packed(0x808080ffU)), 0.21586, 4),
        "mid-gray luminance matches Rec. 709");

  flight::Array<double> weights{0.0, 0.0, 0.0};
  color::get_rec2020_luminance_weights(weights);
  check(array_is(weights, {0.2627, 0.678, 0.0593}) &&
            close(weights.element(0.0) + weights.element(1.0) + weights.element(2.0), 1.0, 8),
        "Rec. 2020 weights are exact and sum to one");
  color::get_rec709_luminance_weights(weights);
  check(array_is(weights, {0.2126, 0.7152, 0.0722}) &&
            close(weights.element(0.0) + weights.element(1.0) + weights.element(2.0), 1.0, 8),
        "Rec. 709 weights are exact and sum to one");
}

void test_oklab() {
  flight::Array<double> rgb{0.0, 0.0, 0.0};
  color::clamp_linear_rgb(rgb, 1.25, -0.2, 0.5);
  check(array_is(rgb, {1.0, 0.0, 0.5}), "linear RGB clamp bounds every channel independently");

  flight::Array<double> lab{0.0, 0.0, 0.0};
  color::linear_rgb_to_oklab(lab, 1.0, 1.0, 1.0);
  check(close(lab.element(0.0), 1.0, 3) && close(lab.element(1.0), 0.0, 4) &&
            close(lab.element(2.0), 0.0, 4),
        "linear white maps to neutral Oklab white");
  color::linear_rgb_to_oklab(lab, 0.0, 0.0, 0.0);
  check(array_is(lab, {0.0, 0.0, 0.0}), "linear black maps exactly to the Oklab origin");

  color::linear_rgb_to_oklab(lab, 0.5, 0.1, 0.8);
  color::oklab_to_linear_rgb(rgb, lab.element(0.0), lab.element(1.0), lab.element(2.0));
  check(close(rgb.element(0.0), 0.5, 5) && close(rgb.element(1.0), 0.1, 5) &&
            close(rgb.element(2.0), 0.8, 5),
        "saturated linear RGB round-trips through Oklab");

  color::oklab_to_linear_rgb(rgb, 0.5, 1.0, 0.0);
  check((rgb.element(0.0) < 0.0 || rgb.element(1.0) < 0.0 || rgb.element(2.0) < 0.0) &&
            (rgb.element(0.0) > 1.0 || rgb.element(1.0) > 1.0 || rgb.element(2.0) > 1.0),
        "Oklab inverse preserves negative and greater-than-one out-of-gamut channels");

  color::linear_rgb_to_oklab(lab, 1.1, -0.05, 0.2);
  color::oklab_to_linear_rgb(rgb, lab.element(0.0), lab.element(1.0), lab.element(2.0));
  check(close(rgb.element(0.0), 1.1, 5) && close(rgb.element(1.0), -0.05, 5) &&
            close(rgb.element(2.0), 0.2, 5),
        "out-of-gamut linear RGB round-trips without a hidden channel clamp");
}

void test_packing() {
  const auto allocated = color::allocate_linear_color();
  const auto separately_allocated = color::allocate_linear_color();
  check(array_is(allocated, {0.0, 0.0, 0.0, 0.0}),
        "allocateLinearColor returns four zero components");
  check(allocated.identity() != separately_allocated.identity(),
        "allocateLinearColor returns fresh storage");

  check(color::compute_rgba_css_string(packed(0xff0000ffU)) == flight::String("rgba(255,0,0,1)") &&
            color::compute_rgba_css_string(packed(0x00ff0080U)) ==
                flight::String("rgba(0,255,0,0.5019607843137255)") &&
            color::compute_rgba_css_string(0.0) == flight::String("rgba(0,0,0,0)"),
        "RGBA CSS formatting preserves every channel exactly");
  check(color::compute_rgb_hex_string(packed(0x00ff0000U)) == flight::String("#ff0000") &&
            color::compute_rgb_hex_string(packed(0x0000ff00U)) == flight::String("#00ff00") &&
            color::compute_rgb_hex_string(packed(0x000000ffU)) == flight::String("#0000ff") &&
            color::compute_rgb_hex_string(0.0) == flight::String("#000000") &&
            color::compute_rgb_hex_string(packed(0x00ffffffU)) == flight::String("#ffffff"),
        "RGB hexadecimal formatting uses exactly six lower-byte digits");

  check(close(color::get_color_alpha(packed(0x11223344U)), 0x44 / 255.0, 2) &&
            color::get_color_alpha(packed(0xffffffffU)) == 1.0 &&
            color::get_color_alpha(packed(0xabcdef00U)) == 0.0,
        "alpha extraction reads the low byte as linear coverage");
  check(color::get_color_rgb(packed(0xff8800ffU)) == packed(0x00ff8800U) &&
            color::get_color_rgb(packed(0x11223344U)) == packed(0x00112233U),
        "RGB extraction drops the low alpha byte");
  check(color::get_color_rgb(color::pack_opaque_color(packed(0x003366ccU))) ==
            packed(0x003366ccU),
        "RGB extraction and opaque packing are exact inverses");

  check(color::pack_color(1.0, 1.0, 1.0, 1.0) == packed(0xffffffffU),
        "packColor packs white to all one bytes");
  check(color::pack_color(0.0, 0.0, 0.0, 1.0) == packed(0x000000ffU),
        "packColor packs opaque black in RGBA order");
  check(color::pack_color(2.0, -1.0, 0.5, 1.0) ==
            color::pack_color(1.0, 0.0, 0.5, 1.0),
        "packColor clamps out-of-range components");
  const double byte_fixture = color::pack_color(0x11 / 255.0, 0x22 / 255.0, 0x33 / 255.0,
                                                0x44 / 255.0);
  check(byte_fixture == packed(0x11223344U) &&
            packed_bytes(byte_fixture) == std::array<std::uint8_t, 4>{0x11, 0x22, 0x33, 0x44},
        "packColor stores exact bytes in R,G,B,A order");

  auto linear = color::allocate_linear_color();
  const auto white_returned = color::unpack_color_to_linear(linear, packed(0xffffffffU));
  check(array_is(linear, {1.0, 1.0, 1.0, 1.0}) &&
            white_returned.identity() == linear.identity() &&
            color::pack_linear_to_color(linear) == packed(0xffffffffU),
        "linear white unpack/pack is exact and preserves out identity");
  color::unpack_color_to_linear(linear, packed(0x000000ffU));
  check(array_is(linear, {0.0, 0.0, 0.0, 1.0}) &&
            color::pack_linear_to_color(linear) == packed(0x000000ffU),
        "linear opaque black unpack/pack is exact");
  color::unpack_color_to_linear(linear, packed(0x00000080U));
  check(color::pack_linear_to_color(linear) == packed(0x00000080U) &&
            close(linear.element(3.0), 0x80 / 255.0, 6),
        "linear pack/unpack passes alpha without gamma conversion");
  color::unpack_color_to_linear(linear, packed(0x808080ffU));
  const auto mid_repacked = packed_bytes(color::pack_linear_to_color(linear));
  check(std::abs(static_cast<int>(mid_repacked[0]) - 0x80) <= 1,
        "mid-gray linear round-trip stays within one byte LSB");

  check(color::pack_opaque_color(packed(0x00336699U)) == packed(0x336699ffU) &&
            color::pack_opaque_color(0.0) == packed(0x000000ffU) &&
            color::pack_opaque_color(packed(0x00ffffffU)) == packed(0xffffffffU),
        "opaque packing widens 24-bit RGB with full alpha");
  check(color::pack_opaque_color(packed(0xaa336699U)) == packed(0x336699ffU),
        "opaque packing masks bits above the lower 24");
  check(color::pack_opaque_color(packed(0x00ffffffU)) > 0.0,
        "opaque packing returns an unsigned 32-bit value");

  check(color::set_color_alpha(packed(0xff8800ffU), 0.0) == packed(0xff880000U) &&
            color::set_color_alpha(packed(0xff880000U), 1.0) == packed(0xff8800ffU),
        "setColorAlpha changes only the low alpha byte");
  check(color::set_color_alpha(packed(0x112233ffU), 2.0) == packed(0x112233ffU) &&
            close(color::get_color_alpha(color::set_color_alpha(packed(0x112233ffU), 0.5)), 0.5, 2),
        "setColorAlpha clamps and rounds to eight bits");

  flight::Array<double> rgba{0.0, 0.0, 0.0, 0.0};
  color::unpack_color_rgba(rgba, packed(0xffffffffU));
  check(array_is(rgba, {1.0, 1.0, 1.0, 1.0}), "raw RGBA unpack extracts white as all ones");
  color::unpack_color_rgba(rgba, packed(0x808080ffU));
  check(close(rgba.element(0.0), 0x80 / 255.0, 4) && rgba.element(0.0) > 0.5,
        "raw RGBA unpack does not gamma-decode mid sRGB");

  color::unpack_color_to_linear(linear, packed(0xffffffffU));
  check(array_is(linear, {1.0, 1.0, 1.0, 1.0}), "linear unpack decodes white endpoint exactly");
  color::unpack_color_to_linear(linear, packed(0x000000ffU));
  check(array_is(linear, {0.0, 0.0, 0.0, 1.0}), "linear unpack decodes black endpoint exactly");
  color::unpack_color_to_linear(linear, packed(0x808080ffU));
  check(close(linear.element(0.0), 0.21586, 4) &&
            linear.element(0.0) == linear.element(1.0) &&
            linear.element(1.0) == linear.element(2.0) && linear.element(0.0) < 0.5,
        "linear unpack gamma-decodes equal mid-gray channels below one half");
}

void test_premultiplication() {
  check(color::premultiply_color_alpha(packed(0xff0000ffU)) == packed(0xff0000ffU),
        "premultiplication leaves opaque color unchanged");
  check(color::premultiply_color_alpha(packed(0xff00007fU)) == packed(0x7f00007fU),
        "premultiplication scales RGB and preserves alpha bytes");
  check(color::premultiply_color_alpha(packed(0xff000000U)) == 0.0,
        "premultiplication maps transparent color to transparent black");
  check(color::unpremultiply_color_alpha(
            color::premultiply_color_alpha(packed(0xab3456ffU))) == packed(0xab3456ffU),
        "opaque premultiply/unpremultiply round-trip is exact");

  check(color::unpremultiply_color_alpha(packed(0xff0000ffU)) == packed(0xff0000ffU),
        "unpremultiplication leaves opaque color unchanged");
  const double premultiplied = color::premultiply_color_alpha(packed(0xff000080U));
  check(packed_bytes(color::unpremultiply_color_alpha(premultiplied))[3] == 0x80,
        "unpremultiplication preserves the alpha byte");
  check(color::unpremultiply_color_alpha(packed(0xff000000U)) == packed(0xff000000U),
        "unpremultiplication guards division by zero and returns transparent input unchanged");

  const double byte_fixture = color::premultiply_color_alpha(packed(0x80402080U));
  check(byte_fixture == packed(0x40201080U) &&
            color::unpremultiply_color_alpha(byte_fixture) == packed(0x80402080U),
        "premultiplication has exact RGBA byte layout and reversible half-alpha rounding");
}

void test_srgb_transfer() {
  check(color::linear_channel_to_srgb(0.0) == 0.0, "linear-to-sRGB maps zero exactly");
  check(close(color::linear_channel_to_srgb(1.0), 1.0, 8), "linear-to-sRGB maps one to one");
  const double linear = 0.5;
  check(close(color::srgb_channel_to_linear(color::linear_channel_to_srgb(linear)), linear, 8),
        "linear-to-sRGB round-trips through decode");

  check(color::srgb_channel_to_linear(0.0) == 0.0 &&
            close(color::srgb_channel_to_linear(1.0), 1.0, 8),
        "sRGB-to-linear maps both endpoints exactly");
  check(close(color::srgb_channel_to_linear(0x80 / 255.0), 0.21586, 5) &&
            color::srgb_channel_to_linear(0x80 / 255.0) < 0.5,
        "sRGB-to-linear decodes mid-gray below linear midpoint");
  const double srgb = 0.25;
  check(close(color::linear_channel_to_srgb(color::srgb_channel_to_linear(srgb)), srgb, 8),
        "sRGB-to-linear round-trips through encode");

  check(color::linear_channel_to_srgb(0.0031308) == 0.0031308 * 12.92,
        "linear transfer threshold takes its exact low branch");
  check(color::srgb_channel_to_linear(0.04045) == 0.04045 / 12.92,
        "sRGB transfer threshold takes its exact low branch");
  check(color::linear_channel_to_srgb(-0.25) < 0.0 &&
            color::linear_channel_to_srgb(2.0) > 1.0,
        "channel transfer deliberately does not hide negative or HDR inputs by clamping");
}

}  // namespace

int main() {
  test_kelvin();
  test_hsl();
  test_hsv();
  test_interpolation();
  test_luminance();
  test_oklab();
  test_packing();
  test_premultiplication();
  test_srgb_transfer();
  if (failures != 0) {
    std::cerr << failures << " color assertion(s) failed\n";
    return 1;
  }
  std::cout << "color generated semantics passed\n";
  return 0;
}
