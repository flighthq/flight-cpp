// Includes the compiler-emitted header produced by typedEmptyFallbackIterationOracle.mjs.
#include "iteration.hpp"
#include <iostream>
#include <memory>
#include <vector>
namespace api = flighthq_example;
using Item = flight::Ref<api::Item>;
using Items = flight::Array<Item>;
using Read = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<api::Options>>>>;
using NullableRead = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<api::NullableOptions>>>>;
int checks = 0;
void check(bool value) { ++checks; if (!value) throw checks; }
struct Sentinel {};
template<class Value> struct Cell final : flight::TypedRowCell<Value> {
  Value value{};
  int reads = 0;
  bool throws = false;
  std::shared_ptr<Sentinel> sentinel = std::make_shared<Sentinel>();
  std::vector<int>* events = nullptr;
  Value& get() override { ++reads; if (events) events->push_back(0); if (throws) throw sentinel; return value; }
  void set(Value next) override { value = std::move(next); }
};
template<class View, class CellType> View view(const std::shared_ptr<CellType>& cell) {
  auto owner = std::make_shared<flight::RowOwner>();
  owner->set_named_cell("formats", cell);
  return View::from_owner(owner);
}
Item item(double size) { auto result = std::make_shared<api::Item>(); result->size = size; return result; }
int main() {
  try {
    auto a = item(1), b = item(2), c = item(3), d = item(4);
    auto cell = std::make_shared<Cell<decltype(api::Options::formats)>>();
    const auto options = view<Read>(cell);
    std::vector<Item> seen;
    auto use = [&](Item next) { seen.push_back(next); };
    api::visit(options, use); check(cell->reads == 1 && seen.empty());
    cell->value = Items{}; api::visit(options, use); check(cell->reads == 2 && seen.empty());
    cell->value = Items{a,b}; api::visit(options, use);
    check(cell->reads == 3 && seen == std::vector<Item>{a,b});
    std::vector<int> events; cell->events = &events; seen.clear();
    api::visit(options, [&](Item next) { events.push_back(static_cast<int>(next->size)); use(next); });
    check(events == std::vector<int>{0,1,2}); cell->events = nullptr;
    cell->throws = true; seen.clear();
    try { api::visit(options, use); check(false); } catch (const std::shared_ptr<Sentinel>& error) { check(error == cell->sentinel && seen.empty()); }
    cell->throws = false;
    auto sentinel = std::make_shared<Sentinel>();
    try { api::visit(options, [&](Item next) { check(next == a); throw sentinel; }); check(false); }
    catch (const std::shared_ptr<Sentinel>& error) { check(error == sentinel); }
    // Mutate the original array while iterating: appended elements are visited in source order.
    Items original{a,b}; cell->value = original; seen.clear();
    api::visit(options, [&](Item next) { use(next); if (next == a) { original.push(c); b->size = 20; } if (next == b) check(next->size == 20); });
    check(seen == std::vector<Item>{a,b,c}); check(cell->value->identity() == original.identity()); b->size = 2;
    // The selected source stays alive after the option property is reset, then releases afterward.
    cell->value = Items{a,b}; auto weak = cell->value->weaken(); seen.clear();
    api::visit(options, [&](Item next) { cell->value.reset(); check(!weak.expired()); use(next); });
    check(seen == std::vector<Item>{a,b} && weak.expired());
    original = Items{a,b,c}; cell->value = original; seen.clear();
    api::visit(options, [&](Item next) { use(next); original.resize(1.0); });
    check(seen == std::vector<Item>{a});
    cell->value = Items{a,b,c,d}; seen.clear(); api::control(options, use); check(seen == std::vector<Item>{b,c});
    auto nullable = std::make_shared<Cell<decltype(api::NullableOptions::formats)>>();
    const auto nullableOptions = view<NullableRead>(nullable);
    for (bool isNull : {false,true}) { nullable->value = isNull ? decltype(nullable->value)(flight::null) : decltype(nullable->value)(flight::undefined); seen.clear(); api::visit_nullable(nullableOptions,use); check(seen.empty()); }
    nullable->value = Items{}; api::visit_nullable(nullableOptions,use); check(seen.empty());
    nullable->value = Items{c,d}; api::visit_nullable(nullableOptions,use); check(seen == std::vector<Item>{c,d});
    auto reads = cell->reads; seen.clear(); api::visit_optional(std::nullopt,use); api::visit_guarded(std::nullopt,use);
    check(cell->reads == reads && seen.empty());
    cell->value = Items{a}; api::visit_optional(options,use); api::visit_guarded(options,use); check(seen == std::vector<Item>{a,a} && cell->reads == reads + 2);
    int evaluations = 0; seen.clear(); api::visit_source([&]() -> std::optional<Items> { ++evaluations; return std::nullopt; }, use); check(evaluations == 1 && seen.empty());
    api::visit_source([&]() -> std::optional<Items> { ++evaluations; return Items{d}; },use); check(evaluations == 2 && seen == std::vector<Item>{d});
    seen.clear(); try { api::visit_source([&]() -> std::optional<Items> { ++evaluations; throw sentinel; },use); check(false); }
    catch (const std::shared_ptr<Sentinel>& error) { check(error == sentinel && evaluations == 3 && seen.empty()); }
    std::cout << "{\"checks\":" << checks << ",\"failures\":0}\n";
  } catch (int failure) { std::cerr << "failed check " << failure << '\n'; return 1; }
}
