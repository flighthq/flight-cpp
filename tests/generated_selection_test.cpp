#include <flight/selection/contract.hpp>
#include <flight/signals/slot.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Native ports of the a627849 selection suites, with exact generated owners in place of test fakes.
//
// MUTATION SENSITIVITY, measured 2026-10-12: eleven one-at-a-time mutations to the selection
// completion headers were compiled and run, then restored. The suite killed removal of duplicate and
// equivalent-select guards, wrong active fallback, lost select-all de-duplication, aliased signal
// payloads, unconditional active signals, removed alternate-pick and pointer-up drag guards, inactive
// lasso point collection, excluded polygon edges, and swapped marquee intersect/contain modes.

namespace {

namespace selection = flight::selection;
namespace signals = flight::signals;
namespace types = flight::types;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename RuntimeRef>
void check_runtime_cells(const RuntimeRef& runtime, const std::string& label) {
  flight::Ref<types::EntityRuntime> base = runtime;
  check(base.get() == runtime.get() && !base.owner_before(runtime) &&
            !runtime.owner_before(base) && &base->binding == &runtime->binding &&
            &base->uid == &runtime->uid,
        label + " retains its original owner and inherited runtime cells");
  const auto previous = runtime->uid;
  base->uid = flight::String("selection-runtime-alias");
  check(runtime->uid == base->uid, label + " reads the live inherited uid cell");
  base->uid = previous;
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return View(value);
}

template <typename Value>
bool identities_are(
    const flight::Array<Value>& actual, std::initializer_list<Value> expected) {
  if (actual.size() != expected.size()) return false;
  std::size_t index = 0;
  for (const auto& value : expected) {
    if (actual.element(static_cast<double>(index)) != value) return false;
    ++index;
  }
  return true;
}

template <typename Callback>
void expect_bad_cast(Callback&& callback, const std::string& message) {
  try {
    callback();
    check(false, message);
  } catch (const std::bad_cast&) {
  } catch (...) {
    check(false, message + " (wrong exception type)");
  }
}

struct TestNode : flight::ReferenceEnabled {
  int id;
};

using TestNodeRef = flight::Ref<TestNode>;

TestNodeRef make_test_node(int id) {
  return flight::make_ref<TestNode>(TestNode{.id = id});
}

flight::Ref<types::SelectionModifierState> make_modifiers(
    bool alt = false, bool ctrl = false, bool meta = false, bool shift = false) {
  return flight::make_ref<types::SelectionModifierState>(types::SelectionModifierState{
      .alt_key = alt,
      .ctrl_key = ctrl,
      .meta_key = meta,
      .shift_key = shift,
  });
}

flight::Ref<types::BoundsNodeAny> make_bounds_node(
    double x, double y, double width, double height, std::shared_ptr<int> compute_count) {
  auto node = flight::make_ref<types::BoundsNodeAny>();
  node->data = std::nullopt;
  node->enabled = true;
  node->kind = flight::String("SelectionTestNode");
  node->name = std::nullopt;

  using RuntimeRef = typename decltype(node->entity_runtime_key)::value_type;
  using Runtime = typename RuntimeRef::element_type;
  auto runtime = flight::make_ref<Runtime>();
  runtime->binding = std::nullopt;
  runtime->uid = std::nullopt;
  runtime->local_bounds_id = 1.0;
  runtime->local_bounds_using_local_bounds_id = 0.0;
  runtime->local_bounds_rectangle = std::nullopt;
  runtime->world_bounds_rectangle = std::nullopt;
  runtime->is_local_bounds_rectangle_valid = std::nullopt;
  runtime->compute_local_bounds_rectangle =
      [=](flight::Ref<types::Rectangle> rectangle, auto) {
        ++*compute_count;
        rectangle->x = x;
        rectangle->y = y;
        rectangle->width = width;
        rectangle->height = height;
      };
  node->entity_runtime_key = runtime;
  return node;
}

void test_selection_state_and_signals() {
  auto a = make_test_node(1);
  auto b = make_test_node(2);
  auto c = make_test_node(3);
  auto missing = make_test_node(4);
  auto state = selection::create_selection_state<TestNodeRef>();
  check_runtime_cells(selection::get_selection_state_runtime<TestNodeRef>(state), "selection");

  using ActiveSlot = std::function<void(std::optional<TestNodeRef>)>;
  using ChangeSlot = std::function<void(flight::Array<TestNodeRef>)>;
  auto signal_view = selection::get_selection_signals<TestNodeRef>(state);
  auto state_signals =
      signal_view.shared_exact_object<types::SelectionSignals<TestNodeRef>>();
  check(state_signals != nullptr, "selection signals preserve their exact generic owner");

  std::vector<std::optional<TestNodeRef>> active_events;
  std::vector<flight::Array<TestNodeRef>> change_events;
  ActiveSlot active_slot = [&](std::optional<TestNodeRef> active) {
    active_events.push_back(active);
  };
  ChangeSlot change_slot = [&](flight::Array<TestNodeRef> selected) {
    change_events.push_back(selected);
  };
  signals::connect_signal<ActiveSlot>(state_signals->on_active_change, active_slot);
  signals::connect_signal<ChangeSlot>(state_signals->on_change, change_slot);

  check(!selection::has_selection<TestNodeRef>(state), "new selection state starts empty");
  check(!selection::get_active_node<TestNodeRef>(state).has_value(),
        "new selection has no active node");

  selection::add_node_to_selection(state, a);
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {a}),
        "add preserves identity");
  check(selection::get_active_node<TestNodeRef>(state) == std::optional<TestNodeRef>{a},
        "add makes the node active");
  check(change_events.size() == 1 && identities_are(change_events[0], {a}),
        "add emits the finalized selection snapshot");

  selection::add_node_to_selection(state, b);
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {a, b}),
        "add preserves insertion order");
  check(change_events.size() == 2 && identities_are(change_events[0], {a}) &&
            identities_are(change_events[1], {a, b}),
        "selection change payloads are stable snapshots");
  const auto event_count = change_events.size();
  const auto active_event_count = active_events.size();
  selection::add_node_to_selection(state, a);
  check(change_events.size() == event_count && active_events.size() == active_event_count &&
            selection::get_active_node<TestNodeRef>(state) == std::optional<TestNodeRef>{b},
        "duplicate add is an identity no-op");

  selection::remove_node_from_selection(state, b);
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {a}) &&
            selection::get_active_node<TestNodeRef>(state) == std::optional<TestNodeRef>{a},
        "removing the active node falls back to the last remaining node");
  const auto remove_event_count = change_events.size();
  selection::remove_node_from_selection(state, missing);
  check(change_events.size() == remove_event_count,
        "removing a missing identity suppresses change signals");

  selection::select_all_nodes(state, flight::Array<TestNodeRef>{c, a, c, b});
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {c, a, b}) &&
            selection::get_selection_count<TestNodeRef>(state) == 3.0 &&
            selection::get_active_node<TestNodeRef>(state) == std::optional<TestNodeRef>{b},
        "select-all de-duplicates in caller order and activates the last identity");
  const auto select_all_event_count = change_events.size();
  const auto select_all_active_count = active_events.size();
  selection::select_all_nodes(state, flight::Array<TestNodeRef>{a, c, b});
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {a, c, b}) &&
            change_events.size() == select_all_event_count + 1 &&
            active_events.size() == select_all_active_count,
        "reordering selection emits change but suppresses unchanged-active signals");
  selection::select_all_nodes(state, flight::Array<TestNodeRef>{c, a, c, b});
  const auto restored_select_all_event_count = change_events.size();
  selection::select_all_nodes(state, flight::Array<TestNodeRef>{c, a, c, b});
  check(change_events.size() == restored_select_all_event_count,
        "equivalent select-all suppresses change signals");

  selection::select_node(state, a);
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {a}) &&
            selection::is_node_selected(state, a) &&
            !selection::is_node_selected(state, b),
        "select-node replaces the ordered identity set");
  const auto select_node_event_count = change_events.size();
  const auto select_node_active_count = active_events.size();
  selection::select_node(state, a);
  check(change_events.size() == select_node_event_count &&
            active_events.size() == select_node_active_count,
        "equivalent select-node suppresses both signals");

  selection::toggle_node_selection(state, a);
  check(!selection::has_selection<TestNodeRef>(state) &&
            !selection::get_active_node<TestNodeRef>(state).has_value(),
        "toggle removes a selected identity and clears active");
  selection::toggle_node_selection(state, c);
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {c}),
        "toggle adds an unselected identity");
  selection::clear_selection<TestNodeRef>(state);
  check(!selection::has_selection<TestNodeRef>(state), "clear empties the selection");
  const auto clear_event_count = change_events.size();
  const auto clear_active_count = active_events.size();
  selection::clear_selection<TestNodeRef>(state);
  check(change_events.size() == clear_event_count && active_events.size() == clear_active_count,
        "clearing an empty selection is a signal no-op");
}

void test_pointer_policy() {
  auto a = make_test_node(11);
  auto b = make_test_node(12);
  auto c = make_test_node(13);
  auto state = selection::create_selection_state<TestNodeRef>();
  auto none = make_modifiers();
  auto shift = make_modifiers(false, false, false, true);
  auto ctrl = make_modifiers(false, true);
  auto meta = make_modifiers(false, false, true);
  auto alt = make_modifiers(true);

  selection::select_all_nodes(state, flight::Array<TestNodeRef>{a, b});
  selection::apply_pointer_selection_policy(state, std::optional<TestNodeRef>{a}, readonly(none));
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {a, b}),
        "unmodified pointer-down keeps an already-selected hit range");
  selection::apply_pointer_selection_policy(state, std::optional<TestNodeRef>{c}, readonly(none));
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {c}),
        "unmodified pointer-down replaces selection for a new hit");
  selection::apply_pointer_selection_policy<TestNodeRef>(state, std::nullopt, readonly(none));
  check(!selection::has_selection<TestNodeRef>(state),
        "unmodified empty pointer-down clears selection");

  selection::select_node(state, a);
  selection::apply_pointer_selection_policy<TestNodeRef>(state, std::nullopt, readonly(ctrl));
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {a}),
        "modified empty pointer-down preserves selection");
  selection::apply_pointer_selection_policy(state, std::optional<TestNodeRef>{b}, readonly(shift));
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {a, b}),
        "shift pointer-down adds a new hit");
  selection::apply_pointer_selection_policy(state, std::optional<TestNodeRef>{b}, readonly(shift));
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {a}),
        "shift pointer-down toggles an existing hit");
  selection::apply_pointer_selection_policy(state, std::optional<TestNodeRef>{a}, readonly(ctrl));
  check(!selection::has_selection<TestNodeRef>(state), "control pointer-down toggles a hit");
  selection::apply_pointer_selection_policy(state, std::optional<TestNodeRef>{c}, readonly(meta));
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {c}),
        "meta pointer-down toggles a hit");
  selection::apply_pointer_selection_policy(state, std::optional<TestNodeRef>{a}, readonly(alt));
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {c}),
        "alternate-pick pointer-down leaves selection unchanged");

  selection::select_all_nodes(state, flight::Array<TestNodeRef>{a, b});
  selection::apply_pointer_up_selection_policy(
      state, std::optional<TestNodeRef>{a}, readonly(none), false);
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {a}),
        "plain pointer-up narrows an already-selected hit");
  selection::select_all_nodes(state, flight::Array<TestNodeRef>{a, b});
  selection::apply_pointer_up_selection_policy(
      state, std::optional<TestNodeRef>{a}, readonly(none), true);
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {a, b}),
        "dragged pointer-up preserves the selection range");
  selection::apply_pointer_up_selection_policy(
      state, std::optional<TestNodeRef>{a}, readonly(shift), false);
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {a, b}),
        "modified pointer-up preserves the selection range");
  selection::apply_pointer_up_selection_policy(
      state, std::optional<TestNodeRef>{c}, readonly(none), false);
  check(identities_are(selection::get_selected_nodes<TestNodeRef>(state), {a, b}),
        "pointer-up ignores a hit outside the existing selection");
}

void test_lasso_and_marquee() {
  auto lasso = selection::create_lasso_selection();
  check_runtime_cells(selection::get_lasso_selection_runtime(lasso), "lasso");
  auto lasso_path_view = selection::get_lasso_selection_path(readonly(lasso));
  auto path = lasso_path_view.shared_exact_object<types::Path>();
  check(path != nullptr && path->commands.empty() && path->data.empty() &&
            path->winding == flight::String("evenOdd"),
        "lasso owns one empty even-odd path");
  selection::add_lasso_selection_point(lasso, 9.0, 9.0);
  check(path->data.empty(), "inactive lasso ignores points");

  selection::begin_lasso_selection(lasso, 0.0, 0.0);
  selection::add_lasso_selection_point(lasso, 10.0, 0.0);
  selection::add_lasso_selection_point(lasso, 10.0, 10.0);
  selection::add_lasso_selection_point(lasso, 0.0, 10.0);
  auto ended_path = selection::end_lasso_selection(lasso);
  check(ended_path == path && path->commands.size() == 5 &&
            path->commands.element(0.0) == types::path_command->move_to &&
            path->commands.element(4.0) == types::path_command->close,
        "lasso end closes and returns the live path identity");
  selection::end_lasso_selection(lasso);
  check(path->commands.size() == 5, "inactive lasso end does not close twice");

  auto inside_count = std::make_shared<int>(0);
  auto boundary_count = std::make_shared<int>(0);
  auto right_boundary_count = std::make_shared<int>(0);
  auto partial_count = std::make_shared<int>(0);
  auto outside_count = std::make_shared<int>(0);
  auto inside = make_bounds_node(2.0, 2.0, 3.0, 3.0, inside_count);
  auto boundary = make_bounds_node(-1.0, 4.0, 2.0, 2.0, boundary_count);
  auto right_boundary = make_bounds_node(9.0, 4.0, 2.0, 2.0, right_boundary_count);
  auto partial = make_bounds_node(8.0, 8.0, 5.0, 5.0, partial_count);
  auto outside = make_bounds_node(20.0, 20.0, 2.0, 2.0, outside_count);
  auto lasso_matches = selection::find_nodes_in_lasso_selection(
      readonly(lasso),
      flight::Array<flight::Ref<types::BoundsNodeAny>>{
          outside, right_boundary, boundary, inside});
  check(identities_are(lasso_matches, {right_boundary, boundary, inside}),
        "lasso includes both polygon-edge centers in caller order");

  selection::begin_lasso_selection(lasso, 1.0, 1.0);
  selection::add_lasso_selection_point(lasso, 2.0, 2.0);
  selection::end_lasso_selection(lasso);
  check(path->commands.size() == 3 && path->data.size() == 4,
        "lasso begin resets the live path before collecting points");
  auto short_matches = selection::find_nodes_in_lasso_selection(
      readonly(lasso), flight::Array<flight::Ref<types::BoundsNodeAny>>{inside});
  check(short_matches.empty(), "lasso with fewer than three points selects nothing");

  auto marquee = selection::create_marquee_selection();
  check_runtime_cells(selection::get_marquee_selection_runtime(marquee), "marquee");
  auto rectangle_view = selection::get_marquee_rectangle(readonly(marquee));
  auto rectangle = rectangle_view.shared_exact_object<types::Rectangle>();
  check(rectangle != nullptr && rectangle->x == 0.0 && rectangle->y == 0.0 &&
            rectangle->width == 0.0 && rectangle->height == 0.0,
        "marquee owns one zero rectangle");
  selection::update_marquee_selection(marquee, 7.0, 8.0);
  check(rectangle->width == 0.0 && rectangle->height == 0.0,
        "inactive marquee ignores updates");
  selection::begin_marquee_selection(marquee, 10.0, 10.0);
  selection::update_marquee_selection(marquee, 0.0, 5.0);
  check(rectangle->x == 0.0 && rectangle->y == 5.0 && rectangle->width == 10.0 &&
            rectangle->height == 5.0,
        "marquee normalizes reverse-direction drag geometry");
  auto ended_rectangle = selection::end_marquee_selection(marquee);
  check(ended_rectangle == rectangle, "marquee end returns the live rectangle identity");
  selection::update_marquee_selection(marquee, 40.0, 40.0);
  check(rectangle->x == 0.0 && rectangle->y == 5.0 && rectangle->width == 10.0 &&
            rectangle->height == 5.0,
        "ended marquee ignores further updates");

  selection::begin_marquee_selection(marquee, 0.0, 0.0);
  selection::update_marquee_selection(marquee, 10.0, 10.0);
  selection::end_marquee_selection(marquee);
  auto candidates =
      flight::Array<flight::Ref<types::BoundsNodeAny>>{outside, partial, inside};
  auto intersecting =
      selection::find_nodes_in_marquee_selection(readonly(marquee), candidates);
  auto contained = selection::find_nodes_in_marquee_selection(
      readonly(marquee), candidates, std::optional<types::MarqueeSelectionMode>{"contain"});
  check(identities_are(intersecting, {partial, inside}),
        "marquee intersection preserves candidate order");
  check(identities_are(contained, {inside}),
        "marquee containment excludes partially overlapping bounds");
  check(*inside_count == 1 && *partial_count == 1 && *outside_count == 1,
        "selection reuses bounds cached at the current local revision");
}

void test_failure_boundaries() {
  auto lasso = flight::make_ref<types::LassoSelection>();
  lasso->entity_runtime_key = std::nullopt;
  expect_bad_cast(
      [&] { (void)selection::get_lasso_selection_runtime(lasso); },
      "uninitialized lasso runtime access throws bad_cast");

  auto marquee = flight::make_ref<types::MarqueeSelection>();
  marquee->entity_runtime_key = std::nullopt;
  expect_bad_cast(
      [&] { (void)selection::get_marquee_selection_runtime(marquee); },
      "uninitialized marquee runtime access throws bad_cast");

  auto state = flight::make_ref<types::SelectionState<TestNodeRef>>();
  state->entity_runtime_key = std::nullopt;
  expect_bad_cast(
      [&] { (void)selection::get_selection_state_runtime<TestNodeRef>(state); },
      "uninitialized selection-state runtime access throws bad_cast");

  auto active_lasso = selection::create_lasso_selection();
  selection::begin_lasso_selection(active_lasso, 0.0, 0.0);
  selection::add_lasso_selection_point(active_lasso, 10.0, 0.0);
  selection::add_lasso_selection_point(active_lasso, 0.0, 10.0);
  selection::end_lasso_selection(active_lasso);
  auto malformed = flight::make_ref<types::BoundsNodeAny>();
  malformed->entity_runtime_key = std::nullopt;
  expect_bad_cast(
      [&] {
        (void)selection::find_nodes_in_lasso_selection(
            readonly(active_lasso),
            flight::Array<flight::Ref<types::BoundsNodeAny>>{malformed});
      },
      "selection against a missing bounds runtime throws bad_cast");
}

} // namespace

int main() {
  test_selection_state_and_signals();
  test_pointer_policy();
  test_lasso_and_marquee();
  test_failure_boundaries();

  if (failures != 0) {
    std::cerr << failures << " selection test(s) failed\n";
    return 1;
  }
  std::cout << "selection tests passed\n";
  return 0;
}
