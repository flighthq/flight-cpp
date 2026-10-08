// Naive C++ port of @flighthq/example-particles
// Ported from .dependencies/flight/examples/packages/particles/src/app.ts

#include <flight/node/hierarchy.hpp>
#include <flight/node/node_transform2d.hpp>
#include <flight/particles/particle_emitter2d.hpp>
#include <flight/particles/particle_config.hpp>
#include <flight/particles/particle_curve.hpp>
#include <flight/particles/particle_forces.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/text/text_label.hpp>
#include <flight/texture/texture.hpp>
#include <flight/texture/texture_atlas.hpp>

#include <cmath>
#include <iostream>

namespace {

constexpr double WIDTH = 800.0;
constexpr double HEIGHT = 500.0;

} // namespace

int main() {
  using namespace flight::node;
  using namespace flight::scene2d;
  using namespace flight::particles;

  auto root = create_display_object(std::nullopt);

  // Fire emitter.
  auto fireEmitter = create_particle_emitter_2_d();
  fireEmitter->x = WIDTH / 2.0;
  fireEmitter->y = HEIGHT * 0.75;
  invalidate_node_local_transform(fireEmitter);
  add_node_child(root, fireEmitter);

  auto fireScaleCurve = build_particle_curve([](double t) {
    double pop = 1.0 + std::sin(t * M_PI) * 0.3;
    return pop * (1.0 - t);
  });
  auto fireAlphaCurve = build_particle_curve([](double t) {
    return 1.0 - t * t;
  });

  auto fireConfig = create_particle_emitter_config({
    .worldSpace = true,
    .velocityInheritance = 0.35,
    .spawnRate = 300.0,
    .lifetimeMin = 0.2,
    .lifetimeMax = 0.55,
    .speedMin = 40.0,
    .speedMax = 130.0,
    .spread = M_PI * 2.0,
    .directionX = 0.0,
    .directionY = -1.0,
    .gravityX = 0.0,
    .gravityY = 200.0,
    .alphaCurve = fireAlphaCurve,
    .scaleCurve = fireScaleCurve,
    .scaleMin = 0.4,
    .scaleMax = 1.4,
    .maxParticles = 3000,
  });

  auto fireState = create_particle_emitter_state();

  // Snow emitter.
  auto snowEmitter = create_particle_emitter_2_d();
  snowEmitter->x = WIDTH * 0.75;
  snowEmitter->y = 0.0;
  invalidate_node_local_transform(snowEmitter);
  add_node_child(root, snowEmitter);

  auto snowScaleCurve = build_particle_curve([](double t) {
    if (t < 0.1) return t / 0.1;
    if (t > 0.8) return (1.0 - t) / 0.2;
    return 1.0;
  });
  auto snowAlphaCurve = build_particle_curve([](double t) {
    if (t < 0.1) return t / 0.1;
    return 1.0 - t * 0.6;
  });

  auto snowConfig = create_particle_emitter_config({
    .worldSpace = true,
    .spawnRate = 80.0,
    .lifetimeMin = 2.0,
    .lifetimeMax = 4.0,
    .speedMin = 15.0,
    .speedMax = 40.0,
    .spread = M_PI * 0.6,
    .directionX = 0.0,
    .directionY = 1.0,
    .gravityX = 0.0,
    .gravityY = 20.0,
    .scaleMin = 0.3,
    .scaleMax = 0.9,
    .maxParticles = 500,
  });

  auto snowState = create_particle_emitter_state();

  // HUD label.
  auto label = flight::text::create_text_label();
  label->data.text = flight::String("PARTICLE EMITTER");
  label->x = 20.0;
  label->y = 20.0;
  invalidate_node_local_transform(label);
  add_node_child(root, label);

  // Simulate particle updates.
  for (int frame = 0; frame < 120; ++frame) {
    double dt = 1.0 / 60.0;
    update_particle_emitter_2_d(fireEmitter, fireConfig, fireState, dt);
    apply_particle_forces(fireState, dt);
    update_particle_emitter_2_d(snowEmitter, snowConfig, snowState, dt);
    apply_particle_forces(snowState, dt);
  }

  std::cout << "Flight particles example (naive C++ port): "
            << "2 emitters (fire + snow), simulated 120 frames.\n";
  return 0;
}
