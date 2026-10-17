// Native Scene2D GL port of @flighthq/example-spatial.
// Source: .dependencies/flight/examples/packages/spatial/src/app.ts

#include "scene2d_host.hpp"
#include "font_host.hpp"
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>
#include <flight/render/render_proxy.hpp>
#include <flight/scene2d_gl/gl_shape_data.hpp>
#include <array>

#include <flight/spatial/spatial_index.hpp>
#include <flight/spatial/uniform_grid.hpp>
#include <flight/types/spatial.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using Frame = flight::examples::Scene2DFrame;
using Shape = flight::Ref<flight::types::Shape>;
using Label = flight::Ref<flight::types::TextLabel>;

constexpr double width = 800.0;
constexpr double height = 500.0;
constexpr std::uint32_t color_idle = 0x4488ccffU;
constexpr std::uint32_t color_overlap = 0xcc4444ffU;
constexpr std::uint32_t color_point = 0x44cc44ffU;
constexpr std::uint32_t color_ray = 0xcccc44ffU;
constexpr std::uint32_t color_region = 0x44ccccffU;

struct Object final {
  Shape shape = flight::shape::create_shape();
  double id{};
  flight::Ref<flight::types::SpatialAabb2D> bounds;
  double velocity_x{};
  double velocity_y{};
};

class Random final {
 public:
  explicit Random(bool deterministic) : deterministic_(deterministic) {
    if (!deterministic_) engine_.seed(std::random_device{}());
  }

  [[nodiscard]] double range(double minimum, double maximum) {
    double amount{};
    if (deterministic_) {
      state_ = state_ * 1664525U + 1013904223U;
      amount = static_cast<double>(state_) / 4294967296.0;
    } else {
      amount = std::generate_canonical<double, 53>(engine_);
    }
    return minimum + amount * (maximum - minimum);
  }

 private:
  bool deterministic_{};
  std::uint32_t state_{0x05a171a1U};
  std::mt19937_64 engine_{};
};

enum class Mode { pairs, point, ray, region };

[[nodiscard]] std::string_view mode_name(Mode mode) {
  switch (mode) {
    case Mode::pairs: return "Pairs";
    case Mode::point: return "Point";
    case Mode::ray: return "Ray";
    case Mode::region: return "Region";
  }
  return "Pairs";
}

class SpatialExample final {
 public:
  explicit SpatialExample(bool capture_mode)
      : capture_mode_(capture_mode),
        active_mode_(capture_mode ? Mode::region : Mode::pairs),
        index_(flight::spatial::create_spatial_index_2d(
            flight::spatial::create_uniform_grid_spatial_backend_2d(100.0))) {
    using namespace flight::shape;
    append_shape_line_style(grid_,1,0x263954ff,0.65);
    for(double x=100;x<width;x+=100) { append_shape_move_to(grid_,x,0); append_shape_line_to(grid_,x,height); }
    for(double y=100;y<height;y+=100) { append_shape_move_to(grid_,0,y); append_shape_line_to(grid_,width,y); }
    attach(grid_);
    Random random(capture_mode);
    for (int index = 0; index < 20; ++index) {
      const double object_width = random.range(30.0, 80.0);
      const double object_height = random.range(20.0, 60.0);
      const double x = random.range(10.0, width - object_width - 10.0);
      const double y = random.range(40.0, height - object_height - 50.0);
      const bool moving = index < 5;
      auto bounds = flight::make_ref<flight::types::SpatialAabb2D>(
          flight::types::SpatialAabb2D{
              .min_x = x,
              .min_y = y,
              .max_x = x + object_width,
              .max_y = y + object_height,
          });
      objects_.push_back(Object{
          .id = static_cast<double>(index),
          .bounds = bounds,
          .velocity_x = moving ? random.range(-30.0, 30.0) : 0.0,
          .velocity_y = moving ? random.range(-30.0, 30.0) : 0.0,
      });
      attach(objects_.back().shape);
      if (flight::spatial::insert_spatial_object_2d(index_, static_cast<double>(index), bounds)) {
        ++inserted_count_;
      }
    }
    attach(overlay_);
    label(0,"Click: point query | R: ray query | Q: region query",10,6,13,0x888888ff);
    label(1,"Mode: Pairs",10,height-30,14,0xccccccff);
  }

  auto root() const { return root_; }
  void frame(const Frame& frame) {
    handle_input(frame);
    const std::size_t updated_count = update(std::min(frame.delta_time, 0.05));
    std::vector<bool> highlighted(objects_.size());
    int results{};
    if (active_mode_ == Mode::pairs) {
      flight::spatial::query_spatial_pairs_2d(index_, pairs_out_);
      results = static_cast<int>(pairs_out_.size());
      for (const auto& pair : pairs_out_) {
        highlight(pair->a, highlighted);
        highlight(pair->b, highlighted);
      }
    } else {
      if (active_mode_ == Mode::point) {
        flight::spatial::query_spatial_point_2d(index_, mouse_x_, mouse_y_, ids_out_);
      } else if (active_mode_ == Mode::ray) {
        flight::spatial::query_spatial_ray_2d(
            index_, 0.0, height / 2.0, mouse_x_, mouse_y_ - height / 2.0, ids_out_);
      } else {
        const auto region = flight::make_ref<flight::types::SpatialAabb2D>(
            flight::types::SpatialAabb2D{
                .min_x = mouse_x_ - 50.0,
                .min_y = mouse_y_ - 50.0,
                .max_x = mouse_x_ + 50.0,
                .max_y = mouse_y_ + 50.0,
            });
        flight::spatial::query_spatial_region_2d(index_, region, ids_out_);
      }
      results = static_cast<int>(ids_out_.size());
      for (const double id : ids_out_) {
        highlight(id, highlighted);
      }
    }
    if (frame.smoke) verify_smoke(updated_count);
    draw(active_mode_, highlighted, results, mouse_x_, mouse_y_);
  }

 private:
  void handle_input(const Frame& frame) {
    if (capture_mode_) return;
    mouse_x_ = frame.input.pointer_x;
    mouse_y_ = frame.input.pointer_y;
    using Kind=flight::examples::SceneInputEvent::Kind;
    for(const auto& event:frame.input.events) {
      if(event.kind==Kind::pointer_down) active_mode_=Mode::point;
      if(event.kind==Kind::pointer_up || event.kind==Kind::pointer_cancel) active_mode_=Mode::pairs;
      if(event.kind==Kind::key_down && event.key==SDLK_R) active_mode_=Mode::ray;
      if(event.kind==Kind::key_down && event.key==SDLK_Q) active_mode_=Mode::region;
      if(event.kind==Kind::key_up && (event.key==SDLK_R || event.key==SDLK_Q)) active_mode_=Mode::pairs;
    }
  }

  [[nodiscard]] std::size_t update(double delta_time) {
    std::size_t updated_count{};
    for (std::size_t index = 0; index < 5; ++index) {
      auto& object = objects_[index];
      auto& bounds = *object.bounds;
      const double object_width = bounds.max_x - bounds.min_x;
      const double object_height = bounds.max_y - bounds.min_y;
      bounds.min_x += object.velocity_x * delta_time;
      bounds.min_y += object.velocity_y * delta_time;
      bounds.max_x = bounds.min_x + object_width;
      bounds.max_y = bounds.min_y + object_height;
      if (bounds.min_x < 0.0) {
        bounds.min_x = 0.0;
        bounds.max_x = object_width;
        object.velocity_x = std::abs(object.velocity_x);
      }
      if (bounds.max_x > width) {
        bounds.max_x = width;
        bounds.min_x = width - object_width;
        object.velocity_x = -std::abs(object.velocity_x);
      }
      if (bounds.min_y < 0.0) {
        bounds.min_y = 0.0;
        bounds.max_y = object_height;
        object.velocity_y = std::abs(object.velocity_y);
      }
      if (bounds.max_y > height) {
        bounds.max_y = height;
        bounds.min_y = height - object_height;
        object.velocity_y = -std::abs(object.velocity_y);
      }
      if (flight::spatial::update_spatial_object_2d(index_, object.id, object.bounds)) {
        ++updated_count;
      }
    }
    return updated_count;
  }

  [[nodiscard]] static bool overlaps(
      const flight::types::SpatialAabb2D& a,
      const flight::types::SpatialAabb2D& b) {
    return a.min_x < b.max_x && a.max_x > b.min_x &&
           a.min_y < b.max_y && a.max_y > b.min_y;
  }

  [[nodiscard]] static bool shares_grid_cell(
      const flight::types::SpatialAabb2D& a,
      const flight::types::SpatialAabb2D& b) {
    constexpr double cell_size = 100.0;
    const double shared_min_x = std::max(
        std::floor(a.min_x / cell_size),
        std::floor(b.min_x / cell_size));
    const double shared_max_x = std::min(
        std::floor(a.max_x / cell_size),
        std::floor(b.max_x / cell_size));
    const double shared_min_y = std::max(
        std::floor(a.min_y / cell_size),
        std::floor(b.min_y / cell_size));
    const double shared_max_y = std::min(
        std::floor(a.max_y / cell_size),
        std::floor(b.max_y / cell_size));
    return shared_min_x <= shared_max_x && shared_min_y <= shared_max_y;
  }

  void verify_smoke(std::size_t updated_count) const {
    if (inserted_count_ != objects_.size() || updated_count != 5 ||
        active_mode_ != Mode::region || mouse_x_ != width * 0.5 ||
        mouse_y_ != height * 0.5) {
      throw std::runtime_error("spatial smoke did not preserve capture state or index updates");
    }

    const flight::types::SpatialAabb2D region{
        .min_x = mouse_x_ - 50.0,
        .min_y = mouse_y_ - 50.0,
        .max_x = mouse_x_ + 50.0,
        .max_y = mouse_y_ + 50.0,
    };
    std::size_t expected_region_count{};
    for (const auto& object : objects_) {
      if (overlaps(*object.bounds, region)) {
        ++expected_region_count;
        if (!ids_out_.includes(object.id)) {
          throw std::runtime_error("spatial region query omitted an overlapping indexed object");
        }
      }
    }
    if (ids_out_.size() != expected_region_count) {
      throw std::runtime_error("spatial region query returned non-overlapping indexed objects");
    }

    flight::Array<double> probe_ids;
    const auto& first = *objects_.front().bounds;
    const double first_x = (first.min_x + first.max_x) * 0.5;
    const double first_y = (first.min_y + first.max_y) * 0.5;
    flight::spatial::query_spatial_point_2d(index_, first_x, first_y, probe_ids);
    if (!probe_ids.includes(objects_.front().id)) {
      throw std::runtime_error("spatial point query did not observe the generated index");
    }
    flight::spatial::query_spatial_ray_2d(index_, 0.0, 0.0, first_x, first_y, probe_ids);
    if (!probe_ids.includes(objects_.front().id)) {
      throw std::runtime_error("spatial ray query did not observe the generated index");
    }

    flight::Array<flight::Ref<flight::types::SpatialPair>> probe_pairs;
    flight::spatial::query_spatial_pairs_2d(index_, probe_pairs);
    std::set<std::pair<std::size_t, std::size_t>> candidate_pairs;
    for (const auto& pair : probe_pairs) {
      const auto first_index = static_cast<std::size_t>(pair->a);
      const auto second_index = static_cast<std::size_t>(pair->b);
      if (pair->a != static_cast<double>(first_index) ||
          pair->b != static_cast<double>(second_index) ||
          first_index >= objects_.size() || second_index >= objects_.size() ||
          first_index >= second_index ||
          !shares_grid_cell(*objects_[first_index].bounds, *objects_[second_index].bounds)) {
        throw std::runtime_error("spatial pair query returned an invalid broadphase candidate");
      }
      candidate_pairs.emplace(first_index, second_index);
    }
    if (candidate_pairs.size() != probe_pairs.size()) {
      throw std::runtime_error("spatial pair query returned a duplicate broadphase candidate");
    }
    for (std::size_t first_index = 0; first_index < objects_.size(); ++first_index) {
      for (std::size_t second_index = first_index + 1;
           second_index < objects_.size();
           ++second_index) {
        const bool expected = shares_grid_cell(
            *objects_[first_index].bounds,
            *objects_[second_index].bounds);
        const bool present = candidate_pairs.contains({first_index, second_index});
        if (expected != present) {
          throw std::runtime_error("spatial pair query did not match the indexed grid cells");
        }
      }
    }
  }

  void highlight(double id, std::vector<bool>& highlighted) const {
    const auto index = static_cast<std::size_t>(id);
    if (index < highlighted.size()) highlighted[index] = true;
  }

  template<class T> void attach(T object) { flight::node::add_node_child(root_,flight::node::native_node_2d(object)); }
  void label(std::size_t i,const char* text,double x,double y,double size,std::uint32_t color) {
    auto value=flight::text::create_text_label(); value->data->text=flight::String(text);
    value->data->text_format->size=size; value->data->text_format->color=color; value->x=x; value->y=y;
    flight::node::invalidate_node_local_transform(flight::node::native_node_2d(value)); labels_[i]=value; attach(value);
  }
  void draw(Mode mode,const std::vector<bool>& highlighted,int results,double mouse_x,double mouse_y) {
    using namespace flight::shape;
    const auto hit_color=mode==Mode::point?color_point:mode==Mode::ray?color_ray:mode==Mode::region?color_region:color_overlap;
    for(std::size_t i=0;i<objects_.size();++i) {
      auto shape=objects_[i].shape; const auto& b=*objects_[i].bounds;
      clear_shape_commands(shape); append_shape_begin_fill(shape,highlighted[i]?hit_color:color_idle,.5);
      append_shape_line_style(shape,1,0x335577ff); append_shape_rectangle(shape,b.min_x,b.min_y,b.max_x-b.min_x,b.max_y-b.min_y); append_shape_end_fill(shape);
      flight::node::invalidate_node_appearance(flight::node::native_node_2d(shape));
    }
    clear_shape_commands(overlay_);
    if(mode==Mode::point) {
      append_shape_line_style(overlay_,2,color_point);
      append_shape_move_to(overlay_,mouse_x-8,mouse_y); append_shape_line_to(overlay_,mouse_x+8,mouse_y);
      append_shape_move_to(overlay_,mouse_x,mouse_y-8); append_shape_line_to(overlay_,mouse_x,mouse_y+8);
    } else if(mode==Mode::ray) {
      const double dx=mouse_x,dy=mouse_y-height/2,length=std::hypot(dx,dy);
      if(length>0) { append_shape_line_style(overlay_,1,color_ray); append_shape_move_to(overlay_,0,height/2);
        append_shape_line_to(overlay_,dx/length*width*1.5,height/2+dy/length*width*1.5); }
    } else if(mode==Mode::region) {
      append_shape_begin_fill(overlay_,color_region,.15); append_shape_line_style(overlay_,1,color_region);
      append_shape_rectangle(overlay_,mouse_x-50,mouse_y-50,100,100); append_shape_end_fill(overlay_);
    }
    flight::node::invalidate_node_appearance(flight::node::native_node_2d(overlay_));
    flight::text::set_text_label_string(labels_[1],flight::String("Mode: "+std::string(mode_name(mode))+"  Hits: "+std::to_string(results)));
  }
 public:
  void verify(const flight::examples::Scene2DRenderFrame& frame) const {
    if(flight::render_gl::get_gl_render_state_runtime(frame.state)->render_proxy_sources.size()!=25)
      throw std::runtime_error("spatial GL lost the authored 25-node graph");
    auto check=[&](Shape shape,std::size_t count) {
      auto proxy=flight::render::get_render_proxy_2d(frame.state,flight::node::native_node_2d(shape));
      if(!proxy || (*proxy)->source.shared_exact_object<flight::types::Shape>()!=shape || !(*proxy)->renderer_data)
        throw std::runtime_error("spatial GL lost the original Shape owner");
      auto data=flight::scene2d_gl::get_gl_shape_data(*(*proxy)->renderer_data);
      if(!data->meshes || data->meshes->size()!=count || data->mesh_version!=shape->entity_runtime_key.value()->local_content_id)
        throw std::runtime_error("spatial GL did not tessellate current grid/object/query geometry");
    };
    check(grid_,1); for(const auto& object:objects_) check(object.shape,2); check(overlay_,2);
    flight::examples::verify_text_labels(frame,labels_);
    std::size_t query_pixels=0;
    for(std::size_t i=0;i<frame.pixels.size();i+=4)
      if(frame.pixels[i]<90 && frame.pixels[i+1]>180 && frame.pixels[i+2]>180) ++query_pixels;
    if(query_pixels<100) throw std::runtime_error("spatial GL framebuffer lacks the actual highlighted query border");
  }
 private:
  flight::types::Node2D root_ = flight::scene2d::create_display_object();
  Shape grid_=flight::shape::create_shape(),overlay_=flight::shape::create_shape();
  std::array<Label,2> labels_;
  std::vector<Object> objects_;
  bool capture_mode_{};
  Mode active_mode_{Mode::pairs};
  double mouse_x_{width * 0.5};
  double mouse_y_{height * 0.5};
  std::size_t inserted_count_{};
  flight::Ref<flight::types::SpatialIndex2D> index_;
  flight::Array<flight::Ref<flight::types::SpatialPair>> pairs_out_;
  flight::Array<double> ids_out_;
};

} // namespace

int main(int argc, char** argv) {
  try {
    bool capture_mode=false;
    for(int i=1;i<argc;++i) if(std::string_view(argv[i])=="--smoke") capture_mode=true;
    SpatialExample example(capture_mode);
    return flight::examples::Scene2DHost::run(argc,argv,example.root(),
        [&](const Frame& f){example.frame(f);},[&](const auto& f){example.verify(f);},
        {.name="spatial",.title="Flight spatial index - Scene2D GL",.width=800,.height=500,.maximum_delta_time=.05},flight::examples::install_text_renderer);
  } catch (const std::exception& error) {
    std::cerr << "spatial example failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
