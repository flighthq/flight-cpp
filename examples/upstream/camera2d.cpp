// Native SDL/OpenGL ES port of @flighthq/example-camera2d.
// Source: .dependencies/flight/examples/packages/camera2d/src/app.ts

#include "sdl_host.hpp"

#include <flight/host_sdl/gl_app.hpp>
#include <flight/host_sdl/web_platform.hpp>
#include <flight/node/native_node2d_access.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/scene2d/sprite.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/bitmap/bitmap.hpp>
#include <flight/texture/texture.hpp>
#include <flight/path/tessellate_stroke_path.hpp>
#include <flight/render/render_proxy.hpp>
#include <flight/render/renderer.hpp>
#include <flight/render_gl/gl_render_state.hpp>
#include <flight/render_gl/gl_render_pass.hpp>
#include <flight/render_gl/gl_render_target.hpp>
#include <flight/scene2d_gl/gl_node_2d.hpp>
#include <flight/scene2d_gl/gl_mesh_shape_renderer.hpp>
#include <flight/scene2d_gl/gl_sprite.hpp>
#include <flight/scene2d_gl/gl_standard_material.hpp>
#include <SDL3/SDL_surface.h>
#include <fstream>
#include <unordered_set>


#include <flight/camera/camera2d.hpp>
#include <flight/camera/parallax.hpp>
#include <flight/camera/view_matrix.hpp>
#include <flight/camera/visible_bounds.hpp>
#include <flight/camera/zoom.hpp>
#include <flight/camera_controls/follow.hpp>
#include <flight/geometry/matrix.hpp>
#include <flight/geometry/rectangle.hpp>
#include <flight/geometry/vector2.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using flight::examples::Canvas;
struct CameraInput final {
  std::unordered_set<SDL_Keycode> keys;
  double pointer_x{}, pointer_y{}, wheel_y{};
  bool key_down(SDL_Keycode key) const { return keys.contains(key); }
};
struct Frame final { const CameraInput& input; double delta_time; std::size_t index; bool smoke; };
using flight::examples::Point;

constexpr double world_width = 2400.0;
constexpr double world_height = 1800.0;
constexpr double player_size = 24.0;
constexpr double player_speed = 300.0;

class Random final {
 public:
  [[nodiscard]] double next() {
    state_ = (state_ * 16807) % 2147483647;
    return static_cast<double>(state_) / 2147483647.0;
  }

 private:
  std::int64_t state_{42};
};

[[nodiscard]] std::uint32_t hsl(double hue) {
  constexpr double saturation = 0.6;
  constexpr double lightness = 0.5;
  const double chroma = (1.0 - std::abs(2.0 * lightness - 1.0)) * saturation;
  const double intermediate = chroma * (1.0 - std::abs(std::fmod(hue / 60.0, 2.0) - 1.0));
  const double match = lightness - chroma / 2.0;
  double red{};
  double green{};
  double blue{};
  if (hue < 60.0) {
    red = chroma;
    green = intermediate;
  } else if (hue < 120.0) {
    red = intermediate;
    green = chroma;
  } else if (hue < 180.0) {
    green = chroma;
    blue = intermediate;
  } else if (hue < 240.0) {
    green = intermediate;
    blue = chroma;
  } else if (hue < 300.0) {
    red = intermediate;
    blue = chroma;
  } else {
    red = chroma;
    blue = intermediate;
  }
  const auto channel = [match](double value) {
    return static_cast<std::uint32_t>(std::lround((value + match) * 255.0));
  };
  return (channel(red) << 16U) | (channel(green) << 8U) | channel(blue);
}

[[nodiscard]] std::string fixed(double value, int precision) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(precision) << value;
  return stream.str();
}

struct Landmark final {
  Point position;
  double width{};
  double height{};
  std::uint32_t color{};
  bool circle{};
};

struct Star final {
  Point position;
  double radius{};
  double brightness{};
};

struct Cloud final {
  Point position;
  double width{};
  double height{};
};

struct Mountain final {
  double x{};
  double peak_height{};
  double width{};
};

template <typename Value>
[[nodiscard]] auto readonly_row(const std::shared_ptr<Value>& value) {
  return flight::structural_ref_cast<
      flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Value>>>>>(
      flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Value>>>>(value));
}

[[nodiscard]] flight::Ref<flight::types::Camera2D> create_example_camera() {
  auto options = flight::make_ref<flight::types::Camera2DOptions>(
      flight::types::Camera2DOptions{
          .rotation = std::nullopt,
          .x = world_width * 0.5,
          .y = world_height * 0.5,
          .zoom = 1.0,
      });
  return flight::camera::create_camera_2d(readonly_row(options));
}

class CameraExample final {
 public:
  CameraExample()
      : camera_(create_example_camera()),
        world_bounds_(flight::geometry::create_rectangle(0.0, 0.0, world_width, world_height)),
        follow_options_(flight::make_ref<flight::types::Camera2DFollowOptions>(
            flight::types::Camera2DFollowOptions{
                .deadzone_half_height = 40.0,
                .deadzone_half_width = 60.0,
                .smooth_time = 0.15,
                .world_bounds = world_bounds_,
            })),
        view_matrix_(flight::geometry::create_matrix()),
        visible_bounds_(flight::geometry::create_rectangle()),
        stars_offset_(flight::geometry::create_vector2()),
        mountains_offset_(flight::geometry::create_vector2()),
        clouds_offset_(flight::geometry::create_vector2()) {
    Random random;
    for (int index = 0; index < 60; ++index) {
      const bool circle = random.next() <= 0.5;
      const double size = 20.0 + random.next() * 80.0;
      landmarks_.push_back({
          .position = {100.0 + random.next() * (world_width - 200.0), 100.0 + random.next() * (world_height - 200.0)},
          .width = size,
          .height = circle ? size : 20.0 + random.next() * 80.0,
          .color = hsl(std::floor(random.next() * 360.0)),
          .circle = circle,
      });
    }
    for (int index = 0; index < 80; ++index) {
      stars_.push_back({
          .position = {random.next() * 800.0, random.next() * 600.0},
          .radius = 1.0 + random.next() * 2.0,
          .brightness = 0.3 + random.next() * 0.7,
      });
    }
    for (int index = 0; index < 12; ++index) {
      clouds_.push_back({
          .position = {random.next() * 800.0, 50.0 + random.next() * 200.0},
          .width = 80.0 + random.next() * 120.0,
          .height = 30.0 + random.next() * 40.0,
      });
    }
    for (int index = 0; index < 8; ++index) {
      mountains_.push_back({
          .x = random.next() * 800.0,
          .peak_height = 100.0 + random.next() * 200.0,
          .width = 120.0 + random.next() * 180.0,
      });
    }
    build_scene();
  }

  void frame(const Frame& frame) {
    update_player(frame);
    update_zoom(frame);
    update_camera(frame.delta_time);
    if (frame.smoke) verify_smoke(frame.index);
    update_scene();
  }

  int run(int argc, char** argv) {
    bool smoke = false; std::optional<std::string> capture;
    for (int i=1;i<argc;++i) {
      std::string_view arg(argv[i]);
      if (arg == "--smoke") smoke = true;
      else if (arg == "--capture" && i+1<argc) capture = argv[++i];
      else throw std::runtime_error("usage: camera2d [--smoke] [--capture frame.ppm]");
    }
    flight::host_sdl::GlApplication app({
        .title = "Flight Camera2D - Scene2D GL", .width = 800, .height = 600, .hidden = smoke});
    auto gl = app.context();
    auto options = flight::make_ref<flight::types::GlRenderStateOptions>();
    options->pixel_ratio = 1; options->scene_graph_sync_policy = flight::String("requiresInvalidation");
    options->blend_realizations = flight::render_gl::standard_gl_blend_realizations;
    options->texture_resolvers = flight::render_gl::standard_gl_texture_resolvers;
    options->stroke_tessellator = flight::types::StrokeTessellator(flight::path::tessellate_stroke_path);
    auto state = flight::render_gl::create_gl_render_state(gl, readonly_row(options));
    flight::render::register_node_renderer(state, flight::types::display_object_kind, flight::scene2d_gl::gl_scene_2d_renderer);
    flight::render::register_node_renderer(state, flight::types::shape_kind, flight::scene2d_gl::gl_mesh_shape_renderer);
    flight::render::register_node_renderer(state, flight::types::sprite_kind, flight::scene2d_gl::gl_sprite_renderer);
    flight::scene2d_gl::register_gl_standard_material(state);
    auto screen = flight::render_gl::create_gl_screen_render_target(gl);
    auto clear = flight::make_ref<flight::types::RenderTargetClear>();
    clear->color = flight::Array<double>{0x1a/255.0, 0x1a/255.0, 0x2e/255.0, 1};
    // SDL paints only this separate HUD asset; every world/parallax pixel comes from Flight GL.
    Canvas hud("Camera2D HUD raster asset", true, 800, 96);
    using Texture = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;
    Texture hud_target(SDL_CreateTexture(hud.native_renderer(), SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, 800, 96), SDL_DestroyTexture);
    if (!hud_target) throw std::runtime_error(SDL_GetError());
    CameraInput input; std::size_t frames=0, maximum_coverage=0;
    flight::host_sdl::GlLoopOptions loop;
    if (smoke) { loop.fixed_delta_time = 1.0/60.0; loop.maximum_frames = 3; }
    app.run([&](const flight::host_sdl::GlFrame& timing) {
      frame(Frame{input,timing.delta_time,timing.index,smoke});
      update_hud(hud, hud_target.get()); gl.make_current();
      if (!flight::render::prepare_scene_2d_render(state, root_)) throw std::runtime_error("Camera2D authored scene was not prepared");
      auto pass = flight::render_gl::begin_gl_render_pass(state, screen, readonly_row(clear));
      flight::scene2d_gl::render_gl_scene_2d(pass, root_);
      flight::render_gl::end_gl_render_pass(pass);
      if (gl.get_error() != gl.no_error) throw std::runtime_error("Camera2D subject GL path raised an error");
      if (smoke) verify_scene(state);
      if (smoke || (capture && frames == 0)) {
        gl.finish(); flight::Uint8Array pixels(800U*600U*4U);
        gl.read_pixels(0,0,800,600,gl.rgba,gl.unsigned_byte,pixels);
        if (gl.get_error() != gl.no_error) throw std::runtime_error("Camera2D GL readback failed");
        std::size_t coverage=0;
        for (std::size_t i=0;i<pixels.size();i+=4) if (pixels[i]!=0x1a || pixels[i+1]!=0x1a || pixels[i+2]!=0x2e) ++coverage;
        const auto center = static_cast<std::size_t>((600-1-300)*800+400)*4;
        if (smoke && (coverage<5000 || pixels[center]<240 || pixels[center+1]<180 || pixels[center+1]>220 || pixels[center+2]<35 || pixels[center+2]>70)) throw std::runtime_error("Camera2D GL framebuffer lacks the authored yellow player or scene coverage");
        maximum_coverage=std::max(maximum_coverage,coverage);
        if (capture && frames == 0) {
          std::ofstream out(*capture,std::ios::binary); out << "P6\n800 600\n255\n";
          for (int y=599;y>=0;--y) for (int x=0;x<800;++x) { auto i=static_cast<std::size_t>((y*800+x)*4); const char rgb[]{static_cast<char>(pixels[i]),static_cast<char>(pixels[i+1]),static_cast<char>(pixels[i+2])}; out.write(rgb,3); }
          if (!out) throw std::runtime_error("Camera2D capture write failed");
        }
      }
      ++frames;
      input.wheel_y = 0;
    }, loop, [&](const SDL_Event& event) {
      if (event.type == SDL_EVENT_KEY_DOWN) input.keys.insert(event.key.key);
      if (event.type == SDL_EVENT_KEY_UP) input.keys.erase(event.key.key);
      if (event.type == SDL_EVENT_MOUSE_MOTION) { input.pointer_x=event.motion.x; input.pointer_y=event.motion.y; }
      if (event.type == SDL_EVENT_MOUSE_WHEEL) { input.wheel_y += event.wheel.y; input.pointer_x=event.wheel.mouse_x; input.pointer_y=event.wheel.mouse_y; }
    });
    if (smoke) {
      if (subject_verified_frames_ != 3 || follow_updates_ != 3 || shape_count_ != 66 || sprite_count_ != 45) throw std::runtime_error("Camera2D GL did not verify all authored frames and nodes");
      std::cout << "scene2d-gl package smoke: rendered " << subject_verified_frames_ << " verified frames, " << node_count_ << " nodes, maximum non-background coverage " << maximum_coverage << '\n';
    }
    flight::render::dispose_scene_2d_render(state, root_);
    return EXIT_SUCCESS;
  }

 private:
  void update_player(const Frame& frame) {
    double horizontal{};
    double vertical{};
    if (frame.input.key_down(SDLK_LEFT) || frame.input.key_down(SDLK_A)) horizontal -= 1.0;
    if (frame.input.key_down(SDLK_RIGHT) || frame.input.key_down(SDLK_D)) horizontal += 1.0;
    if (frame.input.key_down(SDLK_UP) || frame.input.key_down(SDLK_W)) vertical -= 1.0;
    if (frame.input.key_down(SDLK_DOWN) || frame.input.key_down(SDLK_S)) vertical += 1.0;
    const double length = std::hypot(horizontal, vertical);
    if (length != 0.0) {
      horizontal /= length;
      vertical /= length;
    }
    player_.x = std::clamp(player_.x + horizontal * player_speed * frame.delta_time, player_size, world_width - player_size);
    player_.y = std::clamp(player_.y + vertical * player_speed * frame.delta_time, player_size, world_height - player_size);
  }

  void update_zoom(const Frame& frame) {
    if (frame.input.wheel_y == 0.0) return;
    const double direction = frame.input.wheel_y > 0.0 ? 1.0 : -1.0;
    const double next = std::clamp(
        camera_->zoom + direction * 0.1 * camera_->zoom,
        0.25,
        4.0);
    flight::camera::zoom_camera_2d_at_screen_point(
        camera_, 800.0, 600.0, frame.input.pointer_x, frame.input.pointer_y, next);
  }

  void update_camera(double delta_time) {
    flight::camera_controls::update_camera_2d_follow(
        camera_, 800.0, 600.0, player_.x, player_.y, delta_time, follow_options_);
    ++follow_updates_;
    flight::camera::get_camera_2d_view_matrix(camera_, 800.0, 600.0, view_matrix_);
    flight::camera::get_camera_2d_visible_bounds(camera_, 800.0, 600.0, visible_bounds_);
    flight::camera::get_camera_2d_parallax_point(camera_, 800.0, 600.0, 0.1, stars_offset_);
    flight::camera::get_camera_2d_parallax_point(camera_, 800.0, 600.0, 0.4, mountains_offset_);
    flight::camera::get_camera_2d_parallax_point(camera_, 800.0, 600.0, 0.6, clouds_offset_);
  }

  [[nodiscard]] static bool near(double actual, double expected) {
    return std::abs(actual - expected) <= 1e-9;
  }

  void verify_smoke(std::size_t frame_index) {
    if (!near(camera_->x, world_width * 0.5) ||
        !near(camera_->y, world_height * 0.5) || !near(camera_->zoom, 1.0) ||
        follow_updates_ != frame_index + 1 ||
        !near(view_matrix_->a, 1.0) || !near(view_matrix_->b, 0.0) ||
        !near(view_matrix_->c, 0.0) || !near(view_matrix_->d, 1.0) ||
        !near(view_matrix_->tx, -800.0) || !near(view_matrix_->ty, -600.0) ||
        !near(visible_bounds_->x, 800.0) || !near(visible_bounds_->y, 600.0) ||
        !near(visible_bounds_->width, 800.0) || !near(visible_bounds_->height, 600.0) ||
        !near(stars_offset_->x, -120.0) || !near(stars_offset_->y, -90.0) ||
        !near(mountains_offset_->x, -480.0) || !near(mountains_offset_->y, -360.0) ||
        !near(clouds_offset_->x, -720.0) || !near(clouds_offset_->y, -540.0)) {
      throw std::runtime_error(
          "camera2d smoke did not observe generated follow, view, bounds, and parallax output");
    }
    if (smoke_probe_complete_) return;

    auto zoom_probe = create_example_camera();
    flight::camera::zoom_camera_2d_at_screen_point(zoom_probe, 800.0, 600.0, 200.0, 150.0, 2.0);
    if (!near(zoom_probe->x, 1100.0) || !near(zoom_probe->y, 825.0) ||
        !near(zoom_probe->zoom, 2.0)) {
      throw std::runtime_error("camera2d smoke did not observe generated anchored zoom output");
    }

    auto follow_probe = create_example_camera();
    flight::camera_controls::update_camera_2d_follow(
        follow_probe, 800.0, 600.0, 1500.0, 1100.0, 1.0 / 60.0, follow_options_);
    if (!(follow_probe->x > world_width * 0.5 && follow_probe->x < 1500.0 &&
          follow_probe->y > world_height * 0.5 && follow_probe->y < 1100.0)) {
      throw std::runtime_error("camera2d smoke did not observe generated follow output");
    }
    smoke_probe_complete_ = true;
  }


  using Node = flight::types::Node2D;
  using Shape = flight::Ref<flight::types::Shape>;
  using Sprite = flight::Ref<flight::types::Sprite>;

  void attach(Node parent, Node child) { flight::node::add_node_child(parent, child); ++node_count_; }
  void attach(Node parent, Shape child) { attach(parent, flight::node::native_node_2d(child)); }
  void attach(Node parent, Sprite child) { attach(parent, flight::node::native_node_2d(child)); }
  Shape shape(Node parent) { auto out = flight::shape::create_shape(); attach(parent, out); ++shape_count_; shapes_.push_back(out); return out; }
  Sprite sprite(Node parent, flight::Ref<flight::types::Texture2D> texture, double x, double y, double w, double h, double alpha = 1) {
    auto out = flight::scene2d::create_sprite();
    out->data->texture = flight::types::Texture{texture};
    auto size = flight::geometry::create_vector2(); flight::texture::get_texture_view_size(size, texture);
    out->x = x; out->y = y; out->scale_x = w / size->x; out->scale_y = h / size->y; out->alpha = alpha;
    attach(parent, out); ++sprite_count_; return out;
  }
  static flight::Ref<flight::types::Texture2D> bitmap_texture(flight::Ref<flight::types::Bitmap> bitmap) {
    auto opts = flight::make_ref<flight::types::CreateTexture2DOptions>(); opts->source = bitmap;
    return flight::texture::create_texture_2d(readonly_row(opts));
  }
  static void fill(Shape value, std::uint32_t color, double alpha = 1) { flight::shape::append_shape_begin_fill(value, static_cast<double>(color), alpha); }
  static void end(Shape value) { flight::shape::append_shape_end_fill(value); }
  static void polygon(Shape value, flight::Array<double> points) { flight::shape::append_shape_polygon(value, points); }
  static void move(Node node, double x, double y) {
    flight::row_set<flight::RowKey<"x">>(node, x); flight::row_set<flight::RowKey<"y">>(node, y);
    flight::node::invalidate_node_local_transform(node);
  }
  void build_scene() {
    root_ = flight::scene2d::create_display_object(); node_count_ = 1;
    auto layer = [&] { auto out = flight::scene2d::create_display_object(); attach(root_, out); return out; };
    stars_container_ = layer(); mountains_container_ = layer(); clouds_container_ = layer(); world_container_ = layer(); hud_container_ = layer();
    auto star_shape = shape(stars_container_); stars_shape_ = star_shape;
    for (const auto& star : stars_) { fill(star_shape, 0xffffd0ffU, star.brightness); flight::shape::append_shape_circle(star_shape, star.position.x, star.position.y, star.radius); end(star_shape); }
    auto mountain_shape = shape(mountains_container_); mountains_shape_ = mountain_shape;
    for (const auto& mt : mountains_) { fill(mountain_shape, 0x3c5064ffU, 0.6); polygon(mountain_shape, {mt.x-mt.width/2, 600, mt.x, 600-mt.peak_height, mt.x+mt.width/2, 600}); end(mountain_shape); }
    auto cloud_shape = shape(clouds_container_); clouds_shape_ = cloud_shape;
    for (const auto& cloud : clouds_) { fill(cloud_shape, 0xc8d2e6ffU, 0.4); flight::shape::append_shape_ellipse(cloud_shape, cloud.position.x, cloud.position.y, cloud.width/2, cloud.height/2); end(cloud_shape); }

    // One pixel Bitmap stretched by the genuine Sprite graph supplies the authored
    // solid grid strokes. Its 44 instances share one uploaded texture and GL batch.
    auto grid = flight::scene2d::create_display_object(); attach(world_container_, grid);
    grid_bitmap_ = flight::bitmap::create_bitmap(1, 1, static_cast<double>(0x64788cffU));
    auto grid_texture = bitmap_texture(grid_bitmap_);
    for (double x = 0; x <= world_width; x += 100) sprite(grid, grid_texture, x-0.5, 0, 1, world_height, 0.15);
    for (double y = 0; y <= world_height; y += 100) sprite(grid, grid_texture, 0, y-0.5, world_width, 1, 0.15);
    auto border = shape(world_container_);
    flight::shape::append_shape_line_style(border, 3, static_cast<double>(0xc85050ffU), 0.5);
    flight::shape::append_shape_rectangle(border, 0, 0, world_width, world_height); end(border);
    for (const auto& lm : landmarks_) {
      auto out = shape(world_container_); fill(out, lm.color, 0.7);
      if (lm.circle) flight::shape::append_shape_circle(out, lm.position.x, lm.position.y, lm.width/2);
      else flight::shape::append_shape_rectangle(out, lm.position.x-lm.width/2, lm.position.y-lm.height/2, lm.width, lm.height);
      end(out);
    }
    player_shape_ = shape(world_container_); bounds_shape_ = shape(world_container_);
    hud_bitmap_ = flight::bitmap::create_bitmap(800, 96);
    hud_bitmap_->alpha_type = flight::String("premultiplied");
    hud_sprite_ = sprite(hud_container_, bitmap_texture(hud_bitmap_), 0, 0, 800, 96);
  }
  void update_scene() {
    move(stars_container_, stars_offset_->x, stars_offset_->y);
    move(mountains_container_, mountains_offset_->x, mountains_offset_->y);
    move(clouds_container_, clouds_offset_->x, clouds_offset_->y);
    flight::row_set<flight::RowKey<"scaleX">>(world_container_, view_matrix_->a);
    flight::row_set<flight::RowKey<"skewY">>(world_container_, view_matrix_->b);
    flight::row_set<flight::RowKey<"skewX">>(world_container_, view_matrix_->c);
    flight::row_set<flight::RowKey<"scaleY">>(world_container_, view_matrix_->d);
    move(world_container_, view_matrix_->tx, view_matrix_->ty);
    flight::shape::clear_shape_commands(player_shape_); fill(player_shape_, 0xffcc33ffU);
    flight::Array<double> points{player_.x, player_.y-player_size, player_.x+player_size*0.8, player_.y+player_size*0.6, player_.x-player_size*0.8, player_.y+player_size*0.6};
    polygon(player_shape_, points); end(player_shape_);
    flight::shape::append_shape_line_style(player_shape_, 2, static_cast<double>(0xcc9900ffU), 1);
    polygon(player_shape_, points); end(player_shape_);
    flight::shape::clear_shape_commands(bounds_shape_);
    flight::shape::append_shape_line_style(bounds_shape_, 2, static_cast<double>(0x00c864ffU), 0.6);
    flight::shape::append_shape_rectangle(bounds_shape_, visible_bounds_->x, visible_bounds_->y, visible_bounds_->width, visible_bounds_->height); end(bounds_shape_);
  }
  void update_hud(Canvas& canvas, SDL_Texture* target) {
    auto renderer = canvas.native_renderer();
    if (!SDL_SetRenderTarget(renderer, target)) throw std::runtime_error(SDL_GetError());
    canvas.begin(0); draw_hud(canvas);
    using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
    Surface pixels(SDL_RenderReadPixels(renderer, nullptr), SDL_DestroySurface);
    if (!pixels) throw std::runtime_error(SDL_GetError());
    Surface rgba(SDL_ConvertSurface(pixels.get(), SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
    if (!rgba) throw std::runtime_error(SDL_GetError());
    if (SDL_MUSTLOCK(rgba.get()) && !SDL_LockSurface(rgba.get())) throw std::runtime_error(SDL_GetError());
    for (int y = 0; y < 96; ++y) {
      auto* row = static_cast<std::uint8_t*>(rgba->pixels) + y * rgba->pitch;
      for (int x = 0; x < 800*4; ++x) hud_bitmap_->data[static_cast<std::size_t>(y*800*4+x)] = row[x];
    }
    if (SDL_MUSTLOCK(rgba.get())) SDL_UnlockSurface(rgba.get());
    if (!SDL_SetRenderTarget(renderer, nullptr)) throw std::runtime_error(SDL_GetError());
    flight::bitmap::invalidate_bitmap(hud_bitmap_);
  }
  void verify_scene(flight::Ref<flight::types::GlRenderState> state) {
    auto runtime = flight::render_gl::get_gl_render_state_runtime(state);
    auto proxy = flight::render::get_render_proxy_2d(state, flight::node::native_node_2d(player_shape_));
    if (runtime->render_proxy_sources.size() != node_count_ || !proxy || (*proxy)->renderer != flight::scene2d_gl::gl_mesh_shape_renderer || (*proxy)->source.shared_exact_object<flight::types::Shape>() != player_shape_ || !near((*proxy)->transform_2d->tx, view_matrix_->tx) || !near((*proxy)->transform_2d->ty, view_matrix_->ty)) throw std::runtime_error("Camera2D GL did not prepare the authored graph and original player owner");
    auto meshes = flight::scene2d_gl::get_gl_shape_data(*(*proxy)->renderer_data)->meshes;
    auto uploaded = runtime->context->texture_source_premultiplied_texture_cache.get(flight::types::TextureSourceValue{grid_bitmap_});
    if (!meshes || meshes->empty() || !runtime->context->shape_mesh_resources || !runtime->context->quad_batch_resources || !runtime->context->quad_batch_resources.value()->writer_instance_buffer || !uploaded || runtime->quad_batch_writer_count != 0) throw std::runtime_error("Camera2D GL did not tessellate the player, upload the grid texture and flush actual Sprite batches");
    for (const auto& source : shapes_) {
      auto prepared = flight::render::get_render_proxy_2d(state, flight::node::native_node_2d(source));
      if (!prepared || (*prepared)->renderer != flight::scene2d_gl::gl_mesh_shape_renderer ||
          !(*prepared)->renderer_data) throw std::runtime_error("Camera2D GL omitted an authored Shape proxy");
      auto data = flight::scene2d_gl::get_gl_shape_data(*(*prepared)->renderer_data);
      if (!data->meshes || data->meshes->empty()) throw std::runtime_error("Camera2D GL omitted authored fill/stroke meshes");
    }
    const auto verify_layer = [&](Node layer, auto offset) {
      auto prepared = flight::render::get_render_proxy_2d(state, layer);
      if (!prepared || !near((*prepared)->transform_2d->tx, offset->x) ||
          !near((*prepared)->transform_2d->ty, offset->y)) throw std::runtime_error("Camera2D GL parallax graph transform drifted");
    };
    verify_layer(stars_container_, stars_offset_);
    verify_layer(mountains_container_, mountains_offset_);
    verify_layer(clouds_container_, clouds_offset_);
    auto root_children = flight::node::native_node_2d_runtime(root_)->children;
    if (!root_children || root_children->size() != 5 || (*root_children)[0] != stars_container_ ||
        (*root_children)[1] != mountains_container_ || (*root_children)[2] != clouds_container_ ||
        (*root_children)[3] != world_container_ || (*root_children)[4] != hud_container_)
      throw std::runtime_error("Camera2D GL changed authored layer order");
    const auto mesh_count = [&](Shape source) {
      auto prepared = flight::render::get_render_proxy_2d(state, flight::node::native_node_2d(source)).value();
      return flight::scene2d_gl::get_gl_shape_data(*prepared->renderer_data)->meshes.value().size();
    };
    if (mesh_count(stars_shape_) != 80 || mesh_count(mountains_shape_) != 8 ||
        mesh_count(clouds_shape_) != 12 || mesh_count(player_shape_) != 2)
      throw std::runtime_error("Camera2D GL changed authored parallax or player geometry");
    ++subject_verified_frames_;
  }
  void draw_hud(Canvas& canvas) const {
    canvas.fill_rectangle(8.0, 8.0, 260.0, 80.0, 0x000000ffU, 0.5);
    canvas.text(
        "Camera2D: (" + fixed(camera_->x, 0) + ", " + fixed(camera_->y, 0) + ")  Zoom: " + fixed(camera_->zoom, 2),
        16.0,
        16.0,
        0xffffffffU,
        13.0);
    canvas.text("Player: (" + fixed(player_.x, 0) + ", " + fixed(player_.y, 0) + ")", 16.0, 34.0, 0xffffffffU, 13.0);
    canvas.text("WASD/Arrows: move  Scroll: zoom", 16.0, 52.0, 0xffffffffU, 13.0);
    canvas.text("Green = visible bounds  Red = world border", 16.0, 70.0, 0xffffffffU, 13.0);
  }

  Node root_, stars_container_, mountains_container_, clouds_container_, world_container_, hud_container_;
  Shape player_shape_, bounds_shape_, stars_shape_, mountains_shape_, clouds_shape_;
  std::vector<Shape> shapes_;
  Sprite hud_sprite_;
  flight::Ref<flight::types::Bitmap> grid_bitmap_, hud_bitmap_;
  std::size_t node_count_{}, shape_count_{}, sprite_count_{}, subject_verified_frames_{};
  std::vector<Landmark> landmarks_;
  std::vector<Star> stars_;
  std::vector<Cloud> clouds_;
  std::vector<Mountain> mountains_;
  Point player_{world_width * 0.5, world_height * 0.5};
  flight::Ref<flight::types::Camera2D> camera_;
  flight::Ref<flight::types::Rectangle> world_bounds_;
  flight::Ref<flight::types::Camera2DFollowOptions> follow_options_;
  flight::Ref<flight::types::Matrix> view_matrix_;
  flight::Ref<flight::types::Rectangle> visible_bounds_;
  flight::Ref<flight::types::Vector2> stars_offset_;
  flight::Ref<flight::types::Vector2> mountains_offset_;
  flight::Ref<flight::types::Vector2> clouds_offset_;
  std::size_t follow_updates_{};
  bool smoke_probe_complete_{};
};

} // namespace

int main(int argc, char** argv) {
  try {
    CameraExample example;
    return example.run(argc, argv);
  } catch (const std::exception& error) {
    std::cerr << "camera2d example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
