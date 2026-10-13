#include <flight/binpack/contract.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

// Native port of packages/binpack/src/{packRectangles,explainUnpackedRectangles}.test.ts
// at a627849. Besides the upstream invariant cases, the total-order fixture pins the exact
// placement order and coordinates: counts and non-overlap alone would not catch a changed sort.

namespace {

namespace binpack = flight::binpack;
using flight::types::BinPackOptions;
using flight::types::PackableRectangle;
using flight::types::PackedRectangle;
using flight::types::PackResult;
using flight::types::RectangleId;

using Rectangles = flight::Array<flight::Ref<PackableRectangle>>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return flight::structural_ref_cast<View>(value);
}

flight::Ref<PackableRectangle> rectangle(RectangleId id, double width, double height) {
  return flight::make_ref<PackableRectangle>(
      PackableRectangle{.id = std::move(id), .width = width, .height = height});
}

flight::Ref<PackableRectangle> rectangle(const char* id, double width, double height) {
  return rectangle(RectangleId{flight::String(id)}, width, height);
}

flight::Ref<PackableRectangle> rectangle(double id, double width, double height) {
  return rectangle(RectangleId{id}, width, height);
}

flight::Ref<PackResult> pack(Rectangles rectangles) {
  return binpack::pack_rectangles(std::move(rectangles));
}

flight::Ref<PackResult> pack(Rectangles rectangles, const flight::Ref<BinPackOptions>& options) {
  return binpack::pack_rectangles(std::move(rectangles), readonly(options));
}

flight::Array<flight::Ref<flight::types::UnpackedRectangleExplanation>> explain(
    Rectangles rectangles, const flight::Ref<BinPackOptions>& options) {
  return binpack::explain_unpacked_rectangles(std::move(rectangles), readonly(options));
}

std::string id_text(const RectangleId& id) {
  if (std::holds_alternative<double>(id)) {
    return flight::String::from_number(std::get<double>(id)).to_utf8();
  }
  return std::get<flight::String>(id).to_utf8();
}

bool id_is(const RectangleId& id, const char* expected) {
  return std::holds_alternative<flight::String>(id) &&
         std::get<flight::String>(id) == flight::String(expected);
}

bool id_is(const RectangleId& id, double expected) {
  return std::holds_alternative<double>(id) && std::get<double>(id) == expected;
}

flight::Ref<PackedRectangle> placement(const flight::Ref<PackResult>& result, const char* id) {
  for (const auto& candidate : result->placements) {
    if (id_is(candidate->id, id)) return candidate;
  }
  return {};
}

bool overlaps(const flight::Ref<PackedRectangle>& a, const flight::Ref<PackedRectangle>& b) {
  return !(a->x + a->width <= b->x || b->x + b->width <= a->x ||
           a->y + a->height <= b->y || b->y + b->height <= a->y);
}

bool same_result(const flight::Ref<PackResult>& left, const flight::Ref<PackResult>& right) {
  if (left->width != right->width || left->height != right->height ||
      left->placements.size() != right->placements.size() ||
      left->unpacked.size() != right->unpacked.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left->placements.size(); ++index) {
    const auto& a = left->placements[index];
    const auto& b = right->placements[index];
    if (a->id != b->id || a->x != b->x || a->y != b->y ||
        a->width != b->width || a->height != b->height || a->rotated != b->rotated) {
      return false;
    }
  }
  for (std::size_t index = 0; index < left->unpacked.size(); ++index) {
    if (left->unpacked[index] != right->unpacked[index]) return false;
  }
  return true;
}

bool is_power_of_two(double value) {
  if (value <= 0.0 || std::trunc(value) != value) return false;
  const auto integer = static_cast<std::uint64_t>(value);
  return (integer & (integer - 1)) == 0;
}

void check_non_overlapping(const flight::Ref<PackResult>& result, double padding,
                           const std::string& label) {
  for (std::size_t i = 0; i < result->placements.size(); ++i) {
    for (std::size_t j = i + 1; j < result->placements.size(); ++j) {
      const auto& a = result->placements[i];
      const auto& b = result->placements[j];
      const bool apart = a->x + a->width + padding <= b->x ||
                         b->x + b->width + padding <= a->x ||
                         a->y + a->height + padding <= b->y ||
                         b->y + b->height + padding <= a->y;
      check(apart, label + ": placement " + id_text(a->id) +
                       " overlaps placement " + id_text(b->id));
    }
  }
}

void default_extent_occupancy_and_empty_input() {
  auto over = pack(Rectangles{rectangle("over", binpack::bin_pack_default_max_extent + 1.0, 4.0)});
  auto at = pack(Rectangles{rectangle("at", binpack::bin_pack_default_max_extent, 4.0)});
  check(over->placements.empty() && over->unpacked.size() == 1 && id_is(over->unpacked[0], "over"),
        "the default maximum rejects a rectangle one unit beyond it");
  check(at->unpacked.empty() && at->placements.size() == 1,
        "the default maximum admits a rectangle exactly at it");

  auto exact = pack(Rectangles{rectangle("a", 16.0, 16.0)});
  auto rounded_options = flight::make_ref<BinPackOptions>();
  rounded_options->power_of_two = true;
  auto rounded = pack(Rectangles{rectangle("a", 24.0, 24.0)}, rounded_options);
  check(binpack::get_pack_result_occupancy(readonly(exact)) == 1.0,
        "occupancy is one for an exactly covered reported extent");
  check(rounded->width == 32.0 && rounded->height == 32.0 &&
            std::abs(binpack::get_pack_result_occupancy(readonly(rounded)) - 0.5625) < 1e-12,
        "occupancy exposes power-of-two rounding waste");

  auto empty = pack(Rectangles{});
  check(empty->placements.empty() && empty->unpacked.empty() &&
            empty->width == 0.0 && empty->height == 0.0 &&
            binpack::get_pack_result_occupancy(readonly(empty)) == 0.0,
        "empty input has a zero extent and zero rather than NaN occupancy");
}

void exact_placement_order_padding_and_bounds() {
  auto options = flight::make_ref<BinPackOptions>();
  options->padding = 2.0;
  options->border = 4.0;
  auto result = pack(Rectangles{rectangle("a", 10.0, 10.0), rectangle("b", 10.0, 10.0)}, options);
  auto a = placement(result, "a");
  auto b = placement(result, "b");
  check(result->unpacked.empty() && result->placements.size() == 2 && a && b,
        "the padding fixture places both rectangles");
  check(a && a->x == 4.0 && a->y == 4.0 && a->width == 10.0 && a->height == 10.0 && !a->rotated,
        "the first padding fixture placement is exactly (4,4,10,10,false)");
  check(b && b->x == 16.0 && b->y == 4.0 && b->width == 10.0 && b->height == 10.0 && !b->rotated,
        "the second padding fixture placement is exactly (16,4,10,10,false)");
  check(result->width == 30.0 && result->height == 18.0 &&
            b && b->x - (a->x + a->width) == 2.0 &&
            b->x + b->width == result->width - 4.0 &&
            b->y + b->height == result->height - 4.0,
        "reported bounds contain the exact padding gap and four-unit border on every edge");

  auto order_options = flight::make_ref<BinPackOptions>();
  order_options->growable = false;
  order_options->max_width = 40.0;
  order_options->max_height = 10.0;
  auto ordered = pack(Rectangles{rectangle("z", 10.0, 10.0), rectangle(3.0, 10.0, 10.0),
                                 rectangle("a", 10.0, 10.0), rectangle(1.0, 10.0, 10.0)},
                      order_options);
  check(ordered->unpacked.empty() && ordered->placements.size() == 4,
        "the total-order fixture fills one fixed row");
  const bool exact_order = ordered->placements.size() == 4 &&
                           id_is(ordered->placements[0]->id, 1.0) && ordered->placements[0]->x == 0.0 &&
                           id_is(ordered->placements[1]->id, 3.0) && ordered->placements[1]->x == 10.0 &&
                           id_is(ordered->placements[2]->id, "a") && ordered->placements[2]->x == 20.0 &&
                           id_is(ordered->placements[3]->id, "z") && ordered->placements[3]->x == 30.0;
  check(exact_order,
        "equal rectangles use numeric-before-string ascending id order at exact x coordinates 0,10,20,30");
}

void growth_rotation_rounding_and_edge_cases() {
  Rectangles varied;
  for (double i = 0.0; i < 20.0; i += 1.0) {
    varied.push(rectangle(i, 8.0 + std::fmod(i * 7.0, 40.0),
                          6.0 + std::fmod(i * 13.0, 34.0)));
  }
  auto varied_options = flight::make_ref<BinPackOptions>();
  varied_options->padding = 1.0;
  varied_options->border = 2.0;
  auto varied_result = pack(varied, varied_options);
  check(varied_result->unpacked.empty() && varied_result->placements.size() == 20,
        "all twenty varied upstream rectangles fit in a growable bin");
  for (const auto& placed : varied_result->placements) {
    check(placed->x >= 2.0 && placed->y >= 2.0 &&
              placed->x + placed->width <= varied_result->width - 2.0 &&
              placed->y + placed->height <= varied_result->height - 2.0,
          "the varied fixture keeps placement " + id_text(placed->id) + " inside its exact border bounds");
  }
  check_non_overlapping(varied_result, 1.0, "twenty varied rectangles");

  auto fixed = flight::make_ref<BinPackOptions>();
  fixed->growable = false;
  fixed->max_width = 44.0;
  fixed->max_height = 44.0;
  Rectangles squares;
  for (double i = 0.0; i < 12.0; i += 1.0) squares.push(rectangle(i, 20.0, 20.0));
  auto overflow = pack(squares, fixed);
  check(overflow->placements.size() == 4 && overflow->unpacked.size() == 8,
        "a fixed 44x44 bin places exactly four of twelve 20x20 rectangles");
  check_non_overlapping(overflow, 0.0, "fixed-bin overflow");

  auto rotation = flight::make_ref<BinPackOptions>();
  rotation->growable = false;
  rotation->max_width = 20.0;
  rotation->max_height = 8.0;
  rotation->allow_rotation = true;
  auto turned = pack(Rectangles{rectangle("tall", 8.0, 20.0)}, rotation);
  check(turned->unpacked.empty() && turned->placements.size() == 1 &&
            turned->placements[0]->rotated && turned->placements[0]->width == 20.0 &&
            turned->placements[0]->height == 8.0,
        "rotation is used when it is the only orientation that fits");
  rotation->allow_rotation = false;
  auto not_turned = pack(Rectangles{rectangle("tall", 8.0, 20.0)}, rotation);
  check(not_turned->placements.empty() && not_turned->unpacked.size() == 1,
        "the same fixed bin rejects the tall rectangle without rotation");

  auto grow = pack(Rectangles{rectangle("tall", 8.0, 20.0)});
  check(grow->unpacked.empty() && !grow->placements[0]->rotated &&
            grow->placements[0]->width == 8.0 && grow->placements[0]->height == 20.0,
        "a growable bin expands rather than inventing rotation");

  auto rounded_options = flight::make_ref<BinPackOptions>();
  rounded_options->power_of_two = true;
  rounded_options->square = true;
  auto rounded = pack(Rectangles{rectangle("a", 30.0, 20.0), rectangle("b", 17.0, 41.0),
                                 rectangle("c", 25.0, 9.0)},
                      rounded_options);
  check(is_power_of_two(rounded->width) && rounded->width == rounded->height,
        "powerOfTwo plus square reports one square power-of-two extent");
  for (const auto& placed : rounded->placements) {
    check(placed->x + placed->width <= rounded->width &&
              placed->y + placed->height <= rounded->height,
          "rounded bounds still contain placement " + id_text(placed->id));
  }

  auto single_options = flight::make_ref<BinPackOptions>();
  single_options->border = 3.0;
  auto single = pack(Rectangles{rectangle("only", 12.0, 7.0)}, single_options);
  check(single->placements.size() == 1 && single->placements[0]->x == 3.0 &&
            single->placements[0]->y == 3.0 && single->width == 18.0 && single->height == 13.0,
        "a single rectangle has the exact upstream border-corner placement and extent");

  auto huge_options = flight::make_ref<BinPackOptions>();
  huge_options->growable = false;
  huge_options->max_width = 64.0;
  huge_options->max_height = 64.0;
  auto huge = pack(Rectangles{rectangle("big", 500.0, 500.0)}, huge_options);
  check(huge->placements.empty() && huge->unpacked.size() == 1,
        "a rectangle larger than a fixed bin is unpacked");
  huge_options->growable = true;
  auto capped = pack(Rectangles{rectangle("huge", 200.0, 10.0)}, huge_options);
  check(capped->placements.empty() && capped->unpacked.size() == 1,
        "a growable bin still honors its growth cap");

  auto degenerate = pack(Rectangles{rectangle("zeroW", 0.0, 10.0), rectangle("zeroH", 10.0, 0.0),
                                    rectangle("negative", -8.0, 10.0), rectangle("ok", 10.0, 10.0)});
  check(degenerate->placements.size() == 1 && id_is(degenerate->placements[0]->id, "ok") &&
            degenerate->unpacked.size() == 3,
        "zero and negative dimensions are unpacked while the valid rectangle is placed");

  auto duplicates = pack(Rectangles{rectangle("same", 10.0, 10.0), rectangle("same", 10.0, 10.0)});
  check(duplicates->placements.size() == 2 &&
            (duplicates->placements[0]->x != duplicates->placements[1]->x ||
             duplicates->placements[0]->y != duplicates->placements[1]->y),
        "duplicate ids remain two distinct non-overlapping pieces");

  auto fractional = pack(Rectangles{rectangle("frac", 10.5, 4.25)});
  check(fractional->placements[0]->width == 10.5 && fractional->placements[0]->height == 4.25,
        "fractional dimensions are not rounded");

  auto collapsed_options = flight::make_ref<BinPackOptions>();
  collapsed_options->growable = false;
  collapsed_options->max_width = 10.0;
  collapsed_options->max_height = 10.0;
  collapsed_options->border = 6.0;
  auto collapsed = pack(Rectangles{rectangle("a", 4.0, 4.0)}, collapsed_options);
  check(collapsed->placements.empty() && collapsed->unpacked.size() == 1,
        "a border larger than the fixed extent collapses the usable region");

  auto padded_options = flight::make_ref<BinPackOptions>();
  padded_options->padding = 20.0;
  auto padded = pack(Rectangles{rectangle("a", 2.0, 2.0), rectangle("b", 2.0, 2.0)}, padded_options);
  check(padded->placements.size() == 2, "pieces smaller than their padding both remain placeable");
  check_non_overlapping(padded, 20.0, "padding larger than the pieces");
}

struct Random32 {
  std::uint32_t state;
  double next() {
    state = state * 1664525U + 1013904223U;
    return static_cast<double>(state) / 4294967296.0;
  }
};

void seeded_packing_invariants_and_determinism() {
  for (std::uint32_t seed = 1; seed <= 40; ++seed) {
    Random32 random{seed};
    const int count = 1 + static_cast<int>(std::floor(random.next() * 25.0));
    Rectangles rectangles;
    std::map<int, std::pair<double, double>> sizes;
    std::map<int, int> input_counts;
    for (int index = 0; index < count; ++index) {
      const int id = seed % 5 == 0 ? index % 3 : index;
      Random32 size{seed * 1000U + static_cast<std::uint32_t>(id)};
      const double width = 1.0 + std::floor(size.next() * 60.0);
      const double height = 1.0 + std::floor(size.next() * 60.0);
      rectangles.push(rectangle(static_cast<double>(id), width, height));
      sizes[id] = {width, height};
      ++input_counts[id];
    }
    const double padding = std::floor(random.next() * 4.0);
    const double border = std::floor(random.next() * 4.0);
    auto options = flight::make_ref<BinPackOptions>();
    options->padding = padding;
    options->border = border;
    options->heuristic = flight::String(seed % 2 == 0 ? "bestAreaFit" : "bestShortSideFit");
    options->allow_rotation = seed % 3 == 0;

    auto result = pack(rectangles, options);
    const std::string label = "seed " + std::to_string(seed);
    check(result->placements.size() + result->unpacked.size() == rectangles.size(),
          label + " accounts for every input exactly once by count");

    std::map<int, int> output_counts;
    for (const auto& placed : result->placements) {
      const int id = static_cast<int>(std::get<double>(placed->id));
      ++output_counts[id];
      const auto [source_width, source_height] = sizes[id];
      const double expected_width = placed->rotated ? source_height : source_width;
      const double expected_height = placed->rotated ? source_width : source_height;
      check(placed->width == expected_width && placed->height == expected_height,
            label + " preserves the exact size/orientation for id " + std::to_string(id));
      check(placed->x >= border && placed->y >= border &&
                placed->x + placed->width <= result->width - border &&
                placed->y + placed->height <= result->height - border,
            label + " keeps id " + std::to_string(id) + " inside all four border bounds");
    }
    for (const auto& id : result->unpacked) ++output_counts[static_cast<int>(std::get<double>(id))];
    check(output_counts == input_counts, label + " preserves the complete id multiset");
    check_non_overlapping(result, padding, label);

    double placed_area = 0.0;
    for (const auto& placed : result->placements) placed_area += placed->width * placed->height;
    const double reported_area = result->width * result->height;
    const double expected_occupancy = reported_area > 0.0 ? placed_area / reported_area : 0.0;
    check(std::abs(binpack::get_pack_result_occupancy(readonly(result)) - expected_occupancy) < 1e-12,
          label + " occupancy equals exact placed area divided by reported area");
    check(same_result(result, pack(rectangles, options)),
          label + " is byte-for-value deterministic on a second pack");
  }
}

void unpacked_explanations() {
  auto none = binpack::explain_unpacked_rectangles(Rectangles{rectangle("a", 8.0, 8.0)});
  check(none.empty(), "explanations are empty when every rectangle is placed");

  auto fixed = flight::make_ref<BinPackOptions>();
  fixed->growable = false;
  fixed->max_width = 100.0;
  fixed->max_height = 100.0;
  auto reasons = explain(Rectangles{rectangle("huge", 500.0, 10.0), rectangle("filler", 90.0, 90.0),
                                    rectangle("filler2", 90.0, 90.0)},
                         fixed);
  std::map<std::string, std::string> by_id;
  for (const auto& entry : reasons) by_id[id_text(entry->id)] = entry->reason.to_utf8();
  check(by_id["huge"] == "oversized" && by_id["filler2"] == "binExhausted",
        "explanations distinguish impossible size from an exhausted fixed bin");

  fixed->max_width = 10.0;
  fixed->max_height = 10.0;
  fixed->border = 6.0;
  auto collapsed = explain(Rectangles{rectangle("a", 2.0, 2.0), rectangle("b", 2.0, 2.0)}, fixed);
  check(collapsed.size() == 2 && collapsed[0]->reason == flight::String("regionCollapsed") &&
            collapsed[1]->reason == flight::String("regionCollapsed") &&
            collapsed[0]->usable_width == -2.0 && collapsed[0]->usable_height == -2.0,
        "collapsed-region explanations take precedence and report exact net extents");

  auto rotate = flight::make_ref<BinPackOptions>();
  rotate->growable = false;
  rotate->max_width = 300.0;
  rotate->max_height = 100.0;
  auto without = explain(Rectangles{rectangle("tall", 10.0, 200.0)}, rotate);
  rotate->allow_rotation = true;
  auto with = explain(Rectangles{rectangle("tall", 10.0, 200.0)}, rotate);
  check(without.size() == 1 && without[0]->reason == flight::String("oversized") && with.empty(),
        "rotation is a real second chance before oversized is reported");

  auto net = flight::make_ref<BinPackOptions>();
  net->growable = false;
  net->max_width = 100.0;
  net->max_height = 100.0;
  net->border = 5.0;
  auto measured = explain(Rectangles{rectangle("a", 999.0, 4.0)}, net);
  check(measured.size() == 1 && measured[0]->usable_width == 90.0 && measured[0]->usable_height == 90.0,
        "explanations report caps net of both borders");

  auto duplicates = flight::make_ref<BinPackOptions>();
  duplicates->growable = false;
  duplicates->max_width = 64.0;
  duplicates->max_height = 64.0;
  Rectangles one_fails{rectangle("a", 60.0, 60.0), rectangle("a", 60.0, 60.0)};
  auto packed_one = pack(one_fails, duplicates);
  auto explained_one = explain(one_fails, duplicates);
  check(packed_one->placements.size() == 1 && packed_one->unpacked.size() == 1 && explained_one.size() == 1,
        "duplicate-id explanations consume a multiset count when only one piece fails");
  Rectangles both_fail{rectangle("a", 500.0, 500.0), rectangle("a", 500.0, 500.0)};
  check(pack(both_fail, duplicates)->unpacked.size() == 2 && explain(both_fail, duplicates).size() == 2,
        "duplicate-id explanations retain one entry for each failed piece");

  auto seeded_options = flight::make_ref<BinPackOptions>();
  seeded_options->growable = false;
  seeded_options->max_width = 96.0;
  seeded_options->max_height = 96.0;
  for (int seed = 1; seed <= 30; ++seed) {
    double state = static_cast<double>(seed) * 1103515245.0 + 12345.0;
    auto random = [&]() {
      state = std::fmod(state * 1103515245.0 + 12345.0, 2147483648.0);
      return state / 2147483648.0;
    };
    const int count = 1 + static_cast<int>(std::floor(random() * 12.0));
    Rectangles rectangles;
    for (int index = 0; index < count; ++index) {
      const std::string id = "id-" + std::to_string(seed % 2 == 0 ? index % 3 : index);
      rectangles.push(rectangle(id.c_str(), 1.0 + std::floor(random() * 80.0),
                                1.0 + std::floor(random() * 80.0)));
    }
    const auto packed = pack(rectangles, seeded_options);
    const auto explained = explain(rectangles, seeded_options);
    std::map<std::string, int> unpacked_counts;
    std::map<std::string, int> explained_counts;
    for (const auto& id : packed->unpacked) ++unpacked_counts[id_text(id)];
    for (const auto& entry : explained) ++explained_counts[id_text(entry->id)];
    const std::string label = "explanation seed " + std::to_string(seed);
    check(explained.size() == packed->unpacked.size(), label + " has one explanation per failure");
    check(explained_counts == unpacked_counts, label + " preserves the complete failed-id multiset");
  }
}

}  // namespace

int main() {
  default_extent_occupancy_and_empty_input();
  exact_placement_order_padding_and_bounds();
  growth_rotation_rounding_and_edge_cases();
  seeded_packing_invariants_and_determinism();
  unpacked_explanations();
  return failures == 0 ? 0 : 1;
}
