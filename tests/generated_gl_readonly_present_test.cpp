// The actual native present helpers must accept the readonly target views emitted by presentGlScene3D.
// Live GL output checks cover copy/linear-to-sRGB, destination presence, nominal callers and live cells.
#include <flight/host_sdl/gl_app.hpp>
#include <flight/render_gl/gl_present_render_target.hpp>
#include <flight/render_gl/gl_render_target.hpp>
#include <flight/scene3d_gl/present_gl_scene_3d.hpp>
#include <flight/types/gl_render_state.hpp>
#include <flight/types/gl_render_target.hpp>
#include <flight/types/gl_texture_render_target.hpp>

#include <array>
#include <cmath>
#include <exception>
#include <iostream>
#include <optional>
#include <type_traits>

namespace {
namespace render_gl = flight::render_gl;
namespace types = flight::types;
using SourceView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::GlTextureRenderTarget>>>>;
using DestView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::GlRenderTarget>>>>;
using PresentDest = std::optional<std::optional<DestView>>;
using State = flight::Ref<types::GlRenderState>;
static_assert(std::is_same_v<decltype(&render_gl::present_gl_render_target), void (*)(State, SourceView, PresentDest)>);
static_assert(std::is_same_v<decltype(&render_gl::draw_gl_linear_to_srgb_pass), void (*)(State, SourceView, std::optional<DestView>)>);

int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

void run() {
  flight::host_sdl::GlApplication app({.title="Flight readonly present", .width=16, .height=12, .hidden=true});
  const auto gl = app.context();
  const auto state = render_gl::create_gl_render_state(gl);
  const auto runtime = render_gl::get_gl_render_state_runtime(state);
  const auto descriptor = flight::make_structural_ref<flight::RowReadonly<flight::RowOf<flight::Ref<types::RenderTargetDescriptor>>>>(
      flight::row_field<flight::RowKey<"width">>(1.0), flight::row_field<flight::RowKey<"height">>(1.0),
      flight::row_field<flight::RowKey<"format">>(flight::String("rgba8")),
      flight::row_field<flight::RowKey<"colorSpace">>(flight::String("srgb")),
      flight::row_field<flight::RowKey<"depth">>(flight::String("none")),
      flight::row_field<flight::RowKey<"sampleCount">>(1.0));
  const auto source = render_gl::create_gl_texture_render_target(state, descriptor);
  const auto alternate = render_gl::create_gl_texture_render_target(state, descriptor);
  const auto output = render_gl::create_gl_texture_render_target(state, descriptor);
  const SourceView source_view(source);
  const DestView output_view{SourceView(output)};
  const PresentDest destination{std::optional<DestView>{output_view}};
  const auto canvas_target = flight::make_ref<types::GlRenderTarget>();
  canvas_target->gl = gl;
  canvas_target->width = gl.drawing_buffer_width();
  canvas_target->height = gl.drawing_buffer_height();
  const DestView nullable_canvas_view(canvas_target);
  const std::array<int,4> first{32,64,128,128}, second{16,48,96,192};
  const auto clear = [&](std::optional<flight::host_sdl::WebGlFramebuffer> framebuffer,
                         const std::array<int,4>& rgba, int width, int height) {
    gl.bind_framebuffer(gl.framebuffer, framebuffer);
    runtime->current_framebuffer = framebuffer;
    gl.viewport(0,0,width,height);
    gl.disable(gl.scissor_test);
    gl.color_mask(true,true,true,true);
    gl.clear_color(rgba[0]/255.0F,rgba[1]/255.0F,rgba[2]/255.0F,rgba[3]/255.0F);
    gl.clear(gl.color_buffer_bit);
  };
  clear(source->framebuffer,first,1,1);
  clear(alternate->framebuffer,second,1,1);
  const auto expect = [&](const std::array<int,4>& rgba, bool linear, const char* label) {
    flight::Uint8Array pixels(4U);
    gl.read_pixels(0,0,1,1,gl.rgba,gl.unsigned_byte,pixels);
    bool matches=true;
    for (int i=0;i<4;++i) {
      double expected=rgba[i];
      if (linear && i<3) {
        const double c=rgba[i]/255.0;
        expected=255.0*(c<0.0031308 ? 12.92*c : 1.055*std::pow(c,1.0/2.4)-0.055);
      }
      if (std::abs(static_cast<double>(pixels[i])-expected)>2.0) matches=false;
    }
    if (!matches) {
      std::cerr << "FAIL: " << label << " pixel {" << static_cast<int>(pixels[0]) << ','
                << static_cast<int>(pixels[1]) << ',' << static_cast<int>(pixels[2]) << ','
                << static_cast<int>(pixels[3]) << "}\n";
      ++failures;
    }
    check(gl.get_error()==gl.no_error,"present/readback raised a GL error");
  };
  for (bool linear : {false,true}) {
    // The existing readonly view must observe writes made later through the original owner.
    source->color_space=flight::String(linear ? "linear" : "srgb");
    clear(output->framebuffer,{0,0,0,0},1,1);
    render_gl::present_gl_render_target(state,source_view,destination);
    expect(first,linear,"readonly source and destination");
    check(runtime->current_framebuffer==std::optional{output->framebuffer},"destination was not the original framebuffer");
    const auto cached = linear ? render_gl::linear_to_srgb_programs.get(gl) : render_gl::copy_programs.get(gl);
    check(cached.has_value(),"present did not cache its actual GL program");

    clear(output->framebuffer,{0,0,0,0},1,1);
    render_gl::present_gl_render_target(state,source,destination);
    expect(first,linear,"nominal source compatibility");
    check(cached==(linear ? render_gl::linear_to_srgb_programs.get(gl) : render_gl::copy_programs.get(gl)),
          "second draw did not retain the identical cached program");

    clear(std::nullopt,{0,0,0,0},gl.drawing_buffer_width(),gl.drawing_buffer_height());
    render_gl::present_gl_render_target(state,source_view);
    expect(first,linear,"omitted destination canvas");
    clear(std::nullopt,{0,0,0,0},gl.drawing_buffer_width(),gl.drawing_buffer_height());
    render_gl::present_gl_render_target(state,source_view,PresentDest{std::optional<DestView>{}});
    expect(first,linear,"explicit null destination canvas");
    clear(std::nullopt,{0,0,0,0},gl.drawing_buffer_width(),gl.drawing_buffer_height());
    render_gl::present_gl_render_target(state,source_view,PresentDest{std::optional<DestView>{nullable_canvas_view}});
    expect(first,linear,"explicit base target with empty framebuffer cell");
    check(!runtime->current_framebuffer.has_value(), "empty framebuffer cell did not bind the canvas");
  }
  const auto original_texture=source->texture;
  source->texture=alternate->texture;
  source->color_space=flight::String("srgb");
  clear(output->framebuffer,{0,0,0,0},1,1);
  render_gl::present_gl_render_target(state,source_view,destination);
  expect(second,false,"live original texture cell");
  source->texture=original_texture;
  render_gl::destroy_gl_texture_render_target(source);
  render_gl::destroy_gl_texture_render_target(alternate);
  render_gl::destroy_gl_texture_render_target(output);
  for (const auto& teardown : runtime->context->teardowns) teardown(gl);
  runtime->context->teardowns.clear();
  (void)render_gl::copy_programs.erase(gl);
  (void)render_gl::linear_to_srgb_programs.erase(gl);
  check(gl.get_error()==gl.no_error,"real target/program cleanup raised a GL error");
}
}

int main() {
  try {
    run();
    if (failures) return 1;
    std::cout << "readonly present checks passed: eleven real draws, copy/OETF RGBA, nominal/view sources, target/canvas destinations, live owner cells and cached program identity\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "readonly present failed: " << e.what() << '\n'; return 1;
  }
}
