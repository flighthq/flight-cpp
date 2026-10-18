#include <flight/array.hpp>
#include <flight/reference.hpp>

#include <iostream>
#include <optional>

struct Item : flight::ReferenceEnabled { double value; };

int main() {
  int checks = 0;
  int failures = 0;
  const auto check = [&](bool ok) { ++checks; if (!ok) ++failures; };
  flight::Array<double> first{1, 2};
  flight::Array<double> second{3};
  flight::Array<flight::Array<double>> source{first, {}, second};
  const auto result = source.flat();
  check(result.size() == 3);
  check(result.element(0) == 1 && result.element(1) == 2 && result.element(2) == 3);
  check(source.size() == 3 && source.element(0) == first && source.element(2) == second);
  check(result.identity() != first.identity() && result.identity() != second.identity());
  result.element(0) = 9;
  check(first.element(0) == 1);
  first.element(1) = 8;
  check(result.element(1) == 2);
  const auto shallow = first.flat();
  check(shallow.identity() != first.identity() && shallow.element(1) == 8);
  const flight::Array<double> empty;
  check(empty.flat().empty() && empty.flat().identity() != empty.identity());
  check(flight::Array<flight::Array<double>>{{}, {}}.flat().empty());
  const auto item = flight::make_ref<Item>();
  item->value = 4;
  flight::Array<flight::Array<flight::Ref<Item>>> objects{{item}};
  const auto retained = objects.flat();
  check(retained.element(0) == item);
  item->value = 7;
  check(retained.element(0)->value == 7);
  const flight::Array<flight::Array<std::optional<double>>> absent{{std::nullopt, 5}};
  const auto present_undefined = absent.flat();
  check(present_undefined.size() == 2 && !present_undefined.element(0));
  check(present_undefined.element(1).value() == 5);
  const flight::Array<flight::Array<flight::Array<double>>> deeper{source};
  const auto one_level = deeper.flat();
  check(one_level.size() == 3 && one_level.element(0) == first);
  one_level.element(2).push(6);
  check(second.size() == 2 && source.element(2).element(1) == 6);
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
