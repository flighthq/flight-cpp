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

namespace {

// A structural row over the parsed JSON owner, as the Lottie receiver is.
void test_row_receiver_projection() {
  const auto parsed = flight::Json::parse(flight::String("{\"fr\":30,\"text\":\"30\"}"));
  const auto properties = flight::named_properties(parsed);
  using AnyRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Any>>>;
  const auto row = AnyRow::from_owner(properties.shared_owner());
  const auto fr = flight::read_json_scalar_row_member(row, flight::String("fr"));
  const auto text = flight::read_json_scalar_row_member(row, flight::String("text"));
  const auto missing = flight::read_json_scalar_row_member(row, flight::String("absent"));
  assert(fr.is_present() && fr.as_native_number() == 30.0);
  assert(text.type_of() == "string" && text.to_number() == 30.0);
  assert(missing.is_absent());
}

} // namespace

namespace {

// (a) REAL JSON.parse-as-structural-row: object root -> lazy row over the same parsed owner; null
// and primitive roots are preserved (nullopt) so the source's own check runs, not an assertion throw.
void test_json_structural_row() {
  using RowSchema = flight::RowReadonly<flight::RowOf<flight::Any>>;
  const auto object = flight::Json::parse(flight::String("{\"fr\":30,\"text\":\"30\"}"));
  const auto row = flight::json_structural_row<RowSchema>(object);
  assert(row.has_value());
  const auto fr = flight::read_json_scalar_row_member(*row, flight::String("fr"));
  const auto text = flight::read_json_scalar_row_member(*row, flight::String("text"));
  assert(fr.is_present() && fr.as_native_number() == 30.0);
  assert(text.type_of() == "string" && text.to_number() == 30.0);

  const auto null_root = flight::Json::parse(flight::String("null"));
  const auto primitive_root = flight::Json::parse(flight::String("30"));
  assert(!flight::json_structural_row<RowSchema>(null_root).has_value());
  assert(!flight::json_structural_row<RowSchema>(primitive_root).has_value());
}

} // namespace

namespace {

// Assertion carrier: raw root preserved (null vs primitive vs object), member dispatch bounded.
void test_json_assertion_row() {
  using Schema = flight::RowReadonly<flight::RowOf<flight::Any>>;
  using Assertion = flight::JsonAssertionRow<Schema>;

  const Assertion object(flight::Json::parse(flight::String("{\"fr\":30}")));
  assert(object.is_object_root() && !object.is_nullish() && object.type_of() == "object");
  assert(object.read_member(flight::String("fr")).as_native_number() == 30.0);

  const Assertion null_root(flight::Json::parse(flight::String("null")));
  assert(null_root.is_nullish() && null_root.type_of() == "object" && !null_root.to_boolean());
  bool threw = false;
  try {
    (void)null_root.read_member(flight::String("fr"));
  } catch (const flight::TypeError&) {
    threw = true;
  }
  assert(threw);

  const Assertion number_root(flight::Json::parse(flight::String("30")));
  assert(!number_root.is_object_root() && !number_root.is_nullish() &&
         number_root.type_of() == "number" && number_root.to_boolean() &&
         number_root.to_number() == 30.0 &&
         number_root.read_member(flight::String("fr")).is_absent());

  const Assertion string_root(flight::Json::parse(flight::String("\"x\"")));
  assert(string_root.type_of() == "string" && string_root.to_boolean() &&
         string_root.to_number() != string_root.to_number());  // NaN
}

// Relational strings compare UTF-16 code units: a supplementary character's high surrogate (D83D)
// sorts before U+FFFD (FFFD), while UTF-8 byte order would sort U+FFFD first.
void test_json_scalar_utf16_relational() {
  const flight::JsonScalarCell smile{flight::Any(flight::String("\u{1F600}"))};
  const flight::JsonScalarCell replacement{flight::Any(flight::String("\uFFFD"))};
  assert(smile < replacement && !(replacement < smile));
}

} // namespace


namespace {

// The assertion answer AT THE EXISTING ABI: a plain StructuralRef<Schema> for every root kind, with
// root metadata helpers. Object -> parsed-owner row; primitive -> bounded root row; null -> row whose
// member read throws TypeError. No wrapper type crosses params/returns/context fields.
void test_json_assertion_row_adapter() {
  using RowSchema = flight::RowReadonly<flight::RowOf<flight::Any>>;

  const auto object = flight::json_assertion_row<RowSchema>(flight::Json::parse(flight::String("{\"fr\":30}")));
  assert(flight::json_row_type_of(object) == "object" && !flight::json_row_is_nullish(object) &&
         flight::json_row_to_boolean(object));
  assert(flight::read_json_scalar_row_member(object, flight::String("fr")).as_native_number() == 30.0);

  const auto number = flight::json_assertion_row<RowSchema>(flight::Json::parse(flight::String("30")));
  assert(flight::json_row_type_of(number) == "number" && !flight::json_row_is_nullish(number) &&
         flight::json_row_to_boolean(number) &&
         flight::read_json_scalar_row_member(number, flight::String("anything")).is_absent());

  const auto null_root = flight::json_assertion_row<RowSchema>(flight::Json::parse(flight::String("null")));
  assert(flight::json_row_type_of(null_root) == "object" && flight::json_row_is_nullish(null_root) &&
         !flight::json_row_to_boolean(null_root));
  bool threw = false;
  try {
    (void)flight::read_json_scalar_row_member(null_root, flight::String("fr"));
  } catch (const flight::TypeError&) {
    threw = true;
  }
  assert(threw);

  const auto text = flight::json_assertion_row<RowSchema>(flight::Json::parse(flight::String("\"ab\"")));
  assert(flight::json_row_type_of(text) == "string");
  const auto length = flight::read_json_scalar_row_member(text, flight::String("length"));
  const auto first = flight::read_json_scalar_row_member(text, flight::String("0"));
  const auto named = flight::read_json_scalar_row_member(text, flight::String("byteLength"));
  assert(length.is_present() && length.as_native_number() == 2.0);
  assert(first.is_present() && first.to_string() == flight::String("a"));
  assert(named.is_absent());
}

} // namespace

int main() {
  using flight::Any;
  using flight::JsonScalarCell;

  test_row_receiver_projection();
  test_json_structural_row();
  test_json_assertion_row();
  test_json_assertion_row_adapter();
  test_json_scalar_utf16_relational();

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

  // Loose equality: string/string is exact lexical equality, never ToNumber ("01" != "1"),
  // while mixed string/number compares ToNumber ("30" == 30, strict false).
  const JsonScalarCell text_01{Any(flight::String("01"))};
  const JsonScalarCell text_1{Any(flight::String("1"))};
  const JsonScalarCell number_one{Any(1.0)};
  assert(!text_01.loose_equals(text_1) && text_01.strict_equals(text_1) == false);
  assert(text_1.loose_equals(number_one) && !text_1.strict_equals(number_one));
  assert(thirty_text.loose_equals(thirty_text) && !thirty_text.loose_equals(number_one));

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
