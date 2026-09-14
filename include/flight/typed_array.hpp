#pragma once

#include <algorithm>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <flight/array_buffer.hpp>

namespace flight {

class Uint8Clamped {
 public:
  constexpr Uint8Clamped() noexcept = default;

  template <typename Number>
    requires std::is_arithmetic_v<Number>
  Uint8Clamped(Number value) noexcept : value_(clamp(static_cast<double>(value))) {}

  [[nodiscard]] constexpr operator std::uint8_t() const noexcept { return value_; }
  [[nodiscard]] constexpr auto operator<=>(const Uint8Clamped&) const noexcept = default;

  template <typename Number>
    requires std::is_arithmetic_v<Number>
  [[nodiscard]] friend constexpr bool operator==(const Uint8Clamped& left, Number right) noexcept {
    return static_cast<double>(left.value_) == static_cast<double>(right);
  }

  template <typename Number>
    requires std::is_arithmetic_v<Number>
  [[nodiscard]] friend constexpr bool operator==(Number left, const Uint8Clamped& right) noexcept {
    return right == left;
  }

  template <typename Number>
    requires std::is_arithmetic_v<Number>
  [[nodiscard]] friend constexpr bool operator<(const Uint8Clamped& left, Number right) noexcept {
    return static_cast<double>(left.value_) < static_cast<double>(right);
  }

  template <typename Number>
    requires std::is_arithmetic_v<Number>
  [[nodiscard]] friend constexpr bool operator<(Number left, const Uint8Clamped& right) noexcept {
    return static_cast<double>(left) < static_cast<double>(right.value_);
  }

 private:
  [[nodiscard]] static std::uint8_t clamp(double value) noexcept {
    if (std::isnan(value) || value <= 0.0) return 0;
    if (value >= 255.0) return 255;
    const auto lower = std::floor(value);
    const auto fraction = value - lower;
    if (fraction > 0.5 || (fraction == 0.5 && std::fmod(lower, 2.0) != 0.0)) {
      return static_cast<std::uint8_t>(lower + 1.0);
    }
    return static_cast<std::uint8_t>(lower);
  }

  std::uint8_t value_ = 0;
};

template <typename Value>
  requires(std::is_arithmetic_v<Value> || std::same_as<Value, Uint8Clamped>)
class TypedArray {
  static_assert(std::is_trivially_copyable_v<Value>);
  static_assert(alignof(Value) <= alignof(std::max_align_t));

  struct Identity {};
  struct ViewTag {};

 public:
  using const_iterator = const Value*;
  using iterator = Value*;
  using size_type = std::size_t;
  using value_type = Value;

  static constexpr double bytes_per_element = static_cast<double>(sizeof(Value));

  class Length {
   public:
    explicit Length(const TypedArray* owner) : owner_(owner) {}
    [[nodiscard]] operator size_type() const noexcept { return owner_->size(); }
    [[nodiscard]] size_type operator()() const noexcept { return owner_->size(); }

   private:
    const TypedArray* owner_;
  };

  ArrayBufferLike buffer;
  size_type byte_offset = 0;
  size_type byte_length = 0;
  Length length{this};

  TypedArray() = default;

  explicit TypedArray(size_type length)
      : buffer(ArrayBuffer::allocate(required_bytes(length))),
        byte_length(required_bytes(length)),
        length_(length) {}

  template <std::floating_point Number>
  explicit TypedArray(Number length) : TypedArray(array_length(static_cast<double>(length))) {}

  TypedArray(std::initializer_list<Value> values) : TypedArray(values.begin(), values.end()) {}

  TypedArray(const TypedArray& other)
      : buffer(other.buffer),
        byte_offset(other.byte_offset),
        byte_length(other.byte_length),
        length(this),
        length_(other.length_),
        identity_(other.identity_) {}

  TypedArray(TypedArray&& other) noexcept
      : buffer(std::move(other.buffer)),
        byte_offset(other.byte_offset),
        byte_length(other.byte_length),
        length(this),
        length_(other.length_),
        identity_(std::move(other.identity_)) {}

  TypedArray& operator=(const TypedArray& other) {
    buffer = other.buffer;
    byte_offset = other.byte_offset;
    byte_length = other.byte_length;
    length_ = other.length_;
    identity_ = other.identity_;
    return *this;
  }

  TypedArray& operator=(TypedArray&& other) noexcept {
    buffer = std::move(other.buffer);
    byte_offset = other.byte_offset;
    byte_length = other.byte_length;
    length_ = other.length_;
    identity_ = std::move(other.identity_);
    return *this;
  }

  template <typename Range>
    requires requires(const Range& range) {
      std::begin(range);
      std::end(range);
    }
  explicit TypedArray(const Range& values) : TypedArray(std::begin(values), std::end(values)) {}

  template <typename Range>
    requires requires(const Range& range) {
      std::begin(range);
      std::end(range);
    }
  [[nodiscard]] static TypedArray from(const Range& values) {
    return TypedArray(std::begin(values), std::end(values));
  }

  template <typename Range, typename Transform>
    requires requires(const Range& range) {
      std::begin(range);
      std::end(range);
    }
  [[nodiscard]] static TypedArray from(const Range& values, Transform transform) {
    std::vector<Value> output;
    if constexpr (requires { std::size(values); }) output.reserve(std::size(values));
    size_type index = 0;
    for (const auto& value : values) {
      if constexpr (std::invocable<Transform&, decltype(value), double>) {
        output.push_back(convert_element(
            std::invoke(transform, value, static_cast<double>(index))));
      } else {
        output.push_back(convert_element(std::invoke(transform, value)));
      }
      ++index;
    }
    return TypedArray(output.begin(), output.end());
  }

  explicit TypedArray(const ArrayBufferLike& source)
      : TypedArray(source, size_type{0}, remaining_length(source, 0), ViewTag{}) {}

  TypedArray(const ArrayBufferLike& source, double offset)
      : TypedArray(source,
                   detail::buffer_index(offset, "flight::TypedArray byte offset is outside the buffer"),
                   ViewTag{}) {}

  TypedArray(const ArrayBufferLike& source, double offset, double length)
      : TypedArray(source,
                   detail::buffer_index(offset, "flight::TypedArray byte offset is outside the buffer"),
                   array_length(length),
                   ViewTag{}) {}

  [[nodiscard]] Value& operator[](size_type index) const { return mutable_data()[index]; }

  [[nodiscard]] std::optional<Value> at(std::ptrdiff_t index) const {
    const auto normalized = normalize_element_index(index);
    if (!normalized) return std::nullopt;
    return (*this)[*normalized];
  }

  [[nodiscard]] Value& element(double index) const { return (*this)[required_index(index)]; }

  [[nodiscard]] std::optional<Value> get(double index) const {
    const auto normalized = property_index(index);
    return normalized ? std::optional<Value>((*this)[*normalized]) : std::nullopt;
  }

  [[nodiscard]] const_iterator begin() const noexcept { return data(); }
  [[nodiscard]] iterator begin() { return data(); }
  [[nodiscard]] const_iterator end() const noexcept { return data() + length_; }
  [[nodiscard]] iterator end() { return data() + length_; }
  [[nodiscard]] bool empty() const noexcept { return length_ == 0; }

  [[nodiscard]] friend bool operator==(const TypedArray& left, const TypedArray& right) noexcept {
    return left.identity_ == right.identity_;
  }

  [[nodiscard]] const void* identity() const noexcept { return identity_.get(); }

  TypedArray& fill(Value value) const {
    std::fill(mutable_data(), mutable_data() + length_, std::move(value));
    return const_cast<TypedArray&>(*this);
  }

  [[nodiscard]] size_type size() const noexcept { return length_; }

  template <typename Range>
    requires requires(const Range& range) {
      std::begin(range);
      std::end(range);
    }
  void set(const Range& source, std::ptrdiff_t offset = 0) const {
    if (offset < 0 || static_cast<size_type>(offset) > length_) {
      throw std::range_error("flight::TypedArray set offset is outside the view");
    }
    std::vector<Value> snapshot;
    for (const auto& value : source) snapshot.emplace_back(value);
    const auto target = static_cast<size_type>(offset);
    if (snapshot.size() > length_ - target) {
      throw std::range_error("flight::TypedArray source does not fit the view");
    }
    std::move(snapshot.begin(), snapshot.end(), mutable_data() + static_cast<std::ptrdiff_t>(target));
  }

  [[nodiscard]] std::span<Value> span() const { return {mutable_data(), length_}; }

  [[nodiscard]] TypedArray slice(
      std::ptrdiff_t begin_index,
      std::ptrdiff_t end_index = std::numeric_limits<std::ptrdiff_t>::max()) const {
    const auto first = normalize_boundary(begin_index);
    const auto last = normalize_boundary(end_index);
    if (last <= first) return TypedArray();
    return TypedArray(begin() + static_cast<std::ptrdiff_t>(first),
                      begin() + static_cast<std::ptrdiff_t>(last));
  }

  [[nodiscard]] TypedArray subarray(
      std::ptrdiff_t begin_index,
      std::ptrdiff_t end_index = std::numeric_limits<std::ptrdiff_t>::max()) const {
    const auto first = normalize_boundary(begin_index);
    const auto last = normalize_boundary(end_index);
    return TypedArray(buffer, byte_offset + first * sizeof(Value), last <= first ? 0 : last - first, ViewTag{});
  }

 private:
  [[nodiscard]] static size_type array_length(double length) {
    return detail::buffer_index(length, "flight::TypedArray length is outside the supported range");
  }

  [[nodiscard]] static size_type required_bytes(size_type length) {
    if (length > std::numeric_limits<size_type>::max() / sizeof(Value)) {
      throw std::length_error("flight::TypedArray length exceeds addressable storage");
    }
    return length * sizeof(Value);
  }

  static void validate_offset(const ArrayBufferLike& source, size_type offset) {
    if (offset % sizeof(Value) != 0 || offset > source.byte_length()) {
      throw std::range_error("flight::TypedArray byte offset is invalid for the buffer");
    }
    if (reinterpret_cast<std::uintptr_t>(source.data() + offset) % alignof(Value) != 0) {
      throw std::range_error("flight::TypedArray backing storage is not aligned for its element type");
    }
  }

  [[nodiscard]] static size_type remaining_length(const ArrayBufferLike& source, size_type offset) {
    validate_offset(source, offset);
    const auto remaining = source.byte_length() - offset;
    if (remaining % sizeof(Value) != 0) {
      throw std::range_error("flight::TypedArray buffer length is not aligned to its element size");
    }
    return remaining / sizeof(Value);
  }

  [[nodiscard]] std::optional<size_type> property_index(double index) const noexcept {
    if (!std::isfinite(index) || index < 0.0 || std::trunc(index) != index) return std::nullopt;
    if (index >= static_cast<double>(length_)) return std::nullopt;
    return static_cast<size_type>(index);
  }

  [[nodiscard]] size_type required_index(double index) const {
    const auto normalized = property_index(index);
    if (!normalized) throw std::range_error("flight::TypedArray index is outside the view");
    return *normalized;
  }

  template <typename Iterator>
  TypedArray(Iterator first, Iterator last)
      : TypedArray(static_cast<size_type>(std::distance(first, last))) {
    auto destination = begin();
    while (first != last) {
      *destination = convert_element(*first);
      ++first;
      ++destination;
    }
  }

  template <typename Source>
  [[nodiscard]] static Value convert_element(Source&& source) {
    using SourceValue = std::remove_cvref_t<Source>;
    if constexpr (std::same_as<SourceValue, Value>) {
      return source;
    } else if constexpr (std::same_as<Value, Uint8Clamped> &&
                         std::is_arithmetic_v<SourceValue>) {
      return Uint8Clamped(source);
    } else if constexpr (std::floating_point<Value> &&
                         std::is_arithmetic_v<SourceValue>) {
      return static_cast<Value>(source);
    } else if constexpr (std::integral<Value> &&
                         std::is_arithmetic_v<SourceValue>) {
      const auto number = static_cast<long double>(source);
      if (!std::isfinite(number) || number == 0.0L) return Value{0};

      using Unsigned = std::make_unsigned_t<Value>;
      constexpr auto bits = std::numeric_limits<Unsigned>::digits;
      const auto modulus = std::ldexp(1.0L, bits);
      auto wrapped = std::fmod(std::trunc(number), modulus);
      if (wrapped < 0.0L) wrapped += modulus;
      if constexpr (std::unsigned_integral<Value>) {
        return static_cast<Value>(wrapped);
      } else {
        const auto midpoint = modulus / 2.0L;
        return static_cast<Value>(wrapped >= midpoint ? wrapped - modulus : wrapped);
      }
    } else {
      return Value(std::forward<Source>(source));
    }
  }

  TypedArray(const ArrayBufferLike& source, size_type offset, ViewTag)
      : TypedArray(source, offset, remaining_length(source, offset), ViewTag{}) {}

  TypedArray(const ArrayBufferLike& source, size_type offset, size_type length, ViewTag)
      : buffer(source), byte_offset(offset), byte_length(required_bytes(length)), length_(length) {
    if (!source.is_writable()) {
      throw std::invalid_argument("flight::TypedArray requires writable backing storage");
    }
    validate_offset(source, offset);
    if (byte_length > source.byte_length() - offset) {
      throw std::range_error("flight::TypedArray view exceeds its buffer");
    }
  }

  [[nodiscard]] const Value* data() const noexcept {
    return reinterpret_cast<const Value*>(buffer.data() + byte_offset);
  }

  [[nodiscard]] Value* data() { return reinterpret_cast<Value*>(buffer.writable_data() + byte_offset); }

  [[nodiscard]] Value* mutable_data() const {
    return reinterpret_cast<Value*>(
        const_cast<ArrayBufferLike&>(buffer).writable_data() + byte_offset);
  }

  [[nodiscard]] size_type normalize_boundary(std::ptrdiff_t index) const noexcept {
    if (index < 0) {
      const auto magnitude = static_cast<size_type>(-(index + 1)) + 1;
      return magnitude >= length_ ? 0 : length_ - magnitude;
    }
    const auto positive = static_cast<size_type>(index);
    return std::min(positive, length_);
  }

  [[nodiscard]] std::optional<size_type> normalize_element_index(std::ptrdiff_t index) const noexcept {
    if (index < 0) {
      const auto magnitude = static_cast<size_type>(-(index + 1)) + 1;
      if (magnitude > length_) return std::nullopt;
      return length_ - magnitude;
    }
    const auto positive = static_cast<size_type>(index);
    return positive < length_ ? std::optional<size_type>(positive) : std::nullopt;
  }

  size_type length_ = 0;
  std::shared_ptr<Identity> identity_ = std::make_shared<Identity>();
};

using Float32Array = TypedArray<float>;
using Float64Array = TypedArray<double>;
using Int8Array = TypedArray<std::int8_t>;
using Int16Array = TypedArray<std::int16_t>;
using Int32Array = TypedArray<std::int32_t>;
using Uint8Array = TypedArray<std::uint8_t>;
using Uint8ClampedArray = TypedArray<Uint8Clamped>;
using Uint16Array = TypedArray<std::uint16_t>;
using Uint32Array = TypedArray<std::uint32_t>;

} // namespace flight
