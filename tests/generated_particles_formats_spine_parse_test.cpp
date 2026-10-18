#include <flight/particles_formats/spine_parse.hpp>

#include <cmath>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
bool near(double value, double expected) { return std::abs(value - expected) < 1e-12; }
void fails(const flight::String& input, const flight::String& message, bool prefix = false) {
  for (bool document : {false, true}) {
    bool caught = false;
    try {
      if (document) (void)flight::particles_formats::parse_spine_particle_document(input);
      else (void)flight::particles_formats::parse_spine_particle(input);
    } catch (const flight::Error& error) {
      caught = true;
      check(error.name() == flight::String("Error"), "source application errors retain their Error class");
      check(prefix ? error.message().starts_with(message) : error.message() == message,
            "both entrypoints preserve validation path, order and application message");
    }
    check(caught, "invalid input fails at both original public entrypoints");
  }
}
}

int main() {
  namespace parser = flight::particles_formats;
  using flight::String;
  const auto defaults = parser::parse_spine_particle("{}");
  const auto document = parser::parse_spine_particle_document("{}");
  check(defaults != document->config && document->document->life != document->document->velocity,
        "each call and authored range literal creates its own fresh owner");
  check(defaults->max_particles == 500 && defaults->spawn_rate == 20 && defaults->loop &&
        defaults->duration == 0 && defaults->lifetime_min == 0.5 && defaults->lifetime_max == 1.5 &&
        defaults->speed_min == 50 && defaults->speed_max == 150 && defaults->scale_min == 1 &&
        defaults->scale_max == 1 && defaults->scale_end == 0 && defaults->alpha_start == 1 &&
        defaults->alpha_end == 0 && near(defaults->direction_x, 0) && defaults->direction_y == -1 &&
        !defaults->color_curve && !defaults->alpha_curve && document->diagnostics.empty(),
        "missing fields retain original config and factory defaults");
  check(document->document->emission->low == 10 && document->document->emission->high == 30 &&
        document->document->life->low == 500 && document->document->life->high == 1500 &&
        document->document->duration == -1 && document->document->tint.size() == 1 &&
        document->document->alpha.size() == 2 && document->document->images.empty(),
        "the full document retains authored units and distinct fallback timelines");

  const auto parsed = parser::parse_spine_particle_document(R"({"name":"spark","maxParticles":4294967297,"continuous":false,"duration":2500,"emission":{"low":4,"high":12},"life":{"low":400,"high":800},"velocity":{"low":200,"high":50},"angle":{"low":0,"high":60},"rotation":{"low":-90,"high":180},"spawnShape":"ellipse","spawnWidth":{"low":8,"high":12},"spawnHeight":{"low":10,"high":10},"scale":{"low":2,"high":4},"scaleEnd":{"low":3,"high":6},"lifeOffset":{"low":1},"x":{"high":2},"y":{"low":3},"premultiplied":true,"blendMode":"additive","tint":[{"time":1,"color":"0000ff"},{"time":0,"color":"ff0000"},{"time":0.5,"color":"00ff00"}],"alpha":[{"time":1,"alpha":0},{"time":0,"alpha":1},{"time":0.5,"alpha":0.25}],"images":["a","a","\ud800"]})");
  const auto config = parsed->config;
  const auto raw = parsed->document;
  check(config->max_particles == 1 && !config->loop && config->duration == 2.5 &&
        config->spawn_rate == 8 && config->lifetime_min == 0.4 && config->lifetime_max == 0.8 &&
        config->speed_min == 200 && config->speed_max == 50 && config->scale_end == 1.5 &&
        config->emitter_shape == String("circle") && config->emitter_radius == 5 &&
        config->blend_mode == std::optional<String>("add") && near(config->direction_x, std::sqrt(3.) / 2) &&
        near(config->direction_y, -0.5) && near(config->spread, std::acos(-1.) / 6),
        "ranges, bitwise, duration, units, reversed limits, direction and blend stay source-authored");
  check(config->color_start_b == 1 && config->color_end_g == 1 && config->alpha_start == 0 &&
        config->alpha_end == 0.25 && config->alpha_curve && config->alpha_curve->size() == 33 &&
        (*config->alpha_curve)[0] == 1 && (*config->alpha_curve)[16] == 0.25 &&
        (*config->alpha_curve)[32] == 0 && config->color_curve && config->color_curve->size() == 99 &&
        (*config->color_curve)[0] == 1 && (*config->color_curve)[49] == 1 &&
        (*config->color_curve)[98] == 1,
        "original endpoint order differs deliberately from the real sorted multistop curve");
  check(raw->name == String("spark") && raw->duration == 2500 && raw->life->low == 400 &&
        raw->velocity->low == 200 && raw->velocity->high == 50 && raw->tint[0]->time == 1 &&
        raw->alpha[0]->time == 1 && raw->blend_mode == String("additive") &&
        raw->images.size() == 3 && raw->images[0] == raw->images[1] &&
        raw->images[2].native() == std::u16string(1, char16_t(0xd800)),
        "fresh document owners preserve authored order, units, duplicates and exact UTF16");
  const std::vector<String> kinds{"spine.life-offset-unsupported", "spine.position-range-unsupported",
                                  "spine.premultiplied-informational"};
  check(parsed->diagnostics.size() == kinds.size(), "original diagnostic count");
  for (std::size_t i = 0; i < parsed->diagnostics.size(); ++i)
    check(parsed->diagnostics[i]->kind == kinds[i] &&
          parsed->diagnostics[i]->origin == String("collectSpineDiagnostics") &&
          parsed->diagnostics[i]->severity == flight::types::import_diagnostic_severity->skip, "diagnostic kind, order, origin and severity");
  raw->tint[0]->color = "modified";
  check(config->color_start_b == 1 && (*config->color_curve)[98] == 1,
        "document keyframes and baked config retain their distinct source-authored owners");

  for (const auto& value : std::vector<String>{"null", "false", "0", "\"text\"", "[]"}) {
    const auto input = String("{\"life\":") + value + String(",\"tint\":") + value +
                       String(",\"alpha\":") + value + String(",\"images\":") + value + String("}");
    const auto result = parser::parse_spine_particle_document(input);
    check(result->config->lifetime_min == 0.5 && result->config->lifetime_max == 1.5 &&
          result->document->images.empty(), "nonrecord ranges and nonarray timelines keep defaults");
    if (value == String("[]")) check(result->document->tint.empty() && result->document->alpha.empty(),
                                   "present empty arrays remain empty in the document");
  }
  check(!parser::parse_spine_particle(R"({"blendMode":"ADD"})")->blend_mode &&
        parser::parse_spine_particle_document(R"({"blendMode":"ADD","spawnShape":"line"})")->document->blend_mode == String("normal"),
        "unknown config blend remains null while document blend uses its own fallback");
  check(parser::parse_spine_particle(R"({"spawnShape":"ellipse","spawnWidth":{"low":2,"high":4},"spawnHeight":{"low":4,"high":6}})")->emitter_shape == String("rect"),
        "unequal ellipse extents map to the authored rectangle");
  const auto nonfinite = parser::parse_spine_particle(R"({"life":{"low":1e999,"high":-1e999},"velocity":{"low":-0},"scale":{"low":1e999,"high":-1e999}})");
  check(std::isinf(nonfinite->lifetime_min) && std::isinf(nonfinite->lifetime_max) &&
        nonfinite->lifetime_max < 0 && std::signbit(nonfinite->speed_min),
        "JSON overflow and signed zero survive numeric-only reads");
  fails("{", "Invalid Spine particle JSON: ", true);
  for (const auto& [input, kind] : std::vector<std::pair<String, String>>{
      {"null", "null"}, {"[]", "array"}, {"0", "number"}, {"false", "boolean"}, {"\"x\"", "string"}})
    fails(input, String("Invalid Spine particle document: expected a JSON object, got ") + kind);
  for (const auto& [input, path] : std::vector<std::pair<String, String>>{
      {R"({"tint":[null],"alpha":[false],"images":[3]})", "tint[0]"},
      {R"({"tint":[{},[]],"alpha":[false]})", "tint[1]"},
      {R"({"alpha":[{},2],"images":[3]})", "alpha[1]"}})
    fails(input, String("Invalid Spine particle document: ") + path + String(" must be a JSON object"));
  fails(R"({"images":["ok",null]})", "Invalid Spine particle document: images[1] must be a string");
  if (!failures) std::cout << "Original generated Spine particle public parser and full document PASS\n";
  return failures ? 1 : 0;
}
