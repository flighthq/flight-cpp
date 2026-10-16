#include <flight/render/render_proxy.hpp>
#include <flight/render/renderer.hpp>
#include <flight/render_gl/gl_render_state.hpp>
#include <flight/render_gl/gl_quad_material_registry.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/scene2d_gl/gl_quad_batch_writer.hpp>
#include <flight/shape/shape.hpp>
#include <flight/types/gl_render_state.hpp>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
struct Data : flight::ReferenceEnabled { int token{}; };
struct OtherData : flight::ReferenceEnabled {};
}

int main() {
  try {
    using flight::RowKey;
    auto state = flight::make_ref<flight::types::GlRenderState>();
    auto runtime = flight::make_ref<flight::types::GlRenderStateRuntime>();
    state->entity_runtime_key = runtime;
    state->render_alpha = 1;
    state->scene_graph_sync_policy = flight::String("requiresInvalidation");
    runtime->registries = flight::make_ref<flight::types::GlRenderRegistries>();
    auto root = flight::scene2d::create_display_object();
    auto other = flight::scene2d::create_display_object();
    auto shape = flight::shape::create_shape();
    auto child = flight::node::native_node_2d(shape);
    int created = 0, destroyed = 0;
    flight::Ref<Data> original_data;
    auto plain = flight::make_ref<flight::types::NodeRenderer>();
    plain->native_gl_scene_2d = std::make_shared<flight::types::NativeGlScene2DRendererBinding>();
    auto renderer = flight::make_ref<flight::types::NodeRenderer>();
    auto binding = std::make_shared<flight::types::NativeGlScene2DRendererBinding>();
    binding->create_data = [&](auto callback_state, auto source) -> std::optional<flight::ErasedRef> {
      require(callback_state == state, "callback received a copied/base-shaped GL state");
      require(source.template shared_exact_object<flight::types::Shape>() == shape,
              "callback lost the original concrete Shape owner");
      original_data = flight::make_ref<Data>();
      original_data->token = ++created;
      return flight::ErasedRef(original_data);
    };
    binding->destroy_data = [&](auto callback_state, flight::ErasedRef data) {
      require(callback_state == state && data.as<Data>() == original_data,
              "teardown received a different state or renderer-data owner");
      ++destroyed;
    };
    renderer->native_gl_scene_2d = binding;
    auto old_table = runtime->registries->node_renderers;
    flight::render::register_node_renderer(state, flight::types::display_object_kind, plain);
    flight::render::register_node_renderer(state, flight::types::shape_kind, renderer);
    require(old_table.empty(), "registration mutated a retained registry snapshot");
    const auto revision = runtime->renderer_map_id;
    flight::render::register_node_renderer(state, flight::types::shape_kind, renderer);
    require(runtime->renderer_map_id == revision, "identical descriptor registration advanced revision");

    auto material = flight::make_ref<flight::types::Material2D>();
    material->kind = flight::String("test-material");
    using MaterialView = flight::StructuralRef<flight::RowReadonly<
        flight::RowOf<flight::Ref<flight::types::Material>>>>;
    MaterialView material_view(flight::StructuralRef<flight::RowWritable<
        flight::RowOf<flight::Ref<flight::types::Material2D>>>>(material));
    auto material_renderer = flight::make_ref<flight::types::GlQuadMaterialRenderer>();
    flight::render_gl::register_gl_quad_material_renderer(state, material->kind, material_renderer);
    require(material_view.shared_exact_object<flight::types::Material2D>() == material &&
                flight::render_gl::resolve_gl_quad_material_renderer(state, std::optional{material_view}) == material_renderer,
            "material resolution replaced or nominally reinterpreted the original Material2D owner");
    material->kind = flight::String("unregistered");
    require(!flight::render_gl::resolve_gl_quad_material_renderer(state, std::optional{material_view}),
            "material row view stopped reading the original owner's live kind cell");

    flight::row_set<RowKey<"x">>(root, 10.0);
    flight::row_set<RowKey<"alpha">>(root, 0.5);
    shape->x = 2; shape->y = 3; shape->alpha = 0.75;
    flight::node::add_node_child(root, child);
    require(flight::render::prepare_scene_2d_render(state, root), "first preparation was not dirty");
    auto proxy = flight::render::get_render_proxy_2d(state, child).value();
    auto matrix = proxy->transform_2d;
    auto local = flight::node::get_node_local_matrix(child);
    require(proxy->source.shared_exact_object<flight::types::Shape>() == shape &&
                proxy->renderer == renderer && proxy->renderer_data->as<Data>() == original_data &&
                !proxy->renderer_data->as<OtherData>(), "proxy/data cache changed owner or admitted a wrong type");
    require(matrix->tx == 12 && matrix->ty == 3 && proxy->alpha == 0.375,
            "preparation lost inherited transform/alpha");
    require(!flight::render::prepare_scene_2d_render(state, root) && created == 1,
            "unchanged graph rebuilt or recreated cached data");
    require(flight::node::get_node_local_matrix(child) == local, "local matrix cache changed identity");

    flight::row_set<RowKey<"x">>(root, 20.0);
    flight::node::invalidate_node_local_transform(root);
    flight::render::prepare_scene_2d_render(state, root);
    require(proxy->transform_2d == matrix && matrix->tx == 22 && created == 1,
            "parent invalidation replaced a proxy/matrix/data owner or left it stale");

    flight::row_set<RowKey<"x">>(other, 100.0);
    flight::row_set<RowKey<"alpha">>(other, 0.8);
    flight::node::add_node_child(root, other);
    flight::node::add_node_child(other, child);
    flight::render::prepare_scene_2d_render(state, root);
    require(flight::render::get_render_proxy_2d(state, child).value() == proxy &&
                matrix->tx == 122 && std::abs(proxy->alpha - 0.3) < 1e-12 && created == 1,
            "reparenting lost cache identity or inherited state");
    auto other_runtime = flight::node::native_node_2d_runtime(other);
    const auto children_revision = other_runtime->children_id;
    flight::node::add_node_child(other, child);
    require(other_runtime->children_id == children_revision, "appending the last child changed hierarchy revision");
    bool rejected = false;
    try { flight::node::add_node_child(child, root); }
    catch (const flight::TypeError&) { rejected = true; }
    require(rejected, "hierarchy admitted an ancestor cycle");

    flight::Float32Array instance(13U);
    flight::scene2d_gl::write_gl_quad_batch_affine_instance(
        instance, 0, matrix, 4, 5, 0.25, 0.5, 0.5, 0, 0, -0.5, 0.3);
    require(instance[0] == 4 && instance[3] == 5 && instance[4] == 122 && instance[5] == 3 &&
                instance[6] == 0.25F && instance[7] == 0.5F && instance[11] == -0.5F &&
                std::abs(instance[12] - 0.3F) < 1e-6F, "quad writer lost world/UV/alpha packet values");

    flight::render::dispose_scene_2d_render(state, root);
    require(destroyed == 1 && runtime->render_proxy_sources.empty() &&
                !runtime->render_proxy_map.get(child), "subtree disposal lost data teardown or left proxy membership");
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
