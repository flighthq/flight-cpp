// Native Scene2D GL port of @flighthq/example-motionpath.
// Source: .dependencies/flight/examples/packages/motionpath/src/app.ts
#include "scene2d_host.hpp"
#include "font_host.hpp"
#include <flight/motionpath/motion_path.hpp>
#include <flight/path/path.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>
#include <flight/render/render_proxy.hpp>
#include <flight/scene2d_gl/gl_shape_data.hpp>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
namespace {
using Frame=flight::examples::Scene2DFrame;
using Shape=flight::Ref<flight::types::Shape>;
using Label=flight::Ref<flight::types::TextLabel>;
void require(bool value,const char* reason) { if(!value) throw std::runtime_error(reason); }
class MotionPathExample {
 public:
  MotionPathExample() : root_(flight::scene2d::create_display_object()),path_(flight::path::create_path()),
      point_(flight::geometry::create_vector2()),tangent_(flight::geometry::create_vector2()) {
    using namespace flight::shape;
    flight::path::append_path_move_to(path_,100,400);
    flight::path::append_path_cubic_curve_to(path_,250,100,350,100,400,250);
    flight::path::append_path_cubic_curve_to(path_,450,400,550,400,700,100);
    driver_=flight::motionpath::create_motion_path(path_,speed_,mode_);
    for(auto& shape:shapes_) shape=create_shape();
    auto track=shapes_[0]; append_shape_line_style(track,2,0x4488aaff); append_shape_move_to(track,100,400);
    append_shape_cubic_curve_to(track,250,100,350,100,400,250);
    append_shape_cubic_curve_to(track,450,400,550,400,700,100); attach(track);
    constexpr std::array<std::array<double,2>,7> controls{{{100,400},{250,100},{350,100},{400,250},{450,400},{550,400},{700,100}}};
    for(std::size_t i=0;i<controls.size();++i) {
      auto dot=shapes_[i+1]; auto [x,y]=controls[i];
      append_shape_begin_fill(dot,0x335566ff); append_shape_move_to(dot,x,y-4); append_shape_line_to(dot,x+4,y);
      append_shape_line_to(dot,x,y+4); append_shape_line_to(dot,x-4,y); append_shape_line_to(dot,x,y-4); append_shape_end_fill(dot); attach(dot);
    }
    if(flight::motionpath::get_motion_path_position(driver_,point_,tangent_)) draw_arrow();
    attach(shapes_[8]);
    label(0,"Motion Path",20,10,24,0xffffffff);
    label(1,"Speed: 150",20,445,16,0xccccccff); label(2,"Mode: loop (1/2/3)",20,465,16,0xccccccff);
    label(3,"Progress: 0%",250,445,16,0xccccccff); label(4,"+/- speed    1 clamp  2 loop  3 pingpong",250,465,14,0x888888ff);
  }
  auto root() const { return root_; }
  void frame(const Frame& frame) {
    if(frame.smoke && frame.index==0) flight::motionpath::set_motion_path_progress(driver_,.42);
    for(const auto& event:frame.input.events) {
      if(event.kind!=flight::examples::SceneInputEvent::Kind::key_down) continue;
      if(event.key==SDLK_PLUS||event.key==SDLK_EQUALS) { speed_=std::min(speed_+25,500.0); driver_->speed=speed_; update(1,"Speed: "+std::to_string(static_cast<int>(speed_))); }
      if(event.key==SDLK_MINUS||event.key==SDLK_UNDERSCORE) { speed_=std::max(speed_-25,25.0); driver_->speed=speed_; update(1,"Speed: "+std::to_string(static_cast<int>(speed_))); }
      if(event.key==SDLK_1) set_mode("clamp");
      if(event.key==SDLK_2) set_mode("loop");
      if(event.key==SDLK_3) set_mode("pingpong");
    }
    flight::motionpath::update_motion_path(driver_,frame.delta_time);
    sampled_=flight::motionpath::get_motion_path_position(driver_,point_,tangent_);
    if(sampled_) draw_arrow();
    update(3,"Progress: "+std::to_string(static_cast<int>(flight::motionpath::get_motion_path_progress(driver_)*100))+"%");
  }
  void verify(const flight::examples::Scene2DRenderFrame& frame) const {
    require(driver_->path==path_ && driver_->loop_mode==flight::String("loop") && driver_->direction==1 && driver_->speed==150 && driver_->length>600,"motionpath driver lost its authored path/mode/speed");
    const double expected=.42*driver_->length+(static_cast<double>(frame.index)+1)*2.5;
    require(std::abs(driver_->distance-expected)<1e-7 && std::abs(flight::motionpath::get_motion_path_progress(driver_)-expected/driver_->length)<1e-10,"motionpath fixed-step progress changed");
    require(sampled_ && point_->x>100 && point_->x<700 && point_->y>100 && point_->y<400 && std::hypot(tangent_->x,tangent_->y)>0,"motionpath did not sample the actual cubic path");
    auto arrow=shapes_[8];
    require(arrow->x==point_->x && arrow->y==point_->y && std::abs(arrow->rotation-std::atan2(tangent_->y,tangent_->x)*180/std::numbers::pi)<1e-8,"motionpath arrow transform differs from the generated position/tangent");
    require(flight::render_gl::get_gl_render_state_runtime(frame.state)->render_proxy_sources.size()==15,"motionpath persistent graph lost nodes");
    for(auto shape:shapes_) {
      auto proxy=flight::render::get_render_proxy_2d(frame.state,flight::node::native_node_2d(shape));
      require(proxy && (*proxy)->source.shared_exact_object<flight::types::Shape>()==shape && (*proxy)->renderer_data.has_value(),"motionpath Shape owner lost");
      auto data=flight::scene2d_gl::get_gl_shape_data(*(*proxy)->renderer_data);
      require(data->meshes && !data->meshes->empty() && data->mesh_version==shape->entity_runtime_key.value()->local_content_id,"motionpath current geometry did not reach the GL mesh cache");
    }
    flight::examples::verify_text_labels(frame,labels_);
    const int x=static_cast<int>(point_->x*frame.width/800),y=frame.height-1-static_cast<int>(point_->y*frame.height/500);
    const auto pixel=static_cast<std::size_t>((y*frame.width+x)*4);
    require(frame.pixels[pixel]>220 && frame.pixels[pixel+1]<140 && frame.pixels[pixel+2]<110,"motionpath GL framebuffer lacks the actual moving arrow");
  }
 private:
  template<class T> void attach(T value) { flight::node::add_node_child(root_,flight::node::native_node_2d(value)); }
  void label(std::size_t i,const char* text,double x,double y,double size,std::uint32_t color) {
    auto out=flight::text::create_text_label(); out->data->text=flight::String(text); out->data->text_format->size=size; out->data->text_format->color=color;
    out->x=x; out->y=y; flight::node::invalidate_node_local_transform(flight::node::native_node_2d(out)); labels_[i]=out; attach(out);
  }
  void update(std::size_t i,const std::string& text) { flight::text::set_text_label_string(labels_[i],flight::String(text)); }
  void set_mode(const char* mode) { mode_=flight::String(mode); driver_->loop_mode=mode_; update(2,"Mode: "+mode_.to_utf8()+" (1/2/3)"); }
  void draw_arrow() {
    using namespace flight::shape;
    auto arrow=shapes_[8]; clear_shape_commands(arrow); arrow->x=point_->x; arrow->y=point_->y;
    arrow->rotation=flight::motionpath::get_motion_path_heading(driver_)*180/std::numbers::pi;
    flight::node::invalidate_node_local_transform(flight::node::native_node_2d(arrow));
    append_shape_begin_fill(arrow,0xff6644ff); append_shape_move_to(arrow,20,0); append_shape_line_to(arrow,-10,-8);
    append_shape_line_to(arrow,-10,8); append_shape_line_to(arrow,20,0); append_shape_end_fill(arrow);
  }
  flight::types::Node2D root_;
  flight::Ref<flight::types::Path> path_;
  flight::Ref<flight::types::MotionPath> driver_;
  flight::Ref<flight::types::Vector2> point_,tangent_;
  double speed_=150;
  flight::String mode_{"loop"};
  bool sampled_{};
  std::array<Shape,9> shapes_;
  std::array<Label,5> labels_;
};
}
int main(int argc,char** argv) {
  try {
    MotionPathExample example;
    return flight::examples::Scene2DHost::run(argc,argv,example.root(),[&](auto& f){example.frame(f);},[&](auto& f){example.verify(f);},
      {.name="motionpath",.title="Flight motion path - Scene2D GL",.width=800,.height=500,.maximum_delta_time=std::numeric_limits<double>::max()},flight::examples::install_text_renderer);
  } catch(const std::exception& error) { std::cerr<<"motionpath example failed: "<<error.what()<<'\n'; return EXIT_FAILURE; }
}
