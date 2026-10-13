// The generated @flighthq/particles-formats libGDX `.p` codec, exercised natively: parseLibgdxParticle,
// parseLibgdxParticleDocument (config, preserved document and import diagnostics), serializeLibgdxParticle
// and serializeLibgdxParticleDocument (text and warnings). Ported case by case from
// packages/particles-formats/src/libgdxParse.test.ts and libgdxSerialize.test.ts; both upstream files share
// the byte-identical SPARK_P fixture, carried here once. Checks the TypeScript source implies but upstream
// does not assert are marked "Addition:" -- exact parsed values, the full diagnostic list in order, the
// empty-input error text, CRLF input, spawn-shape mapping, built-in defaults, the exact serialized text of a
// small config (including JS `-0` formatting) and the exact warning texts in order.
//
// Not natively evidenced: upstream's "returns a ParticleEmitterConfig (not a ParseResult object)" probes a
// JS object for a missing `document` property. A native config has no such member at all, so the case is
// carried as a compile-time check of the return type plus the numeric maxParticles read.
#include <flight/particles/particle_emitter_config.hpp>
#include <flight/particles_formats/libgdx_parse.hpp>
#include <flight/particles_formats/libgdx_serialize.hpp>

#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <type_traits>
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

bool near(double a, double b, double epsilon = 1e-5) { return std::abs(a - b) <= epsilon; }

// toBeCloseTo(x) defaults to 2 digits: |a - x| < 0.5 * 10^-2.
bool close(double a, double b, int digits = 2) { return near(a, b, 0.5 * std::pow(10.0, -digits)); }

namespace pf = flight::particles_formats;
namespace types = flight::types;
using flight::Ref;
using flight::String;

constexpr double deg_to_rad = std::numbers::pi / 180.0;

const String SPARK_P = String(R"P(Particle Effect
- Spark -
minParticleCount: 4
maxParticleCount: 100
additive: true
imagePath: spark.png
imageCount: 1

Delay
active: false
lowMin: 0
lowMax: 0
highMin: 0
highMax: 0
relative: false
scalingCount: 1
scaling0: 1
timelineCount: 1
timeline0: 0

Duration
lowMin: 3000
lowMax: 3000
highMin: 3000
highMax: 3000
relative: false
scalingCount: 1
scaling0: 1
timelineCount: 1
timeline0: 0

Emission
lowMin: 0
lowMax: 0
highMin: 80
highMax: 120
relative: false
scalingCount: 1
scaling0: 1
timelineCount: 1
timeline0: 0

Life
lowMin: 400
lowMax: 400
highMin: 800
highMax: 800
relative: false
scalingCount: 1
scaling0: 1
timelineCount: 1
timeline0: 0

Life Offset
active: false
lowMin: 0
lowMax: 0
highMin: 0
highMax: 0
relative: false
scalingCount: 1
scaling0: 1
timelineCount: 1
timeline0: 0

X Offset
active: false
lowMin: 0
lowMax: 0
highMin: 0
highMax: 0
relative: false
scalingCount: 1
scaling0: 1
timelineCount: 1
timeline0: 0

Y Offset
active: false
lowMin: 0
lowMax: 0
highMin: 0
highMax: 0
relative: false
scalingCount: 1
scaling0: 1
timelineCount: 1
timeline0: 0

Spawn Shape
shape: point

Spawn Width
lowMin: 0
lowMax: 0
highMin: 0
highMax: 0
relative: false
scalingCount: 1
scaling0: 1
timelineCount: 1
timeline0: 0

Spawn Height
lowMin: 0
lowMax: 0
highMin: 0
highMax: 0
relative: false
scalingCount: 1
scaling0: 1
timelineCount: 1
timeline0: 0

Scale
lowMin: 16
lowMax: 16
highMin: 32
highMax: 32
relative: false
scalingCount: 2
scaling0: 1
scaling1: 0.25
timelineCount: 2
timeline0: 0
timeline1: 1

Velocity
active: true
lowMin: 50
lowMax: 50
highMin: 200
highMax: 200
relative: false
scalingCount: 1
scaling0: 1
timelineCount: 1
timeline0: 0

Angle
active: true
lowMin: 60
lowMax: 60
highMin: 120
highMax: 120
relative: false
scalingCount: 1
scaling0: 1
timelineCount: 1
timeline0: 0

Rotation
active: false
lowMin: 0
lowMax: 0
highMin: 0
highMax: 0
relative: false
scalingCount: 1
scaling0: 1
timelineCount: 1
timeline0: 0

Wind
active: false
lowMin: 0
lowMax: 0
highMin: 0
highMax: 0
relative: false
scalingCount: 1
scaling0: 1
timelineCount: 1
timeline0: 0

Gravity
active: false
lowMin: 0
lowMax: 0
highMin: 0
highMax: 0
relative: false
scalingCount: 1
scaling0: 1
timelineCount: 1
timeline0: 0

Tint
colors: ffaa00,ff0000
timelineCount: 2
timeline0: 0
timeline1: 1

Transparency
lowMin: 0
lowMax: 0
highMin: 1
highMax: 1
relative: false
scalingCount: 2
scaling0: 1
scaling1: 0
timelineCount: 2
timeline0: 0
timeline1: 1

)P");

// JS String.prototype.replace with a string pattern: the first occurrence only.
String replace(const String& text, const std::string& from, const std::string& to) {
  std::string s = text.to_utf8();
  const auto at = s.find(from);
  if (at == std::string::npos) {
    check(false, "fixture replace target not found: " + from);
    return text;
  }
  s.replace(at, from.size(), to);
  return String(s);
}

bool contains(const String& haystack, const std::string& needle) {
  return haystack.to_utf8().find(needle) != std::string::npos;
}

std::string str(const String& s) { return s.to_utf8(); }

bool opt_eq(const std::optional<String>& value, const char* expected) {
  return value.has_value() && value.value() == String(expected);
}

std::string opt_str(const std::optional<String>& value) { return value.has_value() ? value->to_utf8() : "<null>"; }

bool doubles_eq(const flight::Array<double>& values, std::vector<double> expected) {
  if (values.size() != expected.size()) return false;
  for (std::size_t i = 0; i < expected.size(); ++i) {
    if (values.element(static_cast<double>(i)) != expected[i]) return false;
  }
  return true;
}

bool strings_eq(const flight::Array<String>& values, std::vector<std::string> expected) {
  if (values.size() != expected.size()) return false;
  for (std::size_t i = 0; i < expected.size(); ++i) {
    if (values.element(static_cast<double>(i)).to_utf8() != expected[i]) return false;
  }
  return true;
}

using Diagnostics = flight::Array<Ref<types::ImportDiagnostic>>;

std::vector<std::string> kinds(const Diagnostics& diagnostics) {
  std::vector<std::string> out;
  for (std::size_t i = 0; i < diagnostics.size(); ++i) out.push_back(diagnostics.element(static_cast<double>(i))->kind.to_utf8());
  return out;
}

std::string joined(const std::vector<std::string>& values) {
  std::string out = "[";
  for (std::size_t i = 0; i < values.size(); ++i) out += (i ? "," : "") + values[i];
  return out + "]";
}

Ref<types::ImportDiagnostic> find_kind(const Diagnostics& diagnostics, const std::string& kind) {
  for (std::size_t i = 0; i < diagnostics.size(); ++i) {
    auto d = diagnostics.element(static_cast<double>(i));
    if (d->kind.to_utf8() == kind) return d;
  }
  return nullptr;
}

const String& skip() { return types::import_diagnostic_severity->skip; }
const String& recover() { return types::import_diagnostic_severity->recover; }

Ref<types::LibgdxParseOptions> parse_options(double texture_size) {
  return flight::make_ref<types::LibgdxParseOptions>(types::LibgdxParseOptions{.texture_size = texture_size});
}

// Runs `body`, turning any escaped exception into a failing check instead of a crashed run.
void guarded(const std::string& name, const std::function<void()>& body) {
  try {
    body();
  } catch (const std::exception& e) {
    check(false, name + ": unexpected exception: " + e.what());
  } catch (...) {
    check(false, name + ": unexpected non-standard exception");
  }
}

// The anonymous partial-input shape createParticleEmitterConfig accepts, recovered from its signature.
template <typename>
struct FirstParameter;
template <typename Result, typename Parameter>
struct FirstParameter<Result (*)(Parameter)> {
  using type = Parameter;
};
using ConfigInput = FirstParameter<decltype(&flight::particles::create_particle_emitter_config)>::type::value_type::element_type;

Ref<types::ParticleEmitterConfig> make_config(const std::function<void(ConfigInput&)>& fill = [](ConfigInput&) {}) {
  ConfigInput input;
  fill(input);
  return flight::particles::create_particle_emitter_config(flight::make_ref<ConfigInput>(std::move(input)));
}

// The anonymous Partial<LibgdxParticleDocument> serializeLibgdxParticle accepts, recovered from its signature.
template <typename>
struct SecondParameter;
template <typename Result, typename A, typename B, typename C>
struct SecondParameter<Result (*)(A, B, C)> {
  using type = B;
};
using PartialDocument = SecondParameter<decltype(&pf::serialize_libgdx_particle)>::type::value_type::element_type;

// TypeScript passes the full parsed document where Partial<LibgdxParticleDocument> is expected; natively that
// is every member present.
Ref<PartialDocument> as_partial(const Ref<types::LibgdxParticleDocument>& doc) {
  PartialDocument p;
  p.name = doc->name;
  p.min_particle_count = doc->min_particle_count;
  p.max_particle_count = doc->max_particle_count;
  p.additive = doc->additive;
  p.premultiplied_alpha = doc->premultiplied_alpha;
  p.delay = doc->delay;
  p.duration = doc->duration;
  p.emission = doc->emission;
  p.life = doc->life;
  p.life_offset = doc->life_offset;
  p.x_offset = doc->x_offset;
  p.y_offset = doc->y_offset;
  p.spawn_shape = doc->spawn_shape;
  p.spawn_width = doc->spawn_width;
  p.spawn_height = doc->spawn_height;
  p.scale = doc->scale;
  p.velocity = doc->velocity;
  p.angle = doc->angle;
  p.rotation = doc->rotation;
  p.wind = doc->wind;
  p.gravity = doc->gravity;
  p.tint = doc->tint;
  p.transparency = doc->transparency;
  p.image_count = doc->image_count;
  p.image_path = doc->image_path;
  return flight::make_ref<PartialDocument>(std::move(p));
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using Writable = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Type>>>>;
  using Readonly = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return flight::structural_ref_cast<Readonly>(Writable(value));
}

String serialize(const Ref<types::ParticleEmitterConfig>& config,
                 std::optional<Ref<PartialDocument>> existing = std::nullopt,
                 std::optional<Ref<types::LibgdxSerializeOptions>> options = std::nullopt) {
  return pf::serialize_libgdx_particle(readonly(config), existing, options);
}

Ref<types::ParticleSerializeResult> serialize_document(const Ref<types::ParticleEmitterConfig>& config) {
  return pf::serialize_libgdx_particle_document(readonly(config), std::nullopt, std::nullopt);
}

bool any_warning(const flight::Array<String>& warnings, const std::function<bool(const std::string&)>& predicate) {
  for (std::size_t i = 0; i < warnings.size(); ++i) {
    if (predicate(warnings.element(static_cast<double>(i)).to_utf8())) return true;
  }
  return false;
}

std::string lower(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

void expect_diagnostic(const std::string& test, const String& fixture, const std::string& kind, const String& severity) {
  guarded(test, [&] {
    const auto result = pf::parse_libgdx_particle_document(fixture);
    const auto crumb = find_kind(result->diagnostics, kind);
    check(crumb != nullptr, test + ": diagnostic is defined (got " + joined(kinds(result->diagnostics)) + ")");
    if (crumb == nullptr) return;
    check(crumb->severity == severity, test + ": severity " + crumb->severity.to_utf8());
    check(crumb->origin == String("collectLibgdxDiagnostics"), test + ": origin " + crumb->origin.to_utf8());
  });
}

void parse_libgdx_particle() {
  guarded("parses maxParticleCount to maxParticles", [] {
    check(pf::parse_libgdx_particle(SPARK_P)->max_particles == 100.0, "parses maxParticleCount to maxParticles");
  });
  guarded("maps additive=true", [] {
    const auto c = pf::parse_libgdx_particle(SPARK_P);
    check(opt_eq(c->blend_mode, "add"), "maps additive=true to blendMode=\"add\" (got " + opt_str(c->blend_mode) + ")");
  });
  guarded("converts life range", [] {
    const auto c = pf::parse_libgdx_particle(SPARK_P);
    check(close(c->lifetime_min, 0.4), "converts life range from ms to seconds: lifetimeMin");
    check(close(c->lifetime_max, 0.8), "converts life range from ms to seconds: lifetimeMax");
    // Addition: lifetimeMin = max(0, 400 / 1000) and lifetimeMax = 800 / 1000, exactly.
    check(c->lifetime_min == 400.0 / 1000.0 && c->lifetime_max == 800.0 / 1000.0, "Addition: life range is exact");
  });
  guarded("maps velocity", [] {
    const auto c = pf::parse_libgdx_particle(SPARK_P);
    check(close(c->speed_min, 50), "maps velocity to speedMin");
    check(close(c->speed_max, 200), "maps velocity to speedMax");
    // Addition: midpoints of 50..50 and 200..200 are exact.
    check(c->speed_min == 50.0 && c->speed_max == 200.0, "Addition: velocity range is exact");
  });
  guarded("derives direction", [] {
    const auto c = pf::parse_libgdx_particle(SPARK_P);
    check(close(c->direction_x, 0, 2), "derives directionX from angle midpoint");
    check(close(c->direction_y, -1, 2), "derives directionY from angle midpoint");
    check(close(c->spread, (30 * std::numbers::pi) / 180, 3), "derives spread from range");
    // Addition: spread = (120 - 60) * 0.5 * DEG_TO_RAD with DEG_TO_RAD = PI / 180.
    check(c->spread == (120.0 - 60.0) * 0.5 * deg_to_rad, "Addition: spread is exactly 30 * DEG_TO_RAD");
  });
  guarded("maps tint colors", [] {
    const auto c = pf::parse_libgdx_particle(SPARK_P);
    check(close(c->color_start_r, 1), "maps tint colors: colorStartR");
    check(close(c->color_start_g, 0xaa / 255.0, 2), "maps tint colors: colorStartG");
    check(close(c->color_end_r, 1), "maps tint colors: colorEndR");
    check(close(c->color_end_g, 0), "maps tint colors: colorEndG");
    // Addition: hexToRgb divides each parsed byte by 255; ffaa00 -> (1, 170/255, 0), ff0000 -> (1, 0, 0).
    check(c->color_start_r == 1.0 && c->color_start_g == 170.0 / 255.0 && c->color_start_b == 0.0,
          "Addition: start color is exactly (1, 170/255, 0)");
    check(c->color_end_r == 1.0 && c->color_end_g == 0.0 && c->color_end_b == 0.0, "Addition: end color is exactly (1, 0, 0)");
  });
  guarded("maps transparency", [] {
    const auto c = pf::parse_libgdx_particle(SPARK_P);
    check(close(c->alpha_start, 1), "maps transparency scaling to alphaStart");
    check(close(c->alpha_end, 0), "maps transparency scaling to alphaEnd");
  });
  guarded("uses point emitter shape", [] {
    check(pf::parse_libgdx_particle(SPARK_P)->emitter_shape == String("point"), "uses point emitter shape when spawn shape is point");
  });
  guarded("normalises scale by textureSize", [] {
    const auto c = pf::parse_libgdx_particle(SPARK_P, parse_options(32));
    check(close(c->scale_min, 0.5), "normalises scale by textureSize: scaleMin");
    check(close(c->scale_max, 1.0), "normalises scale by textureSize: scaleMax");
    // Addition: 16 / 32 and 32 / 32 are exact; without the option textureSize defaults to 1.
    check(c->scale_min == 0.5 && c->scale_max == 1.0, "Addition: normalised scale is exact");
    const auto raw = pf::parse_libgdx_particle(SPARK_P);
    check(raw->scale_min == 16.0 && raw->scale_max == 32.0, "Addition: default textureSize 1 leaves pixel scale");
  });
  guarded("reads last scale scaling value", [] {
    check(close(pf::parse_libgdx_particle(SPARK_P)->scale_end, 0.25), "reads last scale scaling value as scaleEnd");
  });
  guarded("maps duration > 0", [] {
    const auto c = pf::parse_libgdx_particle(SPARK_P);
    check(c->loop == false, "maps duration > 0 to a finite non-looping emitter: loop");
    check(close(c->duration, 3), "maps duration > 0 to a finite non-looping emitter: duration");
  });
  guarded("maps additive=false", [] {
    const auto c = pf::parse_libgdx_particle(replace(SPARK_P, "additive: true", "additive: false"));
    check(opt_eq(c->blend_mode, "normal"), "maps additive=false to blendMode=\"normal\" (got " + opt_str(c->blend_mode) + ")");
  });
  // throws a clear error for empty input
  for (const char* input : {"", "   "}) {
    bool threw = false;
    std::string message;
    try {
      static_cast<void>(pf::parse_libgdx_particle(String(input)));
    } catch (const std::exception& e) {
      threw = true;
      message = e.what();
    }
    check(threw && message.find("Invalid libGDX particle") != std::string::npos,
          std::string("throws a clear error for empty input '") + input + "' (got '" + message + "')");
    // Addition: the whole message is the one the source throws.
    check(message == "Invalid libGDX particle: input is empty or not a string", "Addition: exact empty-input message");
  }
  guarded("returns a ParticleEmitterConfig", [] {
    // Native form of "not a ParseResult object": the return type is the config itself.
    static_assert(std::is_same_v<decltype(pf::parse_libgdx_particle(SPARK_P)), Ref<types::ParticleEmitterConfig>>);
    check(pf::parse_libgdx_particle(SPARK_P)->max_particles == 100.0, "returns a ParticleEmitterConfig (not a ParseResult object)");
  });

  guarded("Addition: rotation, wind, gravity and spawn rate", [] {
    const auto c = pf::parse_libgdx_particle(SPARK_P);
    // Inactive Rotation/Wind/Gravity sections read as [0, 0]; Emission is never mapped, so spawnRate is
    // createParticleEmitterConfig's default 10.
    check(c->rotation_speed_min == 0.0 && c->rotation_speed_max == 0.0, "Addition: inactive rotation reads as 0");
    check(c->gravity_x == 0.0 && c->gravity_y == 0.0, "Addition: inactive wind/gravity read as 0");
    check(c->spawn_rate == 10.0, "Addition: spawnRate is the substituted default 10");
    check(c->emitter_radius == 0.0 && c->emitter_width == 0.0 && c->emitter_height == 0.0, "Addition: point emitter has no extent");
  });
  guarded("Addition: CRLF input", [] {
    // The source splits on /\r?\n/, so CRLF line endings parse identically.
    std::string crlf;
    for (char ch : SPARK_P.to_utf8()) {
      if (ch == '\n') crlf += '\r';
      crlf += ch;
    }
    const auto result = pf::parse_libgdx_particle_document(String(crlf));
    check(result->document->name == String("Spark"), "Addition: CRLF input keeps the emitter name");
    check(result->document->image_path == String("spark.png"), "Addition: CRLF input keeps imagePath");
    check(result->config->speed_max == 200.0 && result->config->scale_end == 0.25, "Addition: CRLF input keeps values");
  });
  guarded("Addition: spawn shapes", [] {
    // ellipse with equal width/height midpoints -> circle of radius width / 2; unequal -> rect.
    const String ellipse = replace(SPARK_P, "shape: point", "shape: ellipse");
    const String wide = replace(ellipse, "Spawn Width\nlowMin: 0\nlowMax: 0\nhighMin: 0\nhighMax: 0",
                                "Spawn Width\nlowMin: 40\nlowMax: 40\nhighMin: 40\nhighMax: 40");
    const String circle = replace(wide, "Spawn Height\nlowMin: 0\nlowMax: 0\nhighMin: 0\nhighMax: 0",
                                  "Spawn Height\nlowMin: 40\nlowMax: 40\nhighMin: 40\nhighMax: 40");
    const auto c = pf::parse_libgdx_particle(circle);
    check(c->emitter_shape == String("circle") && c->emitter_radius == 20.0, "Addition: equal ellipse maps to circle r=20");
    const String rect = replace(wide, "Spawn Height\nlowMin: 0\nlowMax: 0\nhighMin: 0\nhighMax: 0",
                                "Spawn Height\nlowMin: 20\nlowMax: 20\nhighMin: 20\nhighMax: 20");
    const auto r = pf::parse_libgdx_particle(rect);
    check(r->emitter_shape == String("rect") && r->emitter_width == 40.0 && r->emitter_height == 20.0,
          "Addition: unequal ellipse maps to rect 40x20");
    const auto zero = pf::parse_libgdx_particle(ellipse);
    check(zero->emitter_shape == String("point"), "Addition: zero-size ellipse stays point");
    const auto line = pf::parse_libgdx_particle(replace(SPARK_P, "shape: point", "shape: line"));
    check(line->emitter_shape == String("point"), "Addition: line maps to point");
  });
  guarded("Addition: zero duration loops", [] {
    // documentToConfig: durMid = midpoint of rangeToMinMax(duration); loop = durMid <= 0, duration = durMid > 0 ?
    // durMid / 1000 : 0. The section is named "Duration" (bare line), as sectionsToDocument reads it.
    const auto c = pf::parse_libgdx_particle(replace(SPARK_P, "Duration\nlowMin: 3000\nlowMax: 3000\nhighMin: 3000\nhighMax: 3000",
                                                     "Duration\nlowMin: 0\nlowMax: 0\nhighMin: 0\nhighMax: 0"));
    check(c->loop == true, "Addition: duration midpoint exactly 0 loops");
    check(c->duration == 0.0, "Addition: duration midpoint exactly 0 gives duration 0");
    // Only lowMin/lowMax present: highMin/highMax default to 0 in readRange, so the midpoint is still 0.
    const auto bare = pf::parse_libgdx_particle(String("Particle Effect\n- Bare -\nDuration\nlowMin: 0\nlowMax: 0\n"));
    check(bare->loop == true && bare->duration == 0.0, "Addition: Duration lowMin/lowMax 0 with default highs loops");
  });
  guarded("Addition: rotation speed", [] {
    // rangeToMinMax: lo = (lowMin + lowMax) * 0.5, hi = (highMin + highMax) * 0.5; [min(lo, hi), max(lo, hi)].
    // Rotation 10/30 and 100/140 -> rot [20, 120]. Life 200/600 and 600/1000 -> life [400, 800] ms.
    // lifetimeMid = (400/1000 + 800/1000) * 0.5 || 1; rotationSpeed = rot * DEG_TO_RAD / lifetimeMid.
    String text = replace(SPARK_P, "Rotation\nactive: false\nlowMin: 0\nlowMax: 0\nhighMin: 0\nhighMax: 0",
                          "Rotation\nactive: true\nlowMin: 10\nlowMax: 30\nhighMin: 100\nhighMax: 140");
    text = replace(text, "Life\nlowMin: 400\nlowMax: 400\nhighMin: 800\nhighMax: 800",
                   "Life\nlowMin: 200\nlowMax: 600\nhighMin: 600\nhighMax: 1000");
    const auto c = pf::parse_libgdx_particle(text);
    const double life_min = (200.0 + 600.0) * 0.5;
    const double life_max = (600.0 + 1000.0) * 0.5;
    const double lifetime_mid = (life_min / 1000.0 + life_max / 1000.0) * 0.5;
    const double rot_min = (10.0 + 30.0) * 0.5;
    const double rot_max = (100.0 + 140.0) * 0.5;
    check(c->lifetime_min == life_min / 1000.0 && c->lifetime_max == life_max / 1000.0, "Addition: life midpoints 400/800 ms");
    check(c->rotation_speed_min == (rot_min * deg_to_rad) / lifetime_mid,
          "Addition: rotationSpeedMin = (20 * DEG_TO_RAD) / 0.6 (got " + std::to_string(c->rotation_speed_min) + ")");
    check(c->rotation_speed_max == (rot_max * deg_to_rad) / lifetime_mid,
          "Addition: rotationSpeedMax = (120 * DEG_TO_RAD) / 0.6 (got " + std::to_string(c->rotation_speed_max) + ")");
    // An inactive Rotation section reads as [0, 0] whatever its range.
    const auto off = pf::parse_libgdx_particle(replace(text, "Rotation\nactive: true", "Rotation\nactive: false"));
    check(off->rotation_speed_min == 0.0 && off->rotation_speed_max == 0.0, "Addition: inactive rotation ignores its range");
  });
  guarded("Addition: defaults for a bare emitter", [] {
    // Every section absent: Duration lowMin/lowMax 3000 with highs 0 -> [0, 3000] -> mid 1500 ms (finite 1.5 s),
    // Life 1000..0 -> [0, 1] s, Velocity 0..100 active,
    // Angle 0..360 active -> mid 180, Scale 0..32, one transparency scaling of 1 -> alphaStart = alphaEnd = 1.
    const auto result = pf::parse_libgdx_particle_document(String("Particle Effect\n- Bare -\n"));
    const auto c = result->config;
    check(result->document->name == String("Bare"), "Addition: bare emitter name");
    check(c->max_particles == 100.0, "Addition: default maxParticleCount 100");
    check(c->loop == false && c->duration == 1.5, "Addition: default duration midpoint 1500 ms");
    check(c->lifetime_min == 0.0 && c->lifetime_max == 1.0, "Addition: default life [0, 1]");
    check(c->speed_min == 0.0 && c->speed_max == 100.0, "Addition: default velocity [0, 100]");
    check(c->spread == 180.0 * deg_to_rad, "Addition: default angle spread is 180 deg");
    check(near(c->direction_x, -1.0, 1e-12) && near(c->direction_y, 0.0, 1e-12), "Addition: default angle mid 180 deg");
    check(c->scale_min == 0.0 && c->scale_max == 32.0 && c->scale_end == 1.0, "Addition: default scale");
    check(c->alpha_start == 1.0 && c->alpha_end == 1.0, "Addition: default transparency [1]");
    check(c->color_start_r == 1.0 && c->color_end_b == 1.0, "Addition: default tint ffffff");
    check(opt_eq(c->blend_mode, "normal"), "Addition: default blend normal");
    check(result->document->min_particle_count == 4.0, "Addition: default minParticleCount 4");
    check(strings_eq(result->document->tint->colors, {"ffffff"}), "Addition: default tint colors");
  });
}

void parse_libgdx_particle_document() {
  guarded("returns the same config values", [] {
    const auto config = pf::parse_libgdx_particle(SPARK_P);
    const auto loaded = pf::parse_libgdx_particle_document(SPARK_P)->config;
    check(loaded->max_particles == config->max_particles, "returns the same config values: maxParticles");
    check(close(loaded->speed_max, config->speed_max, 3), "returns the same config values: speedMax");
  });
  guarded("preserves emitter name", [] {
    check(pf::parse_libgdx_particle_document(SPARK_P)->document->name == String("Spark"), "preserves emitter name in document");
  });
  guarded("preserves imagePath", [] {
    check(pf::parse_libgdx_particle_document(SPARK_P)->document->image_path == String("spark.png"), "preserves imagePath in document");
  });
  guarded("Addition: document fields", [] {
    const auto d = pf::parse_libgdx_particle_document(SPARK_P)->document;
    check(d->min_particle_count == 4.0 && d->max_particle_count == 100.0, "Addition: particle counts");
    check(d->additive == true && d->premultiplied_alpha == false && d->image_count == 1.0, "Addition: root flags");
    check(d->emission->low_min == 0.0 && d->emission->high_min == 80.0 && d->emission->high_max == 120.0, "Addition: emission range kept");
    check(doubles_eq(d->scale->scaling, {1, 0.25}) && doubles_eq(d->scale->timeline, {0, 1}), "Addition: scale scaling/timeline");
    check(strings_eq(d->tint->colors, {"ffaa00", "ff0000"}) && doubles_eq(d->tint->timeline, {0, 1}), "Addition: tint colors/timeline");
    check(doubles_eq(d->transparency->scaling, {1, 0}), "Addition: transparency scaling");
    check(d->spawn_shape->shape == String("point") && d->spawn_shape->edges == false && d->spawn_shape->side == String("both"),
          "Addition: spawn shape");
    check(d->velocity->active && d->angle->active && !d->rotation->active && !d->delay->active, "Addition: active flags");
    check(d->life->low_min == 400.0 && d->life->high_max == 800.0 && d->life->relative == false, "Addition: life range");
    // Active-flagged ranges are `{ active, ...readRange(section) }`: the spread range members must be present.
    check(d->velocity->low_min == 50.0 && d->velocity->high_max == 200.0, "Addition: velocity range members kept");
    check(d->angle->low_min == 60.0 && d->angle->high_max == 120.0, "Addition: angle range members kept");
    check(doubles_eq(d->delay->scaling, {1}) && doubles_eq(d->delay->timeline, {0}), "Addition: delay scaling/timeline kept");
  });
  guarded("reports only emission-unsupported", [] {
    const auto diagnostics = pf::parse_libgdx_particle_document(SPARK_P)->diagnostics;
    check(kinds(diagnostics) == std::vector<std::string>{"libgdx.emission-unsupported"},
          "reports only libgdx.emission-unsupported for a standard point emitter (got " + joined(kinds(diagnostics)) + ")");
    if (diagnostics.size() > 0) {
      check(diagnostics.element(0)->severity == recover(), "emission-unsupported severity is Recover");
      check(diagnostics.element(0)->origin == String("collectLibgdxDiagnostics"), "emission-unsupported origin");
      // Addition: a direct report carries no detail.
      check(!diagnostics.element(0)->detail.has_value(), "Addition: emission-unsupported carries no detail");
    }
  });
  guarded("still reports emission-unsupported for zero rate", [] {
    const auto result = pf::parse_libgdx_particle_document(replace(SPARK_P, "highMin: 80\nhighMax: 120", "highMin: 0\nhighMax: 0"));
    check(result->config->spawn_rate != 0.0, "zero-rate emission: Flight substituted its default rate");
    check(kinds(result->diagnostics) == std::vector<std::string>{"libgdx.emission-unsupported"},
          "zero-rate emission still reports only emission-unsupported (got " + joined(kinds(result->diagnostics)) + ")");
    if (result->diagnostics.size() > 0) check(result->diagnostics.element(0)->severity == recover(), "zero-rate severity is Recover");
    // Addition: and the zero-rate document still keeps the authored zeros.
    check(result->document->emission->high_min == 0.0 && result->document->emission->high_max == 0.0, "Addition: zero emission kept in document");
  });
  expect_diagnostic("reports libgdx.delay-unsupported (Skip) when delay is active",
                    replace(SPARK_P, "Delay\nactive: false", "Delay\nactive: true"), "libgdx.delay-unsupported", skip());
  expect_diagnostic("reports libgdx.life-offset-unsupported (Skip) when lifeOffset is active",
                    replace(SPARK_P, "Life Offset\nactive: false", "Life Offset\nactive: true"), "libgdx.life-offset-unsupported", skip());
  expect_diagnostic("reports libgdx.position-offset-unsupported (Skip) when an x/y offset is active",
                    replace(SPARK_P, "X Offset\nactive: false", "X Offset\nactive: true"), "libgdx.position-offset-unsupported", skip());
  expect_diagnostic("reports libgdx.premultiplied-alpha-informational (Skip) when the flag is set",
                    replace(SPARK_P, "maxParticleCount: 100", "maxParticleCount: 100\npremultipliedAlpha: true"),
                    "libgdx.premultiplied-alpha-informational", skip());
  expect_diagnostic("reports libgdx.line-shape-mapped-to-point (Recover) when the spawn shape is line",
                    replace(SPARK_P, "shape: point", "shape: line"), "libgdx.line-shape-mapped-to-point", recover());
  expect_diagnostic("reports libgdx.tint-reduced (Recover) for a multi-stop tint (>2 colors)",
                    replace(SPARK_P, "colors: ffaa00,ff0000", "colors: ff0000,00ff00,0000ff"), "libgdx.tint-reduced", recover());
  expect_diagnostic("reports libgdx.transparency-reduced (Recover) for a multi-stop transparency (>2 stops)",
                    replace(SPARK_P, "highMax: 1\nrelative: false\nscalingCount: 2\nscaling0: 1\nscaling1: 0",
                            "highMax: 1\nrelative: false\nscalingCount: 3\nscaling0: 1\nscaling1: 0.5\nscaling2: 0"),
                    "libgdx.transparency-reduced", recover());
  expect_diagnostic("reports libgdx.spawn-edges-unsupported (Skip) when edge spawning is enabled",
                    replace(SPARK_P, "shape: point", "shape: point\nedges: true"), "libgdx.spawn-edges-unsupported", skip());

  guarded("Addition: diagnostic order", [] {
    // collectLibgdxDiagnostics reports in source order: delay, ..., line shape, emission (always), tint.
    String text = replace(SPARK_P, "Delay\nactive: false", "Delay\nactive: true");
    text = replace(text, "shape: point", "shape: line");
    text = replace(text, "colors: ffaa00,ff0000", "colors: ff0000,00ff00,0000ff");
    const auto result = pf::parse_libgdx_particle_document(text);
    const std::vector<std::string> expected = {"libgdx.delay-unsupported", "libgdx.line-shape-mapped-to-point",
                                               "libgdx.emission-unsupported", "libgdx.tint-reduced"};
    check(kinds(result->diagnostics) == expected, "Addition: diagnostics in source order (got " + joined(kinds(result->diagnostics)) + ")");
    if (result->diagnostics.size() == 4) {
      check(result->diagnostics.element(0)->severity == skip() && result->diagnostics.element(1)->severity == recover() &&
                result->diagnostics.element(2)->severity == recover() && result->diagnostics.element(3)->severity == recover(),
            "Addition: severities Skip, Recover, Recover, Recover");
    }
    // The 3-color tint keeps only the first and last stops.
    check(result->config->color_start_r == 1.0 && result->config->color_start_g == 0.0 && result->config->color_end_b == 1.0 &&
              result->config->color_end_r == 0.0,
          "Addition: reduced tint uses the first and last stops");
  });
  guarded("Addition: side top reports spawn-edges", [] {
    const auto result = pf::parse_libgdx_particle_document(replace(SPARK_P, "shape: point", "shape: point\nside: top"));
    check(find_kind(result->diagnostics, "libgdx.spawn-edges-unsupported") != nullptr, "Addition: side top reports spawn-edges-unsupported");
    check(result->document->spawn_shape->side == String("top"), "Addition: side top is kept in the document");
  });
  // Addition: the document variant rejects empty input with the same message.
  {
    std::string message;
    try {
      static_cast<void>(pf::parse_libgdx_particle_document(String("\n\t ")));
    } catch (const std::exception& e) {
      message = e.what();
    }
    check(message == "Invalid libGDX particle: input is empty or not a string", "Addition: document parse rejects blank input (got '" + message + "')");
  }
}

// One range block exactly as rangeToLines writes it, followed by the blank separator line.
std::string block(const std::string& section, const char* active, const std::string& low_min, const std::string& low_max,
                  const std::string& high_min, const std::string& high_max, std::vector<std::string> scaling,
                  std::vector<std::string> timeline) {
  std::string out = section + "\n";
  if (active) out += std::string("active: ") + active + "\n";
  out += "lowMin: " + low_min + "\nlowMax: " + low_max + "\nhighMin: " + high_min + "\nhighMax: " + high_max + "\n";
  out += "relative: false\nscalingCount: " + std::to_string(scaling.size()) + "\n";
  for (std::size_t i = 0; i < scaling.size(); ++i) out += "scaling" + std::to_string(i) + ": " + scaling[i] + "\n";
  out += "timelineCount: " + std::to_string(timeline.size()) + "\n";
  for (std::size_t i = 0; i < timeline.size(); ++i) out += "timeline" + std::to_string(i) + ": " + timeline[i] + "\n";
  return out + "\n";
}

void serialize_libgdx_particle() {
  guarded("round-trips key config fields", [] {
    const auto config = pf::parse_libgdx_particle(SPARK_P);
    const auto document = pf::parse_libgdx_particle_document(SPARK_P)->document;
    const auto text = serialize(config, as_partial(document));
    const auto config2 = pf::parse_libgdx_particle(text);
    check(config2->max_particles == config->max_particles, "round-trips maxParticles");
    check(close(config2->speed_min, config->speed_min, 1), "round-trips speedMin");
    check(close(config2->speed_max, config->speed_max, 1), "round-trips speedMax");
    check(close(config2->lifetime_min, config->lifetime_min, 2), "round-trips lifetimeMin");
    check(close(config2->lifetime_max, config->lifetime_max, 2), "round-trips lifetimeMax");
    // Addition: everything else configToDocument writes comes back.
    check(opt_eq(config2->blend_mode, "add"), "Addition: round-trips blendMode add");
    check(config2->speed_min == 50.0 && config2->speed_max == 200.0, "Addition: round-trips speed exactly 50..200");
    check(config2->scale_end == 0.25 && config2->scale_min == 16.0 && config2->scale_max == 32.0, "Addition: round-trips scale");
    check(config2->alpha_start == 1.0 && config2->alpha_end == 0.0, "Addition: round-trips alpha");
    check(config2->color_start_g == 170.0 / 255.0 && config2->color_end_r == 1.0 && config2->color_end_g == 0.0, "Addition: round-trips colors");
    check(config2->loop == false && config2->duration == 3.0, "Addition: round-trips finite duration");
    check(close(config2->spread, config->spread, 9) && close(config2->direction_y, config->direction_y, 9), "Addition: round-trips angle");
    // Header and tint lines are exact: the existing document supplies the name, minParticleCount and imagePath,
    // imageCount 1 is omitted, and rgbToHex(1, 170/255, 0) is ffaa00.
    check(contains(text, "Particle Effect\n- Spark -\nminParticleCount: 4\nmaxParticleCount: 100\nadditive: true\nimagePath: spark.png\n\nDelay\n"),
          "Addition: exact header lines");
    check(contains(text, "Tint\ncolors: ffaa00,ff0000\ntimelineCount: 2\ntimeline0: 0\ntimeline1: 1\n"), "Addition: exact tint lines");
    // The authored Emission range is replaced by makeConstRange(spawnRate) = 10.
    check(contains(text, "Emission\nlowMin: 10\nlowMax: 10\nhighMin: 10\nhighMax: 10\n"), "Addition: emission written from spawnRate");
  });
  guarded("preserves imagePath in round-trip", [] {
    const auto config = pf::parse_libgdx_particle(SPARK_P);
    const auto document = pf::parse_libgdx_particle_document(SPARK_P)->document;
    const auto text = serialize(config, as_partial(document));
    check(pf::parse_libgdx_particle_document(text)->document->image_path == String("spark.png"), "preserves imagePath in round-trip");
    // Addition: and the emitter name.
    check(pf::parse_libgdx_particle_document(text)->document->name == String("Spark"), "Addition: preserves emitter name in round-trip");
  });
  guarded("produces text readable by parseLibgdxParticle", [] {
    const auto config = make_config([](ConfigInput& c) {
      c.max_particles = 50.0;
      c.blend_mode = String("add");
    });
    const auto c2 = pf::parse_libgdx_particle(serialize(config));
    check(c2->max_particles == 50.0, "produces text readable by parseLibgdxParticle: maxParticles");
    check(opt_eq(c2->blend_mode, "add"), "produces text readable by parseLibgdxParticle: blendMode");
  });
  guarded("maps blendMode add", [] {
    const auto text = serialize(make_config([](ConfigInput& c) { c.blend_mode = String("add"); }));
    check(contains(text, "additive: true"), "maps blendMode=\"add\" to additive=true");
  });
  guarded("maps blendMode normal", [] {
    const auto text = serialize(make_config([](ConfigInput& c) { c.blend_mode = String("normal"); }));
    check(contains(text, "additive: false"), "maps blendMode=\"normal\" to additive=false");
  });

  guarded("Addition: exact text", [] {
    // directionX 1, directionY 0 -> atan2(-0, 1) = -0 deg; spread 0 -> both angle ends are -0 / +0, which JS
    // template literals print as "0". loop (default true) -> Duration 3000. No existing document -> no name line,
    // minParticleCount 0, no imagePath/imageCount lines. Default config: lifetime 0.5..1, speed 50..100, scale 1,
    // white, alpha 1 -> 0, spawnRate 10.
    const auto config = make_config([](ConfigInput& c) {
      c.max_particles = 50.0;
      c.blend_mode = String("add");
      c.direction_x = 1.0;
      c.direction_y = 0.0;
      c.spread = 0.0;
    });
    const std::string zero = "0";
    std::string expected = "Particle Effect\nminParticleCount: 0\nmaxParticleCount: 50\nadditive: true\n\n";
    expected += block("Delay", "false", zero, zero, zero, zero, {"1"}, {"0"});
    expected += block("Duration", nullptr, "3000", "3000", "3000", "3000", {"1"}, {"0"});
    expected += block("Emission", nullptr, "10", "10", "10", "10", {"1"}, {"0"});
    expected += block("Life", nullptr, "500", "500", "1000", "1000", {"1"}, {"0"});
    expected += block("Life Offset", "false", zero, zero, zero, zero, {"1"}, {"0"});
    expected += block("X Offset", "false", zero, zero, zero, zero, {"1"}, {"0"});
    expected += block("Y Offset", "false", zero, zero, zero, zero, {"1"}, {"0"});
    expected += "Spawn Shape\nshape: point\n\n";
    expected += block("Spawn Width", nullptr, zero, zero, zero, zero, {"1"}, {"0"});
    expected += block("Spawn Height", nullptr, zero, zero, zero, zero, {"1"}, {"0"});
    expected += block("Scale", nullptr, "1", "1", "1", "1", {"1", "1"}, {"0", "1"});
    expected += block("Velocity", "true", "50", "50", "100", "100", {"1"}, {"0"});
    expected += block("Angle", "true", zero, zero, zero, zero, {"1"}, {"0"});
    expected += block("Rotation", "false", zero, zero, zero, zero, {"1"}, {"0"});
    expected += block("Wind", "false", zero, zero, zero, zero, {"1"}, {"0"});
    expected += block("Gravity", "false", zero, zero, zero, zero, {"1"}, {"0"});
    expected += "Tint\ncolors: ffffff,ffffff\ntimelineCount: 2\ntimeline0: 0\ntimeline1: 1\n\n";
    // The final '' line makes the join end in exactly one newline after the Transparency block.
    std::string transparency = block("Transparency", nullptr, "0", "0", "1", "1", {"1", "0"}, {"0", "1"});
    expected += transparency.substr(0, transparency.size() - 1);
    const std::string actual = str(serialize(config));
    check(actual == expected, "Addition: exact serialized text for a small config");
    if (actual != expected) std::cerr << "--- expected\n" << expected << "--- actual\n" << actual << "---\n";
  });
  guarded("Addition: circle spawn shape and textureSize", [] {
    const auto config = make_config([](ConfigInput& c) {
      c.emitter_shape = String("circle");
      c.emitter_radius = 20.0;
      c.scale_min = 0.5;
      c.scale_max = 1.0;
    });
    const auto text = serialize(config, std::nullopt,
                                flight::make_ref<types::LibgdxSerializeOptions>(types::LibgdxSerializeOptions{.texture_size = 32.0}));
    check(contains(text, "Spawn Shape\nshape: ellipse\nedges: false\nside: both\n\nSpawn Width\nlowMin: 40\nlowMax: 40\nhighMin: 40\nhighMax: 40\n"),
          "Addition: circle writes an ellipse of diameter 40");
    check(contains(text, "Scale\nlowMin: 16\nlowMax: 16\nhighMin: 32\nhighMax: 32\n"), "Addition: scale multiplied by textureSize 32");
    const auto back = pf::parse_libgdx_particle(text, parse_options(32));
    check(back->emitter_shape == String("circle") && back->emitter_radius == 20.0, "Addition: circle round-trips");
    check(back->scale_min == 0.5 && back->scale_max == 1.0, "Addition: scale round-trips through textureSize");
  });
}

void serialize_libgdx_particle_document() {
  guarded("returns text and empty warnings", [] {
    const auto result = serialize_document(make_config());
    check(contains(result->text, "Particle Effect"), "returns text and empty warnings for a default config: text");
    check(result->warnings.size() == 0, "returns text and empty warnings for a default config: warnings");
  });
  guarded("warns for multiply", [] {
    const auto result = serialize_document(make_config([](ConfigInput& c) { c.blend_mode = String("multiply"); }));
    check(any_warning(result->warnings, [](const std::string& w) { return lower(w).find("multiply") != std::string::npos; }),
          "warns when multiply/screen blend mode cannot be represented");
  });
  guarded("warns for bursts", [] {
    const auto result = serialize_document(make_config([](ConfigInput& c) { c.burst_count = 5.0; }));
    check(any_warning(result->warnings, [](const std::string& w) { return lower(w).find("burst") != std::string::npos; }),
          "warns when config has emission bursts");
  });
  guarded("warns for color start variance", [] {
    const auto result = serialize_document(make_config([](ConfigInput& c) { c.color_start_variance_r = 0.1; }));
    check(any_warning(result->warnings, [](const std::string& w) { return w.find("colorStartVariance") != std::string::npos; }),
          "warns when config has color start variance");
  });
  guarded("no warnings for add", [] {
    const auto result = serialize_document(make_config([](ConfigInput& c) { c.blend_mode = String("add"); }));
    check(!any_warning(result->warnings, [](const std::string& w) { return lower(w).find("blend") != std::string::npos; }),
          "has no warnings for add blend mode (supported)");
  });
  guarded("Addition: exact warnings in order", [] {
    const auto config = make_config([](ConfigInput& c) {
      c.blend_mode = String("multiply");
      c.color_curve = flight::Array<double>{0, 1, 1, 1};
      c.alpha_curve = flight::Array<double>{0, 1};
      c.burst_count = 5.0;
      c.color_start_variance_r = 0.1;
      c.color_end_variance_g = 0.2;
      c.velocity_inheritance = 0.5;
    });
    const auto result = serialize_document(config);
    check(strings_eq(result->warnings,
                     {"Blend mode 'multiply' has no libGDX equivalent and was approximated as 'normal'",
                      "Color curve has more than two stops; libGDX stores only start and end color",
                      "Alpha curve has more than two stops; libGDX stores only start and end alpha via transparency scaling",
                      "Emission bursts are not supported by the libGDX format and were ignored",
                      "colorStartVariance is not supported by the libGDX format and was ignored",
                      "colorEndVariance is not supported by the libGDX format and was ignored",
                      "velocityInheritance is not supported by the libGDX format and was ignored"}),
          "Addition: exact warning texts in source order");
    // The text half equals serializeLibgdxParticle's output for the same input.
    check(result->text == serialize(config), "Addition: document text equals serializeLibgdxParticle text");
  });
}

}  // namespace

int main() {
  parse_libgdx_particle();
  parse_libgdx_particle_document();
  serialize_libgdx_particle();
  serialize_libgdx_particle_document();
  return failures == 0 ? 0 : 1;
}
