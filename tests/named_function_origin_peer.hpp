#pragma once
#include <functional>

namespace named_origin_test {
inline double source(double value) { return value + 1; }
inline double other(double value) { return value + 1; }
std::function<double(double, bool)> from_peer();
}
