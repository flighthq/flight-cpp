// Focused native behavior for @flighthq/scene2d-formats lottieCounts at Flight a627849, compiled through the
// scene2d-formats-lottie-frame-rate-narrowed-local source patch, which reads `record.fr` into a typeof-narrowed
// local after the layers guard so the whole module header compiles.
// These fixtures pin the source contract of the public catalog: getLottieFeatureNames is DERIVED from the census
// tables (every table value once, in sorted order, 6 layer + 8 mask + 9 shape names), each call returns a fresh
// array, and the tables it reads are not disturbed. They also pin the patched private document guard: layers must
// be an array, fr a positive number, and ip/op numbers.
#include <flight/scene2d_formats/lottie_counts.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

namespace s2f = flight::scene2d_formats;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

std::vector<std::string> strings_of(const flight::Array<flight::String>& array) {
  std::vector<std::string> out;
  for (const auto& value : array) out.push_back(value.to_utf8());
  return out;
}

template <typename Map>
void append_values(std::vector<std::string>& out, const Map& map) {
  for (const auto& value : map.values()) out.push_back(value.to_utf8());
}

std::size_t count_prefix(const std::vector<std::string>& names, const std::string& prefix) {
  return static_cast<std::size_t>(
      std::count_if(names.begin(), names.end(), [&](const std::string& name) { return name.rfind(prefix, 0) == 0; }));
}

flight::Record<flight::String, flight::Any> document(flight::Any layers, flight::Any fr, flight::Any ip, flight::Any op) {
  flight::Record<flight::String, flight::Any> record;
  record.set(flight::String("layers"), layers);
  record.set(flight::String("fr"), fr);
  record.set(flight::String("ip"), ip);
  record.set(flight::String("op"), op);
  return record;
}

}  // namespace

int main() {
  // The catalog is the four census tables' values, concatenated and sorted -- derived, not written out.
  std::vector<std::string> expected;
  append_values(expected, s2f::lottie_layer_features);
  append_values(expected, s2f::lottie_mask_features);
  for (const auto& value : s2f::lottie_mask_condition_features) expected.push_back(value.to_utf8());
  append_values(expected, s2f::lottie_shape_item_features);
  std::sort(expected.begin(), expected.end());

  const auto first = s2f::get_lottie_feature_names();
  const auto names = strings_of(first);
  check(names == expected, "the catalog must be every census table value, sorted");
  check(names.size() == 23, "the catalog must name 23 features");
  check(std::is_sorted(names.begin(), names.end()), "the catalog must be sorted");
  check(count_prefix(names, "layer.") == 6, "six layer features");
  check(count_prefix(names, "mask.") == 8, "six mask modes plus the two conditions");
  check(count_prefix(names, "shape.") == 9, "nine shape-item features");
  check(std::find(names.begin(), names.end(), "mask.multiple") != names.end() &&
            std::find(names.begin(), names.end(), "shape.trimPath") != names.end(),
        "condition and table features must both be present");

  // Each call returns a fresh array; mutating one result disturbs neither the next result nor the tables.
  auto mutated = s2f::get_lottie_feature_names();
  check(mutated.identity() != first.identity(), "each call must return a fresh array");
  mutated.push(flight::String("probe"));
  check(strings_of(s2f::get_lottie_feature_names()) == expected, "a mutated result must not leak into later calls");
  check(s2f::lottie_layer_features.size() == 6 && s2f::lottie_mask_features.size() == 6 &&
            s2f::lottie_mask_condition_features.size() == 2 && s2f::lottie_shape_item_features.size() == 9,
        "the census tables must not be disturbed");

  // The patched document guard: same outcomes as the original conjunction.
  const flight::Any layers = flight::Array<flight::Any>{};
  check(s2f::is_lottie_document(document(layers, 30.0, 0.0, 60.0)), "a well-formed header must be a Lottie document");
  check(!s2f::is_lottie_document(document(layers, 0.0, 0.0, 60.0)), "fr zero is rejected");
  check(!s2f::is_lottie_document(document(layers, -1.0, 0.0, 60.0)), "negative fr is rejected");
  check(!s2f::is_lottie_document(document(layers, flight::String("30"), 0.0, 60.0)), "string fr is rejected");
  check(!s2f::is_lottie_document(document(layers, flight::undefined, 0.0, 60.0)), "missing fr is rejected");
  check(!s2f::is_lottie_document(document(flight::Any(1.0), 30.0, 0.0, 60.0)), "non-array layers are rejected");
  check(!s2f::is_lottie_document(document(layers, 30.0, flight::null, 60.0)), "non-number ip is rejected");
  check(!s2f::is_lottie_document(document(layers, 30.0, 0.0, flight::String("60"))), "non-number op is rejected");

  if (failures == 0) std::cout << "scene2d-formats getLottieFeatureNames: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
