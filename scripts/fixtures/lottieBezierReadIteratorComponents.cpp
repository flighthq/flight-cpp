// Private component experiments using existing runtime APIs. No Bezier body or public API replacement.
#include <flight/json_scalar.hpp>
#include <array>
#include <exception>
#include <iostream>
#include <vector>

namespace {
using flight::Any;
int inherited_getter_reads = 0;
struct InheritedGetter : flight::ReferenceEnabled {
  Any get_v() const { ++inherited_getter_reads; throw flight::RangeError("original v getter"); }
};
const bool prototype_registered = flight::register_class_prototype<InheritedGetter>(
    flight::ClassPrototype{{}, {"v"}, std::nullopt, false});
const bool fields_registered = flight::register_class_fields<InheritedGetter>({});
struct FrameTag : flight::ReferenceEnabled {};
using Frame = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<FrameTag>>>>;

// A test-local use of the existing shared live cursor, with JSON-array/string branches.
// Foreign iterator protocols remain explicitly unrepresented; this is not a product factory.
flight::ArrayIterator<Any> component_iterator(Any source) {
  if (source.is_array()) {
    return flight::ArrayIterator<Any>([source](std::size_t& index) -> std::optional<Any> {
      if (index >= source.array_size()) return std::nullopt;
      return source.array_get(static_cast<double>(index++)).value_or(Any(flight::undefined));
    });
  }
  if (source.kind() == flight::AnyKind::string) {
    return flight::ArrayIterator<Any>([text = source.as_string()](std::size_t& index) -> std::optional<Any> {
      const auto& units = text.native();
      if (index >= units.size()) return std::nullopt;
      const auto start = index++;
      if (units[start] >= 0xd800 && units[start] <= 0xdbff && index < units.size() &&
          units[index] >= 0xdc00 && units[index] <= 0xdfff) ++index;
      return Any(flight::String(units.substr(start, index - start)));
    });
  }
  if (source.kind() == flight::AnyKind::object && source.held_type() == typeid(flight::JsonObject)) {
    throw flight::TypeError("value is not iterable");
  }
  if (source.kind() == flight::AnyKind::object || source.kind() == flight::AnyKind::function ||
      source.kind() == flight::AnyKind::external) {
    throw flight::OutOfProfileError("foreign Symbol.iterator protocol is not represented by this component");
  }
  throw flight::TypeError("value is not iterable");
}
}

int main() {
  int checks = 0, failures = 0;
  auto check = [&](bool value) { ++checks; if (!value) { ++failures; std::cerr << "failed " << checks << '\n'; } };
  const auto native = flight::make_ref<InheritedGetter>();
  const Any erased = Any::object(native);
  check(prototype_registered && fields_registered && flight::has_property(erased, "v"));
  check(inherited_getter_reads == 0);
  const auto row = flight::json_assertion_row<flight::RowReadonly<flight::RowOf<flight::Ref<InheritedGetter>>>>(erased);
  check(row.shared_native_object() == native);
  check(row.shared_owner() == flight::named_properties(erased).shared_owner());
  check(!flight::named_properties(erased).has("v"));
  check(flight::read_json_scalar_row_member(row, "v").is_absent() && inherited_getter_reads == 0);
  bool getter_lookup_refused = false;
  try { (void)flight::member_typeof(erased, "v"); } catch (const flight::OutOfProfileError&) { getter_lookup_refused = true; }
  check(getter_lookup_refused && inherited_getter_reads == 0);
  bool direct_getter_threw = false;
  try { (void)native->get_v(); } catch (const flight::RangeError&) { direct_getter_threw = true; }
  check(direct_getter_threw && inherited_getter_reads == 1);

  // Own accessor cells support deferred evaluation and retain the exact original thrown exception.
  std::vector<flight::String> order;
  const auto sentinel = std::make_exception_ptr(flight::RangeError("original i getter"));
  const auto owner = std::make_shared<flight::RowOwner>();
  owner->set_named_cell("v", std::make_shared<flight::detail::ReadonlyNamedCell>([&] { order.emplace_back("v"); return Any(7.0); }));
  owner->set_named_cell("i", std::make_shared<flight::detail::ReadonlyNamedCell>([&]() -> Any { order.emplace_back("i"); std::rethrow_exception(sentinel); }));
  owner->set_named_cell("o", std::make_shared<flight::detail::ReadonlyNamedCell>([&] { order.emplace_back("o"); return Any(flight::Array<Any>{}); }));
  const Frame frame = Frame::from_owner(owner);
  check(flight::named_properties(frame).has("v") && order.empty());
  int iteration_starts = 0; bool same_error = false;
  try {
    const std::array<Any, 3> captured{
        flight::read_json_scalar_row_member(frame, "v").raw(),
        flight::read_json_scalar_row_member(frame, "i").raw(),
        flight::read_json_scalar_row_member(frame, "o").raw()};
    ++iteration_starts; (void)component_iterator(captured[0]);
  } catch (const flight::RangeError&) { same_error = std::current_exception() == sentinel; }
  check(same_error);
  check(order == std::vector<flight::String>{"v", "i"} && iteration_starts == 0);

  // Code-point iteration and subsequent UTF-16 index reads are different source operations.
  auto text = component_iterator(Any(flight::String("🙂")));
  const auto first = text.next();
  check(!first.done && first.value->as_string().native().size() == 2);
  const auto high = flight::read_json_scalar_property(*first.value, "0").raw().as_string();
  const auto low = flight::read_json_scalar_property(*first.value, "1").raw().as_string();
  check(high.native().size() == 1 && high.native()[0] == 55357);
  check(low.native().size() == 1 && low.native()[0] == 56898);
  check(text.next().done);
  auto lone = component_iterator(Any(flight::String(std::u16string(1, 0xd800))));
  const auto lone_value = lone.next();
  check(lone_value.value->as_string().native() == std::u16string(1, 0xd800) && lone.next().done);

  const auto parsed = flight::Json::parse("[1]");
  auto live = component_iterator(Any(parsed)); auto shared_cursor = live;
  check(live.next().value->as_number() == 1);
  parsed.array_owner()->push(flight::JsonValue(2.0));
  check(shared_cursor.next().value->as_number() == 2);
  check(live.next().done);
  parsed.array_owner()->push(flight::JsonValue(3.0));
  check(shared_cursor.next().done);
  const auto with_undefined = component_iterator(Any(flight::Array<Any>{Any(flight::undefined)})).next();
  check(!with_undefined.done && with_undefined.value->is_undefined());
  bool null_element_error = false;
  try { (void)flight::read_json_scalar_property(Any(flight::null), "0"); } catch (const flight::TypeError&) { null_element_error = true; }
  check(null_element_error);
  bool primitive_error = false;
  try { (void)component_iterator(Any(7.0)); } catch (const flight::TypeError&) { primitive_error = true; }
  check(primitive_error);
  bool foreign_protocol_refused = false;
  try { (void)component_iterator(erased); } catch (const flight::OutOfProfileError&) { foreign_protocol_refused = true; }
  check(foreign_protocol_refused);

  std::cout << "{\"checks\":" << checks << ",\"failures\":" << failures
            << ",\"inheritedPresenceWithoutRead\":true,\"ownReaderCannotReadInheritedGetter\":true"
            << ",\"sameGetterException\":" << (same_error ? "true" : "false")
            << ",\"utf16Units\":[55357,56898],\"liveSharedCursor\":true,\"foreignIteratorRefused\":true}\n";
  return failures ? 1 : 0;
}
