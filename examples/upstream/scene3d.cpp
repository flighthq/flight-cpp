// Native SDL/OpenGL ES port of @flighthq/example-scene3d.
// Sources: .dependencies/flight/examples/packages/scene3d/src/{app,render.webgl}.ts

#include <flight/host_sdl/host.hpp>
#include <flight/host_sdl/web_platform.hpp>
#include <flight/host_sdl/webgl.hpp>

#include <flight/camera/camera.hpp>
#include <flight/camera/projection.hpp>
#include <flight/camera/shadow_camera.hpp>
#include <flight/camera_controls/orbit_camera_controller.hpp>
#include <flight/effects_gl/gl_effect_state.hpp>
#include <flight/geometry/aabb.hpp>
#include <flight/geometry/vector3.hpp>
#include <flight/lighting/ambient_light.hpp>
#include <flight/lighting/directional_light.hpp>
#include <flight/lighting/point_light.hpp>
#include <flight/materials/pbr_materials.hpp>
#include <flight/mesh/mesh_geometry_builders.hpp>
#include <flight/node/node_transform3d.hpp>
#include <flight/render/scene_render.hpp>
#include <flight/render_gl/gl_render_state.hpp>
#include <flight/scene3d/mesh.hpp>
#include <flight/scene3d/scene_node.hpp>
#include <flight/scene3d_gl/gl_shadow_map.hpp>
#include <flight/scene3d_gl/render_gl_scene3_d.hpp>
#include <flight/scene3d_gl/scene3_dgl_pipeline.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>

namespace {

template <typename Value>
using Readonly = flight::StructuralRef<
    flight::RowReadonly<flight::RowOf<flight::Ref<Value>>>>;

template <typename Value>
[[nodiscard]] Readonly<Value> readonly(const std::shared_ptr<Value>& value) {
  return Readonly<Value>(value);
}

[[nodiscard]] flight::Ref<flight::types::StandardPbrMaterial> pbr_material(
    std::uint32_t base_color, double metallic, double roughness) {
  using Options = flight::RowReadonly<flight::RowPartial<
      flight::RowOf<flight::Ref<flight::types::StandardPbrMaterial>>>>;
  return flight::materials::create_standard_pbr_material(
      flight::make_structural_ref<Options>(
          flight::row_field<flight::RowKey<"baseColor">>(
              std::optional<double>{static_cast<double>(base_color)}),
          flight::row_field<flight::RowKey<"metallic">>(
              std::optional<double>{metallic}),
          flight::row_field<flight::RowKey<"roughness">>(
              std::optional<double>{roughness})));
}

[[nodiscard]] flight::Array<std::optional<flight::types::MaterialValue>>
material_list(const flight::Ref<flight::types::StandardPbrMaterial>& material) {
  return {std::optional<flight::types::MaterialValue>{
      flight::types::MaterialValue{
          std::in_place_type<flight::Ref<flight::types::StandardPbrMaterial>>,
          material}}};
}

[[nodiscard]] flight::Ref<flight::types::Camera3D> perspective_camera() {
  auto projection_options =
      flight::make_ref<flight::types::PerspectiveProjectionOptions>();
  projection_options->aspect = 800.0 / 600.0;
  projection_options->fov_y = flight::pi / 4.0;
  auto projection =
      flight::camera::create_perspective_projection(readonly(projection_options));

  auto options = flight::make_ref<flight::types::Camera3DOptions>();
  options->far = 100.0;
  options->near = 0.1;
  options->projection = flight::types::Projection{
      std::in_place_type<flight::Ref<flight::types::PerspectiveProjection>>,
      projection};
  return flight::camera::create_camera3_d(readonly(options));
}

[[nodiscard]] flight::Ref<flight::types::Camera3D> shadow_camera(
    const flight::Ref<flight::types::Vector3>& light_direction) {
  auto projection_options =
      flight::make_ref<flight::types::OrthographicProjectionOptions>();
  projection_options->half_height = 4.0;
  projection_options->half_width = 4.0;
  auto projection =
      flight::camera::create_orthographic_projection(readonly(projection_options));

  auto options = flight::make_ref<flight::types::Camera3DOptions>();
  options->far = 20.0;
  options->near = 0.1;
  options->projection = flight::types::Projection{
      std::in_place_type<flight::Ref<flight::types::OrthographicProjection>>,
      projection};
  auto camera = flight::camera::create_camera3_d(readonly(options));
  auto bounds = flight::geometry::create_aabb(-3.5, -0.8, -2.5, 3.5, 1.0, 2.5);
  flight::camera::configure_directional_shadow_camera3_d(
      camera, readonly(light_direction), readonly(bounds));
  return camera;
}

struct Scene final {
  flight::Ref<flight::types::Node3D> root;
  flight::Ref<flight::types::Camera3D> camera;
  flight::Ref<flight::types::OrbitCameraController> controller;
  flight::Ref<flight::types::DirectionalLight> directional_light;
  flight::Ref<flight::types::Scene3DLights> lights;
  flight::Ref<flight::types::Camera3D> shadow_camera;
};

[[nodiscard]] Scene create_scene() {
  const auto box_geometry =
      flight::mesh::create_box_mesh_geometry(1.0, 1.0, 1.0);
  const auto sphere_geometry =
      flight::mesh::create_sphere_mesh_geometry(0.5, 48.0, 32.0);
  const auto cone_geometry =
      flight::mesh::create_cone_mesh_geometry(0.5, 1.0, 32.0);

  const auto red_material = pbr_material(0xcc3333ffU, 0.0, 0.4);
  const auto gray_metallic_material = pbr_material(0xaaaaaaffU, 1.0, 0.3);
  const auto blue_material = pbr_material(0x3366ccffU, 0.0, 0.5);

  auto root = flight::scene3d::create_node3_d(flight::types::node3_dkind);

  auto ground = flight::scene3d::create_mesh(
      flight::mesh::create_plane_mesh_geometry(7.0, 5.0, 7.0, 5.0),
      material_list(pbr_material(0x182235ffU, 0.0, 0.86)));
  ground->position->y = -0.75;
  flight::node::invalidate_node_local_transform(ground);
  flight::node::add_node_child(root, ground);

  auto box = flight::scene3d::create_mesh(
      box_geometry, material_list(red_material));
  box->position->x = -2.0;
  box->position->y = -0.25;
  flight::node::invalidate_node_local_transform(box);
  flight::node::add_node_child(root, box);

  auto sphere = flight::scene3d::create_mesh(
      sphere_geometry, material_list(gray_metallic_material));
  sphere->position->y = -0.25;
  flight::node::invalidate_node_local_transform(sphere);
  flight::node::add_node_child(root, sphere);

  auto cone = flight::scene3d::create_mesh(
      cone_geometry, material_list(blue_material));
  cone->position->x = 2.0;
  cone->position->y = -0.25;
  flight::node::invalidate_node_local_transform(cone);
  flight::node::add_node_child(root, cone);

  auto camera = perspective_camera();
  auto controller_options =
      flight::make_ref<flight::types::OrbitCameraControllerOptions>();
  controller_options->azimuth = 0.68;
  controller_options->distance = 7.0;
  controller_options->polar = 0.38;
  controller_options->smooth_time = 0.12;
  controller_options->target = readonly(
      flight::geometry::create_vector3(0.0, -0.25, 0.0));
  auto controller = flight::camera_controls::create_orbit_camera_controller(
      readonly(controller_options));

  auto direction = flight::geometry::create_vector3(-1.0, -0.5, -0.7);
  flight::geometry::normalize_vector3(direction, readonly(direction));

  auto directional_options =
      flight::make_ref<flight::types::DirectionalLightOptions>();
  directional_options->casts_shadow = true;
  directional_options->color = static_cast<double>(0xffe3c4ffU);
  directional_options->direction = readonly(direction);
  directional_options->intensity = 3.0;
  directional_options->normal_bias = 0.75;
  directional_options->pcf_radius = 1.0;
  directional_options->shadow_bias = 0.001;
  auto directional =
      flight::lighting::create_directional_light(readonly(directional_options));

  auto ambient_options = flight::make_ref<flight::types::AmbientLightOptions>();
  ambient_options->color = static_cast<double>(0x607090ffU);
  ambient_options->intensity = 0.2;
  auto ambient = flight::lighting::create_ambient_light(readonly(ambient_options));

  auto point_options = flight::make_ref<flight::types::PointLightOptions>();
  point_options->color = static_cast<double>(0x5ea8ffffU);
  point_options->intensity = 8.0;
  point_options->position = readonly(
      flight::geometry::create_vector3(1.8, 1.6, 2.0));
  point_options->range = 7.0;
  auto point = flight::lighting::create_point_light(readonly(point_options));

  auto lights = flight::make_ref<flight::types::Scene3DLights>();
  lights->entity_runtime_key = std::nullopt;
  lights->ambient = readonly(ambient);
  lights->directional = readonly(directional);
  lights->hemisphere = std::nullopt;
  lights->point = flight::Array<Readonly<flight::types::PointLight>>{
      readonly(point)};
  lights->spot = std::nullopt;

  return Scene{
      .root = std::move(root),
      .camera = std::move(camera),
      .controller = std::move(controller),
      .directional_light = std::move(directional),
      .lights = std::move(lights),
      .shadow_camera = shadow_camera(direction),
  };
}

class Scene3DExample final {
 public:
  explicit Scene3DExample(bool smoke)
      : host_(),
        canvas_(flight::host_sdl::GlCanvas::create(
            800, 600, 1.0, flight::String("Flight scene3d - SDL + OpenGL ES"),
            smoke)),
        gl_(canvas_.get_context()),
        platform_input_(canvas_),
        scene_(create_scene()) {
    gl_.make_current();

    auto render_options = flight::make_ref<flight::types::GlRenderOptions>();
    render_options->pixel_ratio = 1.0;
    state_ = flight::render_gl::create_gl_render_state(
        gl_, flight::scene3d_gl::default_scene3_dgl_render_registries,
        render_options);

    auto effect_options = flight::make_ref<flight::types::EffectStateOptions>();
    effect_options->sample_count = 4.0;
    effect_options->format = flight::String("rgba16f");
    effect_options->depth = flight::String("depth-stencil");
    pipeline_ = flight::effects_gl::create_gl_effect_state(
        state_, readonly(effect_options));

    canvas_.add_event_listener(
        flight::String("pointerdown"),
        [this](flight::host_sdl::InputPointerData event) {
          dragging_ = true;
          previous_x_ = event.client_x;
          previous_y_ = event.client_y;
          canvas_.set_pointer_capture(event.pointer_id);
        });
    canvas_.add_event_listener(
        flight::String("pointermove"),
        [this](flight::host_sdl::InputPointerData event) {
          if (!dragging_) return;
          flight::camera_controls::rotate_orbit_camera_controller(
              scene_.controller, -(event.client_x - previous_x_) * 0.008,
              (event.client_y - previous_y_) * 0.008);
          previous_x_ = event.client_x;
          previous_y_ = event.client_y;
        });
    canvas_.add_event_listener(
        flight::String("pointerup"),
        [this](flight::host_sdl::InputPointerData event) {
          dragging_ = false;
          canvas_.release_pointer_capture(event.pointer_id);
        });
    canvas_.add_event_listener(
        flight::String("pointercancel"),
        [this](flight::host_sdl::InputPointerData) { dragging_ = false; });
    canvas_.add_event_listener(
        flight::String("wheel"),
        [this](flight::host_sdl::InputPointerData event) {
          // The web source receives DOM_PIXEL_DELTA (normally about 100 per wheel notch) and
          // multiplies by 0.006. The SDL host intentionally exposes line deltas, so 0.6 preserves
          // the same controller-distance change per notch.
          flight::camera_controls::dolly_orbit_camera_controller(
              scene_.controller, event.delta_y * 0.6);
        });

    flight::camera_controls::update_orbit_camera_controller(
        scene_.controller, scene_.camera, 1.0);
  }

  int run(bool smoke) {
    using namespace std::chrono_literals;
    std::size_t frames = 0;
    bool running = true;
    std::uint64_t previous = flight::host_sdl::Host::ticks_nanoseconds();
    while (running) {
      SDL_Event event{};
      while (host_.poll_event(event)) {
        if (event.type == SDL_EVENT_QUIT ||
            event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
            (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
          running = false;
        }
        static_cast<void>(platform_input_.dispatch(event));
      }

      const std::uint64_t now = flight::host_sdl::Host::ticks_nanoseconds();
      const double delta_time = smoke
                                    ? 1.0 / 60.0
                                    : std::min(
                                          static_cast<double>(now - previous) /
                                              1'000'000'000.0,
                                          0.05);
      previous = now;
      flight::camera_controls::update_orbit_camera_controller(
          scene_.controller, scene_.camera, delta_time);
      render();
      gl_.present();

      ++frames;
      if (smoke && frames >= 3) running = false;
      std::this_thread::sleep_for(8ms);
    }
    return 0;
  }

 private:
  void render() {
    flight::render::prepare_scene3_drender(
        state_, scene_.root, scene_.camera, scene_.lights);
    flight::scene3d_gl::draw_gl_scene3_dshadow_map(
        state_, readonly(scene_.root), readonly(scene_.shadow_camera),
        std::optional<Readonly<flight::types::DirectionalLight>>{
            readonly(scene_.directional_light)});

    auto clear = flight::make_ref<flight::types::RenderTargetClear>();
    clear->color = flight::Array<double>{
        0x0a / 255.0, 0x0c / 255.0, 0x10 / 255.0, 1.0};
    clear->colors = std::nullopt;
    clear->depth = 1.0;
    clear->stencil = std::nullopt;
    auto pass = flight::effects_gl::begin_gl_effect_state(
        state_, pipeline_, readonly(clear), flight::String("linear"));
    gl_.depth_mask(true);
    gl_.clear_depth(1.0);
    gl_.clear(gl_.depth_buffer_bit);
    flight::scene3d_gl::render_gl_scene3_d(
        pass, readonly(scene_.root), readonly(scene_.camera),
        readonly(scene_.lights));
    flight::effects_gl::end_gl_effect_state(
        pass, pipeline_,
        flight::Array<std::variant<
            flight::Ref<flight::types::Adjustment>,
            flight::Ref<flight::types::RenderEffect>>>{});
  }

  flight::host_sdl::Host host_;
  flight::host_sdl::GlCanvas canvas_;
  flight::host_sdl::WebGl2Context gl_;
  flight::host_sdl::WebPlatformInput platform_input_;
  Scene scene_;
  flight::Ref<flight::types::GlRenderState> state_;
  flight::Ref<flight::types::GlEffectState> pipeline_;
  bool dragging_{false};
  double previous_x_{0.0};
  double previous_y_{0.0};
};

} // namespace

int main(int argc, char** argv) {
  if (argc > 2 || (argc == 2 && std::string_view(argv[1]) != "--smoke")) {
    std::cerr << "usage: flight_cpp_upstream_scene3d [--smoke]\n";
    return 2;
  }
  try {
    const bool smoke = argc == 2;
    return Scene3DExample(smoke).run(smoke);
  } catch (const std::exception& error) {
    std::cerr << "Flight SDL scene3d failed: " << error.what() << '\n';
    return 1;
  }
}
