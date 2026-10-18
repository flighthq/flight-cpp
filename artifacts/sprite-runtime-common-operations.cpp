// Isolated component: immutable exact-slot Types input and whole emitted source module.
// No product getter, Types override, source factory or renderer is activated here.
#include <flight/types/contract.hpp>
#include <iostream>
#include <tuple>
#include <type_traits>

#ifndef FLIGHT_SPRITE_SOURCE_HEADER
#error Select the immutable whole emitted Sprite source header explicitly.
#endif

// Declaration-only repairs for the two retained calls to refused functions.
// Signatures follow the retained emitted argument carriers; no body is supplied.
namespace flight::scene2d {
struct data_enabled_kind_name_entity_runtime_key_alpha_visible_blend_mode_clip_material_material_data_pivot_x_pivot_y_rotation_scale_x_scale_y_skew_x_skew_y_x_y_480feb592fa82925;
void initialize_sprite_renderer_data(
    flight::types::EntityConstruction<flight::Ref<flight::types::SpriteIdentityRendererData>>,
    flight::Ref<flight::types::RenderState>, flight::types::NodeAny);
flight::Ref<flight::types::Sprite> create_sprite(flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<data_enabled_kind_name_entity_runtime_key_alpha_visible_blend_mode_clip_material_material_data_pivot_x_pivot_y_rotation_scale_x_scale_y_skew_x_skew_y_x_y_480feb592fa82925>>>>);
}
#include FLIGHT_SPRITE_SOURCE_HEADER

namespace component {
using Traits = flight::Ref<flight::types::Node2DTraits>;
using Generic = flight::types::NodeRuntime<Traits>;
using GenericNode = flight::types::Node<Traits>;
using Exact = flight::types::SpriteRuntime;
using Sprite = flight::types::Sprite;
using Node = flight::types::Node2D;
using Common = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Generic>>>>;
using SpriteSource = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Sprite>>>>;
template<class R> using View = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<R>>>>;
template<class F> struct Args;
template<class R, class... A> struct Args<std::function<R(A...)>> { using tuple = std::tuple<A...>; };
using BoundsSource = std::tuple_element_t<1, Args<decltype(Exact::compute_local_bounds_rectangle)>::tuple>;
using CanAdd = bool(*)(Node, Node);
bool can_add(Node parent, Node child) { return parent.identity() != child.identity(); }

int checks = 0, failures = 0;
void check(const char* label, bool result) {
  ++checks; failures += !result;
  std::cout << label << ": " << (result ? "PASS" : "FAIL") << '\n';
}

// Reads the exact declared physical optional before projecting its live fields.
template<class Object> Common common_for(const std::shared_ptr<Object>& source) {
  const auto runtime = flight::row_get<decltype(Object::entity_runtime_key)>(
      View<Object>(source), flight::types::entity_runtime_key).value();
  return Common(View<typename decltype(runtime)::element_type>(runtime));
}
Common common_for(Node source) {
  if (auto exact = source.shared_exact_object<Sprite>()) return common_for(exact);
  if (auto generic = source.shared_exact_object<GenericNode>()) return common_for(generic);
  throw std::bad_cast();
}
template<class Object> Node node_view(const std::shared_ptr<Object>& source) {
  return Node::from_owner(View<Object>(source).shared_owner());
}
void invalidate_world_bounds(Node source) {
  for (std::optional<Node> current = source; current;
       current = flight::row_get<flight::RowKey<"parent">>(common_for(*current))) {
    auto runtime = common_for(*current);
    flight::row_set<flight::RowKey<"worldBoundsUsingWorldTransformId">>(runtime, -1.0);
    flight::row_set<flight::RowKey<"worldBoundsUsingLocalBoundsId">>(runtime, -1.0);
  }
}

template<class Object, class Runtime>
void common_operations(const std::shared_ptr<Object>& source,
                       std::shared_ptr<Runtime> runtime, Node parent) {
  source->entity_runtime_key = runtime;
  auto common = common_for(source);
  check("common view keeps exact allocation", common.shared_native_object().get() == runtime.get());
  check("common view recovers original runtime", common.template shared_exact_object<Runtime>() == runtime);
  check("common parent is original physical cell", &flight::row_get<flight::RowKey<"parent">>(common) == &runtime->parent);
  flight::row_set<flight::RowKey<"parent">>(common, std::optional<Node>{parent});
  check("parent write retains original source identity", runtime->parent && runtime->parent->identity() == parent.identity());
  runtime->appearance_id = 4294967295.0;
  runtime->local_transform_id = 7;
  runtime->parent_reference_id = 41;
  flight::row_set<flight::RowKey<"appearanceId">>(common,
      flight::unsigned_right_shift(flight::row_get<flight::RowKey<"appearanceId">>(common) + 1, 0));
  flight::row_set<flight::RowKey<"localTransformId">>(common,
      flight::unsigned_right_shift(flight::row_get<flight::RowKey<"localTransformId">>(common) + 1, 0));
  flight::row_set<flight::RowKey<"parentReferenceId">>(common,
      flight::unsigned_right_shift(flight::row_get<flight::RowKey<"parentReferenceId">>(common) + 1, 0));
  flight::row_set<flight::RowKey<"worldTransformUsingParentTransformId">>(common, -1.0);
  check("appearance invalidation keeps uint32 wrap", runtime->appearance_id == 0);
  check("transform invalidation writes exact field", runtime->local_transform_id == 8);
  check("parent invalidation writes exact stamps", runtime->parent_reference_id == 42 && runtime->world_transform_using_parent_transform_id == -1);
  auto children = flight::Array<Node>{parent};
  flight::row_set<flight::RowKey<"children">>(common, std::optional<flight::Array<Node>>{children});
  flight::row_get<flight::RowKey<"children">>(common).value().push(node_view(source));
  check("children mutation is shared with native runtime", runtime->children->size() == 2 && runtime->children.value()[0] == parent && children.size() == 2);
  flight::row_set<flight::RowKey<"canAddChild">>(common, std::function<bool(Node, Node)>(can_add));
  auto& callback = flight::row_get<flight::RowKey<"canAddChild">>(common);
  check("common callback is stored original cell", &callback == &runtime->can_add_child);
  check("common callback retains function pointer", callback.template target<CanAdd>() && *callback.template target<CanAdd>() == can_add);
  check("common callback receives original source views", callback(parent, node_view(source)) && !callback(parent, parent));
  runtime->local_content_id = 19;
  check("native writes remain live through saved view", flight::row_get<flight::RowKey<"localContentId">>(common) == 19);
  auto replacement = flight::make_ref<Runtime>();
  flight::row_set(View<Object>(source), flight::types::entity_runtime_key, replacement);
  check("replacement updates one physical optional", source->entity_runtime_key.value() == replacement && common_for(source).shared_native_object().get() == replacement.get());
  check("saved common view keeps previous allocation", common.shared_native_object().get() == runtime.get());
  flight::row_set(View<Object>(source), flight::types::entity_runtime_key, std::nullopt);
  check("clear reaches physical optional without dynamic shadow", !source->entity_runtime_key && View<Object>(source).shared_owner()->dynamic_value(flight::types::entity_runtime_key) == nullptr);
  bool absent = false;
  try { static_cast<void>(common_for(source)); } catch (const std::bad_optional_access&) { absent = true; }
  check("absent runtime fails at exact physical lookup", absent);
  runtime->parent = std::nullopt;
  runtime->children = std::nullopt;
  std::weak_ptr<Runtime> lifetime = runtime;
  runtime.reset();
  check("saved common view owns detached runtime lifetime", !lifetime.expired());
  common = {};
  check("last view release destroys detached runtime", lifetime.expired());
}

void original_bounds() {
  using Compute = decltype(&flight::scene2d::compute_sprite_local_bounds_rectangle);
  using Valid = decltype(&flight::scene2d::is_sprite_local_bounds_rectangle_valid);
  auto sprite = flight::make_ref<Sprite>();
  auto runtime = flight::make_ref<Exact>();
  sprite->entity_runtime_key = runtime;
  sprite->data = flight::scene2d::create_sprite_data();
  // Store the original emitted functions directly. No adapter or replacement body.
  runtime->compute_local_bounds_rectangle = flight::scene2d::compute_sprite_local_bounds_rectangle;
  runtime->is_local_bounds_rectangle_valid = flight::scene2d::is_sprite_local_bounds_rectangle_valid;
  check("stored compute retains original source function", runtime->compute_local_bounds_rectangle.target<Compute>() && *runtime->compute_local_bounds_rectangle.target<Compute>() == flight::scene2d::compute_sprite_local_bounds_rectangle);
  check("stored validity retains original source function", runtime->is_local_bounds_rectangle_valid->target<Valid>() && *runtime->is_local_bounds_rectangle_valid->target<Valid>() == flight::scene2d::is_sprite_local_bounds_rectangle_valid);
  SpriteSource original(sprite);
  BoundsSource bounds(original);
  check("bounds argument retains original Sprite owner", bounds.shared_owner() == original.shared_owner() && bounds.shared_exact_object<Sprite>() == sprite);
  check("source getter returns original runtime owner", flight::scene2d::get_sprite_runtime(original).shared_exact_object<Exact>() == runtime);
  auto out = flight::make_ref<flight::types::Rectangle>();
  out->x = 3; out->y = 4; out->width = 99; out->height = 99;
  runtime->local_bounds_texture_version = 5;
  check("actual validity rejects stale null stamp", !runtime->is_local_bounds_rectangle_valid.value()(bounds));
  runtime->compute_local_bounds_rectangle(out, bounds);
  check("actual bounds null branch zeros size and preserves origin", out->width == 0 && out->height == 0 && out->x == 3 && out->y == 4);
  check("actual bounds writes exact null texture/version", !runtime->local_bounds_texture && runtime->local_bounds_texture_version == -1);
  check("actual validity accepts refreshed null stamp", runtime->is_local_bounds_rectangle_valid.value()(bounds));
  auto image = flight::make_ref<flight::types::ImageResource>();
  image->width = 20; image->height = 10;
  auto texture = flight::make_ref<flight::types::Texture2D>();
  texture->dimension = flight::String("2d");
  texture->source = flight::types::TextureSourceValue{image};
  texture->uv_scale = flight::make_ref<flight::types::Vector2>();
  texture->uv_scale->x = -2; texture->uv_scale->y = 3;
  texture->uv_rotation = 0; texture->version = 7;
  sprite->data->texture = flight::types::Texture{texture};
  check("actual validity rejects new texture identity", !runtime->is_local_bounds_rectangle_valid.value()(bounds));
  runtime->compute_local_bounds_rectangle(out, bounds);
  check("actual bounds executes texture helper and scales", out->width == 40 && out->height == 30);
  check("actual bounds caches original texture/version", runtime->local_bounds_texture == sprite->data->texture && runtime->local_bounds_texture_version == 7);
  check("actual validity accepts exact texture stamp", runtime->is_local_bounds_rectangle_valid.value()(bounds));
  texture->version = 8; texture->uv_rotation = flight::pi / 2;
  check("actual validity rejects live version mutation", !runtime->is_local_bounds_rectangle_valid.value()(bounds));
  runtime->compute_local_bounds_rectangle(out, bounds);
  check("actual bounds quarter turn uses source dimensions", out->width == 20 && out->height == 60 && runtime->local_bounds_texture_version == 8);
  auto other = flight::make_ref<flight::types::Texture2D>(*texture);
  sprite->data->texture = flight::types::Texture{other};
  check("same-version replacement still invalidates identity", !runtime->is_local_bounds_rectangle_valid.value()(bounds));
  other->source = std::nullopt;
  runtime->compute_local_bounds_rectangle(out, bounds);
  check("actual bounds missing source returns zero", out->width == 0 && out->height == 0 && runtime->is_local_bounds_rectangle_valid.value()(bounds));
  sprite->data->texture = std::nullopt;
  runtime->compute_local_bounds_rectangle(out, bounds);
  check("actual bounds clear resets texture/version", !runtime->local_bounds_texture && runtime->local_bounds_texture_version == -1 && runtime->is_local_bounds_rectangle_valid.value()(bounds));
}
} // namespace component

int main() {
  using namespace component;
  check("flat SpriteRuntime has no fabricated generic base", !std::is_base_of_v<Generic, Exact>);
  auto sprite = flight::make_ref<Sprite>();
  auto node = flight::make_ref<GenericNode>();
  auto sr = flight::make_ref<Exact>();
  auto nr = flight::make_ref<Generic>();
  sprite->entity_runtime_key = sr; node->entity_runtime_key = nr;
  sr->parent = node_view(node);
  sr->world_bounds_using_world_transform_id = nr->world_bounds_using_world_transform_id = 8;
  sr->world_bounds_using_local_bounds_id = nr->world_bounds_using_local_bounds_id = 9;
  invalidate_world_bounds(node_view(sprite));
  check("world bounds invalidation follows mixed owners", sr->world_bounds_using_world_transform_id == -1 && sr->world_bounds_using_local_bounds_id == -1 && nr->world_bounds_using_world_transform_id == -1 && nr->world_bounds_using_local_bounds_id == -1);
  sr->parent = std::nullopt; sprite->entity_runtime_key = std::nullopt; node->entity_runtime_key = std::nullopt;
  sr.reset(); nr.reset();
  std::cout << "exact flat Sprite common operations\n";
  common_operations(sprite, flight::make_ref<Exact>(), node_view(node));
  std::cout << "generic Node common operations\n";
  common_operations(node, flight::make_ref<Generic>(), node_view(sprite));
  original_bounds();
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
