#pragma once

#include <SDL3/SDL_keycode.h>

#include <flight/types/node_2d.hpp>
#include <flight/typed_array.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace flight::types { struct GlRenderState; }

namespace flight::examples {

struct ScenePoint { double x{}, y{}; };

struct SceneInputEvent {
  enum class Kind { key_down, key_up, pointer_down, pointer_up, pointer_cancel };
  Kind kind;
  SDL_Keycode key{};
};

struct SceneInput {
  double pointer_x{}, pointer_y{}, wheel_y{};
  bool pointer_down{}, pointer_pressed{}, pointer_released{};
  std::vector<SDL_Keycode> keys_pressed;
  std::vector<SceneInputEvent> events;
  [[nodiscard]] bool key_down(SDL_Keycode key) const;
  [[nodiscard]] bool key_pressed(SDL_Keycode key) const;
 private:
  friend class Scene2DHost;
  std::unordered_set<SDL_Keycode> held_keys;
};

struct Scene2DFrame {
  const SceneInput& input;
  double time, delta_time;
  std::size_t index;
  bool smoke;
};

struct Scene2DRenderFrame {
  flight::Ref<flight::types::GlRenderState> state;
  const flight::Uint8Array& pixels;
  int width, height;
  std::size_t index;
};

struct Scene2DHostOptions {
  std::string_view name{"scene"};
  std::string_view title{"Flight Scene2D"};
  int width{800}, height{600};
  double maximum_delta_time{0.1};
  std::uint32_t background{0x1a1a2effU};
};

// Example wiring for the SDK's standard Scene2D GL pass. Window ownership and
// scheduling live in the installed GlApplication API. Applications retain and
// update their authored graph; this adapter supplies no drawing primitives.
class Scene2DHost final {
 public:
  using FrameCallback = std::function<void(const Scene2DFrame&)>;
  using CheckCallback = std::function<void(const Scene2DRenderFrame&)>;
  using SetupCallback = std::function<void(flight::Ref<flight::types::GlRenderState>)>;
  static int run(
      int argc, char** argv, flight::types::Node2D root,
      FrameCallback frame, CheckCallback check,
      const Scene2DHostOptions& options = {}, SetupCallback setup = {});
};

} // namespace flight::examples
