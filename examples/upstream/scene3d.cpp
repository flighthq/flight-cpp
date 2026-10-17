// Native SDL/OpenGL ES port of @flighthq/example-scene3d.
// Sources: .dependencies/flight/examples/packages/scene3d/src/{app,render.webgl}.ts

#include <flight/host_sdl/gl_app.hpp>
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
#include <flight/scene3d_gl/render_gl_scene_3d.hpp>
#include <flight/scene3d_gl/scene_3d_gl_pipeline.hpp>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace {

struct FrameCapture final {
  int width;
  int height;
  flight::Uint8Array rgba;
  std::size_t pixels_different_from_corner;
  std::uint8_t minimum_red;
  std::uint8_t maximum_red;
  std::uint8_t minimum_green;
  std::uint8_t maximum_green;
  std::uint8_t minimum_blue;
  std::uint8_t maximum_blue;
};

void write_ppm(const FrameCapture& capture, const std::string& path) {
  std::ofstream output(path, std::ios::binary);
  if (!output) throw std::runtime_error("could not open capture output: " + path);
  output << "P6\n" << capture.width << ' ' << capture.height << "\n255\n";
  // OpenGL's origin is at the lower left; PPM readers expect the first row at the top.
  for (int y = capture.height - 1; y >= 0; --y) {
    for (int x = 0; x < capture.width; ++x) {
      const auto offset = static_cast<std::size_t>((y * capture.width + x) * 4);
      const char rgb[] = {
          static_cast<char>(capture.rgba[offset]),
          static_cast<char>(capture.rgba[offset + 1]),
          static_cast<char>(capture.rgba[offset + 2]),
      };
      output.write(rgb, 3);
    }
  }
  if (!output) throw std::runtime_error("could not write capture output: " + path);
}

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

[[nodiscard]] flight::Array<std::optional<flight::types::Material3DValue>>
material_list(const flight::Ref<flight::types::StandardPbrMaterial>& material) {
  return {std::optional<flight::types::Material3DValue>{
      flight::types::Material3DValue{
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
  return flight::camera::create_camera_3d(readonly(options));
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
  auto camera = flight::camera::create_camera_3d(readonly(options));
  auto bounds = flight::geometry::create_aabb(-3.5, -0.8, -2.5, 3.5, 1.0, 2.5);
  flight::camera::configure_directional_shadow_camera_3d(
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

  auto root = flight::scene3d::create_node_3d(flight::types::node_3d_kind);

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
      : app_({.title = "Flight scene3d - SDL + OpenGL ES",
              .width = 800, .height = 600, .hidden = smoke}),
        canvas_(app_.canvas()),
        gl_(app_.context()),
        platform_input_(canvas_),
        scene_(create_scene()) {
    gl_.make_current();

    auto render_options = flight::make_ref<flight::types::GlRenderStateOptions>(
        *flight::scene3d_gl::gl_scene_3d_render_preset);
    render_options->pixel_ratio = 1.0;
    state_ = flight::render_gl::create_gl_render_state(gl_, readonly(render_options));

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
    ++controller_updates_;
  }

  int run(bool smoke, const std::optional<std::string>& capture_path) {
    std::size_t frames = 0;
    std::size_t verified_frames = 0;
    std::size_t maximum_coverage = 0;
    const auto draw_frame = [&]() {
      auto capture = render(
          smoke || (capture_path.has_value() && !captured_), smoke);
      if (capture.has_value()) {
        const auto& frame = capture.value();
        std::cout << "scene3d framebuffer: " << frame.width << 'x' << frame.height
                  << ", RGB ranges "
                  << static_cast<unsigned int>(frame.minimum_red) << '-'
                  << static_cast<unsigned int>(frame.maximum_red) << ", "
                  << static_cast<unsigned int>(frame.minimum_green) << '-'
                  << static_cast<unsigned int>(frame.maximum_green) << ", "
                  << static_cast<unsigned int>(frame.minimum_blue) << '-'
                  << static_cast<unsigned int>(frame.maximum_blue)
                  << ", pixels different from corner "
                  << frame.pixels_different_from_corner << '\n';
        if (capture_path.has_value() && !captured_) {
          write_ppm(frame, capture_path.value());
          captured_ = true;
        }
        const auto minimum_coverage =
            static_cast<std::size_t>(frame.width * frame.height) / 1000U;
        if (smoke && frame.pixels_different_from_corner < minimum_coverage) {
          throw std::runtime_error(
              "Scene3D produced an effectively uniform framebuffer");
        }
        if (smoke) {
          ++verified_frames;
          maximum_coverage =
              std::max(maximum_coverage, frame.pixels_different_from_corner);
        }
      }
      ++frames;
    };

    // The TypeScript authority renders once immediately after its initial controller update, then
    // schedules the time-stepped animation callback. Preserve that first-frame ordering here.
    draw_frame();
    app_.present();
    flight::host_sdl::GlLoopOptions loop;
    loop.maximum_delta_time = 0.05;
    if (smoke) { loop.fixed_delta_time = 1.0 / 60.0; loop.maximum_frames = 2; }
    app_.run([&](const flight::host_sdl::GlFrame& frame) {
      flight::camera_controls::update_orbit_camera_controller(
          scene_.controller, scene_.camera, frame.delta_time);
      ++controller_updates_;
      draw_frame();
    }, loop, [this](const SDL_Event& event) {
      static_cast<void>(platform_input_.dispatch(event));
    });
    if (smoke) {
      if (verified_frames != frames) {
        throw std::runtime_error(
            "Scene3D smoke did not verify every rendered frame");
      }
      if (subject_verified_frames_ != frames) {
        throw std::runtime_error(
            "Scene3D smoke did not drive the subject render path every frame");
      }
      if (first_render_controller_updates_ != 1 || controller_updates_ != frames) {
        throw std::runtime_error(
            "Scene3D first-frame controller ordering diverged from the authority");
      }
      std::cout << "scene3d-gl package smoke: rendered " << verified_frames
                << " verified frames, maximum non-background coverage "
                << maximum_coverage << '\n';
    }
    return 0;
  }

 private:
  void require_no_gl_error(std::string_view stage) const {
    const auto error = gl_.get_error();
    if (error != gl_.no_error) {
      throw std::runtime_error(
          "OpenGL error after " + std::string(stage) + ": " +
          std::to_string(error));
    }
  }

  [[nodiscard]] std::optional<FrameCapture> render(
      bool capture_frame, bool verify_subject) {
    if (rendered_frames_ == 0) {
      first_render_controller_updates_ = controller_updates_;
    }
    require_no_gl_error("initialization or previous frame");
    auto render_list = flight::render::prepare_scene_3d_render(
        state_, scene_.root, scene_.camera, scene_.lights);
    if (verify_subject) {
      const auto light_block = render_list->lights
                                   .template shared_exact_object<
                                       flight::types::Scene3DLightBlock>();
      if (render_list->mesh_count != 4.0 ||
          render_list->visible_meshes.size() != 4 ||
          render_list->instanced_mesh_count != 0.0 ||
          !render_list->visible_instanced_meshes.empty() || !light_block ||
          light_block->ambient_count != 1.0 ||
          light_block->directional_count != 1.0 ||
          light_block->point_count != 1.0 ||
          light_block->hemisphere_count != 0.0 ||
          light_block->spot_count != 0.0) {
        throw std::runtime_error(
            "Scene3D subject preparation did not expose the authored meshes and lights");
      }
      ++subject_verified_frames_;
    }
    flight::scene3d_gl::draw_gl_scene_3d_shadow_map(
        state_, readonly(scene_.root), readonly(scene_.shadow_camera),
        std::optional<Readonly<flight::types::DirectionalLight>>{
            readonly(scene_.directional_light)});
    require_no_gl_error("shadow-map pass");

    auto clear = flight::make_ref<flight::types::RenderTargetClear>();
    clear->color = flight::Array<double>{
        0x0a / 255.0, 0x0c / 255.0, 0x10 / 255.0, 1.0};
    clear->colors = std::nullopt;
    clear->depth = 1.0;
    clear->stencil = std::nullopt;
    auto pass = flight::effects_gl::begin_gl_effect_pass(
        state_, pipeline_, readonly(clear), flight::String("linear"));
    require_no_gl_error("effects target setup");
    gl_.depth_mask(true);
    gl_.clear_depth(1.0);
    gl_.clear(gl_.depth_buffer_bit);
    flight::scene3d_gl::render_gl_scene_3d(
        pass, readonly(scene_.root), readonly(scene_.camera),
        readonly(scene_.lights));
    require_no_gl_error("main Scene3D pass");
    flight::effects_gl::end_gl_effect_pass(
        pass, pipeline_,
        flight::Array<std::variant<
            flight::Ref<flight::types::Adjustment>,
            flight::Ref<flight::types::Effect>>>{});
    require_no_gl_error("effects presentation");
    ++rendered_frames_;

    if (!capture_frame) return std::nullopt;

    gl_.finish();
    require_no_gl_error("frame completion");
    const int width = gl_.drawing_buffer_width();
    const int height = gl_.drawing_buffer_height();
    flight::Uint8Array rgba(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
    gl_.read_pixels(
        0, 0, width, height, gl_.rgba, gl_.unsigned_byte,
        flight::ArrayBufferView(rgba));
    require_no_gl_error("framebuffer readback");

    const std::uint8_t corner_red = rgba[0];
    const std::uint8_t corner_green = rgba[1];
    const std::uint8_t corner_blue = rgba[2];
    std::size_t different = 0;
    std::uint8_t minimum_red = std::numeric_limits<std::uint8_t>::max();
    std::uint8_t maximum_red = 0;
    std::uint8_t minimum_green = std::numeric_limits<std::uint8_t>::max();
    std::uint8_t maximum_green = 0;
    std::uint8_t minimum_blue = std::numeric_limits<std::uint8_t>::max();
    std::uint8_t maximum_blue = 0;
    for (std::size_t offset = 0; offset < rgba.size(); offset += 4U) {
      const auto red = rgba[offset];
      const auto green = rgba[offset + 1U];
      const auto blue = rgba[offset + 2U];
      minimum_red = std::min(minimum_red, red);
      maximum_red = std::max(maximum_red, red);
      minimum_green = std::min(minimum_green, green);
      maximum_green = std::max(maximum_green, green);
      minimum_blue = std::min(minimum_blue, blue);
      maximum_blue = std::max(maximum_blue, blue);
      const int distance =
          std::abs(static_cast<int>(red) - static_cast<int>(corner_red)) +
          std::abs(static_cast<int>(green) - static_cast<int>(corner_green)) +
          std::abs(static_cast<int>(blue) - static_cast<int>(corner_blue));
      if (distance > 6) ++different;
    }
    return FrameCapture{
        .width = width,
        .height = height,
        .rgba = std::move(rgba),
        .pixels_different_from_corner = different,
        .minimum_red = minimum_red,
        .maximum_red = maximum_red,
        .minimum_green = minimum_green,
        .maximum_green = maximum_green,
        .minimum_blue = minimum_blue,
        .maximum_blue = maximum_blue,
    };
  }

  flight::host_sdl::GlApplication app_;
  flight::host_sdl::GlCanvas canvas_;
  flight::host_sdl::WebGl2Context gl_;
  flight::host_sdl::WebPlatformInput platform_input_;
  Scene scene_;
  flight::Ref<flight::types::GlRenderState> state_;
  flight::Ref<flight::types::GlEffectState> pipeline_;
  bool dragging_{false};
  bool captured_{false};
  std::size_t controller_updates_{0};
  std::size_t first_render_controller_updates_{0};
  std::size_t rendered_frames_{0};
  std::size_t subject_verified_frames_{0};
  double previous_x_{0.0};
  double previous_y_{0.0};
};

} // namespace

int main(int argc, char** argv) {
  bool smoke = false;
  std::optional<std::string> capture_path;
  for (int argument = 1; argument < argc; ++argument) {
    const std::string_view value(argv[argument]);
    if (value == "--smoke") {
      smoke = true;
    } else if (value == "--capture" && argument + 1 < argc) {
      capture_path = argv[++argument];
    } else {
      std::cerr << "usage: flight_cpp_upstream_scene3d [--smoke] "
                   "[--capture output.ppm]\n";
      return 2;
    }
  }
  try {
    return Scene3DExample(smoke).run(smoke, capture_path);
  } catch (const std::exception& error) {
    std::cerr << "Flight SDL scene3d failed: " << error.what() << '\n';
    return 1;
  }
}
