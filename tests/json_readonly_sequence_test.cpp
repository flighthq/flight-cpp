#include <flight/json_scalar.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <type_traits>

using Schema = flight::RowReadonly<flight::RowOf<flight::Any>>;
using Row = flight::StructuralRef<Schema>;
using View = flight::SequenceView<Row>;
using Writable = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Any>>>;
static_assert(!std::is_convertible_v<Row, Writable>);
static_assert(!std::is_constructible_v<flight::Array<Row>, View>);
template <typename T> concept HasPush = requires(T value, Row row) { value.push(row); };
static_assert(!HasPush<View>);
struct NativeLayer : flight::ReferenceEnabled { double value{}; };

int main() {
  int checks = 0, failures = 0;
  const auto check = [&](bool condition) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "check " << checks << " failed\n"; }
  };
  const auto read = [](const Row& row) {
    return flight::read_json_scalar_row_member(row, flight::String("value"));
  };
  std::optional<View> escaped;
  std::optional<Row> old_element;
  std::weak_ptr<flight::JsonArray> lifetime;
  const void* identity = nullptr;
  {
    auto parsed = flight::Json::parse(R"([{"value":1},null,2,{"value":"wrong"}])");
    auto owner = parsed.array_owner();
    lifetime = owner;
    identity = owner.get();
    escaped = flight::json_readonly_sequence<Row>(flight::Any(parsed));
    check(escaped->identity() == identity && escaped->size() == 4);
    old_element = (*escaped)[0];
    check(old_element->shared_native_object().get() == owner->operator[](0).object_owner().get());
    bool native_escape = false;
    try { (void)flight::structural_ref_cast<flight::Ref<NativeLayer>>(*old_element); }
    catch (const std::bad_cast&) { native_escape = true; }
    check(native_escape);
    check(read(*old_element).as_native_number() == 1);
    owner->operator[](0).as_object().set("value", 7);
    check(read((*escaped)[0]).as_native_number() == 7);
    owner->operator[](0) = flight::Json::parse(R"({"value":9})");
    check(read((*escaped)[0]).as_native_number() == 9);
    check(read(*old_element).as_native_number() == 7);
    check((*escaped)[0].identity() != old_element->identity());
    owner->push(flight::Json::parse(R"({"value":11})"));
    check(escaped->size() == 5 && read((*escaped)[4]).as_native_number() == 11);
    owner->erase(4);
    check(escaped->size() == 4);
    check(flight::json_row_raw((*escaped)[1]).is_null());
    check(flight::json_row_raw((*escaped)[2]).as_number() == 2);
    check(read((*escaped)[3]).type_of() == "string");
    bool null_read = false;
    try { (void)read((*escaped)[1]); } catch (const flight::TypeError&) { null_read = true; }
    check(null_read);
    check(read((*escaped)[2]).is_absent());
    bool escape = false;
    try { (void)read((*escaped)[3]).as_native_number(); }
    catch (const flight::NativeScalarEscapeError&) { escape = true; }
    check(escape);
    check(escaped->identity() == identity);
  }
  check(!lifetime.expired() && escaped->identity() == identity);
  check(read((*escaped)[0]).as_native_number() == 9);
  check(read(*old_element).as_native_number() == 7);
  escaped.reset();
  check(lifetime.expired());
  check(read(*old_element).as_native_number() == 7);
  old_element.reset();

  // The same erased bridge retains native array storage; no JsonArray is manufactured.
  flight::Array<flight::Any> native{flight::Any(flight::Json::parse(R"({"value":3})"))};
  const auto native_view = flight::json_readonly_sequence<Row>(flight::Any(native));
  check(native_view.identity() == native.identity() && native_view.size() == 1);
  native.push(flight::Any(flight::undefined));
  check(native_view.size() == 2 && flight::json_row_raw(native_view[1]).is_undefined());
  native[0] = flight::Any(flight::Json::parse(R"({"value":8})"));
  check(read(native_view[0]).as_native_number() == 8);

  int projections = 0;
  const auto delayed = View::from_erased_array(flight::Any(native), [&](const flight::Any&) -> Row {
    ++projections;
    throw flight::TypeError("original element use");
  });
  check(projections == 0);
  check(delayed.size() == 2 && projections == 0);
  bool delayed_error = false;
  try { (void)delayed[0]; } catch (const flight::TypeError&) { delayed_error = true; }
  check(delayed_error && projections == 1);
  // Passing an invalid asserted array does not move its later iteration error before the callee.
  const auto invalid = flight::json_readonly_sequence<Row>(flight::Any(flight::null));
  check(invalid.identity() == nullptr);
  bool iteration_error = false;
  try { (void)invalid.begin(); (void)invalid.end(); }
  catch (const flight::TypeError&) { iteration_error = true; }
  check(iteration_error);
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
