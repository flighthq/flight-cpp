// Focused behavior tests for @flighthq/requirement at the current pin.
//
// The package is small and entirely observable. A requirement set is a pair of sorted, deduplicated
// dimensions -- the facets a producer inspected, and the facts it recorded -- and every operation on it is
// set algebra over that pair. Each case below ports one upstream case
// (packages/requirement/src/requirementSet.test.ts, requirementCollector.test.ts) and asserts the same
// property, plus three the TypeScript suite states in prose and a C++ carrier can show directly: the set
// COPIES its inputs, the identity it deduplicates by separates a pair whose halves would run together under
// a bare join, and the facet vocabulary the collector declares is the pin's own set of names.
//
// Two notes on what is deliberately NOT asserted. `initializeRequirementSet` has no upstream assertion
// beyond being callable, and its behavior is exactly the two creation cases below, which route through it;
// upstream's `reportRequirement` test asserts only that an unengaged sink records nothing, which over a
// by-value optional is the `!sink.has_value()` early return, exercised below through an engaged sink that
// must stay untouched.
#include <flight/requirement/contract.hpp>

#include <iostream>
#include <string>

namespace {

int failures = 0;

bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
  return condition;
}

using Requirement = flight::Ref<flight::types::Requirement>;
using RequirementRow =
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::Requirement>>>>;

// The pin's own facet vocabulary, read through the emitted singleton rather than retyped as literals: the
// collector declares these four, and a test that spelled them itself would agree with a drifted emitter.
const flight::String& scene_blend_mode() { return flight::types::requirement_facet->scene_blend_mode; }
const flight::String& scene_material_kind() { return flight::types::requirement_facet->scene_material_kind; }
const flight::String& scene_node_kind() { return flight::types::requirement_facet->scene_node_kind; }
const flight::String& scene_shape_command() { return flight::types::requirement_facet->scene_shape_command; }

// One producer fact in the shape a parser hands the collector: a row over a fresh Requirement.
RequirementRow requirement(flight::String facet_name, flight::String key) {
  return flight::make_structural_ref<flight::RowReadonly<flight::RowOf<Requirement>>>(
      flight::row_field<flight::RowKey<"facet">>(facet_name), flight::row_field<flight::RowKey<"key">>(key));
}

std::string text(const flight::String& value) { return value.to_utf8(); }

std::string covers_text(const flight::Ref<flight::types::RequirementSet>& set) {
  std::string joined;
  for (const auto& cover : set->covers) {
    if (!joined.empty()) joined += ',';
    joined += text(cover);
  }
  return joined;
}

std::string requirements_text(const flight::Ref<flight::types::RequirementSet>& set) {
  std::string joined;
  for (const auto& entry : set->requirements) {
    if (!joined.empty()) joined += ';';
    joined += text(flight::row_get<flight::RowKey<"facet">>(entry));
    joined += '=';
    joined += text(flight::row_get<flight::RowKey<"key">>(entry));
  }
  return joined;
}

void test_facet_vocabulary_matches_the_pin() {
  check(text(scene_blend_mode()) == "scene.blend-mode" && text(scene_material_kind()) == "scene.material-kind" &&
            text(scene_node_kind()) == "scene.node-kind" && text(scene_shape_command()) == "scene.shape-command",
        "the emitted facet singleton carries the source's declared names");
}

// createRequirementSet: copies, sorts, and deduplicates both dimensions.
void test_create_copies_sorts_and_deduplicates() {
  flight::Array<flight::types::RequirementFacet> covers{scene_node_kind(), scene_blend_mode()};
  flight::Array<RequirementRow> requirements{
      requirement(scene_node_kind(), flight::String("Shape")),
      requirement(scene_blend_mode(), flight::String("Multiply")),
      requirement(scene_node_kind(), flight::String("Shape")),
  };
  const auto set = flight::requirement::create_requirement_set(covers, requirements);

  check(covers_text(set) == "scene.blend-mode,scene.node-kind", "covers are sorted and deduplicated");
  check(requirements_text(set) == "scene.blend-mode=Multiply;scene.node-kind=Shape",
        "requirements are sorted by facet then key and deduplicated by identity");

  // The upstream case mutates its inputs after the call for exactly this reason. A carrier is shared here
  // -- `flight::Array` aliases its storage -- so a set that kept the caller's array instead of its own copy
  // would observe both pushes, and this is the assertion that holds the copy.
  covers.push(scene_shape_command());
  requirements.push(requirement(scene_node_kind(), flight::String("Sprite")));
  check(covers_text(set) == "scene.blend-mode,scene.node-kind", "a later push to the covers array is not seen");
  check(requirements_text(set) == "scene.blend-mode=Multiply;scene.node-kind=Shape",
        "a later push to the requirements array is not seen");
}

// diffRequirementSets: subtracts facts only for facets both operands cover.
void test_diff_only_subtracts_covered_facets() {
  const auto left = flight::requirement::create_requirement_set(
      flight::Array<flight::types::RequirementFacet>{scene_blend_mode(), scene_node_kind()},
      flight::Array<RequirementRow>{
          requirement(scene_blend_mode(), flight::String("Multiply")),
          requirement(scene_node_kind(), flight::String("Shape")),
      });
  const auto baseline = flight::requirement::create_requirement_set(
      flight::Array<flight::types::RequirementFacet>{scene_node_kind()},
      flight::Array<RequirementRow>{
          requirement(scene_node_kind(), flight::String("Sprite")),
      });
  const auto result = flight::requirement::diff_requirement_sets(left, baseline);

  check(covers_text(result) == "scene.node-kind", "completeness is the intersection of the two cover sets");
  // The blend-mode fact is dropped because the baseline did not inspect that facet, so it says nothing about
  // it; the node-kind fact survives because the baseline never asserted it. The baseline's own fact is a
  // negative claim and never enters a difference.
  check(requirements_text(result) == "scene.node-kind=Shape",
        "a fact is kept only where its facet is covered by both and the baseline did not assert it");
}

// mergeRequirementSets: unions facts while intersecting completeness.
void test_merge_unions_facts_and_intersects_completeness() {
  const auto first = flight::requirement::create_requirement_set(
      flight::Array<flight::types::RequirementFacet>{scene_blend_mode(), scene_node_kind()},
      flight::Array<RequirementRow>{
          requirement(scene_node_kind(), flight::String("Shape")),
      });
  const auto second = flight::requirement::create_requirement_set(
      flight::Array<flight::types::RequirementFacet>{scene_node_kind()},
      flight::Array<RequirementRow>{
          requirement(scene_node_kind(), flight::String("Sprite")),
      });
  const auto merged = flight::requirement::merge_requirement_sets(
      flight::Array<flight::Ref<flight::types::RequirementSet>>{first, second});

  check(covers_text(merged) == "scene.node-kind", "a facet is complete in the merge only if every input covered it");
  check(requirements_text(merged) == "scene.node-kind=Shape;scene.node-kind=Sprite",
        "facts are unioned, so a partial producer still contributes its positive facts");

  const auto empty = flight::requirement::merge_requirement_sets(
      flight::Array<flight::Ref<flight::types::RequirementSet>>{});
  check(covers_text(empty).empty() && requirements_text(empty).empty(), "merging nothing yields an empty set");
}

// collectScene2DRequirements: deterministic distinct requirements and the facets the collector inspects.
void test_collector_declares_the_facets_it_inspects() {
  const auto set = flight::requirement::collect_scene2_drequirements([](flight::Array<Requirement> sink) {
    flight::requirement::report_requirement(sink, scene_node_kind(), flight::String("Shape"));
    flight::requirement::report_requirement(sink, scene_blend_mode(), flight::String("Multiply"));
    flight::requirement::report_requirement(sink, scene_node_kind(), flight::String("Shape"));
  });

  check(covers_text(set) == "scene.blend-mode,scene.material-kind,scene.node-kind,scene.shape-command",
        "the collector declares exactly the four facets it inspects");
  check(requirements_text(set) == "scene.blend-mode=Multiply;scene.node-kind=Shape",
        "the reports are deduplicated and ordered");
}

// reportRequirement: an unengaged sink records nothing, an engaged one receives the fact.
void test_report_records_only_with_a_sink() {
  flight::Array<Requirement> sink;
  flight::requirement::report_requirement(std::nullopt, scene_node_kind(), flight::String("Shape"));
  check(sink.size() == 0, "an unengaged sink receives nothing");

  flight::requirement::report_requirement(sink, scene_node_kind(), flight::String("Shape"));
  check(sink.size() == 1, "an engaged sink receives the fact");
  check(sink[0]->facet == scene_node_kind() && text(sink[0]->key) == "Shape",
        "the recorded fact carries the reported facet and key");

  flight::requirement::report_requirement(std::nullopt, scene_blend_mode(), flight::String("Multiply"));
  check(sink.size() == 1, "a later unengaged report adds nothing to an engaged sink");
}

// The identity a requirement is deduplicated by is an ordered pair, and the pair must not run together.
void test_identity_separates_the_pair() {
  const auto plain = flight::requirement::requirement_identity(flight::String("scene.a"), flight::String("bc"));
  check(plain == flight::requirement::requirement_identity(flight::String("scene.a"), flight::String("bc")),
        "the identity is stable for one pair");
  // A bare join of the two halves would make both of these "scene.abc". The source writes a NUL between
  // them for exactly this reason, and this is the assertion that holds the separator in place.
  check(plain != flight::requirement::requirement_identity(flight::String("scene.ab"), flight::String("c")),
        "a shifted facet/key boundary is a different requirement");

  const auto set = flight::requirement::create_requirement_set(
      flight::Array<flight::types::RequirementFacet>{flight::String("scene.a"), flight::String("scene.ab")},
      flight::Array<RequirementRow>{
          requirement(flight::String("scene.a"), flight::String("bc")),
          requirement(flight::String("scene.ab"), flight::String("c")),
      });
  check(requirements_text(set) == "scene.a=bc;scene.ab=c",
        "two facts whose halves would concatenate alike stay two distinct facts");
}

}  // namespace

int main() {
  test_facet_vocabulary_matches_the_pin();
  test_create_copies_sorts_and_deduplicates();
  test_diff_only_subtracts_covered_facets();
  test_merge_unions_facts_and_intersects_completeness();
  test_collector_declares_the_facets_it_inspects();
  test_report_records_only_with_a_sink();
  test_identity_separates_the_pair();
  if (failures != 0) {
    std::cerr << failures << " generated requirement check(s) failed\n";
    return 1;
  }
  std::cout << "generated requirement: all checks passed\n";
  return 0;
}
