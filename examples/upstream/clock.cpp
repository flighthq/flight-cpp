// Native Scene2D GL port of @flighthq/example-clock, preserving app.ts's graph.
#include "scene2d_host.hpp"
#include "font_host.hpp"
#include <flight/clock/clock.hpp>
#include <flight/shape/shape.hpp>
#include <flight/shape/shape_commands.hpp>
#include <flight/text/text_label.hpp>
#include <flight/render/render_proxy.hpp>
#include <flight/scene2d_gl/gl_shape_data.hpp>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
namespace {
using Frame = flight::examples::Scene2DFrame;
using Shape = flight::Ref<flight::types::Shape>;
using Label = flight::Ref<flight::types::TextLabel>;
std::string fixed(double value, int precision) {
  std::ostringstream out; out << std::fixed << std::setprecision(precision) << value; return out.str();
}
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
class ClockExample {
 public:
  ClockExample() : root_(flight::scene2d::create_display_object()), root_clock_(flight::clock::create_clock()),
      a_(flight::clock::create_child_clock(root_clock_)), b_(flight::clock::create_child_clock(root_clock_)) {
    using namespace flight::shape;
    flight::clock::set_clock_scale(a_, 1); flight::clock::set_clock_scale(b_, .5);
    for (auto& shape : shapes_) { shape = create_shape(); attach(shape); }
    auto square = shapes_[0];
    append_shape_begin_fill(square, 0x4488ccff); append_shape_rectangle(square, -30, -30, 60, 60); append_shape_end_fill(square);
    const double h = 50 * std::sqrt(3.0) / 2;
    auto triangle = shapes_[1];
    append_shape_begin_fill(triangle, 0x44cc88ff); append_shape_move_to(triangle, 0, -h/2);
    append_shape_line_to(triangle, 25, h/2); append_shape_line_to(triangle, -25, h/2); append_shape_line_to(triangle, 0, -h/2); append_shape_end_fill(triangle);
    auto circle = shapes_[2];
    append_shape_begin_fill(circle, 0xcc8844ff); append_shape_circle(circle, 0, 0, 30); append_shape_end_fill(circle);
    append_shape_line_style(circle, 3, 0xffffffff); append_shape_move_to(circle, 0, 0); append_shape_line_to(circle, 30, 0);
    const std::array<double,3> xs{400,220,580}, ys{120,300,300};
    for (std::size_t i=0; i<3; ++i) { shapes_[i]->x=xs[i]; shapes_[i]->y=ys[i]; flight::node::invalidate_node_local_transform(flight::node::native_node_2d(shapes_[i])); }
    auto lines=shapes_[3]; append_shape_line_style(lines, 2, 0x555555ff);
    append_shape_move_to(lines,400,150); append_shape_line_to(lines,220,260);
    append_shape_move_to(lines,400,150); append_shape_line_to(lines,580,260);
    label(0,"Hierarchical Clocks",20,10,24,0xffffffff);
    label(1,"Root Clock",360,65,14,0xaaaaaaff); label(2,"",320,160,12,0x8888aaff);
    label(3,"Child A (1x)",175,245,14,0xaaaaaaff); label(4,"",140,345,12,0x88aa88ff);
    label(5,"Child B (0.5x)",530,245,14,0xaaaaaaff); label(6,"",500,345,12,0xaa8844ff);
    label(7,"P root pause    1/2 toggle child pause    Left/Right child A scale    Up/Down child B scale",20,470,13,0x888888ff);
    label(8,"PAUSED",375,112,14,0xff4444ff); label(9,"PAUSED",195,292,14,0xff4444ff); label(10,"PAUSED",555,292,14,0xff4444ff);
    for (std::size_t i=8;i<11;++i) labels_[i]->visible=false;
  }
  auto root() const { return root_; }
  void frame(const Frame& frame) {
    if (frame.smoke && frame.index==0) flight::clock::pause_clock(b_);
    if (frame.input.key_pressed(SDLK_P)) toggle(root_clock_);
    if (frame.input.key_pressed(SDLK_1)) toggle(a_);
    if (frame.input.key_pressed(SDLK_2)) toggle(b_);
    if (frame.input.key_pressed(SDLK_LEFT)) flight::clock::set_clock_scale(a_,std::max(a_->scale-.25,0.0));
    if (frame.input.key_pressed(SDLK_RIGHT)) flight::clock::set_clock_scale(a_,std::min(a_->scale+.25,4.0));
    if (frame.input.key_pressed(SDLK_DOWN)) flight::clock::set_clock_scale(b_,std::max(b_->scale-.25,0.0));
    if (frame.input.key_pressed(SDLK_UP)) flight::clock::set_clock_scale(b_,std::min(b_->scale+.25,4.0));
    flight::clock::advance_clock(root_clock_,frame.delta_time*1000);
    const std::array clocks{root_clock_,a_,b_};
    for (std::size_t i=0;i<3;++i) {
      auto clock=clocks[i]; shapes_[i]->rotation=clock->elapsed*.09;
      flight::node::invalidate_node_local_transform(flight::node::native_node_2d(shapes_[i]));
      std::string info="elapsed "+fixed(clock->elapsed/1000,1)+"s  dt "+fixed(clock->delta_time,1)+"  scale "+fixed(clock->scale,2);
      if (i>0) info+="  eff "+fixed(flight::clock::get_clock_effective_scale(clock),2);
      flight::text::set_text_label_string(labels_[2+2*i],flight::String(info));
      labels_[8+i]->visible=i==0?clock->paused:flight::clock::is_clock_effectively_paused(clock);
      flight::node::invalidate_node_appearance(flight::node::native_node_2d(labels_[8+i]));
    }
    flight::text::set_text_label_string(labels_[3],flight::String("Child A ("+fixed(a_->scale,2)+"x)"));
    flight::text::set_text_label_string(labels_[5],flight::String("Child B ("+fixed(b_->scale,2)+"x)"));
  }
  void verify(const flight::examples::Scene2DRenderFrame& frame) const {
    const double elapsed=(static_cast<double>(frame.index)+1)*1000/60;
    require(std::abs(root_clock_->elapsed-elapsed)<1e-8 && std::abs(a_->elapsed-elapsed)<1e-8 && b_->elapsed==0 && b_->paused,"clock hierarchy no longer advances/pauses its authored children");
    require(std::abs(root_clock_->delta_time-1000.0/60)<1e-8 && b_->delta_time==0 && flight::clock::get_clock_effective_scale(b_)==.5,"clock delta/effective scale changed");
    require(std::abs(shapes_[0]->rotation-elapsed*.09)<1e-8 && shapes_[2]->rotation==0,"clock transforms lost actual elapsed time");
    auto runtime=flight::render_gl::get_gl_render_state_runtime(frame.state);
    require(runtime->render_proxy_sources.size()==16,"clock persistent graph lost nodes");
    for (auto shape:shapes_) {
      auto proxy=flight::render::get_render_proxy_2d(frame.state,flight::node::native_node_2d(shape));
      require(proxy && (*proxy)->source.shared_exact_object<flight::types::Shape>()==shape && (*proxy)->renderer_data.has_value(),"clock Shape owner was replaced");
      auto mesh=flight::scene2d_gl::get_gl_shape_data(*(*proxy)->renderer_data);
      require(mesh->meshes && !mesh->meshes->empty(),"clock Shape did not reach SDK mesh rendering");
    }
    require(!labels_[8]->visible && !labels_[9]->visible && labels_[10]->visible,"clock paused overlays changed");
    flight::examples::verify_text_labels(frame,labels_);
    auto pixel=[&](int x,int y,int c) { const int px=x*frame.width/800,py=frame.height-1-y*frame.height/500; return frame.pixels[static_cast<std::size_t>((py*frame.width+px)*4+c)]; };
    require(pixel(400,120,2)>180 && pixel(220,300,1)>180,"clock shape framebuffer colors are missing");
  }
 private:
  template<class T> void attach(T value) { flight::node::add_node_child(root_,flight::node::native_node_2d(value)); }
  void label(std::size_t index,const char* text,double x,double y,double size,std::uint32_t color) {
    auto value=flight::text::create_text_label(); value->data->text=flight::String(text);
    value->data->text_format->size=size; value->data->text_format->color=color; value->x=x; value->y=y;
    flight::node::invalidate_node_local_transform(flight::node::native_node_2d(value)); labels_[index]=value; attach(value);
  }
  static void toggle(const flight::Ref<flight::types::Clock>& clock) { if(clock->paused) flight::clock::resume_clock(clock); else flight::clock::pause_clock(clock); }
  flight::types::Node2D root_;
  flight::Ref<flight::types::Clock> root_clock_,a_,b_;
  std::array<Shape,4> shapes_;
  std::array<Label,11> labels_;
};
}
int main(int argc,char** argv) {
  try {
    ClockExample example;
    return flight::examples::Scene2DHost::run(argc,argv,example.root(),[&](auto& f){example.frame(f);},[&](auto& f){example.verify(f);},
      {.name="clock",.title="Flight hierarchical clocks - Scene2D GL",.width=800,.height=500},flight::examples::install_text_renderer);
  } catch(const std::exception& error) { std::cerr<<"clock example failed: "<<error.what()<<'\n'; return EXIT_FAILURE; }
}
