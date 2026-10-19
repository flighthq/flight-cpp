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
  int some_calls = 0;
  check(view.some([&](double value, double index, const auto& all) {
    ++some_calls;
    check(all.identity() == source.identity(), "some supplies the retained source view");
    return index == 1 && value == 8;
  }) && some_calls == 2, "some terminates at the first success");
  check(view.some([]() { return true; }), "some supports a zero-parameter view callback");
  check(!flight::SequenceView<double>{}.some([]() { return true; }), "some is false on an empty view");
  flight::Array<double> changing{1, 2};
  const flight::SequenceView<double> changing_view(changing);
  some_calls = 0;
  check(!changing_view.some([&](double value, double index) {
    ++some_calls;
    if (index == 0) { changing.push(9); changing.element(1) = 7; }
    return value == 9;
  }) && some_calls == 2, "some reads live elements but ignores appended indices beyond its initial length");
  check(changing_view.some([](double value) { return value == 7; }), "some sees mutations through the original owner");
  some_calls = 0;
  check(!changing_view.some([&](double, double index) {
    ++some_calls;
    if (index == 0) changing.clear();
    return false;
  }) && some_calls == 1, "some skips indices removed by its callback");
  bool some_threw = false;
  try { (void)view.some([]() -> bool { throw std::runtime_error("some"); }); }
  catch (const std::runtime_error&) { some_threw = true; }
  check(some_threw, "some preserves predicate exceptions");
  check(view.flat_map([]() { return flight::Array<double>{4}; }).size() == 3, "view flatMap supports a zero-parameter callback");
  const auto arities = flight::array_from(source, [](auto... arguments) { return sizeof...(arguments); });
  check(arities.element(0) == 2, "Array.from supplies only value and index");
  check(flight::array_from(source, []() { return 7; }).element(1) == 7, "Array.from ignores unaccepted arguments");
  bool threw = false;
  try { (void)source.map([]() -> double { throw std::runtime_error("callback"); }); }
  catch (const std::runtime_error&) { threw = true; }
  check(threw, "callback exceptions propagate without replacement");
  flight::Array<std::optional<double>> nullable{std::nullopt, 4.0, 8.0};
  int projected_calls = 0;
  const auto projected = nullable.filter_projected(
      [&](std::optional<double> value, double index, const auto& all) {
        ++projected_calls;
        check(all == nullable, "projected filter receives the original owner");
        if (index == 1) { all.element(1) = 9; all.push(12.0); }
        return value.has_value();
      }, [](const auto& value) { return value.value(); });
  check(projected_calls == 3 && projected.size() == 2, "filter snapshots its initial length");
  check(projected.element(0) == 4 && nullable.element(1).value() == 9,
        "filter retains the source value read before the callback writes its slot");
  const auto shortened = nullable.filter_projected(
      [&](const auto& value, double index) { if (index == 0) nullable.clear(); return value.has_value(); },
      [](const auto& value) { return value.value(); });
  check(shortened.empty(), "filter skips indices removed by a callback");
  bool projected_threw = false;
  try { (void)source.filter_projected([]() -> bool { throw std::runtime_error("predicate"); },
                                    [](double value) { return value; }); }
  catch (const std::runtime_error&) { projected_threw = true; }
  check(projected_threw, "projected filter preserves predicate exceptions");
  const flight::Array<double> output;
  const auto output_owner = output.identity();
  check(output.set_index(0, 1) == 1 && output.size() == 1,
        "an indexed write grows an empty output at its end");
  const auto output_alias = output;
  output_alias.set_index(1, 2);
  check(output.identity() == output_owner && output.size() == 2 && output.element(1) == 2,
        "append retains the same observable output owner");
  output.set_index(0, 9);
  check(output.element(0) == 9 && output.element(1) == 2,
        "an indexed overwrite preserves the existing tail");
  for (const double index : {-1.0, 0.5, 3.0, 4294967295.0,
                            std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::quiet_NaN()}) {
    bool refused = false;
    try { output.set_index(index, 5); } catch (const flight::OutOfProfileError&) { refused = true; }
    check(refused && output.size() == 2 && output.element(0) == 9 && output.element(1) == 2,
          "unrepresented holes and properties refuse without changing earlier writes");
  }
  bool write_read_threw = false;
  try { output.set_index(2, []() -> double { throw std::runtime_error("later value"); }()); }
  catch (const std::runtime_error&) { write_read_threw = true; }
  check(write_read_threw && output.size() == 2 && output.element(1) == 2,
        "a failed right-hand read preserves the already-written prefix");
  output.set_index(2, [&]() { output.push(13); return 7.0; }());
  check(output.size() == 3 && output.element(2) == 7 && output.identity() == output_owner,
        "the write uses the length after right-hand side effects");
  using NumericSource = std::variant<flight::Array<double>, flight::Float32Array>;
  static_assert(!std::constructible_from<flight::SequenceView<double>,
                                       std::variant<flight::Array<double>, std::string>>);
  const flight::Array<double> numbers{1, 2};
  NumericSource selected(numbers);
  const flight::SequenceView<double> retained_array(selected);
  numbers.push(3);
  check(retained_array.identity() == numbers.identity() && retained_array.size() == 3 &&
            retained_array[2] == 3 && !retained_array.is_float32_array_backed(),
        "a union sequence view retains the live original Array owner");
  const flight::Float32Array floats(2.0);
  floats.set_index(0, 7); floats.set_index(1, 8);
  selected = floats;
  const flight::SequenceView<double> retained_typed(selected);
  floats.set_index(1, 9);
  check(retained_typed.identity() == floats.identity() && retained_typed[1] == 9 &&
            retained_typed.is_float32_array_backed(),
        "a union sequence view retains its exact typed backing and live values");
  selected = numbers;
  check(retained_typed.identity() == floats.identity() && retained_typed[0] == 7,
        "reassigning the source variant does not replace the retained view owner");
  const auto recovered_array = retained_array.array_owner_if();
  check(recovered_array && recovered_array->identity() == numbers.identity(),
        "exact Array recovery returns the original owner without copying elements");
  recovered_array->set_index(0, 11);
  check(numbers.element(0) == 11 && retained_array[0] == 11,
        "writes through a recovered original Array remain visible in its view");
  check(!retained_typed.array_owner_if(), "typed backing cannot be fabricated as an Array owner");
  const flight::SequenceView<double> projected_array(flight::Array<int>{1, 2});
  check(!projected_array.array_owner_if(), "an element conversion cannot fabricate an Array owner");
  const flight::SequenceView<double> foreign_array(std::make_shared<std::vector<double>>(2, 5));
  check(!foreign_array.array_owner_if(), "a foreign indexed provider remains a foreign owner");
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
