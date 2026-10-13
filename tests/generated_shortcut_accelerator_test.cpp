// Native parity test for @flighthq/shortcut, ported from its own shortcut.test.ts (13 cases).
//
// This is the hardware-key chord package: an accelerator string in, a parsed chord, a canonical spelling or a display
// form out. Nothing here talks to a host -- every function is pure over its arguments except for the caller-owned
// `out` objects, which is why this file can pin the whole alphabet of the contract:
//
//   * Aliases and identity. Ctrl/Control, Cmd/Command/Meta, Alt/Option, Win/Super and Shift are spellings of one
//     modifier each, and keys are spelled by their canonical names (Enter -> Return, Up -> ArrowUp, PgDn -> PageDown).
//   * Order. A chord carries ONE normalized modifier order, whatever order the input used, and CommandOrControl has
//     its own ordinal so a chord mixing it with Control still has a single, input-independent spelling.
//   * The separator rule that makes zoom registerable. A '+' or '-' separates only when it TERMINATES a token, so
//     'CommandOrControl++' and 'CommandOrControl+-' parse as the Plus and Minus keys rather than as a chord with a
//     missing key -- and a lone '+' or '-' is a valid one-key accelerator.
//   * The out-object contract. `out` is cleared and refilled IN PLACE and returned as the very object passed in, so a
//     caller's retained array is never swapped underneath them; an unparseable input returns null and leaves `out`
//     exactly as it was, because the parse fails before the clear.
//   * Display. macOS joins modifier symbols and the key with no separator, Windows and Linux use text labels and '+',
//     and CommandOrControl resolves before the label is chosen; an unparseable accelerator displays as the empty
//     string rather than throwing.
//   * Failure. Every rejection reason is a distinct arm with the token that caused it, and an invalid accelerator is
//     reported as invalid rather than silently normalized.
//
// The rest of the package is not this file. The global-shortcut lifecycle in shortcutExplicitDependency is supplied by
// a hand-written override and is covered by the pre-existing tests/generated_shortcut_test.cpp, which this unit left in
// place and extended by the one upstream case it recorded as unported; this file is the emitted accelerator module's
// own surface, which had no local coverage before it.
#include <flight/shortcut/shortcut.hpp>

#include <iostream>
#include <string>
#include <variant>
#include <vector>

namespace {

namespace shortcut = flight::shortcut;
namespace types = flight::types;

using Accelerators = flight::Array<flight::String>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

std::string utf8(const flight::String& value) { return value.to_utf8(); }

void check_string(const std::string& actual, const std::string& expected, const std::string& message) {
  if (actual == expected) return;
  std::cerr << "FAIL: " << message << " (got '" << actual << "', want '" << expected << "')\n";
  ++failures;
}

// normalizeAccelerator's contract is "null when unparseable"; the tests below read it through the optional so a
// missing value fails loudly rather than comparing an empty default.
void check_normalized(const char* input, const char* expected) {
  const auto normalized = shortcut::normalize_accelerator(flight::String(input));
  if (!normalized.has_value()) {
    std::cerr << "FAIL: '" << input << "' did not parse at all (want '" << expected << "')\n";
    ++failures;
    return;
  }
  check_string(utf8(normalized.value()), expected, std::string("normalize '") + input + "'");
}

std::string display(const char* accelerator, const char* platform) {
  return utf8(shortcut::format_accelerator_for_display(flight::String(accelerator), flight::String(platform)));
}

Accelerators candidates_of(const std::vector<const char*>& values) {
  Accelerators out;
  for (const char* value : values) out.push(flight::String(value));
  return out;
}

// --- Aliases, canonical keys and ordering ---------------------------------------------------------------------

void test_modifier_aliases_resolve_to_one_canonical_spelling() {
  check_normalized("ctrl+k", "Control+K");
  check_normalized("Control+K", "Control+K");
  check_normalized("cmd+k", "Meta+K");
  check_normalized("command+k", "Meta+K");
  check_normalized("meta+k", "Meta+K");
  check_normalized("option+k", "Alt+K");
  check_normalized("alt+k", "Alt+K");
  check_normalized("win+k", "Super+K");
  check_normalized("super+k", "Super+K");
  check_normalized("shift+k", "Shift+K");
  check_normalized("commandorcontrol+k", "CommandOrControl+K");
}

void test_keys_resolve_to_their_canonical_names() {
  check_normalized("escape", "Escape");
  check_normalized("esc", "Escape");
  check_normalized("enter", "Return");
  check_normalized("return", "Return");
  check_normalized("up", "ArrowUp");
  check_normalized("pgdn", "PageDown");
  check_normalized("ins", "Insert");
  check_normalized("del", "Delete");
  check_normalized("spacebar", "Space");
  check_normalized("numadd", "NumpadAdd");
  check_normalized("num5", "Numpad5");
  check_normalized("a", "A");
  check_normalized("9", "9");
  check_normalized("f24", "F24");
}

void test_case_is_ignored_on_both_modifiers_and_keys() {
  check_normalized("CTRL+SHIFT+K", "Control+Shift+K");
  check_normalized("cTrL+sHiFt+escape", "Control+Shift+Escape");
}

void test_the_normalized_modifier_order_does_not_depend_on_the_input_order() {
  check_normalized("shift+ctrl+k", "Control+Shift+K");
  check_normalized("ctrl+shift+k", "Control+Shift+K");
  // The key need not be last in the input: only a SECOND non-modifier token is an error.
  check_normalized("k+shift+ctrl", "Control+Shift+K");
  check_normalized("alt+ctrl+shift+meta+k", "Control+Alt+Shift+Meta+K");
  // CommandOrControl carries its own ordinal, so it sorts after every concrete modifier rather than tying with the
  // Control it resolves to on a non-Apple platform.
  check_normalized("commandorcontrol+super+meta+shift+alt+ctrl+k", "Control+Alt+Shift+Meta+Super+CommandOrControl+K");
}

void test_a_bare_key_is_a_valid_chord() {
  check_normalized("k", "K");
  check_normalized("ctrl+k", "Control+K");
  check(shortcut::is_accelerator_valid(flight::String("K")), "a single key is a parseable accelerator");
}

// --- The separator rule ----------------------------------------------------------------------------------------

void test_a_separator_only_separates_when_it_terminates_a_token() {
  // '-' as a separator, exactly as it always behaved.
  check_normalized("Ctrl-Shift-K", "Control+Shift+K");
  // ...and the two zoom chords, where the trailing separator IS the key.
  check_normalized("CommandOrControl++", "CommandOrControl+Plus");
  check_normalized("CommandOrControl+-", "CommandOrControl+Minus");
  // A lone separator is a one-key accelerator rather than an empty token.
  check_normalized("+", "Plus");
  check_normalized("-", "Minus");
  check_normalized("ctrl+plus", "Control+Plus");
  check_normalized("ctrl+minus", "Control+Minus");
}

// --- The out-object contract ------------------------------------------------------------------------------------

void test_parse_accelerator_fills_the_out_object_and_returns_it() {
  auto out = shortcut::make_parsed_accelerator();
  check(utf8(out->key) == "" && out->modifiers.size() == 0.0,
        "makeParsedAccelerator allocates a zeroed parsed value");

  const auto result = shortcut::parse_accelerator(flight::String("ctrl+shift+k"), out);
  check(result.has_value(), "'ctrl+shift+k' parses");
  check(result.value().get() == out.get(), "and the out object itself is returned, not a copy");
  check_string(utf8(out->key), "K", "with the canonical key written into it");
  check(out->modifiers.size() == 2.0 && out->modifiers.element(0.0) == flight::String("Control") &&
            out->modifiers.element(1.0) == flight::String("Shift"),
        "and the canonical modifier list in order");
}

void test_an_unparseable_input_leaves_the_out_object_untouched() {
  auto out = shortcut::make_parsed_accelerator();
  shortcut::parse_accelerator(flight::String("ctrl+shift+k"), out);

  const auto second = shortcut::parse_accelerator(flight::String("ctrl+notakey"), out);
  check(!second.has_value(), "an unknown key does not parse");
  check_string(utf8(out->key), "K", "and the previous parse is still on the out object");
  check(out->modifiers.size() == 2.0, "including its modifier list, because the parse failed before the clear");
}

void test_parsing_again_into_one_out_object_does_not_accumulate() {
  auto out = shortcut::make_parsed_accelerator();
  shortcut::parse_accelerator(flight::String("ctrl+shift+k"), out);
  shortcut::parse_accelerator(flight::String("alt+p"), out);
  check(out->modifiers.size() == 1.0 && out->modifiers.element(0.0) == flight::String("Alt"),
        "the second parse clears the first result rather than appending to it");
  check_string(utf8(out->key), "P", "and replaces the key");
}

void test_get_accelerator_modifiers_fills_and_returns_the_same_array() {
  Accelerators out;
  out.push(flight::String("stale"));
  const auto result = shortcut::get_accelerator_modifiers(flight::String("ctrl+alt+x"), out);
  check(result.has_value(), "'ctrl+alt+x' parses");
  check(result.value().identity() == out.identity(), "the caller's array is returned itself, not a replacement");
  check(out.size() == 2.0 && out.element(0.0) == flight::String("Control") && out.element(1.0) == flight::String("Alt"),
        "and it was cleared and refilled in place");

  const auto failed = shortcut::get_accelerator_modifiers(flight::String("ctrl+nokey"), out);
  check(!failed.has_value(), "an unparseable accelerator returns null");
  check(out.size() == 2.0, "and leaves the array as the last successful call left it");
}

void test_get_accelerator_key_reports_the_canonical_key() {
  const auto key = shortcut::get_accelerator_key(flight::String("ctrl+enter"));
  check(key.has_value() && utf8(key.value()) == "Return", "the key is reported canonically, alias resolved");
  check(!shortcut::get_accelerator_key(flight::String("ctrl+nokey")).has_value(),
        "and an unparseable accelerator reports no key");
}

// --- Display --------------------------------------------------------------------------------------------------

void test_display_uses_symbols_and_no_separator_on_macos() {
  check_string(display("ctrl+shift+k", "macos"), "⌃⇧K", "macOS joins modifier symbols and the key with nothing");
  check_string(display("ctrl+shift+k", "windows"), "Ctrl+Shift+K", "Windows uses labels and '+'");
  check_string(display("ctrl+shift+k", "linux"), "Ctrl+Shift+K", "and so does Linux");
  check_string(display("alt+k", "macos"), "⌥K", "Alt is the option symbol on macOS");
  check_string(display("meta+k", "macos"), "⌘K", "Meta is the command symbol on macOS");
  check_string(display("meta+k", "windows"), "Win+K", "and the Windows key elsewhere");
  check_string(display("k", "macos"), "K", "a bare key displays as itself");
}

void test_command_or_control_resolves_before_the_label_is_chosen() {
  check_string(display("commandorcontrol+k", "macos"), "⌘K", "CommandOrControl is Meta on macOS");
  check_string(display("commandorcontrol+k", "windows"), "Ctrl+K", "and Control everywhere else");
  check_string(display("commandorcontrol+shift+z", "macos"), "⇧⌘Z", "with the modifiers in canonical order");
  check_string(utf8(shortcut::resolve_command_or_control_modifier(flight::String("macos"))), "Meta",
               "the resolver answers Meta for macOS");
  check_string(utf8(shortcut::resolve_command_or_control_modifier(flight::String("linux"))), "Control",
               "and Control for anything else");
}

void test_display_uses_the_registered_key_labels() {
  check_string(display("arrowup", "windows"), "↑", "an arrow key displays as its symbol");
  check_string(display("enter", "windows"), "↵", "Return displays as the return symbol");
  check_string(display("esc", "macos"), "Esc", "and a key with a text label keeps it");
  check_string(display("delete", "windows"), "⌦", "Delete displays as its symbol");
  check_string(utf8(shortcut::get_accelerator_key_label(flight::String("K"))), "K",
               "a key with no registered label is displayed as itself");
}

void test_an_unparseable_accelerator_displays_as_empty() {
  check_string(display("ctrl+notakey", "macos"), "", "an unparseable accelerator formats to the empty string");
  check_string(display("", "macos"), "", "and so does an empty one");
}

void test_modifier_labels_are_platform_specific_and_empty_when_unknown() {
  check_string(utf8(shortcut::get_accelerator_modifier_label(flight::String("Shift"), flight::String("macos"))), "⇧",
               "Shift is the shift symbol on macOS");
  check_string(utf8(shortcut::get_accelerator_modifier_label(flight::String("Shift"), flight::String("windows"))),
               "Shift", "and the word elsewhere");
  check_string(utf8(shortcut::get_accelerator_modifier_label(flight::String("CommandOrControl"),
                                                             flight::String("windows"))),
               "Ctrl", "CommandOrControl is resolved before its label is chosen");
  check_string(utf8(shortcut::get_accelerator_modifier_label(flight::String("Nonsense"), flight::String("macos"))), "",
               "an unrecognized modifier has no label");
}

// --- Identity, conflict and failure ---------------------------------------------------------------------------

void test_equals_accelerator_compares_normalized_forms() {
  check(shortcut::equals_accelerator(flight::String("ctrl+k"), flight::String("Control+K")),
        "two spellings of one chord are equal");
  check(shortcut::equals_accelerator(flight::String("CTRL+SHIFT+K"), flight::String("shift+ctrl+k")),
        "regardless of case or modifier order");
  check(shortcut::equals_accelerator(flight::String("CommandOrControl+K"), flight::String("commandorcontrol+k")),
        "and an unresolved CommandOrControl still matches itself");
  check(!shortcut::equals_accelerator(flight::String("ctrl+k"), flight::String("cmd+k")),
        "Control and Command are different chords");
  check(!shortcut::equals_accelerator(flight::String("ctrl+k"), flight::String("nonsense")),
        "an unparseable side is never equal to anything");
}

void test_find_accelerator_conflict_answers_with_the_candidate_spelling() {
  const auto with_aliases = candidates_of({"Cmd+N", "ctrl+shift+p", "ctrl+o"});
  const auto hit = shortcut::find_accelerator_conflict(flight::String("Control+Shift+P"), with_aliases);
  check(hit.has_value(), "a chord already in the list is found through any spelling");
  check_string(utf8(hit.value()), "ctrl+shift+p", "and reported in the spelling the list used, not the query's");

  check(!shortcut::find_accelerator_conflict(flight::String("ctrl+y"), with_aliases).has_value(),
        "a chord absent from the list conflicts with nothing");
  check(!shortcut::find_accelerator_conflict(flight::String("ctrl+nonsense"), with_aliases).has_value(),
        "and an unparseable accelerator cannot conflict with anything");

  const auto with_unparseable = candidates_of({"nonsense", "ctrl+k"});
  check_string(utf8(shortcut::find_accelerator_conflict(flight::String("ctrl+k"), with_unparseable).value()), "ctrl+k",
               "an unparseable candidate is skipped rather than matched");
}

void test_parse_detailed_reports_why_a_chord_was_rejected() {
  auto out = shortcut::make_parsed_accelerator();

  const auto empty = shortcut::parse_accelerator_detailed(flight::String("   "), out);
  check(empty.index() == 1, "an empty accelerator is a parse error");
  check_string(utf8(std::get<1>(empty)->reason), "empty", "reported as empty");

  const auto missing = shortcut::parse_accelerator_detailed(flight::String("ctrl+shift"), out);
  check(missing.index() == 1 && utf8(std::get<1>(missing)->reason) == "missing-key",
        "a chord of only modifiers is missing its key");

  const auto duplicate = shortcut::parse_accelerator_detailed(flight::String("ctrl+ctrl+k"), out);
  check(duplicate.index() == 1 && utf8(std::get<1>(duplicate)->reason) == "duplicate-modifier",
        "a repeated modifier is rejected rather than deduplicated");
  check(duplicate.index() == 1 && utf8(std::get<1>(duplicate)->token) == "ctrl",
        "and the token that repeated is named");

  const auto unknown_key = shortcut::parse_accelerator_detailed(flight::String("ctrl+notakey"), out);
  check(unknown_key.index() == 1 && utf8(std::get<1>(unknown_key)->reason) == "unknown-key",
        "an unrecognized key is its own reason");
  check(unknown_key.index() == 1 && utf8(std::get<1>(unknown_key)->token) == "notakey",
        "naming the token that was not a key");

  // Two non-modifier tokens: the first became the key, so the second is an unknown MODIFIER, and the error names the
  // token already taken as the key rather than the one just read.
  const auto unknown_modifier = shortcut::parse_accelerator_detailed(flight::String("ctrl+k+j"), out);
  check(unknown_modifier.index() == 1 && utf8(std::get<1>(unknown_modifier)->reason) == "unknown-modifier",
        "a second non-modifier token is an unknown modifier, not a second key");
  check_string(utf8(std::get<1>(unknown_modifier)->token), "k", "and the token named is the one already taken as key");

  const auto ok = shortcut::parse_accelerator_detailed(flight::String("ctrl+k"), out);
  check(ok.index() == 0 && utf8(std::get<0>(ok)->key) == "K", "and a valid chord reports the parsed value instead");
}

void test_is_accelerator_valid_accepts_chords_and_rejects_malformed_input() {
  check(shortcut::is_accelerator_valid(flight::String("ctrl+shift+k")), "a well-formed chord is valid");
  check(shortcut::is_accelerator_valid(flight::String("k")), "a bare key is valid");
  check(shortcut::is_accelerator_valid(flight::String("CommandOrControl+-")), "the zoom-out chord is valid");
  check(!shortcut::is_accelerator_valid(flight::String("")), "an empty accelerator is not");
  check(!shortcut::is_accelerator_valid(flight::String("ctrl+")), "a dangling separator is not");
  check(!shortcut::is_accelerator_valid(flight::String("ctrl+shift")), "a chord with no key is not");
  check(!shortcut::is_accelerator_valid(flight::String("ctrl+ctrl+k")), "a duplicated modifier is not");
}

} // namespace

int main() {
  test_modifier_aliases_resolve_to_one_canonical_spelling();
  test_keys_resolve_to_their_canonical_names();
  test_case_is_ignored_on_both_modifiers_and_keys();
  test_the_normalized_modifier_order_does_not_depend_on_the_input_order();
  test_a_bare_key_is_a_valid_chord();

  test_a_separator_only_separates_when_it_terminates_a_token();

  test_parse_accelerator_fills_the_out_object_and_returns_it();
  test_an_unparseable_input_leaves_the_out_object_untouched();
  test_parsing_again_into_one_out_object_does_not_accumulate();
  test_get_accelerator_modifiers_fills_and_returns_the_same_array();
  test_get_accelerator_key_reports_the_canonical_key();

  test_display_uses_symbols_and_no_separator_on_macos();
  test_command_or_control_resolves_before_the_label_is_chosen();
  test_display_uses_the_registered_key_labels();
  test_an_unparseable_accelerator_displays_as_empty();
  test_modifier_labels_are_platform_specific_and_empty_when_unknown();

  test_equals_accelerator_compares_normalized_forms();
  test_find_accelerator_conflict_answers_with_the_candidate_spelling();
  test_parse_detailed_reports_why_a_chord_was_rejected();
  test_is_accelerator_valid_accepts_chords_and_rejects_malformed_input();

  if (failures == 0) {
    std::cout << "shortcut parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
