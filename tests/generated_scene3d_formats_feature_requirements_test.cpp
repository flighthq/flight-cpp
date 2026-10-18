// The original scene3d-formats feature-requirement headers, consumed natively: the MD5 mesh and OBJ
// requirement helpers call the feature collectors whose bodies were previously refused. The scans
// read the text line by line, so the loop-bound indexed element member is what makes them consumable.
#include <flight/scene3d_formats/md5_mesh_requirements.hpp>
#include <flight/scene3d_formats/obj_requirements.hpp>
#include <flight/types/requirement.hpp>

#include <iostream>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

bool has_requirement(const flight::Ref<flight::types::RequirementSet>& set, const char* key) {
  for (const auto& requirement : set->requirements) {
    if (flight::row_get<flight::RowKey<"key">>(requirement) == flight::String(key)) return true;
  }
  return false;
}

} // namespace

int main() {
  // MD5: a joints section and a mesh whose shader names a material.
  auto md5 = flight::scene3d_formats::parse_md5_mesh_requirements(
      flight::String("joints {\n  \"root\" -1\n}\nmesh {\n  shader \"brick\"\n}\n"));
  check(md5 != nullptr, "parse_md5_mesh_requirements returns a set");
  check(md5->requirements.size() != 0.0, "the MD5 scan finds requirements");
  check(has_requirement(md5, "md5.Skeleton"), "the joints section reports md5.Skeleton");
  check(has_requirement(md5, "md5.Mesh"), "the mesh section reports md5.Mesh");

  // An empty MD5 text recognizes nothing.
  auto empty = flight::scene3d_formats::parse_md5_mesh_requirements(flight::String(""));
  check(empty != nullptr, "an empty MD5 text returns a set");
  check(empty->requirements.size() == 0.0, "an empty MD5 text reports no requirements");

  // OBJ: a face directive evidences geometry; usemtl evidences material use.
  auto obj = flight::scene3d_formats::parse_obj_requirements(
      flight::String("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\nusemtl brick\n"));
  check(obj != nullptr, "parse_obj_requirements returns a set");
  check(obj->requirements.size() != 0.0, "the OBJ scan finds requirements");
  check(has_requirement(obj, "obj.Face"), "the face directive reports obj.Face");

  auto obj_empty = flight::scene3d_formats::parse_obj_requirements(flight::String("# comment only\n"));
  check(obj_empty != nullptr, "a comment-only OBJ text returns a set");
  check(obj_empty->requirements.size() == 0.0, "a comment-only OBJ text reports no requirements");

  if (failures != 0) {
    std::cerr << failures << " feature requirement check(s) failed\n";
    return 1;
  }
  std::cout << "md5 and obj feature requirements scan the original public headers\n";
  return 0;
}
