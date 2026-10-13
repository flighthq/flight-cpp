// Focused native parity for the independently emitted @flighthq/scene-document YAML subset.
// Full FlightDocument text formatting/materialization remains compiler-refused and is not imitated
// here; this suite pins the parser's exact tree, identity, order, limits, and refusal surface.
// Measured mutations: nesting 64->4 (2 failures), scalar limit 65536->64 (2), anchor reason
// anchor->alias (2), and block duplicate-key->expected-mapping-key (1).
#include <flight/scene_document/scene_document_yaml_subset.hpp>

#include <initializer_list>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace scene_document = flight::scene_document;

using Value = scene_document::SceneDocumentYamlSubsetValue;
using Mapping = scene_document::SceneDocumentYamlSubsetMapping;
using Sequence = flight::Array<Value>;
using Success = scene_document::SceneDocumentYamlSubsetSuccess;
using Refusal = scene_document::SceneDocumentYamlSubsetRefusal;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

flight::Ref<Success> success(const scene_document::SceneDocumentYamlSubsetResult& result) {
  const auto* value = std::get_if<flight::Ref<Success>>(&result);
  return value == nullptr ? nullptr : *value;
}

flight::Ref<Refusal> refusal(const scene_document::SceneDocumentYamlSubsetResult& result) {
  const auto* value = std::get_if<flight::Ref<Refusal>>(&result);
  return value == nullptr ? nullptr : *value;
}

const Mapping* as_mapping(const Value& value) {
  if (!value.has_value()) return nullptr;
  return std::get_if<Mapping>(&value.value());
}

const Sequence* as_sequence(const Value& value) {
  if (!value.has_value()) return nullptr;
  return std::get_if<Sequence>(&value.value());
}

const flight::String* as_string(const Value& value) {
  if (!value.has_value()) return nullptr;
  return std::get_if<flight::String>(&value.value());
}

const double* as_number(const Value& value) {
  if (!value.has_value()) return nullptr;
  return std::get_if<double>(&value.value());
}

const bool* as_boolean(const Value& value) {
  if (!value.has_value()) return nullptr;
  return std::get_if<bool>(&value.value());
}

bool keys_equal(const Mapping& mapping,
                std::initializer_list<const char*> expected) {
  const auto keys = mapping.enumerable_keys();
  if (keys.size() != expected.size()) return false;
  std::size_t index = 0;
  for (const char* key : expected) {
    if (keys[index++] != flight::String(key)) return false;
  }
  return true;
}

flight::Ref<Refusal> expect_refusal(const std::string& source,
                                    const char* reason) {
  const auto result = scene_document::parse_scene_document_yaml_subset(
      flight::String(source));
  const auto rejected = refusal(result);
  check(rejected != nullptr, std::string("input is refused as ") + reason);
  if (rejected != nullptr) {
    check(rejected->kind == flight::String(reason),
          std::string("refusal reason is exactly ") + reason);
    check(rejected->offset >= 0 && rejected->line >= 1 &&
              rejected->column >= 1,
          std::string("refusal has a source position for ") + reason);
  }
  return rejected;
}

void schema_assets_metadata_hierarchy_and_order() {
  const flight::String source(R"YAML(flight: 1
defaultScene: 0
resources:
  - kind: Texture
    key: hero
    source: hero.png
    metadata: { srgb: true, role: primary }
scenes:
  - kind: Scene2D
    scene:
      kind: DisplayObject
      name: root
      children:
        - kind: Sprite
          name: hero-node
          texture: hero
        - kind: DisplayObject
          name: overlay
    custom:
      enabled: true
nullable: null
)YAML");

  const auto result =
      scene_document::parse_scene_document_yaml_subset(source);
  const auto parsed = success(result);
  check(parsed != nullptr, "representative Flight document YAML parses");
  if (parsed == nullptr) return;

  const Mapping* document = as_mapping(parsed->value);
  check(document != nullptr, "document root is a mapping");
  if (document == nullptr) return;
  check(keys_equal(*document,
                   {"flight", "defaultScene", "resources", "scenes",
                    "nullable"}),
        "document fields retain authored traversal order");

  const auto version = document->get(flight::String("flight"));
  const auto default_scene = document->get(flight::String("defaultScene"));
  check(version.has_value() && as_number(version.value()) != nullptr &&
            *as_number(version.value()) == 1,
        "schema version retains its exact numeric value");
  check(default_scene.has_value() &&
            as_number(default_scene.value()) != nullptr &&
            *as_number(default_scene.value()) == 0,
        "default scene retains its exact index");
  check(document->has(flight::String("nullable")) &&
            document->get(flight::String("nullable")).has_value() &&
            !document->get(flight::String("nullable")).value().has_value() &&
            !document->get(flight::String("missing")).has_value(),
        "explicit null remains distinct from an unknown/absent field");

  const auto resources_value = document->get(flight::String("resources"));
  const Sequence* resources = resources_value.has_value()
                                  ? as_sequence(resources_value.value())
                                  : nullptr;
  check(resources != nullptr && resources->size() == 1,
        "resource declarations retain their exact count");
  if (resources != nullptr && resources->size() == 1) {
    const Mapping* resource = as_mapping((*resources)[0]);
    check(resource != nullptr &&
              keys_equal(*resource,
                         {"kind", "key", "source", "metadata"}),
          "resource fields and metadata retain authored order");
    if (resource != nullptr) {
      const auto key = resource->get(flight::String("key"));
      const auto asset_source = resource->get(flight::String("source"));
      check(key.has_value() && as_string(key.value()) != nullptr &&
                *as_string(key.value()) == flight::String("hero") &&
                asset_source.has_value() &&
                as_string(asset_source.value()) != nullptr &&
                *as_string(asset_source.value()) == flight::String("hero.png"),
            "resource identity and asset source remain exact");
      const auto metadata_value = resource->get(flight::String("metadata"));
      const Mapping* metadata = metadata_value.has_value()
                                    ? as_mapping(metadata_value.value())
                                    : nullptr;
      check(metadata != nullptr &&
                keys_equal(*metadata, {"srgb", "role"}),
            "asset metadata remains a nested mapping");
      if (metadata != nullptr) {
        const auto srgb = metadata->get(flight::String("srgb"));
        const auto role = metadata->get(flight::String("role"));
        check(srgb.has_value() && as_boolean(srgb.value()) != nullptr &&
                  *as_boolean(srgb.value()) && role.has_value() &&
                  as_string(role.value()) != nullptr &&
                  *as_string(role.value()) == flight::String("primary"),
              "asset metadata preserves boolean and string scalars");
      }
    }
  }

  const auto scenes_value = document->get(flight::String("scenes"));
  const Sequence* scenes =
      scenes_value.has_value() ? as_sequence(scenes_value.value()) : nullptr;
  check(scenes != nullptr && scenes->size() == 1,
        "scene sequence retains its exact count");
  if (scenes == nullptr || scenes->size() != 1) return;
  const Mapping* scene = as_mapping((*scenes)[0]);
  const auto root_value =
      scene == nullptr ? std::optional<Value>{}
                       : scene->get(flight::String("scene"));
  const Mapping* root =
      root_value.has_value() ? as_mapping(root_value.value()) : nullptr;
  check(root != nullptr &&
            keys_equal(*root, {"kind", "name", "children"}),
        "root node preserves its field order and hierarchy");
  if (root != nullptr) {
    const auto children_value = root->get(flight::String("children"));
    const Sequence* children = children_value.has_value()
                                   ? as_sequence(children_value.value())
                                   : nullptr;
    check(children != nullptr && children->size() == 2,
          "both child nodes remain in the authored hierarchy");
    if (children != nullptr && children->size() == 2) {
      const Mapping* first = as_mapping((*children)[0]);
      const Mapping* second = as_mapping((*children)[1]);
      const auto first_name = first == nullptr
                                  ? std::optional<Value>{}
                                  : first->get(flight::String("name"));
      const auto texture_reference =
          first == nullptr ? std::optional<Value>{}
                           : first->get(flight::String("texture"));
      const auto second_name = second == nullptr
                                   ? std::optional<Value>{}
                                   : second->get(flight::String("name"));
      check(first_name.has_value() && as_string(first_name.value()) != nullptr &&
                *as_string(first_name.value()) == flight::String("hero-node") &&
                second_name.has_value() &&
                as_string(second_name.value()) != nullptr &&
                *as_string(second_name.value()) == flight::String("overlay"),
            "child node ordering is deterministic and exact");
      check(texture_reference.has_value() &&
                as_string(texture_reference.value()) != nullptr &&
                *as_string(texture_reference.value()) == flight::String("hero"),
            "node resource references retain the authored resource key");
    }
  }
  const auto custom_value =
      scene == nullptr ? std::optional<Value>{}
                       : scene->get(flight::String("custom"));
  check(custom_value.has_value() && as_mapping(custom_value.value()) != nullptr,
        "unknown extension metadata is preserved instead of discarded");
}

void identity_copy_edit_remove_and_failure_atomicity() {
  const flight::String source(
      "root:\n  kind: DisplayObject\n  children:\n    - kind: Sprite\n"
      "      name: first\n    - kind: Sprite\n      name: second\n");
  const auto parsed = success(
      scene_document::parse_scene_document_yaml_subset(source));
  check(parsed != nullptr, "identity fixture parses");
  if (parsed == nullptr) return;

  const Mapping* document = as_mapping(parsed->value);
  check(document != nullptr, "identity fixture owns a root mapping");
  if (document == nullptr) return;
  const Mapping alias = *document;
  Mapping clone(document->enumerable_entries());
  check(alias.identity() == document->identity() &&
            clone.identity() != document->identity(),
        "ordinary copies preserve mapping identity while clone creates new storage");

  const auto root_a = document->get(flight::String("root"));
  const auto root_b = alias.get(flight::String("root"));
  const Mapping* root_mapping_a =
      root_a.has_value() ? as_mapping(root_a.value()) : nullptr;
  const Mapping* root_mapping_b =
      root_b.has_value() ? as_mapping(root_b.value()) : nullptr;
  check(root_mapping_a != nullptr && root_mapping_b != nullptr &&
            root_mapping_a->identity() == root_mapping_b->identity(),
        "repeated references retain the exact parsed node owner");
  const auto children_a_value =
      root_mapping_a == nullptr
          ? std::optional<Value>{}
          : root_mapping_a->get(flight::String("children"));
  const auto children_b_value =
      root_mapping_b == nullptr
          ? std::optional<Value>{}
          : root_mapping_b->get(flight::String("children"));
  const Sequence* children_a = children_a_value.has_value()
                                   ? as_sequence(children_a_value.value())
                                   : nullptr;
  const Sequence* children_b = children_b_value.has_value()
                                   ? as_sequence(children_b_value.value())
                                   : nullptr;
  if (children_a != nullptr && children_b != nullptr) {
    const Sequence children_clone = children_a->clone();
    check(children_a->identity() == children_b->identity() &&
              children_clone.identity() != children_a->identity(),
          "array copies retain identity while an explicit clone detaches storage");
  } else {
    check(false, "identity fixture retains its nested children array");
  }

  clone.set(flight::String("edited"), Value{std::in_place,
                                             std::in_place_type<bool>, true});
  check(clone.has(flight::String("edited")) &&
            !document->has(flight::String("edited")),
        "editing a cloned mapping does not mutate the parsed document");
  check(clone.erase(flight::String("root")) &&
            !clone.has(flight::String("root")) &&
            document->has(flight::String("root")),
        "removal from a clone leaves the original hierarchy intact");

  const void* original_identity = document->identity();
  const std::size_t original_size = document->size();
  const auto invalid = expect_refusal("value: &root true",
                                      "flight-document.unsupported.anchor");
  check(invalid != nullptr && document->identity() == original_identity &&
            document->size() == original_size &&
            document->has(flight::String("root")),
        "a failed parse is atomic with respect to an existing parsed tree");
}

void scalars_diagnostics_malformed_inputs_and_limits() {
  const auto scalar_result = success(
      scene_document::parse_scene_document_yaml_subset(flight::String(
          "nullValue: null\ntrueValue: true\nfalseValue: false\n"
          "integer: -12\ndecimal: 1.25\nexponent: -1.25e2\n"
          "color: 0x3366ccff\nlegacyNo: no\nleadingZero: 012\n"
          "quoted: \"line\\nvalue\\u0021\"\n"
          "singleQuoted: 'Flight''s subset'\n")));
  check(scalar_result != nullptr, "explicit scalar fixture parses");
  if (scalar_result != nullptr) {
    const Mapping* values = as_mapping(scalar_result->value);
    const auto exponent = values == nullptr
                              ? std::optional<Value>{}
                              : values->get(flight::String("exponent"));
    const auto legacy = values == nullptr
                            ? std::optional<Value>{}
                            : values->get(flight::String("legacyNo"));
    const auto leading = values == nullptr
                             ? std::optional<Value>{}
                             : values->get(flight::String("leadingZero"));
    const auto quoted = values == nullptr
                            ? std::optional<Value>{}
                            : values->get(flight::String("quoted"));
    check(exponent.has_value() && as_number(exponent.value()) != nullptr &&
              *as_number(exponent.value()) == -125 && legacy.has_value() &&
              as_string(legacy.value()) != nullptr &&
              *as_string(legacy.value()) == flight::String("no") &&
              leading.has_value() && as_string(leading.value()) != nullptr &&
              *as_string(leading.value()) == flight::String("012") &&
              quoted.has_value() && as_string(quoted.value()) != nullptr &&
              *as_string(quoted.value()) == flight::String("line\nvalue!"),
          "scalar rules retain numbers, YAML-1.2 strings, and decoded escapes exactly");
  }

  const auto anchor = expect_refusal("value: &root true",
                                     "flight-document.unsupported.anchor");
  check(anchor != nullptr && anchor->offset == 7 && anchor->line == 1 &&
            anchor->column == 8 && !anchor->limit.has_value() &&
            !anchor->actual.has_value(),
        "anchor refusal reports its exact source position without limits");
  const auto duplicate = expect_refusal(
      "value: 1\nvalue: 2", "flight-document.syntax.duplicate-key");
  check(duplicate != nullptr && duplicate->line == 2 &&
            duplicate->column == 1,
        "duplicate-key refusal points at the second authored key");
  expect_refusal("items: [one, two]",
                 "flight-document.unsupported.flow-sequence");
  expect_refusal("value: |\n  text",
                 "flight-document.unsupported.block-scalar");
  expect_refusal("value: 1e999",
                 "flight-document.scalar.number-out-of-range");
  expect_refusal("value: \"bad\\q\"",
                 "flight-document.syntax.invalid-escape");
  expect_refusal("value: { nested: true",
                 "flight-document.syntax.unterminated-flow-mapping");
  expect_refusal("---\nvalue: true",
                 "flight-document.unsupported.document-separator");
  expect_refusal("value:\ttrue", "flight-document.syntax.tab-character");

  std::string nested;
  for (int index = 0; index < 65; ++index) nested += "{ value: ";
  nested += "null";
  for (int index = 0; index < 65; ++index) nested += " }";
  const auto depth = expect_refusal(
      nested, "flight-document.limit.nesting-depth");
  check(depth != nullptr && depth->limit == std::optional<double>(64) &&
            depth->actual == std::optional<double>(65),
        "nesting refusal reports exact limit and attempted depth");

  const std::string scalar_at_limit(65'536, 'x');
  check(success(scene_document::parse_scene_document_yaml_subset(
                    flight::String(scalar_at_limit))) != nullptr,
        "a scalar at the published code-unit limit is accepted");
  const auto scalar_over = expect_refusal(
      scalar_at_limit + "x", "flight-document.limit.scalar-code-units");
  check(scalar_over != nullptr &&
            scalar_over->limit == std::optional<double>(65'536) &&
            scalar_over->actual == std::optional<double>(65'537),
        "the first scalar beyond the limit is refused before publication");
}

}  // namespace

int main() {
  schema_assets_metadata_hierarchy_and_order();
  identity_copy_edit_remove_and_failure_atomicity();
  scalars_diagnostics_malformed_inputs_and_limits();

  if (failures == 0) {
    std::cout << "generated scene-document tests passed\n";
    return 0;
  }
  std::cerr << failures << " generated scene-document test(s) failed\n";
  return 1;
}
