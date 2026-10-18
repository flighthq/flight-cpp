#pragma once
#include <flight/reference.hpp>
namespace base_package {
struct Target : flight::ReferenceEnabled {
  double width{0.0};
};
} // namespace base_package
