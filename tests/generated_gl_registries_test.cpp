#include <flight/render_gl/gl_render_state.hpp>
#include <flight/scene3d_gl/enable_gl_scene_3d_custom_shader_guards.hpp>
#include <flight/scene3d_gl/gl_mesh_program.hpp>
#include <flight/types/gl_pbr_program.hpp>

#include <iostream>

namespace {
bool check_uniform_type_names() {
  // The actual helper only reads static enums. Use the native host value carrier directly;
  // it needs no SDL surface, mock context, program, or GL operation for these reads.
  const flight::host_sdl::WebGl2Context gl;
  const struct {
    double type;
    const char* expected;
  } cases[] = {
      {gl.float_mat4, "mat4"},
      {gl.float_mat3, "mat3"},
      {gl.float_mat2, "mat2"},
      {gl.float_vec4, "vec4"},
      {gl.float_vec3, "vec3"},
      {gl.float_vec2, "vec2"},
      {gl.float_, "float"},
      {gl.texture_2_d, "gl-type-3553"},
      {0, "gl-type-0"},
      {-1.5, "gl-type--1.5"},
  };
  for (const auto& entry : cases) {
    if (flight::scene3d_gl::gl_uniform_type_name(gl, entry.type) != flight::String(entry.expected)) {
      std::cerr << "Uniform type " << entry.type << " did not map to " << entry.expected << '\n';
      return false;
    }
  }
  std::cout << "Actual uniform type helper passed seven native enum names and three default cases\n";
  return true;
}
}  // namespace

int main() {
  using namespace flight;
  auto options = make_ref<types::GlRenderStateOptions>();
  const auto view = StructuralRef<RowReadonly<RowOf<Ref<types::GlRenderStateOptions>>>>(options);
  const auto empty = render_gl::build_gl_render_registries(view);
  if (!empty->node_renderers.empty() || empty->passes.has_value() ||
      empty->canvas_shape_commands.has_value() ||
      !std::holds_alternative<Undefined>(empty->color_adjustments)) return 1;

  auto blend = make_ref<types::GlBlendRealization>(types::GlBlendRealization{
      .src = String("ONE"), .dst = String("ONE_MINUS_SRC_ALPHA")});
  options->blend_realizations.emplace();
  options->blend_realizations->set(String("first"), blend);
  options->blend_realizations->set(String("second"), blend);
  options->color_adjustments = null;
  int calls = 0;
  types::GlScene3DPass pass = [&calls](auto, auto, auto, auto) { ++calls; };
  Array<types::GlScene3DPass> passes{pass};
  options->passes = passes;
  const auto registries = render_gl::build_gl_render_registries(view);

  // Registry membership is snapshotted, while descriptors and pass closures keep identity.
  options->blend_realizations->erase(String("first"));
  registries->blend_realizations.set(String("third"), blend);
  if (!registries->blend_realizations.has(String("first")) ||
      options->blend_realizations->has(String("third")) ||
      registries->blend_realizations.get(String("first")).value() != blend ||
      !std::holds_alternative<Null>(registries->color_adjustments)) return 2;
  auto keys = registries->blend_realizations.keys();
  if (keys.next().value != String("first") || keys.next().value != String("second")) return 3;
  passes.push(pass);
  if (registries->passes->size() != 1 || registries->passes->identity() == passes.identity()) return 4;
  registries->passes->element(0)({}, {}, {}, {});
  if (calls != 1) return 5;

  const auto pbr = make_ref<types::GlPbrProgram>();
  const auto program = structural_ref_cast<scene3d_gl::GlMeshProgramView>(pbr);
  const auto mutable_program = scene3d_gl::get_gl_mesh_program_projection(program);
  if (mutable_program.shared_owner() != program.shared_owner()) return 6;
  row_set<RowKey<"locObjectAlpha">>(mutable_program, decltype(pbr->loc_object_alpha){null});
  if (!std::holds_alternative<Null>(pbr->loc_object_alpha)) return 7;
  if (!check_uniform_type_names()) return 8;
  std::cout << "GL registry snapshots preserve absence, order and referenced owners\n";
}
