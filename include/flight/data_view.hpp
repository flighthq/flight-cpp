#pragma once

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>

#include <flight/array_buffer.hpp>

namespace flight {

class DataView {
 public:
  ArrayBufferLike buffer;
  std::size_t byte_offset = 0;
  std::size_t byte_length = 0;

  [[nodiscard]] const void* identity() const noexcept { return identity_.get(); }

  explicit DataView(const ArrayBufferLike& source)
      : buffer(source), byte_length(source.byte_length()) {}

  template <typename Offset>
    requires std::is_arithmetic_v<Offset>
  DataView(const ArrayBufferLike& source, Offset offset)
      : DataView(source,
                 detail::buffer_index(static_cast<double>(offset),
                                      "flight::DataView byte offset is outside the buffer"),
                 RemainingTag{}) {}

  template <typename Offset, typename Length>
    requires(std::is_arithmetic_v<Offset> && std::is_arithmetic_v<Length>)
  DataView(const ArrayBufferLike& source, Offset offset, Length length)
      : DataView(source,
                 detail::buffer_index(static_cast<double>(offset),
                                      "flight::DataView byte offset is outside the buffer"),
                 detail::buffer_index(static_cast<double>(length),
                                      "flight::DataView byte length is outside the supported range"),
                 ViewTag{}) {}

  [[nodiscard]] double get_float32(double offset, bool little_endian = false) const {
    return static_cast<double>(std::bit_cast<float>(read_unsigned<std::uint32_t>(offset, little_endian)));
  }

  [[nodiscard]] double get_float64(double offset, bool little_endian = false) const {
    return std::bit_cast<double>(read_unsigned<std::uint64_t>(offset, little_endian));
  }

  [[nodiscard]] double get_int8(double offset) const {
    return static_cast<double>(std::bit_cast<std::int8_t>(read_unsigned<std::uint8_t>(offset, false)));
  }

  [[nodiscard]] double get_int16(double offset, bool little_endian = false) const {
    return static_cast<double>(std::bit_cast<std::int16_t>(read_unsigned<std::uint16_t>(offset, little_endian)));
  }

  [[nodiscard]] double get_int32(double offset, bool little_endian = false) const {
    return static_cast<double>(std::bit_cast<std::int32_t>(read_unsigned<std::uint32_t>(offset, little_endian)));
  }

  [[nodiscard]] double get_uint8(double offset) const {
    return static_cast<double>(read_unsigned<std::uint8_t>(offset, false));
  }

  [[nodiscard]] double get_uint16(double offset, bool little_endian = false) const {
    return static_cast<double>(read_unsigned<std::uint16_t>(offset, little_endian));
  }

  [[nodiscard]] double get_uint32(double offset, bool little_endian = false) const {
    return static_cast<double>(read_unsigned<std::uint32_t>(offset, little_endian));
  }

  void set_float32(double offset, double value, bool little_endian = false) const {
    write_unsigned(offset, std::bit_cast<std::uint32_t>(static_cast<float>(value)), little_endian);
  }

  void set_float64(double offset, double value, bool little_endian = false) const {
    write_unsigned(offset, std::bit_cast<std::uint64_t>(value), little_endian);
  }

  void set_int8(double offset, double value) const {
    write_unsigned(offset, integer_bits<std::uint8_t>(value), false);
  }

  void set_int16(double offset, double value, bool little_endian = false) const {
    write_unsigned(offset, integer_bits<std::uint16_t>(value), little_endian);
  }

  void set_int32(double offset, double value, bool little_endian = false) const {
    write_unsigned(offset, integer_bits<std::uint32_t>(value), little_endian);
  }

  void set_uint8(double offset, double value) const {
    write_unsigned(offset, integer_bits<std::uint8_t>(value), false);
  }

  void set_uint16(double offset, double value, bool little_endian = false) const {
    write_unsigned(offset, integer_bits<std::uint16_t>(value), little_endian);
  }

  void set_uint32(double offset, double value, bool little_endian = false) const {
    write_unsigned(offset, integer_bits<std::uint32_t>(value), little_endian);
  }

 private:
  struct Identity {};
  struct RemainingTag {};
  struct ViewTag {};

  DataView(const ArrayBufferLike& source, std::size_t offset, RemainingTag)
      : DataView(source,
                 offset,
                 offset <= source.byte_length() ? source.byte_length() - offset : 0,
                 ViewTag{}) {
    if (offset > source.byte_length()) {
      throw std::range_error("flight::DataView byte offset is outside the buffer");
    }
  }

  DataView(const ArrayBufferLike& source, std::size_t offset, std::size_t length, ViewTag)
      : buffer(source), byte_offset(offset), byte_length(length) {
    if (offset > source.byte_length() || length > source.byte_length() - offset) {
      throw std::range_error("flight::DataView view exceeds its buffer");
    }
  }

  template <typename Unsigned>
    requires std::is_unsigned_v<Unsigned>
  [[nodiscard]] Unsigned read_unsigned(double offset, bool little_endian) const {
    const auto index = checked_access<Unsigned>(offset);
    Unsigned result = 0;
    const auto* bytes = buffer.data() + byte_offset + index;
    if (little_endian) {
      for (std::size_t position = 0; position < sizeof(Unsigned); ++position) {
        const auto byte = static_cast<Unsigned>(std::to_integer<std::uint8_t>(bytes[position]));
        const auto shifted = static_cast<Unsigned>(byte << (position * 8));
        result = static_cast<Unsigned>(result | shifted);
      }
    } else {
      for (std::size_t position = 0; position < sizeof(Unsigned); ++position) {
        result = static_cast<Unsigned>((result << 8) | std::to_integer<std::uint8_t>(bytes[position]));
      }
    }
    return result;
  }

  template <typename Unsigned>
    requires std::is_unsigned_v<Unsigned>
  void write_unsigned(double offset, Unsigned value, bool little_endian) const {
    const auto index = checked_access<Unsigned>(offset);
    auto* bytes = const_cast<ArrayBufferLike&>(buffer).writable_data() + byte_offset + index;
    for (std::size_t position = 0; position < sizeof(Unsigned); ++position) {
      const auto shift = little_endian ? position * 8 : (sizeof(Unsigned) - position - 1) * 8;
      bytes[position] = static_cast<std::byte>((value >> shift) & static_cast<Unsigned>(0xFF));
    }
  }

  template <typename Value>
  [[nodiscard]] std::size_t checked_access(double offset) const {
    const auto index = detail::buffer_index(offset, "flight::DataView offset is outside the view");
    if (index > byte_length || sizeof(Value) > byte_length - index) {
      throw std::range_error("flight::DataView access exceeds the view");
    }
    return index;
  }

  template <typename Unsigned>
    requires std::is_unsigned_v<Unsigned>
  [[nodiscard]] static Unsigned integer_bits(double value) noexcept {
    if (!std::isfinite(value) || value == 0.0) return 0;
    const double modulus = std::ldexp(1.0, static_cast<int>(sizeof(Unsigned) * 8));
    double result = std::fmod(std::trunc(value), modulus);
    if (result < 0.0) result += modulus;
    return static_cast<Unsigned>(result);
  }

  std::shared_ptr<Identity> identity_ = std::make_shared<Identity>();
};

} // namespace flight
