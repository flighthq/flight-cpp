#include <flight/array.hpp>
#include <flight/sequence_view.hpp>

#include <iostream>
#include <stdexcept>

int main() {
  int checks = 0;
  int failures = 0;
  const auto check = [&](bool ok, const char* message) {
    ++checks;
    if (!ok) { ++failures; std::cerr << message << '\n'; }
  };
  flight::Array<double> source{1.0, 3.0, 2.0};
  int calls = 0;
  const auto zero = source.map([&]() { ++calls; return flight::Array<double>(2.0).fill(0.0); });
  check(calls == 3 && zero.size() == 3 && zero.element(0).size() == 2,
        "a zero-parameter mapper ignores the supplied arguments");
  const auto one = source.map([](double value) { return value * 2; });
  const auto two = source.map([](double value, double index) { return value + index; });
  check(one.element(1) == 6 && two.element(2) == 4, "one and two parameter callbacks keep their arguments");
  const auto filtered = source.filter([&](double value, double index, const flight::Array<double>& all) {
    check(all == source, "the third callback argument is the original array");
    return index == 0 || value > all.element(index - 1);
  });
  check(filtered.size() == 2 && filtered.element(1) == 3, "filter supplies the original array for indexed reads");
  const auto live = source.map([&](double value, double index, const flight::Array<double>& all) {
    if (index == 0) all.element(1) = 8;
    return value;
  });
  check(source.element(1) == 8 && live.element(1) == 8, "writes through the callback array reach the same owner");
  check(source.every([&](double, double, const auto& all) { return all == source; }), "every supplies its source");
  check(source.some([]() { return true; }), "some supports a zero-parameter predicate");
  check(source.find([]() { return true; }).value() == 1, "find supports a zero-parameter predicate");
  check(source.find_index([&](double value, double, const auto& all) { return all == source && value == 8; }) == 1,
        "findIndex supplies its source");
  calls = 0;
  source.for_each([&](double, double, const auto& all) { check(all == source, "forEach retains its source"); ++calls; });
  check(calls == 3, "forEach invokes every element");
  const auto flattened = source.flat_map([&](double value, double, const auto& all) {
    check(all == source, "flatMap retains its source"); return flight::Array<double>{value};
  });
  check(flattened.size() == 3 && flattened.element(1) == 8, "flatMap retains the returned values");
  const flight::SequenceView<double> view(source);
  const auto viewed = view.map([&](double value, double, const flight::SequenceView<double>& all) {
    check(all.identity() == source.identity(), "a view callback retains the original source identity"); return value;
  });
  check(viewed.element(1) == 8, "view mapping reads the same live owner");
  check(view.filter([]() { return true; }).size() == 3, "view filter supports a zero-parameter callback");
  check(view.flat_map([]() { return flight::Array<double>{4}; }).size() == 3, "view flatMap supports a zero-parameter callback");
  const auto arities = flight::array_from(source, [](auto... arguments) { return sizeof...(arguments); });
  check(arities.element(0) == 2, "Array.from supplies only value and index");
  check(flight::array_from(source, []() { return 7; }).element(1) == 7, "Array.from ignores unaccepted arguments");
  bool threw = false;
  try { (void)source.map([]() -> double { throw std::runtime_error("callback"); }); }
  catch (const std::runtime_error&) { threw = true; }
  check(threw, "callback exceptions propagate without replacement");
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
