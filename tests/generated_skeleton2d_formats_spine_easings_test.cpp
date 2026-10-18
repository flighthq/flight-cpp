// Expectations use untouched Flight a627849 buildSpineBinarySegmentEasings and its actual callees.
#include <flight/skeleton2d_formats/spine_binary_bone_timeline_handler.hpp>
#include <cmath>
#include <iostream>
#include <type_traits>
#include <vector>

namespace {
using flight::Array;
using flight::String;
namespace sdk = flight::skeleton2d_formats;
template <typename> struct FirstArgument;
template <typename R, typename A, typename... Rest>
struct FirstArgument<R (*)(A, Rest...)> { using Type = A; };
using TimelineRef = typename FirstArgument<decltype(&sdk::build_spine_binary_segment_easings)>::Type;
using Timeline = typename TimelineRef::element_type;
using Samples = std::vector<std::optional<std::vector<double>>>;
struct Diagnostic { String severity; String kind; String origin; double segments; };
int failures = 0;
int cases = 0;
int samples = 0;
void check(bool okay, const char* name, const char* message) {
  if (!okay) { std::cerr << name << ": " << message << '\n'; ++failures; }
}
std::vector<double> snapshot(const Array<double>& values) {
  return std::vector<double>(values.begin(), values.end());
}
void run(const char* name, Array<std::optional<Array<double>>> curves, Array<double> times,
         Array<double> values, double components, std::optional<Samples> expected,
         std::initializer_list<Diagnostic> expectedDiagnostics) {
  ++cases;
  const auto timeline = flight::make_ref<Timeline>();
  timeline->curves = curves; timeline->times = times; timeline->values = values;
  const auto curveOwner = curves.identity();
  const auto timeOwner = times.identity();
  const auto valueOwner = values.identity();
  const auto beforeTimes = snapshot(times), beforeValues = snapshot(values);
  std::vector<const void*> innerOwners;
  Samples beforeCurves;
  for (const auto& curve : curves) {
    innerOwners.push_back(curve ? curve->identity() : nullptr);
    beforeCurves.push_back(curve ? std::optional{snapshot(*curve)} : std::nullopt);
  }
  const Array<flight::Ref<flight::types::ImportDiagnostic>> diagnostics;
  const auto output = sdk::build_spine_binary_segment_easings(timeline,components,diagnostics);
  check(output.has_value() == expected.has_value(),name,"nullable result");
  constexpr double progress[] = {0,.125,.25,.5,.75,.875,1};
  if (output && expected) {
    check(output->size() == expected->size(),name,"null slots and result order");
    for (std::size_t i = 0; i < expected->size() && i < output->size(); ++i) {
      const auto& got = (*output)[i]; const auto& want = (*expected)[i];
      check(got.has_value() == want.has_value(),name,"null easing position");
      if (!got || !want) continue;
      for (std::size_t j = 0; j < want->size(); ++j) {
        ++samples;
        check(std::abs((*got)(progress[j]) - (*want)[j]) <= 1e-6,name,"original easing numeric result");
      }
    }
    const auto again = sdk::build_spine_binary_segment_easings(timeline,components);
    check(again && again->identity() != output->identity(),name,"fresh output owner");
  }
  check(diagnostics.size() == expectedDiagnostics.size(),name,"diagnostic count");
  std::size_t i = 0;
  for (const auto& want : expectedDiagnostics) {
    if (i >= diagnostics.size()) break;
    const auto& got = diagnostics[i++];
    check(got->severity == want.severity && got->kind == want.kind && got->origin == want.origin,
          name,"original diagnostic fields");
    const auto field = got->detail ? got->detail->get(String("segments")) : std::nullopt;
    check(field && std::holds_alternative<double>(*field) && std::get<double>(*field) == want.segments,
          name,"original diagnostic detail");
  }
  check(timeline->curves.identity() == curveOwner && timeline->times.identity() == timeOwner &&
        timeline->values.identity() == valueOwner,name,"original input owners");
  check(snapshot(times) == beforeTimes && snapshot(values) == beforeValues &&
        curves.size() == beforeCurves.size(),name,"input arrays unchanged");
  for (std::size_t k = 0; k < curves.size() && k < beforeCurves.size(); ++k) {
    check(curves[k].has_value() == beforeCurves[k].has_value(),name,"input null slots unchanged");
    if (curves[k] && beforeCurves[k]) {
      check(curves[k]->identity() == innerOwners[k] && snapshot(*curves[k]) == *beforeCurves[k],
            name,"same live curve owner and values");
    }
  }
}
}
int main() {
  run("empty", {}, {}, {}, 1.0, std::nullopt, {});
  run("nulls", {std::nullopt,std::nullopt}, {0.0,1.0,2.0}, {0.0,1.0,2.0}, 1.0, std::nullopt, {});
  run("linear", {Array<double>{0.25,0.25,0.75,0.75}}, {0.0,1.0}, {0.0,1.0}, 1.0, Samples{std::vector<double>{0.0,0.12500004624442734,0.2500000109635573,0.5,0.7499999890364427,0.8749999537555725,1.0}}, {});
  run("mixed", {std::nullopt,Array<double>{1.25,1.25,1.75,1.75}}, {0.0,1.0,2.0}, {0.0,1.0,2.0}, 1.0, Samples{std::nullopt,std::vector<double>{0.0,0.12500004624442734,0.2500000109635573,0.5,0.7499999890364427,0.8749999537555725,1.0}}, {});
  run("zero span", {Array<double>{0.25,0.25,0.75,0.75}}, {0.0,0.0}, {0.0,1.0}, 1.0, std::nullopt, {});
  run("empty points", {Array<double>{}}, {0.0,1.0}, {0.0,1.0}, 1.0, std::nullopt, {});
  run("zero rise", {Array<double>{0.25,0.25,0.75,0.75}}, {0.0,1.0}, {1.0,1.0}, 1.0, std::nullopt, {});
  run("divergent", {Array<double>{0.25,0.2,0.75,0.8,0.1,0.9,0.9,0.1}}, {0.0,1.0}, {0.0,0.0,1.0,1.0}, 2.0, Samples{std::vector<double>{0.0,0.11171593633412151,0.23647123862829741,0.5,0.7635287613717027,0.8882840636658784,1.0}}, {{String("Skip"),String("spine.per-component-curve-easing-unsupported"),String("buildSpineBinarySegmentEasings"),4.0}});
  run("widest", {Array<double>{0.1,0.3,0.9,0.7,0.1,5.9,0.9,7.1}}, {0.0,1.0}, {0.0,5.0,1.0,8.0}, 2.0, Samples{std::vector<double>{0.0,0.18233366865499614,0.2992055761647088,0.5,0.7007944238352911,0.8176663313450038,1.0}}, {});
  run("clamped", {Array<double>{-2.0,-0.5,3.0,1.5}}, {0.0,1.0}, {0.0,1.0}, 1.0, Samples{std::vector<double>{0.0,-0.019093976243142,0.13547226845088767,0.5,0.8645277315491122,1.0190939762431421,1.0}}, {});
  run("null then curve then empty", {std::nullopt,Array<double>{1.2,1.1,1.8,1.9},Array<double>{}}, {0.0,1.0,2.0,3.0}, {0.0,1.0,2.0,3.0}, 1.0, Samples{std::nullopt,std::vector<double>{0.0,0.09748627154653586,0.22371604943069087,0.5,0.7762839505693091,0.9025137284534643,1.0},std::nullopt}, {});
  std::cout << cases << " untouched-source easing cases, " << samples << " samples\n";
  return failures ? 1 : 0;
}
