#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include <flight/typed_array.hpp>

namespace flight {

struct AudioBufferOptions final {
  double length{};
  double number_of_channels{1.0};
  double sample_rate{};
};

// Shared decoded Float32 PCM with the AudioBuffer surface reached by Flight. Copies preserve the
// same channel arrays and object identity; get_channel_data returns another handle to the channel's
// live view instead of copying its samples.
class AudioBuffer final {
 public:
  explicit AudioBuffer(AudioBufferOptions options) : AudioBuffer(validate(options), ValidatedTag{}) {}

  double duration{};
  double length{};
  double number_of_channels{};
  double sample_rate{};

  void copy_from_channel(
      Float32Array destination,
      double channel_number,
      double start_in_channel = 0.0) const {
    const auto channel = channel_index(channel_number);
    const auto start = sample_index(start_in_channel);
    if (start >= storage_->length) return;
    const auto count = std::min(destination.size(), storage_->length - start);
    std::copy_n(
        storage_->channels[channel].begin() + static_cast<std::ptrdiff_t>(start),
        count,
        destination.begin());
  }

  void copy_to_channel(
      const Float32Array& source,
      double channel_number,
      double start_in_channel = 0.0) const {
    const auto channel = channel_index(channel_number);
    const auto start = sample_index(start_in_channel);
    if (start >= storage_->length) return;
    const auto count = std::min(source.size(), storage_->length - start);
    std::copy_n(
        source.begin(),
        count,
        storage_->channels[channel].begin() + static_cast<std::ptrdiff_t>(start));
  }

  [[nodiscard]] Float32Array get_channel_data(double channel_number) const {
    return storage_->channels[channel_index(channel_number)];
  }

  [[nodiscard]] const void* identity() const noexcept { return storage_.get(); }

  [[nodiscard]] friend bool operator==(const AudioBuffer& left, const AudioBuffer& right) noexcept {
    return left.storage_ == right.storage_;
  }

 private:
  struct ValidatedTag final {};

  struct ValidatedOptions final {
    std::size_t length;
    std::size_t number_of_channels;
    double sample_rate;
  };

  struct Storage final {
    Storage(std::size_t channel_count, std::size_t sample_count)
        : length(sample_count) {
      channels.reserve(channel_count);
      for (std::size_t channel = 0; channel < channel_count; ++channel) {
        channels.emplace_back(sample_count);
      }
    }

    std::size_t length;
    std::vector<Float32Array> channels;
  };

  AudioBuffer(ValidatedOptions options, ValidatedTag)
      : duration(static_cast<double>(options.length) / options.sample_rate),
        length(static_cast<double>(options.length)),
        number_of_channels(static_cast<double>(options.number_of_channels)),
        sample_rate(options.sample_rate),
        storage_(std::make_shared<Storage>(options.number_of_channels, options.length)) {}

  [[nodiscard]] std::size_t channel_index(double value) const {
    if (!std::isfinite(value) || value < 0.0 || std::trunc(value) != value ||
        value >= static_cast<double>(storage_->channels.size())) {
      throw std::range_error("flight::AudioBuffer channel is outside the buffer");
    }
    return static_cast<std::size_t>(value);
  }

  [[nodiscard]] std::size_t sample_index(double value) const {
    if (!std::isfinite(value) || value < 0.0 || std::trunc(value) != value ||
        value > static_cast<double>(storage_->length)) {
      throw std::range_error("flight::AudioBuffer sample offset is outside the buffer");
    }
    return static_cast<std::size_t>(value);
  }

  [[nodiscard]] static ValidatedOptions validate(AudioBufferOptions options) {
    constexpr double maximum_safe_integer = 9'007'199'254'740'991.0;
    if (!std::isfinite(options.length) || options.length <= 0.0 ||
        options.length > maximum_safe_integer || std::trunc(options.length) != options.length ||
        static_cast<long double>(options.length) >
            static_cast<long double>(std::numeric_limits<std::size_t>::max())) {
      throw std::range_error("flight::AudioBuffer length is outside the supported range");
    }
    if (!std::isfinite(options.number_of_channels) || options.number_of_channels < 1.0 ||
        options.number_of_channels > 32.0 ||
        std::trunc(options.number_of_channels) != options.number_of_channels) {
      throw std::range_error("flight::AudioBuffer channel count is outside the supported range");
    }
    if (!std::isfinite(options.sample_rate) || options.sample_rate < 8'000.0 ||
        options.sample_rate > 96'000.0) {
      throw std::range_error("flight::AudioBuffer sample rate is outside the supported range");
    }
    return {
        static_cast<std::size_t>(options.length),
        static_cast<std::size_t>(options.number_of_channels),
        options.sample_rate,
    };
  }

  std::shared_ptr<Storage> storage_;
};

} // namespace flight
