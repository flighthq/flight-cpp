// The generated @flighthq/particles at a627849, exercised natively. Twelve modules are emitted, and two are
// carried by overrides:
// - validateParticleEmitterConfig, supplied by particles-validate-particle-emitter-config;
// - getParticleEmitterSignals, corrected by particles-signals-missing-slot-reads-null.
// accumulateForces' early exits are the particles-forces-case-early-exit-guards source patch.
//
// Ported from packages/particles/src/*.test.ts, module by module. Upstream's injected random functions are
// replayed as fixed sequences, and createRandomSource(12345) is @flighthq/math's own.
//
// Additions the source's semantics imply but upstream does not assert are marked "Addition:". They cover:
// - validateParticleEmitterConfig's exact issue list, order and message text;
// - normalizeParticleEmitterConfig's every clamp, new-object identity and curve identity;
// - the per-spawn random draw order and the values drawn;
// - velocity inheritance, burst repetition and fractional spawn carry;
// - falloff modes, and the turbulence hash bit for bit against an independent port;
// - later forces still applying after an early-exiting one;
// - multi-collider resolution order;
// - capacity growth preserving contents.
//
// The emitter's display node and renderer are out of scope. The force and collision passes read only an
// emitter's SoA `data`, so a node-free ParticleEmitter2D fixture stands in for one, as upstream's does.
#include <flight/math/random.hpp>
#include <flight/particles/apply_particle_collisions.hpp>
#include <flight/particles/apply_particle_forces.hpp>
#include <flight/particles/curve.hpp>
#include <flight/particles/particle_emitter_config.hpp>
#include <flight/particles/particle_emitter_signals.hpp>
#include <flight/particles/particle_emitter_state.hpp>
#include <flight/particles/particle_objects_state.hpp>
#include <flight/particles/particle_spawn_offset.hpp>
#include <flight/particles/step_particle_objects.hpp>
#include <flight/particles/update_particle_objects.hpp>
#include <flight/particles/validate_particle_emitter_config.hpp>

#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <numbers>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

namespace p = flight::particles;
using flight::Float32Array;
using flight::Ref;
using flight::String;
using flight::types::ParticleEmitterConfig;

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double inf = std::numeric_limits<double>::infinity();

bool near(double a, double b, double epsilon = 1e-5) { return std::abs(a - b) <= epsilon; }

// The anonymous partial-input shape createParticleEmitterConfig accepts, recovered from its signature.
template <typename>
struct FirstParameter;
template <typename Result, typename Parameter>
struct FirstParameter<Result (*)(Parameter)> {
  using type = Parameter;
};
using InputRef = FirstParameter<decltype(&p::create_particle_emitter_config)>::type::value_type;
using Input = InputRef::element_type;

Ref<ParticleEmitterConfig> config(const std::function<void(Input&)>& fill = [](Input&) {}) {
  Input input;
  fill(input);
  return p::create_particle_emitter_config(flight::make_ref<Input>(std::move(input)));
}

InputRef input(const std::function<void(Input&)>& fill) {
  Input value;
  fill(value);
  return flight::make_ref<Input>(std::move(value));
}

flight::Array<double> curve(std::initializer_list<double> values) { return flight::Array<double>(values); }

// A RandomSource replaying `values` in order; a draw past the end reads NaN, so an unexpected extra draw shows.
struct Sequence {
  std::shared_ptr<std::vector<double>> values;
  std::shared_ptr<std::size_t> index = std::make_shared<std::size_t>(0);
  explicit Sequence(std::vector<double> v) : values(std::make_shared<std::vector<double>>(std::move(v))) {}
  flight::types::RandomSource source() const {
    auto vs = values;
    auto at = index;
    return [vs, at]() { return *at < vs->size() ? (*vs)[(*at)++] : nan; };
  }
  std::size_t draws() const { return *index; }
};

void particle_emitter_config() {
  const auto c = p::create_particle_emitter_config();
  check(c->alpha_end == 0 && c->alpha_start == 1 && c->burst_count == 0 && c->burst_interval == 0 && c->duration == 0 && c->loop,
        "defaults: alpha, burst, duration and loop");
  check(c->color_end_r == 1 && c->color_end_g == 1 && c->color_end_b == 1 && c->color_start_r == 1 && c->color_start_g == 1 &&
            c->color_start_b == 1,
        "defaults: colors are white");
  check(c->direction_x == 0 && c->direction_y == -1 && c->direction_z == 0 && c->emitter_shape == String("point") &&
            c->emitter_radius == 0 && c->emitter_width == 0 && c->emitter_height == 0 && c->emitter_cone_angle == 0 &&
            c->emitter_depth == 0,
        "defaults: direction and emitter shape");
  check(c->frame_count == 1 && c->frame_rate == 12 && c->gravity_x == 0 && c->gravity_y == 0 && c->gravity_z == 0,
        "defaults: frames and gravity");
  check(c->lifetime_max == 1 && c->lifetime_min == 0.5 && c->max_particles == 1000 && c->region_id_max == 1 && c->region_id_min == 0,
        "defaults: lifetime, capacity and regions");
  check(c->rotation_speed_min == 0 && c->rotation_speed_max == 0 && c->scale_end == 1 && c->velocity_inheritance == 0 &&
            !c->world_space && !c->blend_mode.has_value(),
        "defaults: rotation, scale end, inheritance, world space, blend mode null");
  check(c->color_start_variance_r == 0 && c->color_start_variance_g == 0 && c->color_start_variance_b == 0 &&
            c->color_end_variance_r == 0 && c->color_end_variance_g == 0 && c->color_end_variance_b == 0,
        "defaults: no color variance");
  check(c->scale_max == 1 && c->scale_min == 1 && c->speed_max == 100 && c->speed_min == 50 && c->spawn_rate == 10 &&
            c->spread == std::numbers::pi,
        "defaults: scale, speed, spawn rate and spread");
  check(!c->alpha_curve.has_value() && !c->color_curve.has_value() && !c->scale_curve.has_value(), "defaults: no curves");

  const auto o = config([](Input& i) {
    i.spawn_rate = 60;
    i.max_particles = 500;
  });
  check(o->spawn_rate == 60 && o->max_particles == 500 && o->alpha_end == 0, "overrides individual fields, the rest default");
  check(p::create_particle_emitter_config() != p::create_particle_emitter_config(), "returns a new object each call");

  // Addition: a supplied curve is held by reference, and an explicit null curve is null.
  const auto alpha = curve({1, 0});
  const auto withCurve = config([&alpha](Input& i) {
    i.alpha_curve = alpha;
    i.scale_curve = flight::null;
  });
  check(withCurve->alpha_curve.has_value() && withCurve->alpha_curve->identity() == alpha.identity() &&
            !withCurve->scale_curve.has_value(),
        "a supplied curve keeps its identity and a null curve reads null");
}

void particle_emitter_signals() {
  const auto signals = p::create_particle_emitter_signals();
  check(signals->on_particle_spawn != nullptr && signals->on_particle_death != nullptr && signals->on_emitter_complete != nullptr,
        "creates a signals group with all three signal slots");
  const auto state = p::create_particle_emitter_state();
  const auto read = [](const Ref<flight::types::ParticleEmitterState>& s) -> std::optional<std::optional<Ref<flight::types::ParticleEmitterSignals>>> {
    try {
      return p::get_particle_emitter_signals(s);
    } catch (const std::exception&) {
      return std::nullopt;
    }
  };
  const auto before = read(state);
  check(before.has_value() && !before->has_value(), "returns null from getParticleEmitterSignals before enablement, without throwing");
  const auto a = p::enable_particle_emitter_signals(state);
  const auto b = p::enable_particle_emitter_signals(state);
  check(a == b, "enableParticleEmitterSignals returns the same object on repeated calls");
  const auto got = p::get_particle_emitter_signals(state);
  check(got.has_value() && *got == a, "getParticleEmitterSignals returns the enabled object after enablement");
  // Addition: signals are per state.
  const auto other = p::create_particle_emitter_state();
  const auto otherBefore = read(other);
  check(otherBefore.has_value() && !otherBefore->has_value() && p::enable_particle_emitter_signals(other) != a,
        "another state has its own signals");
}

void particle_emitter_state() {
  const auto s = p::create_particle_emitter_state();
  check(s->lifetimes.size() == 0 && s->velocities.size() == 0 && s->scales.size() == 0 && s->rotation_speeds.size() == 0 &&
            s->color_birth.size() == 0 && s->color_death.size() == 0,
        "returns empty arrays");
  check(s->spawn_accumulator == 0 && s->burst_timer == 0 && s->emitter_age == 0 && std::isnan(s->prev_x) && std::isnan(s->prev_y) &&
            std::isnan(s->prev_z),
        "zero accumulator and burst timer, NaN previous position");
  check(p::create_particle_emitter_state() != p::create_particle_emitter_state(), "returns a new object each call");
  // Addition: an injected random source is the one the state draws from.
  Sequence seq({0.125});
  const auto seeded = p::create_particle_emitter_state(seq.source());
  check(seeded->random() == 0.125 && seq.draws() == 1, "the injected random source is kept");

  {
    auto t = p::create_particle_emitter_state();
    p::ensure_particle_emitter_state_capacity(t, 10, false);
    check(t->lifetimes.size() >= 20 && t->velocities.size() >= 10 * p::particle_velocity_stride && t->scales.size() >= 10 &&
              t->rotation_speeds.size() >= 10,
          "grows the per-particle arrays to hold at least capacity particles");
    check(t->color_birth.size() == 0 && t->color_death.size() == 0, "leaves the color arrays empty without color variance");
  }
  {
    auto t = p::create_particle_emitter_state();
    p::ensure_particle_emitter_state_capacity(t, 8, true);
    check(t->color_birth.size() >= 24 && t->color_death.size() >= 24, "grows the color arrays when color variance is requested");
  }
  {
    auto t = p::create_particle_emitter_state();
    p::ensure_particle_emitter_state_capacity(t, 16, false);
    const auto lifetimes = t->lifetimes;
    const auto velocities = t->velocities;
    p::ensure_particle_emitter_state_capacity(t, 4, false);
    check(t->lifetimes.identity() == lifetimes.identity() && t->velocities.identity() == velocities.identity(),
          "is a no-op when already large enough");
  }
  {
    auto t = p::create_particle_emitter_state();
    p::ensure_particle_emitter_state_capacity(t, 8, false);
    check(t->color_birth.size() == 0, "sized without color");
    p::ensure_particle_emitter_state_capacity(t, 8, true);
    check(t->color_birth.size() >= 24 && t->color_death.size() >= 24, "back-fills color arrays when variance turns on");
  }
  // Addition: growth keeps what was written.
  {
    auto t = p::create_particle_emitter_state();
    p::ensure_particle_emitter_state_capacity(t, 2, false);
    t->lifetimes.set_index(3, 7.5);
    t->velocities.set_index(5, -2.0);
    p::ensure_particle_emitter_state_capacity(t, 50, false);
    check(t->lifetimes.size() >= 100 && t->lifetimes.get(3) == 7.5f && t->velocities.get(5) == -2.0f, "growth preserves existing values");
  }
}

void particle_objects_state() {
  const auto s = p::create_particle_objects_state(10);
  check(s->lifetimes.size() == 20 && s->velocities.size() == 20 && s->spawn_accumulator == 0, "allocates arrays sized to capacity");
  const auto five = p::create_particle_objects_state(5);
  bool dead = true;
  for (int i = 0; i < 5; ++i) dead = dead && five->lifetimes.get(i * 2 + 1) == 0.0f;
  check(dead, "all lifetime slots start as dead (maxAge = 0)");
  check(five->scales.size() == 5 && five->rotation_speeds.size() == 5 && std::isnan(five->prev_x) && std::isnan(five->prev_y),
        "scales and rotation speeds hold one slot each, and the previous position is NaN");
  {
    auto t = p::create_particle_objects_state(10);
    const auto lifetimes = t->lifetimes;
    const auto velocities = t->velocities;
    p::ensure_particle_objects_state_capacity(t, 10);
    check(t->lifetimes.identity() == lifetimes.identity() && t->velocities.identity() == velocities.identity(),
          "does not reallocate when already large enough");
  }
  {
    auto t = p::create_particle_objects_state(5);
    t->lifetimes.set_index(1, 3.0);
    p::ensure_particle_objects_state_capacity(t, 20);
    check(t->lifetimes.size() >= 40 && t->velocities.size() >= 40, "grows arrays when capacity is insufficient");
    check(t->lifetimes.get(1) == 3.0f, "Addition: growth preserves existing values");
  }
}

void spawn_offset() {
  using Out = std::variant<flight::Array<double>, Float32Array>;
  {
    flight::Array<double> out{nan, nan};
    Sequence seq({0.25, 0, 0, 0.75});
    p::write_particle_spawn_offset(Out{out}, 0, config([](Input& i) {
                                     i.emitter_radius = 10;
                                     i.emitter_shape = String("circle");
                                   }),
                                   seq.source());
    check(near(out.element(0), 5) && near(out.element(1), 0), "circle samples its area: sqrt(0.25) * 10 at angle 0");
    p::write_particle_spawn_offset(Out{out}, 0, config([](Input& i) {
                                     i.emitter_height = 10;
                                     i.emitter_shape = String("rect");
                                     i.emitter_width = 20;
                                   }),
                                   seq.source());
    check(out.element(0) == -10 && out.element(1) == 2.5 && seq.draws() == 4, "rect samples x then y, after the circle's two draws");
  }
  {
    flight::Array<double> out{nan, nan};
    p::write_particle_spawn_offset(Out{out}, 0, config([](Input& i) {
                                     i.emitter_shape = String("line");
                                     i.emitter_width = 20;
                                   }),
                                   [] { return 0.0; });
    check(out.element(0) == -10 && out.element(1) == 0, "samples the line from its inclusive left boundary");
  }
  {
    flight::Array<double> out{nan, nan};
    p::write_particle_spawn_offset(Out{out}, 0, config([](Input& i) {
                                     i.emitter_radius = 10;
                                     i.emitter_shape = String("ring");
                                   }),
                                   [] { return 0.25; });
    check(near(out.element(0), 0) && near(out.element(1), 10), "samples the ring circumference");
  }
  for (const char* shape : {"line", "ring"}) {
    flight::Array<double> out{nan, nan};
    int calls = 0;
    p::write_particle_spawn_offset(Out{out}, 0, config([shape](Input& i) {
                                     i.emitter_shape = String(shape);
                                     i.emitter_width = 0;
                                     i.emitter_radius = 0;
                                   }),
                                   [&calls] {
                                     ++calls;
                                     return 0.5;
                                   });
    check(out.element(0) == 0 && out.element(1) == 0 && calls == 0,
          std::string("writes the origin for a degenerate ") + shape + " without drawing randomness");
  }
  // Addition: the offset is honoured and a Float32Array target is written in place.
  {
    Float32Array out(6);
    p::write_particle_spawn_offset(Out{out}, 4, config([](Input& i) {
                                     i.emitter_shape = String("line");
                                     i.emitter_width = 8;
                                   }),
                                   [] { return 1.0; });
    check(out.get(4) == 4.0f && out.get(5) == 0.0f && out.get(0) == 0.0f, "writes at the given offset into a Float32Array");
  }
  // Addition: point, and the 3D shapes, write the origin and draw nothing.
  for (const char* shape : {"point", "sphere", "cone", "box"}) {
    flight::Array<double> out{nan, nan};
    int calls = 0;
    p::write_particle_spawn_offset(Out{out}, 0, config([shape](Input& i) {
                                     i.emitter_shape = String(shape);
                                     i.emitter_radius = 5;
                                     i.emitter_width = 5;
                                     i.emitter_height = 5;
                                   }),
                                   [&calls] {
                                     ++calls;
                                     return 0.5;
                                   });
    check(out.element(0) == 0 && out.element(1) == 0 && calls == 0, std::string("the ") + shape + " shape writes the origin");
  }
}

std::vector<std::string> fields_of(const flight::Array<Ref<flight::types::ParticleConfigIssue>>& issues, const char* severity = nullptr) {
  std::vector<std::string> out;
  for (const auto& issue : issues) {
    if (severity == nullptr || issue->severity == String(severity)) out.push_back(issue->field.to_utf8());
  }
  return out;
}

bool contains(const std::vector<std::string>& values, const std::string& value) {
  for (const auto& v : values)
    if (v == value) return true;
  return false;
}

void validate_and_normalize() {
  {
    const auto in = config([](Input& i) {
      i.spawn_rate = 25;
      i.max_particles = 500;
      i.lifetime_min = 1;
      i.lifetime_max = 2;
    });
    // createParticleEmitterConfig copies a full config's members, so it is the partial input here.
    const auto out = p::normalize_particle_emitter_config(input([&in](Input& i) {
      i.spawn_rate = in->spawn_rate;
      i.max_particles = in->max_particles;
      i.lifetime_min = in->lifetime_min;
      i.lifetime_max = in->lifetime_max;
    }));
    check(out->spawn_rate == 25 && out->max_particles == 500 && out->lifetime_min == 1 && out->lifetime_max == 2,
          "normalize leaves a valid config materially unchanged");
  }
  {
    const auto out = p::normalize_particle_emitter_config(input([](Input& i) {
      i.gravity_y = nan;
      i.spawn_rate = inf;
      i.lifetime_max = -inf;
      i.gravity_z = nan;
      i.velocity_inheritance = -inf;
    }));
    check(out->gravity_y == 0 && out->spawn_rate == 10 && out->lifetime_max == 1, "replaces non-finite fields with their defaults");
    check(out->gravity_z == 0 && out->velocity_inheritance == 0, "Addition: gravityZ and velocityInheritance are repaired too");
  }
  {
    const auto out = p::normalize_particle_emitter_config(input([](Input& i) {
      i.max_particles = -3;
      i.spawn_rate = -10;
      i.burst_count = 4.7;
      i.frame_count = 0;
      i.emitter_radius = -50;
    }));
    check(out->max_particles == 0 && out->spawn_rate == 0 && out->burst_count == 4 && out->frame_count == 1 && out->emitter_radius == 0,
          "clamps negatives and floors integer fields");
  }
  {
    const auto out = p::normalize_particle_emitter_config(input([](Input& i) {
      i.region_id_min = 5;
      i.region_id_max = 2;
    }));
    check(out->region_id_max >= out->region_id_min && out->region_id_min == 5 && out->region_id_max == 5, "keeps regionIdMax >= regionIdMin");
    const auto floored = p::normalize_particle_emitter_config(input([](Input& i) {
      i.region_id_min = -2.5;
      i.region_id_max = 3.9;
    }));
    check(floored->region_id_min == 0 && floored->region_id_max == 3, "Addition: region ids are floored and clamped at zero");
  }
  {
    const auto color = curve({0, 0, 0, 1, 1, 1});
    const auto out = p::normalize_particle_emitter_config(input([&color](Input& i) {
      i.alpha_curve = curve({1, nan, 0});
      i.scale_curve = curve({});
      i.color_curve = color;
    }));
    check(!out->alpha_curve.has_value() && !out->scale_curve.has_value() && out->color_curve.has_value(),
          "drops a curve containing non-finite samples or none, keeps a valid one");
    check(out->color_curve.has_value() && out->color_curve->identity() == color.identity(), "Addition: a kept curve is the same array");
  }
  // Addition: every clamp and floor, and the result is a new object each call.
  {
    const auto out = p::normalize_particle_emitter_config(input([](Input& i) {
      i.burst_interval = -1;
      i.duration = -1;
      i.frame_rate = -1;
      i.lifetime_min = -1;
      i.speed_min = -1;
      i.speed_max = -1;
      i.scale_min = -1;
      i.scale_max = -1;
      i.emitter_cone_angle = -1;
      i.emitter_depth = -1;
      i.emitter_width = -1;
      i.emitter_height = -1;
      i.frame_count = 2.9;
      i.max_particles = 7.9;
      i.gravity_x = -9;
      i.rotation_speed_min = -3;
    }));
    check(out->burst_interval == 0 && out->duration == 0 && out->frame_rate == 0 && out->lifetime_min == 0 && out->speed_min == 0 &&
              out->speed_max == 0 && out->scale_min == 0 && out->scale_max == 0 && out->emitter_cone_angle == 0 &&
              out->emitter_depth == 0 && out->emitter_width == 0 && out->emitter_height == 0,
          "clamps every non-negative member at zero");
    check(out->frame_count == 2 && out->max_particles == 7, "floors frameCount and maxParticles");
    check(out->gravity_x == -9 && out->rotation_speed_min == -3, "leaves members the source does not clamp, negative or not");
    check(p::normalize_particle_emitter_config() != p::normalize_particle_emitter_config(), "returns a new object each call");
  }

  {
    const auto issues = p::validate_particle_emitter_config(config([](Input& i) { i.alpha_curve = curve({0, nan, 1}); }));
    check(fields_of(issues, "error") == std::vector<std::string>{"alphaCurve"}, "validate flags a non-finite curve sample as an error");
  }
  check(p::validate_particle_emitter_config(p::create_particle_emitter_config()).size() == 0, "reports no issues for a default config");
  {
    const auto errors = fields_of(p::validate_particle_emitter_config(config([](Input& i) {
                                    i.gravity_y = nan;
                                    i.spawn_rate = inf;
                                  })),
                                  "error");
    check(contains(errors, "gravityY") && contains(errors, "spawnRate"), "flags non-finite numeric fields as errors");
  }
  {
    const auto issues = p::validate_particle_emitter_config(config([](Input& i) {
      i.spawn_rate = -5;
      i.max_particles = -1;
    }));
    check(contains(fields_of(issues, "warning"), "spawnRate") && contains(fields_of(issues), "maxParticles"),
          "flags negative counts/rates as warnings");
  }
  check(contains(fields_of(p::validate_particle_emitter_config(config([](Input& i) {
          i.lifetime_min = 5;
          i.lifetime_max = 1;
        }))),
                 "lifetimeMin"),
        "flags inverted min/max ranges");
  check(contains(fields_of(p::validate_particle_emitter_config(config([](Input& i) { i.alpha_start = 2; }))), "alphaStart"),
        "flags alpha outside the 0-1 range");

  // Addition: the full issue list, in source order, with the source's messages and severities.
  {
    const auto issues = p::validate_particle_emitter_config(config([](Input& i) {
      i.gravity_z = nan;
      i.alpha_end = -inf;
      i.spawn_rate = -5;
      i.lifetime_min = 3;
      i.lifetime_max = 0;
      i.max_particles = 0;
      i.frame_count = 0.5;
      i.speed_min = 9;
      i.speed_max = 2;
      i.rotation_speed_min = 1;
      i.rotation_speed_max = -1;
      i.alpha_start = 1.5;
      i.color_curve = curve({0, 0, 0, 1});
      i.scale_curve = curve({});
    }));
    std::vector<std::string> got;
    for (const auto& issue : issues) got.push_back(issue->severity.to_utf8() + "|" + issue->field.to_utf8() + "|" + issue->message.to_utf8());
    const std::vector<std::string> want{
        "error|alphaEnd|alphaEnd must be a finite number (got -Infinity)",
        "error|gravityZ|gravityZ must be a finite number (got NaN)",
        "warning|spawnRate|spawnRate must not be negative (got -5)",
        "warning|lifetimeMax|lifetimeMax must be > 0 or particles die instantly",
        "warning|maxParticles|maxParticles must be >= 1 or nothing ever spawns",
        "warning|frameCount|frameCount must be >= 1",
        "warning|lifetimeMin|lifetimeMin (3) is greater than lifetimeMax (0)",
        "warning|speedMin|speedMin (9) is greater than speedMax (2)",
        "warning|rotationSpeedMin|rotationSpeedMin (1) is greater than rotationSpeedMax (-1)",
        "warning|alphaStart|alphaStart (1.5) is outside the expected 0–1 range",
        "warning|colorCurve|colorCurve length (4) is not a multiple of 3",
        "warning|scaleCurve|scaleCurve is empty and will be ignored",
    };
    check(got == want, "validate reports every issue in source order with the source's text");
    if (got != want)
      for (const auto& g : got) std::cerr << "  got: " << g << '\n';
  }
  // Addition: a non-finite member is reported once, as an error, and not again as an inverted or unit range.
  {
    const auto issues = p::validate_particle_emitter_config(config([](Input& i) {
      i.scale_min = nan;
      i.alpha_end = 4;
      i.alpha_start = -inf;
    }));
    check(fields_of(issues) == std::vector<std::string>{"alphaStart", "scaleMin", "alphaEnd"},
          "non-finite members skip the range checks; a finite out-of-range alpha does not");
  }
}

Ref<flight::types::ParticleObjectsUpdateOptions> emitter_at(double x, double y);

// upstream's makeObject: a display object with the members the pool path writes.
Ref<flight::types::ParticleObject> make_object() {
  return flight::make_ref<flight::types::ParticleObject>(flight::types::ParticleObject{
      .pivot_x = 0, .pivot_y = 0, .rotation = 0, .scale_x = 1, .scale_y = 1, .skew_x = 0, .skew_y = 0, .x = 0, .y = 0, .alpha = 1, .visible = false});
}

flight::Array<Ref<flight::types::ParticleObject>> make_objects(int count) {
  flight::Array<Ref<flight::types::ParticleObject>> objects;
  for (int i = 0; i < count; ++i) objects.push(make_object());
  return objects;
}

int visible_count(const flight::Array<Ref<flight::types::ParticleObject>>& objects) {
  int count = 0;
  for (const auto& o : objects) count += o->visible ? 1 : 0;
  return count;
}

void step_objects() {
  auto objects = make_objects(10);
  auto state = p::create_particle_objects_state(10);
  p::step_particle_objects(objects, state, config([](Input& i) {
                             i.spawn_rate = 5;
                             i.lifetime_min = 10;
                             i.lifetime_max = 10;
                           }),
                           1);
  check(visible_count(objects) == 5, "stepParticleObjects spawns into dead slots with no forces or colliders");
}

// Addition: stepParticleObjects runs forces, then the update, then collisions, and passes its update options through.
// Declared after the force and collider helpers it uses.
void step_objects_in_order();


using Keys = flight::Array<Ref<flight::types::CurveKeyframe>>;
using ColorKeys = flight::Array<Ref<flight::types::ColorKeyframe>>;

Ref<flight::types::CurveKeyframe> key(double time, double value) {
  return flight::make_ref<flight::types::CurveKeyframe>(flight::types::CurveKeyframe{.time = time, .value = value});
}
Ref<flight::types::ColorKeyframe> ckey(double time, double r, double g, double b) {
  return flight::make_ref<flight::types::ColorKeyframe>(flight::types::ColorKeyframe{.time = time, .r = r, .g = g, .b = b});
}
std::vector<double> values_of(const flight::Array<double>& a) {
  std::vector<double> out;
  for (const auto v : a) out.push_back(v);
  return out;
}
std::vector<std::vector<double>> keys_of(const Keys& keys) {
  std::vector<std::vector<double>> out;
  for (const auto& k : keys) out.push_back({k->time, k->value});
  return out;
}
std::vector<std::vector<double>> ckeys_of(const ColorKeys& keys) {
  std::vector<std::vector<double>> out;
  for (const auto& k : keys) out.push_back({k->time, k->r, k->g, k->b});
  return out;
}

void curves() {
  using Out = std::variant<flight::Array<double>, Float32Array>;
  {
    const auto lut = p::build_particle_color_curve([](double t) { return flight::Array<double>{t, 0, 1 - t}; }, 3);
    check(values_of(lut) == std::vector<double>{0, 0, 1, 0.5, 0, 0.5, 1, 0, 0}, "bakes an RGB function into an interleaved LUT");
    check(p::build_particle_color_curve([](double) { return flight::Array<double>{1, 1, 1}; }).size() == 99,
          "Addition: the color LUT defaults to 33 samples");
  }
  {
    const auto lut = p::build_particle_curve([](double t) { return t * t; }, 5);
    check(values_of(lut) == std::vector<double>{0, 0.0625, 0.25, 0.5625, 1}, "bakes a scalar function into a LUT sampled at the endpoints");
    check(near(p::sample_particle_curve(lut, 0.5), 0.25, 0.05), "a baked curve samples back its function");
    check(p::build_particle_curve([](double t) { return t; }).size() == 33, "Addition: the scalar LUT defaults to 33 samples");
  }
  {
    flight::Array<double> out{0, 0, 0};
    p::lerp_hsv_direct(Out{out}, 0, 1, 0, 0, 0, 1, 0, 0.5);
    check(out.element(0) > 0.7 && out.element(1) > 0.7 && near(out.element(2), 0, 0.01), "interpolates red to green through yellow");
    flight::Array<double> arc{0, 0, 0};
    p::lerp_hsv_direct(Out{arc}, 0, 0, 0, 1, 1, 0, 0, 0.5);
    check(arc.element(0) > 0.5 && near(arc.element(1), 0, 0.01), "takes the short arc from blue to red, through magenta");
    flight::Array<double> at{9, 9, 9, 0, 0, 0};
    p::lerp_hsv_direct(Out{at}, 3, 1, 0, 0, 1, 0, 0, 0);
    check(at.element(0) == 9 && near(at.element(3), 1) && near(at.element(4), 0) && near(at.element(5), 0), "lerpHsvDirect writes at the given offset");
  }
  {
    flight::Array<double> out{0, 0, 0};
    p::lerp_hsv_in_place(Out{out}, 0, Float32Array({1.0f, 0.0f, 0.0f}), Float32Array({0.0f, 1.0f, 0.0f}), 0.5);
    check(out.element(0) > 0.5 && out.element(1) > 0.5 && near(out.element(2), 0, 0.01), "lerpHsvInPlace: red to green is yellow at 0.5");
    const Float32Array birth({0.2f, 0.4f, 0.6f});
    const Float32Array death({0.6f, 0.4f, 0.2f});
    flight::Array<double> out0{0, 0, 0};
    p::lerp_hsv_in_place(Out{out0}, 0, birth, death, 0);
    flight::Array<double> out1{0, 0, 0};
    p::lerp_hsv_in_place(Out{out1}, 0, birth, death, 1);
    check(near(out0.element(0), 0.2, 1e-3) && near(out0.element(1), 0.4, 1e-3) && near(out0.element(2), 0.6, 1e-3) &&
              near(out1.element(0), 0.6, 1e-3) && near(out1.element(1), 0.4, 1e-3) && near(out1.element(2), 0.2, 1e-3),
          "returns the birth color at t=0 and the death color at t=1");
    flight::Array<double> at{9, 9, 9, 0, 0, 0};
    p::lerp_hsv_in_place(Out{at}, 3, Float32Array({9.0f, 9.0f, 9.0f, 1.0f, 0.0f, 0.0f}), Float32Array({9.0f, 9.0f, 9.0f, 0.0f, 1.0f, 0.0f}), 0);
    check(at.element(0) == 9 && near(at.element(3), 1) && near(at.element(4), 0) && near(at.element(5), 0), "lerpHsvInPlace reads and writes at the offset");
    // Addition: a Float32Array output is written in place.
    Float32Array fout(3);
    p::lerp_hsv_in_place(Out{fout}, 0, birth, death, 1);
    check(near(fout.element(0), 0.6, 1e-3) && near(fout.element(2), 0.2, 1e-3), "writes into a Float32Array output");
  }
  {
    const auto lut = p::particle_color_curve_from_keyframes(ColorKeys{ckey(0, 1, 0, 0), ckey(0.5, 0, 1, 0), ckey(1, 0, 0, 1)});
    flight::Array<double> out{0, 0, 0};
    p::sample_particle_color_curve(out, 0, lut, 0.5);
    check(out.element(1) > 0.8, "bakes an RGB timeline through its middle stop");
    check(lut.size() == 99, "Addition: a color timeline bakes 33 samples by default");
    flight::Array<double> flat{9, 9, 9};
    p::sample_particle_color_curve(flat, 0, p::particle_color_curve_from_keyframes(ColorKeys{}), 0.5);
    check(values_of(flat) == std::vector<double>{0, 0, 0}, "an empty color keyframe list yields a flat black curve");
  }
  {
    check(ckeys_of(p::particle_color_curve_to_keyframes(curve({1, 0, 0, 0, 0, 1}))) == std::vector<std::vector<double>>{{0, 1, 0, 0}, {1, 0, 0, 1}},
          "emits one RGB keyframe per LUT sample at uniform times");
    const auto keys = p::particle_color_curve_to_keyframes(p::particle_color_curve_from_keyframes(ColorKeys{ckey(0, 1, 0, 0), ckey(1, 0, 0, 1)}));
    const auto all = ckeys_of(keys);
    check(all.size() == 33 && all.front() == std::vector<double>{0, 1, 0, 0} && all.back() == std::vector<double>{1, 0, 0, 1},
          "round-trips a baked color curve back to keyframes");
    check(p::particle_color_curve_to_keyframes(curve({})).size() == 0 &&
              ckeys_of(p::particle_color_curve_to_keyframes(curve({0.2, 0.4, 0.6}))) == std::vector<std::vector<double>>{{0, 0.2, 0.4, 0.6}},
          "handles empty and single-entry color LUTs");
  }
  {
    const auto lut = p::particle_curve_from_keyframes(Keys{key(0, 0), key(0.5, 1), key(1, 0)});
    check(near(p::sample_particle_curve(lut, 0), 0, 0.01) && p::sample_particle_curve(lut, 0.5) > 0.9 && near(p::sample_particle_curve(lut, 1), 0, 0.01),
          "interpolates a 3-stop scalar timeline through its middle stop");
    const auto sorted = p::particle_curve_from_keyframes(Keys{key(1, 10), key(0, 0)});
    check(near(p::sample_particle_curve(sorted, 0), 0, 0.1) && near(p::sample_particle_curve(sorted, 1), 10, 0.1), "sorts unsorted keyframes by time");
    const auto clamped = p::particle_curve_from_keyframes(Keys{key(0.25, 2), key(0.75, 6)});
    check(near(p::sample_particle_curve(clamped, 0), 2, 0.1) && near(p::sample_particle_curve(clamped, 1), 6, 0.1) &&
              near(p::sample_particle_curve(clamped, 0.5), 4, 0.1),
          "clamps non-normalized key times at the ends");
    const auto single = p::particle_curve_from_keyframes(Keys{key(0.5, 3)});
    check(near(p::sample_particle_curve(single, 0), 3) && near(p::sample_particle_curve(single, 1), 3), "a single keyframe yields a constant curve");
    const auto empty = p::particle_curve_from_keyframes(Keys{});
    check(p::sample_particle_curve(empty, 0) == 0 && p::sample_particle_curve(empty, 1) == 0, "an empty keyframe list yields a flat zero curve");
    // Addition: sorting does not reorder the caller's array, and the sample count is honoured exactly.
    Keys unsorted{key(1, 10), key(0, 0)};
    const auto five = p::particle_curve_from_keyframes(unsorted, 5);
    check(unsorted.element(0)->time == 1 && values_of(five) == std::vector<double>{0, 2.5, 5, 7.5, 10},
          "bakes the requested samples and leaves the caller's keyframes in their order");
  }
  {
    check(keys_of(p::particle_curve_to_keyframes(curve({0, 0.5, 1}))) == std::vector<std::vector<double>>{{0, 0}, {0.5, 0.5}, {1, 1}},
          "emits one keyframe per LUT sample at uniform times");
    const auto lut = p::particle_curve_from_keyframes(Keys{key(0, 0), key(1, 10)});
    const auto keys = p::particle_curve_to_keyframes(lut);
    const auto all = keys_of(keys);
    check(all.front() == std::vector<double>{0, 0} && all.back() == std::vector<double>{1, 10}, "round-trips a baked scalar curve's ends");
    check(near(p::sample_particle_curve(p::particle_curve_from_keyframes(keys, static_cast<double>(lut.size())), 0.5), p::sample_particle_curve(lut, 0.5)),
          "reconstructing from the keyframes reproduces the samples");
    check(p::particle_curve_to_keyframes(curve({})).size() == 0 &&
              keys_of(p::particle_curve_to_keyframes(curve({7}))) == std::vector<std::vector<double>>{{0, 7}},
          "handles empty and single-entry LUTs");
  }
  {
    flight::Array<double> out{0, 0, 0};
    p::sample_particle_color_curve(out, 0, curve({1, 0, 0, 0, 0, 1}), 0.5);
    check(near(out.element(0), 0.5) && near(out.element(1), 0) && near(out.element(2), 0.5), "interpolates interleaved RGB channels");
    flight::Array<double> at{9, 9, 9, 9, 9};
    p::sample_particle_color_curve(at, 2, curve({0.2, 0.4, 0.6}), 0);
    check(at.element(1) == 9 && near(at.element(2), 0.2) && near(at.element(3), 0.4) && near(at.element(4), 0.6), "sampleParticleColorCurve writes at the offset");
  }
  {
    check(p::sample_particle_curve(curve({0, 0.5, 1}), 0) == 0 && p::sample_particle_curve(curve({0, 0.5, 1}), 1) == 1, "returns endpoints at t=0 and t=1");
    check(near(p::sample_particle_curve(curve({0, 1}), 0.25), 0.25) && near(p::sample_particle_curve(curve({0, 1}), 0.5), 0.5), "linearly interpolates between samples");
    check(p::sample_particle_curve(curve({2, 4}), -1) == 2 && p::sample_particle_curve(curve({2, 4}), 5) == 4, "clamps out-of-range t");
    check(p::sample_particle_curve(curve({}), 0.5) == 0 && p::sample_particle_curve(curve({7}), 0.5) == 7, "handles empty and single-entry curves");
    check(near(p::sample_particle_curve(curve({0, 10, 40}), 0.75), 25), "Addition: interpolates within the right segment");
  }
}

// upstream's createEmitterFixture: a node-free SoA emitter; the force and collision passes read only `data`.
Ref<flight::types::ParticleEmitter2D> emitter_fixture(int capacity) {
  auto data = flight::make_ref<flight::types::ParticleEmitterData>(flight::types::ParticleEmitterData{
      .alphas = Float32Array(capacity),
      .colors = Float32Array(capacity * 3),
      .ids = flight::Uint16Array(capacity),
      .particle_count = 0,
      .positions_z = Float32Array(capacity),
      .transforms = Float32Array(capacity * 4),
      .velocities = Float32Array(capacity * 2),
      .world_space = false});
  return flight::make_ref<flight::types::ParticleEmitter2D>(flight::types::ParticleEmitter2D{.data = data, .x = 0, .y = 0});
}

struct Placed {
  Ref<flight::types::ParticleEmitter2D> emitter;
  Ref<flight::types::ParticleEmitterState> state;
  double vx() const { return state->velocities.element(0); }
  double vy() const { return state->velocities.element(1); }
  double vz() const { return state->velocities.element(2); }
  double px() const { return emitter->data->transforms.element(0); }
  double py() const { return emitter->data->transforms.element(1); }
  double pz() const { return emitter->data->positions_z.element(0); }
};

Placed place(double px = 0, double py = 0, double vx = 0, double vy = 0) {
  Placed out{emitter_fixture(1), p::create_particle_emitter_state()};
  p::ensure_particle_emitter_state_capacity(out.state, 1, false);
  out.emitter->data->particle_count = 1;
  out.emitter->data->transforms.set_index(0, px);
  out.emitter->data->transforms.set_index(1, py);
  out.state->velocities.set_index(0, vx);
  out.state->velocities.set_index(1, vy);
  return out;
}

using Force = flight::types::ParticleForce;
using Forces = flight::Array<Force>;
using flight::types::AttractorForce;
using flight::types::DragForce;
using flight::types::TurbulenceForce;
using flight::types::VortexForce;
using flight::types::WindForce;

Force wind(double x, double y, std::optional<double> z = std::nullopt) {
  return Force{flight::make_ref<WindForce>(WindForce{.kind = String("WindForce"), .x = x, .y = y, .z = z})};
}
Force attractor(double x, double y, double strength, std::optional<double> radius = std::nullopt, std::optional<double> z = std::nullopt,
                std::optional<String> falloff = std::nullopt) {
  return Force{flight::make_ref<AttractorForce>(
      AttractorForce{.kind = String("AttractorForce"), .x = x, .y = y, .z = z, .strength = strength, .radius = radius, .falloff = falloff})};
}
Force vortex(double x, double y, double strength, std::optional<double> z = std::nullopt, std::optional<double> ax = std::nullopt,
             std::optional<double> ay = std::nullopt, std::optional<double> az = std::nullopt) {
  return Force{flight::make_ref<VortexForce>(
      VortexForce{.kind = String("VortexForce"), .x = x, .y = y, .z = z, .axis_x = ax, .axis_y = ay, .axis_z = az, .strength = strength})};
}
Force drag(double strength) { return Force{flight::make_ref<DragForce>(DragForce{.kind = String("DragForce"), .strength = strength})}; }
Force turbulence(double strength, double scale) {
  return Force{flight::make_ref<TurbulenceForce>(TurbulenceForce{.kind = String("TurbulenceForce"), .strength = strength, .scale = scale})};
}

// An independent port of the source's hash2/valueNoise in 32-bit unsigned arithmetic: Math.imul, `^`, `| 0` and `>>>`
// all act on the same 32 bits, so the emitted operators must agree with this bit for bit.
std::uint32_t u32(double v) { return static_cast<std::uint32_t>(static_cast<std::int64_t>(v)); }
double ref_hash2(double x, double y, double seed) {
  std::uint32_t h = (u32(x) * 0x27d4eb2du) ^ (u32(y) * 0x165667b1u) ^ (u32(seed + 1) * 0x9e3779b1u);
  h = (h ^ (h >> 15)) * 0x85ebca6bu;
  h = (h ^ (h >> 13)) * 0xc2b2ae35u;
  return static_cast<double>(h ^ (h >> 16)) / 4294967296.0;
}
double ref_noise(double x, double y, double seed) {
  const double x0 = std::floor(x), y0 = std::floor(y), fx = x - x0, fy = y - y0;
  const double ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
  const double n00 = ref_hash2(x0, y0, seed), n10 = ref_hash2(x0 + 1, y0, seed), n01 = ref_hash2(x0, y0 + 1, seed),
               n11 = ref_hash2(x0 + 1, y0 + 1, seed);
  const double nx0 = n00 + (n10 - n00) * ux, nx1 = n01 + (n11 - n01) * ux;
  return nx0 + (nx1 - nx0) * uy;
}

void forces() {
  {
    auto a = place();
    p::apply_particle_forces(a.emitter, a.state, Forces{attractor(100, 0, 50)}, 1);
    check(near(a.vx(), 50) && near(a.vy(), 0), "attractor pulls velocity toward the point");
  }
  {
    auto a = place();
    p::apply_particle_forces(a.emitter, a.state, Forces{attractor(100, 0, -50)}, 1);
    check(a.vx() < 0, "negative-strength attractor repels");
  }
  {
    auto a = place(0, 0, 42, 7);
    p::apply_particle_forces(a.emitter, a.state, Forces{attractor(1000, 0, 50, 50)}, 1);
    check(a.vx() == 42 && a.vy() == 7, "radius acts as a hard cutoff");
  }
  {
    auto a = place(10, 0);
    p::apply_particle_forces(a.emitter, a.state, Forces{vortex(0, 0, 20)}, 1);
    check(near(a.vx(), 0) && near(a.vy(), 20), "vortex applies tangential velocity");
  }
  {
    auto a = place(0, 0, 100, 0);
    p::apply_particle_forces(a.emitter, a.state, Forces{drag(0.5)}, 1);
    check(near(a.vx(), 50), "drag reduces existing velocity");
  }
  {
    auto a = place();
    p::apply_particle_forces(a.emitter, a.state, Forces{wind(5, -3)}, 2);
    check(near(a.vx(), 10) && near(a.vy(), -6), "wind adds constant acceleration times deltaTime");
  }
  {
    auto a = place(12.5, -7.5);
    auto b = place(12.5, -7.5);
    p::apply_particle_forces(a.emitter, a.state, Forces{turbulence(100, 0.1)}, 1);
    p::apply_particle_forces(b.emitter, b.state, Forces{turbulence(100, 0.1)}, 1);
    check(std::isfinite(a.vx()) && a.vx() == b.vx() && a.vy() == b.vy(), "turbulence is finite and deterministic for a given position");
    // Addition: the exact value of the source's noise, for x, y and z.
    const double ex = (ref_noise(1.25, -0.75, 0) * 2 - 1) * 100;
    const double ey = (ref_noise(1.25, -0.75, 1) * 2 - 1) * 100;
    const double ez = (ref_noise(1.25, 0, 2) * 2 - 1) * 100;
    check(a.vx() == static_cast<float>(ex) && a.vy() == static_cast<float>(ey) && a.vz() == static_cast<float>(ez),
          "turbulence matches the source's integer hash bit for bit");
  }
  {
    auto a = place();
    p::apply_particle_forces(a.emitter, a.state, Forces{wind(10, 0), wind(5, 0)}, 1);
    check(near(a.vx(), 15), "composes multiple forces additively");
  }
  {
    auto a = place(0, 0, 42, 0);
    p::apply_particle_forces(a.emitter, a.state, Forces{}, 1);
    p::apply_particle_forces(a.emitter, a.state, Forces{wind(5, 0)}, 0);
    p::apply_particle_forces(a.emitter, a.state, Forces{wind(5, 0)}, -1);
    check(a.vx() == 42, "is a no-op for an empty force list, zero or negative deltaTime");
  }
  {
    auto a = place();
    p::apply_particle_forces(a.emitter, a.state, Forces{wind(0, 0, 10)}, 1);
    check(near(a.vx(), 0) && near(a.vy(), 0) && near(a.vz(), 10), "wind z adds acceleration on the Z axis");
  }
  {
    auto a = place();
    p::apply_particle_forces(a.emitter, a.state, Forces{attractor(0, 0, 50, std::nullopt, 100)}, 1);
    check(near(a.vx(), 0) && near(a.vy(), 0) && near(a.vz(), 50), "attractor with z pulls toward the 3D point");
  }
  {
    auto a = place(0, 10);
    p::apply_particle_forces(a.emitter, a.state, Forces{vortex(0, 0, 20, 0, 1, 0, 0)}, 1);
    check(near(a.vx(), 0) && near(a.vy(), 0) && near(a.vz(), 20), "vortex with a custom axis rotates around that axis");
  }
  {
    auto a = place(5, 5);
    a.emitter->data->positions_z.set_index(0, 5);
    p::apply_particle_forces(a.emitter, a.state, Forces{turbulence(100, 0.1)}, 1);
    check(std::isfinite(a.vz()) && a.vz() == static_cast<float>((ref_noise(0.5, 0.5, 2) * 2 - 1) * 100),
          "turbulence produces a finite z component from (x, z)");
  }
  {
    auto a = place();
    a.state->velocities.set_index(2, 100);
    p::apply_particle_forces(a.emitter, a.state, Forces{drag(0.5)}, 1);
    check(near(a.vz(), 50), "drag damps z velocity");
  }

  // Addition: an early-exiting attractor or vortex ends only its own case; later forces still apply
  // (particles-forces-case-early-exit-guards).
  {
    auto a = place();
    p::apply_particle_forces(a.emitter, a.state, Forces{attractor(1000, 0, 50, 50), wind(3, 0)}, 1);
    check(near(a.vx(), 3), "a force after an out-of-radius attractor still applies");
    auto b = place();
    p::apply_particle_forces(b.emitter, b.state, Forces{attractor(0, 0, 50), wind(0, 4)}, 1);
    check(near(b.vy(), 4), "a force after a zero-distance attractor still applies");
    auto c = place();
    p::apply_particle_forces(c.emitter, c.state, Forces{vortex(0, 0, 20), wind(0, 0, 6)}, 1);
    check(near(c.vz(), 6), "a force after a zero-distance vortex still applies");
    auto d = place(10, 0);
    p::apply_particle_forces(d.emitter, d.state, Forces{vortex(0, 0, 20, std::nullopt, std::nullopt, std::nullopt, std::nullopt), vortex(100, 0, 5)}, 1);
    check(near(d.vy(), 20 - 5), "two vortices accumulate, the second opposite the first");
  }
  // Addition: falloff modes.
  {
    auto lin = place();
    p::apply_particle_forces(lin.emitter, lin.state, Forces{attractor(10, 0, 40, 40, std::nullopt, String("linear"))}, 1);
    check(near(lin.vx(), 40 * (1 - 10.0 / 40)), "linear falloff scales by 1 - dist / radius");
    auto edge = place();
    p::apply_particle_forces(edge.emitter, edge.state, Forces{attractor(40, 0, 40, 40, std::nullopt, String("linear")), wind(1, 0)}, 1);
    check(near(edge.vx(), 1), "linear falloff at exactly the radius contributes nothing and later forces apply");
    auto noRadius = place();
    p::apply_particle_forces(noRadius.emitter, noRadius.state, Forces{attractor(10, 0, 40, std::nullopt, std::nullopt, String("linear"))}, 1);
    check(near(noRadius.vx(), 40), "linear falloff without a radius is full strength");
    auto inv = place();
    p::apply_particle_forces(inv.emitter, inv.state, Forces{attractor(4, 0, 32, std::nullopt, std::nullopt, String("inverseSquare"))}, 1);
    check(near(inv.vx(), 2), "inverse-square falloff divides by dist squared");
    auto close = place();
    p::apply_particle_forces(close.emitter, close.state, Forces{attractor(0.5, 0, 32, std::nullopt, std::nullopt, String("inverseSquare"))}, 1);
    check(near(close.vx(), 32), "inverse-square falloff clamps distance below 1");
  }
  // Addition: every live particle is visited with its own position; a short positionsZ reads z as 0.
  {
    auto e = emitter_fixture(2);
    e->data->positions_z = Float32Array(1);
    e->data->particle_count = 2;
    e->data->transforms.set_index(4, 10);
    auto s = p::create_particle_emitter_state();
    p::ensure_particle_emitter_state_capacity(s, 2, false);
    p::apply_particle_forces(e, s, Forces{attractor(0, 0, 9, std::nullopt, 30)}, 1);
    check(near(s->velocities.element(2), 9) && near(s->velocities.element(5), 9 * 30 / std::sqrt(100.0 + 900.0)) &&
              near(s->velocities.element(3), -9 * 10 / std::sqrt(1000.0)),
          "each particle accumulates from its own position, z = 0 past positionsZ");
  }

  {
    auto objects = make_objects(2);
    auto state = p::create_particle_objects_state(2);
    p::update_particle_objects(objects, state, config([](Input& i) {
                                 i.spawn_rate = 1;
                                 i.lifetime_min = 100;
                                 i.lifetime_max = 100;
                                 i.speed_min = 0;
                                 i.speed_max = 0;
                               }),
                               1);
    int live = -1;
    for (int i = 0; i < 2; ++i)
      if (objects.element(i)->visible) live = i;
    const int dead = live == 0 ? 1 : 0;
    check(live >= 0, "one object spawned");
    if (live >= 0) {
      objects.element(live)->x = 0;
      objects.element(live)->y = 0;
      p::apply_particle_object_forces(objects, state, Forces{wind(7, 0)}, 1);
      check(near(state->velocities.element(live * 2), 7) && state->velocities.element(dead * 2) == 0, "applies forces only to live objects");
    }
  }
}

using Collider = flight::types::ParticleCollider;
using Colliders = flight::Array<Collider>;

Collider plane(double nx, double ny, double distance, std::optional<double> restitution = std::nullopt, std::optional<double> friction = std::nullopt,
               std::optional<double> nz = std::nullopt) {
  return Collider{flight::make_ref<flight::types::PlaneCollider>(flight::types::PlaneCollider{
      .restitution = restitution, .friction = friction, .kind = String("PlaneCollider"), .nx = nx, .ny = ny, .nz = nz, .distance = distance})};
}
Collider circle(double x, double y, double radius, const char* mode, std::optional<double> restitution = std::nullopt) {
  return Collider{flight::make_ref<flight::types::CircleCollider>(flight::types::CircleCollider{
      .restitution = restitution, .kind = String("CircleCollider"), .x = x, .y = y, .radius = radius, .mode = String(mode)})};
}
Collider sphere(double x, double y, double z, double radius, const char* mode, std::optional<double> restitution = std::nullopt) {
  return Collider{flight::make_ref<flight::types::SphereCollider>(flight::types::SphereCollider{
      .restitution = restitution, .kind = String("SphereCollider"), .x = x, .y = y, .z = z, .radius = radius, .mode = String(mode)})};
}
Collider rect(double x, double y, double w, double h, const char* mode, std::optional<double> restitution = std::nullopt,
              std::optional<double> friction = std::nullopt) {
  return Collider{flight::make_ref<flight::types::RectangleCollider>(flight::types::RectangleCollider{
      .restitution = restitution, .friction = friction, .kind = String("RectangleCollider"), .x = x, .y = y, .width = w, .height = h, .mode = String(mode)})};
}

void step_objects_in_order() {
  auto objects = make_objects(1);
  auto state = p::create_particle_objects_state(1);
  const auto c = config([](Input& i) {
    i.spawn_rate = 1;
    i.lifetime_min = 10;
    i.lifetime_max = 10;
    i.speed_min = 0;
    i.speed_max = 0;
  });
  p::step_particle_objects(objects, state, c, 1);
  const bool spawned = objects.element(0)->visible && objects.element(0)->y == 0;
  const auto idle = config([](Input& i) { i.spawn_rate = 0; });
  p::step_particle_objects(objects, state, idle, 1, Forces{wind(0, 1000)}, flight::Array<flight::types::ParticleCollider>{flight::types::ParticleCollider{
      flight::make_ref<flight::types::PlaneCollider>(flight::types::PlaneCollider{.kind = String("PlaneCollider"), .nx = 0, .ny = -1, .distance = -500})}},
                           emitter_at(3, 4));
  // Wind raises velocity to 1000, the update moves y to 1000, and the floor returns it to 500 and stops it.
  check(spawned && objects.element(0)->y == 500 && state->velocities.element(1) == 0, "step applies forces, then integrates, then collides");
  check(state->prev_x == 3 && state->prev_y == 4, "step passes its update options to the update");
}

void collisions() {
  {
    auto a = place(5, 0, -10, 0);
    p::apply_particle_collisions(a.emitter, a.state, Colliders{circle(0, 0, 10, "exclude", 1)});
    check(near(std::hypot(a.px(), a.py()), 10) && near(a.vx(), 10), "circle exclude pushes to the surface and bounces outward");
  }
  {
    auto a = place(20, 0, 10, 0);
    p::apply_particle_collisions(a.emitter, a.state, Colliders{circle(0, 0, 10, "contain")});
    check(near(std::hypot(a.px(), a.py()), 10) && near(a.vx(), 0), "circle contain keeps a particle inside and stops its outward velocity");
  }
  {
    auto a = place(0, 520, 0, 100);
    p::apply_particle_collisions(a.emitter, a.state, Colliders{});
    check(a.py() == 520 && a.vy() == 100, "is a no-op with no colliders");
  }
  {
    auto a = place(0, 520, 0, 100);
    p::apply_particle_collisions(a.emitter, a.state, Colliders{plane(0, -1, -500)});
    check(near(a.py(), 500) && near(a.vy(), 0), "plane pushes back to the surface and stops (restitution 0)");
  }
  {
    auto a = place(0, 520, 0, 100);
    p::apply_particle_collisions(a.emitter, a.state, Colliders{plane(0, -1, -500, 0.5)});
    check(near(a.vy(), -50), "plane bounces with restitution");
  }
  {
    auto a = place(0, 520, 80, 100);
    p::apply_particle_collisions(a.emitter, a.state, Colliders{plane(0, -1, -500, std::nullopt, 0.25)});
    check(near(a.vx(), 60), "plane friction scales the tangential component");
  }
  {
    auto a = place(0, 100, 0, 100);
    p::apply_particle_collisions(a.emitter, a.state, Colliders{plane(0, -1, -500)});
    check(a.py() == 100 && a.vy() == 100, "leaves particles above the floor untouched");
  }
  {
    auto a = place();
    a.emitter->data->positions_z.set_index(0, 120);
    a.state->velocities.set_index(2, 50);
    p::apply_particle_collisions(a.emitter, a.state, Colliders{plane(0, 0, -100, std::nullopt, std::nullopt, -1)});
    check(near(a.pz(), 100) && near(a.vz(), 0), "a plane with nz pushes back along the 3D normal");
  }
  {
    auto a = place(3, 4, -10, 0);
    p::apply_particle_collisions(a.emitter, a.state, Colliders{sphere(0, 0, 0, 10, "exclude", 1)});
    check(near(std::sqrt(a.px() * a.px() + a.py() * a.py() + a.pz() * a.pz()), 10) && near(a.px(), 6) && near(a.py(), 8),
          "sphere exclude pushes out along the radial direction");
  }
  {
    auto a = place(20, 0, 10, 0);
    p::apply_particle_collisions(a.emitter, a.state, Colliders{sphere(0, 0, 0, 10, "contain")});
    check(near(std::sqrt(a.px() * a.px() + a.py() * a.py() + a.pz() * a.pz()), 10), "sphere contain keeps a particle inside");
  }
  {
    auto a = place();
    a.emitter->data->positions_z.set_index(0, 25);
    a.state->velocities.set_index(2, 10);
    p::apply_particle_collisions(a.emitter, a.state, Colliders{sphere(0, 0, 0, 10, "contain")});
    check(near(a.pz(), 10) && near(a.vz(), 0), "sphere contain with a 3D offset clamps to the surface");
  }
  {
    auto a = place(80, 0, 30, 0);
    p::apply_particle_collisions(a.emitter, a.state, Colliders{rect(0, 0, 100, 100, "contain", 1)});
    check(near(a.px(), 50) && near(a.vx(), -30), "rect contain clamps and reflects");
  }
  {
    auto a = place(40, 0, 0, 0);
    p::apply_particle_collisions(a.emitter, a.state, Colliders{rect(0, 0, 100, 100, "exclude")});
    check(near(a.px(), 50) && a.py() == 0, "rect exclude pushes out along the shallowest axis");
  }
  // Addition: rect exclude on each side, contain on y, and a separating velocity is kept.
  {
    auto l = place(-45, 0, 5, 0);
    p::apply_particle_collisions(l.emitter, l.state, Colliders{rect(0, 0, 100, 100, "exclude")});
    auto t = place(0, -48, 0, 5);
    p::apply_particle_collisions(t.emitter, t.state, Colliders{rect(0, 0, 100, 100, "exclude")});
    auto b = place(0, 44, 0, -5);
    p::apply_particle_collisions(b.emitter, b.state, Colliders{rect(0, 0, 100, 100, "exclude")});
    check(near(l.px(), -50) && near(l.vx(), 0) && near(t.py(), -50) && near(t.vy(), 0) && near(b.py(), 50) && near(b.vy(), 0),
          "rect exclude leaves by the nearest of left, top and bottom and stops inward velocity");
    auto c = place(0, -70, 3, -20);
    p::apply_particle_collisions(c.emitter, c.state, Colliders{rect(0, 0, 100, 100, "contain", 0.5, 0.5)});
    check(near(c.py(), -50) && near(c.vy(), 10) && near(c.vx(), 1.5), "rect contain on y with restitution and friction");
    auto e = place(0, 50, 0, 0);
    p::apply_particle_collisions(e.emitter, e.state, Colliders{rect(0, 0, 100, 100, "exclude")});
    check(e.py() == 50, "a particle on a rect's edge is outside it");
    auto s = place(0, 520, 0, -30);
    p::apply_particle_collisions(s.emitter, s.state, Colliders{plane(0, -1, -500, 1)});
    check(near(s.py(), 500) && near(s.vy(), -30), "a separating particle is moved to the surface but keeps its velocity");
  }
  // Addition: every collider is resolved in order even after a hit, and a particle at a center is not pushed.
  {
    auto a = place(0, 520, 80, 100);
    p::apply_particle_collisions(a.emitter, a.state, Colliders{plane(0, -1, -500), rect(0, 0, 100, 1000, "contain")});
    check(near(a.py(), 500) && near(a.px(), 0) && near(a.vx(), 80), "a second collider is evaluated after the first hits");
    auto b = place(120, 520, 80, 100);
    p::apply_particle_collisions(b.emitter, b.state, Colliders{plane(0, -1, -500), rect(0, 0, 100, 1000, "contain")});
    check(near(b.py(), 500) && near(b.px(), 50) && near(b.vx(), 0) && near(b.vy(), 0), "both colliders correct the same particle in order");
    auto later = place(120, 520, 0, 100);
    p::apply_particle_collisions(later.emitter, later.state, Colliders{rect(0, 0, 100, 2000, "contain"), plane(0, -1, -500)});
    check(near(later.px(), 50) && near(later.py(), 500) && near(later.vy(), 0), "a plane after a hit collider is still resolved");
    auto c = place(0, 0, 5, 5);
    p::apply_particle_collisions(c.emitter, c.state, Colliders{circle(0, 0, 10, "exclude", 1), sphere(0, 0, 0, 10, "exclude", 1)});
    check(c.px() == 0 && c.py() == 0 && c.vx() == 5 && c.vy() == 5, "exclude at the exact center has no normal and leaves the particle");
  }
  // Addition: a short positionsZ is neither read nor written past its end.
  {
    auto e = emitter_fixture(2);
    e->data->positions_z = Float32Array(1);
    e->data->particle_count = 2;
    e->data->transforms.set_index(5, 520);
    auto s = p::create_particle_emitter_state();
    p::ensure_particle_emitter_state_capacity(s, 2, false);
    s->velocities.set_index(4, 100);
    p::apply_particle_collisions(e, s, Colliders{plane(0, -1, -500)});
    check(e->data->positions_z.size() == 1 && near(e->data->transforms.element(5), 500) && near(s->velocities.element(4), 0),
          "the second particle collides with z read as 0");
  }

  const auto place_object = [](double px, double py, double vx, double vy) {
    auto objects = make_objects(2);
    for (const auto& o : objects) o->visible = true;
    auto state = p::create_particle_objects_state(2);
    state->lifetimes.set_index(1, 100);
    state->lifetimes.set_index(3, 0);
    objects.element(0)->x = px;
    objects.element(0)->y = py;
    state->velocities.set_index(0, vx);
    state->velocities.set_index(1, vy);
    return std::make_pair(objects, state);
  };
  {
    auto [objects, state] = place_object(0, 520, 0, 100);
    p::apply_particle_object_collisions(objects, state, Colliders{plane(0, -1, -500)});
    check(near(objects.element(0)->y, 500) && near(state->velocities.element(1), 0), "object path: pushed onto the floor, normal velocity killed");
  }
  {
    auto [objects, state] = place_object(0, 520, 0, 100);
    p::apply_particle_object_collisions(objects, state, Colliders{plane(0, -1, -500, 0.5)});
    check(near(state->velocities.element(1), -50), "object path: bounces with restitution");
  }
  {
    auto [objects, state] = place_object(0, 100, 0, 0);
    objects.element(1)->y = 520;
    p::apply_particle_object_collisions(objects, state, Colliders{plane(0, -1, -500)});
    check(objects.element(1)->y == 520, "object path: skips dead slots");
  }
  {
    auto [objects, state] = place_object(0, 520, 0, 100);
    p::apply_particle_object_collisions(objects, state, Colliders{});
    check(objects.element(0)->y == 520 && state->velocities.element(1) == 100, "object path: no-op with no colliders");
  }
}

using UpdateOptions = flight::types::ParticleObjectsUpdateOptions;
using Callbacks = flight::types::ParticleEmitterCallbacks;

Ref<UpdateOptions> emitter_at(double x, double y) {
  return flight::make_ref<UpdateOptions>(UpdateOptions{.emitter_x = x, .emitter_y = y});
}

void update_objects() {
  {
    auto state = p::create_particle_objects_state(0);
    bool threw = false;
    try {
      p::update_particle_objects(make_objects(0), state, p::create_particle_emitter_config(), 1);
    } catch (...) {
      threw = true;
    }
    check(!threw && state->spawn_accumulator == 0, "does nothing when the objects array is empty");
  }
  {
    auto objects = make_objects(3);
    auto state = p::create_particle_objects_state(3);
    p::update_particle_objects(objects, state, config([](Input& i) {
                                 i.spawn_rate = 2;
                                 i.lifetime_min = 10;
                                 i.lifetime_max = 10;
                               }),
                               1);
    check(visible_count(objects) == 2 && objects.element(0)->visible && objects.element(1)->visible && !objects.element(2)->visible,
          "spawns objects from the pool, into the first dead slots in order");
  }
  const auto still = [](Input& i) {
    i.spawn_rate = 1;
    i.lifetime_min = 10;
    i.lifetime_max = 10;
    i.speed_min = 0;
    i.speed_max = 0;
  };
  {
    auto objects = make_objects(1);
    auto state = p::create_particle_objects_state(1);
    p::update_particle_objects(objects, state, config(still), 1);
    check(objects.element(0)->visible && objects.element(0)->x == 0 && objects.element(0)->y == 0,
          "sets spawned objects visible and positions them at the origin");
  }
  {
    auto objects = make_objects(1);
    auto state = p::create_particle_objects_state(1);
    const auto c = config([](Input& i) {
      i.spawn_rate = 1;
      i.lifetime_min = 0.5;
      i.lifetime_max = 0.5;
      i.speed_min = 0;
      i.speed_max = 0;
    });
    p::update_particle_objects(objects, state, c, 1);
    const bool spawned = objects.element(0)->visible;
    p::update_particle_objects(objects, state, c, 0.6);
    check(spawned && !objects.element(0)->visible && state->lifetimes.element(1) == 0, "kills objects when their lifetime expires");
    // Addition: age reaching the lifetime exactly is death (>=).
    auto exact = make_objects(1);
    auto exactState = p::create_particle_objects_state(1);
    const auto quiet = config([](Input& i) {
      i.spawn_rate = 0;
      i.burst_count = 1;
      i.lifetime_min = 0.5;
      i.lifetime_max = 0.5;
    });
    p::update_particle_objects(exact, exactState, quiet, 0.25);
    p::update_particle_objects(exact, exactState, quiet, 0.5);
    check(!exact.element(0)->visible, "a particle whose age reaches its lifetime exactly dies");
  }
  {
    auto objects = make_objects(1);
    auto state = p::create_particle_objects_state(1);
    const auto c = config([&still](Input& i) {
      still(i);
      i.gravity_x = 100;
      i.gravity_y = 0;
    });
    p::update_particle_objects(objects, state, c, 1);
    p::update_particle_objects(objects, state, c, 1);
    check(objects.element(0)->x > 0, "moves objects according to velocity and gravity");
    check(objects.element(0)->x == 100 && state->velocities.element(0) == 100, "Addition: gravity integrates velocity, then position");
    p::update_particle_objects(objects, state, c, 0.5);
    check(objects.element(0)->x == 175 && state->velocities.element(0) == 150, "Addition: semi-implicit Euler on a half step");
  }
  {
    auto objects = make_objects(1);
    auto state = p::create_particle_objects_state(1);
    const auto c = config([](Input& i) {
      i.spawn_rate = 1;
      i.lifetime_min = 1;
      i.lifetime_max = 1;
      i.alpha_start = 1;
      i.alpha_end = 0;
      i.speed_min = 0;
      i.speed_max = 0;
    });
    p::update_particle_objects(objects, state, c, 1);
    const double atSpawn = objects.element(0)->alpha;
    p::update_particle_objects(objects, state, c, 0.5);
    check(atSpawn == 1 && near(objects.element(0)->alpha, 0.5), "fades alpha from alphaStart to alphaEnd over lifetime");
  }
  {
    auto objects = make_objects(2);
    auto state = p::create_particle_objects_state(2);
    p::update_particle_objects(objects, state, config([](Input& i) {
                                 i.spawn_rate = 100;
                                 i.lifetime_min = 10;
                                 i.lifetime_max = 10;
                               }),
                               1);
    check(visible_count(objects) == 2, "never exceeds the pool");
    check(state->spawn_accumulator == 0, "Addition: whole spawns the pool cannot hold are dropped, not carried");
  }
  {
    auto objects = make_objects(1);
    auto state = p::create_particle_objects_state(1);
    const auto c = config([](Input& i) {
      i.spawn_rate = 2;
      i.lifetime_min = 0.1;
      i.lifetime_max = 0.1;
      i.speed_min = 0;
      i.speed_max = 0;
    });
    p::update_particle_objects(objects, state, c, 1);
    p::update_particle_objects(objects, state, c, 1);
    check(objects.element(0)->visible && state->lifetimes.element(0) == 0, "re-uses dead slots for new spawns in the same frame");
  }
  {
    auto objects = make_objects(1);
    auto state = p::create_particle_objects_state(1);
    const auto c = config([](Input& i) {
      i.spawn_rate = 1;
      i.lifetime_min = 1;
      i.lifetime_max = 1;
      i.speed_min = 0;
      i.speed_max = 0;
      i.scale_min = 2;
      i.scale_max = 2;
      i.scale_end = 0;
    });
    p::update_particle_objects(objects, state, c, 1);
    const double atSpawn = objects.element(0)->scale_x;
    p::update_particle_objects(objects, state, c, 0.5);
    check(atSpawn == 2 && near(objects.element(0)->scale_x, 1) && objects.element(0)->scale_y == objects.element(0)->scale_x,
          "animates scale over lifetime when scaleEnd differs from 1");
  }
  {
    auto objects = make_objects(1);
    auto state = p::create_particle_objects_state(1);
    const auto c = config([&still](Input& i) {
      still(i);
      i.spread = 0;
      i.direction_x = 0;
      i.direction_y = -1;
      i.rotation_speed_min = std::numbers::pi;
      i.rotation_speed_max = std::numbers::pi;
    });
    p::update_particle_objects(objects, state, c, 1);
    const double before = objects.element(0)->rotation;
    p::update_particle_objects(objects, state, c, 1);
    check(near(before, -std::numbers::pi / 2) && near(objects.element(0)->rotation - before, std::numbers::pi, 1e-3),
          "rotates at the per-particle rotation speed, from the spawn angle");
  }
  {
    auto objects = make_objects(20);
    auto state = p::create_particle_objects_state(20);
    p::update_particle_objects(objects, state, config([&still](Input& i) {
                                 still(i);
                                 i.spawn_rate = 20;
                                 i.max_particles = 20;
                                 i.emitter_shape = String("circle");
                                 i.emitter_radius = 50;
                               }),
                               1);
    bool inside = visible_count(objects) == 20;
    for (const auto& o : objects) inside = inside && std::sqrt(o->x * o->x + o->y * o->y) <= 50 + 1e-4;
    check(inside, "spawns objects within the circle emitter shape");
  }
  {
    auto objects = make_objects(1);
    auto state = p::create_particle_objects_state(1, [] { return 0.0; });
    p::update_particle_objects(objects, state, config([&still](Input& i) {
                                 still(i);
                                 i.emitter_shape = String("line");
                                 i.emitter_width = 80;
                               }),
                               1);
    check(objects.element(0)->x == -40 && objects.element(0)->y == 0, "spawns on the line emitter's inclusive boundary");
  }
  {
    auto objects = make_objects(20);
    auto state = p::create_particle_objects_state(20, flight::math::create_random_source(12345));
    p::update_particle_objects(objects, state, config([&still](Input& i) {
                                 still(i);
                                 i.spawn_rate = 20;
                                 i.max_particles = 20;
                                 i.emitter_shape = String("ring");
                                 i.emitter_radius = 50;
                               }),
                               1);
    bool on = true;
    for (const auto& o : objects) on = on && near(std::sqrt(o->x * o->x + o->y * o->y), 50, 1e-4);
    check(on, "spawns objects on the ring emitter boundary");
  }
  for (const char* shape : {"line", "ring"}) {
    auto objects = make_objects(1);
    int calls = 0;
    auto state = p::create_particle_objects_state(1, [&calls] {
      ++calls;
      return 0.25;
    });
    p::update_particle_objects(objects, state, config([shape](Input& i) {
                                 i.emitter_shape = String(shape);
                                 i.emitter_width = 0;
                                 i.emitter_radius = 0;
                                 i.lifetime_min = 10;
                                 i.lifetime_max = 10;
                                 i.region_id_max = 0;
                                 i.region_id_min = 0;
                                 i.scale_max = 1;
                                 i.scale_min = 1;
                                 i.spawn_rate = 1;
                                 i.speed_max = 0;
                                 i.speed_min = 0;
                                 i.spread = 0;
                               }),
                               1);
    check(objects.element(0)->x == 0 && objects.element(0)->y == 0 && calls == 4,
          std::string("treats a degenerate ") + shape + " as a point: lifetime, angle, speed and scale are the only draws");
  }
  {
    auto objects = make_objects(30);
    auto state = p::create_particle_objects_state(30);
    const auto c = config([](Input& i) {
      i.spawn_rate = 0;
      i.burst_count = 10;
      i.burst_interval = 0;
      i.lifetime_min = 10;
      i.lifetime_max = 10;
    });
    p::update_particle_objects(objects, state, c, 1.0 / 60);
    const int first = visible_count(objects);
    p::update_particle_objects(objects, state, c, 1.0 / 60);
    check(first == 10 && visible_count(objects) == 10 && std::isinf(state->burst_timer), "fires a one-shot burst on the first frame only");
  }
  // Addition: a repeating burst fires again once its interval has elapsed.
  {
    auto objects = make_objects(30);
    auto state = p::create_particle_objects_state(30);
    const auto c = config([](Input& i) {
      i.spawn_rate = 0;
      i.burst_count = 3;
      i.burst_interval = 0.5;
      i.lifetime_min = 10;
      i.lifetime_max = 10;
    });
    p::update_particle_objects(objects, state, c, 0.25);
    const int first = visible_count(objects);
    p::update_particle_objects(objects, state, c, 0.25);
    const int second = visible_count(objects);
    p::update_particle_objects(objects, state, c, 0.25);
    check(first == 3 && second == 3 && visible_count(objects) == 6, "a burst repeats at its interval");
  }
  {
    auto objects = make_objects(2);
    auto state = p::create_particle_objects_state(2);
    int count = 0;
    auto options = flight::make_ref<UpdateOptions>(UpdateOptions{
        .callbacks = flight::make_ref<Callbacks>(Callbacks{.on_spawn = [&count](double, double, double) { ++count; }})});
    p::update_particle_objects(objects, state, config([](Input& i) {
                                 i.spawn_rate = 2;
                                 i.lifetime_min = 10;
                                 i.lifetime_max = 10;
                               }),
                               1, options);
    check(count == 2, "fires onSpawn for each spawned object");
  }
  {
    auto objects = make_objects(1);
    auto state = p::create_particle_objects_state(1, [] { return 0.0; });
    const auto c = config([](Input& i) {
      i.duration = 1;
      i.emitter_shape = String("line");
      i.emitter_width = 20;
      i.lifetime_max = 0.5;
      i.lifetime_min = 0.5;
      i.loop = false;
      i.spawn_rate = 1;
      i.speed_max = 0;
      i.speed_min = 0;
    });
    std::vector<std::vector<double>> spawns, deaths;
    auto options = flight::make_ref<UpdateOptions>(UpdateOptions{.callbacks = flight::make_ref<Callbacks>(Callbacks{
                                                                     .on_death = [&deaths](double x, double y, double z) { deaths.push_back({x, y, z}); },
                                                                     .on_spawn = [&spawns](double x, double y, double z) { spawns.push_back({x, y, z}); }})});
    p::update_particle_objects(objects, state, c, 1, options);
    const auto afterSpawn = spawns;
    p::update_particle_objects(objects, state, c, 0.6, options);
    check(afterSpawn == std::vector<std::vector<double>>{{-10, 0, 0}} && deaths == std::vector<std::vector<double>>{{-10, 0, 0}} &&
              spawns.size() == 1,
          "spawn and death callbacks carry the shared 3D payload");
  }
  {
    auto objects = make_objects(1);
    auto state = p::create_particle_objects_state(1);
    const auto c = config([](Input& i) {
      i.spawn_rate = 0;
      i.burst_count = 1;
      i.burst_interval = 0;
      i.lifetime_min = 10;
      i.lifetime_max = 10;
      i.speed_min = 0;
      i.speed_max = 0;
      i.spread = 0;
      i.velocity_inheritance = 1;
    });
    p::update_particle_objects(objects, state, c, 1.0 / 60, emitter_at(0, 0));
    const int before = visible_count(objects);
    state->burst_timer = 0;
    p::update_particle_objects(objects, state, c, 0, emitter_at(100, 0));
    check(visible_count(objects) == before && std::isfinite(state->velocities.element(0)) && std::isfinite(state->velocities.element(1)),
          "ignores a zero-deltaTime frame: no spawning or velocity corruption");
    check(state->prev_x == 0 && state->burst_timer == 0, "Addition: a zero-deltaTime frame records nothing");
  }
  {
    auto objects = make_objects(2);
    auto state = p::create_particle_objects_state(2);
    const auto c = config([](Input& i) {
      i.spawn_rate = 10;
      i.lifetime_min = 10;
      i.lifetime_max = 10;
    });
    p::update_particle_objects(objects, state, c, 1);
    const int before = visible_count(objects);
    const double age = state->lifetimes.element(0);
    p::update_particle_objects(objects, state, c, -1);
    check(visible_count(objects) == before && state->lifetimes.element(0) == age, "ignores a negative-deltaTime frame");
  }
  {
    auto objects = make_objects(50);
    auto state = p::create_particle_objects_state(50);
    const auto c = config([](Input& i) {
      i.spawn_rate = 10;
      i.lifetime_min = 100;
      i.lifetime_max = 100;
      i.duration = 1;
      i.loop = false;
    });
    for (int i = 0; i < 20; ++i) p::update_particle_objects(objects, state, c, 0.1);
    const int live = visible_count(objects);
    for (int i = 0; i < 20; ++i) p::update_particle_objects(objects, state, c, 0.1);
    check(live > 0 && visible_count(objects) == live, "a finite, non-looping emitter stops spawning after its duration");
    // Ten 0.1 s steps sum to 0.9999999999999999 < 1, so -- as in JavaScript -- an eleventh frame still emits.
    check(live == 11, "Addition: emission runs while the accumulated age is below the duration");
    auto looping = p::create_particle_objects_state(2);
    p::update_particle_objects(make_objects(2), looping, config([](Input& i) {
                                 i.duration = 1;
                                 i.loop = true;
                               }),
                               0.5);
    auto endless = p::create_particle_objects_state(2);
    p::update_particle_objects(make_objects(2), endless, config([](Input& i) {
                                 i.duration = 0;
                                 i.loop = false;
                               }),
                               0.5);
    check(state->emitter_age > 3.9 && looping->emitter_age == 0 && endless->emitter_age == 0,
          "Addition: emitterAge advances only for a finite, non-looping emitter");
  }
  {
    auto objects = make_objects(50);
    auto state = p::create_particle_objects_state(50);
    const auto c = config([](Input& i) {
      i.spawn_rate = 10;
      i.lifetime_min = 0.5;
      i.lifetime_max = 0.5;
      i.duration = 1;
      i.loop = false;
    });
    const bool fresh = p::is_particle_objects_complete(objects, state, c);
    for (int i = 0; i < 10; ++i) p::update_particle_objects(objects, state, c, 0.1);
    const bool emitting = p::is_particle_objects_complete(objects, state, c);
    for (int i = 0; i < 10; ++i) p::update_particle_objects(objects, state, c, 0.1);
    check(!fresh && !emitting && visible_count(objects) == 0 && p::is_particle_objects_complete(objects, state, c),
          "isParticleObjectsComplete is true once finished and all objects are dead");
    check(!p::is_particle_objects_complete(objects, state, config([](Input& i) { i.duration = 0; })) &&
              !p::is_particle_objects_complete(objects, state, config([](Input& i) {
                i.duration = 1;
                i.loop = true;
              })),
          "is always false for infinite or looping emitters");
  }
  {
    auto objects = make_objects(1);
    auto state = p::create_particle_objects_state(1);
    const auto c = config([](Input& i) {
      i.spawn_rate = 1;
      i.lifetime_min = 1;
      i.lifetime_max = 1;
      i.speed_min = 0;
      i.speed_max = 0;
      i.scale_min = 4;
      i.scale_max = 4;
      i.alpha_curve = curve({1, 0, 1});
      i.scale_curve = curve({1, 0.5, 0});
      i.alpha_start = 0.3;
    });
    p::update_particle_objects(objects, state, c, 1);
    const double spawnAlpha = objects.element(0)->alpha, spawnScale = objects.element(0)->scale_x;
    p::update_particle_objects(objects, state, c, 0.5);
    check(near(objects.element(0)->alpha, 0, 0.01) && near(objects.element(0)->scale_x, 2, 0.01), "applies alpha and scale curves over lifetime");
    check(spawnAlpha == 1 && spawnScale == 4, "Addition: a spawn reads each curve at t = 0");
  }
  {
    const auto c = config([](Input& i) {
      i.spawn_rate = 5;
      i.lifetime_min = 10;
      i.lifetime_max = 10;
      i.speed_min = 10;
      i.speed_max = 200;
      i.spread = std::numbers::pi;
    });
    const auto run = [&c](double seed) {
      auto objects = make_objects(20);
      auto state = p::create_particle_objects_state(20, flight::math::create_random_source(seed));
      p::update_particle_objects(objects, state, c, 1);
      std::vector<double> out;
      for (const auto& o : objects) out.push_back(o->rotation);
      for (int i = 0; i < 10; ++i) out.push_back(state->velocities.element(i));
      return out;
    };
    check(run(12345) == run(12345), "the injected RNG makes spawning deterministic");
    check(run(12345) != run(54321), "Addition: a different seed spawns differently");
  }

  // Addition: the per-spawn draw order -- lifetime, angle, speed, shape, scale, then rotation speed -- and the values.
  {
    auto objects = make_objects(1);
    Sequence seq({0.5, 0.75, 0.25, 0.5, 0.5, 0.5, 1.0});
    auto state = p::create_particle_objects_state(1, seq.source());
    p::update_particle_objects(objects, state, config([](Input& i) {
                                 i.spawn_rate = 1;
                                 i.lifetime_min = 2;
                                 i.lifetime_max = 4;
                                 i.direction_x = 1;
                                 i.direction_y = 0;
                                 i.spread = 1;
                                 i.speed_min = 10;
                                 i.speed_max = 30;
                                 i.emitter_shape = String("rect");
                                 i.emitter_width = 10;
                                 i.emitter_height = 10;
                                 i.scale_min = 1;
                                 i.scale_max = 3;
                                 i.rotation_speed_min = 2;
                                 i.rotation_speed_max = 6;
                               }),
                               1);
    const auto& o = objects.element(0);
    check(seq.draws() == 7, "a spawn draws seven times with a rect shape and rotation speed");
    check(state->lifetimes.element(1) == 3 && near(o->rotation, 0.5) && near(state->velocities.element(0), std::cos(0.5) * 15, 1e-4) &&
              near(state->velocities.element(1), std::sin(0.5) * 15, 1e-4),
          "lifetime, angle and speed come from the first three draws");
    check(o->x == 0 && o->y == 0 && o->scale_x == 2 && o->scale_y == 2 && state->rotation_speeds.element(0) == 6,
          "the shape takes the next two draws, then scale, then rotation speed");
  }
  // Addition: velocity inheritance adds the emitter's velocity times the factor, and needs a previous position.
  {
    auto objects = make_objects(2);
    auto state = p::create_particle_objects_state(2, [] { return 0.5; });
    const auto c = config([](Input& i) {
      i.spawn_rate = 2;
      i.lifetime_min = 10;
      i.lifetime_max = 10;
      i.speed_min = 0;
      i.speed_max = 0;
      i.velocity_inheritance = 0.5;
    });
    p::update_particle_objects(objects, state, c, 0.5, emitter_at(10, 20));
    const double firstVx = state->velocities.element(0);
    p::update_particle_objects(objects, state, c, 0.5, emitter_at(30, 0));
    check(firstVx == 0 && state->prev_x == 30 && state->prev_y == 0, "the first frame has no previous position, so inherits nothing");
    check(near(state->velocities.element(2), 0.5 * 40) && near(state->velocities.element(3), 0.5 * -40),
          "a later spawn inherits (delta / dt) * factor");
    p::update_particle_objects(objects, state, c, 0.5);
    check(state->prev_x == 30 && state->prev_y == 0, "a frame without an emitter position keeps the previous one");
  }
  // Addition: a fractional spawn rate carries its remainder.
  {
    auto objects = make_objects(10);
    auto state = p::create_particle_objects_state(10);
    const auto c = config([](Input& i) {
      i.spawn_rate = 1.5;
      i.lifetime_min = 10;
      i.lifetime_max = 10;
    });
    p::update_particle_objects(objects, state, c, 1);
    const int first = visible_count(objects);
    p::update_particle_objects(objects, state, c, 1);
    check(first == 1 && visible_count(objects) == 3 && state->spawn_accumulator == 0, "1.5 per second spawns 1, then 2");
  }
  // Addition: a dead slot is not integrated, and a live one ages by deltaTime.
  {
    auto objects = make_objects(2);
    auto state = p::create_particle_objects_state(2);
    const auto c = config([](Input& i) {
      i.spawn_rate = 1;
      i.lifetime_min = 10;
      i.lifetime_max = 10;
      i.speed_min = 0;
      i.speed_max = 0;
      i.gravity_y = 10;
    });
    p::update_particle_objects(objects, state, c, 1);
    objects.element(1)->y = 7;
    p::update_particle_objects(objects, state, config([](Input& i) {
                                 i.spawn_rate = 0;
                                 i.gravity_y = 10;
                               }),
                               0.25);
    check(state->lifetimes.element(0) == 0.25 && objects.element(1)->y == 7 && !objects.element(1)->visible && state->velocities.element(3) == 0,
          "only live slots age and integrate");
  }
}

} // namespace

int main() {
  particle_emitter_config();
  particle_emitter_signals();
  particle_emitter_state();
  particle_objects_state();
  spawn_offset();
  validate_and_normalize();
  step_objects();
  curves();
  forces();
  collisions();
  update_objects();
  step_objects_in_order();
  return failures == 0 ? 0 : 1;
}
