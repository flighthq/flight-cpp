#include <flight/particles_formats/unity_parse.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
bool near(double value, double expected) { return std::abs(value - expected) < 1e-12; }
void fails(const flight::String& input, const flight::String& expected, bool prefix = false) {
  for (bool document : {false, true}) {
    bool caught = false;
    try {
      if (document) (void)flight::particles_formats::parse_unity_particle_document(input);
      else (void)flight::particles_formats::parse_unity_particle(input);
    } catch (const flight::Error& error) {
      caught = true;
      check(error.name() == flight::String("Error"), "original application Error class");
      check(prefix ? error.message().starts_with(expected) : error.message() == expected,
            "original root validation and syntax wrapper");
    }
    check(caught, "invalid root fails at both original entrypoints");
  }
}
// Test the existing explicit Unicode capability. These two transformations are source-oracle
// fixtures; this provider is not a Unicode implementation or an ASCII fallback in the parser.
class FixtureUnicodeService final : public flight::UnicodeService {
 public:
  std::u16string lower(std::u16string_view value) const override {
    if (value == std::u16string_view(u"\u0130")) return u"i\u0307";
    if (value == std::u16string_view(std::u16string(1, char16_t(0xd800)))) return std::u16string(1, char16_t(0xd800));
    throw std::runtime_error("unexpected Unicode fixture input");
  }
  std::u16string upper(std::u16string_view) const override { throw std::runtime_error("unused Unicode fixture"); }
};
}

int main() {
  namespace parser = flight::particles_formats;
  using flight::String;
  const auto config = parser::parse_unity_particle("{}");
  const auto result = parser::parse_unity_particle_document("{}");
  const auto doc = result->document;
  check(config != result->config && config->max_particles == 1000 && config->spawn_rate == 10 &&
        config->loop && config->duration == 0 && config->speed_min == 500 && config->speed_max == 500 &&
        config->lifetime_min == 1 && config->lifetime_max == 1 && config->scale_min == 1 &&
        config->scale_max == 1 && config->scale_end == 1 && config->alpha_start == 1 &&
        config->alpha_end == 1 && !config->blend_mode && !config->color_curve && !config->alpha_curve &&
        !config->scale_curve && config->emitter_shape == String("point"), "source config defaults and fresh calls");
  check(doc->duration == 5 && doc->shape->enabled && doc->shape->shape_type == String("Cone") &&
        doc->shape->radius == 1 && doc->emission->rate_over_time->constant == 10 &&
        doc->emission->bursts.empty() && !doc->start_lifetime->constant_min &&
        !doc->color_over_lifetime->gradient && !doc->size_over_lifetime->curve,
        "actual document defaults and authored optional omissions");
  check(result->diagnostics.size() == 1 && result->diagnostics[0]->kind == String("unity.start-rotation-unsupported"),
        "missing startRotation keeps original undefined optional-field diagnostic behavior");

  const auto options = flight::make_ref<flight::types::UnityParseOptions>();
  options->pixels_per_unit = 10;
  const auto mapped = parser::parse_unity_particle_document(R"({"name":"\ud800😀","maxParticles":4294967297,"looping":false,"duration":-2,"startLifetime":{"mode":"twoConstants","constantMin":3,"constantMax":1},"startSpeed":{"mode":"twoCurves","constantMin":-2,"constantMax":4},"gravityModifier":2,"physicsGravity":5,"shape":{"enabled":true,"shapeType":"Box","scale":{"x":2,"y":3,"z":4}},"emission":{"rateOverTime":{"mode":"twoConstants","constantMin":4,"constantMax":8},"bursts":[{"count":7,"cycleCount":2,"repeatInterval":0.5},{"count":9}]},"blendMode":"AdDiTiVe"})", options);
  check(mapped->config->max_particles == 1 && !mapped->config->loop && mapped->config->duration == 0 &&
        mapped->document->duration == -2 && mapped->config->spawn_rate == 6 && mapped->config->speed_min == -20 &&
        mapped->config->speed_max == 40 && mapped->config->lifetime_min == 3 && mapped->config->lifetime_max == 1 &&
        mapped->config->gravity_y == 100 && mapped->config->emitter_shape == String("rect") &&
        mapped->config->emitter_width == 20 && mapped->config->emitter_height == 30 &&
        near(mapped->config->spread, 2 * std::acos(-1.)) && mapped->config->burst_count == 7 &&
        mapped->config->burst_interval == 0.5 && mapped->config->blend_mode == std::optional<String>("add"),
        "ranges, source units, source bitwise conversion, shape, bursts and string guard");
  check(mapped->document->name.native()[0] == char16_t(0xd800) && mapped->document->name.native().size() == 3 &&
        mapped->document->emission->bursts.size() == 2 &&
        mapped->document->emission->bursts[0] != mapped->document->emission->bursts[1],
        "exact UTF16 and fresh mapped burst owners");
  mapped->document->emission->bursts[0]->count = 70;
  check(mapped->config->burst_count == 7, "document output and config remain distinct authored owners");

  const auto curves = parser::parse_unity_particle_document(R"({"colorOverLifetime":{"enabled":true,"gradient":{"colorKeys":[{"time":0,"r":1,"g":0,"b":0},{"time":0.5,"color":{"r":0,"g":1,"b":0}},false],"alphaKeys":[{},false,{}]}},"sizeOverLifetime":{"enabled":true,"curve":{"keys":[{},false,{}]}}})");
  check(curves->config->color_curve && curves->config->color_curve->size() == 99 &&
        (*curves->config->color_curve)[0] == 1 && (*curves->config->color_curve)[1] == 0 &&
        (*curves->config->color_curve)[48] == 0 && (*curves->config->color_curve)[49] == 1 &&
        (*curves->config->color_curve)[96] == 1 && (*curves->config->color_curve)[97] == 1 &&
        curves->config->alpha_curve && curves->config->alpha_curve->size() == 33 &&
        (*curves->config->alpha_curve)[16] == 1 && curves->config->scale_curve &&
        (*curves->config->scale_curve)[32] == 1,
        "inline/nullish/nested color aliases and primitive keyframes retain source defaults");
  check(!parser::parse_unity_particle(R"({"sizeOverLifetime":{"enabled":true,"curve":[]}})")->scale_curve,
        "Array.prototype.keys is rejected by the original Array.isArray observer");
  for (const auto& primitive : std::vector<String>{"false", "0", "\"text\"", "[]"}) {
    const auto input = String("{\"emission\":{\"bursts\":[") + primitive + String(",{}]}}");
    const auto value = parser::parse_unity_particle_document(input);
    check(value->config->burst_count == 0 && value->document->emission->bursts.size() == 2 &&
          value->document->emission->bursts[0]->cycle_count == 1,
          "non-null primitive burst elements remain accepted with property defaults");
  }
  check(parser::parse_unity_particle(R"({"emission":{"bursts":[null]}})")->burst_count == 0,
        "config alone does not allocate or map the intermediate document");
  bool burst_null = false;
  try { (void)parser::parse_unity_particle_document(R"({"emission":{"bursts":[null]}})"); }
  catch (const flight::TypeError&) { burst_null = true; }
  check(burst_null, "document maps null burst at its original property-read boundary");

  const auto diagnostics = parser::parse_unity_particle_document(R"({"velocityOverLifetime":{"enabled":true},"forceOverLifetime":{"enabled":true},"noise":{"enabled":1},"emission":{"bursts":[{},{}]},"shape":{"enabled":true,"shapeType":"unsupported"},"prewarm":true})")->diagnostics;
  const std::vector<String> kinds{"unity.module-unsupported", "unity.module-unsupported", "unity.extra-bursts-unsupported", "unity.shape-unsupported", "unity.prewarm-unsupported", "unity.start-rotation-unsupported"};
  check(diagnostics.size() == kinds.size(), "unsupported modules aggregate once, boolean-only enabled");
  for (std::size_t i = 0; i < diagnostics.size() && i < kinds.size(); ++i)
    check(diagnostics[i]->kind == kinds[i] && diagnostics[i]->origin == String("collectUnityDiagnostics") &&
          diagnostics[i]->severity == (i == 3 ? flight::types::import_diagnostic_severity->recover : flight::types::import_diagnostic_severity->skip),
          "original diagnostic order, origin and severity");
  check(std::get<String>(diagnostics[0]->detail->get(String("module")).value()) == String("velocity-over-lifetime") &&
        std::get<double>(diagnostics[2]->detail->get(String("burstCount")).value()) == 2,
        "diagnostic details keep module identity and numeric actual array length");
  const auto nonfinite = parser::parse_unity_particle(R"({"startLifetime":1e400,"startSpeed":-0,"gravityModifier":-0,"startSize":{"mode":"twoConstants","constantMin":1e400,"constantMax":-1e400}})");
  check(std::isinf(nonfinite->lifetime_min) && std::signbit(nonfinite->speed_min) &&
        std::signbit(nonfinite->gravity_y) && std::isinf(nonfinite->scale_max) && nonfinite->scale_max < 0,
        "numeric-only readers retain JSON overflow and signed zero");
  fails("{", "Invalid Unity particle JSON: ", true);
  for (const auto& [input, kind] : std::vector<std::pair<String, String>>{{"null", "null"}, {"[]", "array"}, {"false", "boolean"}, {"0", "number"}, {"\"text\"", "string"}})
    fails(input, String("Invalid Unity particle document: expected a JSON object, got ") + kind);

  bool unicode_missing = false;
  try { (void)parser::parse_unity_particle(R"({"blendMode":"İ"})"); }
  catch (const flight::UnicodeServiceUnavailable&) { unicode_missing = true; }
  check(unicode_missing, "existing nonASCII case-conversion capability is explicit");
  {
    const flight::UnicodeServiceScope scope(std::make_shared<FixtureUnicodeService>());
    check(!parser::parse_unity_particle(R"({"blendMode":"İ"})")->blend_mode &&
          !parser::parse_unity_particle(R"({"blendMode":"\ud800"})")->blend_mode,
          "original nonASCII strings reach the existing Unicode provider without narrowing");
  }
  if (!failures) std::cout << "Original generated Unity particle public parser and actual document PASS; Unicode service required for nonASCII case conversion\n";
  return failures ? 1 : 0;
}
