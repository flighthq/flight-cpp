// Consumer test for the @flighthq/scene3d-formats requirement tables at the current pin.
//
// Each table is a module-level `new Map([...])` of pair literals: the source writes `[feature, [requirement]]`
// and the emitter must give the pair the declared `std::tuple` element type. These cases read the tables the
// way a renderer registration pass does -- key order (Map preserves insertion order), the exact requirement
// each feature implies, the owner identity of the requirement, and absence for a feature the table does not
// carry -- so a change that only made the literal compile without preserving the mapping is caught here.
#include <flight/scene3d_formats/awd2_block_requirements.hpp>
#include <flight/scene3d_formats/collada_feature_requirements.hpp>
#include <flight/scene3d_formats/gltf_feature_requirements.hpp>
#include <flight/scene3d_formats/md2_feature_requirements.hpp>
#include <flight/scene3d_formats/md5_mesh_feature_requirements.hpp>
#include <flight/scene3d_formats/obj_feature_requirements.hpp>
#include <flight/scene3d_formats/three_ds_chunk_requirements.hpp>

#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
  return condition;
}

using RequirementsByFeature =
    flight::Map<flight::String, flight::Array<flight::Ref<flight::types::Requirement>>>;
using Requirement = flight::Ref<flight::types::Requirement>;

std::vector<flight::String> keys_of(const RequirementsByFeature& table) {
  std::vector<flight::String> keys;
  for (const auto& entry : table) keys.push_back(entry.first);
  return keys;
}

bool has_keys(const RequirementsByFeature& table, std::initializer_list<const char*> expected, const char* label) {
  const auto keys = keys_of(table);
  if (!check(keys.size() == expected.size(), label)) return false;
  std::size_t index = 0;
  for (const char* key : expected) {
    check(keys[index] == flight::String(key), label);
    ++index;
  }
  return true;
}

// A table entry is one requirement on the scene material-kind facet. `facet` is the shared facet record and
// `key` is the material kind the feature implies.
void check_requirement(const RequirementsByFeature& table, const char* feature, const char* kind, const char* label) {
  const auto entry = table.get(flight::String(feature));
  if (!check(entry.has_value(), label)) return;
  if (!check(entry->size() == 1.0, label)) return;
  const Requirement requirement = entry->element(0.0);
  check(requirement->facet == flight::String("scene.material-kind"), label);
  check(requirement->key == flight::String(kind), label);
}

void collada_table() {
  const auto& table = flight::scene3d_formats::collada_feature_scene_requirements;
  has_keys(table, {"Effect.Blinn", "Effect.Lambert", "Effect.Phong", "Material"}, "collada: key set and order");
  for (const char* feature : {"Effect.Blinn", "Effect.Lambert", "Effect.Phong", "Material"}) {
    check_requirement(table, feature, "StandardPbrMaterial", "collada: feature maps to StandardPbrMaterial");
  }
  const auto first = table.get(flight::String("Material"));
  const auto second = table.get(flight::String("Material"));
  check(first.has_value() && second.has_value() && first->element(0.0).get() == second->element(0.0).get(),
        "collada: the requirement owner is stable across lookups");
  check(!table.get(flight::String("Effect.Unknown")).has_value(), "collada: an absent feature is absent");
}

void gltf_table() {
  const auto& table = flight::scene3d_formats::gltf_feature_scene_requirements;
  has_keys(table, {"Mesh", "KHR_materials_unlit", "KHR_materials_pbrSpecularGlossiness"}, "gltf: key set and order");
  check_requirement(table, "Mesh", "StandardPbrMaterial", "gltf: Mesh maps to StandardPbrMaterial");
  check_requirement(table, "KHR_materials_unlit", "UnlitMaterial", "gltf: unlit maps to UnlitMaterial");
  check_requirement(table, "KHR_materials_pbrSpecularGlossiness", "SpecularGlossinessPbrMaterial",
                    "gltf: specular-glossiness maps to its own material");
}

void md2_table() {
  const auto& table = flight::scene3d_formats::md2_feature_scene_requirements;
  has_keys(table, {"Material"}, "md2: key set");
  check_requirement(table, "Material", "BlinnPhongMaterial", "md2: Material maps to BlinnPhongMaterial");
}

void md5_mesh_table() {
  const auto& table = flight::scene3d_formats::md5_mesh_feature_scene_requirements;
  has_keys(table, {"Material"}, "md5 mesh: key set");
  check_requirement(table, "Material", "BlinnPhongMaterial", "md5 mesh: Material maps to BlinnPhongMaterial");
}

void obj_table() {
  const auto& table = flight::scene3d_formats::obj_feature_scene_requirements;
  has_keys(table, {"MaterialBlinnPhong", "MaterialStandardPbr"}, "obj: key set and order");
  check_requirement(table, "MaterialBlinnPhong", "BlinnPhongMaterial", "obj: blinn-phong feature");
  check_requirement(table, "MaterialStandardPbr", "StandardPbrMaterial", "obj: standard-pbr feature");
}

void awd2_table() {
  const auto& table = flight::scene3d_formats::awd2_block_scene_requirements;
  has_keys(table, {"Material"}, "awd2: key set");
  check_requirement(table, "Material", "ShadedMaterial", "awd2: Material maps to ShadedMaterial");
}

void three_ds_table() {
  const auto& table = flight::scene3d_formats::three_ds_chunk_scene_requirements;
  const flight::String material_chunk = flight::scene3d_formats::get_three_ds_chunk_name(0xafff);
  const auto keys = keys_of(table);
  check(keys.size() == 1.0 && keys[0] == material_chunk, "three-ds: the material chunk is the only entry");
  if (!check(keys.size() == 1.0, "three-ds: one entry")) return;
  check_requirement(table, material_chunk.to_utf8().c_str(), "BlinnPhongMaterial",
                    "three-ds: material chunk maps to BlinnPhongMaterial");
}

} // namespace

int main() {
  collada_table();
  gltf_table();
  md2_table();
  md5_mesh_table();
  obj_table();
  awd2_table();
  three_ds_table();
  if (failures != 0) {
    std::cerr << failures << " failure(s)" << '\n';
    return 1;
  }
  std::cout << "scene3d-formats requirement tables pass" << '\n';
  return 0;
}
