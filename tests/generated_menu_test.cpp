// Focused behavior tests for @flighthq/menu.
//
// `@flighthq/menu` is `assisted` at a627849: three of four modules emit and `menu.ts` is supplied by the
// hand-written override in `overrides/include/flight/menu/menu.hpp`, whose one refused declaration
// (`_validateItem`) is the validator the emitted `validateMenuItemTemplate` calls. This file exercises
// that supply together with the emitted module around it and the emitted templates module.
//
// What is checked, against the pinned TypeScript:
//
//   * the validator's rules and their order: separator restrictions, submenu/type consistency, `checked`
//     restricted to the two toggle types, the adjacent-radio group rule evaluated before descending, and
//     cycle detection against the seen set;
//   * the template helpers fill and keep the defaults the source documents (`type: 'normal'`,
//     `enabled: true`, caller values winning, children normalized at every depth) and clone deeply;
//   * the highlight and select lifecycles are origin-pinned: attach detaches first, one entity never holds
//     two live subscriptions, detach runs that entity's own unsubscribe, and dispose clears the signal;
//   * `enableMenuSignals` is idempotent and `showContextMenu` brackets the provider call with the open and
//     close events;
//   * `destroyAppMenu` destroys each distinct provider once, and a provider whose destroy threw is
//     RETAINED so a later call retries only the failures.
//
// TWO DEFECTS FOUND BY A LATER RE-AUDIT OF THIS FILE:
//
//   1. FIVE OF THE SIX DEFAULT-MENU BUILDERS WERE NEVER CALLED BY ANY LINE. Only App and File were touched;
//      Edit, Help, View and Window were not, so their labels, roles, item ids, separators and accelerators
//      had never been checked at all. Four mutations -- misspelling each of their labels, and giving Edit the
//      File role -- SURVIVED this entire suite, which is how the gap was found.
//   2. `setAppMenu` WAS NEVER CALLED. Both of its upstream cases were uncovered: a mutation returning `true`
//      without asking the provider, and one dropping the items on the way, each survived.
// Both are now covered, and each of the seven survivors was RE-RUN against the fix and is now caught by the
// assertion that was added for it. The upstream suite has 58 cases across two fixtures; this file now
// exercises every builder and both install outcomes.
//
// One thing this file deliberately does not assert: the emitted `destroyAppMenu` never rethrows the first
// failure, so asserting the source's rethrow would fail. That is a measured emitter defect, filed as
// docs/compiler-requests/nullish-assignment-dropped-in-catch.md, and asserting around it here would
// certify the wrong behavior.
#include <flight/executor.hpp>
#include <flight/menu/menu.hpp>
#include <flight/menu/menu_templates.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

int failures = 0;

bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
  return condition;
}

using flight::String;
using Item = flight::types::MenuItemTemplate;
using ItemRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Item>>>>;
using PartialItemRow =
    flight::StructuralRef<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<Item>>>>>;

std::string text(const std::optional<String>& value) {
  return value.has_value() ? value.value().to_utf8() : std::string("<none>");
}

flight::Ref<Item> item(const char* type, const char* label = nullptr) {
  auto value = flight::make_ref<Item>();
  value->type = String(type);
  if (label != nullptr) value->label = String(label);
  return value;
}

flight::Ref<Item> container(const char* type, flight::Array<flight::Ref<Item>> children) {
  auto value = item(type);
  value->submenu = std::move(children);
  return value;
}

// A template tree walker. The default-menu builders are large nested literals, and asserting a whole literal
// is both unreadable and brittle; what matters is that named items are PRESENT in the right tree, which is
// what the upstream cases check.
void walk_ids(const flight::Ref<Item>& node, std::vector<std::string>& out) {
  if (node->id.has_value()) out.push_back(node->id.value().to_utf8());
  if (node->submenu.has_value()) {
    for (const auto& child : node->submenu.value()) walk_ids(child, out);
  }
}

bool has_id(const flight::Ref<Item>& node, const char* id) {
  std::vector<std::string> ids;
  walk_ids(node, ids);
  for (const auto& value : ids) {
    if (value == id) return true;
  }
  return false;
}

double count_type(const flight::Ref<Item>& node, const char* type) {
  double total = node->type.has_value() && node->type.value() == String(type) ? 1.0 : 0.0;
  if (node->submenu.has_value()) {
    for (const auto& child : node->submenu.value()) total += count_type(child, type);
  }
  return total;
}

bool has_accelerator(const flight::Ref<Item>& node, const char* accelerator) {
  if (node->accelerator.has_value() && node->accelerator.value() == String(accelerator)) return true;
  if (node->submenu.has_value()) {
    for (const auto& child : node->submenu.value()) {
      if (has_accelerator(child, accelerator)) return true;
    }
  }
  return false;
}

// has_accelerator answers "is this string anywhere in the tree", which is enough to prove a value is
// PRESENT but not that it is on the right item: two items may legitimately carry the same string, and
// a mutant that moves an accelerator from one to the other keeps it present. find_by_id ties the
// assertion to the named item.
flight::Ref<Item> find_by_id(const flight::Ref<Item>& node, const char* id) {
  if (node->id.has_value() && node->id.value() == String(id)) return node;
  if (node->submenu.has_value()) {
    for (const auto& child : node->submenu.value()) {
      if (auto found = find_by_id(child, id)) return found;
    }
  }
  return nullptr;
}

}  // namespace

int main() {
  // The validator's per-item rules.
  {
    check(!flight::menu::validate_menu_item_template(item("normal", "Open")).has_value(),
          "a plain item is valid");

    const auto labelled_separator = item("separator", "Oops");
    check(text(flight::menu::validate_menu_item_template(labelled_separator)) ==
              "separator item has a label: \"Oops\" (separators should not have labels)",
          "a separator with a label is refused, with the source's text");

    auto accelerated_separator = item("separator");
    accelerated_separator->accelerator = String("Ctrl+K");
    check(text(flight::menu::validate_menu_item_template(accelerated_separator)) ==
              "separator item has an accelerator: \"Ctrl+K\"",
          "a separator with an accelerator is refused");

    auto separator_with_children =
        container("separator", flight::Array<flight::Ref<Item>>{item("normal", "Child")});
    check(text(flight::menu::validate_menu_item_template(separator_with_children)) ==
              "separator item has a submenu",
          "a separator with children is refused");

    // A non-submenu item carrying children: the type defaults in the message when it is absent.
    auto normal_with_children =
        container("normal", flight::Array<flight::Ref<Item>>{item("normal", "Child")});
    check(text(flight::menu::validate_menu_item_template(normal_with_children)) ==
              "item type \"normal\" has a submenu (only type \"submenu\" should carry children)",
          "children on a non-submenu item are refused");

    auto checkable = item("normal", "Tick");
    checkable->checked = true;
    check(text(flight::menu::validate_menu_item_template(checkable)) ==
              "item type \"normal\" has \"checked\" (only \"checkbox\" and \"radio\" items are checkable)",
          "checked on a non-toggle item is refused");

    auto checkbox = item("checkbox", "Tick");
    checkbox->checked = true;
    check(!flight::menu::validate_menu_item_template(checkbox).has_value(),
          "checked on a checkbox is valid");

    // An item with no type at all: the message reports the defaulted type. (An engaged empty type is a
    // different input and must not be confused with an absent one.)
    auto untyped = flight::make_ref<Item>();
    untyped->submenu = flight::Array<flight::Ref<Item>>{item("normal")};
    check(text(flight::menu::validate_menu_item_template(untyped)) ==
              "item type \"normal\" has a submenu (only type \"submenu\" should carry children)",
          "an absent type defaults to normal in the message text");
  }

  // The adjacent-radio rule: it only sees siblings, so it runs where the child list is known.
  {
    auto first = item("radio", "One");
    first->checked = true;
    auto second = item("radio", "Two");
    second->checked = true;
    auto third = item("radio", "Three");

    auto adjacent = container("submenu", flight::Array<flight::Ref<Item>>{first, second, third});
    check(text(flight::menu::validate_menu_item_template(adjacent)) ==
              "radio group has 2 checked items (a radio group may have at most one)",
          "two checked members of one adjacent radio run are refused");

    // A non-radio item starts a new run, so two checked radios separated by a normal item are fine.
    auto separated = container(
        "submenu", flight::Array<flight::Ref<Item>>{first, item("normal", "Break"), second});
    check(!flight::menu::validate_menu_item_template(separated).has_value(),
          "a non-radio item starts a new run");

    // And the emitted rule is reachable directly.
    flight::Array<flight::Ref<Item>> children =
        flight::Array<flight::Ref<Item>>{first, second};
    check(text(flight::menu::validate_radio_groups(
              flight::SequenceView<ItemRow>(children))).find("radio group has 2 checked items") == 0,
          "validateRadioGroups is the rule the validator runs before descending");
  }

  // A valid nested tree validates, and a cycle is detected rather than recursing forever.
  {
    auto leaf = item("normal", "Leaf");
    auto branch = container("submenu", flight::Array<flight::Ref<Item>>{leaf});
    auto root = container("submenu", flight::Array<flight::Ref<Item>>{branch, item("separator")});
    check(!flight::menu::validate_menu_item_template(root).has_value(),
          "a valid nested tree validates through every level");

    auto cyclic = item("submenu");
    cyclic->submenu = flight::Array<flight::Ref<Item>>{cyclic};
    bool threw = false;
    std::string message;
    try {
      static_cast<void>(flight::menu::validate_menu_item_template(cyclic));
    } catch (const std::exception& error) {
      threw = true;
      message = error.what();
    }
    check(threw, "a submenu that contains its own ancestor is a cycle");
    check(message == "validateMenuItemTemplate: cyclic submenu reference detected",
          "and it reports the source's cycle message");
  }

  // createMenuItemTemplate fills and keeps the documented defaults, at every depth.
  {
    auto created = flight::menu::create_menu_item_template();
    check(created->type.has_value() && created->type.value() == String("normal"),
          "a template with no argument gets type 'normal'");
    check(created->enabled.has_value() && created->enabled.value(),
          "and enabled true");

    // The argument case is NOT asserted: the emitted lowering never reads the partial row, so a
    // caller's label, type and submenu are all dropped and the source's recursive default-fill never
    // runs. That is a measured emitter defect -- `createMenuItemTemplate({ label: 'Save' })` returns an
    // item with no label -- filed as
    // docs/compiler-requests/object-spread-of-a-parameter-dropped.md. Asserting the source's behavior
    // here would fail, and asserting the lowered behavior would certify it.
  }

  // cloneMenuTemplate is a deep clone: same shape and values, no shared children.
  {
    auto leaf = item("normal", "Leaf");
    auto original = container("submenu", flight::Array<flight::Ref<Item>>{leaf});
    const auto clone = flight::menu::clone_menu_template(original);
    check(clone != original, "the clone is a different object");
    check(clone->type.has_value() && clone->type.value() == String("submenu"), "with the same type");
    check(clone->submenu.has_value() && static_cast<double>(clone->submenu->size()) == 1.0,
          "and the same children");
    if (clone->submenu.has_value() && !clone->submenu->empty()) {
      check(clone->submenu.value().element(0.0) != leaf, "whose objects are clones, not shares");
      check(clone->submenu.value().element(0.0)->label.has_value() &&
                clone->submenu.value().element(0.0)->label.value() == String("Leaf"),
            "carrying the same values");
    }
  }

  // Highlight and select: origin-pinned subscriptions, one live subscription per entity.
  {
    int subscribe_calls = 0;
    int unsubscribe_calls = 0;
    std::function<void(String)> listener;
    auto host = flight::make_ref<flight::types::HostMenuHighlightCapability>();
    host->subscribe = [&](std::function<void(String)> next) {
      ++subscribe_calls;
      listener = std::move(next);
      return std::function<void()>([&unsubscribe_calls]() { ++unsubscribe_calls; });
    };

    const auto highlight = flight::menu::create_menu_highlight();
    int observed = 0;
    flight::String seen;
    flight::signals::connect_signal<std::function<void(String)>>(
        highlight->on_menu_item_highlight, [&](String id) {
          ++observed;
          seen = id;
        });

    flight::menu::attach_menu_highlight(host, highlight);
    check(subscribe_calls == 1, "attach subscribes once");
    listener(String("recent"));
    check(observed == 1 && seen == String("recent"), "provider notifications reach the signal");

    flight::menu::attach_menu_highlight(host, highlight);
    check(subscribe_calls == 2 && unsubscribe_calls == 1,
          "reattaching releases the prior origin before subscribing again");

    flight::menu::detach_menu_highlight(highlight);
    check(unsubscribe_calls == 2, "detach runs this entity's own unsubscribe");
    flight::menu::detach_menu_highlight(highlight);
    check(unsubscribe_calls == 2, "and a second detach has nothing left to release");

    flight::menu::dispose_menu_highlight(highlight);
    check(!highlight->on_menu_item_highlight->data.has_value(), "dispose clears the signal");

    int select_unsubscribes = 0;
    auto select_host = flight::make_ref<flight::types::HostMenuSelectCapability>();
    select_host->subscribe = [&select_unsubscribes](std::function<void(String)>) {
      return std::function<void()>([&select_unsubscribes]() { ++select_unsubscribes; });
    };
    const auto select_value = flight::menu::create_menu_select();
    flight::menu::attach_menu_select(select_host, select_value);
    flight::menu::detach_menu_select(select_value);
    check(select_unsubscribes == 1, "the select lifecycle runs its own unsubscribe");
    flight::menu::dispose_menu_select(select_value);
    check(!select_value->on_menu_item_select->data.has_value(), "and dispose clears its signal");
  }

  // Signals: enabling is idempotent, and showing a context menu brackets the provider call.
  {
    const auto first = flight::menu::enable_menu_signals();
    const auto second = flight::menu::enable_menu_signals();
    check(first == second, "enableMenuSignals creates its group once");
    const auto readable = flight::menu::get_menu_signals();
    check(readable.has_value(), "getMenuSignals reports the enabled group");

    int opens = 0;
    int closes = 0;
    flight::signals::connect_signal<std::function<void()>>(first->on_context_menu_open,
                                                           [&opens]() { ++opens; });
    flight::signals::connect_signal<std::function<void()>>(first->on_context_menu_close,
                                                           [&closes]() { ++closes; });

    flight::Array<flight::Ref<Item>> items = flight::Array<flight::Ref<Item>>{item("normal", "Copy")};
    bool popped = false;
    auto popup_host = flight::make_ref<flight::types::HostMenuPopupCapability>();
    popup_host->popup = [&](flight::Array<flight::Ref<Item>> received, double x, double y) {
      popped = received.size() == 1.0 && x == 4.0 && y == 5.0;
      return flight::Task<std::optional<String>>::resolve(std::optional<String>{String("copy")});
    };
    const auto answer = flight::menu::show_context_menu(popup_host, items, 4.0, 5.0).get();
    check(popped, "the provider receives the items and coordinates");
    check(answer.has_value() && answer.value() == String("copy"), "and its answer comes back");
    check(opens == 1, "the open event fires once for the call");
    // The close event is attached to the provider's settlement, so it runs on the executor after the
    // promise settles rather than inline with the return.
    flight::default_queue_executor()->run_until_idle();
    check(closes == 1, "and the close event fires once the promise settles");
  }

  // destroyAppMenu: distinct providers once each, and a failed provider is retained for a retry.
  {
    using Host = flight::Ref<flight::types::HostAppMenuCapability>;
    using HostRow =
        flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::HostAppMenuCapability>>>>;

    bool failing = true;
    int attempts = 0;
    int destroyed = 0;
    auto flaky = flight::make_ref<flight::types::HostAppMenuCapability>();
    flaky->destroy = [&failing, &attempts, &destroyed]() {
      ++attempts;
      if (failing) throw std::runtime_error("provider failed");
      ++destroyed;
    };
    flaky->set_app_menu = [](flight::Array<flight::Ref<Item>>) { return true; };

    auto healthy = flight::make_ref<flight::types::HostAppMenuCapability>();
    healthy->destroy = [&destroyed]() { ++destroyed; };
    healthy->set_app_menu = [](flight::Array<flight::Ref<Item>>) { return true; };

    flight::Array<HostRow> providers = flight::Array<HostRow>{};
    providers.push(HostRow(flaky));
    providers.push(HostRow(healthy));

    // CORRECTED SEMANTICS, and the correction matters. The upstream source records the FIRST failure in
    // the catch, keeps that provider OUT of the destroyed set so it is retried, and -- after attempting
    // EVERY provider -- rethrows the recorded failure. The emission this suite was written against
    // emitted a no-op catch and a presence test that was never true, so it swallowed the failure
    // entirely, and the assertions here used to certify that: they called destroy_app_menu and expected
    // no throw. A regeneration now emits the catch binding faithfully (failure ??= error; ... throw
    // failure), which is what the source always said. So the failure is expected here, and what is worth
    // asserting is that it does not stop the loop.
    bool threw = false;
    try {
      flight::menu::destroy_app_menu(providers);
    } catch (const std::runtime_error&) {
      threw = true;
    }
    check(threw, "a provider failure is rethrown once every provider has been attempted");
    check(attempts == 1 && destroyed == 1,
          "the failure did not stop the loop: the healthy provider was still destroyed");

    // The failed provider was retained, so a later call retries only the failure -- and fails again.
    threw = false;
    try {
      flight::menu::destroy_app_menu(providers);
    } catch (const std::runtime_error&) {
      threw = true;
    }
    check(threw && attempts == 2 && destroyed == 1,
          "a retained failure is attempted again, not skipped");

    failing = false;
    flight::menu::destroy_app_menu(providers);
    check(attempts == 3 && destroyed == 2, "and it is destroyed on the retry, which no longer throws");

    flight::menu::destroy_app_menu(providers);
    check(attempts == 3 && destroyed == 2, "a provider that succeeded is never destroyed twice");
  }

  // The templates module builds the documented default menus.
  {
    const auto app_menu = flight::menu::create_default_app_menu_template(String("Flight"));
    check(app_menu->type.has_value() && app_menu->type.value() == String("submenu"),
          "the app menu is a submenu");
    check(app_menu->role.has_value() && app_menu->role.value() == String("appMenu"),
          "with the app-menu role");
    check(app_menu->submenu.has_value() && static_cast<double>(app_menu->submenu->size()) > 0.0,
          "and children");
    if (app_menu->submenu.has_value() && static_cast<double>(app_menu->submenu->size()) > 0.0) {
      const auto about = app_menu->submenu.value().element(0.0);
      check(about->id.has_value() && about->id.value() == String("about"), "the first is About");
      check(about->label.has_value() && about->label.value() == String("About Flight"),
            "labelled with the application name");
      check(about->role.has_value() && about->role.value() == String("about"), "with the about role");
    }
    const auto separator = app_menu->submenu.value().element(1.0);
    check(separator->type.has_value() && separator->type.value() == String("separator"),
          "and the second is a separator");
    check(!flight::menu::validate_menu_item_template(ItemRow(app_menu)).has_value(),
          "the default app menu passes the package's own validator");

    const auto file_menu = flight::menu::create_default_file_menu_template();
    check(file_menu->role.has_value() && file_menu->role.value() == String("fileMenu"),
          "the file menu carries the file-menu role");
    check(!flight::menu::validate_menu_item_template(ItemRow(file_menu)).has_value(),
          "and the file menu validates too");
  }

  // THE FIVE DEFAULT-MENU BUILDERS THIS SUITE NEVER CALLED (and a fuller look at the two it did). Four
  // mutations -- misspelling the Edit, Window, Help and View labels, and giving Edit the File role --
  // SURVIVED this whole suite before this block existed, which is how the gap was found.
  {
    const auto edit = flight::menu::create_default_edit_menu_template();
    check(text(edit->label) == "Edit" && text(edit->role) == "editMenu" && text(edit->type) == "submenu",
          "the Edit template is the Edit submenu");
    check(has_id(edit, "undo") && has_id(edit, "redo") && has_id(edit, "cut") && has_id(edit, "copy") &&
              has_id(edit, "paste") && has_id(edit, "delete") && has_id(edit, "selectAll"),
          "with undo, redo, cut, copy, paste, delete and selectAll");
    check(count_type(edit, "separator") >= 1.0, "and a separator between its groups");

    const auto file = flight::menu::create_default_file_menu_template();
    check(text(file->label) == "File" && text(file->role) == "fileMenu",
          "the File template is the File submenu");
    check(has_id(file, "newFile") && has_id(file, "openFile") && has_id(file, "saveFile") &&
              has_id(file, "saveFileAs") && has_id(file, "closeFile"),
          "with newFile, openFile, saveFile, saveFileAs and closeFile");
    check(has_accelerator(file, "CmdOrCtrl+N") && has_accelerator(file, "CmdOrCtrl+S"),
          "and New and Save carry accelerators");
    check(count_type(file, "separator") >= 2.0, "with separators between its groups");

    const auto help = flight::menu::create_default_help_menu_template();
    check(text(help->label) == "Help" && text(help->role) == "helpMenu",
          "the Help template is the Help submenu");
    check(has_id(help, "help"), "carrying its help item");

    const auto view = flight::menu::create_default_view_menu_template();
    check(text(view->label) == "View" && text(view->role) == "viewMenu",
          "the View template is the View submenu");
    check(has_id(view, "reload") && has_id(view, "zoomIn") && has_id(view, "zoomOut") &&
              has_id(view, "resetZoom") && has_id(view, "toggleFullscreen"),
          "with reload, zoomIn, zoomOut, resetZoom and toggleFullscreen");

    const auto window = flight::menu::create_default_window_menu_template();
    check(text(window->label) == "Window" && text(window->role) == "windowMenu",
          "the Window template is the Window submenu");
    check(has_id(window, "minimize") && has_id(window, "close"), "with minimize and close");

    // Every builder's tree validates. The upstream suite validates only the App and File menus; nothing had
    // ever checked that the other four are well-formed at all.
    check(!flight::menu::validate_menu_item_template(ItemRow(edit)).has_value() &&
              !flight::menu::validate_menu_item_template(ItemRow(help)).has_value() &&
              !flight::menu::validate_menu_item_template(ItemRow(view)).has_value() &&
              !flight::menu::validate_menu_item_template(ItemRow(window)).has_value(),
          "and the Edit, Help, View and Window trees are all well-formed");
  }

  // setAppMenu: the provider's own answer is the answer, and every item reaches it in order.
  {
    std::vector<std::string> delivered;
    bool answer = true;
    auto host = flight::make_ref<flight::types::HostAppMenuCapability>();
    host->set_app_menu = [&delivered, &answer](flight::Array<flight::Ref<Item>> items) {
      for (const auto& entry : items) {
        delivered.push_back(entry->id.has_value() ? entry->id.value().to_utf8() : std::string("<none>"));
      }
      return answer;
    };

    flight::Array<flight::Ref<Item>> items = flight::Array<flight::Ref<Item>>{};
    auto alpha = item("normal", "Alpha");
    alpha->id = String("alpha");
    auto beta = item("normal", "Beta");
    beta->id = String("beta");
    items.push(alpha);
    items.push(beta);

    check(flight::menu::set_app_menu(host, items), "setAppMenu reports the provider's own answer");
    check(delivered.size() == 2 && delivered[0] == "alpha" && delivered[1] == "beta",
          "and delivers every item to the provider, in order");

    answer = false;
    check(!flight::menu::set_app_menu(host, items), "a refused install is reported as false");
  }

  // ACCELERATOR AND LABEL VALUES. Upstream's own menu-templates.test.ts asserts only that an
  // accelerator is DEFINED (`expect(quit?.accelerator).toBeDefined()`), never which one it is, so
  // neither that fixture nor this suite could tell a wrong accelerator from the right one. Five
  // mutations survived the whole suite on that account -- the quit accelerator, Redo losing its
  // Shift, Save As losing its Shift, toggleFullscreen, and zoomIn -- plus the Help item's label.
  // The strings below are the ones the pinned source defines, read from
  // packages/menu/src/menu-templates.ts, not from the emitted header.
  {
    const auto app = flight::menu::create_default_app_menu_template(String("Flight"));
    const auto quit = find_by_id(app, "quit");
    const auto hide = find_by_id(app, "hide");
    check(quit && text(quit->accelerator) == "CmdOrCtrl+Q",
          "the app menu's Quit carries the accelerator the source defines");
    check(hide && text(hide->accelerator) == "CmdOrCtrl+H",
          "and Hide carries its own, so moving one to the other is caught");
    check(quit && text(quit->label) == "Quit Flight" && hide && text(hide->label) == "Hide Flight",
          "and both interpolate the application name into their labels");

    const auto edit = flight::menu::create_default_edit_menu_template();
    const auto redo = find_by_id(edit, "redo");
    check(redo && text(redo->accelerator) == "Shift+CmdOrCtrl+Z",
          "the Edit menu's Redo keeps the Shift its accelerator requires");

    const auto file = flight::menu::create_default_file_menu_template();
    const auto save_as = find_by_id(file, "saveFileAs");
    check(save_as && text(save_as->accelerator) == "Shift+CmdOrCtrl+S",
          "and Save As stays Shift-distinguished from Save");

    const auto view = flight::menu::create_default_view_menu_template();
    const auto fullscreen = find_by_id(view, "toggleFullscreen");
    const auto zoom_in = find_by_id(view, "zoomIn");
    check(fullscreen && text(fullscreen->accelerator) == "F11",
          "Toggle Full Screen uses the function key the source names");
    check(zoom_in && text(zoom_in->accelerator) == "CmdOrCtrl+Plus",
          "and Zoom In is Plus, not the '=' key");

    const auto help = flight::menu::create_default_help_menu_template();
    const auto help_item = find_by_id(help, "help");
    check(help_item && text(help_item->label) == "Search",
          "the Help menu's item is labelled Search");
  }

  // A SHARED SUBMENU OBJECT IS A DAG, NOT A CYCLE. The validator keeps a `seen` set and unwinds it on
  // the way back out, so the SAME template object may appear twice as siblings -- it is not its own
  // ancestor. Dropping that unwind left the whole suite passing while making the second visit throw
  // "cyclic submenu reference detected": a legal template falsely rejected. The existing cycle case
  // (a submenu containing its own ancestor) still throws and is asserted above.
  {
    auto shared = container("submenu", flight::Array<flight::Ref<Item>>{item("normal", "Leaf")});
    shared->id = String("shared");
    auto root = container("submenu", flight::Array<flight::Ref<Item>>{shared, shared});
    root->id = String("root");
    bool threw = false;
    std::optional<String> error;
    try {
      error = flight::menu::validate_menu_item_template(ItemRow(root));
    } catch (...) {
      threw = true;
    }
    check(!threw, "a submenu object shared by two siblings does not throw");
    check(!error.has_value(), "and it validates as a legal DAG rather than a cycle");
  }

  if (failures == 0) std::cout << "generated menu: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
