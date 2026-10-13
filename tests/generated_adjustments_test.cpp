// Native port of the 137 upstream test declarations in packages/adjustments/src/*.test.ts at
// Flight a627849. Cases are grouped by observable semantics and retain upstream numeric references.
// Vitest's toBeCloseTo(x, digits) means absolute error < 10^-digits / 2; close() mirrors that rule.
// Mutation check (2026-10-13): optional array recognition, composition order, LUT size flooring,
// LUT clamping, trilinear interpolation, matrix-length failure, exposure base, and HSL clamping died.
#include <flight/adjustments/contract.hpp>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

namespace adjustments = flight::adjustments;
namespace types = flight::types;

int failures = 0;

void check(bool condition, std::string_view message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool close(double actual, double expected, int digits = 5) {
  return std::abs(actual - expected) < std::pow(10.0, -digits) / 2.0;
}

bool array_close(const flight::Array<double>& actual, const flight::Array<double>& expected,
                 int digits = 10) {
  if (actual.size() != expected.size()) return false;
  for (std::size_t i = 0; i < actual.size(); ++i) {
    if (!close(actual[i], expected[i], digits)) return false;
  }
  return true;
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return flight::structural_ref_cast<View>(value);
}

using AdjustmentView = flight::StructuralRef<flight::RowReadonly<
    flight::RowOf<flight::Ref<adjustments::kind_6fea82d7aa842443>>>>;

template <typename Type>
AdjustmentView adjustment_view(const std::shared_ptr<Type>& value) {
  using Writable = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Type>>>>;
  return flight::structural_ref_cast<AdjustmentView>(Writable(value));
}

flight::Array<double> transformed(const types::ColorTransformFunction& transform,
                                  double r, double g, double b) {
  flight::Array<double> out{0.0, 0.0, 0.0};
  transform(out, r, g, b);
  return out;
}

std::uint32_t packed(double value) {
  return static_cast<std::uint32_t>(value);
}

void matrix_math() {
  const auto identity = adjustments::create_identity_color_matrix();
  check(adjustments::color_matrix_length == 20.0 && identity.size() == 20,
        "matrix constants and identity length match the contract");
  check(packed(adjustments::apply_color_matrix_to_color(identity, 0x2040607fU)) == 0x2040607fU,
        "identity matrix preserves packed color");

  const auto invert = adjustments::create_invert_color_matrix();
  check(packed(adjustments::apply_color_matrix_to_color(invert, 0xff0000ffU)) == 0x00ffffffU,
        "invert maps red to cyan and preserves alpha");
  const auto gray = adjustments::create_grayscale_color_matrix();
  const auto gray_red = packed(adjustments::apply_color_matrix_to_color(gray, 0xff0000ffU));
  check(((gray_red >> 24U) & 0xffU) == ((gray_red >> 16U) & 0xffU) &&
            ((gray_red >> 16U) & 0xffU) == ((gray_red >> 8U) & 0xffU),
        "grayscale produces equal RGB channels");

  const auto brightness = adjustments::create_brightness_color_matrix(0.25);
  check(brightness.size() == 20 && brightness.element(4) == 0.25 &&
            brightness.element(9) == 0.25 && brightness.element(14) == 0.25,
        "brightness writes each RGB offset");
  const auto mixer = adjustments::create_channel_mixer_color_matrix(
      flight::Array<double>{0, 1, 0}, flight::Array<double>{0, 0, 1},
      flight::Array<double>{1, 0, 0});
  check(packed(adjustments::apply_color_matrix_to_color(mixer, 0x112233ffU)) == 0x223311ffU,
        "channel mixer uses row-major RGB rows");

  const auto balance = adjustments::create_color_balance_color_matrix(
      flight::Array<double>{0, 0, 0}, flight::Array<double>{50, 0, 0},
      flight::Array<double>{0, 0, 0});
  check(balance.size() == 20 && close(balance.element(4), 0.25),
        "color balance weights midtones by one half and normalizes percent");
  check(array_close(adjustments::create_color_balance_color_matrix(
                        flight::Array<double>{0, 0, 0}, flight::Array<double>{0, 0, 0},
                        flight::Array<double>{0, 0, 0}), identity),
        "zero color balance is identity");

  const auto tint0 = adjustments::create_color_matrix_from_tint(0xff0000ffU, 0.0);
  check(array_close(tint0, identity), "zero tint amount is identity");
  const auto contrast = adjustments::create_contrast_color_matrix(2.0);
  check(contrast.size() == 20 && contrast.element(0) == 2.0 && contrast.element(4) == -0.5,
        "contrast scales about the 0.5 pivot");
  check(array_close(adjustments::create_contrast_color_matrix(1.0), identity),
        "unit contrast is identity");
  check(array_close(adjustments::create_desaturate_color_matrix(0.0), identity),
        "zero desaturation is identity");
  check(array_close(adjustments::create_desaturate_color_matrix(1.0), gray),
        "full desaturation matches grayscale");

  const auto hue0 = adjustments::create_hue_rotate_color_matrix(0.0);
  const auto hue360 = adjustments::create_hue_rotate_color_matrix(360.0);
  check(array_close(hue0, identity) && array_close(hue360, identity, 9),
        "zero and full-turn hue rotations are identity within trig tolerance");

  const auto levels = adjustments::create_levels_color_matrix(0, 1, 0, 1, 1.0);
  check(array_close(levels, identity), "full-range unit-gamma levels are identity");
  check(adjustments::create_levels_color_matrix(128.0 / 255.0, 1, 0, 1).element(0) > 1.0,
        "levels black clipping amplifies the remaining range");
  check(adjustments::create_levels_color_matrix(0.5, 0.5, 0.2, 0.8).element(0) == 1.0,
        "zero-width levels input takes its explicit finite fallback");

  const auto opacity = adjustments::create_opacity_color_matrix(0.5);
  check(packed(adjustments::apply_color_matrix_to_color(opacity, 0xffffffffU)) == 0xffffff80U,
        "opacity scales and rounds alpha");
  check(adjustments::create_polaroid_color_matrix().size() == 20 &&
            adjustments::create_sepia_color_matrix().size() == 20 &&
            adjustments::create_technicolor_color_matrix().size() == 20 &&
            adjustments::create_vintage_color_matrix().size() == 20,
        "preset matrix factories return complete matrices");

  check(array_close(adjustments::create_saturation_color_matrix(1.0), identity),
        "unit saturation is identity");
  check(array_close(adjustments::create_saturation_color_matrix(0.0), gray),
        "zero saturation matches grayscale");
  const auto warm = adjustments::create_white_balance_color_matrix(50, 0);
  const auto cool = adjustments::create_white_balance_color_matrix(-50, 0);
  const auto green = adjustments::create_white_balance_color_matrix(0, 100);
  check(array_close(adjustments::create_white_balance_color_matrix(0, 0), identity) &&
            warm.element(0) > 1 && warm.element(12) < 1 && cool.element(0) < 1 &&
            cool.element(12) > 1 && green.element(6) > 1,
        "white balance normalizes neutral, temperature, and tint parameters");

  const auto product = adjustments::multiply_color_matrix(identity, gray);
  check(array_close(product, gray), "matrix multiplication preserves the other operand under identity");
  flight::Array<double> out(20.0);
  const auto returned = adjustments::multiply_color_matrix(identity, identity, out);
  check(returned.identity() == out.identity() && array_close(out, identity),
        "matrix multiplication writes and returns the supplied output");
  auto alias_a = gray.slice();
  adjustments::multiply_color_matrix(alias_a, identity, alias_a);
  check(array_close(alias_a, gray), "matrix multiplication is alias-safe when output is a");
  auto alias_b = gray.slice();
  adjustments::multiply_color_matrix(identity, alias_b, alias_b);
  check(array_close(alias_b, gray), "matrix multiplication is alias-safe when output is b");
  auto concat = gray.slice();
  adjustments::concat_color_matrix(concat, identity);
  check(array_close(concat, gray), "concat is alias-safe and mutates its target");

  check(array_close(adjustments::fuse_color_matrices({}), identity),
        "empty fusion returns identity");
  auto single = adjustments::fuse_color_matrices(flight::Array<flight::Array<double>>{invert});
  check(array_close(single, invert) && single.identity() != invert.identity(),
        "single-matrix fusion returns an independent copy");
  const auto sepia = adjustments::create_sepia_color_matrix();
  const auto expected = adjustments::multiply_color_matrix(
      contrast, adjustments::multiply_color_matrix(gray, invert));
  const auto fused = adjustments::fuse_color_matrices(
      flight::Array<flight::Array<double>>{invert, gray, contrast});
  check(array_close(fused, expected), "fusion composes index zero first");
  const auto brightness_then_contrast = adjustments::fuse_color_matrices(
      flight::Array<flight::Array<double>>{
          adjustments::create_brightness_color_matrix(0.2),
          adjustments::create_contrast_color_matrix(2.0)});
  check(close(brightness_then_contrast.element(4), -0.1, 10),
        "fusion order applies brightness before contrast, including translated bias");
  const auto forward = adjustments::fuse_color_matrices(
      flight::Array<flight::Array<double>>{invert, sepia});
  const auto reverse = adjustments::fuse_color_matrices(
      flight::Array<flight::Array<double>>{sepia, invert});
  check(packed(adjustments::apply_color_matrix_to_color(forward, 0xff0000ffU)) !=
            packed(adjustments::apply_color_matrix_to_color(reverse, 0xff0000ffU)),
        "matrix composition order is observably noncommutative");

  const auto unclamped = adjustments::create_brightness_color_matrix(2.0);
  check(packed(adjustments::apply_color_matrix_to_color(unclamped, 0xffffffffU)) == 0xffffffffU &&
            packed(adjustments::apply_color_matrix_to_color(invert, 0xffffffffU)) == 0x000000ffU,
        "packed application clamps high and low channels to SDR bytes");
}

void descriptor_factories() {
  auto brightness = adjustments::create_brightness_contrast_adjustment();
  check(brightness->kind == flight::String("BrightnessContrastAdjustment") &&
            packed(adjustments::apply_color_matrix_to_color(brightness->color_matrix,
                                                            0x3366aaffU)) == 0x3366aaffU,
        "brightness/contrast defaults to identity and carries its kind");
  auto brightness_options = flight::make_ref<types::BrightnessContrastAdjustment>(
      types::BrightnessContrastAdjustment{});
  brightness_options->brightness = 0.15;
  brightness_options->contrast = 0.35;
  auto shader = adjustments::create_brightness_contrast_adjustment(brightness_options);
  check(packed(adjustments::apply_color_matrix_to_color(shader->color_matrix, 0xffffffffU)) ==
            0xbababaffU,
        "brightness/contrast reproduces the upstream shader equation");

  auto mixer_options = flight::make_ref<types::ChannelMixerAdjustment>(
      types::ChannelMixerAdjustment{});
  mixer_options->matrix = flight::Array<double>{0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0};
  auto channel = adjustments::create_channel_mixer_adjustment(mixer_options);
  check(channel->kind == flight::String("ChannelMixerAdjustment") &&
            packed(adjustments::apply_color_matrix_to_color(channel->color_matrix,
                                                            0xaabbccffU)) == 0xccaabbffU,
        "channel-mixer descriptor rotates rows in source order");
  auto short_mixer = flight::make_ref<types::ChannelMixerAdjustment>(
      types::ChannelMixerAdjustment{});
  short_mixer->matrix = flight::Array<double>{1, 0, 0};
  auto normalized = adjustments::create_channel_mixer_adjustment(short_mixer);
  check(packed(adjustments::apply_color_matrix_to_color(normalized->color_matrix,
                                                        0x123456ffU)) == 0x123456ffU,
        "short channel-mixer input fills missing cells from the identity defaults");

  auto blind = adjustments::create_color_blind_simulation_adjustment();
  check(blind->type == flight::String("deuteranopia") &&
            packed(adjustments::apply_color_matrix_to_color(blind->color_matrix,
                                                            0xff0000ffU)) == 0x9fb300ffU,
        "color-blind simulation defaults to deuteranopia");
  auto blind_options = flight::make_ref<types::ColorBlindSimulationAdjustment>(
      types::ColorBlindSimulationAdjustment{});
  blind_options->type = flight::String("achromatopsia");
  auto mono = adjustments::create_color_blind_simulation_adjustment(blind_options);
  check(packed(adjustments::apply_color_matrix_to_color(mono->color_matrix,
                                                        0xff0000ffU)) == 0x4c4c4cffU,
        "achromatopsia maps red to equal-channel luma");
  bool invalid_blind_type_failed = false;
  blind_options->type = flight::String("not-a-mode");
  try {
    (void)adjustments::create_color_blind_simulation_adjustment(blind_options);
  } catch (const std::bad_optional_access&) {
    invalid_blind_type_failed = true;
  }
  check(invalid_blind_type_failed, "unknown color-blind mode fails instead of silently normalizing");

  auto exposure_options = flight::make_ref<types::ExposureAdjustment>(types::ExposureAdjustment{});
  exposure_options->exposure = 1.0;
  auto exposure = adjustments::create_exposure_adjustment(exposure_options);
  check(packed(adjustments::apply_color_matrix_to_color(exposure->color_matrix,
                                                        0x204060ffU)) == 0x4080c0ffU &&
            packed(adjustments::apply_color_matrix_to_color(exposure->color_matrix,
                                                            0x808080ffU)) == 0xffffffffU,
        "exposure uses 2^stops and clamps overflow");

  auto grayscale = adjustments::create_grayscale_adjustment();
  auto gray_pixel = packed(adjustments::apply_color_matrix_to_color(grayscale->color_matrix,
                                                                    0xff0000ffU));
  check(grayscale->kind == flight::String("GrayscaleAdjustment") &&
            ((gray_pixel >> 24U) & 0xffU) == 54U &&
            ((gray_pixel >> 16U) & 0xffU) == 54U && ((gray_pixel >> 8U) & 0xffU) == 54U,
        "grayscale descriptor uses BT.709 luma");
  auto zero_gray = flight::make_ref<types::GrayscaleAdjustment>(types::GrayscaleAdjustment{});
  zero_gray->intensity = 0.0;
  check(packed(adjustments::apply_color_matrix_to_color(
            adjustments::create_grayscale_adjustment(zero_gray)->color_matrix,
            0x2040a0ffU)) == 0x2040a0ffU,
        "zero grayscale intensity is identity");

  auto inverted = adjustments::create_invert_adjustment();
  check(packed(adjustments::apply_color_matrix_to_color(inverted->color_matrix,
                                                        0x808080ffU)) == 0x7f7f7fffU,
        "full invert preserves the upstream midpoint rounding");
  auto half_invert = flight::make_ref<types::InvertAdjustment>(types::InvertAdjustment{});
  half_invert->intensity = 0.5;
  check(packed(adjustments::apply_color_matrix_to_color(
            adjustments::create_invert_adjustment(half_invert)->color_matrix,
            0x00ffccffU)) == 0x808080ffU,
        "half invert collapses RGB to rounded midpoint");

  auto sepia = adjustments::create_sepia_adjustment();
  check(((packed(adjustments::apply_color_matrix_to_color(sepia->color_matrix,
                                                          0xff0000ffU)) >> 24U) & 0xffU) == 100U,
        "full sepia preserves the upstream red-row reference");
  auto zero_sepia = flight::make_ref<types::SepiaAdjustment>(types::SepiaAdjustment{});
  zero_sepia->intensity = 0.0;
  check(packed(adjustments::apply_color_matrix_to_color(
            adjustments::create_sepia_adjustment(zero_sepia)->color_matrix,
            0x3366ccffU)) == 0x3366ccffU,
        "zero sepia intensity is identity");

  auto tint = adjustments::create_tint_adjustment(0x808080ffU);
  check(tint->kind == flight::String("TintAdjustment") && tint->color_matrix.size() == 20 &&
            packed(adjustments::apply_color_matrix_to_color(tint->color_matrix,
                                                            0xffffffffU)) == 0x808080ffU &&
            packed(adjustments::apply_color_matrix_to_color(tint->color_matrix,
                                                            0x808080ffU)) == 0x404040ffU,
        "tint extracts normalized RGBA scales and applies them diagonally");
}

void pointwise_transforms() {
  auto hue = adjustments::create_hue_saturation_adjustment();
  auto hue_out = transformed(hue->transform, 0.2, 0.4, 0.6);
  check(hue->kind == flight::String("HueSaturationAdjustment") &&
            close(hue_out.element(0), 0.2, 4) && close(hue_out.element(1), 0.4, 4) &&
            close(hue_out.element(2), 0.6, 4),
        "hue/saturation defaults to identity");
  auto hue_options = flight::make_ref<types::HueSaturationAdjustment>(
      types::HueSaturationAdjustment{});
  hue_options->hue = 120.0;
  auto rotated = transformed(adjustments::create_hue_saturation_adjustment(hue_options)->transform,
                             1, 0, 0);
  check(close(rotated.element(0), 0, 4) && close(rotated.element(1), 1, 4) &&
            close(rotated.element(2), 0, 4),
        "120-degree hue rotation maps red toward green");
  hue_options->hue = std::nullopt;
  hue_options->saturation = 0.0;
  auto desaturated = transformed(
      adjustments::create_hue_saturation_adjustment(hue_options)->transform, 1, 0, 0);
  check(close(desaturated.element(0), 0.5, 4) &&
            close(desaturated.element(0), desaturated.element(1), 4) &&
            close(desaturated.element(1), desaturated.element(2), 4),
        "zero HSL saturation produces lightness grey");
  hue_options->saturation = 2.0;
  hue_options->lightness = 2.0;
  const auto hue_clamped = transformed(
      adjustments::create_hue_saturation_adjustment(hue_options)->transform, 0.2, 0.4, 0.6);
  check(hue_clamped.element(0) >= 0 && hue_clamped.element(0) <= 1 &&
            hue_clamped.element(1) >= 0 && hue_clamped.element(1) <= 1 &&
            hue_clamped.element(2) >= 0 && hue_clamped.element(2) <= 1,
        "hue/saturation clamps normalized saturation and lightness");

  auto grade = adjustments::create_color_grade_adjustment();
  auto neutral = transformed(grade->transform, 0.3, 0.5, 0.7);
  check(close(neutral.element(0), 0.3, 2) && close(neutral.element(1), 0.5, 2) &&
            close(neutral.element(2), 0.7, 2),
        "color grade defaults to the source's packed near-neutral response");
  auto grade_options = flight::make_ref<types::ColorGradeAdjustment>(
      types::ColorGradeAdjustment{});
  grade_options->contrast = 2.0;
  auto grade_contrast = transformed(
      adjustments::create_color_grade_adjustment(grade_options)->transform, 0.75, 0.75, 0.75);
  check(close(grade_contrast.element(0), 1.0, 4),
        "color grade contrast pivots about mid-grey");
  grade_options->contrast = std::nullopt;
  grade_options->saturation = 0.0;
  auto grade_gray = transformed(
      adjustments::create_color_grade_adjustment(grade_options)->transform, 1, 0, 0);
  check(close(grade_gray.element(0), 0.2126, 2) && close(grade_gray.element(1), 0.2126, 2) &&
            close(grade_gray.element(2), 0.2126, 2),
        "color grade desaturates toward Rec.709 luma");

  auto lgg = adjustments::create_lift_gamma_gain_adjustment();
  auto lgg_neutral = transformed(lgg->transform, 0.3, 0.6, 0.9);
  check(close(lgg_neutral.element(0), 0.3, 2) && close(lgg_neutral.element(1), 0.6, 2) &&
            close(lgg_neutral.element(2), 0.9, 2),
        "lift/gamma/gain default is packed near-neutral");
  auto lgg_options = flight::make_ref<types::LiftGammaGainAdjustment>(
      types::LiftGammaGainAdjustment{});
  lgg_options->gain = 0x808080ffU;
  auto gained = transformed(
      adjustments::create_lift_gamma_gain_adjustment(lgg_options)->transform, 1, 1, 1);
  check(close(gained.element(0), 128.0 / 255.0, 2),
        "lift/gamma/gain decodes packed per-channel gain");
  lgg_options->gain = std::nullopt;
  lgg_options->lift = 0xffffffffU;
  auto lifted = transformed(
      adjustments::create_lift_gamma_gain_adjustment(lgg_options)->transform, 0, 0, 0);
  check(close(lifted.element(0), 1, 4), "lift pulls black toward the packed lift color");
}

void matrix_descriptors_and_resolution() {
  const auto identity = adjustments::create_identity_color_matrix();
  auto authored = adjustments::create_color_matrix_adjustment(identity);
  check(authored->kind == flight::String("ColorMatrixAdjustment") &&
            array_close(authored->color_matrix, identity) &&
            authored->color_matrix.identity() != identity.identity(),
        "authored matrix descriptor owns an independent matrix copy");
  auto matrix = adjustments::get_adjustment_color_matrix(adjustment_view(authored));
  check(matrix.has_value() && matrix->identity() == authored->color_matrix.identity() &&
            adjustments::is_color_matrix_adjustment(adjustment_view(authored)),
        "matrix classification returns the descriptor's exact array identity");

  bool short_matrix_failed = false;
  try {
    (void)adjustments::create_color_matrix_adjustment(flight::Array<double>{1, 2, 3});
  } catch (const flight::Error&) {
    short_matrix_failed = true;
  }
  check(short_matrix_failed, "authored matrix rejects lengths other than twenty");

  auto plain = flight::make_ref<types::Adjustment>(types::Adjustment{.kind = flight::String("Blur")});
  check(!adjustments::get_adjustment_color_matrix(adjustment_view(plain)).has_value() &&
            !adjustments::is_color_matrix_adjustment(adjustment_view(plain)),
        "plain non-matrix adjustment has no matrix evidence");

  flight::Array<flight::Ref<types::Adjustment>> stack;
  stack.push(authored);
  const auto resolved = adjustments::resolve_color_adjustments_color_matrix(stack);
  check(resolved.has_value() && array_close(*resolved, identity),
        "matrix resolution returns the fully fused matrix");
  check(!adjustments::resolve_color_adjustments_color_matrix(std::nullopt).has_value() &&
            !adjustments::resolve_color_adjustments_color_matrix(
                flight::Array<flight::Ref<types::Adjustment>>{}).has_value(),
        "null and empty matrix stacks resolve to absence");
  flight::Array<flight::Ref<types::Adjustment>> plain_stack{plain};
  check(!adjustments::resolve_color_adjustments_color_matrix(plain_stack).has_value(),
        "a non-matrix operation makes complete matrix resolution fail");

  auto bias = flight::make_ref<types::ColorScaleBias>(types::ColorScaleBias{
      .alpha_scale = 1, .alpha_bias = 0, .blue_scale = 1, .blue_bias = 0,
      .green_scale = 1, .green_bias = 0, .red_scale = 1, .red_bias = 0});
  check(adjustments::resolve_color_adjustments_color_scale_bias(std::nullopt, bias) ==
            adjustments::color_adjustment_none,
        "null scale/bias stack resolves to NONE");
  const auto affine_matrix = flight::Array<double>{
      0.5, 0, 0, 0, 0.4, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0};
  auto affine = adjustments::create_color_matrix_adjustment(affine_matrix);
  flight::Array<flight::Ref<types::Adjustment>> affine_stack{affine};
  check(adjustments::resolve_color_adjustments_color_scale_bias(affine_stack, bias) ==
            adjustments::color_adjustment_affine &&
            bias->red_scale == 0.5 && bias->green_scale == 0 && bias->red_bias == 0.4 &&
            bias->alpha_scale == 1,
        "affine resolution extracts exact diagonal scales and biases");
  auto half = adjustments::create_color_matrix_adjustment(flight::Array<double>{
      0.5, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0});
  flight::Array<flight::Ref<types::Adjustment>> halves{half, half};
  adjustments::resolve_color_adjustments_color_scale_bias(halves, bias);
  check(bias->red_scale == 0.25, "affine resolution composes multipliers in order");
  auto saturation = adjustments::create_color_matrix_adjustment(
      adjustments::create_saturation_color_matrix(0));
  flight::Array<flight::Ref<types::Adjustment>> mixing{saturation};
  check(!adjustments::is_affine_color_matrix(saturation->color_matrix) &&
            adjustments::resolve_color_adjustments_color_scale_bias(mixing, bias) ==
                adjustments::color_adjustment_channel_mixing &&
            close(bias->red_scale, 0.299),
        "channel-mixing resolution reports its tier and writes only the affine projection");
  check(adjustments::resolve_color_adjustments_color_scale_bias(plain_stack, bias) ==
            adjustments::color_adjustment_channel_mixing,
        "a non-matrix operation prevents affine resolution");

  auto payload = flight::make_ref<types::ColorScaleBias>(types::ColorScaleBias{
      .alpha_scale = 1, .alpha_bias = 0, .blue_scale = 1, .blue_bias = 0,
      .green_scale = 1, .green_bias = 0.25, .red_scale = 0.5, .red_bias = 0});
  auto bridge = adjustments::create_color_scale_bias_adjustment(readonly(payload));
  check(bridge->kind == flight::String("ColorScaleBiasAdjustment") &&
            bridge->color_matrix.element(0) == 0.5 && bridge->color_matrix.element(9) == 0.25 &&
            flight::row_get<flight::RowKey<"redScale">>(bridge->color_scale_bias) == 0.5,
        "scale/bias bridge preserves a readable payload and diagonal matrix");
}

void lut_and_cache() {
  types::ColorTransformFunction invert = [](flight::Array<double> out, double r, double g, double b) {
    out.element(0) = 1 - r;
    out.element(1) = 1 - g;
    out.element(2) = 1 - b;
  };
  types::ColorTransformFunction brighten = [](flight::Array<double> out, double r, double g, double b) {
    out.element(0) = r + 0.5;
    out.element(1) = g + 0.5;
    out.element(2) = b + 0.5;
  };
  const auto identity_lut = adjustments::bake_color_lut({}, 8);
  flight::Array<double> sampled{0, 0, 0};
  adjustments::sample_color_lut(readonly(identity_lut), sampled, 0.25, 0.5, 0.75);
  check(identity_lut->size == 8 && identity_lut->samples.size() == 8 * 8 * 8 * 3 &&
            close(sampled.element(0), 0.25) && close(sampled.element(1), 0.5) &&
            close(sampled.element(2), 0.75),
        "empty transform stack bakes an identity cube");
  const auto normalized = adjustments::bake_color_lut({}, 1.2);
  const auto floored = adjustments::bake_color_lut({}, 3.9);
  check(normalized->size == 2 && floored->size == 3 &&
            adjustments::bake_color_lut({})->size == adjustments::color_lut_default_size,
        "LUT size floors finite input, enforces two cells, and defaults to standard resolution");

  const auto composed = adjustments::bake_color_lut(
      flight::Array<types::ColorTransformFunction>{invert, brighten}, 16);
  adjustments::sample_color_lut(readonly(composed), sampled, 0.2, 0.2, 0.2);
  check(close(sampled.element(0), 1), "LUT transforms run left-to-right and clamp baked samples");
  types::ColorTransformFunction ramp = [](flight::Array<double> out, double r, double, double) {
    out.element(0) = r;
    out.element(1) = r;
    out.element(2) = r;
  };
  const auto ramp_lut = adjustments::bake_color_lut(
      flight::Array<types::ColorTransformFunction>{ramp}, 2);
  adjustments::sample_color_lut(readonly(ramp_lut), sampled, 0.5, 0, 0);
  check(close(sampled.element(0), 0.5), "LUT samples interpolate trilinearly between cells");
  adjustments::sample_color_lut(readonly(identity_lut), sampled, 2, -1, 0.5);
  check(close(sampled.element(0), 1) && close(sampled.element(1), 0) &&
            close(sampled.element(2), 0.5),
        "LUT sampling clamps coordinates to the cube boundary");

  auto hue = adjustments::create_hue_saturation_adjustment();
  auto hue_transform = adjustments::get_adjustment_color_transform(adjustment_view(hue));
  check(hue_transform.has_value() && adjustments::is_color_lut_adjustment(adjustment_view(hue)),
        "LUT-tier descriptor exposes its pointwise transform");
  auto matrix = adjustments::create_invert_adjustment();
  auto matrix_transform = adjustments::get_adjustment_color_transform(adjustment_view(matrix));
  flight::Array<double> cyan{0, 0, 0};
  if (matrix_transform.has_value()) cyan = transformed(*matrix_transform, 1, 0, 0);
  check(matrix_transform.has_value() && close(cyan.element(0), 0) && close(cyan.element(1), 1) &&
            close(cyan.element(2), 1) && !adjustments::is_color_lut_adjustment(adjustment_view(matrix)),
        "matrix-tier descriptor is wrapped as an opaque-alpha pointwise transform");
  auto plain = flight::make_ref<types::Adjustment>(types::Adjustment{.kind = flight::String("Blur")});
  check(!adjustments::get_adjustment_color_transform(adjustment_view(plain)).has_value(),
        "non-pointwise operation has no transform");

  auto lookup = adjustments::create_lookup_table_grade_adjustment();
  auto passthrough = transformed(lookup->transform, 0.2, 0.4, 0.6);
  check(close(passthrough.element(0), 0.2) && close(passthrough.element(1), 0.4) &&
            close(passthrough.element(2), 0.6),
        "lookup grade without a LUT is a passthrough");
  auto lookup_options = flight::make_ref<types::LookupTableGradeAdjustment>(
      types::LookupTableGradeAdjustment{});
  lookup_options->lut = adjustments::bake_color_lut(
      flight::Array<types::ColorTransformFunction>{invert}, 16);
  auto full = transformed(
      adjustments::create_lookup_table_grade_adjustment(lookup_options)->transform, 0, 0, 0);
  check(close(full.element(0), 1, 4), "lookup grade applies a supplied LUT at full strength");
  lookup_options->strength = 0.5;
  auto mixed = transformed(
      adjustments::create_lookup_table_grade_adjustment(lookup_options)->transform, 0, 0, 0);
  check(close(mixed.element(0), 0.5, 4), "lookup grade mixes by normalized strength");

  auto cache = adjustments::create_color_lut_cache();
  check(!cache->signature.has_value() && !cache->lut.has_value(), "new LUT cache starts empty");
  flight::Array<AdjustmentView> empty_run;
  const flight::SequenceView<AdjustmentView> empty_view(empty_run);
  auto first = adjustments::bake_color_lut_for_run(cache, empty_view, 4);
  auto second = adjustments::bake_color_lut_for_run(cache, empty_view, 4);
  auto resized = adjustments::bake_color_lut_for_run(cache, empty_view, 8);
  check(first == second && cache->signature.has_value() && resized != first && resized->size == 8,
        "LUT cache reuses equal signatures and rekeys on size");
}

}  // namespace

int main() {
  matrix_math();
  descriptor_factories();
  pointwise_transforms();
  matrix_descriptors_and_resolution();
  lut_and_cache();
  if (failures != 0) {
    std::cerr << failures << " adjustment conformance check(s) failed\n";
    return 1;
  }
  std::cout << "adjustments conformance checks passed\n";
  return 0;
}
