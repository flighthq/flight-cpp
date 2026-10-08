// Naive C++ port of @flighthq/example-scene3d
// Ported from .dependencies/flight/examples/packages/scene3d/src/app.ts

#include <flight/lighting/ambient_light.hpp>
#include <flight/lighting/directional_light.hpp>
#include <flight/lighting/point_light.hpp>
#include <flight/geometry/vector3.hpp>
#include <flight/mesh/mesh_geometry_builders.hpp>
#include <flight/scene3d/mesh.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene3d/scene.hpp>
#include <flight/camera/camera.hpp>
#include <flight/camera_controls/orbit_camera_controller.hpp>
#include <flight/camera/projection.hpp>
#include <flight/materials/pbr_materials.hpp>

#include <cmath>
#include <iostream>

int main() {
  using namespace flight::node;
  using namespace flight::mesh;
  using namespace flight::lighting;

  // NOT AVAILABLE: generated mesh layout and required 3D constructors do not compile at this SDK pin.
  auto boxGeometry = create_box_mesh_geometry(1.0, 1.0, 1.0);
  auto sphereGeometry = create_sphere_mesh_geometry(0.5, 48, 32);
  auto coneGeometry = create_cone_mesh_geometry(0.5, 1.0, 32);
  auto planeGeometry = create_plane_mesh_geometry(10.0, 10.0);

  auto redMaterial = flight::scene3d::create_standard_pbr_material({
    .baseColor = 0xcc3333ff,
    .metallic = 0.0,
    .roughness = 0.4,
  });

  auto grayMetallicMaterial = flight::scene3d::create_standard_pbr_material({
    .baseColor = 0xaaaaaaff,
    .metallic = 1.0,
    .roughness = 0.3,
  });

  auto blueMaterial = flight::scene3d::create_standard_pbr_material({
    .baseColor = 0x3366ccff,
    .metallic = 0.0,
    .roughness = 0.5,
  });

  auto scene = flight::scene3d::create_node_3_d(flight::scene3d::Node3DKind);

  auto box = create_mesh(boxGeometry, redMaterial);
  box->x = -2.0;
  box->y = 0.5;
  invalidate_node_local_transform(box);
  add_node_child(scene, box);

  auto sphere = create_mesh(sphereGeometry, grayMetallicMaterial);
  sphere->x = 0.0;
  sphere->y = 0.5;
  invalidate_node_local_transform(sphere);
  add_node_child(scene, sphere);

  auto cone = create_mesh(coneGeometry, blueMaterial);
  cone->x = 2.0;
  cone->y = 0.5;
  invalidate_node_local_transform(cone);
  add_node_child(scene, cone);

  auto floor = create_mesh(planeGeometry, flight::scene3d::create_standard_pbr_material({
    .baseColor = 0x888888ff,
    .metallic = 0.0,
    .roughness = 0.8,
  }));
  add_node_child(scene, floor);

  auto ambient = create_ambient_light({
    .color = 0x404040ff,
    .intensity = 0.3,
  });
  add_node_child(scene, ambient);

  auto directional = create_directional_light({
    .color = 0xffffffff,
    .intensity = 1.0,
  });
  add_node_child(scene, directional);

  auto point = create_point_light({
    .color = 0xff8844ff,
    .intensity = 2.0,
    .range = 10.0,
  });
  point->x = 1.0;
  point->y = 3.0;
  invalidate_node_local_transform(point);
  add_node_child(scene, point);

  auto camera = flight::scene3d::create_camera_3_d(
    flight::scene3d::create_perspective_projection(60.0, 800.0 / 600.0, 0.1, 100.0));
  auto orbitController = flight::scene3d::create_orbit_camera_controller(camera, {
    .distance = 6.0,
    .azimuth = 0.5,
    .elevation = 0.3,
  });

  for (int frame = 0; frame < 120; ++frame) {
    double dt = 1.0 / 60.0;
    flight::scene3d::rotate_orbit_camera_controller(orbitController, 0.01, 0.0);
    flight::scene3d::update_orbit_camera_controller(orbitController, dt);
  }

  std::cout << "Flight scene3d example (naive C++ port): "
            << "3 meshes, 3 lights, orbit camera.\n";
  return 0;
}
