#pragma once

#include <stdexcept>
#include <utility>

#include <flight/string.hpp>

namespace flight {

class Error : public std::runtime_error {
 public:
  explicit Error(String message = String())
      : std::runtime_error(message.to_utf8()), message_(std::move(message)) {}

  [[nodiscard]] const String& message() const noexcept { return message_; }
  [[nodiscard]] static String name() { return String("Error"); }

 private:
  String message_;
};

class RangeError final : public Error {
 public:
  explicit RangeError(String message = String()) : Error(std::move(message)) {}

  [[nodiscard]] static String name() { return String("RangeError"); }
};

class TypeError final : public Error {
 public:
  explicit TypeError(String message = String()) : Error(std::move(message)) {}

  [[nodiscard]] static String name() { return String("TypeError"); }
};

} // namespace flight
