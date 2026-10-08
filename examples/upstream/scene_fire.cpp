// Naive C++ port of @flighthq/example-scene-fire
// Ported from .dependencies/flight/examples/packages/scene-fire/src/app.ts

#include <flight/lighting/ambient_light.hpp>
#include <flight/lighting/point_light.hpp>
#include <flight/geometry/vector3.hpp>
#include <flight/mesh/mesh_geometry_builders.hpp>
#include <flight/scene3d/mesh.hpp>
#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/types/particle_emitter3_d.hpp>
#include <flight/particles/particle_emitter_config.hpp>
#include <flight/particles/particle_emitter_state.hpp>
#include <flight/camera/camera.hpp>
#include <flight/camera_controls/orbit_camera_controller.hpp>
#include <flight/camera/projection.hpp>
#include <flight/scene3d/scene.hpp>
#include <flight/materials/pbr_materials.hpp>
#include <flight/effects/bloom_effect.hpp>
#include <flight/effects/tone_map_effect.hpp>
#include <flight/effects/vignette_effect.hpp>
#include <flight/texture/texture.hpp>
#include <flight/textureatlas/texture_atlas.hpp>

#include <cmath>
#include <iostream>

int main() {
  using namespace flight::node;
  using namespace flight::mesh;
  using namespace flight::lighting;
  using namespace flight::particles;

  auto scene = flight::scene3d::create_node_3_d(flight::scene3d::Node3DKind);

  auto floorGeom = create_plane_mesh_geometry(8.0, 8.0);
  auto floorMat = flight::scene3d::create_standard_pbr_material({
    .baseColor = 0x333333ff,
    .metallic = 0.0,
    .roughness = 0.9,
  });
  auto floor = create_mesh(floorGeom, floorMat);
  add_node_child(scene, floor);

  auto pedestal = create_mesh(
    create_cylinder_mesh_geometry(0.3, 0.3, 0.8, 16),
    flight::scene3d::create_standard_pbr_material({
      .baseColor = 0x555555ff,
      .metallic = 0.2,
      .roughness = 0.6,
    }));
  pedestal->y = 0.4;
  invalidate_node_local_transform(pedestal);
  add_node_child(scene, pedestal);

  auto emitterConfig = create_particle_emitter_config({
    .maxParticles = 500.0,
    .spawnRate = 80.0,
    .lifetime = 1.5,
    .speed = 2.0,
    .startColor = 0xff6600ff,
    .endColor = 0xff000000,
    .startSize = 0.3,
    .endSize = 0.05,
  });

  auto emitterState = create_particle_emitter_state();
  auto emitter = create_particle_emitter_3_d(emitterConfig, emitterState);
  emitter->x = 0.0;
  emitter->y = 0.8;
  invalidate_node_local_transform(emitter);
  add_node_child(scene, emitter);

  prewarm_particle_emitter_3_d(emitter, emitterConfig, emitterState, 2.0);

  auto ambient = create_ambient_light({.color = 0x111111ff, .intensity = 0.2});
  add_node_child(scene, ambient);

  auto fireLight = create_point_light({
    .color = 0xff6622ff,
    .intensity = 5.0,
    .range = 8.0,
  });
  fireLight->x = 0.0;
  fireLight->y = 1.5;
  invalidate_node_local_transform(fireLight);
  add_node_child(scene, fireLight);

  auto camera = flight::scene3d::create_camera_3_d(
    flight::scene3d::create_perspective_projection(50.0, 800.0 / 600.0, 0.1, 50.0));
  auto orbitController = flight::scene3d::create_orbit_camera_controller(camera, {
    .distance = 5.0,
    .azimuth = 0.3,
    .elevation = 0.4,
  });

  auto bloom = flight::effects::create_bloom_effect({.threshold = 0.8, .intensity = 0.6});
  auto toneMap = flight::effects::create_tone_map_effect();
  auto vignette = flight::effects::create_vignette_effect({.intensity = 0.4});

  constexpr double dt = 1.0 / 60.0;
  for (int frame = 0; frame < 180; ++frame) {
    step_particle_emitter_3_d(emitter, emitterConfig, emitterState, dt);
    flight::scene3d::rotate_orbit_camera_controller(orbitController, 0.005, 0.0);
    flight::scene3d::update_orbit_camera_controller(orbitController, dt);
  }

  std::cout << "Flight scene-fire example (naive C++ port): "
            << "particle fire with bloom/tonemap/vignette effects.\n";
  return 0;
}
