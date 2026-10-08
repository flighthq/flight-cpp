// Naive C++ port of @flighthq/example-materialshowcase
// Ported from .dependencies/flight/examples/packages/materialshowcase/src/app.ts

#include <flight/lighting/ambient_light.hpp>
#include <flight/lighting/directional_light.hpp>
#include <flight/lighting/point_light.hpp>
#include <flight/materials/pbr_materials.hpp>
#include <flight/materials/classic_materials.hpp>
#include <flight/materials/unlit_materials.hpp>
#include <flight/materials/extended_pbr_material.hpp>
#include <flight/materials/anisotropy_pbr_extension.hpp>
#include <flight/materials/clearcoat_pbr_extension.hpp>
#include <flight/materials/iridescence_pbr_extension.hpp>
#include <flight/materials/sheen_pbr_extension.hpp>
#include <flight/materials/specular_pbr_extension.hpp>
#include <flight/materials/wrapped_diffuse_pbr_extension.hpp>
#include <flight/materials/transmission_volume_pbr_extension.hpp>
#include <flight/scene3d/mesh.hpp>
#include <flight/mesh/mesh_geometry_builders.hpp>
#include <flight/scene3d/scene.hpp>
#include <flight/camera/camera.hpp>
#include <flight/camera/projection.hpp>
#include <flight/camera_controls/orbit_camera_controller.hpp>
#include <flight/geometry/vector3.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>

#include <cmath>
#include <iostream>
#include <string_view>
#include <vector>

int main() {
  using namespace flight::node;
  using namespace flight::mesh;
  using namespace flight::scene3d;
  using namespace flight::camera;
  using namespace flight::camera_controls;
  using namespace flight::lighting;
  using namespace flight::material;
  using namespace flight::math;

  auto geometry = create_sphere_mesh_geometry(0.68, 36, 24);

  auto standard = create_standard_pbr_material({
    .baseColor = 0x4bbce8ff,
    .metallic = 0.25,
    .roughness = 0.28,
  });

  auto specGloss = create_specular_glossiness_pbr_material({
    .diffuse = 0xc05cffff,
    .glossiness = 0.72,
    .specular = 0xd8e7ffff,
  });

  auto blinnPhong = create_blinn_phong_material({
    .diffuse = 0xea4f68ff,
    .shininess = 72.0,
    .specular = 0xffd8deff,
  });

  auto lambert = create_lambert_material({
    .diffuse = 0xeab44fff,
    .emissive = 0x160d00ff,
  });

  auto phong = create_phong_material({
    .diffuse = 0x7ed259ff,
    .shininess = 64.0,
    .specular = 0xd8ffd0ff,
  });

  auto toon = create_toon_material({.baseColor = 0x7d66ffff, .steps = 4});
  auto unlit = create_unlit_material({.baseColor = 0xff794dff});
  auto emissive = create_emissive_material({.emissive = 0x35d7ffff, .emissiveStrength = 1.8});
  auto depth = create_depth_material({.far = 24.0, .near = 0.1});
  auto wireframe = create_wireframe_material({.color = 0xf0f5ffff, .thickness = 1.5});

  struct MaterialEntry {
    std::string_view name;
    std::uint32_t color;
  };

  std::vector<MaterialEntry> entries = {
    {"Standard PBR", 0x4bbce8ff},
    {"Specular-glossiness", 0xc05cffff},
    {"Blinn-Phong", 0xea4f68ff},
    {"Lambert", 0xeab44fff},
    {"Phong", 0x7ed259ff},
    {"Toon", 0x7d66ffff},
    {"Unlit", 0xff794dff},
    {"Emissive", 0x35d7ffff},
    {"Depth", 0x000000ff},
    {"Wireframe", 0xf0f5ffff},
  };

  // NOT AVAILABLE: generated mesh layout and required 3D constructors do not compile at this SDK pin.
  auto scene = create_node_3_d(flight::types::Node3DKind);

  for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
    auto mesh = create_mesh(geometry, {standard});
    mesh->position.x = (static_cast<double>(i % 5) - 2.0) * 1.75 - 0.7;
    mesh->position.y = (1.5 - std::floor(static_cast<double>(i) / 5.0)) * 1.65;
    invalidate_node_local_transform(mesh);
    add_node_child(scene, mesh);
  }

  auto selectionRing = create_mesh(
    create_torus_mesh_geometry(0.8, 0.042, 12, 64),
    {create_unlit_material({.baseColor = 0x7ef7c8ff})}
  );
  add_node_child(scene, selectionRing);

  auto camera = create_camera_3_d({
    .far = 60.0,
    .near = 0.1,
    .projection = create_perspective_projection({
      .aspect = 800.0 / 600.0,
      .fovY = M_PI / 4.0,
    }),
  });

  auto cameraController = create_orbit_camera_controller({
    .azimuth = 0.0,
    .distance = 12.7,
    .maxDistance = 18.0,
    .minDistance = 8.0,
    .polar = 0.03,
    .smoothTime = 0.12,
    .target = create_vector_3(-0.7, 0.0, 0.0),
  });
  update_orbit_camera_controller(cameraController, camera, 1.0);

  auto directionDir = create_vector_3(-0.75, -0.9, -0.5);
  normalize_vector_3(directionDir, directionDir);

  auto ambient = create_ambient_light({.color = 0x59709fff, .intensity = 0.18});
  auto directional = create_directional_light({
    .color = 0xffe4c4ff,
    .direction = directionDir,
    .intensity = 2.2,
  });

  auto pointLight1 = create_point_light({
    .color = 0x56c8ffff,
    .intensity = 22.0,
    .position = create_vector_3(-4.0, 2.8, 3.0),
    .range = 14.0,
  });

  auto pointLight2 = create_point_light({
    .color = 0xff5fa8ff,
    .intensity = 13.0,
    .position = create_vector_3(3.6, -2.0, 2.2),
    .range = 11.0,
  });

  std::cout << "Flight materialshowcase example (naive C++ port): "
            << entries.size() << " materials on sphere geometry.\n";
  return 0;
}
