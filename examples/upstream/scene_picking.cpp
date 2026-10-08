// Naive C++ port of @flighthq/example-scene-picking
// Ported from .dependencies/flight/examples/packages/scene-picking/src/app.ts

#include <flight/lighting/ambient_light.hpp>
#include <flight/lighting/directional_light.hpp>
#include <flight/geometry/vector3.hpp>
#include <flight/mesh/mesh_geometry_builders.hpp>
#include <flight/scene3d/mesh.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/camera/camera.hpp>
#include <flight/camera_controls/orbit_camera_controller.hpp>
#include <flight/picking/pick_scene3_d.hpp>
#include <flight/camera/projection.hpp>
#include <flight/scene3d/scene.hpp>
#include <flight/materials/pbr_materials.hpp>

#include <array>
#include <cmath>
#include <iostream>

namespace {

constexpr std::array<std::uint32_t, 6> COLORS = {
  0xe95f6dff, 0x44c6d8ff, 0xf1af4bff, 0x8d71eaff, 0x62c979ff, 0xe076b4ff,
};

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::mesh;
  using namespace flight::lighting;

  // NOT AVAILABLE: generated mesh layout and required 3D constructors do not compile at this SDK pin.
  auto scene = flight::scene3d::create_node_3_d(flight::scene3d::Node3DKind);

  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      int index = row * 3 + col;
      std::uint32_t baseColor = COLORS[index % COLORS.size()];
      auto material = flight::scene3d::create_standard_pbr_material({
        .baseColor = baseColor,
        .metallic = 0.15,
        .roughness = 0.3,
      });

      auto geometry = [&]() {
        if (index % 3 == 0) return create_box_mesh_geometry(1.05, 1.05, 1.05);
        if (index % 3 == 1) return create_sphere_mesh_geometry(0.55, 32, 24);
        return create_torus_mesh_geometry(0.45, 0.15, 24, 16);
      }();

      auto mesh = create_mesh(geometry, material);
      mesh->x = (col - 1) * 2.5;
      mesh->y = 0.6;
      mesh->z = (row - 1) * 2.5;
      invalidate_node_local_transform(mesh);
      add_node_child(scene, mesh);
    }
  }

  auto ambient = create_ambient_light({.color = 0x404060ff, .intensity = 0.4});
  add_node_child(scene, ambient);

  auto directional = create_directional_light({.color = 0xffffeeff, .intensity = 1.0});
  add_node_child(scene, directional);

  auto camera = flight::scene3d::create_camera_3_d(
    flight::scene3d::create_perspective_projection(50.0, 800.0 / 600.0, 0.1, 100.0));
  auto orbitController = flight::scene3d::create_orbit_camera_controller(camera, {
    .distance = 8.0,
    .azimuth = 0.4,
    .elevation = 0.5,
  });

  auto hit = flight::scene3d::create_scene_3_d_hit();
  flight::scene3d::pick_scene_3_d(scene, camera, 400.0, 300.0, 800.0, 600.0, hit);

  constexpr double dt = 1.0 / 60.0;
  for (int frame = 0; frame < 60; ++frame) {
    flight::scene3d::rotate_orbit_camera_controller(orbitController, 0.02, 0.0);
    flight::scene3d::update_orbit_camera_controller(orbitController, dt);
  }

  std::cout << "Flight scene-picking example (naive C++ port): "
            << "9 pickable meshes in a 3x3 grid.\n";
  return 0;
}
