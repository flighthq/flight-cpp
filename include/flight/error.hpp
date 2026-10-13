#pragma once

#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

#include <flight/reference.hpp>
#include <flight/string.hpp>

namespace flight {

class Any;

// ES2022 `ErrorOptions`: the second argument of `new Error(message, { cause })`. Defined in
// flight/any.hpp, where `Any` is complete, because a cause is any source value.
struct ErrorOptions;

// `ReferenceEnabled` because a source-language error is an OBJECT: `catch` hands out the value that
// was thrown, `e === caught` is identity, and a `RangeError` read through an `Error` binding must
// still be the RangeError. Without the marker `flight::Ref<Error>` is the value shape, so every
// reference to an Error would be a copy -- and a copy slices the subclass, which is precisely the
// wrong answer `name` has to give. Marking it makes `Ref<Error>` a shared reference to the object.
class Error : public std::runtime_error, public ReferenceEnabled {
 public:
  explicit Error(String message = String())
      : std::runtime_error(message.to_utf8()), message_(std::move(message)) {}

  // The emitter lowers a TypeScript `catch (e)` to `catch (const std::exception& e)` and then reaches the
  // caught value's `.message` through `static_cast<flight::Error>(e)`. `what()` IS the message for every
  // exception this runtime throws -- the base is constructed from `message.to_utf8()` directly above -- so
  // taking it preserves the text rather than inventing one. Explicit, matching the message constructor, and
  // it cannot hijack copy construction: for an `Error` argument the copy constructor is the better match.
  explicit Error(const std::exception& caught) : Error(String(caught.what())) {}

  // `new Error(message, { cause })`. The cause is kept exactly as given -- the same erased value,
  // so an object cause keeps its identity -- and `cause()` answers it. Defined in flight/any.hpp.
  Error(String message, const ErrorOptions& options);

  [[nodiscard]] const String& message() const noexcept { return message_; }

  // `error.stack`. ECMAScript does not define `stack`; it is an implementation-defined property that
  // engines which capture a call stack add to their errors. This runtime captures none, so an Error
  // here has no `stack` and reading it is `undefined` -- the answer the source gets from any engine
  // that records no trace. Nothing is fabricated: there is no trace to report.
  [[nodiscard]] std::optional<String> stack() const { return std::nullopt; }

  // `error.cause`: present exactly when the Error was constructed with an `ErrorOptions` carrying a
  // cause, `undefined` otherwise. Defined in flight/any.hpp.
  [[nodiscard]] std::optional<Any> cause() const;

  // `name` is a DYNAMIC source property, and the emitted read is `value.name` through an Error-typed
  // value that may hold a subclass: `catch` hands back whatever was thrown, and `e instanceof Error`
  // is true for a RangeError. A static accessor would answer the name of the class the reference was
  // declared as, so reading `name` off a RangeError held as an `Error&` -- which is every catch site
  // and every Error-typed storage slot -- would say "Error". The answer has to come from the object,
  // so this is an instance accessor and it is virtual; the subclasses below override it.
  [[nodiscard]] virtual String name() const { return String("Error"); }

 private:
  String message_;
  // Held behind a pointer because `Any` is incomplete here; null means no cause was given.
  std::shared_ptr<const Any> cause_;
};

// The object a caught-exception pointer owns, as the reference a non-awaiting handler binds.
//
// C++ forbids `co_await` inside a handler, so a catch body that awaits is hoisted out of it and the
// handler's `const std::exception&` does not survive the jump. `std::exception_ptr` does: it OWNS the
// thrown object for as long as it is held. This re-enters that object as the very reference the handler
// would have bound -- the same object, not a copy and not a second Error -- so a hoisted body reads its
// catch binding exactly as a non-awaiting body does, and every conversion that follows from that
// representation (`static_cast<flight::Error>`, the erased Error view, a task rejection) answers the same.
[[nodiscard]] inline const std::exception& caught_exception(const std::exception_ptr& pointer) {
  if (pointer == nullptr) {
    // Unreachable from source: the pointer is read only on the branch that recorded it, and it is recorded
    // from inside a handler, which always has an exception to record. Reaching this means the emitter
    // re-bound a caught value on a path that never caught -- a defect, not a value the source could hold,
    // so it fails as one rather than inventing an exception.
    throw std::logic_error("flight: a hoisted catch body was entered without a caught exception");
  }
  try {
    std::rethrow_exception(pointer);
  } catch (const std::exception& caught) {
    // Safe to return: the object lives in the exception the caller's pointer owns, which outlives this
    // handler. A thrown value that is not a class exception never reaches here -- the handler this
    // re-binding belongs to catches `const std::exception&`, so the hoisted body is not entered for it.
    return caught;
  }
}

class RangeError final : public Error {
 public:
  explicit RangeError(String message = String()) : Error(std::move(message)) {}
  RangeError(String message, const ErrorOptions& options) : Error(std::move(message), options) {}

  [[nodiscard]] String name() const override { return String("RangeError"); }
};

class TypeError final : public Error {
 public:
  explicit TypeError(String message = String()) : Error(std::move(message)) {}
  TypeError(String message, const ErrorOptions& options) : Error(std::move(message), options) {}

  [[nodiscard]] String name() const override { return String("TypeError"); }
};

} // namespace flight
