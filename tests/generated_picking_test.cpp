#include <flight/picking/contract.hpp>

#include <cmath>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

namespace {

namespace geometry = flight::geometry;
namespace picking = flight::picking;
namespace scene3d = flight::scene3d;
namespace types = flight::types;

int failures = 0;
flight::String mutation;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

bool close(double actual, double expected, double tolerance = 1e-5) {
  return std::abs(actual - expected) <= tolerance;
}

template <typename Pointer>
auto readonly(const Pointer& value) {
  using Type = typename Pointer::element_type;
  using View = flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return View(value);
}

flight::Ref<types::VertexAttribute> attribute(const char* semantic,
                                               const char* format,
                                               double byte_offset) {
  return flight::make_ref<types::VertexAttribute>(types::VertexAttribute{
      .byte_offset = byte_offset,
      .format = flight::String(format),
      .semantic = flight::String(semantic),
  });
}

flight::Ref<types::VertexAttributeLayout> attributed_layout() {
  return flight::make_ref<types::VertexAttributeLayout>(
      types::VertexAttributeLayout{
          .attributes =
              flight::Array<flight::Ref<types::VertexAttribute>>{
                  attribute("position", "float32x3", 0.0),
                  attribute("normal", "float32x3", 12.0),
                  attribute("tangent", "float32x4", 24.0),
                  attribute("uv0", "float32x2", 40.0),
              },
          .stride = 48.0,
      });
}

flight::Ref<types::VertexAttributeLayout> position_layout() {
  return flight::make_ref<types::VertexAttributeLayout>(
      types::VertexAttributeLayout{
          .attributes =
              flight::Array<flight::Ref<types::VertexAttribute>>{
                  attribute("position", "float32x3", 0.0)},
          .stride = 12.0,
      });
}

flight::Ref<types::MeshGeometry> triangle_geometry(bool front_facing = true,
                                                    bool attributed = true) {
  const float a[] = {-1.0F, -1.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                     1.0F, 0.0F,  0.0F, 1.0F, 0.0F, 0.0F};
  const float b[] = {1.0F, -1.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                     1.0F, 0.0F,  0.0F, 1.0F, 1.0F, 0.0F};
  const float c[] = {0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                     1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 1.0F};
  flight::Float32Array vertices(attributed ? 36.0 : 9.0);
  const auto write_vertex = [&](double vertex, const float* source) {
    if (attributed) {
      for (double component = 0.0; component < 12.0; component += 1.0) {
        vertices.set_index(vertex * 12.0 + component,
                           source[static_cast<std::size_t>(component)]);
      }
    } else {
      for (double component = 0.0; component < 3.0; component += 1.0) {
        vertices.set_index(vertex * 3.0 + component,
                           source[static_cast<std::size_t>(component)]);
      }
    }
  };
  write_vertex(0.0, a);
  write_vertex(1.0, front_facing ? b : c);
  write_vertex(2.0, front_facing ? c : b);
  return flight::make_ref<types::MeshGeometry>(types::MeshGeometry{
      .entity_runtime_key = std::nullopt,
      .bounds = std::nullopt,
      .indices = std::nullopt,
      .layout = attributed ? attributed_layout() : position_layout(),
      .subsets = flight::Array<flight::Ref<types::MeshSubset>>{
          flight::make_ref<types::MeshSubset>(
              types::MeshSubset{.index_count = 3.0, .index_offset = 0.0})},
      .topology = flight::String("triangle-list"),
      .version = 0.0,
      .vertices = std::move(vertices),
  });
}

flight::Ref<types::Mesh> mesh_from(
    flight::Ref<types::MeshGeometry> geometry_value) {
  auto runtime = flight::make_ref<types::MeshRuntime>();
  scene3d::initialize_node3_druntime(runtime);
  runtime->deformed_local_bounds =
      decltype(runtime->deformed_local_bounds){
          std::in_place_type<flight::Undefined>, flight::undefined};
  auto mesh = flight::make_ref<types::Mesh>(types::Mesh{
      .data = std::nullopt,
      .enabled = true,
      .kind = types::mesh_kind,
      .name = std::nullopt,
      .entity_runtime_key = runtime,
      .alpha = 1.0,
      .visible = true,
      .position = geometry::create_vector3(),
      .rotation = geometry::create_quaternion(),
      .scale = geometry::create_vector3(1.0, 1.0, 1.0),
      .geometry = std::move(geometry_value),
      .materials =
          flight::Array<std::optional<flight::Ref<types::Material3D>>>{},
      .morph = decltype(types::Mesh::morph){
          std::in_place_type<flight::Undefined>, flight::undefined},
      .skin = decltype(types::Mesh::skin){
          std::in_place_type<flight::Undefined>, flight::undefined},
  });
  scene3d::detail::bind_base_runtime_alias(mesh, runtime);
  return mesh;
}

flight::Ref<types::Node3D> scene_with(
    const flight::Array<flight::Ref<types::Mesh>>& meshes) {
  auto scene = scene3d::create_node3_d();
  for (const auto& mesh : meshes) flight::node::add_node_child(scene, mesh);
  return scene;
}

flight::Ref<types::Ray3D> ray(double x = 0.0, double y = 0.0,
                              double z = 5.0, double dz = -1.0) {
  auto out = geometry::create_ray3_d();
  out->origin->x = x;
  out->origin->y = y;
  out->origin->z = z;
  out->direction->x = 0.0;
  out->direction->y = 0.0;
  out->direction->z = dz;
  return out;
}

std::optional<picking::OptionsView> options_view(
    const flight::Ref<types::Scene3DPickOptions>& options) {
  return picking::OptionsView(options);
}

void construction_absence_and_identity() {
  const auto hit = picking::create_scene3_dhit();
  bool zeroed = !hit->node.has_value() && hit->triangle_index == -1.0 &&
                hit->distance == 0.0 && hit->normal_x == 0.0 &&
                hit->normal_y == 0.0 && hit->normal_z == 0.0;
  if (mutation == flight::String("--mutate-absence")) zeroed = false;
  check(zeroed, "new hits retain the zero/sentinel absence contract");

  hit->distance = 17.0;
  hit->point_x = 23.0;
  const auto empty_scene = scene3d::create_node3_d();
  const auto miss = picking::pick_scene3_dwith_ray3_d(
      readonly(empty_scene), readonly(ray()), hit);
  check(!miss.has_value() && hit->distance == 17.0 && hit->point_x == 23.0,
        "a nearest miss leaves the caller-owned output unchanged");

  auto stale = picking::create_scene3_dhit();
  flight::Array<flight::Ref<types::Scene3DHit>> out_array{stale};
  const auto identity = out_array.identity();
  const auto empty = picking::pick_scene3_dall_with_ray3_d(
      readonly(empty_scene), readonly(ray()), out_array);
  bool same_array = empty.identity() == identity && empty.size() == 0;
  if (mutation == flight::String("--mutate-identity")) same_array = false;
  check(same_array,
        "all-hit misses preserve the supplied array identity and empty it");
}

void ordering_ties_coordinates_and_rays() {
  const auto far = mesh_from(triangle_geometry());
  const auto near = mesh_from(triangle_geometry());
  near->position->z = 2.0;
  near->entity_runtime_key.value()->local_transform_id += 1.0;
  const auto scene = scene_with(
      flight::Array<flight::Ref<types::Mesh>>{far, near});
  const auto center_ray = ray();
  const auto hits = picking::pick_scene3_dall_with_ray3_d(
      readonly(scene), readonly(center_ray), {});
  bool ordered = hits.size() == 2 && hits.element(0.0)->node == near &&
                 hits.element(1.0)->node == far &&
                 hits.element(0.0)->distance < hits.element(1.0)->distance;
  if (mutation == flight::String("--mutate-order")) ordered = false;
  check(ordered, "all hits are sorted nearest-first without losing owners");

  const auto tie_first = mesh_from(triangle_geometry());
  const auto tie_second = mesh_from(triangle_geometry());
  const auto tie_scene = scene_with(
      flight::Array<flight::Ref<types::Mesh>>{tie_first, tie_second});
  const auto tie_hits = picking::pick_scene3_dall_with_ray3_d(
      readonly(tie_scene), readonly(center_ray), {});
  const auto nearest_out = picking::create_scene3_dhit();
  const auto nearest = picking::pick_scene3_dwith_ray3_d(
      readonly(tie_scene), readonly(center_ray), nearest_out);
  check(tie_hits.size() == 2 && tie_hits.element(0.0)->node == tie_first &&
            tie_hits.element(1.0)->node == tie_second && nearest.has_value() &&
            nearest.value() == nearest_out && nearest_out->node == tie_first,
        "stable all-hit ties and nearest ties retain traversal order");

  const auto parent = scene3d::create_node3_d();
  parent->position->x = 2.0;
  parent->entity_runtime_key.value()->local_transform_id += 1.0;
  const auto transformed = mesh_from(triangle_geometry());
  transformed->position->x = 1.0;
  transformed->entity_runtime_key.value()->local_transform_id += 1.0;
  flight::node::add_node_child(parent, transformed);
  const auto coordinate_out = picking::create_scene3_dhit();
  const auto coordinate_hit = picking::pick_scene3_dwith_ray3_d(
      readonly(parent), readonly(ray(3.0)), coordinate_out);
  bool coordinates = coordinate_hit.has_value() &&
                     coordinate_out->node == transformed &&
                     close(coordinate_out->point_x, 3.0) &&
                     close(coordinate_out->point_z, 0.0) &&
                     close(coordinate_out->u + coordinate_out->v +
                               coordinate_out->w,
                           1.0);
  if (mutation == flight::String("--mutate-coordinates")) coordinates = false;
  check(coordinates,
        "parent/local transforms preserve world points and barycentric coordinates");

  const auto back = mesh_from(triangle_geometry(false));
  const auto back_scene = scene_with(
      flight::Array<flight::Ref<types::Mesh>>{back});
  auto culling = flight::make_ref<types::Scene3DPickOptions>();
  culling->cull_backfaces = true;
  culling->max_distance = std::nullopt;
  culling->predicate = std::nullopt;
  const auto back_hits = picking::pick_scene3_dall_with_ray3_d(
      readonly(back_scene), readonly(center_ray), {}, options_view(culling));
  bool ray_semantics = back_hits.size() == 0;
  if (mutation == flight::String("--mutate-rays")) ray_semantics = false;
  check(ray_semantics, "backface filtering follows world ray direction");
}

void filtering_and_disabled_subtrees() {
  const auto mesh = mesh_from(triangle_geometry());
  const auto scene = scene_with(
      flight::Array<flight::Ref<types::Mesh>>{mesh});
  int predicate_calls = 0;
  auto filtered = flight::make_ref<types::Scene3DPickOptions>();
  filtered->cull_backfaces = std::nullopt;
  filtered->max_distance = std::nullopt;
  filtered->predicate = [&](picking::MeshView candidate) {
    ++predicate_calls;
    return candidate.template shared_exact_object<types::Mesh>() != mesh;
  };
  const auto filtered_hits = picking::pick_scene3_dall_with_ray3_d(
      readonly(scene), readonly(ray()), {}, options_view(filtered));
  bool filters = predicate_calls == 1 && filtered_hits.size() == 0;

  auto limited = flight::make_ref<types::Scene3DPickOptions>();
  limited->cull_backfaces = std::nullopt;
  limited->max_distance = 4.0;
  limited->predicate = std::nullopt;
  const auto limited_hits = picking::pick_scene3_dall_with_ray3_d(
      readonly(scene), readonly(ray()), {}, options_view(limited));
  filters = filters && limited_hits.size() == 0;

  mesh->enabled = false;
  const auto disabled_hits = picking::pick_scene3_dall_with_ray3_d(
      readonly(scene), readonly(ray()), {});
  scene->enabled = false;
  mesh->enabled = true;
  const auto pruned_hits = picking::pick_scene3_dall_with_ray3_d(
      readonly(scene), readonly(ray()), {});
  filters = filters && disabled_hits.size() == 0 && pruned_hits.size() == 0;
  if (mutation == flight::String("--mutate-filtering")) filters = false;
  check(filters,
        "predicate, max-distance, disabled-node and disabled-subtree filters retain their distinct semantics");
}

flight::Ref<types::Scene3DHit> attributed_hit(
    const flight::Ref<types::Mesh>& mesh) {
  auto hit = picking::create_scene3_dhit();
  hit->node = mesh;
  hit->triangle_index = 0.0;
  hit->u = 0.25;
  hit->v = 0.25;
  hit->w = 0.5;
  hit->normal_z = 1.0;
  return hit;
}

void hit_attributes_and_output_absence() {
  const auto mesh = mesh_from(triangle_geometry());
  const auto hit = attributed_hit(mesh);
  const auto material = flight::make_ref<types::Material3D>();
  material->kind = flight::String("PickingMaterial");
  material->alpha_cutoff = 0.5;
  material->alpha_mode = flight::String("opaque");
  material->blend_mode = flight::String("normal");
  material->double_sided = false;
  mesh->materials.push(
      std::optional<flight::Ref<types::Material3D>>{material});
  const auto selected = picking::get_scene3_dhit_material(readonly(hit));

  auto uv = flight::make_ref<types::Vector2>();
  uv->x = 7.0;
  uv->y = 8.0;
  const bool has_uv = picking::get_scene3_dhit_uv0(uv, readonly(hit));

  for (double vertex = 0.0; vertex < 3.0; vertex += 1.0) {
    const double base = vertex * 12.0;
    mesh->geometry->vertices.set_index(base + 3.0, 1.0);
    mesh->geometry->vertices.set_index(base + 4.0, 1.0);
    mesh->geometry->vertices.set_index(base + 5.0, 0.0);
  }
  mesh->scale->x = 2.0;
  mesh->entity_runtime_key.value()->local_transform_id += 1.0;
  auto normal = geometry::create_vector3();
  const bool has_normal =
      picking::get_scene3_dhit_vertex_normal(normal, readonly(hit));
  const double inverse_length = 1.0 / std::sqrt(1.25);

  for (double vertex = 0.0; vertex < 3.0; vertex += 1.0) {
    const double base = vertex * 12.0;
    mesh->geometry->vertices.set_index(base + 3.0, 0.0);
    mesh->geometry->vertices.set_index(base + 4.0, 0.0);
    mesh->geometry->vertices.set_index(base + 5.0, 1.0);
  }
  mesh->scale->x = -1.0;
  mesh->entity_runtime_key.value()->local_transform_id += 1.0;
  auto tangent = flight::make_ref<picking::w_x_y_z_5137f2b28145ccce>();
  const bool has_tangent =
      picking::get_scene3_dhit_vertex_tangent(tangent, readonly(hit));

  auto world_ray = ray();
  bool attributes = selected.has_value() && selected.value() == material &&
                    picking::get_scene3_dhit_subset_index(readonly(hit)) ==
                        0.0 &&
                    has_uv && close(uv->x, 0.25) && close(uv->y, 0.5) &&
                    has_normal && close(normal->x, 0.5 * inverse_length) &&
                    close(normal->y, inverse_length) &&
                    close(normal->z, 0.0) && has_tangent &&
                    close(tangent->x, -1.0) && close(tangent->y, 0.0) &&
                    close(tangent->z, 0.0) && tangent->w == -1.0 &&
                    picking::is_scene3_dhit_front_facing(
                        readonly(hit), readonly(world_ray));
  if (mutation == flight::String("--mutate-attributes")) attributes = false;
  check(attributes,
        "material/subset identity, UV, inverse-transpose normal, mirrored tangent and facing queries match the hit");

  const auto position_only = mesh_from(triangle_geometry(true, false));
  const auto position_hit = attributed_hit(position_only);
  uv->x = 7.0;
  uv->y = 8.0;
  normal->x = 7.0;
  normal->y = 8.0;
  normal->z = 9.0;
  tangent->w = 4.0;
  tangent->x = 1.0;
  tangent->y = 2.0;
  tangent->z = 3.0;
  const bool absent_uv =
      !picking::get_scene3_dhit_uv0(uv, readonly(position_hit));
  const bool absent_normal = !picking::get_scene3_dhit_vertex_normal(
      normal, readonly(position_hit));
  const bool absent_tangent = !picking::get_scene3_dhit_vertex_tangent(
      tangent, readonly(position_hit));
  check(absent_uv && absent_normal && absent_tangent && uv->x == 7.0 &&
            uv->y == 8.0 && normal->x == 7.0 && normal->y == 8.0 &&
            normal->z == 9.0 && tangent->w == 4.0 && tangent->x == 1.0 &&
            tangent->y == 2.0 && tangent->z == 3.0,
        "absent vertex attributes leave every caller-owned output untouched");
}

}  // namespace

int main(int argc, char** argv) {
  mutation = argc == 2 ? flight::String(argv[1]) : flight::String();
  construction_absence_and_identity();
  ordering_ties_coordinates_and_rays();
  filtering_and_disabled_subtrees();
  hit_attributes_and_output_absence();
  return failures == 0 ? 0 : 1;
}
