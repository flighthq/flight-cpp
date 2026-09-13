#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

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

struct OwnedBufferStorage {
  explicit OwnedBufferStorage(std::size_t length)
      : data(static_cast<std::byte*>(
            ::operator new(std::max(length, std::size_t{1}), std::align_val_t{alignof(std::max_align_t)}))),
        length(length) {
    std::memset(data, 0, length);
  }

  OwnedBufferStorage(const OwnedBufferStorage&) = delete;
  OwnedBufferStorage& operator=(const OwnedBufferStorage&) = delete;

  ~OwnedBufferStorage() { ::operator delete(data, std::align_val_t{alignof(std::max_align_t)}); }

  std::byte* data;
  std::size_t length;
};

} // namespace detail

enum class ArrayBufferKind {
  array_buffer,
  shared_array_buffer,
  external,
};

enum class ArrayBufferMutability {
  read_only,
  read_write,
};

enum class ArrayBufferConcurrency {
  single_threaded,
  shared,
  caller_synchronized,
};

class ArrayBufferLike {
  struct Backing {
    std::shared_ptr<const void> owner;
    const std::byte* data;
    std::byte* writable_data;
    std::size_t length;
    ArrayBufferKind kind;
    ArrayBufferMutability mutability;
    ArrayBufferConcurrency concurrency;
  };

 public:
  using size_type = std::size_t;

  class ByteLength final {
   public:
    explicit ByteLength(const ArrayBufferLike* owner) noexcept : owner_(owner) {}
    [[nodiscard]] operator size_type() const noexcept { return owner_->backing_->length; }
    [[nodiscard]] size_type operator()() const noexcept { return owner_->backing_->length; }

   private:
    const ArrayBufferLike* owner_;
  };

  ByteLength byte_length{this};

  ArrayBufferLike() : backing_(make_owned(0, ArrayBufferKind::array_buffer,
                                           ArrayBufferConcurrency::single_threaded)) {}

  ArrayBufferLike(const ArrayBufferLike& other)
      : byte_length(this), backing_(other.backing_) {}

  ArrayBufferLike(ArrayBufferLike&& other) noexcept
      : byte_length(this), backing_(std::move(other.backing_)) {}

  ArrayBufferLike& operator=(const ArrayBufferLike& other) {
    backing_ = other.backing_;
    return *this;
  }

  ArrayBufferLike& operator=(ArrayBufferLike&& other) noexcept {
    backing_ = std::move(other.backing_);
    return *this;
  }

  template <typename Owner>
  [[nodiscard]] static ArrayBufferLike from_external(
      std::shared_ptr<Owner> owner,
      std::byte* data,
      size_type byte_length,
      ArrayBufferConcurrency concurrency = ArrayBufferConcurrency::caller_synchronized) {
    static_assert(!std::is_const_v<Owner>);
    validate_external(owner, data, byte_length);
    return ArrayBufferLike(std::make_shared<Backing>(Backing{
        std::move(owner), data, data, byte_length, ArrayBufferKind::external,
        ArrayBufferMutability::read_write, concurrency}));
  }

  template <typename Owner>
  [[nodiscard]] static ArrayBufferLike from_external(
      std::shared_ptr<Owner> owner,
      const std::byte* data,
      size_type byte_length,
      ArrayBufferConcurrency concurrency = ArrayBufferConcurrency::caller_synchronized) {
    validate_external(owner, data, byte_length);
    return ArrayBufferLike(std::make_shared<Backing>(Backing{
        std::move(owner), data, nullptr, byte_length, ArrayBufferKind::external,
        ArrayBufferMutability::read_only, concurrency}));
  }

  [[nodiscard]] const std::byte* data() const noexcept { return backing_->data; }
  [[nodiscard]] std::byte* data() { return writable_data(); }
  [[nodiscard]] std::byte* writable_data() {
    if (!is_writable()) throw std::logic_error("flight::ArrayBufferLike backing is read-only");
    return backing_->writable_data;
  }
  [[nodiscard]] const void* identity() const noexcept { return backing_.get(); }
  [[nodiscard]] ArrayBufferKind kind() const noexcept { return backing_->kind; }
  [[nodiscard]] ArrayBufferMutability mutability() const noexcept { return backing_->mutability; }
  [[nodiscard]] ArrayBufferConcurrency concurrency() const noexcept { return backing_->concurrency; }
  [[nodiscard]] bool is_writable() const noexcept {
    return backing_->mutability == ArrayBufferMutability::read_write;
  }

  [[nodiscard]] friend bool operator==(const ArrayBufferLike& left,
                                       const ArrayBufferLike& right) noexcept {
    return left.backing_ == right.backing_;
  }

 protected:
  ArrayBufferLike(size_type byte_length, ArrayBufferKind kind, ArrayBufferConcurrency concurrency)
      : backing_(make_owned(byte_length, kind, concurrency)) {}

 private:
  explicit ArrayBufferLike(std::shared_ptr<Backing> backing) : backing_(std::move(backing)) {}

  [[nodiscard]] static std::shared_ptr<Backing> make_owned(
      size_type byte_length, ArrayBufferKind kind, ArrayBufferConcurrency concurrency) {
    auto storage = std::make_shared<detail::OwnedBufferStorage>(byte_length);
    return std::make_shared<Backing>(Backing{
        storage, storage->data, storage->data, storage->length, kind,
        ArrayBufferMutability::read_write, concurrency});
  }

  template <typename Owner, typename Byte>
  static void validate_external(const std::shared_ptr<Owner>& owner, Byte* data, size_type) {
    if (!owner) throw std::invalid_argument("flight::ArrayBufferLike external backing requires an owner");
    if (data == nullptr) throw std::invalid_argument("flight::ArrayBufferLike external backing requires bytes");
  }

  std::shared_ptr<Backing> backing_;
};

class ArrayBuffer : public ArrayBufferLike {
 public:
  using size_type = ArrayBufferLike::size_type;

  ArrayBuffer() = default;

  explicit ArrayBuffer(double byte_length)
      : ArrayBuffer(detail::buffer_index(
            byte_length, "flight::ArrayBuffer length is outside the supported range"), OwnedTag{}) {}

  [[nodiscard]] static ArrayBuffer allocate(size_type byte_length) {
    return ArrayBuffer(byte_length, OwnedTag{});
  }

 private:
  struct OwnedTag {};

  ArrayBuffer(size_type byte_length, OwnedTag)
      : ArrayBufferLike(byte_length, ArrayBufferKind::array_buffer,
                        ArrayBufferConcurrency::single_threaded) {}
};

class SharedArrayBuffer : public ArrayBufferLike {
 public:
  using size_type = ArrayBufferLike::size_type;

  SharedArrayBuffer() : ArrayBufferLike(0, ArrayBufferKind::shared_array_buffer,
                                        ArrayBufferConcurrency::shared) {}

  explicit SharedArrayBuffer(double byte_length)
      : SharedArrayBuffer(detail::buffer_index(
            byte_length, "flight::SharedArrayBuffer length is outside the supported range"), OwnedTag{}) {}

  [[nodiscard]] static SharedArrayBuffer allocate(size_type byte_length) {
    return SharedArrayBuffer(byte_length, OwnedTag{});
  }

 private:
  struct OwnedTag {};

  SharedArrayBuffer(size_type byte_length, OwnedTag)
      : ArrayBufferLike(byte_length, ArrayBufferKind::shared_array_buffer,
                        ArrayBufferConcurrency::shared) {}
};

} // namespace flight
