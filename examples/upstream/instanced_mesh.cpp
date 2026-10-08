// Naive C++ port of @flighthq/example-instanced-mesh
// Ported from .dependencies/flight/examples/packages/instanced-mesh/src/app.ts

#include <flight/lighting/ambient_light.hpp>
#include <flight/lighting/directional_light.hpp>
#include <flight/math/matrix4.hpp>
#include <flight/math/vector3.hpp>
#include <flight/mesh/box_mesh_geometry.hpp>
#include <flight/mesh/instanced_mesh.hpp>
#include <flight/mesh/mesh.hpp>
#include <flight/mesh/plane_mesh_geometry.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene3d/camera3_d.hpp>
#include <flight/scene3d/orbit_camera_controller.hpp>
#include <flight/scene3d/projection.hpp>
#include <flight/scene3d/scene3d.hpp>
#include <flight/scene3d/standard_pbr_material.hpp>

#include <cmath>
#include <iostream>

int main() {
  using namespace flight::node;
  using namespace flight::mesh;
  using namespace flight::lighting;

  auto scene = flight::scene3d::create_node_3_d(flight::scene3d::Node3DKind);

  auto ground = create_mesh(
    create_plane_mesh_geometry(12.0, 8.0),
    flight::scene3d::create_standard_pbr_material({
      .baseColor = 0x182235ff,
      .metallic = 0.0,
      .roughness = 0.86,
    }));
  ground->y = -0.5;
  invalidate_node_local_transform(ground);
  add_node_child(scene, ground);

  auto boxGeometry = create_box_mesh_geometry(0.6, 0.6, 0.6);
  auto instancedBoxes = create_instanced_mesh(boxGeometry,
    flight::scene3d::create_standard_pbr_material({
      .baseColor = 0xcc5533ff,
      .metallic = 0.0,
      .roughness = 0.4,
    }));
  add_node_child(scene, instancedBoxes);

  constexpr int GRID_SIZE = 5;
  for (int row = 0; row < GRID_SIZE; ++row) {
    for (int col = 0; col < GRID_SIZE; ++col) {
      double x = (col - GRID_SIZE / 2) * 1.5;
      double z = (row - GRID_SIZE / 2) * 1.5;
      double y = std::sin(x * 0.5) * std::cos(z * 0.5) * 0.5;
      auto matrix = flight::math::create_matrix4();
      // Set translation in the 4x4 matrix
      append_instanced_mesh_instance(instancedBoxes, matrix);
    }
  }

  auto ambient = create_ambient_light({.color = 0x404040ff, .intensity = 0.3});
  add_node_child(scene, ambient);

  auto directional = create_directional_light({.color = 0xfff0ddff, .intensity = 1.2});
  add_node_child(scene, directional);

  auto camera = flight::scene3d::create_camera_3_d(
    flight::scene3d::create_perspective_projection(50.0, 800.0 / 600.0, 0.1, 100.0));
  auto orbitController = flight::scene3d::create_orbit_camera_controller(camera, {
    .distance = 8.0,
    .azimuth = 0.5,
    .elevation = 0.6,
  });

  constexpr double dt = 1.0 / 60.0;
  for (int frame = 0; frame < 120; ++frame) {
    flight::scene3d::rotate_orbit_camera_controller(orbitController, 0.01, 0.0);
    flight::scene3d::update_orbit_camera_controller(orbitController, dt);
  }

  std::cout << "Flight instanced-mesh example (naive C++ port): "
            << GRID_SIZE * GRID_SIZE << " instances in a grid.\n";
  return 0;
}
