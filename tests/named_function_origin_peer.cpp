#include "named_function_origin_peer.hpp"
#include <flight/callable.hpp>

namespace named_origin_test {
std::function<double(double, bool)> from_peer() {
  return flight::NamedFunction<double(double, bool)>::from<&source>(
      [](double value, bool) { return source(value); });
}
}
