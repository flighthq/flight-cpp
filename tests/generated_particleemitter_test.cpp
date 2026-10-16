// The generated @flighthq/particleemitter at a627849, exercised natively at the data level: the SoA particle
// buffer operations of particleEmitter.ts and particleEmitter3D.ts (append, remove, reserve, compact, clear,
// capacity, per-particle getters and setters, createParticleEmitterData, local bounds, view-depth sorting) and
// the sub-emitter bursts of emitParticleBurst2D.ts and emitParticleBurst3D.ts.
//
// Ported from packages/particleemitter/src/{particleEmitter,particleEmitter3D,emitParticleBurst2D,
// emitParticleBurst3D}.test.ts, one function per upstream describe block. Upstream's seededRandom LCG is
// reproduced with JavaScript's double arithmetic and ToInt32, and createRandomSource is @flighthq/math's own.
// Where an addition needs exact values, a RandomSource replaying a fixed sequence stands in; a draw past its end
// reads NaN, so an unexpected extra draw shows.
//
// Fixture provenance: upstream builds every emitter with createParticleEmitter2D/3D, which is residual here (see
// below). The fixtures mirror what those constructors produce, read from the source: createNode2D's
// initializeNode (enabled true, name null, data from createParticleEmitterData) and the transform, appearance,
// blend-mode, material and clip trait initializers (pivot 0, rotation 0, scale 1, skew 0, x/y 0, alpha 1,
// visible true, blendMode/material/materialData/clip null); for 3D, initializeNode3D (enabled true, name null,
// alpha 1, visible true, position (0,0,0), rotation identity, scale (1,1,1)) and blendMode 'normal'. The runtime
// slot holds the scene2d override's create_node_2d_runtime() / scene3d override's create_node_3d_runtime(), because
// a ParticleEmitter2DRuntime cannot be built (createParticleEmitter2DRuntime is refused); that base runtime lacks
// the emitter's computeLocalBoundsRectangle method, which no ported case here reaches.
//
// Lines marked "// Addition:" assert what the TypeScript source implies but upstream does not check: exact array
// contents after append/remove/compact, exact capacity growth, lane-complete swaps, sentinel handling through
// Uint16 wrapping, bounds values for rotated/scaled quads, burst spawn counts, per-spawn random draw order and
// values, capacity of both emitter and state after bursts, and out-of-range / zero-capacity behavior. Every
// expected value is derived from the .ts source, not from observed C++ output.
//
// Not natively evidenced (residual): the compiler refuses createParticleEmitter2D (contextual union callable),
// createParticleEmitter2DRuntime (reference assertion without heritage), getParticleEmitter2DRuntime and
// setParticleEmitter2DLocalBoundsRectangle (readonly-to-writable structural assertion), copyLocalBoundsRectangle
// and the defaultMethods table; create_particle_emitter_2d / create_particle_emitter_3d are declarations without a
// definition, so cloneParticleEmitter2D/3D (which call them) cannot link. All are blocked on Scene2D/Scene3D
// node-runtime heritage. Upstream cases not ported for that reason:
//   - particleEmitter.test.ts: cloneParticleEmitter2D (2 cases), createParticleEmitter2D (3),
//     createParticleEmitter2DRuntime (2), getParticleEmitter2DRuntime (2), setParticleEmitter2DLocalBoundsRectangle
//     (3), and the getNodeLocalBoundsRectangle half of "does not store the result on the emitter" (it dispatches
//     through the refused runtime method; the revision half is ported).
//   - particleEmitter3D.test.ts: cloneParticleEmitter3D (1), createParticleEmitter3D (4).
//   - emitParticleBurst2D.test.ts: "composes into a death sub-emitter via onDeath" drives updateParticleEmitter2D,
//     and the emitted update_particle_emitter_2d.hpp does not compile at this pin (`Traits` is undeclared at its
//     lines 97 and 446, the getNodeWorldMatrix and invalidateNodeLocalBounds casts).
#include <flight/particleemitter/emit_particle_burst_2d.hpp>
#include <flight/particleemitter/emit_particle_burst_3d.hpp>
#include <flight/particleemitter/particle_emitter.hpp>
#include <flight/particleemitter/particle_emitter_3d.hpp>

#include <flight/geometry/matrix4.hpp>
#include <flight/geometry/quaternion.hpp>
#include <flight/geometry/rectangle.hpp>
#include <flight/geometry/vector3.hpp>
#include <flight/math/random.hpp>
#include <flight/particles/particle_emitter_config.hpp>
#include <flight/particles/particle_emitter_state.hpp>
#include <flight/scene2d/display_object.hpp>
#include <flight/scene3d/scene_node.hpp>

#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

bool near(double a, double b, double epsilon = 1e-5) { return std::abs(a - b) <= epsilon; }
// toBeCloseTo(x) with vitest's default precision of 2 digits.
bool close(double a, double b) { return near(a, b, 0.005); }

namespace pe = flight::particleemitter;
namespace types = flight::types;
using flight::Float32Array;
using flight::Ref;
using flight::String;

constexpr double pi = std::numbers::pi;
constexpr double nan = std::numeric_limits<double>::quiet_NaN();

template <typename Fn>
void guarded(const std::string& name, Fn&& fn) {
  try {
    fn();
  } catch (const std::exception& e) {
    check(false, name + ": unexpected exception: " + e.what());
  } catch (...) {
    check(false, name + ": unexpected non-standard exception");
  }
}

// ---- fixtures -------------------------------------------------------------------------------------------------

// createParticleEmitter2D() as createNode2D + createParticleEmitterData would build it (see header).
Ref<types::ParticleEmitter2D> emitter2d(Ref<types::ParticleEmitterData> data = pe::create_particle_emitter_data()) {
  auto out = flight::make_ref<types::ParticleEmitter2D>();
  out->data = data;
  out->enabled = true;
  out->kind = types::particle_emitter_2d_kind;
  out->name = std::nullopt;
  out->entity_runtime_key = flight::scene2d::create_node_2d_runtime();
  out->alpha = 1;
  out->visible = true;
  out->blend_mode = std::nullopt;
  out->clip = std::nullopt;
  out->material = std::nullopt;
  out->material_data = std::nullopt;
  out->pivot_x = 0;
  out->pivot_y = 0;
  out->rotation = 0;
  out->scale_x = 1;
  out->scale_y = 1;
  out->skew_x = 0;
  out->skew_y = 0;
  out->x = 0;
  out->y = 0;
  return out;
}

// createParticleEmitter3D() as initializeNode3D + createParticleEmitterData would build it (see header).
Ref<types::ParticleEmitter3D> emitter3d(Ref<types::ParticleEmitterData> data = pe::create_particle_emitter_data()) {
  auto out = flight::make_ref<types::ParticleEmitter3D>();
  out->data = data;
  out->enabled = true;
  out->kind = types::particle_emitter_3d_kind;
  out->name = std::nullopt;
  out->entity_runtime_key = flight::scene3d::create_node_3d_runtime();
  out->alpha = 1;
  out->visible = true;
  out->position = flight::geometry::create_vector3();
  out->rotation = flight::geometry::create_quaternion();
  out->scale = flight::geometry::create_vector3(1.0, 1.0, 1.0);
  out->blend_mode = String("normal");
  return out;
}

Ref<types::TextureAtlasRegion> region(double id = 0, double x = 0, double y = 0, double width = 32, double height = 32,
                                      types::TextureAtlasRotation rotation = types::TextureAtlasRotation::None) {
  auto r = flight::make_ref<types::TextureAtlasRegion>();
  r->id = id;
  r->x = x;
  r->y = y;
  r->width = width;
  r->height = height;
  r->pivot_x = std::nullopt;
  r->pivot_y = std::nullopt;
  r->rotation = rotation;
  r->source_x = 0;
  r->source_y = 0;
  r->trimmed = false;
  return r;
}

Ref<types::TextureAtlas> atlas(std::initializer_list<Ref<types::TextureAtlasRegion>> regions) {
  auto a = flight::make_ref<types::TextureAtlas>();
  a->regions = flight::Array<Ref<types::TextureAtlasRegion>>(regions);
  a->texture = std::nullopt;
  a->image_width = 0;
  a->image_height = 0;
  a->scale = 1;
  return a;
}

Ref<types::Vector2> vec2(double x, double y) {
  auto v = flight::make_ref<types::Vector2>();
  v->x = x;
  v->y = y;
  return v;
}

Ref<types::Vector3> vec3(double x, double y, double z) { return flight::geometry::create_vector3(x, y, z); }

Ref<types::AabbLike> aabb() {
  auto out = flight::make_ref<types::AabbLike>();
  out->min = vec3(0, 0, 0);
  out->max = vec3(0, 0, 0);
  return out;
}

Float32Array f32(std::initializer_list<float> values) { return Float32Array(values); }
flight::Uint16Array u16(std::initializer_list<std::uint16_t> values) { return flight::Uint16Array(values); }

// createParticleEmitterConfig(partial): every field is `partial.x ?? default`, so a default config with the named
// fields overwritten is the same object.
Ref<types::ParticleEmitterConfig> config(const std::function<void(types::ParticleEmitterConfig&)>& fill = {}) {
  auto c = flight::particles::create_particle_emitter_config();
  if (fill) fill(*c);
  return c;
}

// A RandomSource replaying `values` in order; a draw past the end reads NaN.
struct Sequence {
  std::shared_ptr<std::vector<double>> values;
  std::shared_ptr<std::size_t> index = std::make_shared<std::size_t>(0);
  explicit Sequence(std::vector<double> v) : values(std::make_shared<std::vector<double>>(std::move(v))) {}
  types::RandomSource source() const {
    auto vs = values;
    auto at = index;
    return [vs, at]() { return *at < vs->size() ? (*vs)[(*at)++] : nan; };
  }
  std::size_t draws() const { return *index; }
};

// emitParticleBurst3D.test.ts's seededRandom: s = (s * 1103515245 + 12345) & 0x7fffffff, in JS doubles.
types::RandomSource seeded_random(double seed) {
  auto s = std::make_shared<double>(seed);
  return [s]() {
    const double product = (*s) * 1103515245.0 + 12345.0;
    double m = std::fmod(std::trunc(product), 4294967296.0);
    if (m < 0) m += 4294967296.0;
    const auto u = static_cast<std::uint32_t>(m);
    *s = static_cast<double>(u & 0x7fffffffu);
    return *s / 2147483647.0;
  };
}

double t(const Ref<types::ParticleEmitter2D>& e, double i) { return e->data->transforms.get_index(i); }
double t(const Ref<types::ParticleEmitter3D>& e, double i) { return e->data->transforms.get_index(i); }

// =============================================================================================================
// particleEmitter.test.ts
// =============================================================================================================

void append_particle_emitter_2d_particle() {
  guarded("appendParticleEmitter2DParticle", [] {
    {
      auto emitter = emitter2d();
      const double idx = pe::append_particle_emitter_2d_particle(emitter, 2, 10, 20, 0.5, 1.5);
      check(idx == 0, "append2D: returns index 0");
      check(emitter->data->particle_count == 1, "append2D: particleCount 1");
      check(emitter->data->ids.get_index(0) == 2, "append2D: id");
      check(t(emitter, 0) == 10 && t(emitter, 1) == 20 && t(emitter, 2) == 0.5 && t(emitter, 3) == 1.5,
            "append2D: transform x, y, rotation, scale");
      // Addition: the remaining lanes of the new slot are written: velocity (0,0) and positionsZ 0.
      check(emitter->data->velocities.get_index(0) == 0 && emitter->data->velocities.get_index(1) == 0 &&
                emitter->data->positions_z.get_index(0) == 0,
            "append2D: velocity and positionsZ zeroed");
    }
    {
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, 0, 0, 0, 0, 1);
      check(emitter->data->alphas.get_index(0) == 1, "append2D: alpha 1");
      check(emitter->data->colors.get_index(0) == 1 && emitter->data->colors.get_index(1) == 1 &&
                emitter->data->colors.get_index(2) == 1,
            "append2D: color white");
    }
    {
      auto emitter = emitter2d();
      check(pe::append_particle_emitter_2d_particle(emitter, 0, 0, 0, 0, 1) == 0, "append2D: first index 0");
      check(pe::append_particle_emitter_2d_particle(emitter, 1, 5, 5, 0, 1) == 1, "append2D: second index 1");
      check(emitter->data->particle_count == 2, "append2D: count 2");
    }
    {
      auto emitter = emitter2d();
      for (int i = 0; i < 10; ++i) pe::append_particle_emitter_2d_particle(emitter, i, 0, 0, 0, 1);
      check(emitter->data->particle_count == 10, "append2D: auto-grow count 10");
      check(pe::get_particle_emitter_2d_capacity(emitter) >= 10, "append2D: auto-grow capacity >= 10");
      // Addition: growth is max(needed, count * 2 || 8): 8 on the first append, 16 on the ninth.
      check(pe::get_particle_emitter_2d_capacity(emitter) == 16, "append2D: capacity is exactly 16 after 10 appends");
      check(emitter->data->ids.size() == 16 && emitter->data->alphas.size() == 16 &&
                emitter->data->transforms.size() == 64 && emitter->data->colors.size() == 48 &&
                emitter->data->velocities.size() == 32 && emitter->data->positions_z.size() == 16,
            "append2D: every lane sized for 16 particles");
      // Addition: growth preserves earlier particles' contents.
      bool ids_ok = true;
      for (int i = 0; i < 10; ++i) ids_ok = ids_ok && emitter->data->ids.get_index(i) == i;
      check(ids_ok, "append2D: ids 0..9 survive growth");
    }
    {
      // Addition: the first append on an empty emitter reserves exactly 8.
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, 0, 0, 0, 0, 1);
      check(pe::get_particle_emitter_2d_capacity(emitter) == 8, "append2D: first growth is to 8");
    }
    {
      // Addition: re-appending over a cleared slot rewrites stale alpha, color and velocity.
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, 3, 1, 1, 0, 1);
      pe::set_particle_emitter_2d_particle_alpha(emitter, 0, 0.25);
      pe::set_particle_emitter_2d_particle_color(emitter, 0, 0.1, 0.2, 0.3);
      pe::set_particle_emitter_2d_particle_velocity(emitter, 0, 4, 5);
      emitter->data->positions_z.set_index(0, 9);
      pe::clear_particle_emitter_2d(emitter);
      pe::append_particle_emitter_2d_particle(emitter, 4, 2, 2, 0, 1);
      check(emitter->data->alphas.get_index(0) == 1 && emitter->data->colors.get_index(0) == 1 &&
                emitter->data->colors.get_index(1) == 1 && emitter->data->colors.get_index(2) == 1 &&
                emitter->data->velocities.get_index(0) == 0 && emitter->data->velocities.get_index(1) == 0 &&
                emitter->data->positions_z.get_index(0) == 0,
            "append2D: stale slot lanes are reset");
    }
    {
      // Addition: ids are Uint16; -1 and 0x1ffff wrap to the 0xffff deletion sentinel.
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, -1, 0, 0, 0, 1);
      pe::append_particle_emitter_2d_particle(emitter, 0x1ffff, 0, 0, 0, 1);
      check(emitter->data->ids.get_index(0) == 0xffff && emitter->data->ids.get_index(1) == 0xffff,
            "append2D: ids wrap modulo 2^16");
    }
  });
}

void clear_particle_emitter_2d() {
  guarded("clearParticleEmitter2D", [] {
    auto emitter = emitter2d();
    pe::reserve_particle_emitter_2d(emitter, 50);
    emitter->data->particle_count = 10;
    const double before = pe::get_particle_emitter_2d_capacity(emitter);
    const auto ids = emitter->data->ids;
    pe::clear_particle_emitter_2d(emitter);
    check(emitter->data->particle_count == 0, "clear2D: count 0");
    check(pe::get_particle_emitter_2d_capacity(emitter) == before, "clear2D: capacity kept");
    // Addition: the buffers themselves are kept (same identity), not reallocated.
    check(emitter->data->ids == ids, "clear2D: ids array identity kept");
    // Addition: getters see the cleared range as out of range.
    check(pe::get_particle_emitter_2d_particle_id(emitter, 0) == -1, "clear2D: index 0 is out of range after clear");
  });
}

void compact_particle_emitter_2d() {
  guarded("compactParticleEmitter2D", [] {
    {
      auto emitter = emitter2d();
      pe::compact_particle_emitter_2d(emitter);
      check(emitter->data->particle_count == 0, "compact2D: empty no-op");
    }
    {
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, 10, 1, 1, 0, 1);
      pe::append_particle_emitter_2d_particle(emitter, 11, 2, 2, 0, 1);
      pe::append_particle_emitter_2d_particle(emitter, 12, 3, 3, 0, 1);
      emitter->data->ids.set_index(1, pe::particle_emitter_deleted_id);
      pe::compact_particle_emitter_2d(emitter);
      check(emitter->data->particle_count == 2, "compact2D: count 2");
      check(pe::get_particle_emitter_2d_particle_id(emitter, 0) == 10, "compact2D: first survivor");
      check(pe::get_particle_emitter_2d_particle_id(emitter, 1) == 12, "compact2D: second survivor");
      check(t(emitter, 4) == 3, "compact2D: survivor kept its transform x");
      // Addition: the whole transform moved, not just x.
      check(t(emitter, 5) == 3 && t(emitter, 6) == 0 && t(emitter, 7) == 1, "compact2D: survivor y, rotation, scale");
    }
    {
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, 1, 0, 0, 0, 1);
      pe::append_particle_emitter_2d_particle(emitter, 2, 0, 0, 0, 1);
      pe::compact_particle_emitter_2d(emitter);
      check(emitter->data->particle_count == 2, "compact2D: fully live count unchanged");
      check(pe::get_particle_emitter_2d_particle_id(emitter, 0) == 1 && pe::get_particle_emitter_2d_particle_id(emitter, 1) == 2,
            "compact2D: fully live order unchanged");
    }
    {
      // Addition: sentinels at the head, middle and tail; every lane of each survivor slides down in order.
      auto emitter = emitter2d();
      for (int i = 0; i < 6; ++i) {
        pe::append_particle_emitter_2d_particle(emitter, 20 + i, i, 10 * i, 0.5 * i, 1 + i);
        pe::set_particle_emitter_2d_particle_alpha(emitter, i, 0.125 * i);
        pe::set_particle_emitter_2d_particle_color(emitter, i, 0.0625 * i, 0.125 * i, 0.25 * i);
        pe::set_particle_emitter_2d_particle_velocity(emitter, i, i, -i);
        emitter->data->positions_z.set_index(i, 100 + i);
      }
      emitter->data->ids.set_index(0, 0xffff);
      emitter->data->ids.set_index(2, 0xffff);
      emitter->data->ids.set_index(5, 0xffff);
      pe::compact_particle_emitter_2d(emitter);
      check(emitter->data->particle_count == 3, "compact2D: three survivors");
      const int survivors[3] = {1, 3, 4};
      for (int w = 0; w < 3; ++w) {
        const int r = survivors[w];
        const auto& d = emitter->data;
        const std::string tag = "compact2D: slot " + std::to_string(w) + " holds source " + std::to_string(r);
        check(d->ids.get_index(w) == 20 + r, tag + " id");
        check(d->transforms.get_index(w * 4) == r && d->transforms.get_index(w * 4 + 1) == 10 * r &&
                  d->transforms.get_index(w * 4 + 2) == 0.5 * r && d->transforms.get_index(w * 4 + 3) == 1 + r,
              tag + " transform");
        check(d->alphas.get_index(w) == 0.125 * r, tag + " alpha");
        check(d->colors.get_index(w * 3) == 0.0625 * r && d->colors.get_index(w * 3 + 1) == 0.125 * r &&
                  d->colors.get_index(w * 3 + 2) == 0.25 * r,
              tag + " color");
        check(d->velocities.get_index(w * 2) == r && d->velocities.get_index(w * 2 + 1) == -r, tag + " velocity");
        check(d->positions_z.get_index(w) == 100 + r, tag + " positionsZ");
      }
    }
    {
      // Addition: an all-deleted buffer compacts to zero.
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, -1, 0, 0, 0, 1);
      pe::append_particle_emitter_2d_particle(emitter, 0xffff, 0, 0, 0, 1);
      pe::compact_particle_emitter_2d(emitter);
      check(emitter->data->particle_count == 0, "compact2D: all sentinels compact to zero");
    }
  });
}

void compute_particle_emitter_2d_local_bounds_rectangle() {
  guarded("computeParticleEmitter2DLocalBoundsRectangle", [] {
    using flight::geometry::create_rectangle;
    {
      auto emitter = emitter2d();
      auto out = create_rectangle(1.0, 2.0, 3.0, 4.0);
      pe::compute_particle_emitter_2d_local_bounds_rectangle(out, emitter);
      check(out->x == 0 && out->y == 0 && out->width == 0 && out->height == 0, "bounds2D: null atlas is zero");
    }
    {
      auto emitter = emitter2d();
      emitter->data->atlas = atlas({region()});
      auto out = create_rectangle(1.0, 2.0, 3.0, 4.0);
      pe::compute_particle_emitter_2d_local_bounds_rectangle(out, emitter);
      check(out->width == 0 && out->height == 0, "bounds2D: zero count is zero size");
      // Addition: the origin is reset too.
      check(out->x == 0 && out->y == 0, "bounds2D: zero count resets origin");
    }
    {
      auto emitter = emitter2d();
      emitter->data->atlas = atlas({region(0, 0, 0, 10, 20)});
      emitter->data->particle_count = 1;
      emitter->data->ids = u16({0});
      emitter->data->transforms = f32({5, 10, 0, 1});
      auto out = create_rectangle();
      pe::compute_particle_emitter_2d_local_bounds_rectangle(out, emitter);
      check(close(out->x, 5) && close(out->y, 10) && close(out->width, 10) && close(out->height, 20),
            "bounds2D: single axis-aligned particle");
    }
    {
      auto emitter = emitter2d();
      emitter->data->atlas = atlas({region(0, 0, 0, 10, 20, types::TextureAtlasRotation::Clockwise90)});
      emitter->data->particle_count = 1;
      emitter->data->ids = u16({0});
      emitter->data->transforms = f32({5, 10, 0, 1});
      auto out = create_rectangle();
      pe::compute_particle_emitter_2d_local_bounds_rectangle(out, emitter);
      check(out->height == 10 && out->width == 20 && out->x == 5 && out->y == 10, "bounds2D: rotated region uses upright extent");
    }
    {
      auto emitter = emitter2d();
      emitter->data->atlas = atlas({region(0, 0, 0, 10, 10)});
      emitter->data->particle_count = 2;
      emitter->data->ids = u16({0, 0});
      emitter->data->transforms = f32({0, 0, 0, 1, 50, 50, 0, 1});
      auto out = create_rectangle();
      pe::compute_particle_emitter_2d_local_bounds_rectangle(out, emitter);
      check(close(out->x, 0) && close(out->y, 0) && close(out->width, 60) && close(out->height, 60),
            "bounds2D: union over two particles");
    }
    {
      auto emitter = emitter2d();
      emitter->data->atlas = atlas({region(0, 0, 0, 10, 10)});
      emitter->data->particle_count = 2;
      emitter->data->ids = u16({0, 99});
      emitter->data->transforms = f32({0, 0, 0, 1, 1000, 1000, 0, 1});
      auto out = create_rectangle();
      pe::compute_particle_emitter_2d_local_bounds_rectangle(out, emitter);
      check(close(out->width, 10) && close(out->height, 10), "bounds2D: out-of-range region id skipped");
    }
    {
      auto emitter = emitter2d();
      emitter->data->atlas = atlas({region(0, 0, 0, 10, 10)});
      emitter->data->particle_count = 1;
      emitter->data->ids = u16({0});
      emitter->data->transforms = f32({0, 0, 0, 1});
      // getNodeLocalBoundsRevision(emitter) reads the node runtime's localBoundsId; flight::node's template does
      // not accept a ParticleEmitter2D row, so the same cell is read directly.
      const auto& runtime = emitter->entity_runtime_key.value();
      const double before = runtime->local_bounds_id;
      pe::compute_particle_emitter_2d_local_bounds_rectangle(create_rectangle(), emitter);
      check(runtime->local_bounds_id == before,
            "bounds2D: compute does not bump the local bounds revision");
    }
    {
      // Addition: rotation pi/2 at scale 2 on a 10x20 region at (5,10): corners (5,10), (5,30), (-35,30), (-35,10).
      auto emitter = emitter2d();
      emitter->data->atlas = atlas({region(0, 0, 0, 10, 20)});
      emitter->data->particle_count = 1;
      emitter->data->ids = u16({0});
      emitter->data->transforms = f32({5, 10, static_cast<float>(pi / 2), 2});
      auto out = create_rectangle();
      pe::compute_particle_emitter_2d_local_bounds_rectangle(out, emitter);
      check(near(out->x, -35) && near(out->y, 10) && near(out->width, 40) && near(out->height, 20),
            "bounds2D: rotated and scaled quad corners");
    }
    {
      // Addition: Counterclockwise90 swaps width and height as Clockwise90 does.
      auto emitter = emitter2d();
      emitter->data->atlas = atlas({region(0, 0, 0, 10, 20, types::TextureAtlasRotation::Counterclockwise90)});
      emitter->data->particle_count = 1;
      emitter->data->ids = u16({0});
      emitter->data->transforms = f32({0, 0, 0, 3});
      auto out = create_rectangle();
      pe::compute_particle_emitter_2d_local_bounds_rectangle(out, emitter);
      check(near(out->x, 0) && near(out->y, 0) && near(out->width, 60) && near(out->height, 30),
            "bounds2D: counterclockwise region swaps extent and scales");
    }
    {
      // Addition: a zero-size region and the 0xffff sentinel both contribute nothing; if nothing contributes the
      // result is the zero rectangle.
      auto emitter = emitter2d();
      emitter->data->atlas = atlas({region(0, 0, 0, 0, 10)});
      emitter->data->particle_count = 2;
      emitter->data->ids = u16({0, 0xffff});
      emitter->data->transforms = f32({7, 7, 0, 1, 9, 9, 0, 1});
      auto out = create_rectangle(1.0, 2.0, 3.0, 4.0);
      pe::compute_particle_emitter_2d_local_bounds_rectangle(out, emitter);
      check(out->x == 0 && out->y == 0 && out->width == 0 && out->height == 0, "bounds2D: no contributor is zero");
    }
    {
      // Addition: only the first particleCount particles are bounded.
      auto emitter = emitter2d();
      emitter->data->atlas = atlas({region(0, 0, 0, 10, 10)});
      emitter->data->particle_count = 1;
      emitter->data->ids = u16({0, 0});
      emitter->data->transforms = f32({0, 0, 0, 1, 500, 500, 0, 1});
      auto out = create_rectangle();
      pe::compute_particle_emitter_2d_local_bounds_rectangle(out, emitter);
      check(near(out->width, 10) && near(out->height, 10), "bounds2D: particles past particleCount ignored");
    }
  });
}

void create_particle_emitter_data() {
  guarded("createParticleEmitterData", [] {
    {
      auto data = pe::create_particle_emitter_data();
      check(data->alphas.size() == 0, "createData: alphas empty Float32Array");
      check(!data->atlas.has_value(), "createData: atlas null");
      check(data->ids.size() == 0, "createData: ids empty Uint16Array");
      check(data->particle_count == 0, "createData: count 0");
      check(data->transforms.size() == 0, "createData: transforms empty");
      // Addition: the remaining defaults.
      check(data->colors.size() == 0 && data->positions_z.size() == 0 && data->velocities.size() == 0 && !data->world_space,
            "createData: colors/positionsZ/velocities empty, worldSpace false");
    }
    // A Partial<ParticleEmitterData> object literal: a schema-only row holding just the named members.
    using PartialSchema = flight::RowReadonly<flight::RowPartial<flight::RowOf<Ref<types::ParticleEmitterData>>>>;
    using PartialRow = flight::StructuralRef<PartialSchema>;
    {
      auto partial = flight::make_structural_ref<PartialSchema>(flight::row_field<flight::RowKey<"particleCount">>(5.0));
      auto data = pe::create_particle_emitter_data(std::optional<PartialRow>{partial});
      check(data->particle_count == 5, "createData: pre-defined particleCount");
      // Addition: absent members still take their defaults.
      check(data->alphas.size() == 0 && !data->atlas.has_value() && !data->world_space,
            "createData: absent members default");
    }
    {
      // Addition: a supplied typed array is adopted by reference, not copied (`data?.alphas ?? ...`).
      const auto alphas = f32({0.5f, 0.25f});
      auto partial = flight::make_structural_ref<PartialSchema>(flight::row_field<flight::RowKey<"alphas">>(alphas),
                                                                flight::row_field<flight::RowKey<"worldSpace">>(true));
      auto data = pe::create_particle_emitter_data(std::optional<PartialRow>{partial});
      check(data->alphas == alphas, "createData: supplied alphas keep identity");
      check(data->world_space, "createData: supplied worldSpace");
    }
  });
}

void get_particle_emitter_2d_capacity() {
  guarded("getParticleEmitter2DCapacity", [] {
    check(pe::get_particle_emitter_2d_capacity(emitter2d()) == 0, "capacity2D: new emitter 0");
    {
      auto emitter = emitter2d();
      emitter->data->ids = flight::Uint16Array(10);
      emitter->data->alphas = Float32Array(20);
      emitter->data->transforms = Float32Array(40);
      check(pe::get_particle_emitter_2d_capacity(emitter) == 10, "capacity2D: minimum across arrays");
    }
    {
      // Addition: a partial transform stride floors (43 / 4 -> 10) and the 2D capacity ignores the colors,
      // velocities and positionsZ lanes.
      auto emitter = emitter2d();
      emitter->data->ids = flight::Uint16Array(12);
      emitter->data->alphas = Float32Array(12);
      emitter->data->transforms = Float32Array(43);
      check(pe::get_particle_emitter_2d_capacity(emitter) == 10, "capacity2D: transform stride floors, other lanes ignored");
    }
  });
}

void get_particle_emitter_2d_particle_alpha() {
  guarded("getParticleEmitter2DParticleAlpha", [] {
    {
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, 0, 0, 0, 0, 1);
      pe::set_particle_emitter_2d_particle_alpha(emitter, 0, 0.5);
      check(close(pe::get_particle_emitter_2d_particle_alpha(emitter, 0), 0.5), "getAlpha2D: valid index");
    }
    {
      auto emitter = emitter2d();
      check(pe::get_particle_emitter_2d_particle_alpha(emitter, 0) == -1, "getAlpha2D: index 0 on empty");
      check(pe::get_particle_emitter_2d_particle_alpha(emitter, -1) == -1, "getAlpha2D: index -1");
    }
    {
      // Addition: bounded by particleCount, not capacity.
      auto emitter = emitter2d();
      pe::reserve_particle_emitter_2d(emitter, 4);
      pe::append_particle_emitter_2d_particle(emitter, 0, 0, 0, 0, 1);
      check(pe::get_particle_emitter_2d_particle_alpha(emitter, 1) == -1, "getAlpha2D: index == count is out of range");
    }
  });
}

void get_particle_emitter_2d_particle_id() {
  guarded("getParticleEmitter2DParticleId", [] {
    auto emitter = emitter2d();
    pe::append_particle_emitter_2d_particle(emitter, 7, 0, 0, 0, 1);
    check(pe::get_particle_emitter_2d_particle_id(emitter, 0) == 7, "getId2D: valid index");
    auto empty = emitter2d();
    check(pe::get_particle_emitter_2d_particle_id(empty, 0) == -1, "getId2D: index 0 on empty");
    check(pe::get_particle_emitter_2d_particle_id(empty, -1) == -1, "getId2D: index -1");
    // Addition: index == particleCount with capacity behind it is still out of range.
    check(pe::get_particle_emitter_2d_particle_id(emitter, 1) == -1, "getId2D: index == count");
  });
}

void get_particle_emitter_2d_particle_velocity() {
  guarded("getParticleEmitter2DParticleVelocity", [] {
    {
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, 0, 0, 0, 0, 1);
      pe::set_particle_emitter_2d_particle_velocity(emitter, 0, 3.5, -1.5);
      auto out = vec2(0, 0);
      const bool result = pe::get_particle_emitter_2d_particle_velocity(out, emitter, 0);
      check(result, "getVelocity2D: returns true");
      check(close(out->x, 3.5) && close(out->y, -1.5), "getVelocity2D: writes vx, vy");
    }
    {
      auto emitter = emitter2d();
      auto out = vec2(99, 99);
      check(!pe::get_particle_emitter_2d_particle_velocity(out, emitter, 0), "getVelocity2D: out of range returns false");
      check(out->x == 99 && out->y == 99, "getVelocity2D: out of range writes nothing");
    }
    {
      // Addition: the stride is 2 — particle 1's velocity is read from lanes 2 and 3.
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, 0, 0, 0, 0, 1);
      pe::append_particle_emitter_2d_particle(emitter, 0, 0, 0, 0, 1);
      pe::set_particle_emitter_2d_particle_velocity(emitter, 1, 6, 7);
      auto out = vec2(0, 0);
      pe::get_particle_emitter_2d_particle_velocity(out, emitter, 1);
      check(out->x == 6 && out->y == 7 && emitter->data->velocities.get_index(2) == 6 &&
                emitter->data->velocities.get_index(3) == 7 && emitter->data->velocities.get_index(0) == 0,
            "getVelocity2D: stride-2 layout");
    }
  });
}

void initialize_particle_emitter_data() {
  guarded("initializeParticleEmitterData", [] {
    auto fn = &pe::initialize_particle_emitter_data;
    check(fn != nullptr, "initializeData: is a function");
  });
}

void remove_particle_emitter_2d_particle() {
  guarded("removeParticleEmitter2DParticle", [] {
    {
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, 0, 0, 0, 0, 1);
      pe::append_particle_emitter_2d_particle(emitter, 1, 10, 10, 0, 1);
      pe::append_particle_emitter_2d_particle(emitter, 2, 20, 20, 0, 1);
      pe::set_particle_emitter_2d_particle_alpha(emitter, 2, 0.5);
      pe::set_particle_emitter_2d_particle_color(emitter, 2, 0.25, 0.5, 0.75);
      pe::set_particle_emitter_2d_particle_velocity(emitter, 2, 3, 4);
      emitter->data->positions_z.set_index(2, 8);
      pe::remove_particle_emitter_2d_particle(emitter, 0);
      check(emitter->data->particle_count == 2, "remove2D: count 2");
      check(pe::get_particle_emitter_2d_particle_id(emitter, 0) == 2, "remove2D: last swapped into index 0");
      // Addition: every lane of the last particle moved into slot 0, and slot 1 is untouched.
      const auto& d = emitter->data;
      check(t(emitter, 0) == 20 && t(emitter, 1) == 20 && t(emitter, 2) == 0 && t(emitter, 3) == 1, "remove2D: transform swapped");
      check(d->alphas.get_index(0) == 0.5, "remove2D: alpha swapped");
      check(d->colors.get_index(0) == 0.25 && d->colors.get_index(1) == 0.5 && d->colors.get_index(2) == 0.75,
            "remove2D: color swapped");
      check(d->velocities.get_index(0) == 3 && d->velocities.get_index(1) == 4, "remove2D: velocity swapped");
      check(d->positions_z.get_index(0) == 8, "remove2D: positionsZ swapped");
      check(pe::get_particle_emitter_2d_particle_id(emitter, 1) == 1 && t(emitter, 4) == 10, "remove2D: slot 1 untouched");
      // Addition: the vacated tail slot is not cleared, only excluded by the count.
      check(d->ids.get_index(2) == 2 && t(emitter, 8) == 20, "remove2D: tail storage retained");
    }
    {
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, 5, 0, 0, 0, 1);
      pe::remove_particle_emitter_2d_particle(emitter, 0);
      check(emitter->data->particle_count == 0, "remove2D: only particle");
    }
    {
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, 0, 0, 0, 0, 1);
      pe::remove_particle_emitter_2d_particle(emitter, -1);
      pe::remove_particle_emitter_2d_particle(emitter, 1);
      check(emitter->data->particle_count == 1, "remove2D: out of range no-op");
    }
    {
      // Addition: removing the last index decrements without moving anything.
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, 1, 1, 1, 0, 1);
      pe::append_particle_emitter_2d_particle(emitter, 2, 2, 2, 0, 1);
      pe::remove_particle_emitter_2d_particle(emitter, 1);
      check(emitter->data->particle_count == 1 && pe::get_particle_emitter_2d_particle_id(emitter, 0) == 1 && t(emitter, 0) == 1,
            "remove2D: removing last keeps first in place");
      // Addition: removing from an empty emitter cannot drive the count negative.
      auto empty = emitter2d();
      pe::remove_particle_emitter_2d_particle(empty, 0);
      check(empty->data->particle_count == 0, "remove2D: empty stays 0");
    }
  });
}

void reserve_particle_emitter_2d() {
  guarded("reserveParticleEmitter2D", [] {
    {
      auto emitter = emitter2d();
      pe::reserve_particle_emitter_2d(emitter, 50);
      check(emitter->data->ids.size() == 50, "reserve2D: ids 50");
      check(emitter->data->alphas.size() == 50, "reserve2D: alphas 50");
      check(emitter->data->transforms.size() == 200, "reserve2D: transforms 200");
      // Addition: the other lanes at their strides.
      check(emitter->data->colors.size() == 150 && emitter->data->velocities.size() == 100 &&
                emitter->data->positions_z.size() == 50,
            "reserve2D: colors 150, velocities 100, positionsZ 50");
    }
    {
      auto emitter = emitter2d();
      pe::reserve_particle_emitter_2d(emitter, 50);
      const auto ids = emitter->data->ids;
      const auto alphas = emitter->data->alphas;
      const auto transforms = emitter->data->transforms;
      pe::reserve_particle_emitter_2d(emitter, 50);
      check(emitter->data->ids == ids && emitter->data->alphas == alphas && emitter->data->transforms == transforms,
            "reserve2D: no reallocation when sufficient");
      // Addition: a smaller request never shrinks.
      pe::reserve_particle_emitter_2d(emitter, 10);
      check(emitter->data->ids == ids && pe::get_particle_emitter_2d_capacity(emitter) == 50, "reserve2D: smaller request no-op");
    }
    {
      // Addition: growth preserves contents.
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, 9, 1, 2, 3, 4);
      pe::set_particle_emitter_2d_particle_color(emitter, 0, 0.5, 0.25, 0.125);
      pe::reserve_particle_emitter_2d(emitter, 100);
      check(pe::get_particle_emitter_2d_capacity(emitter) == 100 && pe::get_particle_emitter_2d_particle_id(emitter, 0) == 9 &&
                t(emitter, 3) == 4 && emitter->data->colors.get_index(2) == 0.125,
            "reserve2D: contents preserved across growth");
    }
    {
      // Addition: zero capacity on an empty emitter is satisfied; nothing is allocated.
      auto emitter = emitter2d();
      const auto ids = emitter->data->ids;
      pe::reserve_particle_emitter_2d(emitter, 0);
      check(emitter->data->ids == ids && emitter->data->ids.size() == 0, "reserve2D: zero capacity no-op");
    }
  });
}

void set_particle_emitter_2d_particle() {
  guarded("setParticleEmitter2DParticle", [] {
    {
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, 0, 0, 0, 0, 1);
      pe::set_particle_emitter_2d_particle(emitter, 0, 3, 5, 10, 1.57, 2.0);
      check(emitter->data->ids.get_index(0) == 3, "set2D: id");
      check(close(t(emitter, 0), 5) && close(t(emitter, 1), 10) && close(t(emitter, 2), 1.57) && close(t(emitter, 3), 2.0),
            "set2D: transform");
      // Addition: alpha, color and velocity are not touched.
      check(emitter->data->alphas.get_index(0) == 1 && emitter->data->colors.get_index(0) == 1 &&
                emitter->data->velocities.get_index(0) == 0,
            "set2D: other lanes untouched");
    }
    {
      auto emitter = emitter2d();
      pe::append_particle_emitter_2d_particle(emitter, 0, 0, 0, 0, 1);
      emitter->data->ids.set_index(0, 0);
      pe::set_particle_emitter_2d_particle(emitter, -1, 9, 0, 0, 0, 1);
      pe::set_particle_emitter_2d_particle(emitter, 1, 9, 0, 0, 0, 1);
      check(emitter->data->ids.get_index(0) == 0, "set2D: out of range no-op");
      // Addition: the reserved-but-unused slot 1 was not written either.
      check(emitter->data->ids.get_index(1) == 0, "set2D: slot past count not written");
    }
  });
}

void set_particle_emitter_2d_particle_alpha() {
  guarded("setParticleEmitter2DParticleAlpha", [] {
    auto emitter = emitter2d();
    pe::append_particle_emitter_2d_particle(emitter, 0, 0, 0, 0, 1);
    pe::set_particle_emitter_2d_particle_alpha(emitter, 0, 0.25);
    check(close(emitter->data->alphas.get_index(0), 0.25), "setAlpha2D: valid");
    emitter->data->alphas.set_index(0, 0.5);
    pe::set_particle_emitter_2d_particle_alpha(emitter, -1, 0.9);
    pe::set_particle_emitter_2d_particle_alpha(emitter, 1, 0.9);
    check(close(emitter->data->alphas.get_index(0), 0.5), "setAlpha2D: out of range no-op");
    // Addition: slot 1 (in capacity, past count) is not written.
    check(emitter->data->alphas.get_index(1) == 0, "setAlpha2D: slot past count not written");
  });
}

void set_particle_emitter_2d_particle_color() {
  guarded("setParticleEmitter2DParticleColor", [] {
    auto emitter = emitter2d();
    pe::append_particle_emitter_2d_particle(emitter, 0, 0, 0, 0, 1);
    pe::set_particle_emitter_2d_particle_color(emitter, 0, 0.2, 0.4, 0.8);
    check(close(emitter->data->colors.get_index(0), 0.2) && close(emitter->data->colors.get_index(1), 0.4) &&
              close(emitter->data->colors.get_index(2), 0.8),
          "setColor2D: valid");
    emitter->data->colors.set_index(0, 0.5);
    pe::set_particle_emitter_2d_particle_color(emitter, -1, 0.9, 0.9, 0.9);
    pe::set_particle_emitter_2d_particle_color(emitter, 1, 0.9, 0.9, 0.9);
    check(close(emitter->data->colors.get_index(0), 0.5), "setColor2D: out of range no-op");
    // Addition: stride 3 — particle 1's color lanes 3..5 untouched.
    check(emitter->data->colors.get_index(3) == 0 && emitter->data->colors.get_index(5) == 0, "setColor2D: slot past count not written");
  });
}

void set_particle_emitter_2d_particle_velocity() {
  guarded("setParticleEmitter2DParticleVelocity", [] {
    auto emitter = emitter2d();
    pe::append_particle_emitter_2d_particle(emitter, 0, 0, 0, 0, 1);
    pe::set_particle_emitter_2d_particle_velocity(emitter, 0, 2.5, -3.0);
    check(close(emitter->data->velocities.get_index(0), 2.5) && close(emitter->data->velocities.get_index(1), -3.0),
          "setVelocity2D: valid");
    emitter->data->velocities.set_index(0, 1);
    pe::set_particle_emitter_2d_particle_velocity(emitter, -1, 9, 9);
    pe::set_particle_emitter_2d_particle_velocity(emitter, 1, 9, 9);
    check(close(emitter->data->velocities.get_index(0), 1), "setVelocity2D: out of range no-op");
    // Addition: lanes 2..3 (particle 1) not written.
    check(emitter->data->velocities.get_index(2) == 0 && emitter->data->velocities.get_index(3) == 0,
          "setVelocity2D: slot past count not written");
  });
}

// =============================================================================================================
// particleEmitter3D.test.ts
// =============================================================================================================

void append_particle_emitter_3d_particle() {
  guarded("appendParticleEmitter3DParticle", [] {
    {
      auto emitter = emitter3d();
      const double index = pe::append_particle_emitter_3d_particle(emitter, 5, 10, 20, 30, 0.5, 2);
      check(index == 0 && emitter->data->particle_count == 1, "append3D: index 0, count 1");
      check(emitter->data->ids.get_index(0) == 5, "append3D: id");
      check(t(emitter, 0) == 10 && t(emitter, 1) == 20 && t(emitter, 2) == 0.5 && t(emitter, 3) == 2, "append3D: transform");
      check(emitter->data->positions_z.get_index(0) == 30, "append3D: z");
      check(emitter->data->alphas.get_index(0) == 1, "append3D: alpha 1");
      // Addition: white color and zero 3-lane velocity.
      check(emitter->data->colors.get_index(0) == 1 && emitter->data->colors.get_index(1) == 1 &&
                emitter->data->colors.get_index(2) == 1,
            "append3D: color white");
      check(emitter->data->velocities.get_index(0) == 0 && emitter->data->velocities.get_index(1) == 0 &&
                emitter->data->velocities.get_index(2) == 0,
            "append3D: velocity zero");
    }
    {
      auto emitter = emitter3d();
      for (int i = 0; i < 20; ++i) pe::append_particle_emitter_3d_particle(emitter, i, i, i, i, 0, 1);
      check(emitter->data->particle_count == 20, "append3D: auto-grow count");
      check(pe::get_particle_emitter_3d_capacity(emitter) >= 20, "append3D: auto-grow capacity");
      // Addition: 8 -> 16 -> 32.
      check(pe::get_particle_emitter_3d_capacity(emitter) == 32, "append3D: capacity is exactly 32 after 20 appends");
      check(emitter->data->velocities.size() == 96 && emitter->data->colors.size() == 96 && emitter->data->transforms.size() == 128,
            "append3D: 3-lane velocities, colors, transforms sized for 32");
      bool ok = true;
      for (int i = 0; i < 20; ++i) ok = ok && emitter->data->positions_z.get_index(i) == i && emitter->data->ids.get_index(i) == i;
      check(ok, "append3D: z and ids preserved across growth");
    }
  });
}

void clear_particle_emitter_3d() {
  guarded("clearParticleEmitter3D", [] {
    auto emitter = emitter3d();
    pe::append_particle_emitter_3d_particle(emitter, 1, 0, 0, 0, 0, 1);
    pe::append_particle_emitter_3d_particle(emitter, 2, 0, 0, 0, 0, 1);
    pe::clear_particle_emitter_3d(emitter);
    check(emitter->data->particle_count == 0, "clear3D: count 0");
    // Addition: capacity kept.
    check(pe::get_particle_emitter_3d_capacity(emitter) == 8, "clear3D: capacity kept");
  });
}

void compact_particle_emitter_3d() {
  guarded("compactParticleEmitter3D", [] {
    {
      auto emitter = emitter3d();
      pe::append_particle_emitter_3d_particle(emitter, 1, 10, 0, 0, 0, 1);
      pe::append_particle_emitter_3d_particle(emitter, 2, 20, 0, 0, 0, 1);
      pe::append_particle_emitter_3d_particle(emitter, 3, 30, 0, 0, 0, 1);
      emitter->data->ids.set_index(1, pe::particle_emitter_3_d_deleted_id);
      pe::compact_particle_emitter_3d(emitter);
      check(emitter->data->particle_count == 2, "compact3D: count 2");
      check(emitter->data->ids.get_index(0) == 1 && emitter->data->ids.get_index(1) == 3, "compact3D: ids");
      check(t(emitter, 0) == 10 && t(emitter, 4) == 30, "compact3D: transforms");
    }
    {
      auto emitter = emitter3d();
      pe::append_particle_emitter_3d_particle(emitter, 1, 0, 0, 0, 0, 1);
      pe::append_particle_emitter_3d_particle(emitter, 2, 0, 0, 0, 0, 1);
      pe::set_particle_emitter_3d_particle_velocity(emitter, 1, 4, 5, 6);
      emitter->data->ids.set_index(0, pe::particle_emitter_3_d_deleted_id);
      pe::compact_particle_emitter_3d(emitter);
      auto out = vec3(0, 0, 0);
      pe::get_particle_emitter_3d_particle_velocity(out, emitter, 0);
      check(close(out->x, 4) && close(out->y, 5) && close(out->z, 6), "compact3D: Z velocity lane carried");
    }
    {
      auto emitter = emitter3d();
      pe::compact_particle_emitter_3d(emitter);
      check(emitter->data->particle_count == 0, "compact3D: empty no-op");
    }
    {
      // Addition: positionsZ, alpha and color slide with the survivor.
      auto emitter = emitter3d();
      pe::append_particle_emitter_3d_particle(emitter, 0xffff, 0, 0, 1, 0, 1);
      pe::append_particle_emitter_3d_particle(emitter, 7, 2, 3, 4, 0.5, 6);
      pe::set_particle_emitter_3d_particle_alpha(emitter, 1, 0.25);
      pe::set_particle_emitter_3d_particle_color(emitter, 1, 0.5, 0.25, 0.125);
      pe::compact_particle_emitter_3d(emitter);
      const auto& d = emitter->data;
      check(d->particle_count == 1 && d->ids.get_index(0) == 7, "compact3D: survivor at 0");
      check(t(emitter, 0) == 2 && t(emitter, 1) == 3 && t(emitter, 2) == 0.5 && t(emitter, 3) == 6 && d->positions_z.get_index(0) == 4,
            "compact3D: transform and z slid");
      check(d->alphas.get_index(0) == 0.25 && d->colors.get_index(0) == 0.5 && d->colors.get_index(1) == 0.25 &&
                d->colors.get_index(2) == 0.125,
            "compact3D: alpha and color slid");
    }
  });
}

void compute_particle_emitter_3d_local_bounds_aabb() {
  guarded("computeParticleEmitter3DLocalBoundsAabb", [] {
    {
      auto emitter = emitter3d();
      auto out = aabb();
      pe::compute_particle_emitter_3d_local_bounds_aabb(out, emitter);
      check(out->min->x == 0 && out->max->z == 0, "bounds3D: empty is zero box");
    }
    {
      auto emitter = emitter3d();
      pe::append_particle_emitter_3d_particle(emitter, 1, 0, 0, 0, 0, 2);
      pe::append_particle_emitter_3d_particle(emitter, 2, 10, -4, 6, 0, 2);
      auto out = aabb();
      pe::compute_particle_emitter_3d_local_bounds_aabb(out, emitter);
      const double r = std::sqrt(0.5) * 2;
      check(close(out->min->x, -r) && close(out->min->y, -4 - r) && close(out->min->z, -r), "bounds3D: min");
      check(close(out->max->x, 10 + r) && close(out->max->y, r) && close(out->max->z, 6 + r), "bounds3D: max");
      // Addition: exact to float precision, not just two digits.
      check(near(out->min->x, -r) && near(out->max->z, 6 + r), "bounds3D: precise radius SQRT1_2 * scale");
    }
    {
      // Addition: an empty emitter overwrites all six components of a prefilled box.
      auto emitter = emitter3d();
      auto out = aabb();
      out->min = vec3(-1, -2, -3);
      out->max = vec3(4, 5, 6);
      pe::compute_particle_emitter_3d_local_bounds_aabb(out, emitter);
      check(out->min->x == 0 && out->min->y == 0 && out->min->z == 0 && out->max->x == 0 && out->max->y == 0 && out->max->z == 0,
            "bounds3D: empty zeros all six");
    }
    {
      // Addition: a negative scale bounds by its magnitude; ids and rotation are irrelevant.
      auto emitter = emitter3d();
      pe::append_particle_emitter_3d_particle(emitter, 0xffff, 1, 2, 3, 1.25, -4);
      auto out = aabb();
      pe::compute_particle_emitter_3d_local_bounds_aabb(out, emitter);
      const double r = std::sqrt(0.5) * 4;
      check(near(out->min->x, 1 - r) && near(out->min->y, 2 - r) && near(out->min->z, 3 - r) && near(out->max->x, 1 + r) &&
                near(out->max->y, 2 + r) && near(out->max->z, 3 + r),
            "bounds3D: negative scale uses |scale|");
    }
  });
}

void get_particle_emitter_3d_capacity() {
  guarded("getParticleEmitter3DCapacity", [] {
    check(pe::get_particle_emitter_3d_capacity(emitter3d()) == 0, "capacity3D: fresh 0");
    {
      auto emitter = emitter3d();
      pe::reserve_particle_emitter_3d(emitter, 64);
      check(pe::get_particle_emitter_3d_capacity(emitter) == 64, "capacity3D: after reserve 64");
    }
    {
      auto emitter = emitter3d();
      pe::reserve_particle_emitter_3d(emitter, 8);
      emitter->data->positions_z = Float32Array(3);
      emitter->data->colors = Float32Array(6);
      check(pe::get_particle_emitter_3d_capacity(emitter) == 2, "capacity3D: bounded by every lane");
    }
    {
      // Addition: the velocity lane at stride 3 floors (23 / 3 -> 7).
      auto emitter = emitter3d();
      pe::reserve_particle_emitter_3d(emitter, 8);
      emitter->data->velocities = Float32Array(23);
      check(pe::get_particle_emitter_3d_capacity(emitter) == 7, "capacity3D: velocity stride 3 floors");
    }
  });
}

void get_particle_emitter_3d_particle_alpha() {
  guarded("getParticleEmitter3DParticleAlpha", [] {
    auto emitter = emitter3d();
    pe::append_particle_emitter_3d_particle(emitter, 1, 0, 0, 0, 0, 1);
    pe::set_particle_emitter_3d_particle_alpha(emitter, 0, 0.5);
    check(pe::get_particle_emitter_3d_particle_alpha(emitter, 0) == 0.5, "getAlpha3D: valid");
    check(pe::get_particle_emitter_3d_particle_alpha(emitter3d(), 0) == -1, "getAlpha3D: out of range");
    // Addition: negative index.
    check(pe::get_particle_emitter_3d_particle_alpha(emitter, -1) == -1, "getAlpha3D: negative index");
  });
}

void get_particle_emitter_3d_particle_id() {
  guarded("getParticleEmitter3DParticleId", [] {
    auto emitter = emitter3d();
    pe::append_particle_emitter_3d_particle(emitter, 42, 0, 0, 0, 0, 1);
    check(pe::get_particle_emitter_3d_particle_id(emitter, 0) == 42, "getId3D: valid");
    check(pe::get_particle_emitter_3d_particle_id(emitter3d(), 5) == -1, "getId3D: out of range");
    // Addition: index == count, and negative.
    check(pe::get_particle_emitter_3d_particle_id(emitter, 1) == -1 && pe::get_particle_emitter_3d_particle_id(emitter, -1) == -1,
          "getId3D: count and negative bounds");
  });
}

void get_particle_emitter_3d_particle_velocity() {
  guarded("getParticleEmitter3DParticleVelocity", [] {
    {
      auto emitter = emitter3d();
      pe::append_particle_emitter_3d_particle(emitter, 1, 0, 0, 0, 0, 1);
      pe::set_particle_emitter_3d_particle_velocity(emitter, 0, 3.5, -2.1, 7.7);
      auto out = vec3(0, 0, 0);
      check(pe::get_particle_emitter_3d_particle_velocity(out, emitter, 0), "getVelocity3D: true");
      check(close(out->x, 3.5) && close(out->y, -2.1) && close(out->z, 7.7), "getVelocity3D: full 3D velocity");
    }
    {
      auto out = vec3(0, 0, 0);
      check(!pe::get_particle_emitter_3d_particle_velocity(out, emitter3d(), 0), "getVelocity3D: out of range false");
      // Addition: nothing written.
      auto sentinel = vec3(9, 9, 9);
      pe::get_particle_emitter_3d_particle_velocity(sentinel, emitter3d(), 0);
      check(sentinel->x == 9 && sentinel->y == 9 && sentinel->z == 9, "getVelocity3D: out of range writes nothing");
    }
  });
}

void get_particle_emitter_3d_runtime() {
  guarded("getParticleEmitter3DRuntime", [] {
    auto emitter = emitter3d();
    auto runtime = pe::get_particle_emitter_3d_runtime(emitter);
    (void)runtime;
    check(true, "getRuntime3D: returns the runtime");
  });
}

void is_particle_emitter_3d() {
  guarded("isParticleEmitter3D", [] {
    // TypeScript widens the emitter to `{ kind: string }` implicitly; the runtime offers no implicit conversion from
    // the nominal ParticleEmitter3D to that anonymous row, so the row view is taken over the same owner explicitly.
    using KindRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<Ref<types::kind_6fea82d7aa842443>>>>;
    check(pe::is_particle_emitter_3d(flight::structural_ref_cast<KindRow>(emitter3d())), "is3D: true for a 3D emitter");
    auto mesh = flight::make_ref<types::kind_6fea82d7aa842443>();
    mesh->kind = String("Mesh");
    check(!pe::is_particle_emitter_3d(mesh), "is3D: false for other kinds");
    // Addition: the 2D kind is not the 3D kind.
    auto other = flight::make_ref<types::kind_6fea82d7aa842443>();
    other->kind = types::particle_emitter_2d_kind;
    check(!pe::is_particle_emitter_3d(other), "is3D: false for ParticleEmitter2D kind");
  });
}

void remove_particle_emitter_3d_particle() {
  guarded("removeParticleEmitter3DParticle", [] {
    {
      auto emitter = emitter3d();
      pe::append_particle_emitter_3d_particle(emitter, 1, 10, 0, 5, 0, 1);
      pe::append_particle_emitter_3d_particle(emitter, 2, 20, 0, 15, 0, 1);
      pe::append_particle_emitter_3d_particle(emitter, 3, 30, 0, 25, 0, 1);
      pe::set_particle_emitter_3d_particle_velocity(emitter, 2, 7, 8, 9);
      pe::remove_particle_emitter_3d_particle(emitter, 0);
      check(emitter->data->particle_count == 2, "remove3D: count");
      check(emitter->data->ids.get_index(0) == 3 && t(emitter, 0) == 30 && emitter->data->positions_z.get_index(0) == 25,
            "remove3D: last swapped in");
      auto out = vec3(0, 0, 0);
      pe::get_particle_emitter_3d_particle_velocity(out, emitter, 0);
      check(close(out->z, 9), "remove3D: Z velocity carried");
      // Addition: the x/y velocity lanes too, and the middle particle untouched.
      check(out->x == 7 && out->y == 8, "remove3D: x/y velocity carried");
      check(emitter->data->ids.get_index(1) == 2 && emitter->data->positions_z.get_index(1) == 15, "remove3D: slot 1 untouched");
    }
    {
      auto emitter = emitter3d();
      pe::append_particle_emitter_3d_particle(emitter, 1, 0, 0, 0, 0, 1);
      pe::remove_particle_emitter_3d_particle(emitter, 5);
      check(emitter->data->particle_count == 1, "remove3D: out of range no-op");
      // Addition: negative index no-op too.
      pe::remove_particle_emitter_3d_particle(emitter, -1);
      check(emitter->data->particle_count == 1, "remove3D: negative no-op");
    }
  });
}

void reserve_particle_emitter_3d() {
  guarded("reserveParticleEmitter3D", [] {
    {
      auto emitter = emitter3d();
      pe::reserve_particle_emitter_3d(emitter, 100);
      const auto& d = emitter->data;
      check(d->ids.size() >= 100 && d->alphas.size() >= 100 && d->transforms.size() >= 400 && d->positions_z.size() >= 100 &&
                d->velocities.size() >= 300 && d->colors.size() >= 300,
            "reserve3D: grows all buffers");
      // Addition: exactly to those sizes.
      check(d->ids.size() == 100 && d->alphas.size() == 100 && d->transforms.size() == 400 && d->positions_z.size() == 100 &&
                d->velocities.size() == 300 && d->colors.size() == 300,
            "reserve3D: exact sizes");
    }
    {
      auto emitter = emitter3d();
      pe::reserve_particle_emitter_3d(emitter, 100);
      pe::reserve_particle_emitter_3d(emitter, 50);
      check(pe::get_particle_emitter_3d_capacity(emitter) == 100, "reserve3D: does not shrink");
    }
    {
      auto emitter = emitter3d();
      pe::reserve_particle_emitter_3d(emitter, 8);
      const auto ids = emitter->data->ids;
      emitter->data->positions_z = Float32Array(0);
      emitter->data->velocities = Float32Array(0);
      pe::reserve_particle_emitter_3d(emitter, 8);
      check(emitter->data->positions_z.size() >= 8, "reserve3D: repairs positionsZ");
      check(emitter->data->velocities.size() >= 24, "reserve3D: repairs velocities");
      // Addition: lanes that were already long enough keep their identity.
      check(emitter->data->ids == ids, "reserve3D: sufficient lanes are not reallocated");
    }
  });
}

void set_particle_emitter_3d_particle() {
  guarded("setParticleEmitter3DParticle", [] {
    {
      auto emitter = emitter3d();
      pe::append_particle_emitter_3d_particle(emitter, 0, 0, 0, 0, 0, 1);
      pe::set_particle_emitter_3d_particle(emitter, 0, 7, 100, 200, 300, 1.5, 3);
      check(emitter->data->ids.get_index(0) == 7 && t(emitter, 0) == 100 && t(emitter, 1) == 200 && t(emitter, 2) == 1.5 &&
                t(emitter, 3) == 3 && emitter->data->positions_z.get_index(0) == 300,
            "set3D: all transform fields including z");
    }
    {
      auto emitter = emitter3d();
      pe::set_particle_emitter_3d_particle(emitter, 0, 1, 0, 0, 0, 0, 1);
      check(emitter->data->particle_count == 0, "set3D: out of range no-op");
      // Addition: no storage was allocated by the rejected write.
      check(emitter->data->ids.size() == 0, "set3D: nothing allocated");
    }
  });
}

void set_particle_emitter_3d_particle_alpha() {
  guarded("setParticleEmitter3DParticleAlpha", [] {
    auto emitter = emitter3d();
    pe::append_particle_emitter_3d_particle(emitter, 1, 0, 0, 0, 0, 1);
    pe::set_particle_emitter_3d_particle_alpha(emitter, 0, 0.75);
    check(emitter->data->alphas.get_index(0) == 0.75, "setAlpha3D: valid");
    auto empty = emitter3d();
    pe::set_particle_emitter_3d_particle_alpha(empty, 0, 0.5);
    check(empty->data->particle_count == 0, "setAlpha3D: out of range no-op");
  });
}

void set_particle_emitter_3d_particle_color() {
  guarded("setParticleEmitter3DParticleColor", [] {
    auto emitter = emitter3d();
    pe::append_particle_emitter_3d_particle(emitter, 1, 0, 0, 0, 0, 1);
    pe::set_particle_emitter_3d_particle_color(emitter, 0, 0.2, 0.4, 0.8);
    check(close(emitter->data->colors.get_index(0), 0.2) && close(emitter->data->colors.get_index(1), 0.4) &&
              close(emitter->data->colors.get_index(2), 0.8),
          "setColor3D: valid");
    // Addition: out of range (index == count) leaves stride-3 lanes 3..5 untouched.
    pe::set_particle_emitter_3d_particle_color(emitter, 1, 0.9, 0.9, 0.9);
    check(emitter->data->colors.get_index(3) == 0 && emitter->data->colors.get_index(4) == 0 && emitter->data->colors.get_index(5) == 0,
          "setColor3D: out of range no-op");
  });
}

void set_particle_emitter_3d_particle_velocity() {
  guarded("setParticleEmitter3DParticleVelocity", [] {
    auto emitter = emitter3d();
    pe::append_particle_emitter_3d_particle(emitter, 1, 0, 0, 0, 0, 1);
    pe::set_particle_emitter_3d_particle_velocity(emitter, 0, 5.5, -3.2, 9.1);
    check(close(emitter->data->velocities.get_index(0), 5.5) && close(emitter->data->velocities.get_index(1), -3.2) &&
              close(emitter->data->velocities.get_index(2), 9.1),
          "setVelocity3D: valid");
    auto empty = emitter3d();
    pe::set_particle_emitter_3d_particle_velocity(empty, 0, 1, 2, 3);
    check(empty->data->particle_count == 0, "setVelocity3D: out of range no-op");
    // Addition: particle 1 is at lanes 3..5 (stride 3).
    pe::append_particle_emitter_3d_particle(emitter, 2, 0, 0, 0, 0, 1);
    pe::set_particle_emitter_3d_particle_velocity(emitter, 1, 1, 2, 3);
    check(emitter->data->velocities.get_index(3) == 1 && emitter->data->velocities.get_index(4) == 2 &&
              emitter->data->velocities.get_index(5) == 3 && near(emitter->data->velocities.get_index(2), 9.1),
          "setVelocity3D: stride-3 layout");
  });
}

void sort_particle_emitter_3d_indices_by_view_depth() {
  guarded("sortParticleEmitter3DIndicesByViewDepth", [] {
    using flight::Float64Array;
    using flight::Uint32Array;
    {
      auto emitter = emitter3d();
      pe::append_particle_emitter_3d_particle(emitter, 10, 0, 0, -2, 0, 1);
      pe::append_particle_emitter_3d_particle(emitter, 11, 0, 0, -10, 0, 1);
      pe::append_particle_emitter_3d_particle(emitter, 12, 0, 0, -5, 0, 1);
      pe::append_particle_emitter_3d_particle(emitter, 13, 0, 0, -5, 0, 1);
      Uint32Array indices(4);
      Float64Array depths(4);
      check(pe::sort_particle_emitter_3d_indices_by_view_depth(indices, depths, emitter, flight::geometry::create_matrix4()),
            "sort3D: returns true");
      check(indices.get_index(0) == 1 && indices.get_index(1) == 2 && indices.get_index(2) == 3 && indices.get_index(3) == 0,
            "sort3D: stable back-to-front [1,2,3,0]");
      check(emitter->data->ids.get_index(0) == 10 && emitter->data->ids.get_index(1) == 11 && emitter->data->ids.get_index(2) == 12 &&
                emitter->data->ids.get_index(3) == 13,
            "sort3D: storage not reordered");
      // Addition: depths keep each source particle's view Z at its source index.
      check(depths.get_index(0) == -2 && depths.get_index(1) == -10 && depths.get_index(2) == -5 && depths.get_index(3) == -5,
            "sort3D: depths at source indices");
    }
    {
      auto emitter = emitter3d();
      pe::append_particle_emitter_3d_particle(emitter, 10, 2, 0, 0, 0, 1);
      pe::append_particle_emitter_3d_particle(emitter, 11, -3, 0, 0, 0, 1);
      auto m = flight::geometry::create_matrix4();
      m->m.set_index(2, 1);
      m->m.set_index(10, 0);
      Uint32Array indices(2);
      pe::sort_particle_emitter_3d_indices_by_view_depth(indices, Float64Array(2), emitter, m);
      check(indices.get_index(0) == 1 && indices.get_index(1) == 0, "sort3D: caller matrix applied");
    }
    {
      auto emitter = emitter3d();
      pe::append_particle_emitter_3d_particle(emitter, 10, 0, 0, -2, 0, 1);
      pe::append_particle_emitter_3d_particle(emitter, 11, 0, 0, -10, 0, 1);
      Uint32Array indices({99u});
      Float64Array depths({88.0, 77.0});
      check(!pe::sort_particle_emitter_3d_indices_by_view_depth(indices, depths, emitter, flight::geometry::create_matrix4()),
            "sort3D: too-small scratch returns false");
      check(indices.size() == 1 && indices.get_index(0) == 99, "sort3D: indices untouched");
      check(depths.get_index(0) == 88 && depths.get_index(1) == 77, "sort3D: depths untouched");
    }
    {
      auto emitter = emitter3d();
      pe::append_particle_emitter_3d_particle(emitter, 10, 0, 0, -2, 0, 1);
      emitter->data->positions_z = Float32Array(0);
      Uint32Array indices({99u});
      Float64Array depths({88.0});
      check(!pe::sort_particle_emitter_3d_indices_by_view_depth(indices, depths, emitter, flight::geometry::create_matrix4()),
            "sort3D: incomplete storage returns false");
      check(indices.get_index(0) == 99 && depths.get_index(0) == 88, "sort3D: outputs untouched on incomplete storage");
    }
    {
      // Addition: the translation column m[14] offsets every depth; ordering is by full view Z.
      auto emitter = emitter3d();
      pe::append_particle_emitter_3d_particle(emitter, 0, 0, 0, 1, 0, 1);
      pe::append_particle_emitter_3d_particle(emitter, 1, 0, 0, 3, 0, 1);
      pe::append_particle_emitter_3d_particle(emitter, 2, 0, 0, 2, 0, 1);
      auto m = flight::geometry::create_matrix4();
      m->m.set_index(14, -10);
      Uint32Array indices(3);
      Float64Array depths(3);
      pe::sort_particle_emitter_3d_indices_by_view_depth(indices, depths, emitter, m);
      check(depths.get_index(0) == -9 && depths.get_index(1) == -7 && depths.get_index(2) == -8, "sort3D: m[14] translation");
      check(indices.get_index(0) == 0 && indices.get_index(1) == 2 && indices.get_index(2) == 1, "sort3D: ascending view Z");
    }
    {
      // Addition: an empty emitter succeeds and writes nothing.
      Uint32Array indices({5u});
      Float64Array depths({6.0});
      check(pe::sort_particle_emitter_3d_indices_by_view_depth(indices, depths, emitter3d(), flight::geometry::create_matrix4()) &&
                indices.get_index(0) == 5 && depths.get_index(0) == 6,
            "sort3D: empty emitter true and untouched");
    }
  });
}

// =============================================================================================================
// emitParticleBurst2D.test.ts
// =============================================================================================================

Ref<types::ParticleEmitter2D> burst_emitter2d() {
  auto e = emitter2d();
  e->data->atlas = atlas({region(0, 0, 0, 32, 32)});
  return e;
}

void emit_particle_burst_2d() {
  namespace particles = flight::particles;
  guarded("emitParticleBurst2D", [] {
    {
      auto emitter = burst_emitter2d();
      auto state = particles::create_particle_emitter_state();
      auto c = config([](auto& c) { c.speed_min = 0; c.speed_max = 0; c.lifetime_min = 10; c.lifetime_max = 10; });
      const double n = pe::emit_particle_burst_2d(emitter, state, c, 8, 200, 300);
      check(n == 8 && emitter->data->particle_count == 8, "burst2D: spawns the requested count");
      bool ok = true;
      for (int i = 0; i < 8; ++i) ok = ok && close(t(emitter, i * 4), 200) && close(t(emitter, i * 4 + 1), 300);
      check(ok, "burst2D: every particle at the burst point");
      // Addition: reserve is to exactly the new count, and the state lanes follow (lifetimes x2, velocities x3).
      check(emitter->data->ids.size() == 8 && state->lifetimes.size() == 16 && state->velocities.size() == 24 &&
                state->scales.size() == 8 && state->rotation_speeds.size() == 8 && state->color_birth.size() == 0,
            "burst2D: emitter and state capacity exactly 8, no color-variance lanes");
      // Addition: every lifetime is [age 0, 10].
      bool life = true;
      for (int i = 0; i < 8; ++i) life = life && state->lifetimes.get_index(i * 2) == 0 && state->lifetimes.get_index(i * 2 + 1) == 10;
      check(life, "burst2D: lifetimes [0, 10]");
    }
    {
      auto emitter = burst_emitter2d();
      auto state = particles::create_particle_emitter_state();
      auto c = config([](auto& c) { c.max_particles = 5; c.lifetime_min = 10; c.lifetime_max = 10; });
      check(pe::emit_particle_burst_2d(emitter, state, c, 100, 0, 0) == 5, "burst2D: capped at maxParticles");
      check(emitter->data->particle_count == 5, "burst2D: count capped");
      // Addition: a full emitter spawns nothing and draws nothing.
      Sequence seq({});
      state->random = seq.source();
      check(pe::emit_particle_burst_2d(emitter, state, c, 3, 0, 0) == 0 && emitter->data->particle_count == 5 && seq.draws() == 0,
            "burst2D: full emitter returns 0 without drawing");
    }
    {
      auto emitter = burst_emitter2d();
      auto state = particles::create_particle_emitter_state();
      auto c = config([](auto& c) { c.max_particles = 100; c.lifetime_min = 10; c.lifetime_max = 10; });
      pe::emit_particle_burst_2d(emitter, state, c, 3, 0, 0);
      pe::emit_particle_burst_2d(emitter, state, c, 4, 0, 0);
      check(emitter->data->particle_count == 7, "burst2D: adds to existing particles");
      // Addition: the second burst grew storage to exactly 7.
      check(emitter->data->ids.size() == 7 && state->lifetimes.size() == 14, "burst2D: second burst reserves to 7");
    }
    {
      auto emitter = burst_emitter2d();
      auto state = particles::create_particle_emitter_state();
      auto c = config([](auto& c) { c.lifetime_min = 10; c.lifetime_max = 10; });
      pe::emit_particle_burst_2d(emitter, state, c, 3, 0, 0);
      check(state->spawn_accumulator == 0 && state->emitter_age == 0, "burst2D: continuous bookkeeping untouched");
      // Addition: burstTimer and prev position are untouched too.
      check(state->burst_timer == 0 && std::isnan(state->prev_x) && std::isnan(state->prev_y), "burst2D: burstTimer and prev untouched");
    }
    {
      auto run = []() {
        auto emitter = burst_emitter2d();
        auto state = particles::create_particle_emitter_state(flight::math::create_random_source(99));
        auto c = config([](auto& c) {
          c.speed_min = 10;
          c.speed_max = 200;
          c.spread = pi;
          c.lifetime_min = 1;
          c.lifetime_max = 5;
        });
        pe::emit_particle_burst_2d(emitter, state, c, 10, 0, 0);
        std::vector<double> out;
        for (int i = 0; i < 20; ++i) out.push_back(state->velocities.get_index(i));
        return std::make_pair(out, state);
      };
      const auto a = run();
      const auto b = run();
      check(a.first == b.first, "burst2D: deterministic with a seeded state");
      // Addition: each speed lies in [speedMin, speedMax] and the z lane is 0.
      bool ok = true;
      for (int i = 0; i < 10; ++i) {
        const double vx = a.second->velocities.get_index(i * 3);
        const double vy = a.second->velocities.get_index(i * 3 + 1);
        const double speed = std::hypot(vx, vy);
        ok = ok && speed >= 10 - 1e-3 && speed <= 200 + 1e-3 && a.second->velocities.get_index(i * 3 + 2) == 0;
      }
      check(ok, "burst2D: seeded speeds in range, vz 0");
    }
    {
      auto emitter = burst_emitter2d();
      auto state = particles::create_particle_emitter_state();
      auto c = config([](auto& c) { c.lifetime_min = 10; c.lifetime_max = 10; c.alpha_start = 1; });
      pe::emit_particle_burst_2d(emitter, state, c, 2, 0, 0, 0x8040c0ff);
      bool ok = true;
      for (int i = 0; i < 2; ++i) {
        ok = ok && close(emitter->data->colors.get_index(i * 3), 0x80 / 255.0) &&
             close(emitter->data->colors.get_index(i * 3 + 1), 0x40 / 255.0) &&
             close(emitter->data->colors.get_index(i * 3 + 2), 0xc0 / 255.0) && close(emitter->data->alphas.get_index(i), 1);
      }
      check(ok, "burst2D: tint modulates color and alpha");
    }
    {
      auto emitter = burst_emitter2d();
      auto state = particles::create_particle_emitter_state();
      auto c = config([](auto& c) {
        c.lifetime_min = 10;
        c.lifetime_max = 10;
        c.color_start_r = 0.5;
        c.color_start_g = 1;
        c.color_start_b = 0.25;
        c.alpha_start = 1;
      });
      pe::emit_particle_burst_2d(emitter, state, c, 1, 0, 0, 0xffffff80);
      check(close(emitter->data->colors.get_index(0), 0.5) && close(emitter->data->colors.get_index(1), 1) &&
                close(emitter->data->colors.get_index(2), 0.25),
            "burst2D: tint composes with config color");
      check(close(emitter->data->alphas.get_index(0), 0x80 / 255.0), "burst2D: tint alpha");
    }
    {
      auto emitter = burst_emitter2d();
      auto state = particles::create_particle_emitter_state();
      auto c = config([](auto& c) { c.lifetime_min = 10; c.lifetime_max = 10; });
      pe::emit_particle_burst_2d(emitter, state, c, 1, 0, 0);
      check(close(emitter->data->colors.get_index(0), 1) && close(emitter->data->colors.get_index(1), 1) &&
                close(emitter->data->colors.get_index(2), 1),
            "burst2D: untinted without a tint");
    }
    {
      // Addition: per-spawn draw order is lifetime, angle, speed, [shape: none for point], scale, region id,
      // rotation speed; values derived from the source formulas.
      auto emitter = burst_emitter2d();
      Sequence seq({0.5, 0.75, 0.25, 0.5, 0.6, 0.25, 0.0, 0.0, 0.5, 0.0, 0.99, 0.5});
      auto state = particles::create_particle_emitter_state(seq.source());
      auto c = config([](auto& c) {
        c.lifetime_min = 1;
        c.lifetime_max = 3;
        c.speed_min = 10;
        c.speed_max = 30;
        c.direction_x = 1;
        c.direction_y = 0;
        c.spread = pi / 2;
        c.scale_min = 1;
        c.scale_max = 2;
        c.region_id_min = 2;
        c.region_id_max = 6;
        c.rotation_speed_min = -1;
        c.rotation_speed_max = 1;
      });
      const double n = pe::emit_particle_burst_2d(emitter, state, c, 2, 7, -3);
      check(n == 2 && seq.draws() == 12, "burst2D draws: two particles consume exactly 12 draws");
      const auto& d = emitter->data;
      const auto& v = state->velocities;
      check(state->lifetimes.get_index(0) == 0 && state->lifetimes.get_index(1) == 2 && state->lifetimes.get_index(3) == 1,
            "burst2D draws: lifetimes 2 and 1");
      check(near(v.get_index(0), 15 * std::cos(pi / 4)) && near(v.get_index(1), 15 * std::sin(pi / 4)) && v.get_index(2) == 0,
            "burst2D draws: particle 0 velocity at pi/4, speed 15");
      check(near(v.get_index(3), 0) && near(v.get_index(4), -20) && v.get_index(5) == 0, "burst2D draws: particle 1 velocity at -pi/2, speed 20");
      check(t(emitter, 0) == 7 && t(emitter, 1) == -3 && near(t(emitter, 2), pi / 4) && t(emitter, 3) == 1.5,
            "burst2D draws: particle 0 transform (point, angle, scale 1.5)");
      check(near(t(emitter, 6), -pi / 2) && t(emitter, 7) == 1, "burst2D draws: particle 1 rotation and scale");
      check(state->scales.get_index(0) == 1.5 && state->scales.get_index(1) == 1, "burst2D draws: state scales");
      check(d->ids.get_index(0) == 4 && d->ids.get_index(1) == 5, "burst2D draws: region ids 2 + floor(u * 4)");
      check(state->rotation_speeds.get_index(0) == -0.5 && state->rotation_speeds.get_index(1) == 0, "burst2D draws: rotation speeds");
      check(d->positions_z.get_index(0) == 0 && d->alphas.get_index(0) == 1 && d->colors.get_index(0) == 1,
            "burst2D draws: z 0, alphaStart, colorStart");
    }
    {
      // Addition: color variance draws six values after scale; tint scales data color, colorBirth and colorDeath.
      auto emitter = burst_emitter2d();
      Sequence seq({0.5, 0.5, 0.5, 0.5, 0.0, 0.3, 0.3, 0.1, 0.3, 0.3, 0.5});
      auto state = particles::create_particle_emitter_state(seq.source());
      auto c = config([](auto& c) { c.color_start_variance_r = 0.5; c.color_end_variance_r = 0.5; });
      pe::emit_particle_burst_2d(emitter, state, c, 1, 0, 0, 0x80ffff40);
      const double tr = 0x80 / 255.0;
      check(seq.draws() == 11, "burst2D variance: 11 draws");
      check(state->color_birth.size() == 3 && state->color_death.size() == 3, "burst2D variance: color lanes reserved");
      check(near(emitter->data->colors.get_index(0), 0.5 * tr) && near(emitter->data->colors.get_index(1), 1) &&
                near(emitter->data->colors.get_index(2), 1),
            "burst2D variance: data color r0 * tint");
      check(near(state->color_birth.get_index(0), 0.5 * tr) && near(state->color_death.get_index(0), 0.6 * tr) &&
                near(state->color_death.get_index(1), 1),
            "burst2D variance: birth/death tinted");
      check(near(emitter->data->alphas.get_index(0), 0x40 / 255.0), "burst2D variance: alpha * tint alpha");
    }
    {
      // Addition: the default config has rotationSpeedMin == rotationSpeedMax == 0, so no rotation-speed draw:
      // exactly lifetime, angle, speed, scale and region id (5 draws) per particle, and rotationSpeeds stays 0.
      auto emitter = burst_emitter2d();
      Sequence seq({0.5, 0.5, 0.5, 0.5, 0.5, 0.25, 0.25, 0.25, 0.25, 0.25});
      auto state = particles::create_particle_emitter_state(seq.source());
      check(pe::emit_particle_burst_2d(emitter, state, config(), 2, 0, 0) == 2 && seq.draws() == 10,
            "burst2D default: exactly 5 draws per particle (no rotation-speed draw)");
      check(state->rotation_speeds.get_index(0) == 0 && state->rotation_speeds.get_index(1) == 0,
            "burst2D default: rotation speeds 0");
      // Particle 1 draws 0.25 for lifetime (0.5 + 0.25 * 0.5), speed (50 + 0.25 * 50), angle -pi/2 + (0.25 - 0.5) * 2pi.
      check(near(state->lifetimes.get_index(3), 0.625) && near(std::hypot(state->velocities.get_index(3), state->velocities.get_index(4)), 62.5, 1e-4) &&
                near(t(emitter, 6), -pi / 2 - pi / 2),
            "burst2D default: second particle consumed its own five draws in order");
    }
    {
      // Addition: with color variance, colorBirth = (r0, g0, b0) and colorDeath = (r1, g1, b1) on all three lanes,
      // each clamp01(base + (u - 0.5) * 2 * variance), drawn after scale in the order r0 g0 b0 r1 g1 b1.
      auto emitter = burst_emitter2d();
      Sequence seq({0.5, 0.5, 0.5, 0.5, 0.0, 0.5, 0.75, 0.25, 0.75, 0.0, 0.5});
      auto state = particles::create_particle_emitter_state(seq.source());
      auto c = config([](auto& c) {
        c.color_start_r = 0.5;
        c.color_start_g = 0.5;
        c.color_start_b = 0.5;
        c.color_start_variance_r = 0.25;
        c.color_start_variance_g = 0.25;
        c.color_start_variance_b = 0.25;
        c.color_end_r = 0.2;
        c.color_end_g = 0.4;
        c.color_end_b = 0.6;
        c.color_end_variance_r = 0.1;
        c.color_end_variance_g = 0.1;
        c.color_end_variance_b = 0.1;
      });
      pe::emit_particle_burst_2d(emitter, state, c, 1, 0, 0);
      check(seq.draws() == 11, "burst2D variance lanes: 11 draws");
      const auto& birth = state->color_birth;
      const auto& death = state->color_death;
      check(near(birth.get_index(0), 0.25) && near(birth.get_index(1), 0.5) && near(birth.get_index(2), 0.625),
            "burst2D variance lanes: colorBirth R, G, B");
      check(near(death.get_index(0), 0.15) && near(death.get_index(1), 0.45) && near(death.get_index(2), 0.5),
            "burst2D variance lanes: colorDeath R, G, B");
      check(near(emitter->data->colors.get_index(0), 0.25) && near(emitter->data->colors.get_index(1), 0.5) &&
                near(emitter->data->colors.get_index(2), 0.625),
            "burst2D variance lanes: data colors equal colorBirth");
    }
    {
      // Addition: count is floored, and a non-positive count spawns nothing without drawing.
      auto emitter = burst_emitter2d();
      auto state = particles::create_particle_emitter_state();
      auto c = config();
      check(pe::emit_particle_burst_2d(emitter, state, c, 2.9, 0, 0) == 2 && emitter->data->particle_count == 2, "burst2D: count floored");
      Sequence seq({});
      state->random = seq.source();
      check(pe::emit_particle_burst_2d(emitter, state, c, 0, 0, 0) == 0 && pe::emit_particle_burst_2d(emitter, state, c, -4, 0, 0) == 0 &&
                emitter->data->particle_count == 2 && seq.draws() == 0,
            "burst2D: zero/negative count spawns nothing");
      // Addition: partial cap with live particles: 3 live of 5 max, ask 10 -> 2.
      auto e2 = burst_emitter2d();
      auto s2 = particles::create_particle_emitter_state();
      auto c5 = config([](auto& c) { c.max_particles = 5; });
      pe::emit_particle_burst_2d(e2, s2, c5, 3, 0, 0);
      check(pe::emit_particle_burst_2d(e2, s2, c5, 10, 0, 0) == 2 && e2->data->particle_count == 5, "burst2D: partial cap");
    }
  });
}

// =============================================================================================================
// emitParticleBurst3D.test.ts
// =============================================================================================================

void emit_particle_burst_3d() {
  namespace particles = flight::particles;
  guarded("emitParticleBurst3D", [] {
    {
      auto emitter = emitter3d();
      auto state = particles::create_particle_emitter_state(seeded_random(42));
      auto c = config([](auto& c) { c.max_particles = 100; c.speed_min = 0; c.speed_max = 0; c.emitter_shape = String("point"); });
      const double spawned = pe::emit_particle_burst_3d(emitter, state, c, 5, 10, 20, 30);
      check(spawned == 5 && emitter->data->particle_count == 5, "burst3D: spawns 5");
      bool ok = true;
      for (int i = 0; i < 5; ++i) ok = ok && t(emitter, i * 4) == 10 && t(emitter, i * 4 + 1) == 20 && emitter->data->positions_z.get_index(i) == 30;
      check(ok, "burst3D: at the 3D position");
    }
    {
      auto emitter = emitter3d();
      auto state = particles::create_particle_emitter_state(seeded_random(42));
      auto c = config([](auto& c) { c.max_particles = 3; });
      check(pe::emit_particle_burst_3d(emitter, state, c, 10, 0, 0, 0) == 3 && emitter->data->particle_count == 3, "burst3D: caps to max");
    }
    {
      auto emitter = emitter3d();
      Sequence probe({});
      auto state = particles::create_particle_emitter_state(seeded_random(42));
      auto c = config([](auto& c) { c.max_particles = 2; });
      pe::emit_particle_burst_3d(emitter, state, c, 2, 0, 0, 0);
      state->random = probe.source();
      check(pe::emit_particle_burst_3d(emitter, state, c, 5, 0, 0, 0) == 0, "burst3D: full returns 0");
      // Addition: and draws nothing, count unchanged.
      check(probe.draws() == 0 && emitter->data->particle_count == 2, "burst3D: full draws nothing");
    }
    {
      auto emitter = emitter3d();
      auto state = particles::create_particle_emitter_state(seeded_random(42));
      auto c = config([](auto& c) { c.max_particles = 50; c.speed_min = 10; c.speed_max = 10; c.emitter_shape = String("sphere"); });
      pe::emit_particle_burst_3d(emitter, state, c, 50, 0, 0, 0);
      bool pos = false;
      bool neg = false;
      bool unit = true;
      for (int i = 0; i < emitter->data->particle_count; ++i) {
        const double vz = state->velocities.get_index(i * 3 + 2);
        if (vz > 1) pos = true;
        if (vz < -1) neg = true;
        // Addition: every sphere direction is a unit vector times speed 10.
        unit = unit && near(std::hypot(state->velocities.get_index(i * 3), state->velocities.get_index(i * 3 + 1), vz), 10, 1e-3);
      }
      check(pos && neg, "burst3D: sphere velocities span +z and -z");
      check(unit, "burst3D: sphere velocity magnitude equals speed");
    }
    {
      auto emitter = emitter3d();
      auto state = particles::create_particle_emitter_state(seeded_random(42));
      auto c = config([](auto& c) {
        c.max_particles = 50;
        c.emitter_shape = String("box");
        c.emitter_width = 10;
        c.emitter_height = 10;
        c.emitter_depth = 10;
        c.speed_min = 0;
        c.speed_max = 0;
      });
      pe::emit_particle_burst_3d(emitter, state, c, 50, 100, 200, 300);
      bool z_offset = false;
      bool inside = true;
      for (int i = 0; i < emitter->data->particle_count; ++i) {
        const double z = emitter->data->positions_z.get_index(i);
        if (std::abs(z - 300) > 0.1) z_offset = true;
        // Addition: box offsets stay within half-extents.
        inside = inside && std::abs(t(emitter, i * 4) - 100) <= 5 && std::abs(t(emitter, i * 4 + 1) - 200) <= 5 && std::abs(z - 300) <= 5;
      }
      check(z_offset, "burst3D: box applies z offsets");
      check(inside, "burst3D: box offsets within half extents");
    }
    {
      auto emitter = emitter3d();
      auto state = particles::create_particle_emitter_state(seeded_random(42));
      auto c = config([](auto& c) { c.max_particles = 10; c.alpha_start = 0.5; });
      pe::emit_particle_burst_3d(emitter, state, c, 3, 0, 0, 0);
      bool ok = true;
      for (int i = 0; i < 3; ++i) ok = ok && close(emitter->data->alphas.get_index(i), 0.5);
      check(ok, "burst3D: initial alpha from config");
    }
    {
      auto emitter = emitter3d();
      auto state = particles::create_particle_emitter_state(seeded_random(42));
      auto c = config([](auto& c) { c.max_particles = 10; c.alpha_start = 1; });
      pe::emit_particle_burst_3d(emitter, state, c, 2, 0, 0, 0, 0x8040c0ff);
      bool ok = true;
      for (int i = 0; i < 2; ++i) {
        ok = ok && close(emitter->data->colors.get_index(i * 3), 0x80 / 255.0) &&
             close(emitter->data->colors.get_index(i * 3 + 1), 0x40 / 255.0) &&
             close(emitter->data->colors.get_index(i * 3 + 2), 0xc0 / 255.0) && close(emitter->data->alphas.get_index(i), 1);
      }
      check(ok, "burst3D: tint modulates color and alpha");
    }
    {
      auto emitter = emitter3d();
      auto state = particles::create_particle_emitter_state(seeded_random(42));
      auto c = config([](auto& c) {
        c.max_particles = 10;
        c.color_start_r = 0.5;
        c.color_start_g = 1;
        c.color_start_b = 0.25;
        c.alpha_start = 1;
      });
      pe::emit_particle_burst_3d(emitter, state, c, 1, 0, 0, 0, 0xffffff80);
      check(close(emitter->data->colors.get_index(0), 0.5) && close(emitter->data->colors.get_index(1), 1) &&
                close(emitter->data->colors.get_index(2), 0.25) && close(emitter->data->alphas.get_index(0), 0x80 / 255.0),
            "burst3D: tint composes with config color");
    }
    {
      // Addition: point-shape draw order is lifetime, speed, angle (speed before angle, unlike 2D), scale, id;
      // vz = directionZ * speed / |direction|, and the stored rotation is atan2(vy, vx).
      auto emitter = emitter3d();
      Sequence seq({0.5, 0.5, 0.75, 0.0, 0.5});
      auto state = particles::create_particle_emitter_state(seq.source());
      auto c = config([](auto& c) { c.direction_x = 0; c.direction_y = 0; c.direction_z = 2; c.spread = pi / 2; });
      pe::emit_particle_burst_3d(emitter, state, c, 1, 1, 2, 3);
      const auto& v = state->velocities;
      check(seq.draws() == 5, "burst3D draws: point consumes 5 draws");
      check(state->lifetimes.get_index(1) == 0.75, "burst3D draws: lifetime 0.75");
      check(near(v.get_index(0), 75 * std::cos(pi / 4), 1e-4) && near(v.get_index(1), 75 * std::sin(pi / 4), 1e-4) &&
                v.get_index(2) == 75,
            "burst3D draws: velocity (75cos45, 75sin45, 75)");
      check(t(emitter, 0) == 1 && t(emitter, 1) == 2 && emitter->data->positions_z.get_index(0) == 3 && near(t(emitter, 2), pi / 4) &&
                t(emitter, 3) == 1 && emitter->data->ids.get_index(0) == 0,
            "burst3D draws: transform, z, rotation atan2(vy,vx), scale, id");
    }
    {
      // Addition: box draws three offsets after the angle: (u - 0.5) * extent per axis.
      auto emitter = emitter3d();
      Sequence seq({0.5, 0.5, 0.5, 0.25, 0.75, 0.0, 0.5, 0.5});
      auto state = particles::create_particle_emitter_state(seq.source());
      auto c = config([](auto& c) {
        c.emitter_shape = String("box");
        c.emitter_width = 10;
        c.emitter_height = 20;
        c.emitter_depth = 30;
        c.speed_min = 0;
        c.speed_max = 0;
      });
      pe::emit_particle_burst_3d(emitter, state, c, 1, 100, 200, 300);
      check(seq.draws() == 8, "burst3D box: 8 draws");
      check(t(emitter, 0) == 97.5 && t(emitter, 1) == 205 && emitter->data->positions_z.get_index(0) == 285, "burst3D box: offsets");
    }
    {
      // Addition: sphere rejection sampling rejects s2 >= 1 and s2 == 0, then (u, v) = (0.5, 0) gives the
      // direction (sqrt(3)/2, 0, 1/2).
      auto emitter = emitter3d();
      Sequence seq({0.5, 0.5, 0.99, 0.99, 0.5, 0.5, 0.75, 0.5, 0.5, 0.5});
      auto state = particles::create_particle_emitter_state(seq.source());
      auto c = config([](auto& c) { c.emitter_shape = String("sphere"); c.speed_min = 10; c.speed_max = 10; });
      pe::emit_particle_burst_3d(emitter, state, c, 1, 4, 5, 6);
      const auto& v = state->velocities;
      check(seq.draws() == 10, "burst3D sphere: 10 draws with two rejections");
      check(near(v.get_index(0), 5 * std::sqrt(3.0), 1e-4) && near(v.get_index(1), 0) && near(v.get_index(2), 5),
            "burst3D sphere: velocity");
      check(t(emitter, 0) == 4 && t(emitter, 1) == 5 && emitter->data->positions_z.get_index(0) == 6 && near(t(emitter, 2), 0),
            "burst3D sphere: zero radius keeps the burst point");
    }
    {
      // Addition: a cone3d sample at theta 0 points exactly along the configured direction (default (0,-1,0)).
      auto emitter = emitter3d();
      Sequence seq({0.5, 0.5, 0.0, 0.3, 0.5, 0.5});
      auto state = particles::create_particle_emitter_state(seq.source());
      auto c = config([](auto& c) { c.emitter_shape = String("cone3d"); c.emitter_cone_angle = pi / 2; c.speed_min = 10; c.speed_max = 10; });
      pe::emit_particle_burst_3d(emitter, state, c, 1, 0, 0, 0);
      const auto& v = state->velocities;
      check(seq.draws() == 6, "burst3D cone: 6 draws");
      check(near(v.get_index(0), 0) && near(v.get_index(1), -10) && near(v.get_index(2), 0), "burst3D cone: velocity along -Y");
      check(near(t(emitter, 2), -pi / 2), "burst3D cone: rotation atan2(-10, 0)");
    }
    {
      // Addition: a colorCurve of exactly 3 entries (one RGB key) is a curve: colors sample it, not colorStart.
      auto emitter = emitter3d();
      Sequence seq({0.5, 0.5, 0.5, 0.5, 0.5});
      auto state = particles::create_particle_emitter_state(seq.source());
      auto c = config([](auto& c) { c.color_curve = flight::Array<double>({0.2, 0.4, 0.6}); });
      pe::emit_particle_burst_3d(emitter, state, c, 1, 0, 0, 0);
      check(near(emitter->data->colors.get_index(0), 0.2) && near(emitter->data->colors.get_index(1), 0.4) &&
                near(emitter->data->colors.get_index(2), 0.6),
            "burst3D: 3-entry colorCurve is sampled");
    }
    {
      // Addition: cone3d with emitterConeAngle exactly 0 takes the sphere path: (u, v) = (0.5, 0) gives the direction
      // (sqrt(3)/2, 0, 1/2), not the cone axis (0, -1, 0).
      auto emitter = emitter3d();
      Sequence seq({0.5, 0.5, 0.75, 0.5, 0.5, 0.5});
      auto state = particles::create_particle_emitter_state(seq.source());
      auto c = config([](auto& c) { c.emitter_shape = String("cone3d"); c.emitter_cone_angle = 0; c.speed_min = 10; c.speed_max = 10; });
      pe::emit_particle_burst_3d(emitter, state, c, 1, 0, 0, 0);
      const auto& v = state->velocities;
      check(seq.draws() == 6, "burst3D cone angle 0: 6 draws");
      check(near(v.get_index(0), 5 * std::sqrt(3.0), 1e-4) && near(v.get_index(1), 0) && near(v.get_index(2), 5),
            "burst3D cone angle 0: sphere direction");
    }
    {
      // Addition: an empty scaleCurve is not a curve; the transform scale is spawnScale (2), not 2 * sample([]) = 0.
      auto emitter = emitter3d();
      Sequence seq({0.5, 0.5, 0.5, 0.5, 0.5});
      auto state = particles::create_particle_emitter_state(seq.source());
      auto c = config([](auto& c) { c.scale_min = 2; c.scale_max = 2; c.scale_curve = flight::Array<double>(); });
      pe::emit_particle_burst_3d(emitter, state, c, 1, 0, 0, 0);
      check(t(emitter, 3) == 2 && state->scales.get_index(0) == 2, "burst3D: empty scaleCurve leaves spawnScale");
    }
    {
      // Addition: both sphere rejection loops reject a candidate exactly on the unit circle (u = 2*0 - 1 = -1,
      // v = 0, s2 = 1). Draws: lifetime, speed, (0, 0.5) rejected, (0.75, 0.5) accepted; radius cbrt(0.125) * 4 = 2;
      // position (0, 0.5) rejected, (0.75, 0.5) accepted -> offset (0.5 * sqrt(3) * 2, 0, 0.5 * 2); scale; id.
      auto emitter = emitter3d();
      Sequence seq({0.5, 0.5, 0.0, 0.5, 0.75, 0.5, 0.125, 0.0, 0.5, 0.75, 0.5, 0.5, 0.5});
      auto state = particles::create_particle_emitter_state(seq.source());
      auto c = config([](auto& c) { c.emitter_shape = String("sphere"); c.emitter_radius = 4; c.speed_min = 10; c.speed_max = 10; });
      pe::emit_particle_burst_3d(emitter, state, c, 1, 10, 20, 30);
      const auto& v = state->velocities;
      check(seq.draws() == 13, "burst3D sphere on-circle: 13 draws (two rejections)");
      check(near(v.get_index(0), 5 * std::sqrt(3.0), 1e-4) && near(v.get_index(1), 0) && near(v.get_index(2), 5),
            "burst3D sphere on-circle: direction from the accepted candidate");
      check(near(t(emitter, 0), 10 + std::sqrt(3.0), 1e-4) && near(t(emitter, 1), 20) && near(emitter->data->positions_z.get_index(0), 31),
            "burst3D sphere on-circle: position offset from the accepted candidate");
    }
    {
      // Addition: deterministic under the upstream LCG.
      auto run = []() {
        auto emitter = emitter3d();
        auto state = particles::create_particle_emitter_state(seeded_random(7));
        auto c = config([](auto& c) { c.emitter_shape = String("sphere"); c.emitter_radius = 3; });
        pe::emit_particle_burst_3d(emitter, state, c, 6, 0, 0, 0);
        std::vector<double> out;
        for (int i = 0; i < 6; ++i) {
          out.push_back(t(emitter, i * 4));
          out.push_back(emitter->data->positions_z.get_index(i));
          out.push_back(state->velocities.get_index(i * 3 + 2));
        }
        return out;
      };
      check(run() == run(), "burst3D: deterministic with seeded random");
    }
  });
}

}  // namespace

int main() {
  // particleEmitter.test.ts
  append_particle_emitter_2d_particle();
  clear_particle_emitter_2d();
  compact_particle_emitter_2d();
  compute_particle_emitter_2d_local_bounds_rectangle();
  create_particle_emitter_data();
  get_particle_emitter_2d_capacity();
  get_particle_emitter_2d_particle_alpha();
  get_particle_emitter_2d_particle_id();
  get_particle_emitter_2d_particle_velocity();
  initialize_particle_emitter_data();
  remove_particle_emitter_2d_particle();
  reserve_particle_emitter_2d();
  set_particle_emitter_2d_particle();
  set_particle_emitter_2d_particle_alpha();
  set_particle_emitter_2d_particle_color();
  set_particle_emitter_2d_particle_velocity();
  // particleEmitter3D.test.ts
  append_particle_emitter_3d_particle();
  clear_particle_emitter_3d();
  compact_particle_emitter_3d();
  compute_particle_emitter_3d_local_bounds_aabb();
  get_particle_emitter_3d_capacity();
  get_particle_emitter_3d_particle_alpha();
  get_particle_emitter_3d_particle_id();
  get_particle_emitter_3d_particle_velocity();
  get_particle_emitter_3d_runtime();
  is_particle_emitter_3d();
  remove_particle_emitter_3d_particle();
  reserve_particle_emitter_3d();
  set_particle_emitter_3d_particle();
  set_particle_emitter_3d_particle_alpha();
  set_particle_emitter_3d_particle_color();
  set_particle_emitter_3d_particle_velocity();
  sort_particle_emitter_3d_indices_by_view_depth();
  // emitParticleBurst2D.test.ts / emitParticleBurst3D.test.ts
  emit_particle_burst_2d();
  emit_particle_burst_3d();
  if (failures == 0) std::cout << "generated_particleemitter_test: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
