#include <flight/regexp.hpp>

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
int checks = 0;
void check(bool value, const char* message) {
  ++checks;
  if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
}

int main() {
  using flight::RegExp;
  using flight::String;
  const auto captures = RegExp("(a)?(b*)").exec("");
  check(captures && captures->has_capture(0) && !captures->has_capture(1) &&
        captures->has_capture(2), "unmatched and participating empty groups differ");
  check(!captures->capture(1) && captures->capture(2) == String(), "capture preserves empty value");
  for (double index : {-1.0, 0.5, 3.0, std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}) {
    check(!captures->capture(index), "non-index and missing capture remain undefined");
  }
  check(flight::regexp_capture_value(captures->capture(2)).empty(), "empty member receiver is present");
  bool threw = false;
  try { static_cast<void>(flight::regexp_capture_value(captures->capture(1))); }
  catch (const flight::TypeError& error) { threw = error.name() == String("TypeError"); }
  check(threw, "missing String member receiver throws TypeError");

  const String units(std::u16string{u'x', 0xd83d, 0xde00, 0xd800, u'y', 0xdc00});
  RegExp expression("(.)", "g");
  auto alias = expression;
  for (std::size_t index = 0; index < units.length(); ++index) {
    const auto match = alias.exec(units);
    check(match && match->index == static_cast<double>(index) && match->input == units &&
          match->capture(1) == units.slice(static_cast<std::ptrdiff_t>(index), static_cast<std::ptrdiff_t>(index + 1)), "exec uses original UTF16 units and index");
  }
  check(!expression.exec(units), "failed global exec resets shared lastIndex");
  check(alias.exec(units)->index == 0.0, "alias sees lastIndex reset");
  RegExp empty("()", "g");
  check(empty.exec(units)->index == 0.0 && empty.exec(units)->index == 0.0,
        "exec does not advance an empty match");
  RegExp nonglobal("(.)");
  check(nonglobal.exec(units)->index == 0.0 && nonglobal.exec(units)->index == 0.0,
        "nonglobal exec leaves iteration unchanged");

  const auto matches = units.match(RegExp("(.)", "g"));
  check(matches && matches->size() == units.length() && matches->capture(3) == units.slice(3, 4),
        "global String match exposes every present UTF16 match");
  const auto replaced = units.replace(RegExp("(.)", "g"),
      [](const String&, const std::optional<String>& capture, double offset, const String& input) {
        if (!capture || input.length() != 6) std::exit(2);
        return *capture + String::from_number(offset);
      });
  String expected;
  for (std::size_t i = 0; i < units.length(); ++i)
    expected = expected + units.slice(static_cast<std::ptrdiff_t>(i), static_cast<std::ptrdiff_t>(i + 1)) + String::from_number(static_cast<double>(i));
  check(replaced == expected, "callback captures and offsets preserve original UTF16");
  check(units.replace(RegExp("x"), String("$'")) == units.slice(1) + units.slice(1),
        "replacement suffix preserves lone surrogates");
  check(units.replace(RegExp("[\\s\\S]", "g"), String("_")) == String("______"),
        "combined whitespace classes match each code unit");
  check(String(std::u16string{0, 0xd800}).replace(RegExp("\\S", "g"), "_") == String("__"),
        "nonspace ranges include NUL and isolated surrogates");
  for (char16_t space : {char16_t(0x00a0), char16_t(0x1680), char16_t(0x2028), char16_t(0xfeff)}) {
    const String input(std::u16string{space});
    check(RegExp("^\\s$").test(input) && !RegExp("^\\S$").test(input),
          "ECMAScript whitespace is independent of locale");
  }
  check(!RegExp(".").test(String(u"\u2028")) && RegExp("[.]").test("."),
        "dot excludes line terminators and stays literal in a class");
  check(units.search(RegExp("y")) == 4.0, "search reports UTF16 offset");
  check(String("a,b").split(RegExp(","))[1] == String("b"), "regexp split remains usable");
  std::cout << checks << " capture/runtime checks passed\n";
}
