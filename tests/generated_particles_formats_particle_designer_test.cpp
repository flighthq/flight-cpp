// The generated @flighthq/particles-formats Particle Designer plist codec, exercised natively:
// parseParticleDesignerPlist, parseParticleDesignerPlistDocument (config, preserved document and import
// diagnostics), serializeParticleDesignerPlist and serializeParticleDesignerPlistDocument (text and warnings).
// Ported case by case from packages/particles-formats/src/particleDesignerParse.test.ts and
// particleDesignerSerialize.test.ts; the two upstream files carry different FIRE_PLIST fixtures (the parse one
// has a DOCTYPE line), so both are kept byte-for-byte. Checks the TypeScript source implies but upstream does
// not assert are marked "Addition:" -- exact parsed values, the blend-function table, emitter-shape mapping,
// the full diagnostic list in order, `| 0` truncation, boolean and empty values falling back to defaults,
// single-pass XML unescaping, the exact serialized plist of a small config (integer vs real, JS `-0`), the
// exact escaped texture line, document round-trip equality and the exact warning texts in order.
//
// Not natively evidenced: upstream's "returns a ParticleEmitterConfig (not a Parsed object)" probes a JS object
// for a missing `document` property, and `Array.isArray(result.warnings)` probes a JS runtime type. Native
// types answer both statically, so they are carried as compile-time checks of the declared types.
#include <flight/particles/particle_emitter_config.hpp>
#include <flight/particles_formats/particle_designer_parse.hpp>
#include <flight/particles_formats/particle_designer_serialize.hpp>

#include <cctype>
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

// toBeCloseTo(x, n): |a - x| < 0.5 * 10^-n, n defaulting to 2.
bool close(double a, double b, int digits = 2) { return near(a, b, 0.5 * std::pow(10.0, -digits)); }

namespace pf = flight::particles_formats;
namespace types = flight::types;
using flight::Ref;
using flight::String;

constexpr double deg_to_rad = std::numbers::pi / 180.0;

// particleDesignerParse.test.ts FIRE_PLIST.
const String FIRE_PLIST = String(R"P(<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>maxParticles</key><integer>200</integer>
  <key>emitterType</key><integer>0</integer>
  <key>duration</key><real>-1</real>
  <key>particleLifespan</key><real>1.5</real>
  <key>particleLifespanVariance</key><real>0.5</real>
  <key>speed</key><real>100</real>
  <key>speedVariance</key><real>20</real>
  <key>angle</key><real>90</real>
  <key>angleVariance</key><real>30</real>
  <key>gravityx</key><real>0</real>
  <key>gravityy</key><real>200</real>
  <key>sourcePositionVariancex</key><real>0</real>
  <key>sourcePositionVariancey</key><real>0</real>
  <key>startParticleSize</key><real>32</real>
  <key>startParticleSizeVariance</key><real>8</real>
  <key>finishParticleSize</key><real>8</real>
  <key>finishParticleSizeVariance</key><real>0</real>
  <key>startColorRed</key><real>1</real>
  <key>startColorGreen</key><real>0.5</real>
  <key>startColorBlue</key><real>0</real>
  <key>startColorAlpha</key><real>1</real>
  <key>startColorVarianceRed</key><real>0</real>
  <key>startColorVarianceGreen</key><real>0</real>
  <key>startColorVarianceBlue</key><real>0</real>
  <key>startColorVarianceAlpha</key><real>0</real>
  <key>finishColorRed</key><real>1</real>
  <key>finishColorGreen</key><real>0</real>
  <key>finishColorBlue</key><real>0</real>
  <key>finishColorAlpha</key><real>0</real>
  <key>finishColorVarianceRed</key><real>0</real>
  <key>finishColorVarianceGreen</key><real>0</real>
  <key>finishColorVarianceBlue</key><real>0</real>
  <key>finishColorVarianceAlpha</key><real>0</real>
  <key>rotationStart</key><real>0</real>
  <key>rotationStartVariance</key><real>0</real>
  <key>rotationEnd</key><real>0</real>
  <key>rotationEndVariance</key><real>0</real>
  <key>maxRadius</key><real>0</real>
  <key>maxRadiusVariance</key><real>0</real>
  <key>minRadius</key><real>0</real>
  <key>minRadiusVariance</key><real>0</real>
  <key>rotatePerSecond</key><real>0</real>
  <key>rotatePerSecondVariance</key><real>0</real>
  <key>blendFuncSource</key><integer>770</integer>
  <key>blendFuncDestination</key><integer>771</integer>
  <key>textureFileName</key><string>fire.png</string>
</dict>
</plist>)P");

// particleDesignerSerialize.test.ts FIRE_PLIST (no DOCTYPE line).
const String FIRE_PLIST_SERIALIZE = String(R"P(<?xml version="1.0" encoding="utf-8"?>
<plist version="1.0">
<dict>
  <key>maxParticles</key><integer>200</integer>
  <key>emitterType</key><integer>0</integer>
  <key>duration</key><real>-1</real>
  <key>particleLifespan</key><real>1.5</real>
  <key>particleLifespanVariance</key><real>0.5</real>
  <key>speed</key><real>100</real>
  <key>speedVariance</key><real>20</real>
  <key>angle</key><real>90</real>
  <key>angleVariance</key><real>30</real>
  <key>gravityx</key><real>0</real>
  <key>gravityy</key><real>200</real>
  <key>sourcePositionVariancex</key><real>0</real>
  <key>sourcePositionVariancey</key><real>0</real>
  <key>startParticleSize</key><real>32</real>
  <key>startParticleSizeVariance</key><real>8</real>
  <key>finishParticleSize</key><real>8</real>
  <key>finishParticleSizeVariance</key><real>0</real>
  <key>startColorRed</key><real>1</real>
  <key>startColorGreen</key><real>0.5</real>
  <key>startColorBlue</key><real>0</real>
  <key>startColorAlpha</key><real>1</real>
  <key>startColorVarianceRed</key><real>0</real>
  <key>startColorVarianceGreen</key><real>0</real>
  <key>startColorVarianceBlue</key><real>0</real>
  <key>startColorVarianceAlpha</key><real>0</real>
  <key>finishColorRed</key><real>1</real>
  <key>finishColorGreen</key><real>0</real>
  <key>finishColorBlue</key><real>0</real>
  <key>finishColorAlpha</key><real>0</real>
  <key>finishColorVarianceRed</key><real>0</real>
  <key>finishColorVarianceGreen</key><real>0</real>
  <key>finishColorVarianceBlue</key><real>0</real>
  <key>finishColorVarianceAlpha</key><real>0</real>
  <key>rotationStart</key><real>0</real>
  <key>rotationStartVariance</key><real>0</real>
  <key>rotationEnd</key><real>0</real>
  <key>rotationEndVariance</key><real>0</real>
  <key>maxRadius</key><real>0</real>
  <key>maxRadiusVariance</key><real>0</real>
  <key>minRadius</key><real>0</real>
  <key>minRadiusVariance</key><real>0</real>
  <key>rotatePerSecond</key><real>0</real>
  <key>rotatePerSecondVariance</key><real>0</real>
  <key>blendFuncSource</key><integer>770</integer>
  <key>blendFuncDestination</key><integer>771</integer>
  <key>textureFileName</key><string>fire.png</string>
</dict>
</plist>)P");

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

bool opt_eq(const std::optional<String>& value, const char* expected) {
  return value.has_value() && value.value() == String(expected);
}

std::string opt_str(const std::optional<String>& value) { return value.has_value() ? value->to_utf8() : "<null>"; }

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

Ref<types::ParticleDesignerParseOptions> parse_options(double texture_size) {
  return flight::make_ref<types::ParticleDesignerParseOptions>(types::ParticleDesignerParseOptions{.texture_size = texture_size});
}

Ref<types::ParticleDesignerSerializeOptions> serialize_options(double texture_size) {
  return flight::make_ref<types::ParticleDesignerSerializeOptions>(types::ParticleDesignerSerializeOptions{.texture_size = texture_size});
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

// The anonymous Partial<ParticleDesignerDocument> the serializer accepts, recovered from its signature.
template <typename>
struct SecondParameter;
template <typename Result, typename A, typename B, typename C>
struct SecondParameter<Result (*)(A, B, C)> {
  using type = B;
};
using PartialDocument = SecondParameter<decltype(&pf::serialize_particle_designer_plist)>::type::value_type::element_type;

// TypeScript passes the full parsed document where Partial<ParticleDesignerDocument> is expected; natively
// that is every member present.
Ref<PartialDocument> as_partial(const Ref<types::ParticleDesignerDocument>& d) {
  PartialDocument p;
  p.max_particles = d->max_particles;
  p.emitter_type = d->emitter_type;
  p.duration = d->duration;
  p.particle_lifespan = d->particle_lifespan;
  p.particle_lifespan_variance = d->particle_lifespan_variance;
  p.speed = d->speed;
  p.speed_variance = d->speed_variance;
  p.angle = d->angle;
  p.angle_variance = d->angle_variance;
  p.gravityx = d->gravityx;
  p.gravityy = d->gravityy;
  p.source_position_variancex = d->source_position_variancex;
  p.source_position_variancey = d->source_position_variancey;
  p.start_particle_size = d->start_particle_size;
  p.start_particle_size_variance = d->start_particle_size_variance;
  p.finish_particle_size = d->finish_particle_size;
  p.finish_particle_size_variance = d->finish_particle_size_variance;
  p.start_color_red = d->start_color_red;
  p.start_color_green = d->start_color_green;
  p.start_color_blue = d->start_color_blue;
  p.start_color_alpha = d->start_color_alpha;
  p.start_color_variance_red = d->start_color_variance_red;
  p.start_color_variance_green = d->start_color_variance_green;
  p.start_color_variance_blue = d->start_color_variance_blue;
  p.start_color_variance_alpha = d->start_color_variance_alpha;
  p.finish_color_red = d->finish_color_red;
  p.finish_color_green = d->finish_color_green;
  p.finish_color_blue = d->finish_color_blue;
  p.finish_color_alpha = d->finish_color_alpha;
  p.finish_color_variance_red = d->finish_color_variance_red;
  p.finish_color_variance_green = d->finish_color_variance_green;
  p.finish_color_variance_blue = d->finish_color_variance_blue;
  p.finish_color_variance_alpha = d->finish_color_variance_alpha;
  p.rotation_start = d->rotation_start;
  p.rotation_start_variance = d->rotation_start_variance;
  p.rotation_end = d->rotation_end;
  p.rotation_end_variance = d->rotation_end_variance;
  p.max_radius = d->max_radius;
  p.max_radius_variance = d->max_radius_variance;
  p.min_radius = d->min_radius;
  p.min_radius_variance = d->min_radius_variance;
  p.rotate_per_second = d->rotate_per_second;
  p.rotate_per_second_variance = d->rotate_per_second_variance;
  p.blend_func_source = d->blend_func_source;
  p.blend_func_destination = d->blend_func_destination;
  p.texture_file_name = d->texture_file_name;
  return flight::make_ref<PartialDocument>(std::move(p));
}

Ref<PartialDocument> texture_only(const char* name) {
  PartialDocument p;
  p.texture_file_name = String(name);
  return flight::make_ref<PartialDocument>(std::move(p));
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using Writable = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Type>>>>;
  using Readonly = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return flight::structural_ref_cast<Readonly>(Writable(value));
}

String serialize(const Ref<types::ParticleEmitterConfig>& config, std::optional<Ref<PartialDocument>> existing = std::nullopt,
                 std::optional<Ref<types::ParticleDesignerSerializeOptions>> options = std::nullopt) {
  return pf::serialize_particle_designer_plist(readonly(config), existing, options);
}

Ref<types::ParticleSerializeResult> serialize_document(const Ref<types::ParticleEmitterConfig>& config,
                                                       std::optional<Ref<PartialDocument>> existing = std::nullopt) {
  return pf::serialize_particle_designer_plist_document(readonly(config), existing, std::nullopt);
}

std::string lower(std::string s) {
  for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool any_warning(const flight::Array<String>& warnings, const std::function<bool(const std::string&)>& predicate) {
  for (std::size_t i = 0; i < warnings.size(); ++i) {
    if (predicate(warnings.element(static_cast<double>(i)).to_utf8())) return true;
  }
  return false;
}

void expect_diagnostic(const std::string& test, const String& plist, const std::string& kind, const String& severity) {
  guarded(test, [&] {
    const auto diagnostics = pf::parse_particle_designer_plist_document(plist)->diagnostics;
    const auto crumb = find_kind(diagnostics, kind);
    check(crumb != nullptr, test + ": diagnostic is defined (got " + joined(kinds(diagnostics)) + ")");
    if (crumb == nullptr) return;
    check(crumb->severity == severity, test + ": severity " + crumb->severity.to_utf8());
    check(crumb->origin == String("collectParticleDesignerDiagnostics"), test + ": origin " + crumb->origin.to_utf8());
  });
}

void parse_particle_designer_plist() {
  // describe('color variance and blend mode')
  guarded("maps startColorVariance", [] {
    const auto c = pf::parse_particle_designer_plist(replace(FIRE_PLIST, "<key>startColorVarianceRed</key><real>0</real>",
                                                             "<key>startColorVarianceRed</key><real>0.3</real>"));
    check(close(c->color_start_variance_r, 0.3), "maps startColorVariance fields to colorStartVariance");
    // Addition: the parsed real is carried through unchanged.
    check(c->color_start_variance_r == 0.3 && c->color_start_variance_g == 0.0, "Addition: colorStartVariance is exact");
  });
  guarded("maps additive blend func", [] {
    const auto c = pf::parse_particle_designer_plist(replace(FIRE_PLIST, "<key>blendFuncDestination</key><integer>771</integer>",
                                                             "<key>blendFuncDestination</key><integer>1</integer>"));
    check(opt_eq(c->blend_mode, "add"), "maps additive blend func (770, 1) to blendMode=\"add\" (got " + opt_str(c->blend_mode) + ")");
  });
  guarded("round-trips color variance through serialize", [] {
    const String modified = replace(FIRE_PLIST, "<key>startColorVarianceGreen</key><real>0</real>",
                                    "<key>startColorVarianceGreen</key><real>0.2</real>");
    const auto config = pf::parse_particle_designer_plist(modified);
    const auto document = pf::parse_particle_designer_plist_document(modified)->document;
    const auto xml = serialize(config, as_partial(document));
    const auto config2 = pf::parse_particle_designer_plist(xml);
    check(close(config2->color_start_variance_g, 0.2, 3), "round-trips color variance through serialize");
    // Addition: the variance is written as the exact real.
    check(contains(xml, "\t<key>startColorVarianceGreen</key>\n\t<real>0.2</real>\n"), "Addition: variance written as <real>0.2</real>");
  });

  // describe('lightweight, returns config directly')
  guarded("returns a ParticleEmitterConfig", [] {
    static_assert(std::is_same_v<decltype(pf::parse_particle_designer_plist(FIRE_PLIST)), Ref<types::ParticleEmitterConfig>>);
    check(pf::parse_particle_designer_plist(FIRE_PLIST)->max_particles == 200.0, "returns a ParticleEmitterConfig (not a Parsed object)");
  });
  guarded("parses maxParticles", [] {
    check(pf::parse_particle_designer_plist(FIRE_PLIST)->max_particles == 200.0, "parses maxParticles");
  });
  guarded("converts lifetime", [] {
    const auto c = pf::parse_particle_designer_plist(FIRE_PLIST);
    check(close(c->lifetime_min, 1.0), "converts lifetime with variance to min/max: lifetimeMin");
    check(close(c->lifetime_max, 2.0), "converts lifetime with variance to min/max: lifetimeMax");
    // Addition: max(0, 1.5 - 0.5) and 1.5 + 0.5 are exact.
    check(c->lifetime_min == 1.0 && c->lifetime_max == 2.0, "Addition: lifetime is exact");
  });
  guarded("converts speed", [] {
    const auto c = pf::parse_particle_designer_plist(FIRE_PLIST);
    check(close(c->speed_min, 80), "converts speed with variance to min/max: speedMin");
    check(close(c->speed_max, 120), "converts speed with variance to min/max: speedMax");
    check(c->speed_min == 80.0 && c->speed_max == 120.0, "Addition: speed is exact");
  });
  guarded("maps angle=90", [] {
    const auto c = pf::parse_particle_designer_plist(FIRE_PLIST);
    check(close(c->direction_x, 0, 3), "maps angle=90 to upward direction: directionX");
    check(close(c->direction_y, -1, 3), "maps angle=90 to upward direction: directionY");
  });
  guarded("converts angleVariance", [] {
    const auto c = pf::parse_particle_designer_plist(FIRE_PLIST);
    check(close(c->spread, (30 * std::numbers::pi) / 180, 4), "converts angleVariance to spread in radians");
    // Addition: spread = angleVariance * DEG_TO_RAD with DEG_TO_RAD = PI / 180.
    check(c->spread == 30.0 * deg_to_rad, "Addition: spread is exactly 30 * DEG_TO_RAD");
  });
  guarded("maps gravity", [] {
    const auto c = pf::parse_particle_designer_plist(FIRE_PLIST);
    check(close(c->gravity_x, 0), "maps gravity directly: gravityX");
    check(close(c->gravity_y, 200), "maps gravity directly: gravityY");
    check(c->gravity_x == 0.0 && c->gravity_y == 200.0, "Addition: gravity is exact");
  });
  guarded("maps color start and end", [] {
    const auto c = pf::parse_particle_designer_plist(FIRE_PLIST);
    check(close(c->color_start_r, 1), "maps color start and end: colorStartR");
    check(close(c->color_start_g, 0.5), "maps color start and end: colorStartG");
    check(close(c->color_start_b, 0), "maps color start and end: colorStartB");
    check(close(c->color_end_r, 1), "maps color start and end: colorEndR");
    check(close(c->color_end_g, 0), "maps color start and end: colorEndG");
    check(close(c->color_end_b, 0), "maps color start and end: colorEndB");
  });
  guarded("maps alpha start and end", [] {
    const auto c = pf::parse_particle_designer_plist(FIRE_PLIST);
    check(close(c->alpha_start, 1), "maps alpha start and end: alphaStart");
    check(close(c->alpha_end, 0), "maps alpha start and end: alphaEnd");
  });
  guarded("normalises scale by textureSize", [] {
    const auto c = pf::parse_particle_designer_plist(FIRE_PLIST, parse_options(32));
    check(close(c->scale_min, 0.75), "normalises scale by textureSize: scaleMin");
    check(close(c->scale_max, 1.25), "normalises scale by textureSize: scaleMax");
    // Addition: (32 - 8) / 32 and (32 + 8) / 32 are exact.
    check(c->scale_min == 0.75 && c->scale_max == 1.25, "Addition: normalised scale is exact");
  });
  guarded("computes scaleEnd", [] {
    const auto c = pf::parse_particle_designer_plist(FIRE_PLIST, parse_options(32));
    check(close(c->scale_end, 0.25), "computes scaleEnd as finishSize / startSize");
    check(c->scale_end == 0.25, "Addition: scaleEnd is exactly (8/32) / (32/32)");
  });
  guarded("uses point emitter shape", [] {
    check(pf::parse_particle_designer_plist(FIRE_PLIST)->emitter_shape == String("point"),
          "uses point emitter shape when sourcePositionVariance is zero");
  });
  guarded("maps duration=-1", [] {
    const auto c = pf::parse_particle_designer_plist(FIRE_PLIST);
    check(c->loop == true, "maps duration=-1 to an infinite (looping) emitter: loop");
    check(c->duration == 0.0, "maps duration=-1 to an infinite (looping) emitter: duration");
  });
  guarded("maps a positive duration", [] {
    const auto c = pf::parse_particle_designer_plist(
        replace(FIRE_PLIST, "<key>duration</key><real>-1</real>", "<key>duration</key><real>2.5</real>"));
    check(c->loop == false, "maps a positive duration to a finite, non-looping emitter: loop");
    check(close(c->duration, 2.5), "maps a positive duration to a finite, non-looping emitter: duration");
  });

  // describe('malformed input')
  guarded("falls back to defaults for empty numeric tags", [] {
    const auto c = pf::parse_particle_designer_plist(
        String("<plist><dict><key>maxParticles</key><integer></integer><key>speed</key><real></real></dict></plist>"));
    check(c->max_particles == 200.0, "falls back to defaults (not NaN) for empty numeric tags: maxParticles");
    check(std::isfinite(c->speed_min), "falls back to defaults for empty numeric tags: speedMin finite");
    check(std::isfinite(c->speed_max), "falls back to defaults for empty numeric tags: speedMax finite");
    // Addition: the default speed 100 with variance 0.
    check(c->speed_min == 100.0 && c->speed_max == 100.0, "Addition: empty speed falls back to exactly 100");
  });
  {
    bool threw = false;
    try {
      static_cast<void>(pf::parse_particle_designer_plist(String("")));
    } catch (...) {
      threw = true;
    }
    check(!threw, "returns an all-defaults config for empty input without throwing");
  }
  guarded("garbage input", [] {
    const auto c = pf::parse_particle_designer_plist(String("not xml at all"));
    check(std::isfinite(c->max_particles), "returns an all-defaults config for garbage input: maxParticles finite");
    check(std::isfinite(c->gravity_y), "returns an all-defaults config for garbage input: gravityY finite");
    // Addition: every rawDictToConfig default, exactly.
    check(c->max_particles == 200.0 && c->gravity_y == 0.0, "Addition: garbage defaults maxParticles 200, gravityY 0");
    check(c->loop == true && c->duration == 0.0, "Addition: garbage defaults loop forever");
    check(c->lifetime_min == 1.0 && c->lifetime_max == 1.0 && c->speed_min == 100.0, "Addition: garbage defaults lifetime/speed");
    check(c->scale_min == 32.0 && c->scale_max == 32.0 && c->scale_end == 0.5, "Addition: garbage defaults sizes 32 -> 16");
    check(c->alpha_start == 1.0 && c->alpha_end == 0.0, "Addition: garbage defaults alpha 1 -> 0");
    check(opt_eq(c->blend_mode, "normal"), "Addition: garbage defaults blend 770/771 normal (got " + opt_str(c->blend_mode) + ")");
  });

  guarded("Addition: blend function table", [] {
    auto blend = [](const char* src, const char* dst) {
      String plist = replace(FIRE_PLIST, "<key>blendFuncSource</key><integer>770</integer>",
                             std::string("<key>blendFuncSource</key><integer>") + src + "</integer>");
      plist = replace(plist, "<key>blendFuncDestination</key><integer>771</integer>",
                      std::string("<key>blendFuncDestination</key><integer>") + dst + "</integer>");
      return pf::parse_particle_designer_plist(plist)->blend_mode;
    };
    check(opt_eq(blend("770", "771"), "normal"), "Addition: (770, 771) is normal");
    check(opt_eq(blend("1", "1"), "add"), "Addition: (1, 1) is add");
    check(opt_eq(blend("1", "771"), "normal"), "Addition: premultiplied (1, 771) is normal");
    check(!blend("0", "0").has_value(), "Addition: unknown (0, 0) is null (got " + opt_str(blend("0", "0")) + ")");
  });
  guarded("Addition: emitter shapes", [] {
    String circle = replace(FIRE_PLIST, "<key>sourcePositionVariancex</key><real>0</real>", "<key>sourcePositionVariancex</key><real>10</real>");
    circle = replace(circle, "<key>sourcePositionVariancey</key><real>0</real>", "<key>sourcePositionVariancey</key><real>10</real>");
    const auto c = pf::parse_particle_designer_plist(circle);
    check(c->emitter_shape == String("circle") && c->emitter_radius == 10.0 && c->emitter_width == 0.0,
          "Addition: equal variances map to a circle of radius vx");
    const auto r = pf::parse_particle_designer_plist(
        replace(circle, "<key>sourcePositionVariancey</key><real>10</real>", "<key>sourcePositionVariancey</key><real>5</real>"));
    check(r->emitter_shape == String("rect") && r->emitter_width == 20.0 && r->emitter_height == 10.0 && r->emitter_radius == 0.0,
          "Addition: unequal variances map to a rect of 2*vx by 2*vy");
  });
  guarded("Addition: maxParticles truncation and non-number values", [] {
    const auto t = pf::parse_particle_designer_plist(
        replace(FIRE_PLIST, "<key>maxParticles</key><integer>200</integer>", "<key>maxParticles</key><real>150.7</real>"));
    check(t->max_particles == 150.0, "Addition: maxParticles is truncated by | 0");
    // A <true/> is a boolean, not a number, so num() falls back to the default.
    const auto b = pf::parse_particle_designer_plist(
        replace(FIRE_PLIST, "<key>maxParticles</key><integer>200</integer>", "<key>maxParticles</key><true/>"));
    check(b->max_particles == 200.0, "Addition: a boolean maxParticles falls back to 200");
    check(b->speed_min == 80.0, "Addition: keys after a self-closing boolean still parse");
  });
}

void parse_particle_designer_plist_document() {
  guarded("raises no data-integrity diagnostic", [] {
    const auto diagnostics = pf::parse_particle_designer_plist_document(FIRE_PLIST, parse_options(32))->diagnostics;
    std::vector<std::string> integrity;
    for (std::size_t i = 0; i < diagnostics.size(); ++i) {
      if (diagnostics.element(static_cast<double>(i))->severity != skip()) integrity.push_back(diagnostics.element(static_cast<double>(i))->kind.to_utf8());
    }
    check(integrity.empty(), "raises no data-integrity diagnostic for a well-formed plist (got " + joined(integrity) + ")");
  });
  guarded("returns the same config values", [] {
    const auto config = pf::parse_particle_designer_plist(FIRE_PLIST, parse_options(32));
    const auto loaded = pf::parse_particle_designer_plist_document(FIRE_PLIST, parse_options(32))->config;
    check(loaded->max_particles == config->max_particles, "returns the same config values: maxParticles");
    check(close(loaded->direction_y, config->direction_y, 5), "returns the same config values: directionY");
    check(close(loaded->gravity_y, config->gravity_y, 5), "returns the same config values: gravityY");
  });
  guarded("preserves textureFileName", [] {
    check(pf::parse_particle_designer_plist_document(FIRE_PLIST)->document->texture_file_name == String("fire.png"),
          "preserves textureFileName in document");
  });
  guarded("preserves blend function", [] {
    const auto document = pf::parse_particle_designer_plist_document(FIRE_PLIST)->document;
    check(document->blend_func_source == 770.0, "preserves blend function in document: source");
    check(document->blend_func_destination == 771.0, "preserves blend function in document: destination");
  });
  guarded("Addition: document fields", [] {
    const auto d = pf::parse_particle_designer_plist_document(FIRE_PLIST)->document;
    check(d->max_particles == 200.0 && d->emitter_type == 0.0 && d->duration == -1.0, "Addition: header fields");
    check(d->particle_lifespan == 1.5 && d->particle_lifespan_variance == 0.5, "Addition: lifespan fields");
    check(d->speed == 100.0 && d->speed_variance == 20.0 && d->angle == 90.0 && d->angle_variance == 30.0, "Addition: motion fields");
    check(d->start_particle_size == 32.0 && d->start_particle_size_variance == 8.0 && d->finish_particle_size == 8.0, "Addition: size fields");
    check(d->start_color_green == 0.5 && d->finish_color_alpha == 0.0, "Addition: color fields");
  });

  // describe('import warnings')
  guarded("has no warnings for a standard gravity emitter", [] {
    const auto diagnostics = pf::parse_particle_designer_plist_document(FIRE_PLIST)->diagnostics;
    check(diagnostics.size() == 0, "has no warnings for a standard gravity emitter (got " + joined(kinds(diagnostics)) + ")");
  });
  expect_diagnostic("reports particledesigner.radial-approximated (Recover) for a radial emitter",
                    replace(FIRE_PLIST, "<key>emitterType</key><integer>0</integer>", "<key>emitterType</key><integer>1</integer>"),
                    "particledesigner.radial-approximated", recover());
  expect_diagnostic("reports particledesigner.radial-acceleration-unsupported (Skip)",
                    replace(FIRE_PLIST, "<key>maxParticles</key><integer>200</integer>",
                            "<key>maxParticles</key><integer>200</integer><key>radialAcceleration</key><real>50</real>"),
                    "particledesigner.radial-acceleration-unsupported", skip());
  expect_diagnostic("reports particledesigner.tangential-acceleration-unsupported (Skip)",
                    replace(FIRE_PLIST, "<key>maxParticles</key><integer>200</integer>",
                            "<key>maxParticles</key><integer>200</integer><key>tangentialAcceleration</key><real>50</real>"),
                    "particledesigner.tangential-acceleration-unsupported", skip());
  expect_diagnostic("reports particledesigner.finish-size-variance-unsupported (Skip)",
                    replace(FIRE_PLIST, "<key>finishParticleSizeVariance</key><real>0</real>",
                            "<key>finishParticleSizeVariance</key><real>4</real>"),
                    "particledesigner.finish-size-variance-unsupported", skip());
  expect_diagnostic("reports particledesigner.alpha-variance-unsupported (Skip)",
                    replace(FIRE_PLIST, "<key>startColorVarianceAlpha</key><real>0</real>",
                            "<key>startColorVarianceAlpha</key><real>0.5</real>"),
                    "particledesigner.alpha-variance-unsupported", skip());
  guarded("Addition: diagnostic order", [] {
    String plist = replace(FIRE_PLIST, "<key>emitterType</key><integer>0</integer>", "<key>emitterType</key><integer>1</integer>");
    plist = replace(plist, "<key>maxParticles</key><integer>200</integer>",
                    "<key>maxParticles</key><integer>200</integer><key>radialAccelVariance</key><real>5</real>"
                    "<key>tangentialAccelVariance</key><real>5</real>");
    plist = replace(plist, "<key>finishParticleSizeVariance</key><real>0</real>", "<key>finishParticleSizeVariance</key><real>4</real>");
    plist = replace(plist, "<key>finishColorVarianceAlpha</key><real>0</real>", "<key>finishColorVarianceAlpha</key><real>0.1</real>");
    const auto diagnostics = pf::parse_particle_designer_plist_document(plist)->diagnostics;
    const std::vector<std::string> expected = {"particledesigner.radial-approximated", "particledesigner.radial-acceleration-unsupported",
                                               "particledesigner.tangential-acceleration-unsupported",
                                               "particledesigner.finish-size-variance-unsupported", "particledesigner.alpha-variance-unsupported"};
    check(kinds(diagnostics) == expected, "Addition: all five diagnostics in source order (got " + joined(kinds(diagnostics)) + ")");
    if (diagnostics.size() == 5) {
      bool severities = diagnostics.element(0)->severity == recover();
      for (double i = 1; i < 5; ++i) severities = severities && diagnostics.element(i)->severity == skip();
      check(severities, "Addition: severities Recover, Skip, Skip, Skip, Skip");
    }
  });
  guarded("Addition: single-pass unescape", [] {
    // &lt; is replaced before &amp;, so "&amp;lt;" decodes once to "&lt;", never to "<".
    const auto document = pf::parse_particle_designer_plist_document(
        replace(FIRE_PLIST, "<string>fire.png</string>", "<string>a&amp;lt;b&quot;c&apos;d&gt;</string>"))->document;
    check(document->texture_file_name == String("a&lt;b\"c'd>"),
          "Addition: entities decode once (got " + document->texture_file_name.to_utf8() + ")");
  });
}

// One kv() entry exactly as documentToPlist writes it.
std::string kv(const std::string& key, const std::string& tag, const std::string& value) {
  return "\t<key>" + key + "</key>\n\t<" + tag + ">" + value + "</" + tag + ">\n";
}

void serialize_particle_designer_plist() {
  // From particleDesignerParse.test.ts describe('serializeParticleDesignerPlist').
  guarded("parse file: round-trips key config fields", [] {
    const auto config = pf::parse_particle_designer_plist(FIRE_PLIST, parse_options(32));
    const auto document = pf::parse_particle_designer_plist_document(FIRE_PLIST, parse_options(32))->document;
    const auto xml = serialize(config, as_partial(document), serialize_options(32));
    const auto config2 = pf::parse_particle_designer_plist(xml, parse_options(32));
    check(config2->max_particles == config->max_particles, "round-trips key config fields: maxParticles");
    check(close(config2->direction_x, config->direction_x, 3), "round-trips key config fields: directionX");
    check(close(config2->gravity_y, config->gravity_y, 1), "round-trips key config fields: gravityY");
    check(close(config2->color_start_r, config->color_start_r, 3), "round-trips key config fields: colorStartR");
    check(close(config2->alpha_end, config->alpha_end, 3), "round-trips key config fields: alphaEnd");
  });
  guarded("parse file: preserves textureFileName", [] {
    const auto config = pf::parse_particle_designer_plist(FIRE_PLIST);
    const auto document = pf::parse_particle_designer_plist_document(FIRE_PLIST)->document;
    check(contains(serialize(config, as_partial(document)), "fire.png"), "preserves textureFileName");
  });
  guarded("parse file: produces valid plist XML", [] {
    const auto xml = serialize(pf::parse_particle_designer_plist(FIRE_PLIST));
    check(contains(xml, "<?xml version=\"1.0\""), "produces valid plist XML: declaration");
    check(contains(xml, "<plist version=\"1.0\">"), "produces valid plist XML: plist element");
  });
  guarded("parse file: escapes and round-trips XML special characters", [] {
    const auto xml = serialize(pf::parse_particle_designer_plist(FIRE_PLIST), texture_only("a&b<c>.png"));
    check(!contains(xml, "a&b<c>"), "escapes XML special characters: raw text absent");
    check(contains(xml, "&amp;"), "escapes XML special characters: &amp; present");
    check(pf::parse_particle_designer_plist_document(xml)->document->texture_file_name == String("a&b<c>.png"),
          "escaped texture filename round-trips intact");
    // Addition: the exact escaped line.
    check(contains(xml, "\t<key>textureFileName</key>\n\t<string>a&amp;b&lt;c&gt;.png</string>\n</dict>\n</plist>"),
          "Addition: exact escaped textureFileName line");
  });

  // From particleDesignerSerialize.test.ts describe('serializeParticleDesignerPlist').
  guarded("produces valid plist XML with required structure", [] {
    const auto xml = serialize(pf::parse_particle_designer_plist(FIRE_PLIST_SERIALIZE));
    check(contains(xml, "<?xml version=\"1.0\""), "produces valid plist XML with required structure: declaration");
    check(contains(xml, "<plist version=\"1.0\">"), "produces valid plist XML with required structure: plist");
    check(contains(xml, "<dict>"), "produces valid plist XML with required structure: dict");
  });
  guarded("preserves textureFileName from existing document", [] {
    const auto config = pf::parse_particle_designer_plist(FIRE_PLIST_SERIALIZE);
    const auto document = pf::parse_particle_designer_plist_document(FIRE_PLIST_SERIALIZE)->document;
    check(contains(serialize(config, as_partial(document)), "fire.png"), "preserves textureFileName from existing document");
  });
  guarded("round-trips through parse -> serialize -> parse", [] {
    const auto config = pf::parse_particle_designer_plist(FIRE_PLIST_SERIALIZE, parse_options(32));
    const auto document = pf::parse_particle_designer_plist_document(FIRE_PLIST_SERIALIZE, parse_options(32))->document;
    const auto xml = serialize(config, as_partial(document), serialize_options(32));
    const auto config2 = pf::parse_particle_designer_plist(xml, parse_options(32));
    check(config2->max_particles == config->max_particles, "round-trips parse -> serialize -> parse: maxParticles");
    check(close(config2->direction_x, config->direction_x, 3), "round-trips parse -> serialize -> parse: directionX");
    check(close(config2->gravity_y, config->gravity_y, 1), "round-trips parse -> serialize -> parse: gravityY");
  });
  guarded("escapes XML special characters in the texture filename", [] {
    const auto xml = serialize(pf::parse_particle_designer_plist(FIRE_PLIST_SERIALIZE), texture_only("a&b<c>.png"));
    check(!contains(xml, "a&b<c>"), "escapes XML special characters in the texture filename: raw absent");
    check(contains(xml, "&amp;"), "escapes XML special characters in the texture filename: &amp; present");
  });

  guarded("Addition: document round-trip", [] {
    // configToDocument inverts rawDictToConfig for every field the config carries, and the existing document
    // supplies the rest, so a second parse sees the same document.
    const auto first = pf::parse_particle_designer_plist_document(FIRE_PLIST_SERIALIZE);
    const auto xml = serialize(first->config, as_partial(first->document));
    const auto d = pf::parse_particle_designer_plist_document(xml)->document;
    const auto o = first->document;
    check(d->max_particles == o->max_particles && d->emitter_type == o->emitter_type && d->duration == o->duration,
          "Addition: round-trip header fields");
    check(d->particle_lifespan == 1.5 && d->particle_lifespan_variance == 0.5, "Addition: round-trip lifespan");
    check(d->speed == 100.0 && d->speed_variance == 20.0, "Addition: round-trip speed");
    check(near(d->angle, 90.0, 1e-9) && near(d->angle_variance, 30.0, 1e-9), "Addition: round-trip angle");
    check(d->gravityy == 200.0 && d->source_position_variancex == 0.0, "Addition: round-trip gravity/position");
    check(d->start_particle_size == 32.0 && d->start_particle_size_variance == 8.0 && d->finish_particle_size == 8.0,
          "Addition: round-trip sizes");
    check(d->start_color_green == 0.5 && d->finish_color_red == 1.0 && d->finish_color_alpha == 0.0, "Addition: round-trip colors");
    check(d->blend_func_source == 770.0 && d->blend_func_destination == 771.0, "Addition: round-trip blend function");
    check(d->texture_file_name == String("fire.png"), "Addition: round-trip texture");
    // duration -1 and the integer-valued fields are written as <integer>, fractional ones as <real>.
    check(contains(xml, kv("duration", "integer", "-1")) && contains(xml, kv("particleLifespan", "real", "1.5")) &&
              contains(xml, kv("startColorGreen", "real", "0.5")) && contains(xml, kv("speed", "integer", "100")),
          "Addition: integer vs real tags");
  });
  guarded("Addition: exact plist", [] {
    // directionX 1, directionY 0 -> atan2(-0, 1) * RAD_TO_DEG = -0, an integer JS prints as "0". Default config
    // otherwise: lifetime 0.5..1, speed 50..100, scale 1, white, alpha 1 -> 0, blendMode null -> 770/771 fallback,
    // duration 0 with no existing document -> -1.
    const auto config = make_config([](ConfigInput& c) {
      c.max_particles = 50.0;
      c.direction_x = 1.0;
      c.direction_y = 0.0;
      c.spread = 0.0;
    });
    std::string expected =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n<dict>\n";
    const std::string i = "integer";
    expected += kv("maxParticles", i, "50") + kv("emitterType", i, "0") + kv("duration", i, "-1");
    expected += kv("particleLifespan", "real", "0.75") + kv("particleLifespanVariance", "real", "0.25");
    expected += kv("speed", i, "75") + kv("speedVariance", i, "25") + kv("angle", i, "0") + kv("angleVariance", i, "0");
    expected += kv("gravityx", i, "0") + kv("gravityy", i, "0") + kv("sourcePositionVariancex", i, "0") +
                kv("sourcePositionVariancey", i, "0");
    expected += kv("startParticleSize", i, "1") + kv("startParticleSizeVariance", i, "0") + kv("finishParticleSize", i, "1") +
                kv("finishParticleSizeVariance", i, "0");
    expected += kv("startColorRed", i, "1") + kv("startColorGreen", i, "1") + kv("startColorBlue", i, "1") + kv("startColorAlpha", i, "1");
    expected += kv("startColorVarianceRed", i, "0") + kv("startColorVarianceGreen", i, "0") + kv("startColorVarianceBlue", i, "0") +
                kv("startColorVarianceAlpha", i, "0");
    expected += kv("finishColorRed", i, "1") + kv("finishColorGreen", i, "1") + kv("finishColorBlue", i, "1") + kv("finishColorAlpha", i, "0");
    expected += kv("finishColorVarianceRed", i, "0") + kv("finishColorVarianceGreen", i, "0") + kv("finishColorVarianceBlue", i, "0") +
                kv("finishColorVarianceAlpha", i, "0");
    expected += kv("rotationStart", i, "0") + kv("rotationStartVariance", i, "0") + kv("rotationEnd", i, "0") +
                kv("rotationEndVariance", i, "0");
    expected += kv("maxRadius", i, "0") + kv("maxRadiusVariance", i, "0") + kv("minRadius", i, "0") + kv("minRadiusVariance", i, "0");
    expected += kv("rotatePerSecond", i, "0") + kv("rotatePerSecondVariance", i, "0");
    expected += kv("blendFuncSource", i, "770") + kv("blendFuncDestination", i, "771");
    expected += kv("textureFileName", "string", "");
    expected += "</dict>\n</plist>";
    const std::string actual = serialize(config).to_utf8();
    check(actual == expected, "Addition: exact serialized plist for a small config");
    if (actual != expected) std::cerr << "--- expected\n" << expected << "\n--- actual\n" << actual << "\n---\n";
  });
  guarded("Addition: blend mode and shape mapping", [] {
    const auto add = serialize(make_config([](ConfigInput& c) {
      c.blend_mode = String("add");
      c.emitter_shape = String("rect");
      c.emitter_width = 20.0;
      c.emitter_height = 10.0;
    }));
    check(contains(add, kv("blendFuncSource", "integer", "770") + kv("blendFuncDestination", "integer", "1")), "Addition: add writes 770/1");
    check(contains(add, kv("sourcePositionVariancex", "integer", "10") + kv("sourcePositionVariancey", "integer", "5")),
          "Addition: rect writes half extents");
    // An unrepresentable blend mode keeps the existing document's functions.
    PartialDocument existing;
    existing.blend_func_source = 1.0;
    existing.blend_func_destination = 771.0;
    const auto multiply = serialize(make_config([](ConfigInput& c) { c.blend_mode = String("multiply"); }),
                                    flight::make_ref<PartialDocument>(std::move(existing)));
    check(contains(multiply, kv("blendFuncSource", "integer", "1") + kv("blendFuncDestination", "integer", "771")),
          "Addition: multiply falls back to the existing blend functions");
  });
}

void serialize_particle_designer_plist_document() {
  guarded("returns { text, warnings }", [] {
    const auto result = serialize_document(pf::parse_particle_designer_plist(FIRE_PLIST_SERIALIZE));
    static_assert(std::is_same_v<decltype(result->text), String>);
    static_assert(std::is_same_v<decltype(result->warnings), flight::Array<String>>);
    check(contains(result->text, "<plist version=\"1.0\">"), "returns { text, warnings } for a standard config: text");
    check(result->warnings.size() == 0, "returns { text, warnings } for a standard config: no warnings");
  });
  guarded("warns when the config uses unsupported features", [] {
    const auto result = serialize_document(make_config([](ConfigInput& c) { c.burst_count = 5.0; }));
    check(any_warning(result->warnings, [](const std::string& w) { return lower(w).find("burst") != std::string::npos; }),
          "warns when the config uses unsupported features");
  });
  guarded("preserves textureFileName in the text output", [] {
    const auto config = pf::parse_particle_designer_plist(FIRE_PLIST_SERIALIZE);
    const auto document = pf::parse_particle_designer_plist_document(FIRE_PLIST_SERIALIZE)->document;
    check(contains(serialize_document(config, as_partial(document))->text, "fire.png"),
          "preserves textureFileName from existing document in the text output");
  });
  guarded("Addition: exact warnings in order", [] {
    const auto config = make_config([](ConfigInput& c) {
      c.blend_mode = String("screen");
      c.color_curve = flight::Array<double>{0, 1, 1, 1};
      c.alpha_curve = flight::Array<double>{0, 1};
      c.scale_curve = flight::Array<double>{0, 1};
      c.burst_count = 5.0;
      c.velocity_inheritance = 0.5;
      c.color_start_variance_r = 0.1;  // representable in Particle Designer: no warning
    });
    const auto result = serialize_document(config);
    check(strings_eq(result->warnings,
                     {"Blend mode 'screen' has no Particle Designer equivalent and was approximated as 'normal'",
                      "Color curve has more than two stops; Particle Designer stores only start and end colors",
                      "Alpha curve has more than two stops; Particle Designer stores only start and end alpha",
                      "Scale curve has more than two stops; Particle Designer stores only start and end size",
                      "Emission bursts are not supported by the Particle Designer format and were ignored",
                      "velocityInheritance is not supported by the Particle Designer format and was ignored"}),
          "Addition: exact warning texts in source order");
    check(result->text == serialize(config), "Addition: document text equals serializeParticleDesignerPlist text");
  });
}

}  // namespace

int main() {
  parse_particle_designer_plist();
  parse_particle_designer_plist_document();
  serialize_particle_designer_plist();
  serialize_particle_designer_plist_document();
  return failures == 0 ? 0 : 1;
}
