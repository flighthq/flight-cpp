// Focused vertical slice for the approved lazy typed JSON projection, in the exact emitted shapes:
// a proven JSON number property read as a JsonScalarCell, arithmetic lowered through explicit
// ToNumber, typeof on the raw value, and a native-double parameter boundary through
// as_native_number(). A wrong-typed property must reach that native boundary and raise the dedicated
// out-of-profile error -- never an implicit unwrap, never a silent coercion.
#include <flight/json_scalar.hpp>

#include <cassert>
#include <cmath>
#include <cstdio>

namespace {

// Stands for a native parameter/field/return boundary in emitted code.
[[nodiscard]] double native_double(double value) { return value; }

} // namespace

int main() {
  using flight::Any;
  using flight::JsonScalarCell;

  const auto document = flight::Json::parse(flight::String("{\"fr\":30,\"bad\":\"x\"}"));
  const Any receiver(document);

  const JsonScalarCell fr = flight::read_json_scalar_property(receiver, flight::String("fr"));
  const JsonScalarCell bad = flight::read_json_scalar_property(receiver, flight::String("bad"));
  const JsonScalarCell absent = flight::read_json_scalar_property(receiver, flight::String("missing"));

  // Arithmetic on a proven JSON number property: explicit ToNumber through the cell operations,
  // source evaluation order intact.
  const double duration = (100.0 - 10.0) / fr;
  assert(duration == 3.0);
  assert((fr + 2.0).as_native_number() == 32.0 && fr * 2.0 == 60.0 && fr - 10.0 == 20.0);
  assert(fr > 0.0 && bad > 0.0 == false);

  // `+` is ToPrimitive, not universal ToNumber: a string operand concatenates in both orders.
  const JsonScalarCell thirty_text{Any(flight::String("30"))};
  const JsonScalarCell two{Any(2.0)};
  const JsonScalarCell concat_left = thirty_text + two;
  const JsonScalarCell concat_right = two + thirty_text;
  const JsonScalarCell concat_both = thirty_text + thirty_text;
  assert(concat_left.type_of() == "string" && concat_left.to_string() == flight::String("302"));
  assert(concat_right.type_of() == "string" && concat_right.to_string() == flight::String("230"));
  assert(concat_both.to_string() == flight::String("3030"));
  assert(thirty_text - two == 28.0 && thirty_text * two == 60.0 && thirty_text / two == 15.0);

  // Relational: two strings are lexical; mixed string/number is ToNumber. NaN comparisons false.
  const JsonScalarCell five_text{Any(flight::String("5"))};
  assert(thirty_text < five_text);          // "30" < "5" lexically true
  assert(!(thirty_text < two));             // 30 < 2 numerically false
  assert(two < thirty_text);                // 2 < 30 numerically true
  assert(!(absent > 0.0) && !(absent < 0.0));  // NaN comparisons false

  // null/undefined/NaN in `+`: null is 0 numerically, undefined/NaN stay NaN.
  const JsonScalarCell null_value{Any(flight::null)};
  const JsonScalarCell undefined_value;
  const JsonScalarCell nan_value{Any(std::numeric_limits<double>::quiet_NaN())};
  assert((null_value + two).to_number() == 2.0);
  assert(std::isnan((undefined_value + two).to_number()));
  assert(std::isnan((nan_value + two).to_number()));
  assert(fr.type_of() == "number" && !fr.is_nullish() && fr.to_boolean());

  // typeof observes the RAW value, not the declared type.
  assert(bad.type_of() == "string" && std::isnan(bad.to_number()));
  assert(absent.type_of() == "undefined" && absent.is_absent());

  // Native-double boundary: an actual number passes.
  assert(native_double(fr.as_native_number()) == 30.0);

  // Wrong-typed and absent values reach the native boundary and fail there with the dedicated error.
  for (const JsonScalarCell* cell : {&bad, &absent}) {
    bool escaped = false;
    try {
      (void)native_double(cell->as_native_number());
    } catch (const flight::NativeScalarEscapeError&) {
      escaped = true;
    }
    assert(escaped);
  }

  std::puts("JSON scalar vertical slice: PASS");
  return 0;
}
