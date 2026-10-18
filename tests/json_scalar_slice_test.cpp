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

  // Arithmetic on a proven JSON number property: explicit ToNumber, source evaluation order intact.
  const double duration = (100.0 - 10.0) / fr.to_number();
  assert(duration == 3.0);
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
