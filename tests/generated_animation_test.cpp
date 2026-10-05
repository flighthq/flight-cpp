#include <flight/animation/animation_track.hpp>

#include <cmath>
#include <iostream>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

bool close(double actual, double expected, double tolerance = 1e-5) {
  return std::abs(actual - expected) <= tolerance;
}

flight::Ref<flight::types::AnimationTrack> linear_track(
    flight::SequenceView<double> times,
    flight::SequenceView<double> values) {
  auto track = flight::make_ref<flight::types::AnimationTrack>();
  track->interpolation = flight::String("Linear");
  track->times = std::move(times);
  track->values = std::move(values);
  track->components = 1.0;
  track->quaternion = false;
  track->easing = std::nullopt;
  track->segment_easings = std::nullopt;
  return track;
}

} // namespace

int main() {
  auto track = linear_track(
      flight::SequenceView<double>(flight::Array<double>{0.0, 1.0, 2.0}),
      flight::SequenceView<double>(flight::Float32Array{0.0f, 10.0f, 20.0f}));
  track->segment_easings =
      flight::Array<std::optional<flight::Ref<flight::types::EasingFunction>>>{
          std::function<double(double)>([](double value) { return value * value; }),
          std::nullopt,
      };
  track->easing = std::function<double(double)>([](double) { return 0.25; });

  flight::Array<double> sampled{0.0};
  flight::animation::sample_animation_track(
      std::variant<flight::Array<double>, flight::Float32Array>{sampled}, track, 0.5);
  if (!check(sampled.element(0.0) == 2.5,
             "segment easing did not reshape linear animation sampling")) {
    return 1;
  }
  flight::animation::sample_animation_track(
      std::variant<flight::Array<double>, flight::Float32Array>{sampled}, track, 1.5);
  if (!check(sampled.element(0.0) == 12.5,
             "missing segment easing did not fall back to the track easing")) {
    return 1;
  }

  flight::Float32Array typed_sample(1.0);
  track->interpolation = flight::String("Step");
  flight::animation::sample_animation_track(
      std::variant<flight::Array<double>, flight::Float32Array>{typed_sample}, track, 0.9);
  if (!check(typed_sample.get_index(0.0) == 0.0,
             "step animation did not hold the previous keyframe in a typed output")) {
    return 1;
  }
  track->interpolation = flight::String("Linear");

  auto cubic = linear_track(
      flight::SequenceView<double>(flight::Array<double>{0.0, 1.0}),
      flight::SequenceView<double>(flight::Array<double>{0.0, 0.0, 0.0, 0.0, 10.0, 0.0}));
  cubic->interpolation = flight::String("Cubic");
  flight::Array<double> cubic_sample{0.0};
  flight::animation::sample_animation_track(
      std::variant<flight::Array<double>, flight::Float32Array>{cubic_sample}, cubic, 0.5);
  if (!check(cubic_sample.element(0.0) == 5.0,
             "cubic animation did not apply Hermite interpolation")) {
    return 1;
  }

  const double sine = std::sin(std::acos(-1.0) / 4.0);
  const double cosine = std::cos(std::acos(-1.0) / 4.0);
  auto quaternion = linear_track(
      flight::SequenceView<double>(flight::Array<double>{0.0, 1.0}),
      flight::SequenceView<double>(
          flight::Array<double>{0.0, 0.0, 0.0, 1.0, 0.0, 0.0, sine, cosine}));
  quaternion->components = 4.0;
  quaternion->quaternion = true;
  flight::Array<double> quaternion_sample{0.0, 0.0, 0.0, 0.0};
  flight::animation::sample_animation_track(
      std::variant<flight::Array<double>, flight::Float32Array>{quaternion_sample},
      quaternion,
      0.5);
  if (!check(close(quaternion_sample.element(2.0), std::sin(std::acos(-1.0) / 8.0)) &&
                 close(quaternion_sample.element(3.0), std::cos(std::acos(-1.0) / 8.0)),
             "quaternion animation did not slerp over the shortest arc")) {
    return 1;
  }

  const auto clone = flight::animation::clone_animation_track(track);
  if (!check(clone->values.is_float32_array_backed(),
             "animation clone did not preserve Float32Array backing") ||
      !check(clone->values.identity() != track->values.identity() &&
                 clone->times.identity() != track->times.identity(),
             "animation clone retained a mutable number buffer") ||
      !check(clone->segment_easings.has_value() &&
                 clone->segment_easings->identity() != track->segment_easings->identity(),
             "animation clone retained its segment-easing array")) {
    return 1;
  }

  const auto trimmed = flight::animation::trim_animation_track(track, 0.5, 2.0);
  if (!check(trimmed->times.size() == 2 && trimmed->times[0] == 0.5 &&
                 trimmed->times[1] == 1.5,
             "animation trim did not retain and rebase in-range keyframes") ||
      !check(trimmed->values.size() == 2 && trimmed->values[0] == 10.0 &&
                 trimmed->values[1] == 20.0,
             "animation trim did not copy matching keyframe values") ||
      !check(trimmed->segment_easings.has_value() &&
                 trimmed->segment_easings->size() == 1,
             "animation trim did not slice segment easings to the retained range")) {
    return 1;
  }

  const auto valid = flight::animation::validate_animation_track(track);
  if (!check(!valid.has_value(), "a valid animation track produced diagnostics")) return 1;

  const auto invalid = linear_track(
      flight::SequenceView<double>(flight::Array<double>{1.0, 0.0}),
      flight::SequenceView<double>(flight::Array<double>{4.0}));
  invalid->segment_easings =
      flight::Array<std::optional<flight::Ref<flight::types::EasingFunction>>>{};
  const auto diagnostics = flight::animation::validate_animation_track(invalid);
  if (!check(diagnostics.has_value() && diagnostics->size() == 3,
             "invalid animation fields did not produce every diagnostic") ||
      !check(diagnostics->element(0.0)->code == flight::String("nonAscendingTimes") &&
                 diagnostics->element(1.0)->code == flight::String("valuesLengthMismatch") &&
                 diagnostics->element(2.0)->code ==
                     flight::String("segmentEasingsLengthMismatch"),
             "animation diagnostics did not retain source order and codes")) {
    return 1;
  }

  return 0;
}
