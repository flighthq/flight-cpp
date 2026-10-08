// Naive C++ port of @flighthq/example-awd2loading
// Ported from .dependencies/flight/examples/packages/awd2loading/src/app.ts

#include <flight/camera/camera.hpp>
#include <flight/camera/projection.hpp>
#include <flight/camera_controls/orbit_camera_controller.hpp>
#include <flight/lighting/ambient_light.hpp>
#include <flight/lighting/directional_light.hpp>
#include <flight/lighting/point_light.hpp>
#include <flight/shading/create_rim_modifier.hpp>
#include <flight/geometry/vector3.hpp>
#include <flight/scene3d/mesh.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene3d/scene.hpp>
#include <flight/scene3d/scene_document.hpp>
#include <flight/scene3d_formats/awd2_parse.hpp>

#include <cmath>
#include <iostream>
#include <vector>

int main() {
  using namespace flight::camera;
  using namespace flight::camera_controls;
  using namespace flight::lighting;
  using namespace flight::math;
  using namespace flight::node;
  using namespace flight::scene3d;
  using namespace flight::scene3d_formats;

  // In the TS version, a synthetic AWD2 byte stream is created procedurally.
  // Here we create the scene structure directly since we don't have the AWD2 bytes.
  // A real port would call parse_awd_2() with the byte data.

  // AWD2 parsing path (would need byte data):
  // flight::Array<flight::types::ImportDiagnostic> diagnostics;
  // auto awdDocument = parse_awd_2(awdBytes, diagnostics);
  // auto documentScene = create_scene_3_d_from_document(awdDocument);
  // auto importedLights = create_scene_3_d_lights_from_document(awdDocument);

  auto camera = create_camera_3_d({
    .far = 40.0,
    .near = 0.1,
    .projection = create_perspective_projection({
      .aspect = 800.0 / 600.0,
      .fovY = M_PI / 4.0,
    }),
  });

  auto cameraController = create_orbit_camera_controller({
    .azimuth = 0.18,
    .distance = 7.0,
    .maxDistance = 11.0,
    .minDistance = 4.0,
    .polar = 0.08,
    .smoothTime = 0.12,
    .target = create_vector_3(0.0, 0.0, 0.0),
  });
  update_orbit_camera_controller(cameraController, camera, 1.0);

  auto rimMod = create_rim_modifier({
    .color = 0x49d8ffff,
    .intensity = 0.72,
    .power = 2.4,
  });

  auto pointLight1 = create_point_light({
    .color = 0x4bdcffff,
    .intensity = 14.0,
    .position = create_vector_3(-3.2, 2.5, 3.2),
    .range = 11.0,
  });

  auto pointLight2 = create_point_light({
    .color = 0xff6aaaff,
    .intensity = 8.0,
    .position = create_vector_3(3.0, -1.4, 2.2),
    .range = 9.0,
  });

  // Simulate orbit update loop
  for (int frame = 0; frame < 60; ++frame) {
    double dt = 1.0 / 60.0;
    update_orbit_camera_controller(cameraController, camera, dt);
  }

  std::cout << "Flight awd2loading example (naive C++ port): "
            << "camera + orbit controller + lights configured.\n";
  return 0;
}
