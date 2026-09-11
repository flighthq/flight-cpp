#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>

namespace flight {

namespace detail {

inline std::size_t buffer_index(double value, const char* subject) {
  constexpr double maximum_safe_integer = 9007199254740991.0;
  if (!std::isfinite(value) || value < 0.0 || value > maximum_safe_integer ||
      static_cast<long double>(value) > static_cast<long double>(std::numeric_limits<std::size_t>::max())) {
    throw std::range_error(subject);
  }
  return static_cast<std::size_t>(std::trunc(value));
}

} // namespace detail

class ArrayBuffer {
  struct Storage {
    explicit Storage(std::size_t length)
        : data(static_cast<std::byte*>(
              ::operator new(std::max(length, std::size_t{1}), std::align_val_t{alignof(std::max_align_t)}))),
          length(length) {
      std::memset(data, 0, length);
    }

    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;

    ~Storage() { ::operator delete(data, std::align_val_t{alignof(std::max_align_t)}); }

    std::byte* data;
    std::size_t length;
  };

 public:
  using size_type = std::size_t;

  ArrayBuffer() : storage_(std::make_shared<Storage>(0)) {}

  explicit ArrayBuffer(double byte_length)
      : storage_(std::make_shared<Storage>(
            detail::buffer_index(byte_length, "flight::ArrayBuffer length is outside the supported range"))) {}

  [[nodiscard]] size_type byte_length() const noexcept { return storage_->length; }
  [[nodiscard]] const std::byte* data() const noexcept { return storage_->data; }
  [[nodiscard]] std::byte* data() noexcept { return storage_->data; }

  [[nodiscard]] static ArrayBuffer allocate(size_type byte_length) {
    return ArrayBuffer(byte_length, nullptr);
  }

  [[nodiscard]] friend bool operator==(const ArrayBuffer& left, const ArrayBuffer& right) noexcept {
    return left.storage_ == right.storage_;
  }

 private:
  explicit ArrayBuffer(size_type byte_length, std::nullptr_t)
      : storage_(std::make_shared<Storage>(byte_length)) {}

  std::shared_ptr<Storage> storage_;
};

} // namespace flight
