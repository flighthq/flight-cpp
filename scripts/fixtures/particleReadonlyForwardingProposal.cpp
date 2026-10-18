// Synthetic ABI/owner controls for an INACTIVE compiler proposal, not original parser qualification.
#include "proposal.hpp"
#include <iostream>
#include <type_traits>
#include <vector>
namespace api = flighthq_example;
using Read = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<api::Options>>>>;
using Native = double (*)(flight::String, std::optional<flight::Ref<api::Options>>);
static_assert(std::is_same_v<decltype(&api::dispatch), Native>);
static_assert(!std::is_invocable_v<decltype(&api::dispatch), flight::String, std::optional<Read>>);
static_assert(std::is_invocable_v<decltype(&api::consume), flight::String, std::optional<Read>>);
struct Sentinel {};
struct Cell final : flight::TypedRowCell<std::optional<double>> {
  std::optional<double> value{7}; int reads = 0; bool throws = false;
  std::shared_ptr<Sentinel> sentinel = std::make_shared<Sentinel>();
  std::optional<double>& get() override { ++reads; if (throws) throw sentinel; return value; }
  void set(std::optional<double> next) override { value = next; }
};
int checks = 0;
void check(bool result) { ++checks; if (!result) throw checks; }
int main() {
  try {
    const Native native = &api::dispatch;
    auto cell = std::make_shared<Cell>(), extra = std::make_shared<Cell>();
    auto owner = std::make_shared<flight::RowOwner>();
    owner->set_named_cell("size", cell); owner->set_named_cell("kind", extra);
    std::weak_ptr<flight::RowOwner> lifetime = owner;
    Read options = Read::from_owner(owner);
    std::optional<Read> retained;
    int calls = 0;
    api::sink->parse = [&](flight::String text, std::optional<Read> value) {
      ++calls; check(text == "input"); check(extra->reads == 0);
      if (!value) return 5.0;
      check(value->identity() == owner.get());
      retained = value;
      return flight::row_get<flight::RowKey<"size">>(*value).value_or(5);
    };
    check(api::consume("input") == 5 && calls == 1 && cell->reads == 0);
    check(api::consume("input", options) == 7 && calls == 2 && cell->reads == 1);
    check(retained->shared_owner() == owner && !retained->shared_native_object());
    cell->value = 11; check(api::consume("input", options) == 11 && cell->reads == 2);
    cell->value.reset(); check(api::consume("input", options) == 5 && cell->reads == 3);
    cell->throws = true;
    try { api::consume("input",options); check(false); }
    catch (const std::shared_ptr<Sentinel>& error) { check(error == cell->sentinel && cell->reads == 4); }
    cell->throws = false;
    auto sentinel = std::make_shared<Sentinel>();
    api::sink->parse = [&](flight::String, std::optional<Read> value) -> double {
      check(value->identity() == owner.get()); check(cell->reads == 4); throw sentinel;
    };
    try { api::consume("input",options); check(false); }
    catch (const std::shared_ptr<Sentinel>& error) { check(error == sentinel && cell->reads == 4); }
    api::sink->parse = [](flight::String, std::optional<Read> value) { return value ? flight::row_get<flight::RowKey<"size">>(*value).value_or(5) : 5; };
    auto nominal = std::make_shared<api::Options>(); nominal->size = 13;
    check(native("input", nominal) == 13); check(native == &api::dispatch);
    owner.reset(); options = {}; check(!lifetime.expired());
    retained.reset(); check(lifetime.expired());
    std::cout << "{\"checks\":" << checks << ",\"failures\":0}\n";
  } catch (int failure) { std::cerr << "failed check " << failure << '\n'; return 1; }
}
