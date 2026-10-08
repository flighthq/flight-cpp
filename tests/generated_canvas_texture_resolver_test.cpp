#include <flight/scene2d_canvas/canvas_image_texture_resolver.hpp>

#include <iostream>
#include <variant>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

}  // namespace

int main() {
  namespace canvas = flight::scene2d_canvas;
  namespace types = flight::types;

  auto creator = flight::make_ref<types::CanvasRenderSurfaceCreator>();
  creator->create_render_surface = [](double, double, double) {
    return std::optional<flight::host_sdl::GlCanvas>{};
  };
  creator->destroy_render_surface = [](flight::host_sdl::GlCanvas) {};

  const auto resolvers = canvas::create_canvas_texture_resolvers(creator);
  check(resolvers->entity_runtime_key.has_value(), "creation attaches the Entity runtime marker");
  check(std::holds_alternative<flight::Null>(resolvers->registry),
        "a new resolver set has no registry");
  check(canvas::owned_surfaces.has(resolvers), "creation installs the owned-surface set");

  auto source = flight::make_ref<types::TextureSource>();
  source->kind = flight::String("image");
  auto texture_2d = flight::make_ref<types::Texture2D>();
  texture_2d->dimension = flight::String("2d");
  texture_2d->source = source;
  types::Texture texture{std::in_place_type<flight::Ref<types::Texture2D>>, texture_2d};

  bool resolved = false;
  types::CanvasTextureResolver resolver =
      [&](flight::Ref<types::CanvasTextureResolvers>, types::Texture received) {
        resolved = std::get<flight::Ref<types::Texture2D>>(received) == texture_2d;
        return std::optional<flight::host_sdl::ImageSource>{};
      };
  canvas::register_canvas_texture_resolver(
      resolvers, flight::String("image"), std::optional<types::CanvasTextureResolver>{resolver});
  (void)canvas::resolve_canvas_texture(resolvers, texture);
  check(resolved, "the registered source-kind resolver receives the same Texture2D owner");

  flight::Uint8ClampedArray pixels(4);
  pixels.set_index(0.0, 12);
  pixels.set_index(1.0, 34);
  pixels.set_index(2.0, 56);
  pixels.set_index(3.0, 255);
  const auto image_source = flight::host_sdl::ImageSource::rgba8(1, 1, pixels);
  const auto image_resource = flight::make_ref<types::ImageResource>();
  image_resource->kind = types::image_texture_source_kind;
  image_resource->source = flight::make_ref<types::HostImageSource>(image_source);
  texture_2d->source = std::reinterpret_pointer_cast<types::TextureSource>(image_resource);
  canvas::register_canvas_image_texture_resolver(resolvers);
  const auto resolved_image = canvas::resolve_canvas_texture(resolvers, texture);
  check(resolved_image.has_value() && resolved_image->identity() == image_source.identity(),
        "the image resolver recovers the original host image owner");

  bool missed = false;
  resolvers->registry_miss = decltype(types::CanvasTextureResolvers::registry_miss){
      std::in_place_type<canvas::RegistryMiss>,
      [&](types::RenderRegistryTable registry, flight::String kind) {
        missed = registry == types::RenderRegistryTable::TextureResolver &&
                 kind == flight::String("image");
      }};
  canvas::register_canvas_texture_resolver(
      resolvers, flight::String("image"), std::optional<types::CanvasTextureResolver>{});
  (void)canvas::resolve_canvas_texture(resolvers, texture);
  check(missed, "an absent source-kind resolver reports through the configured miss seam");

  canvas::destroy_canvas_texture_resolvers(resolvers);
  check(!canvas::owned_surfaces.has(resolvers), "destroy forgets the owned-surface set");
  check(std::holds_alternative<flight::Null>(resolvers->registry), "destroy clears the registry");
  check(std::holds_alternative<flight::Null>(resolvers->registry_miss),
        "destroy clears the miss seam");

  if (failures == 0) std::cout << "canvas texture resolver behavior matches Flight\n";
  return failures == 0 ? 0 : 1;
}
