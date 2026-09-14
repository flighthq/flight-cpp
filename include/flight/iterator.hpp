#pragma once

#include <optional>

namespace flight {

template <typename Value>
struct IteratorResult final {
  std::optional<Value> value;
  bool done{true};
};

} // namespace flight
