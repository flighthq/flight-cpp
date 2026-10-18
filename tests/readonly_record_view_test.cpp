#include <flight/object.hpp>
#include <flight/readonly_record_view.hpp>

#include <iostream>
#include <optional>
#include <type_traits>
#include <variant>
#include <vector>

using Narrow = std::variant<double, flight::String>;
using Wide = std::variant<bool, double, flight::String>;
using Record = flight::Record<flight::String, Narrow>;
using View = flight::ReadonlyRecordView<Wide>;

template <typename T>
concept CanWriteBool = requires(T value) { value.set(flight::String("key"), true); };

static_assert(!CanWriteBool<View>);
static_assert(!std::is_constructible_v<Record, View>);
static_assert(!std::is_constructible_v<flight::ReadonlyRecordView<Narrow>, flight::Record<flight::String, Wide>>);
static_assert(!std::is_constructible_v<View, flight::Record<flight::String, flight::Any>>);
static_assert(!std::is_constructible_v<View, flight::Record<double, Narrow>>);

struct Diagnostic { std::optional<View> detail; };

int main() {
  int checks = 0;
  int failures = 0;
  const auto check = [&](bool ok) { ++checks; if (!ok) ++failures; };
  Record record;
  record.set(flight::String("label"), flight::String("first"));
  record.set(flight::String("2"), 2.0);
  record.set(flight::String("1"), 1.0);
  std::vector<Diagnostic> sink;
  const auto report = [&](std::optional<Record> detail) { sink.push_back(Diagnostic{detail}); };
  report(record);
  const View view = sink.front().detail.value();
  check(view.identity() == record.identity() && view == record);
  check(view.get(flight::String("label")) == std::optional<Wide>(flight::String("first")));
  record.set(flight::String("label"), flight::String("changed"));
  check(std::get<flight::String>(sink.front().detail->get(flight::String("label")).value()) == flight::String("changed"));
  record.set(flight::String("added"), 5.0);
  check(view.has(flight::String("added")) && std::get<double>(view.get(flight::String("added")).value()) == 5);
  record.erase(flight::String("label"));
  check(!view.has(flight::String("label")) && !view.get(flight::String("label")));
  check(!view.get(flight::String("missing")));
  const auto keys = flight::object_keys(view);
  check(keys.size() == 3 && keys.element(0) == flight::String("1") && keys.element(1) == flight::String("2") && keys.element(2) == flight::String("added"));
  check(std::get<double>(view.get(2.0).value()) == 2);
  const auto entries = flight::object_entries(view);
  check(entries.size() == 3 && std::get<0>(entries.element(2)) == flight::String("added"));
  const auto values = flight::object_values(view);
  check(values.size() == 3 && std::get<double>(values.element(0)) == 1);
  const flight::ReadonlyRecordView<Narrow> narrow_view(record);
  const View forwarded(narrow_view);
  check(forwarded.identity() == record.identity() && forwarded == view);
  report(std::nullopt);
  check(!sink.back().detail);
  report(Record{});
  check(sink.back().detail && sink.back().detail->enumerable_keys().empty());
  std::weak_ptr<void> lifetime;
  std::optional<View> escaped;
  {
    Record local;
    local.set(flight::String("keep"), 7.0);
    lifetime = local.shared_owner();
    escaped.emplace(local);
  }
  check(!lifetime.expired() && std::get<double>(escaped->get(flight::String("keep")).value()) == 7);
  escaped.reset();
  check(lifetime.expired());
  flight::Record<flight::String, Wide> wide;
  wide.set(flight::String("boolean"), false);
  check(std::get<bool>(View(wide).get(flight::String("boolean")).value()) == false);
  const View first;
  const View second;
  check(first.identity() != second.identity());
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
