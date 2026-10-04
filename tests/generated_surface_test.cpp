#include <flight/surface/canvas_surface.hpp>

#include <iostream>
#include <optional>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

}  // namespace

int main() {
  using namespace flight::types;

  const auto native_handle = flight::Any::external<int>(42);
  std::shared_ptr<flight::RowOwner> acquired_owner;
  std::shared_ptr<flight::RowOwner> released_owner;
  int acquire_calls = 0;
  int release_calls = 0;
  const auto capability = flight::make_ref<HostCanvasCapability>(
      HostCanvasCapability{
          .acquire = [&](auto surface, auto) {
            ++acquire_calls;
            acquired_owner = surface.shared_owner();
            const auto recovered =
                flight::surface::get_surface_handle(surface).template external_if<int>();
            if (recovered == nullptr || *recovered != 42) {
              return std::optional<flight::CanvasRenderingContext2D>{};
            }
            return std::optional<flight::CanvasRenderingContext2D>{
                flight::CanvasRenderingContext2D{}};
          },
          .create = [](auto, double, double, auto) {
            return std::optional<flight::Any>{};
          },
          .release = [&](auto surface) {
            ++release_calls;
            released_owner = surface.shared_owner();
          },
      });

  const auto created = flight::surface::create_canvas_surface_from_native_handle(
      flight::surface::ReadonlyCanvasCapability(capability), native_handle);
  if (!check(created.has_value() && acquire_calls == 1,
             "canvas surface did not acquire exactly once") ||
      !check(created.value()->brand == flight::String("CanvasSurface"),
             "canvas surface brand changed")) {
    return 1;
  }

  const auto surface_row =
      flight::surface::ReadonlyCanvasSurface(created.value());
  const auto runtime = flight::surface::get_surface_runtime(
      flight::surface::readonly_surface_view(surface_row));
  const auto generic_runtime = created.value()->entity_runtime_key.value();
  const auto recovered_handle = flight::surface::get_surface_handle(
      flight::surface::readonly_surface_view(surface_row));
  if (!check(generic_runtime.get() ==
                 static_cast<EntityRuntime*>(runtime.get()),
             "surface runtime was copied instead of stored through one owner") ||
      !check(recovered_handle == native_handle,
             "surface handle identity changed across runtime storage") ||
      !check(acquired_owner == surface_row.shared_owner(),
             "canvas acquire received a copied surface row")) {
    return 1;
  }

  flight::surface::destroy_canvas_surface(
      flight::surface::ReadonlyCanvasCapability(capability), surface_row);
  if (!check(release_calls == 1 && released_owner == surface_row.shared_owner(),
             "canvas release did not receive the returned surface owner")) {
    return 1;
  }

  capability->acquire = [](auto, auto) {
    return std::optional<flight::CanvasRenderingContext2D>{};
  };
  const auto refused = flight::surface::create_canvas_surface_from_native_handle(
      flight::surface::ReadonlyCanvasCapability(capability), native_handle);
  if (!check(!refused.has_value(),
             "canvas acquisition failure did not remain a null result")) {
    return 1;
  }

  return 0;
}
