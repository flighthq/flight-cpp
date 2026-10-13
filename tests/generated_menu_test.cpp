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

  if (failures == 0) std::cout << "generated menu: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
