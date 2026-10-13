// The generated @flighthq/particles-formats registry, detection, unified parse and Starling PEX codec at
// a627849, exercised natively. Ported from formatRegistry.test.ts, starlingPexParse.test.ts and
// starlingPexSerialize.test.ts. detect.ts and parseParticleConfig.ts are exercised through test-local codecs: the
// built-in codec table lives in registerBuiltInParticleFormats.ts, which does not compile at this pin because it
// imports the Pixi parser (docs/compiler-requests/parsed-json-object-shape-reads.md). Upstream's
// built-in-detection cases are that residual.
//
// Additions are marked "Addition:". They cover:
// - registry order, replacement position and the codec's own identity;
// - exact diagnostic order, severity, origin and detail;
// - byte-exact PEX text for a default config;
// - escaping of the texture name;
// - parse round trips of serialized output;
// - malformed-input failures with their exact messages;
// - the full Spine and Unity serialize warning lists.
//
// Not natively evidenced: the Spine and Unity serialized text, which is `{}`
// (docs/compiler-requests/json-stringify-nominal-ref-empty-object.md).
#include <flight/particles/particle_emitter_config.hpp>
#include <flight/particles_formats/detect.hpp>
#include <flight/particles_formats/format_registry.hpp>
#include <flight/particles_formats/parse_particle_config.hpp>
#include <flight/particles_formats/starling_pex_parse.hpp>
#include <flight/particles_formats/spine_serialize.hpp>
#include <flight/particles_formats/starling_pex_serialize.hpp>
#include <flight/particles_formats/unity_serialize.hpp>

#include <cmath>
#include <functional>
#include <iostream>
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

namespace pf = flight::particles_formats;
namespace types = flight::types;
using flight::Ref;
using flight::String;

// The anonymous partial shapes the emitted signatures take, recovered from those signatures.
template <typename>
struct Parameters;
template <typename Result, typename... Args>
struct Parameters<Result (*)(Args...)> {
  template <std::size_t I>
  using at = std::tuple_element_t<I, std::tuple<Args...>>;
};
using ConfigInputRef = Parameters<decltype(&flight::particles::create_particle_emitter_config)>::at<0>::value_type;
using ConfigInput = ConfigInputRef::element_type;
using PexExistingRef = Parameters<decltype(&pf::serialize_starling_pex)>::at<1>::value_type;
using PexSerializeOptionsRef = Parameters<decltype(&pf::serialize_starling_pex)>::at<2>::value_type;

Ref<types::ParticleEmitterConfig> config(const std::function<void(ConfigInput&)>& fill = [](ConfigInput&) {}) {
  ConfigInput input;
  fill(input);
  return flight::particles::create_particle_emitter_config(flight::make_ref<ConfigInput>(std::move(input)));
}

std::string text(const String& s) { return s.to_utf8(); }
bool contains(const String& haystack, const std::string& needle) { return text(haystack).find(needle) != std::string::npos; }

std::string replace_once(std::string s, const std::string& from, const std::string& to) {
  const auto at = s.find(from);
  if (at == std::string::npos) return "<replace target missing: " + from + ">";
  return s.replace(at, from.size(), to);
}

const types::ImportDiagnosticSeverity& severity(const char* name) {
  const auto& s = types::import_diagnostic_severity;
  if (std::string(name) == "recover") return s->recover;
  if (std::string(name) == "reject") return s->reject;
  if (std::string(name) == "skip") return s->skip;
  return s->drop;
}

std::optional<Ref<types::ImportDiagnostic>> find_kind(const flight::Array<Ref<types::ImportDiagnostic>>& diagnostics, const char* kind) {
  for (const auto& d : diagnostics)
    if (d->kind == String(kind)) return d;
  return std::nullopt;
}

// --- registry -------------------------------------------------------------------------------------------------

const String test_kind("test.MyFormat");

using ParseOptionsView = std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<Ref<types::ParseParticleConfigOptions>>>>>;

double max_particles_of(const String& t) {
  const double parsed = flight::parse_int(t.slice(10), 10.0);
  return flight::to_boolean(parsed) ? parsed : 50.0;
}

// upstream's testCodec.
Ref<types::ParticleFormatCodec> make_codec(std::function<bool(String)> detect = [](String t) { return t.starts_with(String("MY_FORMAT:")); }) {
  using Document = std::remove_cvref_t<decltype(std::declval<types::ParticleFormatCodec>().parse_to_document(String(), std::nullopt))>::element_type;
  return flight::make_ref<types::ParticleFormatCodec>(types::ParticleFormatCodec{
      .detect = std::move(detect),
      .parse_to_config =
          [](String t, ParseOptionsView) {
            return config([&t](ConfigInput& i) { i.max_particles = max_particles_of(t); });
          },
      .parse_to_document =
          [](String t, ParseOptionsView) {
            return flight::make_ref<Document>(Document{.config = config([&t](ConfigInput& i) { i.max_particles = max_particles_of(t); }),
                                                       .diagnostics = flight::Array<Ref<types::ImportDiagnostic>>{}});
          },
      .serialize = [](auto) {
        return flight::make_ref<types::ParticleSerializeResult>(types::ParticleSerializeResult{.text = String("MY_FORMAT:50"), .warnings = {}});
      }});
}

void registry() {
  const auto codec = make_codec();
  pf::register_particle_format(test_kind, codec);
  check(pf::detect_registered_particle_format(String("MY_FORMAT:100")) == test_kind, "returns the kind when a codec detects the input");
  pf::unregister_particle_format(test_kind);
  check(!pf::detect_registered_particle_format(String("totally unknown")).has_value(), "returns null when no codec matches");
  {
    pf::register_particle_format(test_kind, make_codec([](String) -> bool { throw flight::Error(String("boom")); }));
    bool threw = false;
    std::optional<String> got;
    try {
      got = pf::detect_registered_particle_format(String("anything"));
    } catch (...) {
      threw = true;
    }
    check(!threw && !got.has_value(), "ignores codecs that throw in detect()");
    pf::unregister_particle_format(test_kind);
  }
  check(!pf::get_particle_format_codec(String("nope.Missing")).has_value(), "getParticleFormatCodec returns null for an unregistered kind");
  pf::register_particle_format(test_kind, codec);
  {
    const auto got = pf::get_particle_format_codec(test_kind);
    check(got.has_value() && *got == codec, "returns the registered codec itself");
    bool listed = false;
    for (const auto& k : pf::get_registered_particle_formats()) listed = listed || k == test_kind;
    check(listed, "getRegisteredParticleFormats includes a newly registered kind");
  }
  pf::unregister_particle_format(test_kind);
  {
    bool listed = false;
    for (const auto& k : pf::get_registered_particle_formats()) listed = listed || k == test_kind;
    check(!listed, "does not include unregistered kinds");
  }

  pf::register_particle_format(test_kind, codec);
  {
    const auto result = pf::parse_registered_particle_format(String("MY_FORMAT:75"), test_kind);
    check(result->format == test_kind && result->config->max_particles == 75 && result->diagnostics.size() == 0,
          "parses with the registered codec and returns config and format");
  }
  pf::unregister_particle_format(test_kind);
  {
    const auto result = pf::parse_registered_particle_format(String("whatever"), String("nope.Missing"));
    const auto crumb = find_kind(result->diagnostics, "particles.unknown-format");
    check(crumb.has_value() && (*crumb)->severity == severity("reject") && (*crumb)->origin == String("parseRegisteredParticleFormat"),
          "reports particles.unknown-format (Reject) when no codec is registered");
    check(result->format == String("nope.Missing") && result->config->max_particles == 1000 && result->diagnostics.size() == 1 &&
              crumb.has_value() && (*crumb)->detail.has_value() &&
              (*crumb)->detail->get(String("reason")) == std::optional<std::variant<bool, double, String>>{String("no-registered-codec")},
          "Addition: one diagnostic with reason no-registered-codec, the requested format and a default config");
  }
  {
    auto failing = make_codec();
    failing->parse_to_document = [](String, ParseOptionsView) -> decltype(failing->parse_to_document(String(), std::nullopt)) {
      throw flight::Error(String("bad input"));
    };
    pf::register_particle_format(test_kind, failing);
    const auto result = pf::parse_registered_particle_format(String("MY_FORMAT:bad"), test_kind);
    const auto crumb = find_kind(result->diagnostics, "particles.parse-error");
    check(crumb.has_value() && (*crumb)->severity == severity("reject") && (*crumb)->origin == String("parseRegisteredParticleFormat") &&
              (*crumb)->detail.has_value() &&
              (*crumb)->detail->get(String("message")) == std::optional<std::variant<bool, double, String>>{String("bad input")},
          "catches codec parse errors and reports particles.parse-error (Reject) with the message");
    check(result->config->max_particles == 1000 && result->format == test_kind, "Addition: a failed parse returns a default config");
    pf::unregister_particle_format(test_kind);
  }
  {
    std::optional<double> seen;
    auto spy = make_codec();
    const auto inner = spy->parse_to_document;
    spy->parse_to_document = [&seen, inner](String t, ParseOptionsView options) {
      seen = options.has_value() ? flight::structural_ref_cast<Ref<types::ParseParticleConfigOptions>>(*options)->texture_size : std::optional<double>{};
      return inner(t, options);
    };
    pf::register_particle_format(test_kind, spy);
    pf::parse_registered_particle_format(String("MY_FORMAT:75"), test_kind,
                                         flight::make_ref<types::ParseParticleConfigOptions>(types::ParseParticleConfigOptions{.texture_size = 32}));
    check(seen == 32.0, "passes shared parse options to the registered codec");
    pf::unregister_particle_format(test_kind);
  }
  {
    const auto first = make_codec([](String) { return true; });
    const auto second = make_codec([](String) { return false; });
    pf::register_particle_format(test_kind, first);
    pf::register_particle_format(test_kind, second);
    check(pf::get_particle_format_codec(test_kind) == std::optional<Ref<types::ParticleFormatCodec>>{second}, "is last-write-wins");
    pf::unregister_particle_format(test_kind);
  }
  {
    pf::register_particle_format(test_kind, codec);
    const auto document = pf::parse_particle_config_document(String("MY_FORMAT:125"));
    check(pf::parse_particle_config(String("MY_FORMAT:125"))->max_particles == 125 && document->diagnostics.size() == 0 &&
              document->format == test_kind,
          "extends both unified parsers without editing their dispatch code");
    check(pf::unregister_particle_format(test_kind), "unregister returns true when a codec is removed");
  }
  check(!pf::unregister_particle_format(String("nope.NotRegistered")), "unregister returns false when none was registered");

  // Addition: detection runs in registration order, and re-registering keeps a kind's original position.
  {
    const String a("test.A"), b("test.B");
    pf::register_particle_format(a, make_codec([](String t) { return t.starts_with(String("X")); }));
    pf::register_particle_format(b, make_codec([](String t) { return t.starts_with(String("X")); }));
    const auto firstWins = pf::detect_registered_particle_format(String("X1"));
    pf::register_particle_format(a, make_codec([](String t) { return t.starts_with(String("X")); }));
    const auto stillFirst = pf::detect_registered_particle_format(String("X1"));
    check(firstWins == a && stillFirst == a, "the first registered matching codec wins, and replacement keeps its place");
    check(pf::detect_particle_format(String("X1")) == a, "detectParticleFormat answers through the registry");
    pf::unregister_particle_format(a);
    check(pf::detect_particle_format(String("X1")) == b, "unregistering a codec exposes the next one");
    pf::unregister_particle_format(b);
    check(!pf::detect_particle_format(String("")).has_value(), "nothing registered detects nothing");
  }
  // Addition: the unified parsers' own diagnostics when nothing detects the text.
  {
    const auto result = pf::parse_particle_config_document(String("unknown"));
    const auto crumb = find_kind(result->diagnostics, "particles.unknown-format");
    check(result->diagnostics.size() == 1 && crumb.has_value() && (*crumb)->origin == String("parseParticleConfigDocument") &&
              (*crumb)->severity == severity("reject") && !result->format.has_value() &&
              (*crumb)->detail->get(String("reason")) == std::optional<std::variant<bool, double, String>>{String("no-format-detected")},
          "an undetected document reports no-format-detected and no format");
    bool threw = false;
    double max = -1;
    try {
      max = pf::parse_particle_config(String("unknown"))->max_particles;
    } catch (...) {
      threw = true;
    }
    check(!threw && max == 1000, "parseParticleConfig returns a default config, not an error, when no format is detected");
    auto failing = make_codec();
    failing->parse_to_config = [](String, ParseOptionsView) -> Ref<types::ParticleEmitterConfig> { throw flight::Error(String("broken")); };
    pf::register_particle_format(test_kind, failing);
    threw = false;
    try {
      max = pf::parse_particle_config(String("MY_FORMAT:5"))->max_particles;
    } catch (...) {
      threw = true;
    }
    check(!threw && max == 1000, "parseParticleConfig returns a default config when the codec throws");
    pf::unregister_particle_format(test_kind);
  }
}

// --- Starling PEX -----------------------------------------------------------------------------------------------

const std::string fire_pex = R"(<?xml version="1.0" encoding="utf-8"?>
<particleEmitterConfig>
  <attribute name="maxParticles" value="200"/>
  <attribute name="emitterType" value="0"/>
  <attribute name="duration" value="-1"/>
  <attribute name="particleLifespan" value="1.5"/>
  <attribute name="particleLifespanVariance" value="0.5"/>
  <attribute name="speed" value="100"/>
  <attribute name="speedVariance" value="20"/>
  <attribute name="angle" value="90"/>
  <attribute name="angleVariance" value="30"/>
  <attribute name="gravityx" value="0"/>
  <attribute name="gravityy" value="200"/>
  <attribute name="sourcePositionVariancex" value="0"/>
  <attribute name="sourcePositionVariancey" value="0"/>
  <attribute name="startParticleSize" value="32"/>
  <attribute name="startParticleSizeVariance" value="8"/>
  <attribute name="finishParticleSize" value="8"/>
  <attribute name="finishParticleSizeVariance" value="0"/>
  <attribute name="startColor" red="1" green="0.5" blue="0" alpha="1"/>
  <attribute name="startColorVariance" red="0" green="0" blue="0" alpha="0"/>
  <attribute name="finishColor" red="1" green="0" blue="0" alpha="0"/>
  <attribute name="finishColorVariance" red="0" green="0" blue="0" alpha="0"/>
  <attribute name="rotationStart" value="0"/>
  <attribute name="rotationStartVariance" value="0"/>
  <attribute name="rotationEnd" value="0"/>
  <attribute name="rotationEndVariance" value="0"/>
  <attribute name="maxRadius" value="0"/>
  <attribute name="maxRadiusVariance" value="0"/>
  <attribute name="minRadius" value="0"/>
  <attribute name="minRadiusVariance" value="0"/>
  <attribute name="rotatePerSecond" value="0"/>
  <attribute name="rotatePerSecondVariance" value="0"/>
  <attribute name="radialAcceleration" value="0"/>
  <attribute name="radialAccelVariance" value="0"/>
  <attribute name="tangentialAcceleration" value="0"/>
  <attribute name="tangentialAccelVariance" value="0"/>
  <attribute name="blendFuncSource" value="770"/>
  <attribute name="blendFuncDestination" value="771"/>
  <attribute name="textureFileName" value="fire.png"/>
</particleEmitterConfig>)";

Ref<types::StarlingPexParseOptions> pex_size(double size) {
  return flight::make_ref<types::StarlingPexParseOptions>(types::StarlingPexParseOptions{.texture_size = size});
}

Ref<types::ParticleEmitterConfig> pex(const std::string& xml, std::optional<double> size = std::nullopt) {
  return pf::parse_starling_pex(String(xml.c_str()), size.has_value() ? std::optional<Ref<types::StarlingPexParseOptions>>{pex_size(*size)} : std::nullopt);
}

void starling_parse() {
  const auto c = pex(fire_pex);
  check(c->max_particles == 200, "parses maxParticles from attribute-style PEX");
  check(near(c->lifetime_min, 1.0) && near(c->lifetime_max, 2.0), "converts lifetime with variance to min/max");
  check(near(c->speed_min, 80) && near(c->speed_max, 120), "converts speed with variance to min/max");
  check(near(c->direction_x, 0, 5e-4) && near(c->direction_y, -1, 5e-4), "maps angle 90 to upward direction (-Y screen)");
  check(near(c->spread, 30 * std::numbers::pi / 180, 5e-5), "converts angleVariance to spread in radians");
  check(near(c->gravity_x, 0) && near(c->gravity_y, 200), "maps gravity directly");
  check(near(c->color_start_r, 1) && near(c->color_start_g, 0.5) && near(c->color_start_b, 0) && near(c->color_end_r, 1) &&
            near(c->color_end_g, 0) && near(c->color_end_b, 0),
        "maps startColor/finishColor to colorStart/End");
  check(near(c->alpha_start, 1) && near(c->alpha_end, 0), "maps the color alphas to alphaStart/End");
  const auto sized = pex(fire_pex, 32);
  check(near(sized->scale_min, 0.75) && near(sized->scale_max, 1.25), "normalises scale by textureSize");
  check(c->blend_mode == String("normal"), "maps normal blend func (770, 771) to normal");
  check(pex(replace_once(fire_pex, "blendFuncDestination\" value=\"771\"", "blendFuncDestination\" value=\"1\""))->blend_mode == String("add"),
        "maps additive blend func (770, 1) to add");
  check(c->loop && c->duration == 0, "maps duration -1 to an infinite looping emitter");
  check(c->emitter_shape == String("point"), "uses a point emitter shape when sourcePositionVariance is zero");
  {
    auto s = replace_once(fire_pex, "sourcePositionVariancex\" value=\"0\"", "sourcePositionVariancex\" value=\"20\"");
    s = replace_once(s, "sourcePositionVariancey\" value=\"0\"", "sourcePositionVariancey\" value=\"20\"");
    const auto circle = pex(s);
    check(circle->emitter_shape == String("circle") && near(circle->emitter_radius, 20), "maps a circle emitter when x and y variance agree");
  }
  for (const char* bad : {"not xml", "{}"}) {
    std::string message;
    try {
      static_cast<void>(pex(bad));
    } catch (const flight::Error& error) {
      message = text(error.message());
    } catch (const std::exception& error) {
      message = std::string("non-Error: ") + error.what();
    }
    check(message == "Invalid Starling PEX: input must be an XML string beginning with <",
          std::string("throws a clear error for non-XML input: ") + bad + " -> " + message);
  }

  {
    const auto config = pex(fire_pex, 32);
    const auto loaded = pf::parse_starling_pex_document(String(fire_pex.c_str()), pex_size(32));
    check(loaded->config->max_particles == config->max_particles && near(loaded->config->direction_y, config->direction_y) &&
              near(loaded->config->gravity_y, config->gravity_y),
          "parseStarlingPexDocument returns the same config values as parseStarlingPex");
  }
  const auto doc = pf::parse_starling_pex_document(String(fire_pex.c_str()));
  check(doc->document->texture_file_name == String("fire.png"), "preserves textureFileName in the document");
  check(doc->document->blend_func_source == 770 && doc->document->blend_func_destination == 771, "preserves the blend function in the document");
  check(doc->diagnostics.size() == 0, "has no diagnostics for a standard gravity emitter");
  const auto diagnosed = [](const std::string& from, const std::string& to, const char* kind, const char* sev) {
    const auto result = pf::parse_starling_pex_document(String(replace_once(fire_pex, from, to).c_str()));
    const auto crumb = find_kind(result->diagnostics, kind);
    check(crumb.has_value() && (*crumb)->severity == severity(sev) && (*crumb)->origin == String("collectStarlingPexDiagnostics"),
          std::string("reports ") + kind);
  };
  diagnosed("emitterType\" value=\"0\"", "emitterType\" value=\"1\"", "starlingpex.radial-approximated", "recover");
  diagnosed("radialAcceleration\" value=\"0\"", "radialAcceleration\" value=\"50\"", "starlingpex.radial-acceleration-unsupported", "skip");
  diagnosed("tangentialAcceleration\" value=\"0\"", "tangentialAcceleration\" value=\"50\"", "starlingpex.tangential-acceleration-unsupported",
            "skip");
  diagnosed("finishParticleSizeVariance\" value=\"0\"", "finishParticleSizeVariance\" value=\"4\"", "starlingpex.finish-size-variance-unsupported",
            "skip");
  diagnosed("startColorVariance\" red=\"0\" green=\"0\" blue=\"0\" alpha=\"0\"", "startColorVariance\" red=\"0\" green=\"0\" blue=\"0\" alpha=\"0.5\"",
            "starlingpex.alpha-variance-unsupported", "skip");

  // Addition: diagnostics come in the collector's order, and a plist-style element value is read as well.
  {
    auto s = replace_once(fire_pex, "emitterType\" value=\"0\"", "emitterType\" value=\"1\"");
    s = replace_once(s, "radialAcceleration\" value=\"0\"", "radialAcceleration\" value=\"5\"");
    s = replace_once(s, "tangentialAcceleration\" value=\"0\"", "tangentialAcceleration\" value=\"5\"");
    std::vector<std::string> kinds;
    const auto result = pf::parse_starling_pex_document(String(s.c_str()));
    for (const auto& d : result->diagnostics) kinds.push_back(text(d->kind));
    check(kinds == std::vector<std::string>{"starlingpex.radial-approximated", "starlingpex.radial-acceleration-unsupported",
                                            "starlingpex.tangential-acceleration-unsupported"},
          "diagnostics are reported in the collector's order");
  }
  {
    const std::string plist = "<?xml version=\"1.0\"?>\n<particleEmitterConfig>\n<key>maxParticles</key><integer>42</integer>\n"
                              "<key>maxParticles</key><integer>7</integer>\n</particleEmitterConfig>";
    check(pex(plist)->max_particles == 42, "an element-style value is read, and a repeated key keeps its first value");
    const std::string attrFirst = "<particleEmitterConfig>\n  <attribute name=\"maxParticles\" value=\"9\"/>\n"
                                  "<key>maxParticles</key><integer>11</integer>\n</particleEmitterConfig>";
    check(pex(attrFirst)->max_particles == 9, "an attribute value is not overwritten by an element-style one");
    // A key followed by a self-closing tag other than <true/> or <false/> stores nothing.
    const std::string selfClosed = "<particleEmitterConfig>\n<key>textureFileName</key><foo/>\n</particleEmitterConfig>";
    check(pf::parse_starling_pex_document(String(selfClosed.c_str()))->document->texture_file_name == String(""),
          "a non-boolean self-closing tag after a key stores nothing");
  }
  // Addition: a duration of exactly 0 is an infinite, looping emitter; a positive one is finite.
  {
    const auto zero = pex(replace_once(fire_pex, "duration\" value=\"-1\"", "duration\" value=\"0\""));
    const auto finite = pex(replace_once(fire_pex, "duration\" value=\"-1\"", "duration\" value=\"2.5\""));
    check(zero->loop && zero->duration == 0 && !finite->loop && finite->duration == 2.5, "duration 0 loops, duration 2.5 is finite");
  }
}

std::string number_text(double v) { return text(flight::to_string(v)); }

void starling_serialize() {
  {
    const auto config = pex(fire_pex, 32);
    const auto document = pf::parse_starling_pex_document(String(fire_pex.c_str()), pex_size(32))->document;
    using Existing = PexExistingRef::element_type;
    using Options = PexSerializeOptionsRef::element_type;
    // The parsed document is a full StarlingPexDocument; pass it as the partial `existing`, as the source does.
    Existing existing;
    existing.texture_file_name = document->texture_file_name;
    existing.blend_func_source = document->blend_func_source;
    existing.blend_func_destination = document->blend_func_destination;
    existing.emitter_type = document->emitter_type;
    const auto xml = pf::serialize_starling_pex(config, flight::make_ref<Existing>(existing), flight::make_ref<Options>(Options{.texture_size = 32}));
    const auto again = pf::parse_starling_pex(xml, pex_size(32));
    check(again->max_particles == config->max_particles && near(again->direction_x, config->direction_x, 5e-4) &&
              near(again->gravity_y, config->gravity_y, 0.05) && near(again->color_start_r, config->color_start_r, 5e-4) &&
              near(again->alpha_end, config->alpha_end, 5e-4),
          "round-trips key config fields");
    check(contains(pf::serialize_starling_pex(config, flight::make_ref<Existing>(existing)), "fire.png"), "preserves textureFileName in a round trip");
    // Addition: the round trip also keeps speed, lifetime, spread and scale.
    check(near(again->speed_min, config->speed_min, 1e-9) && near(again->speed_max, config->speed_max, 1e-9) &&
              near(again->lifetime_min, config->lifetime_min, 1e-9) && near(again->spread, config->spread, 1e-9) &&
              near(again->scale_min, config->scale_min, 1e-9) && near(again->scale_max, config->scale_max, 1e-9),
          "round-trips speed, lifetime, spread and scale");
  }
  {
    const auto xml = pf::serialize_starling_pex(pex(fire_pex));
    check(contains(xml, "<?xml version=\"1.0\"") && contains(xml, "<particleEmitterConfig>") && contains(xml, "</particleEmitterConfig>"),
          "produces XML with a particleEmitterConfig root");
  }
  {
    const auto xml = pf::serialize_starling_pex(config([](ConfigInput& i) {
      i.max_particles = 50;
      i.blend_mode = String("add");
    }));
    const auto c2 = pf::parse_starling_pex(xml);
    check(c2->max_particles == 50 && c2->blend_mode == String("add"), "produces XML parseStarlingPex reads back for a fresh config");
  }
  check(contains(pf::serialize_starling_pex(config([](ConfigInput& i) { i.blend_mode = String("normal"); })), "blendFuncDestination\" value=\"771\""),
        "maps normal to blendFuncDestination 771");
  check(contains(pf::serialize_starling_pex(config([](ConfigInput& i) { i.blend_mode = String("add"); })), "blendFuncDestination\" value=\"1\""),
        "maps add to blendFuncDestination 1");
  {
    const auto result = pf::serialize_starling_pex_document(config());
    check(contains(result->text, "<particleEmitterConfig>") && result->warnings.size() == 0, "returns text and no warnings for a default config");
  }
  const auto warns = [](const std::function<void(ConfigInput&)>& fill, const std::string& word) {
    // Held first: a range-for over a member of a temporary result would outlive the result.
    const auto result = pf::serialize_starling_pex_document(config(fill));
    for (const auto& w : result->warnings) {
      std::string lower = text(w);
      for (auto& ch : lower) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      if (lower.find(word) != std::string::npos) return true;
    }
    return false;
  };
  check(warns([](ConfigInput& i) { i.blend_mode = String("multiply"); }, "multiply"), "warns when multiply cannot be represented");
  check(warns([](ConfigInput& i) { i.burst_count = 5; }, "burst"), "warns when the config has emission bursts");
  check(!warns([](ConfigInput& i) { i.blend_mode = String("add"); }, "blend"), "no blend warning for add");

  // Addition: byte-exact PEX for a default config.
  {
    const std::string expected = std::string("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<particleEmitterConfig>\n") +
                                 "  <attribute name=\"maxParticles\" value=\"1000\"/>\n"
                                 "  <attribute name=\"emitterType\" value=\"0\"/>\n"
                                 "  <attribute name=\"duration\" value=\"-1\"/>\n"
                                 "  <attribute name=\"particleLifespan\" value=\"0.75\"/>\n"
                                 "  <attribute name=\"particleLifespanVariance\" value=\"0.25\"/>\n"
                                 "  <attribute name=\"speed\" value=\"75\"/>\n"
                                 "  <attribute name=\"speedVariance\" value=\"25\"/>\n"
                                 "  <attribute name=\"angle\" value=\"90\"/>\n"
                                 "  <attribute name=\"angleVariance\" value=\"180\"/>\n"
                                 "  <attribute name=\"gravityx\" value=\"0\"/>\n"
                                 "  <attribute name=\"gravityy\" value=\"0\"/>\n"
                                 "  <attribute name=\"sourcePositionVariancex\" value=\"0\"/>\n"
                                 "  <attribute name=\"sourcePositionVariancey\" value=\"0\"/>\n"
                                 "  <attribute name=\"startParticleSize\" value=\"1\"/>\n"
                                 "  <attribute name=\"startParticleSizeVariance\" value=\"0\"/>\n"
                                 "  <attribute name=\"finishParticleSize\" value=\"1\"/>\n"
                                 "  <attribute name=\"finishParticleSizeVariance\" value=\"0\"/>\n"
                                 "  <attribute name=\"startColor\" red=\"1\" green=\"1\" blue=\"1\" alpha=\"1\"/>\n"
                                 "  <attribute name=\"startColorVariance\" red=\"0\" green=\"0\" blue=\"0\" alpha=\"0\"/>\n"
                                 "  <attribute name=\"finishColor\" red=\"1\" green=\"1\" blue=\"1\" alpha=\"0\"/>\n"
                                 "  <attribute name=\"finishColorVariance\" red=\"0\" green=\"0\" blue=\"0\" alpha=\"0\"/>\n"
                                 "  <attribute name=\"rotationStart\" value=\"0\"/>\n"
                                 "  <attribute name=\"rotationStartVariance\" value=\"0\"/>\n"
                                 "  <attribute name=\"rotationEnd\" value=\"0\"/>\n"
                                 "  <attribute name=\"rotationEndVariance\" value=\"0\"/>\n"
                                 "  <attribute name=\"maxRadius\" value=\"0\"/>\n"
                                 "  <attribute name=\"maxRadiusVariance\" value=\"0\"/>\n"
                                 "  <attribute name=\"minRadius\" value=\"0\"/>\n"
                                 "  <attribute name=\"minRadiusVariance\" value=\"0\"/>\n"
                                 "  <attribute name=\"rotatePerSecond\" value=\"0\"/>\n"
                                 "  <attribute name=\"rotatePerSecondVariance\" value=\"0\"/>\n"
                                 "  <attribute name=\"radialAcceleration\" value=\"0\"/>\n"
                                 "  <attribute name=\"radialAccelVariance\" value=\"0\"/>\n"
                                 "  <attribute name=\"tangentialAcceleration\" value=\"0\"/>\n"
                                 "  <attribute name=\"tangentialAccelVariance\" value=\"0\"/>\n"
                                 "  <attribute name=\"blendFuncSource\" value=\"770\"/>\n"
                                 "  <attribute name=\"blendFuncDestination\" value=\"771\"/>\n"
                                 "</particleEmitterConfig>";
    const auto got = text(pf::serialize_starling_pex(config()));
    check(got == expected, "serializes a default config byte-for-byte");
    if (got != expected) std::cerr << got << '\n';
  }
  // Addition: the texture name is escaped, existing radial values are kept, and every documented warning is reported in order.
  {
    using Existing = PexExistingRef::element_type;
    Existing existing;
    existing.texture_file_name = String("a&b<c>.png");
    existing.max_radius = 12.5;
    existing.emitter_type = 1;
    const auto xml = pf::serialize_starling_pex(config(), flight::make_ref<Existing>(existing));
    check(contains(xml, "<attribute name=\"textureFileName\" value=\"a&amp;b&lt;c&gt;.png\"/>") &&
              contains(xml, "<attribute name=\"maxRadius\" value=\"12.5\"/>") && contains(xml, "<attribute name=\"emitterType\" value=\"1\"/>"),
          "existing document fields are kept and the texture name is XML-escaped");
    const auto all = pf::serialize_starling_pex_document(config([](ConfigInput& i) {
      i.blend_mode = String("screen");
      i.color_curve = flight::Array<double>{0, 0, 0, 1, 1, 1};
      i.alpha_curve = flight::Array<double>{1, 0};
      i.scale_curve = flight::Array<double>{1, 0};
      i.burst_count = 2;
      i.velocity_inheritance = 0.5;
    }));
    std::vector<std::string> got;
    for (const auto& w : all->warnings) got.push_back(text(w));
    check(got == std::vector<std::string>{
                     "Blend mode 'screen' has no Starling PEX equivalent and was approximated as 'normal'",
                     "Color curve has more than two stops; Starling PEX stores only start and end colors",
                     "Alpha curve has more than two stops; Starling PEX stores only start and end alpha",
                     "Scale curve has more than two stops; Starling PEX stores only start and end size",
                     "Emission bursts are not supported by the Starling PEX format and were ignored",
                     "velocityInheritance is not supported by the Starling PEX format and was ignored",
                 },
          "every serialize warning, in the collector's order, with its text");
  }
  // Addition: a finite emitter writes its duration; a circle and a rect write their variances.
  {
    const auto finite = text(pf::serialize_starling_pex(config([](ConfigInput& i) {
      i.duration = 2.5;
      i.loop = false;
    })));
    const auto circle = text(pf::serialize_starling_pex(config([](ConfigInput& i) {
      i.emitter_shape = String("circle");
      i.emitter_radius = 7;
    })));
    const auto rect = text(pf::serialize_starling_pex(config([](ConfigInput& i) {
      i.emitter_shape = String("rect");
      i.emitter_width = 10;
      i.emitter_height = 4;
    })));
    check(finite.find("name=\"duration\" value=\"2.5\"") != std::string::npos &&
              circle.find("name=\"sourcePositionVariancex\" value=\"7\"/>\n  <attribute name=\"sourcePositionVariancey\" value=\"7\"") !=
                  std::string::npos &&
              rect.find("name=\"sourcePositionVariancex\" value=\"5\"/>\n  <attribute name=\"sourcePositionVariancey\" value=\"2\"") != std::string::npos,
          "duration and source variances follow the config");
  }
}


// --- Spine and Unity serializers --------------------------------------------------------------------------------
// Both serializers compile, but their text is JSON.stringify of a generated document struct, which writes `{}`
// (docs/compiler-requests/json-stringify-nominal-ref-empty-object.md, where the exact expected text for a default
// config is recorded). This test therefore does not bless their text: upstream's "produces valid JSON" cases
// would pass on `{}`. Its parse -> serialize -> parse cases need the parsers, which are this package's residual
// (docs/compiler-requests/parsed-json-object-shape-reads.md). The warnings are computed from the config alone,
// so they are ported, and pinned in full.

bool has_warning(const flight::Array<String>& warnings, const std::string& word) {
  for (const auto& w : warnings)
    if (text(w).find(word) != std::string::npos) return true;
  return false;
}

std::vector<std::string> texts(const flight::Array<String>& values) {
  std::vector<std::string> out;
  for (const auto& v : values) out.push_back(text(v));
  return out;
}

void spine_unity_serialize() {
  check(pf::serialize_spine_particle_document(config())->warnings.size() == 0, "Spine: no warnings for a default config");
  {
    const auto bursts = pf::serialize_spine_particle_document(config([](ConfigInput& i) { i.burst_count = 5; }));
    const auto variance = pf::serialize_spine_particle_document(config([](ConfigInput& i) { i.color_start_variance_r = 0.1; }));
    check(has_warning(bursts->warnings, "burst"), "Spine: warns when the config has emission bursts");
    check(has_warning(variance->warnings, "colorStartVariance"), "Spine: warns when the config has color start variance");
    const auto all = pf::serialize_spine_particle_document(config([](ConfigInput& i) {
      i.burst_count = 1;
      i.color_end_variance_g = 0.2;
      i.color_start_variance_b = 0.2;
      i.velocity_inheritance = 1;
    }));
    check(texts(all->warnings) == std::vector<std::string>{"Emission bursts are not supported by the Spine particle format and were ignored",
                                                           "colorEndVariance is not supported by the Spine format and was ignored",
                                                           "colorStartVariance is not supported by the Spine format and was ignored",
                                                           "velocityInheritance is not supported by the Spine format and was ignored"},
          "Addition: every Spine warning, in order, with its text");
  }
  check(pf::serialize_unity_particle_document(config())->warnings.size() == 0, "Unity: no warnings for a default config");
  {
    const auto gravity = pf::serialize_unity_particle_document(config([](ConfigInput& i) { i.gravity_x = 50; }));
    const auto variance = pf::serialize_unity_particle_document(config([](ConfigInput& i) { i.color_end_variance_r = 0.2; }));
    const auto plain = pf::serialize_unity_particle_document(config([](ConfigInput& i) {
      i.max_particles = 100;
      i.blend_mode = String("add");
      i.loop = true;
    }));
    check(has_warning(gravity->warnings, "gravityX"), "Unity: warns when the config has horizontal gravity");
    check(has_warning(variance->warnings, "colorEndVariance"), "Unity: warns when the config has color end variance");
    check(plain->warnings.size() == 0, "Unity: no warnings for a config with standard representable features");
    const auto all = pf::serialize_unity_particle_document(config([](ConfigInput& i) {
      i.color_end_variance_b = 0.1;
      i.color_start_variance_g = 0.1;
      i.velocity_inheritance = 2;
      i.gravity_x = -1;
    }));
    check(texts(all->warnings) == std::vector<std::string>{"colorEndVariance is not supported by the Unity format and was ignored",
                                                           "colorStartVariance is not supported by the Unity format and was ignored",
                                                           "velocityInheritance is not supported by the Unity format and was ignored",
                                                           "gravityX (horizontal gravity) is not supported by the Unity format and was ignored"},
          "Addition: every Unity warning, in order, with its text");
  }
}

} // namespace

int main() {
  registry();
  starling_parse();
  starling_serialize();
  spine_unity_serialize();
  return failures == 0 ? 0 : 1;
}
