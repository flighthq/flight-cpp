#pragma once

// The ONE out-of-profile error family for a value that cannot cross a native typed boundary in the
// shared view family. It is deliberately NOT flight::Error/DOMException, so it cannot be observed as
// a JS-shaped catchable error; callers see a distinct C++ failure at the actual native boundary and
// nowhere earlier. Both the JSON scalar projection and the GL physical-cell typed views construct
// this same type -- there is no second family and no copy.

#include <stdexcept>
#include <string>
#include <string_view>

namespace flight {

class NativeScalarEscapeError final : public std::runtime_error {
 public:
  explicit NativeScalarEscapeError(std::string_view held)
      : std::runtime_error("an out-of-profile payload of type '" + std::string(held) +
                           "' cannot be used where a native value is required") {}
};

} // namespace flight
