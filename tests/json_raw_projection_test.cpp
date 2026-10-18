#include <flight/json_scalar.hpp>
#include <cassert>
#include <exception>
#include <functional>

namespace {
struct GetterOwner : flight::ReferenceEnabled {
  flight::Any value;
  int reads = 0;
  std::exception_ptr thrown;
  flight::Any get_v() {
    ++reads;
    if (thrown) std::rethrow_exception(thrown);
    return value;
  }
};
const bool prototype = flight::register_class_prototype<GetterOwner>(flight::ClassPrototype{{}, {"v"}, std::nullopt, false});
const bool fields = flight::register_class_fields<GetterOwner>({});
const bool readers = flight::register_class_property_readers<GetterOwner>({
  {"v", [](const flight::Any& erased) -> flight::Any {
    const auto owner = erased.object_reference_if<GetterOwner>();
    if (!owner) throw flight::OutOfProfileError("wrong getter receiver");
    return owner->get_v();
  }},
});
template<typename Error, typename F> void throws(F run) {
  bool caught = false;
  try { run(); } catch (const Error&) { caught = true; }
  assert(caught);
}
}

int main() {
  using flight::Any;
  assert(prototype && fields && readers);
  const auto original = flight::make_ref<GetterOwner>();
  original->value = Any(flight::Array<Any>{Any(3.0)});
  const auto erased = Any::object(original);
  assert(flight::has_property(erased, "v") && original->reads == 0);
  throws<flight::OutOfProfileError>([&] { (void)flight::member_typeof(erased, "v"); });
  assert(original->reads == 0);
  using Schema = flight::RowReadonly<flight::RowOf<flight::Ref<GetterOwner>>>;
  const auto row = flight::json_assertion_row<Schema>(erased);
  assert(row.shared_native_object() == original);
  assert(flight::json_row_raw(row).object_reference_if<GetterOwner>() == original);
  const auto v = flight::read_json_scalar_property(flight::json_row_raw(row), "v");
  assert(original->reads == 1 && v.raw() == original->value);
  assert(flight::read_json_scalar_property(v.raw(), "length").as_native_number() == 1);
  assert(flight::read_json_scalar_property(v.raw(), "0").as_native_number() == 3);
  assert(flight::read_json_scalar_property(v.raw(), "1").is_absent());
  original->thrown = std::make_exception_ptr(flight::RangeError("same getter exception"));
  bool same = false;
  try { (void)flight::read_json_scalar_property(erased, "v"); }
  catch (const flight::RangeError&) { same = std::current_exception() == original->thrown; }
  assert(same && original->reads == 2);
  original->thrown = {};

  // The callback executes without the metadata lock held and can reenter another property query.
  const auto reader = flight::find_exact_class_property_reader(typeid(GetterOwner), "v");
  assert(reader && reader->read);
  const auto reentrant = flight::register_class_property_readers<GetterOwner>({
    {"v", [](const Any& value) -> Any { assert(flight::has_property(value, "v")); return Any(7.0); }},
  });
  assert(reentrant && flight::read_json_scalar_property(erased, "v").as_native_number() == 7);

  const auto parsed = flight::Json::parse("[1]");
  auto cursor = flight::json_scalar_values(Any(parsed));
  auto alias = cursor;
  assert(cursor.next().value->as_number() == 1);
  parsed.array_owner()->push(flight::JsonValue(2.0));
  assert(alias.next().value->as_number() == 2);
  assert(cursor.next().done);
  parsed.array_owner()->push(flight::JsonValue(3.0));
  assert(alias.next().done);
  auto text = flight::json_scalar_values(Any(flight::String("🙂")));
  const auto point = text.next();
  assert(point.value->as_string().native().size() == 2 && text.next().done);
  assert(flight::read_json_scalar_property(*point.value, "0").raw().as_string().native()[0] == 55357);
  assert(flight::read_json_scalar_property(*point.value, "1").raw().as_string().native()[0] == 56898);
  throws<flight::TypeError>([] { (void)flight::json_scalar_values(Any(7.0)); });
  throws<flight::TypeError>([] { (void)flight::read_json_scalar_property(Any(flight::null), "0"); });
  throws<flight::OutOfProfileError>([&] { (void)flight::json_scalar_values(erased); });
  throws<flight::OutOfProfileError>([] { (void)flight::read_json_scalar_property(Any::function([] { return 1.0; }), "0"); });
  throws<flight::OutOfProfileError>([] { (void)flight::read_json_scalar_property(Any::external(7), "0"); });
  throws<flight::NativeScalarEscapeError>([] { (void)flight::JsonScalarCell{Any(false)}.as_native_number(); });
  const std::optional<double> missing;
  assert(flight::json_property_value(missing, Any(flight::null)).is_null());
  assert(flight::json_property_value(missing, Any(flight::undefined)).is_undefined());
  throws<flight::OutOfProfileError>([&] { (void)flight::json_property_value(missing); });
  throws<flight::OutOfProfileError>([] { (void)flight::json_property_value(std::function<double()>([] { return 1.0; })); });
}
