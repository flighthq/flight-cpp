#include <flight/render_gl/gl_render_state.hpp>
#include <flight/scene3d_gl/gl_mesh_program.hpp>
#include <flight/types/gl_pbr_program.hpp>

#include <iostream>

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
  std::cout << "GL registry snapshots preserve absence, order and referenced owners\n";
}
