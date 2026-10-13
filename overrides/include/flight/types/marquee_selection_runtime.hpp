// Declared EntityRuntime heritage for the a627849 selection runtime.
#pragma once

#include <flight/runtime.hpp>
#include <flight/types/entity.hpp>
#include <flight/types/rectangle.hpp>

namespace flight::types {

struct MarqueeSelectionRuntime : public EntityRuntime {
  bool active;
  flight::Ref<Rectangle> rectangle;
  double start_x;
  double start_y;
};

} // namespace flight::types
