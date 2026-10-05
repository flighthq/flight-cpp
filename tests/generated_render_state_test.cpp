#include <flight/types/render_proxy.hpp>

#include <iostream>
#include <optional>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

} // namespace

int main() {
  auto state = flight::make_ref<flight::types::RenderState>();
  auto renderer = flight::make_ref<flight::types::Renderer>();
  auto data = flight::make_ref<flight::types::RendererData>();
  auto proxy = flight::make_ref<flight::types::RenderProxy>();

  int calls = 0;
  renderer->destroy_data =
      [&](flight::Ref<flight::types::RenderState> received_state,
          flight::Ref<flight::types::RendererData> received_data) {
        if (received_state == state && received_data == data) ++calls;
      };
  proxy->renderer = renderer;
  proxy->renderer_data = data;

  flight::types::invoke_render_proxy_destroy_data(
      std::optional<flight::Ref<flight::types::RenderProxy>>{proxy}, state);
  if (!check(calls == 1,
             "render proxy destroy did not forward its state and renderer data")) {
    return 1;
  }

  flight::types::invoke_render_proxy_destroy_data(std::nullopt, state);
  proxy->renderer_data = std::nullopt;
  flight::types::invoke_render_proxy_destroy_data(
      std::optional<flight::Ref<flight::types::RenderProxy>>{proxy}, state);
  if (!check(calls == 1,
             "render proxy destroy ran without a proxy or renderer data")) {
    return 1;
  }

  proxy->renderer_data = data;
  renderer->destroy_data = std::nullopt;
  flight::types::invoke_render_proxy_destroy_data(
      std::optional<flight::Ref<flight::types::RenderProxy>>{proxy}, state);
  if (!check(calls == 1, "render proxy destroy ran without a destroy hook")) return 1;

  return 0;
}
