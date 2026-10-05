// Derived from @flighthq/animation/packages/animation/src/animationTrack.ts.
#pragma once
#include <cmath>
#include <cstdint>
#include <flight/sequence_view.hpp>
#include <optional>
#include <random>
#include <variant>
#include <flight/runtime.hpp>

static_assert(flight::runtime_contract.compiler_contract == "flight-runtime-contract/2", "Flight compiler/runtime contract mismatch");
static_assert(flight::runtime_contract.cpp_abi == 1, "Flight C++ runtime ABI mismatch");

#include <flight/types/entity.hpp>

#include <flight/types/easing_function.hpp>

#include <flight/types/animation_track.hpp>

#include <flight/types/animation_track_validation_diagnostic.hpp>

#include <flight/types/animation_interpolation.hpp>

#include <flight/entity/entity.hpp>

namespace flight::animation {

using flight::types::AnimationInterpolation;
using flight::types::AnimationTrack;
using flight::types::AnimationTrackValidationDiagnostic;
using flight::types::EasingFunction;
using flight::types::EntityConstruction;

using ReadonlyAnimationTrack = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<AnimationTrack>>>>;

using flight::entity::allocate_entity;
using flight::entity::finish_entity;

#ifndef FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_ANIMATION_TIMES_VALUES_COMPONENTS_INTERPOLATION_QUATERNION_EASING_SEGMENT_EASINGS_9B0EADFED5E06042
#define FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_ANIMATION_TIMES_VALUES_COMPONENTS_INTERPOLATION_QUATERNION_EASING_SEGMENT_EASINGS_9B0EADFED5E06042
struct times_values_components_interpolation_quaternion_easing_segment_easings_9b0eadfed5e06042 : public flight::ReferenceEnabled {
  flight::SequenceView<double> times;
  flight::SequenceView<double> values;
  std::optional<double> components;
  std::optional<flight::Ref<AnimationInterpolation>> interpolation;
  std::optional<bool> quaternion;
  std::variant<flight::Ref<EasingFunction>, flight::Null, flight::Undefined> easing = std::variant<flight::Ref<EasingFunction>, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>, flight::undefined};
  std::variant<flight::Array<std::optional<flight::Ref<EasingFunction>>>, flight::Null, flight::Undefined> segment_easings = std::variant<flight::Array<std::optional<flight::Ref<EasingFunction>>>, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>, flight::undefined};
};
#endif // FLIGHT_COMPILER_ANONYMOUS__FLIGHTHQ_ANIMATION_TIMES_VALUES_COMPONENTS_INTERPOLATION_QUATERNION_EASING_SEGMENT_EASINGS_9B0EADFED5E06042

inline void initialize_animation_track(flight::Ref<EntityConstruction<flight::Ref<AnimationTrack>>> out, flight::Ref<times_values_components_interpolation_quaternion_easing_segment_easings_9b0eadfed5e06042> opts) {
  const double components = ([&]() -> double { auto nullish_coalesce_left = opts->components; if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return 1.0; }());
  std::optional<flight::Ref<EasingFunction>> easing = ([&]() -> std::optional<flight::Ref<EasingFunction>> { auto nullish_coalesce_left = ([&]() -> std::variant<flight::Ref<EasingFunction>, flight::Null, flight::Undefined> { auto optional_property = opts->easing; if (std::holds_alternative<flight::Undefined>(optional_property)) return std::variant<flight::Ref<EasingFunction>, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>, flight::undefined}; if (std::holds_alternative<flight::Null>(optional_property)) return std::variant<flight::Ref<EasingFunction>, flight::Null, flight::Undefined>{std::in_place_type<flight::Null>, flight::null}; return std::variant<flight::Ref<EasingFunction>, flight::Null, flight::Undefined>{std::in_place_type<flight::Ref<EasingFunction>>, std::get<flight::Ref<EasingFunction>>(optional_property)}; }()); if (const auto* alternative = std::get_if<flight::Ref<EasingFunction>>(&nullish_coalesce_left)) return std::optional<flight::Ref<EasingFunction>>{*alternative}; return std::nullopt; }());
  flight::Ref<AnimationInterpolation> interpolation = ([&]() -> flight::Ref<AnimationInterpolation> { auto nullish_coalesce_left = opts->interpolation; if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return flight::String("Linear"); }());
  const bool quaternion = ([&]() -> bool { auto nullish_coalesce_left = opts->quaternion; if (nullish_coalesce_left.has_value()) return nullish_coalesce_left.value(); return false; }());
  std::optional<flight::Array<std::optional<flight::Ref<EasingFunction>>>> segment_easings = ([&]() -> std::optional<flight::Array<std::optional<flight::Ref<EasingFunction>>>> { auto nullish_coalesce_left_2 = ([&]() -> std::variant<flight::Array<std::optional<flight::Ref<EasingFunction>>>, flight::Null, flight::Undefined> { auto optional_property_2 = opts->segment_easings; if (std::holds_alternative<flight::Undefined>(optional_property_2)) return std::variant<flight::Array<std::optional<flight::Ref<EasingFunction>>>, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>, flight::undefined}; if (std::holds_alternative<flight::Null>(optional_property_2)) return std::variant<flight::Array<std::optional<flight::Ref<EasingFunction>>>, flight::Null, flight::Undefined>{std::in_place_type<flight::Null>, flight::null}; return std::variant<flight::Array<std::optional<flight::Ref<EasingFunction>>>, flight::Null, flight::Undefined>{std::in_place_type<flight::Array<std::optional<flight::Ref<EasingFunction>>>>, std::get<flight::Array<std::optional<flight::Ref<EasingFunction>>>>(optional_property_2)}; }()); if (const auto* alternative = std::get_if<flight::Array<std::optional<flight::Ref<EasingFunction>>>>(&nullish_coalesce_left_2)) return std::optional<flight::Array<std::optional<flight::Ref<EasingFunction>>>>{*alternative}; return std::nullopt; }());
  (out->components = components);
  (out->easing = easing);
  (out->interpolation = interpolation);
  (out->quaternion = quaternion);
  (out->segment_easings = segment_easings);
  (out->times = opts->times);
  (out->values = opts->values);
}

inline flight::Ref<AnimationTrack> create_animation_track(flight::Ref<times_values_components_interpolation_quaternion_easing_segment_easings_9b0eadfed5e06042> opts) {
  flight::Ref<EntityConstruction<flight::Ref<AnimationTrack>>> out = allocate_entity<flight::Ref<AnimationTrack>>();
  initialize_animation_track(out, opts);
  return finish_entity(out);
}


inline flight::SequenceView<double> clone_number_buffer(
    const flight::SequenceView<double>& source) {
  if (source.is_float32_array_backed()) {
    flight::Float32Array out(static_cast<double>(source.size()));
    for (std::size_t index = 0; index < source.size(); ++index) {
      out.set_index(static_cast<double>(index), source[index]);
    }
    return flight::SequenceView<double>(std::move(out));
  }
  flight::Array<double> out;
  for (const double value : source) out.push(value);
  return flight::SequenceView<double>(std::move(out));
}

inline flight::Ref<AnimationTrack> clone_animation_track(
    flight::Ref<AnimationTrack> track) {
  auto out = allocate_entity<flight::Ref<AnimationTrack>>();
  out->components = track->components;
  out->easing = track->easing;
  out->interpolation = track->interpolation;
  out->quaternion = track->quaternion;
  out->segment_easings = track->segment_easings.has_value()
                             ? std::optional(track->segment_easings->slice())
                             : std::nullopt;
  out->times = clone_number_buffer(track->times);
  out->values = clone_number_buffer(track->values);
  return finish_entity(out);
}

inline flight::Ref<AnimationTrack> clone_animation_track(ReadonlyAnimationTrack track) {
  return clone_animation_track(track.shared_object());
}

inline double keyframe_stride(flight::Ref<AnimationTrack> track) {
  return ((track->interpolation == flight::String("Cubic")) ? (track->components * 3.0) : track->components);
}


inline flight::Ref<AnimationTrack> trim_animation_track(
    flight::Ref<AnimationTrack> track,
    double start_time,
    double end_time) {
  const auto count = track->times.size();
  const double stride = keyframe_stride(track);
  flight::Array<double> out_times;
  flight::Array<double> out_values;
  flight::Array<double> source_keyframes;
  for (std::size_t keyframe = 0; keyframe < count; ++keyframe) {
    const double time = track->times[keyframe];
    if (time < start_time || time > end_time) continue;
    out_times.push(time - start_time);
    source_keyframes.push(static_cast<double>(keyframe));
    const auto offset = static_cast<std::size_t>(static_cast<double>(keyframe) * stride);
    for (std::size_t component = 0;
         component < static_cast<std::size_t>(stride);
         ++component) {
      out_values.push(track->values[offset + component]);
    }
  }

  auto out = allocate_entity<flight::Ref<AnimationTrack>>();
  out->components = track->components;
  out->easing = track->easing;
  out->interpolation = track->interpolation;
  out->quaternion = track->quaternion;
  if (!track->segment_easings.has_value()) {
    out->segment_easings = std::nullopt;
  } else if (source_keyframes.size() < 2) {
    out->segment_easings = flight::Array<std::optional<flight::Ref<EasingFunction>>>{};
  } else {
    out->segment_easings = track->segment_easings->slice(
        source_keyframes.element(0.0),
        source_keyframes.element(static_cast<double>(source_keyframes.size() - 1)));
  }
  out->times = flight::SequenceView<double>(std::move(out_times));
  out->values = flight::SequenceView<double>(std::move(out_values));
  return finish_entity(out);
}

inline flight::Ref<AnimationTrack> trim_animation_track(
    ReadonlyAnimationTrack track,
    double start_time,
    double end_time) {
  return trim_animation_track(track.shared_object(), start_time, end_time);
}

inline std::optional<flight::Array<flight::Ref<AnimationTrackValidationDiagnostic>>>
validate_animation_track(flight::Ref<AnimationTrack> track) {
  flight::Array<flight::Ref<AnimationTrackValidationDiagnostic>> diagnostics;
  const auto count = track->times.size();
  for (std::size_t keyframe = 1; keyframe < count; ++keyframe) {
    if (track->times[keyframe] <= track->times[keyframe - 1]) {
      const double index = static_cast<double>(keyframe);
      diagnostics.push(flight::make_ref<AnimationTrackValidationDiagnostic>(
          AnimationTrackValidationDiagnostic{
              .code = flight::String("nonAscendingTimes"),
              .index = index,
              .message = flight::String("times[") + flight::to_string(index) +
                         flight::String("] (") + flight::to_string(track->times[keyframe]) +
                         flight::String(") is not greater than times[") +
                         flight::to_string(index - 1.0) + flight::String("] (") +
                         flight::to_string(track->times[keyframe - 1]) +
                         flight::String("); times must be strictly ascending.")}));
    }
  }
  const double expected = static_cast<double>(count) * keyframe_stride(track);
  if (static_cast<double>(track->values.size()) != expected) {
    diagnostics.push(flight::make_ref<AnimationTrackValidationDiagnostic>(
        AnimationTrackValidationDiagnostic{
            .code = flight::String("valuesLengthMismatch"),
            .index = std::nullopt,
            .message = flight::String("values.length (") +
                       flight::to_string(static_cast<double>(track->values.size())) +
                       flight::String(") must equal keyCount * componentsPerKeyframe (") +
                       flight::to_string(expected) + flight::String(").")}));
  }
  const double expected_easings = std::max(0.0, static_cast<double>(count) - 1.0);
  if (track->segment_easings.has_value() &&
      static_cast<double>(track->segment_easings->size()) != expected_easings) {
    diagnostics.push(flight::make_ref<AnimationTrackValidationDiagnostic>(
        AnimationTrackValidationDiagnostic{
            .code = flight::String("segmentEasingsLengthMismatch"),
            .index = std::nullopt,
            .message = flight::String("segmentEasings.length (") +
                       flight::to_string(
                           static_cast<double>(track->segment_easings->size())) +
                       flight::String(") must equal keyCount - 1 (") +
                       flight::to_string(expected_easings) + flight::String(").")}));
  }
  if (diagnostics.empty()) return std::nullopt;
  return diagnostics;
}

inline std::optional<flight::Array<flight::Ref<AnimationTrackValidationDiagnostic>>>
validate_animation_track(ReadonlyAnimationTrack track) {
  return validate_animation_track(track.shared_object());
}

inline double keyframe_value_offset(flight::Ref<AnimationTrack> track, double k) {
  const double stride = keyframe_stride(track);
  return ((track->interpolation == flight::String("Cubic")) ? ((k * stride) + track->components) : (k * stride));
}

inline void copy_keyframe_value(std::variant<flight::Array<double>, flight::Float32Array> out, flight::Ref<AnimationTrack> track, double k) {
  const double off = keyframe_value_offset(track, k);
  {
    double c = 0.0;
    while ((c < track->components)) {
      ([&]() { auto&& indexed_source = out; const auto indexed_index = c; const auto indexed_value = track->values[(off + c)]; std::visit([&](auto& indexed_receiver) { if constexpr (requires { indexed_receiver.set_index(indexed_index, indexed_value); }) indexed_receiver.set_index(indexed_index, indexed_value); else indexed_receiver.element(indexed_index) = indexed_value; }, indexed_source); return indexed_value; }());
      (c += 1.0);
    }
  }
}

inline void normalize_flat_quaternion(std::variant<flight::Array<double>, flight::Float32Array> out) {
  const double x = ([&]() -> double { auto&& indexed_source_2 = out; const auto indexed_index_2 = 0.0; return std::visit([&](const auto& indexed_receiver_2) -> double { if constexpr (requires { indexed_receiver_2.get_index(indexed_index_2); }) return indexed_receiver_2.get_index(indexed_index_2); else return indexed_receiver_2.element(indexed_index_2); }, indexed_source_2); }());
  const double y = ([&]() -> double { auto&& indexed_source_3 = out; const auto indexed_index_3 = 1.0; return std::visit([&](const auto& indexed_receiver_3) -> double { if constexpr (requires { indexed_receiver_3.get_index(indexed_index_3); }) return indexed_receiver_3.get_index(indexed_index_3); else return indexed_receiver_3.element(indexed_index_3); }, indexed_source_3); }());
  const double z = ([&]() -> double { auto&& indexed_source_4 = out; const auto indexed_index_4 = 2.0; return std::visit([&](const auto& indexed_receiver_4) -> double { if constexpr (requires { indexed_receiver_4.get_index(indexed_index_4); }) return indexed_receiver_4.get_index(indexed_index_4); else return indexed_receiver_4.element(indexed_index_4); }, indexed_source_4); }());
  const double w = ([&]() -> double { auto&& indexed_source_5 = out; const auto indexed_index_5 = 3.0; return std::visit([&](const auto& indexed_receiver_5) -> double { if constexpr (requires { indexed_receiver_5.get_index(indexed_index_5); }) return indexed_receiver_5.get_index(indexed_index_5); else return indexed_receiver_5.element(indexed_index_5); }, indexed_source_5); }());
  const double len = std::hypot(std::hypot(x, y, z), w);
  if ((len > 0.0)) {
    const double inv = (1.0 / len);
    ([&]() { auto&& indexed_source_6 = out; const auto indexed_index_6 = 0.0; const auto indexed_value_2 = (x * inv); std::visit([&](auto& indexed_receiver_6) { if constexpr (requires { indexed_receiver_6.set_index(indexed_index_6, indexed_value_2); }) indexed_receiver_6.set_index(indexed_index_6, indexed_value_2); else indexed_receiver_6.element(indexed_index_6) = indexed_value_2; }, indexed_source_6); return indexed_value_2; }());
    ([&]() { auto&& indexed_source_7 = out; const auto indexed_index_7 = 1.0; const auto indexed_value_3 = (y * inv); std::visit([&](auto& indexed_receiver_7) { if constexpr (requires { indexed_receiver_7.set_index(indexed_index_7, indexed_value_3); }) indexed_receiver_7.set_index(indexed_index_7, indexed_value_3); else indexed_receiver_7.element(indexed_index_7) = indexed_value_3; }, indexed_source_7); return indexed_value_3; }());
    ([&]() { auto&& indexed_source_8 = out; const auto indexed_index_8 = 2.0; const auto indexed_value_4 = (z * inv); std::visit([&](auto& indexed_receiver_8) { if constexpr (requires { indexed_receiver_8.set_index(indexed_index_8, indexed_value_4); }) indexed_receiver_8.set_index(indexed_index_8, indexed_value_4); else indexed_receiver_8.element(indexed_index_8) = indexed_value_4; }, indexed_source_8); return indexed_value_4; }());
    ([&]() { auto&& indexed_source_9 = out; const auto indexed_index_9 = 3.0; const auto indexed_value_5 = (w * inv); std::visit([&](auto& indexed_receiver_9) { if constexpr (requires { indexed_receiver_9.set_index(indexed_index_9, indexed_value_5); }) indexed_receiver_9.set_index(indexed_index_9, indexed_value_5); else indexed_receiver_9.element(indexed_index_9) = indexed_value_5; }, indexed_source_9); return indexed_value_5; }());
  }
}

inline void sample_cubic_segment(std::variant<flight::Array<double>, flight::Float32Array> out, flight::Ref<AnimationTrack> track, double i, double alpha, double dt) {
  flight::Ref<AnimationTrack> object_pattern_value = track;
  const double components = object_pattern_value->components;
  flight::SequenceView<double> values = object_pattern_value->values;
  const double stride = (components * 3.0);
  const double a2 = (alpha * alpha);
  const double a3 = (a2 * alpha);
  const double h00 = (((2.0 * a3) - (3.0 * a2)) + 1.0);
  const double h10 = ((a3 - (2.0 * a2)) + alpha);
  const double h01 = ((-2.0 * a3) + (3.0 * a2));
  const double h11 = (a3 - a2);
  const double base0 = (i * stride);
  const double base1 = ((i + 1.0) * stride);
  {
    double c = 0.0;
    while ((c < components)) {
      {
        const double p0 = values[((base0 + components) + c)];
        const double m0 = values[((base0 + (components * 2.0)) + c)];
        const double p1 = values[((base1 + components) + c)];
        const double m1 = values[(base1 + c)];
        ([&]() { auto&& indexed_source_10 = out; const auto indexed_index_10 = c; const auto indexed_value_6 = ((((h00 * p0) + ((h10 * dt) * m0)) + (h01 * p1)) + ((h11 * dt) * m1)); std::visit([&](auto& indexed_receiver_10) { if constexpr (requires { indexed_receiver_10.set_index(indexed_index_10, indexed_value_6); }) indexed_receiver_10.set_index(indexed_index_10, indexed_value_6); else indexed_receiver_10.element(indexed_index_10) = indexed_value_6; }, indexed_source_10); return indexed_value_6; }());
      }
      (c += 1.0);
    }
  }
  if ((track->quaternion && (components == 4.0))) {
    normalize_flat_quaternion(out);
  }
}

inline void slerp_flat_quaternion(std::variant<flight::Array<double>, flight::Float32Array> out, flight::SequenceView<double> values, double oa, double ob, double alpha) {
  const double ax = values[oa];
  const double ay = values[(oa + 1.0)];
  const double az = values[(oa + 2.0)];
  const double aw = values[(oa + 3.0)];
  double bx = values[ob];
  double by = values[(ob + 1.0)];
  double bz = values[(ob + 2.0)];
  double bw = values[(ob + 3.0)];
  double cosom = ((((ax * bx) + (ay * by)) + (az * bz)) + (aw * bw));
  if ((cosom < 0.0)) {
    (cosom = -cosom);
    (bx = -bx);
    (by = -by);
    (bz = -bz);
    (bw = -bw);
  }
  double scale0;
  double scale1;
  if (((1.0 - cosom) > 0.000001)) {
    const double omega = std::acos(cosom);
    const double sinom = std::sin(omega);
    (scale0 = (std::sin(((1.0 - alpha) * omega)) / sinom));
    (scale1 = (std::sin((alpha * omega)) / sinom));
  }
  else {
    (scale0 = (1.0 - alpha));
    (scale1 = alpha);
  }
  ([&]() { auto&& indexed_source_11 = out; const auto indexed_index_11 = 0.0; const auto indexed_value_7 = ((scale0 * ax) + (scale1 * bx)); std::visit([&](auto& indexed_receiver_11) { if constexpr (requires { indexed_receiver_11.set_index(indexed_index_11, indexed_value_7); }) indexed_receiver_11.set_index(indexed_index_11, indexed_value_7); else indexed_receiver_11.element(indexed_index_11) = indexed_value_7; }, indexed_source_11); return indexed_value_7; }());
  ([&]() { auto&& indexed_source_12 = out; const auto indexed_index_12 = 1.0; const auto indexed_value_8 = ((scale0 * ay) + (scale1 * by)); std::visit([&](auto& indexed_receiver_12) { if constexpr (requires { indexed_receiver_12.set_index(indexed_index_12, indexed_value_8); }) indexed_receiver_12.set_index(indexed_index_12, indexed_value_8); else indexed_receiver_12.element(indexed_index_12) = indexed_value_8; }, indexed_source_12); return indexed_value_8; }());
  ([&]() { auto&& indexed_source_13 = out; const auto indexed_index_13 = 2.0; const auto indexed_value_9 = ((scale0 * az) + (scale1 * bz)); std::visit([&](auto& indexed_receiver_13) { if constexpr (requires { indexed_receiver_13.set_index(indexed_index_13, indexed_value_9); }) indexed_receiver_13.set_index(indexed_index_13, indexed_value_9); else indexed_receiver_13.element(indexed_index_13) = indexed_value_9; }, indexed_source_13); return indexed_value_9; }());
  ([&]() { auto&& indexed_source_14 = out; const auto indexed_index_14 = 3.0; const auto indexed_value_10 = ((scale0 * aw) + (scale1 * bw)); std::visit([&](auto& indexed_receiver_14) { if constexpr (requires { indexed_receiver_14.set_index(indexed_index_14, indexed_value_10); }) indexed_receiver_14.set_index(indexed_index_14, indexed_value_10); else indexed_receiver_14.element(indexed_index_14) = indexed_value_10; }, indexed_source_14); return indexed_value_10; }());
}


inline void sample_animation_track(
    std::variant<flight::Array<double>, flight::Float32Array> out,
    flight::Ref<AnimationTrack> track,
    double time) {
  const auto count = track->times.size();
  if (count == 0) {
    for (std::size_t component = 0;
         component < static_cast<std::size_t>(track->components);
         ++component) {
      std::visit(
          [&](auto& selected) {
            if constexpr (requires { selected.set_index(0.0, 0.0); }) {
              selected.set_index(static_cast<double>(component), 0.0);
            } else {
              selected.element(static_cast<double>(component)) = 0.0;
            }
          },
          out);
    }
    return;
  }
  if (count == 1 || time <= track->times[0]) {
    copy_keyframe_value(out, track, 0.0);
    return;
  }
  if (time >= track->times[count - 1]) {
    copy_keyframe_value(out, track, static_cast<double>(count - 1));
    return;
  }

  std::size_t low = 0;
  std::size_t high = count - 1;
  while (low < high) {
    const auto middle = (low + high + 1) >> 1;
    if (track->times[middle] <= time) low = middle;
    else high = middle - 1;
  }
  const double keyframe = static_cast<double>(low);
  const double start = track->times[low];
  const double duration = track->times[low + 1] - start;
  double alpha = duration > 0.0 ? (time - start) / duration : 0.0;

  std::optional<flight::Ref<EasingFunction>> easing;
  if (track->segment_easings.has_value()) {
    const auto segment = track->segment_easings->get(keyframe);
    if (segment.has_value() && segment->has_value()) easing = segment->value();
  }
  if (!easing.has_value()) easing = track->easing;
  if (easing.has_value()) alpha = easing.value()(alpha);

  if (track->interpolation == flight::String("Step")) {
    copy_keyframe_value(out, track, keyframe);
    return;
  }
  if (track->interpolation == flight::String("Cubic")) {
    sample_cubic_segment(out, track, keyframe, alpha, duration);
    return;
  }

  const double left_offset = keyframe_value_offset(track, keyframe);
  const double right_offset = keyframe_value_offset(track, keyframe + 1.0);
  if (track->quaternion && track->components == 4.0) {
    slerp_flat_quaternion(out, track->values, left_offset, right_offset, alpha);
    return;
  }
  for (std::size_t component = 0;
       component < static_cast<std::size_t>(track->components);
       ++component) {
    const double component_offset = static_cast<double>(component);
    const double left = track->values.element(left_offset + component_offset);
    const double value = left +
                         (track->values.element(right_offset + component_offset) - left) * alpha;
    std::visit(
        [&](auto& selected) {
          if constexpr (requires { selected.set_index(0.0, 0.0); }) {
            selected.set_index(component_offset, value);
          } else {
            selected.element(component_offset) = value;
          }
        },
        out);
  }
}

inline void sample_animation_track(
    std::variant<flight::Array<double>, flight::Float32Array> out,
    ReadonlyAnimationTrack track,
    double time) {
  sample_animation_track(std::move(out), track.shared_object(), time);
}

} // namespace flight::animation
