#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <variant>

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
  [[nodiscard]] std::byte* data() { return buffer.writable_data() + byte_offset; }
  [[nodiscard]] constexpr std::size_t bytes_per_element() const noexcept {
    switch (kind) {
      case ArrayBufferViewKind::float64_array: return 8;
      case ArrayBufferViewKind::float32_array:
      case ArrayBufferViewKind::int32_array:
      case ArrayBufferViewKind::uint32_array: return 4;
      case ArrayBufferViewKind::int16_array:
      case ArrayBufferViewKind::uint16_array: return 2;
      case ArrayBufferViewKind::data_view:
      case ArrayBufferViewKind::int8_array:
      case ArrayBufferViewKind::uint8_array:
      case ArrayBufferViewKind::uint8_clamped_array: return 1;
    }
    return 1;
  }
  [[nodiscard]] const void* identity() const noexcept { return identity_; }
  [[nodiscard]] bool same_backing(const ArrayBufferView& other) const noexcept {
    return buffer == other.buffer;
  }

  ArrayBufferLike buffer;
  std::size_t byte_offset;
  std::size_t byte_length;
  ArrayBufferViewKind kind;

 private:
  const void* identity_;
};

[[nodiscard]] constexpr bool is_array_buffer_view(const ArrayBufferView&) noexcept {
  return true;
}

[[nodiscard]] constexpr bool is_array_buffer_view(const DataView&) noexcept {
  return true;
}

template <typename Value>
[[nodiscard]] constexpr bool is_array_buffer_view(const TypedArray<Value>&) noexcept {
  return true;
}

[[nodiscard]] constexpr bool is_array_buffer_view(const ArrayBufferLike&) noexcept {
  return false;
}

template <typename... Values>
  requires requires(const std::variant<Values...>& value) {
    std::visit([](const auto& alternative) { return is_array_buffer_view(alternative); }, value);
  }
[[nodiscard]] bool is_array_buffer_view(const std::variant<Values...>& value) {
  return std::visit(
      [](const auto& alternative) { return is_array_buffer_view(alternative); }, value);
}

// `ArrayBuffer.isView(value)`: whether the value is a VIEW over a buffer rather than a buffer.
//
// The distinction is the whole point of the call and the SDK uses it to decide how to reach the
// bytes: a view carries an offset and a length into someone else's storage, a buffer IS the
// storage. Answering it structurally -- does this type present as a view -- rather than by a
// runtime tag keeps it correct for every typed array and for `DataView` without this header
// having to enumerate them.
template <typename Value>
[[nodiscard]] constexpr bool array_buffer_is_view(const Value&) noexcept {
  // A view carries its buffer, offset and length as DATA MEMBERS -- `TypedArray` and `DataView`
  // both do. `ArrayBuffer` has neither a `buffer` member nor a `byte_offset`; its `byte_length` is
  // a method. So plain member access separates the two exactly, and adding a third view type gets
  // the right answer without touching this.
  return requires(const Value& value) {
    value.buffer;
    value.byte_offset;
    value.byte_length;
  };
}

} // namespace flight
