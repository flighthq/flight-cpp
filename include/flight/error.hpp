#pragma once

#include <exception>
#include <stdexcept>
#include <utility>

#include <flight/string.hpp>

namespace flight {

class Error : public std::runtime_error {
 public:
  explicit Error(String message = String())
      : std::runtime_error(message.to_utf8()), message_(std::move(message)) {}

  // The emitter lowers a TypeScript `catch (e)` to `catch (const std::exception& e)` and then reaches the
  // caught value's `.message` through `static_cast<flight::Error>(e)`. `what()` IS the message for every
  // exception this runtime throws -- the base is constructed from `message.to_utf8()` directly above -- so
  // taking it preserves the text rather than inventing one. Explicit, matching the message constructor, and
  // it cannot hijack copy construction: for an `Error` argument the copy constructor is the better match.
  explicit Error(const std::exception& caught) : Error(String(caught.what())) {}

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
