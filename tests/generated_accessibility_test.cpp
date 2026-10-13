// Native parity test for @flighthq/accessibility, ported from its own accessibility.test.ts (6 cases).
//
// The package is six functions that do nothing but route an operation to the Host provider the caller selected.
// There is no module state, no registry and no caching, which is exactly why the contract worth pinning is not
// "does it work" but "what does it pass through, unchanged":
//
//   * The DEFAULTS. `announceAccessibility` declares liveness as optional and the module substitutes 'polite'; a
//     caller who supplies 'assertive' must get 'assertive', not the default.
//   * The OUTCOME. Every operation returns the provider's own result object, and the module rebuilds nothing: the
//     variant ARM the provider answered with is the arm the caller receives, as well as its reason.
//   * The SELECTION. Every call goes to the provider handed in and to no other, which is what makes the Host
//     parameter explicit rather than ambient -- destroy is asserted against a second provider that must stay
//     untouched, and a call log asserts each function reaches exactly its own entry point.
//   * The PAYLOAD. A published node arrives whole: id, role, label, description, value, parentId (the tree
//     relationship), and its nested bounds and states rows. Those last two are rows inside a row, so a lowering
//     that flattened them would drop them here.
//   * No validation. An empty id is forwarded as given; deciding whether an id means anything is the provider's
//     job, and a module that invented its own sentinel for it would be answering for a seam it does not own.
//   * A throwing seam is not swallowed.
//
// The type is at the current pin, emitted whole with no override, so this is the generated module's own surface.
//
// One thing the emitted tree cannot do, recorded in docs/parity/plat.md rather than worked around here: the
// generated row-member table has no key for `role`, `description` or `parentId`, so those members cannot be read
// by key (`flight::row_get<flight::RowKey<"role">>`) even though a Ref-backed row carries them faithfully -- see
// test_a_published_node_arrives_whole, which is written against the object, not the key.
#include <flight/accessibility/accessibility.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

namespace accessibility = flight::accessibility;
namespace types = flight::types;

using Host = types::HostAccessibilityCapability;
using HostRow = flight::RowReadonly<flight::RowOf<flight::Ref<Host>>>;
using Node = types::AccessibilityNode;
using NodeRow = flight::RowReadonly<flight::RowOf<flight::Ref<Node>>>;
using BoundsRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::Rectangle>>>;
using StatesRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::AccessibilityState>>>;
using Blocked = types::reason_76420527388c74c3<flight::String>;
using Plain = types::reason_5d065c7686cd3932<flight::String>;
using Outcome = types::AccessibilityOperationOutcome<flight::String>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

std::string utf8(const flight::String& value) { return value.to_utf8(); }

std::string reason_of(const Outcome& outcome) {
  if (outcome.index() == 0) return utf8(std::get<0>(outcome)->reason);
  return utf8(std::get<1>(outcome)->reason);
}

Outcome plain(const char* reason) {
  return Outcome{std::in_place_index<1>, flight::make_ref<Plain>(Plain{.reason = flight::String(reason)})};
}

Outcome blocked(const char* reason) {
  return Outcome{std::in_place_index<0>, flight::make_ref<Blocked>(Blocked{.reason = flight::String(reason)})};
}

// A provider that records which entry point was called and with what, and answers with a scripted outcome.
struct Provider {
  std::vector<std::string> calls;
  flight::String last_message;
  flight::String last_liveness;
  flight::String last_id;
  flight::Ref<Node> last_node;
  bool last_node_set = false;
  Outcome answer = plain("ok");
  bool destroy_throws = false;

  flight::Ref<Host> capability() {
    auto host = flight::make_ref<Host>();
    host->announce = [this](flight::String message, flight::String liveness) {
      calls.push_back("announce");
      last_message = message;
      last_liveness = liveness;
      return answer;
    };
    host->clear = [this]() {
      calls.push_back("clear");
      return answer;
    };
    host->destroy = [this]() {
      calls.push_back("destroy");
      if (destroy_throws) throw std::runtime_error("provider is gone");
    };
    host->remove_node = [this](flight::String id) {
      calls.push_back("removeNode");
      last_id = id;
      return answer;
    };
    host->set_focus = [this](flight::String id) {
      calls.push_back("setFocus");
      last_id = id;
      return answer;
    };
    host->set_node = [this](flight::StructuralRef<NodeRow> node) {
      calls.push_back("setNode");
      last_node = flight::structural_ref_cast<flight::Ref<Node>>(node);
      last_node_set = last_node != nullptr;
      return answer;
    };
    return host;
  }

  flight::StructuralRef<HostRow> row() {
    const auto host = capability();
    return flight::make_structural_ref<HostRow>(flight::row_field<flight::RowKey<"announce">>(host->announce),
                                                flight::row_field<flight::RowKey<"clear">>(host->clear),
                                                flight::row_field<flight::RowKey<"destroy">>(host->destroy),
                                                flight::row_field<flight::RowKey<"removeNode">>(host->remove_node),
                                                flight::row_field<flight::RowKey<"setFocus">>(host->set_focus),
                                                flight::row_field<flight::RowKey<"setNode">>(host->set_node));
  }
};

// --- announce: the default and the payload ---------------------------------------------------------------------

void test_announce_supplies_polite_when_liveness_is_omitted() {
  Provider provider;
  const auto row = provider.row();
  const auto outcome = accessibility::announce_accessibility(row, flight::String("saved"));

  check(provider.calls == std::vector<std::string>({"announce"}), "announce reached the provider's announce");
  check(utf8(provider.last_message) == "saved", "with the message it was given");
  check(utf8(provider.last_liveness) == "polite",
        "and 'polite' where the caller gave none, rather than an empty liveness");
  check(reason_of(outcome) == "ok" && outcome.index() == 1, "while returning the provider's own outcome");
}

void test_announce_passes_an_explicit_liveness_through() {
  Provider provider;
  const auto row = provider.row();
  accessibility::announce_accessibility(row, flight::String("incoming call"), flight::String("assertive"));

  check(utf8(provider.last_liveness) == "assertive",
        "an explicit liveness is passed through rather than replaced by the default");
}

void test_announce_returns_the_provider_outcome_unchanged() {
  Provider provider;
  const auto row = provider.row();

  provider.answer = blocked("no-dom");
  const auto blocked_result = accessibility::announce_accessibility(row, flight::String("x"));
  check(reason_of(blocked_result) == "no-dom", "a blocked outcome keeps its reason");
  check(blocked_result.index() == 0, "and the arm the provider answered with");

  provider.answer = plain("ok");
  const auto plain_result = accessibility::announce_accessibility(row, flight::String("x"));
  check(reason_of(plain_result) == "ok" && plain_result.index() == 1,
        "and an ok outcome keeps its own arm, so the two are not collapsed into one");
}

// --- The other four delegating operations ----------------------------------------------------------------------

void test_clear_returns_the_provider_outcome() {
  Provider provider;
  provider.answer = blocked("destroyed");
  const auto outcome = accessibility::clear_accessibility_tree(provider.row());
  check(provider.calls == std::vector<std::string>({"clear"}), "clear reached the provider's clear");
  check(reason_of(outcome) == "destroyed" && outcome.index() == 0, "and returned its outcome unchanged");
}

void test_destroy_calls_only_the_selected_provider() {
  Provider first;
  Provider second;
  const auto first_row = first.row();
  static_cast<void>(second.row());

  accessibility::destroy_accessibility(first_row);
  check(first.calls == std::vector<std::string>({"destroy"}), "destroy reached that provider");
  check(second.calls.empty(), "and no other provider was touched");
}

void test_remove_node_forwards_the_id_and_returns_the_not_found_outcome() {
  Provider provider;
  provider.answer = blocked("node-not-found");
  const auto outcome = accessibility::remove_accessibility_node(provider.row(), flight::String("missing"));

  check(provider.calls == std::vector<std::string>({"removeNode"}), "removeNode reached the provider's removeNode");
  check(utf8(provider.last_id) == "missing", "forwards the id it was given");
  check(reason_of(outcome) == "node-not-found", "and returns the provider's not-found outcome");
}

void test_set_focus_forwards_the_id_and_returns_the_not_moved_outcome() {
  Provider provider;
  provider.answer = blocked("focus-not-moved");
  const auto outcome = accessibility::set_accessibility_focus(provider.row(), flight::String("subject"));

  check(provider.calls == std::vector<std::string>({"setFocus"}), "setFocus reached the provider's setFocus");
  check(utf8(provider.last_id) == "subject", "forwards the id it was given");
  check(reason_of(outcome) == "focus-not-moved", "and returns the provider's not-moved outcome");
}

void test_an_empty_id_is_forwarded_rather_than_validated() {
  Provider provider;
  provider.answer = blocked("node-not-found");
  const auto outcome = accessibility::remove_accessibility_node(provider.row(), flight::String(""));

  check(utf8(provider.last_id) == "", "an empty id reaches the provider as an empty id");
  check(reason_of(outcome) == "node-not-found",
        "and the provider's own answer is what the caller sees, the module having judged nothing");
}

// --- The published node ---------------------------------------------------------------------------------------

flight::Ref<Node> full_node() {
  const auto bounds = flight::make_structural_ref<BoundsRow>(flight::row_field<flight::RowKey<"x">>(10.0),
                                                            flight::row_field<flight::RowKey<"y">>(20.0),
                                                            flight::row_field<flight::RowKey<"width">>(30.0),
                                                            flight::row_field<flight::RowKey<"height">>(40.0));
  const auto states = flight::make_structural_ref<StatesRow>(
      flight::row_field<flight::RowKey<"disabled">>(true), flight::row_field<flight::RowKey<"checked">>(false),
      flight::row_field<flight::RowKey<"expanded">>(true));
  return flight::make_ref<Node>(Node{.id = flight::String("subject"),
                                     .role = flight::String("button"),
                                     .label = flight::String("Subject"),
                                     .description = flight::String("Opens the subject"),
                                     .value = flight::String("ready"),
                                     .parent_id = flight::String("root"),
                                     .bounds = bounds,
                                     .states = states});
}

void test_a_published_node_arrives_whole() {
  Provider provider;
  const auto node = full_node();
  const auto outcome = accessibility::set_accessibility_node(provider.row(), flight::StructuralRef<NodeRow>(node));

  check(provider.calls == std::vector<std::string>({"setNode"}), "setNode reached the provider's setNode");
  check(reason_of(outcome) == "ok", "and returned the provider's outcome");
  check(provider.last_node_set, "the provider recovered the published node");
  if (!provider.last_node_set) return;

  const auto received = provider.last_node;
  check(utf8(received->id) == "subject" && utf8(received->role) == "button",
        "with its id and its semantic role");
  check(received->label.has_value() && utf8(*received->label) == "Subject", "and its label");
  check(received->description.has_value() && utf8(*received->description) == "Opens the subject",
        "and its description");
  check(received->value.has_value() && utf8(*received->value) == "ready", "and its value");
  check(received->parent_id.has_value() && utf8(*received->parent_id) == "root",
        "and its place in the tree");
  check(received->bounds.has_value(), "and its bounds row");
  if (received->bounds.has_value()) {
    check(flight::row_get<flight::RowKey<"x">>(*received->bounds) == 10.0 &&
              flight::row_get<flight::RowKey<"width">>(*received->bounds) == 30.0,
          "whose geometry survived the nesting");
  }
  check(received->states.has_value(), "and its states row");
  if (received->states.has_value()) {
    // A member of a partial row reads back as an optional, so a `false` state is only distinguishable from an
    // absent one by comparing the optional itself -- a bool conversion would call the present-false one true.
    check(flight::row_get<flight::RowKey<"disabled">>(*received->states) == std::optional<bool>(true) &&
              flight::row_get<flight::RowKey<"checked">>(*received->states) == std::optional<bool>(false) &&
              flight::row_get<flight::RowKey<"expanded">>(*received->states) == std::optional<bool>(true),
          "with each state it carried, set or clear, preserved -- including the cleared one, which is present "
          "and false rather than absent");
  }
}

void test_the_published_node_is_the_object_the_caller_handed_over() {
  Provider provider;
  const auto node = full_node();
  accessibility::set_accessibility_node(provider.row(), flight::StructuralRef<NodeRow>(node));

  check(provider.last_node_set && provider.last_node.get() == node.get(),
        "the provider receives the very object that was published, not a copy of it");
}

void test_a_sparse_node_is_published_with_its_absences_intact() {
  Provider provider;
  const auto node = flight::make_ref<Node>(Node{.id = flight::String("plain"),
                                                .role = flight::String("group"),
                                                .label = std::nullopt,
                                                .description = std::nullopt,
                                                .value = std::nullopt,
                                                .parent_id = std::nullopt,
                                                .bounds = std::nullopt,
                                                .states = std::nullopt});
  accessibility::set_accessibility_node(provider.row(), flight::StructuralRef<NodeRow>(node));

  check(provider.last_node_set, "a node with only id and role publishes");
  if (!provider.last_node_set) return;
  check(!provider.last_node->label.has_value() && !provider.last_node->bounds.has_value() &&
            !provider.last_node->states.has_value(),
        "and the members it left out are still absent rather than defaulted to something present");
}

void test_publishing_uses_only_the_supplied_host() {
  Provider first;
  Provider second;
  const auto node = full_node();
  accessibility::set_accessibility_node(first.row(), flight::StructuralRef<NodeRow>(node));

  check(first.calls == std::vector<std::string>({"setNode"}), "the supplied host published the node");
  check(second.calls.empty(), "and the other host was never reached");
}

// --- The host seam ---------------------------------------------------------------------------------------------

void test_each_function_reaches_exactly_its_own_entry_point() {
  Provider provider;
  const auto row = provider.row();
  const auto node = full_node();

  accessibility::announce_accessibility(row, flight::String("m"));
  accessibility::set_accessibility_node(row, flight::StructuralRef<NodeRow>(node));
  accessibility::set_accessibility_focus(row, flight::String("i"));
  accessibility::remove_accessibility_node(row, flight::String("i"));
  accessibility::clear_accessibility_tree(row);
  accessibility::destroy_accessibility(row);

  check(provider.calls == std::vector<std::string>({"announce", "setNode", "setFocus", "removeNode", "clear",
                                                    "destroy"}),
        "each of the six functions called its own entry point once, in the order invoked");
}

void test_a_throwing_host_seam_is_not_swallowed() {
  Provider provider;
  provider.destroy_throws = true;
  bool threw = false;
  try {
    accessibility::destroy_accessibility(provider.row());
  } catch (const std::exception&) {
    threw = true;
  }
  check(threw, "a provider that throws on destroy propagates rather than being absorbed by the module");
}

} // namespace

int main() {
  test_announce_supplies_polite_when_liveness_is_omitted();
  test_announce_passes_an_explicit_liveness_through();
  test_announce_returns_the_provider_outcome_unchanged();

  test_clear_returns_the_provider_outcome();
  test_destroy_calls_only_the_selected_provider();
  test_remove_node_forwards_the_id_and_returns_the_not_found_outcome();
  test_set_focus_forwards_the_id_and_returns_the_not_moved_outcome();
  test_an_empty_id_is_forwarded_rather_than_validated();

  test_a_published_node_arrives_whole();
  test_the_published_node_is_the_object_the_caller_handed_over();
  test_a_sparse_node_is_published_with_its_absences_intact();
  test_publishing_uses_only_the_supplied_host();

  test_each_function_reaches_exactly_its_own_entry_point();
  test_a_throwing_host_seam_is_not_swallowed();

  if (failures == 0) {
    std::cout << "accessibility parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
