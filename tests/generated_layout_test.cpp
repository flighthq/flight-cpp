// Native parity test for @flighthq/layout, ported from its own suites (19 cases across three modules).
//
// The package declares eight modules and resolves none of the three solvers: `anchorLayout`, `flexLayout` and
// `gridLayout` are refused at this pin (object-key iteration; two type assertions the emitter cannot lower), and
// although partial headers are written for them, five of the package's eight headers do not compile -- the three
// refused ones plus the two that include them. What compiles, and what this file covers, is the other three:
//
//   * layoutState -- the state record itself: its eight initialized fields, and the register/unregister pair where
//     passing no resolver ERASES the kind rather than storing an empty one.
//   * enableLayoutGuards -- the single seam that turns a silent failure sentinel into a structured warning.
//   * resolveLayoutTree -- the driver. It validates the two output buffers by length before it does anything, walks
//     the tree in index order, writes each ROOT as the available rectangle (with the available size sanitized: a
//     non-finite or non-positive width or height becomes 0), dispatches every other node through its PARENT's
//     registered resolver, and on any failure records the structured explanation on the state and returns false.
//     `explainLayoutResolution` is the reader for those records -- and it is deliberately two things at once, which
//     is why it gets its own group below: a retained-failure lookup that answers only for the node the failure names
//     (or for any node when asked with a negative index), and a static diagnosis for a tree that has no retained
//     failure at all.
//
// Coverage is of the emitted surface only. Nothing here asserts anchor, flex or grid behaviour, because those
// modules have no compilable surface at this pin; that boundary is recorded in docs/parity/plat.md rather than
// papered over, and no test invents an API for them.
#include <flight/layout/layout_state.hpp>
#include <flight/layout/enable_layout_guards.hpp>
#include <flight/layout/resolve_layout_tree.hpp>

#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace layout = flight::layout;
namespace types = flight::types;

using Tree = types::LayoutTree;
using TreeRow = flight::RowReadonly<flight::RowOf<flight::Ref<Tree>>>;
using Node = types::LayoutNode<>;
using Explanation = types::LayoutResolutionExplanation;
using ExplanationRow = flight::RowReadonly<flight::RowOf<flight::Ref<Explanation>>>;
using Outcome = std::optional<flight::String>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void check_close(double value, double expected, const std::string& message) {
  if (value == expected) return;
  std::cerr << "FAIL: " << message << " (got " << value << ", want " << expected << ")\n";
  ++failures;
}

std::string utf8(const flight::String& value) { return value.to_utf8(); }

flight::Ref<Node> node(const char* kind, double parent_index) {
  auto out = flight::make_ref<Node>();
  out->kind = flight::String(kind);
  out->parent_index = parent_index;
  return out;
}

// A tree the caller holds by reference, since the module reads it through a row over the same object.
flight::Ref<Tree> tree_of(std::vector<flight::Ref<Node>> nodes) {
  auto out = flight::make_ref<Tree>();
  for (auto& entry : nodes) out->nodes.push(entry);
  return out;
}

flight::StructuralRef<TreeRow> as_row(const flight::Ref<Tree>& tree) {
  return flight::StructuralRef<TreeRow>(tree);
}

flight::Array<double> sizes_of(std::initializer_list<double> values) {
  flight::Array<double> out;
  for (double value : values) out.push(value);
  return out;
}

// The upstream cases read the explanation as a plain record, so the checks below read the returned fields the
// same way rather than comparing an object identity.
std::string kind_of(const std::optional<flight::Ref<Explanation>>& explanation) {
  if (!explanation.has_value()) return "<null>";
  return utf8(explanation.value()->kind);
}

// --- layoutState ----------------------------------------------------------------------------------------------

void test_a_fresh_state_is_initialized_to_its_declared_defaults() {
  const auto state = layout::create_layout_state();
  check(!state->guard.has_value(), "no guard is installed");
  check_close(state->last_failure_actual_length, 0.0, "no retained actual length");
  check_close(state->last_failure_required_length, 0.0, "no retained required length");
  check_close(state->last_failure_node_index, -1.0, "and the not-a-node sentinel for the failure node");
  check_close(state->last_failure_parent_index, -1.0, "and for its parent");
  check(!state->last_failure_kind.has_value() && !state->last_failure_resolver_kind.has_value(),
        "with no failure kind and no failing resolver kind");
  check(state->resolvers.size() == 0.0, "and an empty resolver registry");
}

void test_registering_and_unregistering_a_resolver_kind() {
  const auto state = layout::create_layout_state();
  types::LayoutResolver resolver = [](flight::Float32Array, flight::StructuralRef<TreeRow>,
                                      flight::SequenceView<double>, double, double) -> Outcome { return std::nullopt; };

  layout::register_layout_resolver(state, flight::String("acme.Root"), resolver);
  check(state->resolvers.has(flight::String("acme.Root")), "a registered kind is in the registry");

  // Passing no resolver is the UNREGISTER: the kind is erased rather than stored as an empty entry, which is what
  // makes resolution report an unregistered kind again afterwards.
  layout::register_layout_resolver(state, flight::String("acme.Root"), std::nullopt);
  check(!state->resolvers.has(flight::String("acme.Root")), "and an absent resolver erases the kind");
}

void test_registering_the_same_kind_again_replaces_it() {
  const auto state = layout::create_layout_state();
  int first_calls = 0;
  int second_calls = 0;
  layout::register_layout_resolver(state, flight::String("acme.Root"),
                                   [&first_calls](flight::Float32Array, flight::StructuralRef<TreeRow>,
                                                  flight::SequenceView<double>, double, double) -> Outcome {
                                     ++first_calls;
                                     return std::nullopt;
                                   });
  layout::register_layout_resolver(state, flight::String("acme.Root"),
                                   [&second_calls](flight::Float32Array, flight::StructuralRef<TreeRow>,
                                                   flight::SequenceView<double>, double, double) -> Outcome {
                                     ++second_calls;
                                     return std::nullopt;
                                   });

  auto tree = tree_of({node("acme.Root", -1.0), node("acme.Leaf", 0.0)});
  auto out = flight::Float32Array(8u);
  layout::resolve_layout_tree(out, state, as_row(tree), flight::SequenceView<double>(sizes_of({0, 0, 0, 0})), 10.0, 10.0);
  check(second_calls == 1 && first_calls == 0, "re-registering a kind replaces the resolver rather than stacking");
}

// --- enableLayoutGuards ---------------------------------------------------------------------------------------

void test_the_guard_receives_the_structured_explanation_of_a_sentinel() {
  const auto state = layout::create_layout_state();
  std::vector<std::string> warnings;
  layout::enable_layout_guards(state, types::LayoutResolutionGuard([&warnings](flight::StructuralRef<ExplanationRow> explanation) {
    warnings.push_back(utf8(flight::row_get<flight::RowKey<"kind">>(explanation)));
  }));
  check(state->guard.has_value(), "enabling guards installs the sink on the state");

  // An output buffer one slot short: the driver records OutputTooSmall and returns false, and the guard is what
  // turns that silent sentinel into something a caller can see.
  auto tree = tree_of({node("acme.Root", -1.0)});
  auto out = flight::Float32Array(3u);
  const bool resolved = layout::resolve_layout_tree(out, state, as_row(tree),
                                                    flight::SequenceView<double>(sizes_of({0, 0})), 100.0, 100.0);
  check(!resolved, "resolution failed on the short buffer");
  check(warnings == std::vector<std::string>({"OutputTooSmall"}), "and the guard was warned with the explanation");
}

void test_the_guard_is_silent_when_resolution_succeeds() {
  const auto state = layout::create_layout_state();
  int warnings = 0;
  layout::enable_layout_guards(state, types::LayoutResolutionGuard([&warnings](flight::StructuralRef<ExplanationRow>) {
    ++warnings;
  }));

  auto tree = tree_of({node("acme.Root", -1.0)});
  auto out = flight::Float32Array(4u);
  check(layout::resolve_layout_tree(out, state, as_row(tree), flight::SequenceView<double>(sizes_of({0, 0})), 10.0, 10.0),
        "resolution succeeded");
  check(warnings == 0, "so the guard stayed silent");
}

// --- explainLayoutResolution: the static diagnosis ------------------------------------------------------------

void test_an_unregistered_container_is_diagnosed_without_running_resolution() {
  const auto state = layout::create_layout_state();
  auto tree = tree_of({node("acme.Root", -1.0), node("acme.Leaf", 0.0)});
  const auto explanation = layout::explain_layout_resolution(state, as_row(tree), 0.0);

  check(explanation.has_value(), "a root with an unregistered kind is diagnosed");
  if (!explanation.has_value()) return;
  check(utf8(explanation.value()->kind) == "UnregisteredKind", "as an unregistered kind");
  check_close(explanation.value()->node_index, 0.0, "naming the node");
  check_close(explanation.value()->parent_index, -1.0, "and the root sentinel as its parent");
  check_close(explanation.value()->actual_length, 0.0, "with no lengths reported, resolution never having run");
  check_close(explanation.value()->required_length, 0.0, "for either side");
  check(explanation.value()->resolver_kind.has_value() &&
            utf8(*explanation.value()->resolver_kind) == "acme.Root",
        "and the kind that has no resolver");
}

void test_an_invalid_hierarchy_is_diagnosed_without_retained_state() {
  const auto state = layout::create_layout_state();

  auto below_sentinel = tree_of({node("acme.Root", -2.0)});
  const auto first = layout::explain_layout_resolution(state, as_row(below_sentinel), 0.0);
  check(first.has_value() && utf8(first.value()->kind) == "InvalidHierarchy", "a parent below the sentinel is invalid");
  check(first.has_value() && first.value()->parent_index == -2.0, "with the offending parent index reported");

  auto self_parent = tree_of({node("acme.Root", 0.0)});
  const auto second = layout::explain_layout_resolution(state, as_row(self_parent), 0.0);
  check(second.has_value() && utf8(second.value()->kind) == "InvalidHierarchy",
        "and so is a node that is its own parent");
  check(second.has_value() && second.value()->parent_index == 0.0, "reported the same way");
}

void test_the_upper_node_index_is_exclusive() {
  const auto state = layout::create_layout_state();
  auto tree = tree_of({node("acme.Root", -1.0), node("acme.Leaf", 0.0)});
  check(!layout::explain_layout_resolution(state, as_row(tree), 2.0).has_value(),
        "the node index equal to the node count is past the end");
}

void test_an_unregistered_parent_is_reported_only_when_it_has_a_child() {
  const auto state = layout::create_layout_state();
  auto with_child = tree_of({node("acme.Root", -1.0), node("acme.Leaf", 0.0)});
  const auto explained = layout::explain_layout_resolution(state, as_row(with_child), 0.0);
  check(explained.has_value() && utf8(explained.value()->kind) == "UnregisteredKind",
        "a container that has something to arrange reports its missing resolver");
  check(explained.has_value() && explained.value()->parent_index == -1.0,
        "with the root sentinel as its parent");

  auto leaf_only = tree_of({node("acme.Root", -1.0)});
  check(!layout::explain_layout_resolution(state, as_row(leaf_only), 0.0).has_value(),
        "while a container with nothing under it has nothing to arrange and is not reported");
}

// --- resolveLayoutTree -----------------------------------------------------------------------------------------

void test_every_root_is_written_as_the_available_rectangle() {
  const auto state = layout::create_layout_state();
  auto tree = tree_of({node("acme.Root", -1.0), node("acme.Root", -1.0)});
  auto out = flight::Float32Array(8u);
  check(layout::resolve_layout_tree(out, state, as_row(tree), flight::SequenceView<double>(sizes_of({0, 0, 0, 0})), 320.0, 180.0),
        "two roots resolve without any resolver");
  check_close(out.element(0.0), 0.0, "the first root is at the origin");
  check_close(out.element(1.0), 0.0, "on both axes");
  check_close(out.element(2.0), 320.0, "with the full available width");
  check_close(out.element(3.0), 180.0, "and height");
  check_close(out.element(4.0), 0.0, "and the second root is too");
  check_close(out.element(7.0), 180.0, "at the same size");
}

void test_a_non_finite_or_non_positive_available_size_becomes_zero() {
  const auto state = layout::create_layout_state();
  auto tree = tree_of({node("acme.Root", -1.0)});
  auto out = flight::Float32Array(4u);
  check(layout::resolve_layout_tree(out, state, as_row(tree), flight::SequenceView<double>(sizes_of({0, 0})),
                                    std::nan(""), -1.0),
        "the root still resolves");
  check_close(out.element(2.0), 0.0, "but a NaN width becomes 0 rather than propagating");
  check_close(out.element(3.0), 0.0, "and so does a negative height");
}

void test_a_child_is_dispatched_through_its_parent_kind() {
  const auto state = layout::create_layout_state();
  double seen_parent = -9.0;
  double seen_child = -9.0;
  layout::register_layout_resolver(state, flight::String("acme.Root"),
                                   [&seen_parent, &seen_child](flight::Float32Array target, flight::StructuralRef<TreeRow>,
                                                               flight::SequenceView<double>, double parent_index,
                                                               double child_index) -> Outcome {
                                     seen_parent = parent_index;
                                     seen_child = child_index;
                                     const double offset = child_index * 4.0;
                                     target.set_index(offset, 10.0);
                                     target.set_index(offset + 1.0, 20.0);
                                     target.set_index(offset + 2.0, 30.0);
                                     target.set_index(offset + 3.0, 40.0);
                                     return std::nullopt;
                                   });

  auto tree = tree_of({node("acme.Root", -1.0), node("acme.Leaf", 0.0)});
  auto out = flight::Float32Array(8u);
  check(layout::resolve_layout_tree(out, state, as_row(tree), flight::SequenceView<double>(sizes_of({0, 0, 0, 0})), 100.0, 100.0),
        "a resolved tree reports success");
  check_close(seen_parent, 0.0, "the resolver was called with the parent's index");
  check_close(seen_child, 1.0, "and the child's");
  check_close(out.element(4.0), 10.0, "and the rectangle it wrote landed at the child's offset");
  check_close(out.element(7.0), 40.0, "all four values of it");
}

void test_the_output_buffer_is_measured_before_anything_else() {
  const auto state = layout::create_layout_state();
  auto tree = tree_of({node("acme.Root", -1.0), node("acme.Leaf", 0.0)});
  auto out = flight::Float32Array(7u);
  check(!layout::resolve_layout_tree(out, state, as_row(tree), flight::SequenceView<double>(sizes_of({0, 0, 0, 0})),
                                     100.0, 100.0),
        "one slot short of the four-per-node requirement fails");

  const auto explanation = layout::explain_layout_resolution(state, as_row(tree), -1.0);
  check(explanation.has_value() && utf8(explanation.value()->kind) == "OutputTooSmall",
        "with the shortage recorded as its own kind");
  check(explanation.has_value() && explanation.value()->actual_length == 7.0, "naming the actual length");
  check(explanation.has_value() && explanation.value()->required_length == 8.0, "and the required one");
  check(explanation.has_value() && explanation.value()->node_index == -1.0 &&
            explanation.value()->parent_index == -1.0,
        "with no node to blame, because no node was reached");
}

void test_the_intrinsic_size_buffer_is_measured_too() {
  const auto state = layout::create_layout_state();
  auto tree = tree_of({node("acme.Root", -1.0), node("acme.Leaf", 0.0)});
  auto out = flight::Float32Array(8u);
  check(!layout::resolve_layout_tree(out, state, as_row(tree), flight::SequenceView<double>(sizes_of({0, 0, 0})),
                                     100.0, 100.0),
        "too few intrinsic sizes fails even with an adequate output buffer");

  const auto explanation = layout::explain_layout_resolution(state, as_row(tree), -1.0);
  check(explanation.has_value() && utf8(explanation.value()->kind) == "IntrinsicSizesTooSmall",
        "with the shortage recorded as its own kind, distinct from the output one");
}

void test_a_child_that_does_not_follow_its_parent_is_rejected() {
  const auto state = layout::create_layout_state();
  auto tree = tree_of({node("acme.Root", 0.0)});
  auto out = flight::Float32Array(4u);
  check(!layout::resolve_layout_tree(out, state, as_row(tree), flight::SequenceView<double>(sizes_of({0, 0})), 100.0, 100.0),
        "a node that is its own parent does not resolve");

  const auto explanation = layout::explain_layout_resolution(state, as_row(tree), 0.0);
  check(explanation.has_value() && utf8(explanation.value()->kind) == "InvalidHierarchy", "it is an invalid hierarchy");
  check(explanation.has_value() && explanation.value()->node_index == 0.0, "reported at that node");
}

void test_a_parent_index_below_the_root_sentinel_is_rejected() {
  const auto state = layout::create_layout_state();
  auto tree = tree_of({node("acme.Root", -2.0)});
  auto out = flight::Float32Array(4u);
  check(!layout::resolve_layout_tree(out, state, as_row(tree), flight::SequenceView<double>(sizes_of({0, 0})), 100.0, 100.0),
        "a parent below the sentinel does not resolve");

  const auto explanation = layout::explain_layout_resolution(state, as_row(tree), 0.0);
  check(explanation.has_value() && utf8(explanation.value()->kind) == "InvalidHierarchy", "it is an invalid hierarchy");
  check(explanation.has_value() && explanation.value()->parent_index == -2.0,
        "and the parent index it named is the one reported");
}

void test_a_failure_names_the_node_its_resolver_blames() {
  const auto state = layout::create_layout_state();

  layout::register_layout_resolver(state, flight::String("acme.Root"),
                                   [](flight::Float32Array, flight::StructuralRef<TreeRow>, flight::SequenceView<double>,
                                      double, double) -> Outcome { return flight::String("InvalidItemStyle"); });
  auto tree = tree_of({node("acme.Root", -1.0), node("acme.Leaf", 0.0)});
  auto out = flight::Float32Array(8u);
  check(!layout::resolve_layout_tree(out, state, as_row(tree), flight::SequenceView<double>(sizes_of({0, 0, 0, 0})),
                                     100.0, 100.0),
        "a resolver that reports a failure fails the resolution");

  const auto item = layout::explain_layout_resolution(state, as_row(tree), -1.0);
  check(item.has_value() && utf8(item.value()->kind) == "InvalidItemStyle", "the resolver's own kind is retained");
  check(item.has_value() && item.value()->node_index == 1.0,
        "and an item-style failure is charged to the CHILD, not the container that arranged it");
  check(item.has_value() && item.value()->resolver_kind.has_value() &&
            utf8(*item.value()->resolver_kind) == "acme.Root",
        "with the resolver kind that raised it");

  // The other half of the same rule: a container-style failure is charged to the PARENT instead.
  layout::register_layout_resolver(state, flight::String("acme.Root"),
                                   [](flight::Float32Array, flight::StructuralRef<TreeRow>, flight::SequenceView<double>,
                                      double, double) -> Outcome { return flight::String("InvalidContainerStyle"); });
  auto second_out = flight::Float32Array(8u);
  check(!layout::resolve_layout_tree(second_out, state, as_row(tree),
                                     flight::SequenceView<double>(sizes_of({0, 0, 0, 0})), 100.0, 100.0),
        "the container-style failure fails too");
  const auto container = layout::explain_layout_resolution(state, as_row(tree), -1.0);
  check(container.has_value() && utf8(container.value()->kind) == "InvalidContainerStyle",
        "retained as its own kind");
  check(container.has_value() && container.value()->node_index == 0.0,
        "and charged to the PARENT this time");
}

void test_a_retained_failure_answers_only_for_its_own_node() {
  const auto state = layout::create_layout_state();
  layout::register_layout_resolver(state, flight::String("acme.Root"),
                                   [](flight::Float32Array, flight::StructuralRef<TreeRow>, flight::SequenceView<double>,
                                      double, double) -> Outcome { return flight::String("InvalidItemStyle"); });
  auto tree = tree_of({node("acme.Root", -1.0), node("acme.Leaf", 0.0)});
  auto out = flight::Float32Array(8u);
  static_cast<void>(layout::resolve_layout_tree(out, state, as_row(tree),
                                                flight::SequenceView<double>(sizes_of({0, 0, 0, 0})), 100.0, 100.0));

  check(kind_of(layout::explain_layout_resolution(state, as_row(tree), -1.0)) == "InvalidItemStyle",
        "a negative index asks for whatever is retained");
  check(kind_of(layout::explain_layout_resolution(state, as_row(tree), 1.0)) == "InvalidItemStyle",
        "and so does the node the failure names");
  check(!layout::explain_layout_resolution(state, as_row(tree), 0.0).has_value(),
        "while a different node is answered with nothing, not with someone else's failure");
}

void test_a_node_zero_failure_does_not_escape_to_another_node() {
  const auto state = layout::create_layout_state();
  auto tree = tree_of({node("acme.Root", -1.0), node("acme.Leaf", 0.0)});
  auto out = flight::Float32Array(8u);
  check(!layout::resolve_layout_tree(out, state, as_row(tree), flight::SequenceView<double>(sizes_of({0, 0, 0, 0})),
                                     100.0, 100.0),
        "an unregistered root fails the resolution");
  check(kind_of(layout::explain_layout_resolution(state, as_row(tree), 0.0)) == "UnregisteredKind",
        "and the failing node reports it");
  check(!layout::explain_layout_resolution(state, as_row(tree), 1.0).has_value(),
        "while the child, which is not the retained failure's node, reports nothing");
}

void test_a_successful_resolution_clears_what_a_failed_one_retained() {
  const auto state = layout::create_layout_state();
  auto tree = tree_of({node("acme.Root", -1.0)});

  auto too_small = flight::Float32Array(3u);
  check(!layout::resolve_layout_tree(too_small, state, as_row(tree),
                                     flight::SequenceView<double>(sizes_of({0, 0})), 100.0, 100.0),
        "the short buffer fails first");
  check(state->last_failure_kind.has_value(), "leaving a failure retained on the state");

  auto adequate = flight::Float32Array(4u);
  check(layout::resolve_layout_tree(adequate, state, as_row(tree), flight::SequenceView<double>(sizes_of({0, 0})),
                                    100.0, 100.0),
        "and the next resolution succeeds");
  check(!layout::explain_layout_resolution(state, as_row(tree), -1.0).has_value(),
        "so nothing stale is left to explain");
  check_close(state->last_failure_node_index, -1.0, "the node sentinel is back");
  check_close(state->last_failure_parent_index, -1.0, "and so is the parent sentinel");
}

void test_resolving_again_rewrites_the_same_output_array() {
  const auto state = layout::create_layout_state();
  auto tree = tree_of({node("acme.Root", -1.0)});
  auto out = flight::Float32Array(4u);

  check(layout::resolve_layout_tree(out, state, as_row(tree), flight::SequenceView<double>(sizes_of({0, 0})), 10.0, 20.0),
        "a first resolution writes the caller's array");
  check_close(out.element(2.0), 10.0, "with the first available width");
  check(layout::resolve_layout_tree(out, state, as_row(tree), flight::SequenceView<double>(sizes_of({0, 0})), 30.0, 40.0),
        "a second resolution reuses it");
  check_close(out.element(2.0), 30.0, "rewriting the width in place");
  check_close(out.element(3.0), 40.0, "and the height");
}

} // namespace

int main() {
  test_a_fresh_state_is_initialized_to_its_declared_defaults();
  test_registering_and_unregistering_a_resolver_kind();
  test_registering_the_same_kind_again_replaces_it();

  test_the_guard_receives_the_structured_explanation_of_a_sentinel();
  test_the_guard_is_silent_when_resolution_succeeds();

  test_an_unregistered_container_is_diagnosed_without_running_resolution();
  test_an_invalid_hierarchy_is_diagnosed_without_retained_state();
  test_the_upper_node_index_is_exclusive();
  test_an_unregistered_parent_is_reported_only_when_it_has_a_child();

  test_every_root_is_written_as_the_available_rectangle();
  test_a_non_finite_or_non_positive_available_size_becomes_zero();
  test_a_child_is_dispatched_through_its_parent_kind();
  test_the_output_buffer_is_measured_before_anything_else();
  test_the_intrinsic_size_buffer_is_measured_too();
  test_a_child_that_does_not_follow_its_parent_is_rejected();
  test_a_parent_index_below_the_root_sentinel_is_rejected();
  test_a_failure_names_the_node_its_resolver_blames();
  test_a_retained_failure_answers_only_for_its_own_node();
  test_a_node_zero_failure_does_not_escape_to_another_node();
  test_a_successful_resolution_clears_what_a_failed_one_retained();
  test_resolving_again_rewrites_the_same_output_array();

  if (failures == 0) {
    std::cout << "layout parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
