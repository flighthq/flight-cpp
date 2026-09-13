#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <flight/data_view.hpp>
#include <flight/typed_array.hpp>

namespace flight {

enum class ArrayBufferViewKind {
  data_view,
  float32_array,
  float64_array,
  int8_array,
  int16_array,
  int32_array,
  uint8_array,
  uint8_clamped_array,
  uint16_array,
  uint32_array,
};

namespace detail {

template <typename Value>
struct array_buffer_view_kind;

template <>
struct array_buffer_view_kind<float> {
  static constexpr auto value = ArrayBufferViewKind::float32_array;
};

template <>
struct array_buffer_view_kind<double> {
  static constexpr auto value = ArrayBufferViewKind::float64_array;
};

template <>
struct array_buffer_view_kind<std::int8_t> {
  static constexpr auto value = ArrayBufferViewKind::int8_array;
};

template <>
struct array_buffer_view_kind<std::int16_t> {
  static constexpr auto value = ArrayBufferViewKind::int16_array;
};

template <>
struct array_buffer_view_kind<std::int32_t> {
  static constexpr auto value = ArrayBufferViewKind::int32_array;
};

template <>
struct array_buffer_view_kind<std::uint8_t> {
  static constexpr auto value = ArrayBufferViewKind::uint8_array;
};

template <>
struct array_buffer_view_kind<Uint8Clamped> {
  static constexpr auto value = ArrayBufferViewKind::uint8_clamped_array;
};

template <>
struct array_buffer_view_kind<std::uint16_t> {
  static constexpr auto value = ArrayBufferViewKind::uint16_array;
};

template <>
struct array_buffer_view_kind<std::uint32_t> {
  static constexpr auto value = ArrayBufferViewKind::uint32_array;
};

} // namespace detail

class ArrayBufferView {
 public:
  ArrayBufferView(const DataView& source)
      : buffer(source.buffer),
        byte_offset(source.byte_offset),
        byte_length(source.byte_length),
        kind(ArrayBufferViewKind::data_view),
        identity_(source.identity()) {}

  template <typename Value>
  ArrayBufferView(const TypedArray<Value>& source)
      : buffer(source.buffer),
        byte_offset(source.byte_offset),
        byte_length(source.byte_length),
        kind(detail::array_buffer_view_kind<Value>::value),
        identity_(source.identity()) {}

  [[nodiscard]] const std::byte* data() const noexcept { return buffer.data() + byte_offset; }
  [[nodiscard]] std::byte* data() noexcept { return buffer.data() + byte_offset; }
  [[nodiscard]] const void* identity() const noexcept { return identity_; }
  [[nodiscard]] bool same_backing(const ArrayBufferView& other) const noexcept {
    return buffer == other.buffer;
  }

  ArrayBuffer buffer;
  std::size_t byte_offset;
  std::size_t byte_length;
  ArrayBufferViewKind kind;

 private:
  const void* identity_;
};

} // namespace flight
