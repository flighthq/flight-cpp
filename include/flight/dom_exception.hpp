#pragma once

#include <stdexcept>
#include <utility>

#include <flight/string.hpp>

namespace flight {

// DOMException is separate from the ECMAScript Error hierarchy. Public value members match the
// Web IDL readonly attributes reached by generated code; copies preserve their exception values.
class DOMException : public std::runtime_error {
 public:
  explicit DOMException(String message_value = String(), String name_value = String("Error"))
      : std::runtime_error(message_value.to_utf8()),
        message(std::move(message_value)),
        name(std::move(name_value)),
        code(legacy_code(name)) {}

  String message;
  String name;
  double code{};

 private:
  [[nodiscard]] static double legacy_code(const String& value) {
    if (value == String("IndexSizeError")) return 1.0;
    if (value == String("HierarchyRequestError")) return 3.0;
    if (value == String("WrongDocumentError")) return 4.0;
    if (value == String("InvalidCharacterError")) return 5.0;
    if (value == String("NoModificationAllowedError")) return 7.0;
    if (value == String("NotFoundError")) return 8.0;
    if (value == String("NotSupportedError")) return 9.0;
    if (value == String("InUseAttributeError")) return 10.0;
    if (value == String("InvalidStateError")) return 11.0;
    if (value == String("SyntaxError")) return 12.0;
    if (value == String("InvalidModificationError")) return 13.0;
    if (value == String("NamespaceError")) return 14.0;
    if (value == String("InvalidAccessError")) return 15.0;
    if (value == String("TypeMismatchError")) return 17.0;
    if (value == String("SecurityError")) return 18.0;
    if (value == String("NetworkError")) return 19.0;
    if (value == String("AbortError")) return 20.0;
    if (value == String("URLMismatchError")) return 21.0;
    if (value == String("QuotaExceededError")) return 22.0;
    if (value == String("TimeoutError")) return 23.0;
    if (value == String("InvalidNodeTypeError")) return 24.0;
    if (value == String("DataCloneError")) return 25.0;
    return 0.0;
  }
};

} // namespace flight
