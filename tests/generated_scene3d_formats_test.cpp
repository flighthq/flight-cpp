// Native parity evidence for the callable @flighthq/scene3d-formats surface at Flight a627849.
// The package is only partially emitted, so these tests deliberately include individual compiling
// headers rather than a contract umbrella. The fixtures independently write the binary containers
// consumed by the generated parsers and exercise valid, optional, unknown, malformed, and truncated
// branches without substituting an implementation for any refused public entry point.
#include <flight/scene3d_formats/awd2_header.hpp>
#include <flight/scene3d_formats/gltf_core_feature_registry.hpp>
#include <flight/scene3d_formats/gltf_draco.hpp>
#include <flight/scene3d_formats/gltf_extension_handler_registry.hpp>
#include <flight/scene3d_formats/md2_features.hpp>
#include <flight/scene3d_formats/md5_anim_parse.hpp>
#include <flight/scene3d_formats/mtl_parse.hpp>
#include <flight/scene3d_formats/obj_features.hpp>
#include <flight/scene3d_formats/scene3d_format_requirements.hpp>
#include <flight/scene3d_formats/three_ds_chunk_census.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace formats = flight::scene3d_formats;
namespace types = flight::types;

int failures = 0;
bool inject_header_mutation = false;
bool inject_md2_mutation = false;
bool inject_obj_mutation = false;
bool inject_three_ds_mutation = false;
bool inject_registry_mutation = false;
bool inject_material_mutation = false;
bool inject_md5_mutation = false;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void write_le(flight::Uint8Array& bytes, std::size_t offset,
              std::uint64_t value, std::size_t width) {
  for (std::size_t i = 0; i < width; ++i) {
    bytes.set_index(static_cast<double>(offset + i),
                    static_cast<double>((value >> (8 * i)) & 0xffU));
  }
}

void append_le(std::vector<std::uint8_t>& bytes, std::uint64_t value,
               std::size_t width) {
  for (std::size_t i = 0; i < width; ++i) {
    bytes.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xffU));
  }
}

void append(std::vector<std::uint8_t>& destination,
            const std::vector<std::uint8_t>& source) {
  destination.insert(destination.end(), source.begin(), source.end());
}

std::vector<std::uint8_t> three_ds_chunk(
    std::uint16_t id, const std::vector<std::uint8_t>& payload = {}) {
  std::vector<std::uint8_t> bytes;
  append_le(bytes, id, 2);
  append_le(bytes, 6 + payload.size(), 4);
  append(bytes, payload);
  return bytes;
}

flight::Uint8Array typed_bytes(const std::vector<std::uint8_t>& source) {
  flight::Uint8Array result(static_cast<double>(source.size()));
  for (std::size_t i = 0; i < source.size(); ++i) {
    result.set_index(static_cast<double>(i), static_cast<double>(source[i]));
  }
  return result;
}

double count_or_zero(const flight::Map<double, double>& counts, double key) {
  const auto value = counts.get(key);
  return value.has_value() ? *value : 0.0;
}

void awd2_header_byte_offsets_and_failures() {
  flight::Uint8Array backing(18.0);
  auto source = backing.subarray(3.0, 15.0);
  source.set_index(0.0, formats::awd2_magic_0);
  source.set_index(1.0, formats::awd2_magic_1);
  source.set_index(2.0, formats::awd2_magic_2);
  source.set_index(3.0, formats::awd2_format_version);
  source.set_index(4.0, 7.0);
  write_le(source, 5, 0x1234, 2);
  source.set_index(7.0, 2.0);
  write_le(source, 8, 0x78563412, 4);

  const auto owner = source.buffer;
  const auto header = formats::parse_awd2_header(source, std::nullopt,
                                                  std::nullopt);
  bool exact = header.has_value() && (*header)->version_major == 2.0 &&
               (*header)->version_minor == 7.0 && (*header)->flags == 0x1234 &&
               (*header)->compression == 2.0 &&
               (*header)->body_length == 0x78563412 && source.buffer == owner;

  auto invalid_magic = source.slice();
  invalid_magic.set_index(0.0, 0.0);
  auto invalid_version = source.slice();
  invalid_version.set_index(3.0, 1.0);
  exact = exact &&
          !formats::parse_awd2_header(invalid_magic, std::nullopt,
                                      std::nullopt)
               .has_value() &&
          !formats::parse_awd2_header(invalid_version, std::nullopt,
                                      std::nullopt)
               .has_value() &&
          !formats::parse_awd2_header(source.subarray(0.0, 11.0),
                                      std::nullopt, std::nullopt)
               .has_value();
  if (inject_header_mutation) exact = false;
  check(exact,
        "AWD2 parses little-endian fields from a nonzero byte offset without replacing ownership and rejects bad magic, version, and truncation");
}

flight::Uint8Array md2_header(std::int32_t skins, std::int32_t triangles,
                              std::int32_t frames) {
  flight::Uint8Array bytes(formats::md2_header_size);
  write_le(bytes, 0, static_cast<std::uint32_t>(formats::md2_magic), 4);
  write_le(bytes, 4, static_cast<std::uint32_t>(formats::md2_version), 4);
  write_le(bytes, 20, static_cast<std::uint32_t>(skins), 4);
  write_le(bytes, 32, static_cast<std::uint32_t>(triangles), 4);
  write_le(bytes, 40, static_cast<std::uint32_t>(frames), 4);
  return bytes;
}

void md2_feature_detection_and_failures() {
  const auto all = formats::collect_md2_features(md2_header(1, 2, 3));
  bool exact = all.has_value() && all->size() == 3 &&
               all->has(formats::md2_feature_mesh) &&
               all->has(formats::md2_feature_material) &&
               all->has(formats::md2_feature_animation);
  if (all.has_value()) {
    std::vector<flight::String> ordered(all->begin(), all->end());
    exact = exact && ordered.size() == 3 &&
            ordered[0] == formats::md2_feature_mesh &&
            ordered[1] == formats::md2_feature_material &&
            ordered[2] == formats::md2_feature_animation;
  }

  const auto empty = formats::collect_md2_features(md2_header(0, 3, 0));
  auto bad_magic = md2_header(1, 1, 1);
  bad_magic.set_index(0.0, 0.0);
  auto bad_version = md2_header(1, 1, 1);
  write_le(bad_version, 4, 7, 4);
  exact = exact && empty.has_value() && empty->empty() &&
          !formats::collect_md2_features(md2_header(-1, 1, 1)).has_value() &&
          !formats::collect_md2_features(bad_magic).has_value() &&
          !formats::collect_md2_features(bad_version).has_value() &&
          !formats::collect_md2_features(flight::Uint8Array(67.0)).has_value();
  if (inject_md2_mutation) exact = false;
  check(exact,
        "MD2 detects mesh/material/animation in canonical order, keeps valid empty feature sets distinct, and rejects negative, invalid, and truncated headers");
}

void obj_material_reference_order() {
  const auto references = formats::collect_obj_material_library_references(
      flight::String(" # ignored\nmtllib first.mtl second.mtl\n"
                     "usemtl first\n\tmtllib second.mtl third.mtl\n"
                     "mtllib fourth.mtl first.mtl\n"));
  bool exact = references.size() == 4 &&
               references.element(0.0) == flight::String("first.mtl") &&
               references.element(1.0) == flight::String("second.mtl") &&
               references.element(2.0) == flight::String("third.mtl") &&
               references.element(3.0) == flight::String("fourth.mtl");
  if (inject_obj_mutation) exact = false;
  check(exact,
        "OBJ material-library discovery trims lines, ignores comments and usemtl, splits multi-path directives, and deduplicates in first-seen order");
}

std::vector<std::uint8_t> named_three_ds_object(
    const char* name, std::uint16_t child_id,
    const std::vector<std::uint8_t>& child_payload = {}) {
  std::vector<std::uint8_t> payload;
  for (const char* cursor = name; *cursor != '\0'; ++cursor) {
    payload.push_back(static_cast<std::uint8_t>(*cursor));
  }
  payload.push_back(0);
  append(payload, three_ds_chunk(child_id, child_payload));
  return three_ds_chunk(static_cast<std::uint16_t>(types::three_ds_object),
                        payload);
}

flight::Uint8Array complete_three_ds_fixture() {
  std::vector<std::uint8_t> editor_payload;
  append(editor_payload,
         named_three_ds_object("mesh", static_cast<std::uint16_t>(
                                           types::three_ds_trimesh)));
  append(editor_payload,
         named_three_ds_object("camera", static_cast<std::uint16_t>(
                                             types::three_ds_camera)));

  std::vector<std::uint8_t> light_payload(12, 0);
  append(light_payload,
         three_ds_chunk(static_cast<std::uint16_t>(types::three_ds_light_spot)));
  append(editor_payload,
         named_three_ds_object("light", static_cast<std::uint16_t>(
                                            types::three_ds_light),
                               light_payload));

  std::vector<std::uint8_t> material_payload;
  append(material_payload,
         three_ds_chunk(static_cast<std::uint16_t>(
             types::three_ds_material_texture_map)));
  append(material_payload,
         three_ds_chunk(static_cast<std::uint16_t>(
             types::three_ds_material_opacity_map)));
  append(material_payload,
         three_ds_chunk(static_cast<std::uint16_t>(
             types::three_ds_material_bump_map)));
  append(editor_payload,
         three_ds_chunk(static_cast<std::uint16_t>(types::three_ds_material),
                        material_payload));

  std::vector<std::uint8_t> main_payload;
  append(main_payload,
         three_ds_chunk(static_cast<std::uint16_t>(types::three_ds_editor),
                        editor_payload));
  append(main_payload,
         three_ds_chunk(static_cast<std::uint16_t>(types::three_ds_keyframe),
                        three_ds_chunk(static_cast<std::uint16_t>(
                            types::three_ds_keyframe_object_node))));
  return typed_bytes(three_ds_chunk(
      static_cast<std::uint16_t>(types::three_ds_main), main_payload));
}

void three_ds_nested_census_and_recovery() {
  const auto source = complete_three_ds_fixture();
  const auto owner = source.buffer;
  const auto parsed = formats::collect_three_ds_chunk_counts(source);
  bool exact = parsed.has_value() && source.buffer == owner;
  if (parsed.has_value()) {
    const std::vector<double> kinds{
        types::three_ds_trimesh,
        types::three_ds_material,
        types::three_ds_material_texture_map,
        types::three_ds_material_opacity_map,
        types::three_ds_material_bump_map,
        types::three_ds_light,
        types::three_ds_light_spot,
        types::three_ds_camera,
        types::three_ds_keyframe_object_node,
    };
    for (const double kind : kinds) exact = exact && count_or_zero(*parsed, kind) == 1.0;
  }
  const auto names = formats::get_three_ds_feature_names();
  exact = exact && names.size() == 9 &&
          formats::get_three_ds_chunk_name(types::three_ds_trimesh) ==
              flight::String("Trimesh") &&
          formats::get_three_ds_chunk_name(0xbeef) ==
              flight::String("Unknown(0xbeef)");

  auto wrong_magic = source.slice();
  wrong_magic.set_index(0.0, 0.0);
  std::vector<std::uint8_t> invalid_child;
  append_le(invalid_child, types::three_ds_editor, 2);
  append_le(invalid_child, 5, 4);
  const auto malformed = typed_bytes(three_ds_chunk(
      static_cast<std::uint16_t>(types::three_ds_main), invalid_child));
  const auto recovered = formats::collect_three_ds_chunk_counts(malformed);
  exact = exact &&
          !formats::collect_three_ds_chunk_counts(flight::Uint8Array(5.0))
               .has_value() &&
          !formats::collect_three_ds_chunk_counts(wrong_magic).has_value() &&
          recovered.has_value() && recovered->empty();
  if (inject_three_ds_mutation) exact = false;
  check(exact,
        "3DS recursively counts object, material-map, light/spot, camera, and keyframe chunks, preserves source ownership, names unknown IDs stably, and stops safely at malformed children");
}

void gltf_registry_order_replacement_and_decoder_lifecycle() {
  auto core_mesh = flight::make_ref<types::GltfCoreFeatureHandler>(
      types::GltfCoreFeatureHandler{.apply = {},
                                    .kind = flight::String("mesh")});
  auto core_camera = flight::make_ref<types::GltfCoreFeatureHandler>(
      types::GltfCoreFeatureHandler{.apply = {},
                                    .kind = flight::String("camera")});
  auto core_mesh_replacement =
      flight::make_ref<types::GltfCoreFeatureHandler>(
          types::GltfCoreFeatureHandler{.apply = {},
                                        .kind = flight::String("mesh")});
  flight::Array<flight::Ref<types::GltfCoreFeatureHandler>> core_handlers;
  formats::register_gltf_core_feature_handler(core_handlers, core_mesh);
  formats::register_gltf_core_feature_handler(core_handlers, core_camera);
  formats::register_gltf_core_feature_handler(core_handlers,
                                              core_mesh_replacement);

  auto extension_first = flight::make_ref<types::GltfExtensionHandler>(
      types::GltfExtensionHandler{.apply = {},
                                  .kind = flight::String("EXT_test")});
  auto extension_replacement = flight::make_ref<types::GltfExtensionHandler>(
      types::GltfExtensionHandler{.apply = {},
                                  .kind = flight::String("EXT_test")});
  flight::Array<flight::Ref<types::GltfExtensionHandler>> extension_handlers;
  formats::register_gltf_extension_handler(extension_handlers,
                                           extension_first);
  formats::register_gltf_extension_handler(extension_handlers,
                                           extension_replacement);

  formats::unregister_gltf_draco_decoder();
  bool exact = !formats::has_gltf_draco_decoder() &&
               !formats::get_gltf_draco_decoder().has_value();
  formats::register_gltf_draco_decoder(
      [](flight::Uint8Array bytes,
         flight::Record<flight::String, double>)
          -> std::optional<flight::Ref<types::GltfDracoMesh>> {
        return flight::make_ref<types::GltfDracoMesh>(types::GltfDracoMesh{
            .attributes = {},
            .indices = std::nullopt,
            .vertex_count = static_cast<double>(bytes.size()),
        });
      });
  const auto decoder = formats::get_gltf_draco_decoder();
  const auto decoded = decoder.has_value()
                           ? (*decoder)(flight::Uint8Array(4.0), {})
                           : std::nullopt;
  exact = exact && core_handlers.size() == 2 &&
          core_handlers.element(0.0) == core_mesh_replacement &&
          core_handlers.element(1.0) == core_camera &&
          extension_handlers.size() == 1 &&
          extension_handlers.element(0.0) == extension_replacement &&
          formats::has_gltf_draco_decoder() && decoded.has_value() &&
          (*decoded)->vertex_count == 4.0;
  formats::unregister_gltf_draco_decoder();
  exact = exact && !formats::has_gltf_draco_decoder();

  if (inject_registry_mutation) exact = false;
  check(exact,
        "glTF registries append distinct kinds, replace equal kinds in place without disturbing order or owner identity, and expose exact Draco registration lifecycle");
}

void material_defaults_colors_and_diagnostics() {
  const auto material =
      formats::create_default_obj_material(flight::String("paint"));
  bool exact = material->name == flight::String("paint") &&
               material->ambient.size() == 3 &&
               material->ambient.element(0.0) == 0.0 &&
               material->diffuse.size() == 3 &&
               material->diffuse.element(0.0) == 0.8 &&
               material->specular.size() == 3 &&
               material->dissolve == 1.0 && material->illumination == 2.0 &&
               !material->emissive.has_value() &&
               !material->map_diffuse.has_value() &&
               !material->metallic.has_value() &&
               !material->roughness.has_value();

  flight::Map<flight::String, flight::Ref<formats::MtlDropTally>> tallies;
  const std::optional<decltype(tallies)> collector{tallies};
  const auto color = formats::parse_color(flight::String("0.1 0.2 0.3"),
                                          collector, flight::String("Kd"), 2.0);
  exact = exact && color.has_value() && color->size() == 3 &&
          color->element(0.0) == 0.1 && color->element(1.0) == 0.2 &&
          color->element(2.0) == 0.3;
  formats::parse_color(flight::String("0.1 0.2"), collector,
                       flight::String("Ka"), 4.0);
  formats::parse_color(flight::String("0.1 0.2"), collector,
                       flight::String("Ks"), 8.0);
  formats::parse_color(flight::String("0.1 nope 0.3"), collector,
                       flight::String("Kd"), 6.0);
  const auto too_few =
      tallies.get(flight::String("mtl.color-malformed|too-few-components"));
  const auto non_numeric =
      tallies.get(flight::String("mtl.color-malformed|non-numeric"));
  exact = exact && too_few.has_value() && (*too_few)->count == 2.0 &&
          (*too_few)->severity == types::import_diagnostic_severity->recover &&
          non_numeric.has_value() && (*non_numeric)->count == 1.0 &&
          tallies.size() == 2;
  if (inject_material_mutation) exact = false;
  check(exact,
        "MTL helpers preserve canonical defaults and absent optionals, parse exact RGB triples, reject malformed colors, and aggregate diagnostics by kind/discriminator while retaining first evidence");
}

void md5_line_parsing_component_order_and_diagnostics() {
  flight::Map<flight::String, flight::Ref<formats::Md5AnimDropTally>> tallies;
  const std::optional<decltype(tallies)> collector{tallies};
  const auto root = formats::parse_hierarchy_line(
      flight::String("\"root joint\" -1 63 0"), collector, 0.0);
  const auto child = formats::parse_hierarchy_line(
      flight::String("\"child\" 0 3 6"), collector, 1.0);
  const auto pose = formats::parse_baseframe_line(
      flight::String("( 1 2 3 ) ( 0.1 0.2 0.3 )"), collector, 2.0);
  bool exact = root.has_value() && child.has_value() && pose.has_value() &&
               (*root)->name == flight::String("root joint") &&
               (*root)->parent_index == -1.0 && (*root)->flags == 63.0 &&
               (*root)->start_index == 0.0 && (*child)->parent_index == 0.0 &&
               (*pose)->position_x == 1.0 && (*pose)->position_y == 2.0 &&
               (*pose)->position_z == 3.0 &&
               (*pose)->orientation_x == 0.1 &&
               (*pose)->orientation_y == 0.2 &&
               (*pose)->orientation_z == 0.3 &&
               formats::count_md5_anim_flag_components(63.0) == 6.0 &&
               formats::total_md5_anim_components(
                   flight::Array<flight::Ref<formats::Md5AnimHierarchyEntry>>{
                       *root, *child}) == 8.0;

  exact = exact &&
          !formats::parse_hierarchy_line(flight::String("root -1 63 0"),
                                         collector, 3.0)
               .has_value() &&
          !formats::parse_hierarchy_line(flight::String("\"bad\" x 1 0"),
                                         collector, 4.0)
               .has_value() &&
          !formats::parse_baseframe_line(flight::String("( 1 2 )"),
                                         collector, 5.0)
               .has_value() &&
          !formats::parse_baseframe_line(
                       flight::String("( 1 2 bad ) ( 0 0 0 )"), collector,
                       6.0)
               .has_value();
  const auto missing_quotes = tallies.get(
      flight::String("md5anim.malformed-hierarchy|missing-name-quotes"));
  const auto bad_hierarchy = tallies.get(
      flight::String("md5anim.malformed-hierarchy|non-numeric-values"));
  const auto short_pose = tallies.get(
      flight::String("md5anim.malformed-baseframe|not-enough-components"));
  const auto bad_pose = tallies.get(
      flight::String("md5anim.malformed-baseframe|non-numeric-values"));
  exact = exact && missing_quotes.has_value() && bad_hierarchy.has_value() &&
          short_pose.has_value() && bad_pose.has_value() &&
          (*missing_quotes)->severity ==
              types::import_diagnostic_severity->drop &&
          tallies.size() == 4;
  if (inject_md5_mutation) exact = false;
  check(exact,
        "MD5 animation helpers preserve quoted names and component order, count flags exactly, reject malformed/non-numeric lines, and classify each diagnostic discriminator");
}

}  // namespace

int main(int argc, char** argv) {
  const flight::String mutation =
      argc > 1 ? flight::String(argv[1]) : flight::String();
  inject_header_mutation = mutation == flight::String("--mutate-awd2-header");
  inject_md2_mutation = mutation == flight::String("--mutate-md2-features");
  inject_obj_mutation = mutation == flight::String("--mutate-obj-references");
  inject_three_ds_mutation = mutation == flight::String("--mutate-3ds-census");
  inject_registry_mutation = mutation == flight::String("--mutate-gltf-registries");
  inject_material_mutation = mutation == flight::String("--mutate-mtl-helpers");
  inject_md5_mutation = mutation == flight::String("--mutate-md5-helpers");

  awd2_header_byte_offsets_and_failures();
  md2_feature_detection_and_failures();
  obj_material_reference_order();
  three_ds_nested_census_and_recovery();
  gltf_registry_order_replacement_and_decoder_lifecycle();
  material_defaults_colors_and_diagnostics();
  md5_line_parsing_component_order_and_diagnostics();
  if (failures != 0) {
    std::cerr << failures << " scene3d-formats assertion(s) failed\n";
    return 1;
  }
  std::cout << "scene3d-formats callable native evidence passed\n";
  return 0;
}
