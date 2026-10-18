// The actual native present helpers must accept the readonly target views emitted by presentGlScene3D.
// Live GL output checks cover copy/linear-to-sRGB, destination presence, nominal callers and live cells.
#include <flight/host_sdl/gl_app.hpp>
#include <flight/render_gl/gl_present_render_target.hpp>
#include <flight/render_gl/gl_render_target.hpp>
#include <flight/scene3d_gl/present_gl_scene_3d.hpp>
#include <flight/scene3d_gl/gl_custom_shader_mesh_material_renderer.hpp>
#include <flight/scene3d_gl/gl_classic_prelude.hpp>
#include <flight/scene3d_gl/gl_pbr_prelude.hpp>
#include <flight/scene3d_gl/gl_shaded_prelude.hpp>
#include <flight/types/gl_custom_material_shader_source.hpp>
#include <flight/types/gl_render_state.hpp>
#include <flight/types/gl_render_target.hpp>

#include <array>
#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <type_traits>

namespace {
namespace render_gl = flight::render_gl;
namespace types = flight::types;
using SourceView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::GlTextureRenderTarget>>>>;
using DestView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::GlRenderTarget>>>>;
using PresentDest = std::optional<std::optional<DestView>>;
using State = flight::Ref<types::GlRenderState>;
struct ExternalProgramPadding { double application_data = 3.0; };
struct ExternalProgram : ExternalProgramPadding, types::GlMeshProgram {};
struct UnrelatedCachedOwner : flight::ReferenceEnabled { int value = 0; };
static_assert(std::is_same_v<decltype(&render_gl::present_gl_render_target), void (*)(State, SourceView, PresentDest)>);
static_assert(std::is_same_v<decltype(&render_gl::draw_gl_linear_to_srgb_pass), void (*)(State, SourceView, std::optional<DestView>)>);

int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}

void check_rederived_program_and_callbacks(const State& state) {
  namespace scene = flight::scene3d_gl;
  using ClassicRead = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::GlClassicDefineKey>>>>;
  const auto key = flight::make_ref<types::GlClassicDefineKey>();
  key->lighting_model = flight::String("lambert");
  const ClassicRead read_key(key);
  const auto first = scene::ensure_gl_classic_program(state, read_key);
  const auto second = scene::ensure_gl_classic_program(state, read_key);
  check(first == second && !first.owner_before(second) && !second.owner_before(first),
        "the classic compiler callback replaced its cached program owner");
  const auto gl = state->gl;
  const auto actual_model = gl.get_uniform_location(first->program, flight::String("u_model"));
  const auto actual_view_projection = gl.get_uniform_location(first->program, flight::String("u_viewProjection"));
  check(first->loc_model.has_value() && first->loc_view_projection.has_value() &&
            actual_model.has_value() && actual_view_projection.has_value() &&
            first->loc_model->native_name() == actual_model->native_name() &&
            first->loc_view_projection->native_name() == actual_view_projection->native_name(),
        "the classic factory lost its inherited program or live GL locations");
  using ProgramRead = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::GlMeshProgram>>>>;
  const ProgramRead program(first);
  const auto original_location = first->loc_model;
  first->loc_model = std::nullopt;
  check(!flight::row_get<flight::RowKey<"locModel">>(program).has_value(),
        "a readonly base program copied its original uniform cell");
  first->loc_model = original_location;

  using Contribution = types::GlPbrExtensionShaderContribution;
  const auto a = flight::make_ref<Contribution>();
  const auto b = flight::make_ref<Contribution>();
  a->fragment_declarations = flight::String("A0"); b->fragment_declarations = flight::String("B0");
  a->fragment_functions = flight::String("A1"); b->fragment_functions = flight::String("B1");
  a->apply_surface = flight::String("A2"); b->apply_surface = flight::String("B2");
  a->contribute_punctual = flight::String("A3"); b->contribute_punctual = flight::String("B3");
  a->contribute_ibl = flight::String("A4"); b->contribute_ibl = flight::String("B4");
  a->finalize = flight::String("A5"); b->finalize = flight::String("B5");
  const flight::Array<flight::Ref<Contribution>> contributions{a, b};
  const auto body = scene::pbr_extension_declarations + flight::String("|") +
      scene::pbr_extension_functions + flight::String("|") + scene::pbr_extension_surface +
      flight::String("|") + scene::pbr_extension_punctual + flight::String("|") +
      scene::pbr_extension_ibl + flight::String("|") + scene::pbr_extension_finalize;
  check(scene::compose_gl_pbr_extension_source(body, contributions) ==
            flight::String("A0\nB0|A1\nB1|A2\nB2|A3\nB3|A4\nB4|A5\nB5"),
        "the six PBR compiler callbacks changed contribution order or sections");
  a->fragment_declarations = flight::String("live");
  check(scene::compose_gl_pbr_extension_source(scene::pbr_extension_declarations, contributions) ==
            flight::String("live\nB0") && contributions.element(0) == a && contributions.element(1) == b,
        "the PBR callbacks copied contribution owners or hid a live source cell");
  std::weak_ptr<ExternalProgram> external_lifetime;
  {
    const auto runtime = scene::get_gl_scene_3d_runtime(state);
    const flight::String cache_key("external-application-program");
    flight::Ref<ExternalProgram> created;
    int compilations = 0;
    const std::function<flight::Ref<ExternalProgram>(flight::host_sdl::WebGl2Context)> compile =
        [&](flight::host_sdl::WebGl2Context owner_gl) {
          ++compilations;
          created = flight::make_ref<ExternalProgram>();
          created->program = scene::compile_gl_program(owner_gl, flight::String(
              "#version 300 es\nlayout(location=0) in vec3 p;\nvoid main(){gl_Position=vec4(p,1.0);}\n"),
              flight::String("#version 300 es\nprecision highp float;\nout vec4 c;\nvoid main(){c=vec4(1.0);}\n"));
          return created;
        };
    const auto external = scene::ensure_gl_scene_3d_program<flight::Ref<ExternalProgram>>(
        state, cache_key, compile);
    const auto again = scene::ensure_gl_scene_3d_program<flight::Ref<ExternalProgram>>(
        state, cache_key, compile);
    external_lifetime = external;
    const auto cached = runtime->program_cache.get(cache_key);
    check(compilations == 1 && external == created && external == again &&
              !external.owner_before(created) && !created.owner_before(external) &&
              cached.has_value() && cached->as<ExternalProgram>() == external &&
              !cached->as<types::GlMeshProgram>(),
          "the generic program cache lost its exact arbitrary derived owner");
    const auto base = cached ? cached->declared_base_as<types::GlMeshProgram>() : nullptr;
    check(base && base.get() == static_cast<types::GlMeshProgram*>(external.get()) &&
              !base.owner_before(external) && !external.owner_before(base) &&
              !cached->declared_base_as<types::GlClassicProgram>(),
          "the generic cache inferred a type or replaced its declared base owner");
    const auto ibl = flight::make_ref<types::GlScene3DIbl>();
    ibl->brdf_lut = gl.create_texture().value();
    ibl->irradiance_cube = gl.create_texture().value();
    ibl->prefiltered_cube = gl.create_texture().value();
    const auto bake_framebuffer = gl.create_framebuffer().value();
    const auto environment_cube = gl.create_texture().value();
    runtime->ibl = ibl;
    runtime->ibl_bake_framebuffer = bake_framebuffer;
    runtime->environment_source_cube = environment_cube;
    runtime->environment_source_cube_face_versions = flight::Array<double>{1.0, 2.0};
    runtime->active_mesh_program = base;
    scene::destroy_gl_scene_3d_runtime(state);
    check(runtime->program_cache.size() == 0 && !runtime->active_mesh_program &&
              scene::get_gl_scene_3d_runtime(state) == runtime,
          "teardown did not retain the same cleared runtime for lazy reuse");
    check(!static_cast<bool>(external->program) && !static_cast<bool>(first->program) &&
              external->program.native_name() == 0 && first->program.native_name() == 0,
          "actual scene teardown did not delete every cached GL program");
    check(!runtime->ibl && !runtime->ibl_bake_framebuffer && !runtime->environment_source_cube &&
              runtime->environment_source_cube_face_versions.empty() &&
              !static_cast<bool>(ibl->brdf_lut) && !static_cast<bool>(ibl->irradiance_cube) &&
              !static_cast<bool>(ibl->prefiltered_cube) && !static_cast<bool>(bake_framebuffer) &&
              !static_cast<bool>(environment_cube),
          "full scene teardown did not clear and delete populated IBL/environment resources");
  }
  check(external_lifetime.expired(), "a destroyed generic cache retained its derived program");
  const auto invalid_runtime = scene::get_gl_scene_3d_runtime(state);
  const flight::String invalid_key("unrelated-cache-owner");
  invalid_runtime->program_cache.set(invalid_key, flight::make_ref<UnrelatedCachedOwner>());
  bool rejected = false;
  try { scene::destroy_gl_scene_3d_runtime(state); }
  catch (const flight::Error&) { rejected = true; }
  check(rejected && invalid_runtime->program_cache.has(invalid_key),
        "scene teardown accepted or silently discarded an unrelated cache owner");
  static_cast<void>(invalid_runtime->program_cache.erase(invalid_key));
  scene::destroy_gl_scene_3d_runtime(state);
  check(gl.get_error() == gl.no_error, "rederived program checks raised a real GL error");
}

void check_custom_source_handoff(const State& state) {
  namespace scene = flight::scene3d_gl;
  using CustomRead = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<types::GlCustomMaterialShaderSource>>>>;
  using CustomWrite = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<types::GlCustomMaterialShaderSource>>>>;
  static_assert(!std::is_constructible_v<CustomWrite, CustomRead>);
  const auto registries = render_gl::get_gl_render_state_runtime(state)->registries;
  const auto original_registry = registries->custom_material_shaders;
  const flight::String key("readonly-helper-source");
  check(!scene::get_gl_custom_material_shader_source(state, key).has_value(),
        "an absent registered custom source did not retain its null result");
  const flight::String vertex(R"GLSL(#version 300 es
precision highp float;
layout(location=0) in vec3 a_position;
uniform mat4 u_viewProjection;
uniform mat4 u_model;
uniform mat3 u_normalMatrix;
uniform vec3 u_cameraPosition;
void main() { gl_Position = u_viewProjection * u_model * vec4(a_position, 1.0); }
)GLSL");
  const flight::String fragment(R"GLSL(#version 300 es
precision highp float;
out vec4 fragColor;
void main() { fragColor = vec4(1.0); }
)GLSL");
  const auto first = flight::make_ref<types::GlCustomMaterialShaderSource>(
      types::GlCustomMaterialShaderSource{.fragment = fragment, .vertex = vertex});
  const auto second = flight::make_ref<types::GlCustomMaterialShaderSource>(
      types::GlCustomMaterialShaderSource{.fragment = fragment, .vertex = vertex});
  const CustomRead first_view(first), second_view(second);
  scene::register_gl_custom_material_shader(state, key, first_view);
  const auto first_registry = registries->custom_material_shaders;
  auto retained = scene::get_gl_custom_material_shader_source(state, key);
  check(retained.has_value(), "the public custom source lookup lost its registered value");
  if (!retained) { registries->custom_material_shaders = original_registry; return; }
  const auto retained_owner = retained->shared_object();
  check(retained->shared_owner() == first_view.shared_owner() &&
            retained_owner.get() == first.get() && !retained_owner.owner_before(first) &&
            !first.owner_before(retained_owner),
        "the public custom source getter replaced the readonly row or native owner");
  first->vertex = vertex + flight::String("\n// live original source\n");
  check(flight::row_get<flight::RowKey<"vertex">>(retained.value()) == first->vertex,
        "a retained readonly source did not see its original live vertex cell");
  scene::register_gl_custom_material_shader(state, key, second_view);
  const auto current = scene::get_gl_custom_material_shader_source(state, key);
  check(current.has_value() && current->shared_owner() == second_view.shared_owner() &&
            current->shared_object().get() == second.get(),
        "source re-registration did not preserve last-write lookup and the second owner");
  check(retained->shared_object().get() == first.get() &&
            flight::row_get<flight::RowKey<"vertex">>(retained.value()) == first->vertex &&
            first_registry.get(key).value() == first && !original_registry.get(key).has_value(),
        "source re-registration changed an earlier view or persistent registry snapshot");
  registries->custom_material_shaders = original_registry;
}

void run() {
  flight::host_sdl::GlApplication app({.title="Flight readonly present", .width=16, .height=12, .hidden=true});
  const auto gl = app.context();
  const auto state = render_gl::create_gl_render_state(gl);
  check_rederived_program_and_callbacks(state);
  check_custom_source_handoff(state);
  const auto runtime = render_gl::get_gl_render_state_runtime(state);
  const auto context = runtime->context;
  flight::Ref<types::EntityRuntime> context_base = context;
  check(context.get() == context_base.get() && !context.owner_before(context_base) &&
            !context_base.owner_before(context) && &context->binding == &context_base->binding &&
            &context->uid == &context_base->uid,
        "GL context heritage keeps the initialized owner and inherited runtime cells");
  const auto previous_uid = context->uid;
  context_base->uid = flight::String("gl-context-runtime-alias");
  check(context->uid == context_base->uid, "GL context reads the live inherited uid cell");
  context_base->uid = previous_uid;
  auto identity = flight::node::transform3d_detail::fresh_matrix();
  flight::Ref<types::Entity> matrix_base = identity;
  check(!identity->entity_runtime_key && identity->m.size() == 16 &&
            matrix_base.get() == identity.get() &&
            &matrix_base->entity_runtime_key == &identity->entity_runtime_key,
        "fresh inherited Matrix4 retains one owner and absent runtime");
  for (std::size_t i = 0; i < 16; ++i)
    check(identity->m[i] == (i % 5 == 0 ? 1.0F : 0.0F), "fresh Matrix4 remains identity");
  const auto material = flight::scene3d_gl::default_material;
  flight::Ref<types::Material> material_base = material;
  check(material_base.get() == material.get() &&
            &material_base->kind == &material->kind &&
            material->kind == types::standard_material_kind && material->alpha_cutoff == 0.5 &&
            material->alpha_mode == flight::String("opaque") &&
            material->blend_mode == types::blend_mode->normal && !material->double_sided,
        "default material preserves its inherited kind and authored fields");
  const auto descriptor = flight::make_structural_ref<flight::RowReadonly<flight::RowOf<flight::Ref<types::RenderTargetDescriptor>>>>(
      flight::row_field<flight::RowKey<"width">>(1.0), flight::row_field<flight::RowKey<"height">>(1.0),
      flight::row_field<flight::RowKey<"format">>(flight::String("rgba8")),
      flight::row_field<flight::RowKey<"colorSpace">>(flight::String("srgb")),
      flight::row_field<flight::RowKey<"depth">>(flight::String("none")),
      flight::row_field<flight::RowKey<"sampleCount">>(1.0));
  const auto source = render_gl::create_gl_texture_render_target(state, descriptor);
  const auto alternate = render_gl::create_gl_texture_render_target(state, descriptor);
  const auto output = render_gl::create_gl_texture_render_target(state, descriptor);
  const auto pass = render_gl::acquire_gl_render_pass_handle(gl, state, output);
  flight::Ref<types::Entity> pass_base = pass;
  check(!pass->entity_runtime_key && pass->state == state &&
            &pass_base->entity_runtime_key == &pass->entity_runtime_key &&
            pass->target->framebuffer == std::optional{output->framebuffer},
        "fresh pass handle preserves inherited runtime absence and its native payload");
  render_gl::release_gl_render_pass_handle(gl, pass);
  const auto reused_pass = render_gl::acquire_gl_render_pass_handle(gl, state, output);
  check(reused_pass == pass && !reused_pass.owner_before(pass_base) &&
            !pass_base.owner_before(reused_pass),
        "pooled pass handle replaced its original allocation or control block");
  (void)render_gl::pass_handle_pool.erase(gl);
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
  const auto previous_axes = output->requested_axes;
  render_gl::resize_gl_texture_render_target(state, output, 3.2, 2.1);
  check(output->width == 4 && output->height == 3 && output->requested_axes != previous_axes &&
            previous_axes->width == 1 && previous_axes->height == 1 &&
            output->requested_axes->format == previous_axes->format &&
            output->requested_axes->color_attachments == previous_axes->color_attachments &&
            output->requested_axes->color_formats.size() == previous_axes->color_formats.size() &&
            output->requested_axes->sample_count == previous_axes->sample_count &&
            output->requested_axes->depth == previous_axes->depth &&
            output->requested_axes->color_space == previous_axes->color_space,
        "resize preserves inherited descriptor axes and the retained prior snapshot");
  for (std::size_t i = 0; i < previous_axes->color_formats.size(); ++i)
    check(output->requested_axes->color_formats.element(static_cast<double>(i)) ==
              previous_axes->color_formats.element(static_cast<double>(i)),
          "resize preserves each authored color format");
  clear(output->framebuffer, first, 4, 3);
  expect(first, false, "resized inherited descriptor backing");
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
