#include <flight/particles_formats/pixi_parse.hpp>

#include <cmath>
#include <iostream>
#include <utility>
#include <vector>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
bool near(double actual, double expected) { return std::abs(actual - expected) < 1e-12; }

// Non-ASCII case conversion is an explicit host service. These whole-string mappings exercise
// its public seam, including contextual Greek casing, without replacing any importer logic.
class TestUnicode final : public flight::UnicodeService {
 public:
  std::u16string lower(std::u16string_view value) const override {
    if (value == u"ΟΣ") return u"ος";
    if (value == u"K") return u"k";
    throw flight::UnicodeServiceUnavailable();
  }
  std::u16string upper(std::u16string_view) const override { throw flight::UnicodeServiceUnavailable(); }
};
}

int main() {
  namespace parser = flight::particles_formats;
  using flight::String;
  const auto defaults = parser::parse_pixi_particle("{}");
  const auto other = parser::parse_pixi_particle("{}");
  check(defaults != other, "each original public parse creates a fresh config owner");
  check(defaults->max_particles == 1000 && defaults->spawn_rate == 10 &&
        defaults->lifetime_min == 0.5 && defaults->lifetime_max == 1 &&
        defaults->speed_min == 50 && defaults->speed_max == 100 &&
        defaults->scale_min == 0.5 && defaults->scale_max == 1 && defaults->scale_end == 0.5 &&
        defaults->alpha_start == 1 && defaults->alpha_end == 0,
        "missing keys reach all authored scalar and start/end defaults");
  check(defaults->emitter_shape == String("point") && defaults->emitter_radius == 0 &&
        defaults->direction_x == -1 && near(defaults->direction_y, 0) &&
        defaults->blend_mode == std::optional<String>("normal") && !defaults->alpha_curve &&
        !defaults->color_curve && !defaults->scale_curve && defaults->loop && !defaults->world_space,
        "original factory defaults and direction remain represented");

  const auto mapped = parser::parse_pixi_particle(R"({"maxParticles":4294967297,"frequency":0.25,"lifetime":{"min":2,"max":4},"speed":{"start":10,"end":{"value":20}},"scale":{"start":2,"end":4},"alpha":{"start":0.75,"end":0.25},"color":{"start":"#f0a","end":{"value":"12x45z"}},"angle":{"min":10,"max":50},"spawnRect":{"w":12,"h":9},"spawnCircle":{"r":3},"blendMode":"ADDITIVE","rotationSpeed":180})");
  check(mapped->max_particles == 1 && mapped->spawn_rate == 4 && mapped->lifetime_min == 2 &&
        mapped->lifetime_max == 4 && mapped->speed_min == 10 && mapped->speed_max == 20 &&
        mapped->scale_min == 2 && mapped->scale_max == 4 && mapped->scale_end == 0.5 &&
        mapped->alpha_start == 0.75 && mapped->alpha_end == 0.25,
        "nested original JSON shapes supply scalar, wrapped endpoints and bitwise values");
  check(mapped->color_start_r == 240.0 / 255 && mapped->color_start_g == 175.0 / 255 &&
        mapped->color_start_b == 1 && mapped->color_end_r == 18.0 / 255 &&
        mapped->color_end_g == 1 && mapped->color_end_b == 5.0 / 255,
        "original color slicing, padding and permissive parseInt remain intact");
  check(mapped->emitter_shape == String("circle") && mapped->emitter_radius == 3 &&
        mapped->emitter_width == 0 && mapped->emitter_height == 0 &&
        mapped->blend_mode == std::optional<String>("add") &&
        near(mapped->direction_x, std::sqrt(3.0) / 2) && near(mapped->direction_y, 0.5) &&
        near(mapped->spread, std::acos(-1.0) / 9) && near(mapped->rotation_speed_min, std::acos(-1.0)) &&
        mapped->rotation_speed_min == mapped->rotation_speed_max,
        "circle precedence, clockwise angle, spread, blend and rotation mapping remain authored");

  for (const auto& [value, shape] : std::vector<std::pair<String, String>>{
      {"null", "point"}, {"false", "point"}, {"0", "point"}, {"\"\"", "point"},
      {"1", "circle"}, {"\"s\"", "circle"}, {"[]", "circle"}, {"{}", "circle"}}) {
    const auto input = String("{\"lifetime\":") + value + String(",\"spawnCircle\":") + value + String("}");
    const auto config = parser::parse_pixi_particle(input);
    check(config->lifetime_min == 0.5 && config->lifetime_max == 1 && config->emitter_radius == 0 &&
          config->emitter_shape == shape, "primitive/nullish fields default while erased truthiness preserves source branches");
  }
  const auto document = parser::parse_pixi_particle_document(R"({"spawnBurst":null,"spawnPolygon":false,"acceleration":{"x":2},"behaviors":null})");
  const std::vector<String> kinds{"pixi.spawn-burst-mapped-to-point", "pixi.spawn-polygon-mapped-to-point",
                                  "pixi.acceleration-unsupported", "pixi.behaviors-partial"};
  check(document->diagnostics.size() == kinds.size(), "all original diagnostic branches contribute");
  for (std::size_t i = 0; i < document->diagnostics.size(); ++i) {
    const auto diagnostic = document->diagnostics.element(static_cast<double>(i));
    check(i < kinds.size() && diagnostic->kind == kinds[i] && diagnostic->origin == String("collectPixiDiagnostics") &&
          diagnostic->severity == (i < 2 ? String("Recover") : String("Skip")) && !diagnostic->detail,
          "diagnostic order, severity, origin and absent detail are preserved");
  }
  check(parser::parse_pixi_particle_document("{}")->diagnostics.empty(), "missing diagnostic keys stay absent");
  bool null_error = false;
  try { static_cast<void>(parser::parse_pixi_particle_document(R"({"acceleration":null})")); }
  catch (const flight::TypeError&) { null_error = true; }
  check(null_error && parser::parse_pixi_particle(R"({"acceleration":null})")->max_particles == 1000,
        "direct null acceleration throws only in diagnostics after parsing");

  for (const auto* input : {"", "{", "[1,]", "{}x"}) {
    bool syntax_error = false;
    try { static_cast<void>(parser::parse_pixi_particle(input)); }
    catch (const flight::Error& error) { syntax_error = error.name() == String("Error") && error.message().starts_with("Invalid Pixi particle JSON: "); }
    check(syntax_error, "syntax errors keep the source format prefix and Error class");
  }
  for (const auto& [input, kind] : std::vector<std::pair<String, String>>{
      {"null", "null"}, {"[]", "array"}, {"[{}]", "array"}, {"false", "boolean"},
      {"1e400", "number"}, {"\"s\"", "string"}}) {
    bool root_error = false;
    try { static_cast<void>(parser::parse_pixi_particle(input)); }
    catch (const flight::Error& error) { root_error = error.message() == String("Invalid Pixi particle document: expected a JSON object, got ") + kind; }
    check(root_error, "root-kind rejection remains outside the syntax catch");
  }
  const auto zeros = parser::parse_pixi_particle(R"({"speed":{"start":0,"end":-0},"scale":{"start":-0,"end":0},"rotationSpeed":-0})");
  check(std::signbit(zeros->speed_min) && !std::signbit(zeros->speed_max) &&
        std::signbit(zeros->scale_min) && !std::signbit(zeros->scale_max) &&
        std::signbit(zeros->rotation_speed_min), "JSON signed zero survives endpoint math and factory fields");
  const auto range = parser::parse_pixi_particle(R"({"frequency":1e-320,"lifetime":{"min":1e400},"angle":{"min":1e308,"max":1e308}})");
  check(std::isinf(range->spawn_rate) && range->lifetime_min == 0.5 && std::isnan(range->direction_x),
        "JSON numeric range follows source finite guards and subsequent arithmetic");
  const auto whitespace = parser::parse_pixi_particle(String("{\"color\":{\"start\":\"") + String::from_char_code(0x00A0) + String("1f2030\"}}"));
  check(whitespace->color_start_r == 1.0 / 255, "Unicode whitespace reaches the original parseInt color path");
  const auto surrogate = parser::parse_pixi_particle(String("{\"color\":{\"start\":\"") + String::from_char_code(0xD800) + String("\"}}"));
  check(surrogate->color_start_r == 1, "literal lone UTF16 units remain valid JSON string values");
  {
    flight::UnicodeServiceScope unicode(std::make_shared<TestUnicode>());
    check(!parser::parse_pixi_particle(u"{\"blendMode\":\"ΟΣ\"}")->blend_mode &&
          !parser::parse_pixi_particle(u"{\"blendMode\":\"K\"}")->blend_mode,
          "non-ASCII blend strings use the configured host Unicode service and authored fallback");
  }
  return failures == 0 ? 0 : 1;
}
