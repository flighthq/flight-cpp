#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <flight/array_buffer_view.hpp>
#include <flight/string.hpp>
#include <flight/task.hpp>
#include <flight/text_decoder.hpp>
#include <flight/typed_array.hpp>

namespace flight {

class Blob;

struct BlobOptions final {
  String endings{"transparent"};
  String type;
};

namespace detail {

struct BlobState final {
  std::vector<std::byte> bytes;
  String type;
};

inline String normalize_blob_type(const String& value) {
  std::string bytes = value.to_utf8();
  if (std::ranges::any_of(bytes, [](unsigned char byte) { return byte < 0x20 || byte > 0x7E; })) {
    return {};
  }
  std::ranges::transform(bytes, bytes.begin(), [](unsigned char byte) {
    return byte >= 'A' && byte <= 'Z' ? static_cast<char>(byte + ('a' - 'A'))
                                      : static_cast<char>(byte);
  });
  return String::from_utf8(bytes);
}

inline void append_blob_bytes(std::vector<std::byte>& output, std::span<const std::byte> bytes) {
  output.insert(output.end(), bytes.begin(), bytes.end());
}

inline void append_blob_part(std::vector<std::byte>& output, const String& value) {
  const auto bytes = value.to_utf8();
  append_blob_bytes(output, std::as_bytes(std::span(bytes)));
}

inline void append_blob_part(std::vector<std::byte>& output, const ArrayBufferLike& value) {
  append_blob_bytes(output, {value.data(), value.byte_length()});
}

inline void append_blob_part(std::vector<std::byte>& output, const ArrayBufferView& value) {
  append_blob_bytes(output, {value.data(), value.byte_length});
}

template <typename Value>
inline void append_blob_part(std::vector<std::byte>& output, const TypedArray<Value>& value) {
  append_blob_bytes(output, std::as_bytes(value.span()));
}

template <typename... Values>
inline void append_blob_part(std::vector<std::byte>& output, const std::variant<Values...>& value) {
  std::visit([&](const auto& part) { append_blob_part(output, part); }, value);
}

void append_blob_part(std::vector<std::byte>& output, const Blob& value);

} // namespace detail

class Blob final {
 private:
  std::shared_ptr<detail::BlobState> state_;

 public:
  using weak_type = std::weak_ptr<detail::BlobState>;

  Blob() : state_(std::make_shared<detail::BlobState>()) {}

  template <typename Parts>
    requires requires(const Parts& parts) {
      std::begin(parts);
      std::end(parts);
    }
  explicit Blob(const Parts& parts, BlobOptions options = {})
      : state_(std::make_shared<detail::BlobState>()),
        type(detail::normalize_blob_type(options.type)) {
    for (const auto& part : parts) detail::append_blob_part(state_->bytes, part);
    state_->type = type;
    size = state_->bytes.size();
  }

  [[nodiscard]] friend bool operator==(const Blob& left, const Blob& right) noexcept {
    return left.state_ == right.state_;
  }
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }
  [[nodiscard]] weak_type weaken() const noexcept { return state_; }
  [[nodiscard]] static std::optional<Blob> lock_weak(const weak_type& weak) noexcept {
    auto state = weak.lock();
    return state ? std::optional<Blob>(Blob(std::move(state))) : std::nullopt;
  }

  [[nodiscard]] Task<ArrayBuffer> array_buffer() const {
    auto output = ArrayBuffer::allocate(state_->bytes.size());
    if (!state_->bytes.empty()) {
      std::memcpy(output.data(), state_->bytes.data(), state_->bytes.size());
    }
    return Task<ArrayBuffer>::resolve(std::move(output));
  }

  [[nodiscard]] std::span<const std::byte> bytes() const noexcept { return state_->bytes; }

  [[nodiscard]] Blob slice(
      double start = 0,
      double end = std::numeric_limits<double>::infinity(),
      String content_type = {}) const {
    const auto first = normalize_index(start);
    const auto last = normalize_index(end);
    std::vector<std::byte> output;
    if (last > first) {
      output.assign(state_->bytes.begin() + static_cast<std::ptrdiff_t>(first),
                    state_->bytes.begin() + static_cast<std::ptrdiff_t>(last));
    }
    return Blob(std::make_shared<detail::BlobState>(detail::BlobState{
        std::move(output), detail::normalize_blob_type(content_type)}));
  }

  [[nodiscard]] Task<String> text() const {
    std::vector<std::uint8_t> values;
    values.reserve(state_->bytes.size());
    for (const auto byte : state_->bytes) values.push_back(std::to_integer<std::uint8_t>(byte));
    return Task<String>::resolve(TextDecoder().decode(Uint8Array(values)));
  }

  std::size_t size{0};
  String type;

 private:
  explicit Blob(std::shared_ptr<detail::BlobState> state)
      : state_(std::move(state)), size(state_->bytes.size()), type(state_->type) {}

  [[nodiscard]] std::size_t normalize_index(double index) const noexcept {
    if (std::isnan(index)) return 0;
    if (index == std::numeric_limits<double>::infinity()) return state_->bytes.size();
    if (index == -std::numeric_limits<double>::infinity()) return 0;
    const auto integer = std::trunc(index);
    if (integer < 0) {
      const auto magnitude = -integer;
      return magnitude >= static_cast<double>(state_->bytes.size())
               ? 0
               : state_->bytes.size() - static_cast<std::size_t>(magnitude);
    }
    return integer >= static_cast<double>(state_->bytes.size())
             ? state_->bytes.size()
             : static_cast<std::size_t>(integer);
  }
};

namespace detail {

inline void append_blob_part(std::vector<std::byte>& output, const Blob& value) {
  append_blob_bytes(output, value.bytes());
}

} // namespace detail

} // namespace flight
