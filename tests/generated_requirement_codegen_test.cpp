// Focused behavior tests for @flighthq/requirement-codegen at the current pin.
//
// The package is one planning kernel: given an inventory (the catalog) and the facts a producer recorded (a
// requirement set), it resolves what one backend must import and register, records what that backend has
// deliberately declined with the catalog's reason, and reports everything still unresolved. Every upstream case
// ports here one for one (packages/requirement-codegen/src/requirementCodegen.test.ts), and the disposition
// cases keep the distinction the feature exists for: a decision and an oversight must not look alike, and a
// disposition names one backend, one facet and one kind exactly.
//
// Two groups of cases go beyond the upstream suite, because the kernel has two halves the source tests do not
// reach. `expandRequirements` is private and untested upstream, and this repository supplies it through a source
// patch, so its equivalence claim is asserted here directly: expansion is ADDITIVE (the original requirement
// stays, because the parser backend still has to resolve it), a translation matches its exact key or a bare
// format namespace, and the expansion is deduplicated like every other requirement. The plan is also asserted to
// be an allocated entity, which is what the upstream case means by `EntityRuntimeKey in plan`.
#include <flight/requirement_codegen/contract.hpp>

#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

namespace {

int failures = 0;

bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
  return condition;
}

using Entry = flight::Ref<flight::types::RequirementCatalogEntry>;
using Requirement = flight::Ref<flight::types::Requirement>;
using RequirementRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<Requirement>>>;
using CatalogRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::RequirementCatalog>>>>;
using SetRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::RequirementSet>>>>;

// What the upstream suite means by `EntityRuntimeKey in plan`: the plan is an allocated entity, not a bare
// record. That is a type-level fact, so it is asserted where it can actually fail -- at compile time, against
// the emitted declarations rather than against a value that no input could make false.
using Plan = std::remove_reference_t<decltype(*flight::requirement_codegen::create_requirement_codegen_plan(
    std::declval<CatalogRow>(), std::declval<SetRow>(), flight::String()))>;
static_assert(std::is_base_of_v<flight::types::RequirementCodegenPlan, Plan>,
              "the emitted plan must be the codegen plan type, not a look-alike record");
static_assert(requires(Plan& plan) { plan.entity_runtime_key; },
              "the emitted plan must carry the entity runtime key slot allocate_entity/finish_entity fill in");

const flight::String& scene_node_kind() { return flight::types::requirement_facet->scene_node_kind; }
const flight::String& scene_shape_command() { return flight::types::requirement_facet->scene_shape_command; }
const flight::String& document_format() { return flight::types::requirement_facet->document_format; }

std::string text(const flight::String& value) { return value.to_utf8(); }

Entry entry(const char* backend, flight::String facet, const char* kind, const char* symbol, const char* impl_import,
            const char* registrar_import = "", const char* registrar_symbol = "") {
  flight::types::RequirementCatalogEntry value{.backend = flight::String(backend),
                                               .facet = facet,
                                               .implementation_import = flight::String(impl_import),
                                               .implementation_symbol = flight::String(symbol),
                                               .kind = flight::String(kind)};
  if (registrar_import[0] != '\0') value.registrar_import = flight::String(registrar_import);
  if (registrar_symbol[0] != '\0') value.registrar_symbol = flight::String(registrar_symbol);
  return flight::make_ref<flight::types::RequirementCatalogEntry>(value);
}

RequirementRow requirement(flight::String facet, const char* key) {
  return flight::make_structural_ref<flight::RowReadonly<flight::RowOf<Requirement>>>(
      flight::row_field<flight::RowKey<"facet">>(facet), flight::row_field<flight::RowKey<"key">>(flight::String(key)));
}

flight::Ref<flight::types::RequirementTranslation> translation(flight::String from_facet, const char* from_key,
                                                              flight::Array<Requirement> to) {
  return flight::make_ref<flight::types::RequirementTranslation>(flight::types::RequirementTranslation{
      .from = flight::make_ref<flight::types::Requirement>(
          flight::types::Requirement{.facet = from_facet, .key = flight::String(from_key)}),
      .to = to});
}

flight::Array<flight::Ref<flight::types::RequirementDisposition>> dispositions_of(
    flight::Array<flight::Ref<flight::types::RequirementDisposition>> values) {
  return values;
}

flight::Ref<flight::types::RequirementDisposition> disposition(flight::String facet, const char* backend,
                                                               const char* kind, const char* reason) {
  return flight::make_ref<flight::types::RequirementDisposition>(flight::types::RequirementDisposition{
      .backend = flight::String(backend), .facet = facet, .kind = flight::String(kind), .reason = flight::String(reason)});
}

CatalogRow catalog(flight::Array<Entry> entries) {
  return flight::make_structural_ref<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::RequirementCatalog>>>>(
      flight::row_field<flight::RowKey<"entries">>(entries));
}

CatalogRow catalog_with_dispositions(flight::Array<Entry> entries,
                                     flight::Array<flight::Ref<flight::types::RequirementDisposition>> dispositions) {
  return flight::make_structural_ref<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::RequirementCatalog>>>>(
      flight::row_field<flight::RowKey<"entries">>(entries),
      flight::row_field<flight::RowKey<"dispositions">>(dispositions_of(dispositions)));
}

CatalogRow catalog_with_translations(flight::Array<Entry> entries,
                                     flight::Array<flight::Ref<flight::types::RequirementTranslation>> translations) {
  return flight::make_structural_ref<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::RequirementCatalog>>>>(
      flight::row_field<flight::RowKey<"entries">>(entries),
      flight::row_field<flight::RowKey<"translations">>(translations));
}

SetRow set(flight::Array<flight::types::RequirementFacet> covers, flight::Array<RequirementRow> requirements) {
  return flight::make_structural_ref<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::RequirementSet>>>>(
      flight::row_field<flight::RowKey<"covers">>(covers),
      flight::row_field<flight::RowKey<"requirements">>(flight::array_of<RequirementRow>(requirements)));
}

std::string entries_text(const auto& plan) {
  std::string joined;
  for (const auto& value : plan->entries) {
    if (!joined.empty()) joined += ';';
    joined += text(flight::row_get<flight::RowKey<"implementationSymbol">>(value));
  }
  return joined;
}

// A declination carries the requirement it declined AND the catalog's reason. Only the reason is read here:
// the requirement member is declared and written, but no emitted module ever names it as a RowKey, and this
// SDK's structural member table is generated from exactly the keys generated code names -- so
// `row_get<RowKey<"requirement">>` on a declination view fails its schema assertion. Which requirement a
// declination names is therefore asserted by ELIMINATION instead: the case below asks about two requirements,
// disposes one and leaves the other, and the two lists then have to come out the way round they do.
std::string declined_text(const auto& plan) {
  std::string joined;
  for (const auto& value : plan->declined) {
    if (!joined.empty()) joined += ';';
    joined += text(flight::row_get<flight::RowKey<"reason">>(value));
  }
  return joined;
}

std::string unresolved_text(const auto& plan) {
  std::string joined;
  for (const auto& value : plan->unresolved) {
    if (!joined.empty()) joined += ';';
    joined += text(flight::row_get<flight::RowKey<"key">>(value));
  }
  return joined;
}

// createRequirementCodegenPlan selects the matching backend rows in requirement and catalog order, and the plan
// is an allocated entity.
void test_selects_matching_rows_in_order() {
  const auto catalog_row = catalog(flight::Array<Entry>{
      entry("webgl", scene_node_kind(), "Shape", "glShapeRenderer", "@flighthq/scene2d-gl", "@flighthq/render",
            "registerNodeRenderer"),
      entry("webgl", scene_node_kind(), "Shape", "registerGlShapeCommands", "@flighthq/scene2d-gl",
            "@flighthq/scene2d-gl", "registerGlShapeCommands"),
      entry("webgpu", scene_node_kind(), "Shape", "gpuShapeRenderer", "@flighthq/scene2d-wgpu"),
  });
  const auto plan = flight::requirement_codegen::create_requirement_codegen_plan(
      catalog_row,
      set(flight::Array<flight::types::RequirementFacet>{scene_node_kind()},
          flight::Array<RequirementRow>{requirement(scene_node_kind(), "Shape"),
                                        requirement(scene_node_kind(), "Sprite")}),
      flight::String("webgl"));

  check(text(plan->backend) == "webgl", "the plan carries the backend it was asked about");
  check(entries_text(plan) == "glShapeRenderer;registerGlShapeCommands",
        "both webgl rows for Shape are selected, in catalog order, and nothing else");
  check(unresolved_text(plan) == "Sprite", "the requirement no row satisfies is unresolved");
}

// deduplicates repeated positive requirements without treating covers as requests.
void test_deduplicates_without_treating_covers_as_requests() {
  const auto plan = flight::requirement_codegen::create_requirement_codegen_plan(
      catalog(flight::Array<Entry>{entry("webgl", scene_node_kind(), "Shape", "glShapeRenderer",
                                         "@flighthq/scene2d-gl")}),
      set(flight::Array<flight::types::RequirementFacet>{scene_node_kind(), scene_shape_command()},
          flight::Array<RequirementRow>{requirement(scene_node_kind(), "Shape"),
                                        requirement(scene_node_kind(), "Shape")}),
      flight::String("webgl"));

  check(entries_text(plan) == "glShapeRenderer", "a repeated requirement resolves once");
  check(unresolved_text(plan).empty(), "a deduplicated requirement is not unresolved");
  check(declined_text(plan).empty(), "nothing is declined here");
}

// returns an empty plan for empty catalog contents and requirements.
void test_empty_catalog_and_requirements() {
  const auto plan = flight::requirement_codegen::create_requirement_codegen_plan(
      catalog(flight::Array<Entry>{}),
      set(flight::Array<flight::types::RequirementFacet>{}, flight::Array<RequirementRow>{}),
      flight::String("webgl"));

  check(entries_text(plan).empty() && unresolved_text(plan).empty() && declined_text(plan).empty(),
        "an empty catalog and an empty set plan nothing");
}

// routes a disposed requirement to declined rather than unresolved, carrying the reason.
void test_disposed_requirement_is_declined_with_its_reason() {
  const auto plan = flight::requirement_codegen::create_requirement_codegen_plan(
      catalog_with_dispositions(flight::Array<Entry>{}, flight::Array<flight::Ref<flight::types::RequirementDisposition>>{
                                                           disposition(scene_node_kind(), "dom", "Tilemap",
                                                                       "no DOM tilemap path")}),
      set(flight::Array<flight::types::RequirementFacet>{scene_node_kind()},
          flight::Array<RequirementRow>{requirement(scene_node_kind(), "Tilemap")}),
      flight::String("dom"));

  check(unresolved_text(plan).empty(), "a disposed requirement is not reported as an oversight");
  check(declined_text(plan) == "no DOM tilemap path", "the decline carries the catalog's reason");
  check(entries_text(plan).empty(), "a declined requirement imports and registers nothing");
}

// leaves an undisposed gap in unresolved, so an oversight still reports.
void test_undisposed_gap_reports() {
  const auto plan = flight::requirement_codegen::create_requirement_codegen_plan(
      catalog_with_dispositions(flight::Array<Entry>{}, flight::Array<flight::Ref<flight::types::RequirementDisposition>>{}),
      set(flight::Array<flight::types::RequirementFacet>{scene_node_kind()},
          flight::Array<RequirementRow>{requirement(scene_node_kind(), "Tilemap")}),
      flight::String("dom"));

  check(declined_text(plan).empty(), "nothing was declined");
  check(unresolved_text(plan) == "Tilemap", "an undisposed gap is unresolved");
}

// A disposition names one backend, one facet and one kind: any loose match would silence requirements nobody
// considered, which is what the feature exists to prevent rather than introduce.
void test_disposition_axes_are_exact() {
  const auto disposed = [](flight::String facet) {
    return catalog_with_dispositions(
        flight::Array<Entry>{},
        flight::Array<flight::Ref<flight::types::RequirementDisposition>>{disposition(facet, "dom", "Tilemap",
                                                                                     "no DOM tilemap path")});
  };
  const auto asked = [](const char* backend, const char* kind) {
    return set(flight::Array<flight::types::RequirementFacet>{scene_node_kind()},
               flight::Array<RequirementRow>{requirement(scene_node_kind(), kind)});
  };

  check(unresolved_text(flight::requirement_codegen::create_requirement_codegen_plan(
            disposed(scene_node_kind()), asked("canvas", "Tilemap"), flight::String("canvas"))) == "Tilemap",
        "a disposition for another backend does not apply");
  check(unresolved_text(flight::requirement_codegen::create_requirement_codegen_plan(
            disposed(scene_node_kind()), asked("dom", "QuadBatch"), flight::String("dom"))) == "QuadBatch",
        "a disposition for another kind does not apply");
  check(unresolved_text(flight::requirement_codegen::create_requirement_codegen_plan(
            disposed(document_format()), asked("dom", "Tilemap"), flight::String("dom"))) == "Tilemap",
        "a disposition for another facet does not apply");
}

// A disposition is a fallback for a gap, never an override: if the backend CAN satisfy it, it does.
void test_disposition_never_overrides_a_resolving_row() {
  const auto plan = flight::requirement_codegen::create_requirement_codegen_plan(
      catalog_with_dispositions(
          flight::Array<Entry>{entry("dom", scene_node_kind(), "Tilemap", "domTilemapRenderer", "@flighthq/scene2d-dom")},
          flight::Array<flight::Ref<flight::types::RequirementDisposition>>{
              disposition(scene_node_kind(), "dom", "Tilemap", "stale decision")}),
      set(flight::Array<flight::types::RequirementFacet>{scene_node_kind()},
          flight::Array<RequirementRow>{requirement(scene_node_kind(), "Tilemap")}),
      flight::String("dom"));

  check(entries_text(plan) == "domTilemapRenderer", "a row that resolves is used");
  check(declined_text(plan).empty() && unresolved_text(plan).empty(), "and nothing is declined or reported");
}

// reports one requirement declined and another missing in the same plan.
void test_declined_and_missing_in_one_plan() {
  const auto plan = flight::requirement_codegen::create_requirement_codegen_plan(
      catalog_with_dispositions(flight::Array<Entry>{}, flight::Array<flight::Ref<flight::types::RequirementDisposition>>{
                                                           disposition(scene_node_kind(), "dom", "Tilemap",
                                                                       "no DOM tilemap path")}),
      set(flight::Array<flight::types::RequirementFacet>{scene_node_kind()},
          flight::Array<RequirementRow>{requirement(scene_node_kind(), "Tilemap"),
                                        requirement(scene_node_kind(), "QuadBatch")}),
      flight::String("dom"));

  // Two requirements were asked about and only one was disposed of: the declined list holds the disposed one
  // and the unresolved list names the other, which is what pins the requirement a declination stands for.
  check(declined_text(plan) == "no DOM tilemap path", "the deliberate gap is recorded as declined");
  check(unresolved_text(plan) == "QuadBatch", "the oversight is reported as unresolved, by elimination the other");
}

// Expansion is ADDITIVE: the original requirement stays, because the parser backend still has to resolve it.
void test_translation_is_additive() {
  const auto plan = flight::requirement_codegen::create_requirement_codegen_plan(
      catalog_with_translations(
          flight::Array<Entry>{entry("gl", scene_node_kind(), "Sprite", "glSpriteRenderer", "@flighthq/scene2d-gl")},
          flight::Array<flight::Ref<flight::types::RequirementTranslation>>{translation(
              scene_node_kind(), "Shape", flight::Array<Requirement>{flight::make_ref<flight::types::Requirement>(
                                              flight::types::Requirement{.facet = scene_node_kind(),
                                                                         .key = flight::String("Sprite")})})}),
      set(flight::Array<flight::types::RequirementFacet>{scene_node_kind()},
          flight::Array<RequirementRow>{requirement(scene_node_kind(), "Shape")}),
      flight::String("gl"));

  check(entries_text(plan) == "glSpriteRenderer", "a translation adds the render requirement the format implies");
  check(unresolved_text(plan) == "Shape",
        "and the original requirement is still resolved by the parser backend, so it stays");
}

// A translation matches its exact key, or a bare format namespace, and nothing else.
void test_translation_match_shapes() {
  const auto catalog_row = catalog_with_translations(
      flight::Array<Entry>{}, flight::Array<flight::Ref<flight::types::RequirementTranslation>>{translation(
                                 document_format(), "swf", flight::Array<Requirement>{flight::make_ref<flight::types::Requirement>(
                                                               flight::types::Requirement{.facet = scene_node_kind(),
                                                                                          .key = flight::String("MovieClip")})})});
  const auto asked = [](const char* key) {
    return set(flight::Array<flight::types::RequirementFacet>{document_format()},
               flight::Array<RequirementRow>{requirement(document_format(), key)});
  };
  const auto plan_for = [&](const char* key) {
    return flight::requirement_codegen::create_requirement_codegen_plan(catalog_row, asked(key), flight::String("gl"));
  };

  check(unresolved_text(plan_for("swf.NestedTag")) == "swf.NestedTag;MovieClip",
        "a namespace translation applies to every requirement in the namespace, additively");
  check(unresolved_text(plan_for("tiled")) == "tiled",
        "a requirement outside the namespace is untouched, and a bare key matches no namespace");
}

}  // namespace

int main() {
  test_selects_matching_rows_in_order();
  test_deduplicates_without_treating_covers_as_requests();
  test_empty_catalog_and_requirements();
  test_disposed_requirement_is_declined_with_its_reason();
  test_undisposed_gap_reports();
  test_disposition_axes_are_exact();
  test_disposition_never_overrides_a_resolving_row();
  test_declined_and_missing_in_one_plan();
  test_translation_is_additive();
  test_translation_match_shapes();
  if (failures != 0) {
    std::cerr << failures << " generated requirement-codegen check(s) failed\n";
    return 1;
  }
  std::cout << "generated requirement-codegen: all checks passed\n";
  return 0;
}
