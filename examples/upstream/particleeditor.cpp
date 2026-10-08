// Naive C++ port of @flighthq/example-particleeditor
// Ported from .dependencies/flight/examples/packages/particleeditor/src/app.ts

#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/text/text_label.hpp>
#include <flight/texture/texture.hpp>
#include <flight/textureatlas/texture_atlas.hpp>
#include <flight/types/particle_emitter2_d.hpp>
#include <flight/particles/particle_emitter_config.hpp>
#include <flight/particles/particle_emitter_state.hpp>
#include <flight/particles/apply_particle_forces.hpp>
#include <flight/particles/curve.hpp>
#include <flight/spring/spring.hpp>
#include <flight/spring/spring_config.hpp>

#include <cmath>
#include <iostream>

int main() {
  using namespace flight::node;
  using namespace flight::scene2d;
  using namespace flight::particles;
  using namespace flight::spring;
  using namespace flight::texture;

  constexpr double WIDTH = 800.0;
  constexpr double HEIGHT = 600.0;

  // NOT AVAILABLE: generated Scene2D construction/hierarchy is partial at this SDK pin.
  auto root = create_display_object(std::nullopt);

  auto atlas = create_texture_atlas({});
  add_texture_atlas_region(atlas, 0, 0, 16, 16);

  auto config = create_particle_emitter_config();
  config->maxParticles = 500.0;
  config->spawnRate = 120.0;
  config->lifetime = 1.2;
  config->startSpeed = 180.0;
  config->startSpeedVariance = 40.0;
  config->startAngle = -M_PI / 2.0;
  config->startAngleVariance = 0.35;

  auto scaleCurve = build_particle_curve({1.0, 0.6, 0.0});
  auto alphaCurve = build_particle_curve({1.0, 0.8, 0.0});
  auto colorCurve = particle_color_curve_from_keyframes({
    {0.0, 0xffffdcff},
    {0.3, 0xff8833ff},
    {0.7, 0xcc2200ff},
    {1.0, 0x440000ff},
  });

  auto emitter = create_particle_emitter_2_d(config, atlas);
  emitter->x = WIDTH / 2.0;
  emitter->y = HEIGHT * 0.75;
  invalidate_node_local_transform(emitter);
  add_node_child(root, emitter);

  auto state = create_particle_emitter_state();

  auto springCfg = create_spring_config(5.0, 0.6);
  auto spring2D = create_spring_2_d(WIDTH / 2.0, HEIGHT * 0.75);

  // Gravity force
  flight::types::ParticleForce gravity;
  gravity.x = 0.0;
  gravity.y = 50.0;

  // Simulate frames
  for (int frame = 0; frame < 180; ++frame) {
    double dt = 1.0 / 60.0;
    update_spring_2_d(spring2D, WIDTH / 2.0, HEIGHT * 0.75, springCfg, dt);
    emitter->x = spring2D->x.value;
    emitter->y = spring2D->y.value;
    invalidate_node_local_transform(emitter);
    update_particle_emitter_2_d(emitter, dt);
    apply_particle_forces(emitter, {gravity}, dt);
  }

  // Title
  auto title = flight::text::create_text_label();
  title->data.text = flight::String("Particle Editor");
  title->x = 24.0;
  title->y = 18.0;
  invalidate_node_local_transform(title);
  add_node_child(root, title);

  std::cout << "Flight particleeditor example (naive C++ port): "
            << "emitter at (" << spring2D->x.value << ", " << spring2D->y.value
            << ") after 180 frames.\n";
  return 0;
}
