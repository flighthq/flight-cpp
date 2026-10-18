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


namespace {

// Wrong-typed parsed-JSON composites in a declared numeric field. The Node oracle for these:
//   typeof object, truthy (even empty); Number([])=0, [30]=30, [1,2]=NaN, {}=NaN, [null]=0;
//   String([])="", [30]="30", [1,2]="1,2", {}="[object Object]", [[1,2],[3]]="1,2,3";
//   []+2="2", [30]+2="302", {}+2="[object Object]2", [1,2]+"x"="1,2x";
//   []<1 true (0<1), [30]<5 false, [30]<"5" true (lexical), {}<1 false (NaN).
void test_json_composite_operations() {
  const auto cell = [](const char* text) {
    return flight::JsonScalarCell{flight::Any(flight::Json::parse(flight::String(text)))};
  };
  const auto empty_array = cell("[]");
  const auto one = cell("[30]");
  const auto two = cell("[1,2]");
  const auto object = cell("{}");
  const auto nested = cell("[[1,2],[3]]");
  const auto null_element = cell("[null]");
  const auto zero = flight::JsonScalarCell{flight::Any(0.0)};
  const auto thirty = flight::JsonScalarCell{flight::Any(30.0)};
  const auto five_text = flight::JsonScalarCell{flight::Any(flight::String("5"))};
  const auto x_text = flight::JsonScalarCell{flight::Any(flight::String("x"))};

  assert(empty_array.type_of() == "object" && empty_array.to_boolean() && object.to_boolean());
  assert(empty_array.to_number() == 0.0 && one.to_number() == 30.0 && std::isnan(two.to_number()) &&
         std::isnan(object.to_number()) && null_element.to_number() == 0.0);
  assert(empty_array.to_string() == flight::String("") && one.to_string() == flight::String("30") &&
         two.to_string() == flight::String("1,2") &&
         object.to_string() == flight::String("[object Object]") &&
         nested.to_string() == flight::String("1,2,3"));
  assert((empty_array + 2.0).to_string() == flight::String("2") &&
         (one + 2.0).to_string() == flight::String("302") &&
         (object + 2.0).to_string() == flight::String("[object Object]2") &&
         (two + x_text).to_string() == flight::String("1,2x"));
  assert((empty_array < 1.0) && !(one < 5.0) && (one < five_text) && !(object < 1.0));
  assert(empty_array.loose_equals(zero) && one.loose_equals(thirty));
}

} // namespace


namespace {

// Same-owner writes into a parsed JSON document carried as a structural row. Every operation mutates
// the one parsed tree: a write through the row is visible through a fresh view of the same value, an
// existing member is replaced in place, a missing key is not fabricated by the view, undefined is
// refused, erase is its own operation, keys keep their exact UTF-16 spelling, and arrays insert,
// replace and erase in place with the same storage identity.
void test_json_row_writes() {
  using RowSchema = flight::RowReadonly<flight::RowOf<flight::Any>>;

  const auto parsed = flight::Json::parse(flight::String("{\"a\":1,\"n\":null}"));
  const auto row = flight::json_assertion_row<RowSchema>(parsed);
  assert(flight::json_row_is_object(row) && !flight::json_row_is_array(row));

  assert(flight::json_row_set(row, flight::String("b"), flight::Any(2.0)));
  const auto view = flight::named_properties(parsed);
  assert(view.has(flight::String("b")) && view.get(flight::String("b")).as_number() == 2.0);

  assert(flight::json_row_set(row, flight::String("a"), flight::Any(flight::String("x"))));
  assert(flight::read_json_scalar_row_member(row, flight::String("a")).to_string() == flight::String("x"));

  assert(flight::json_row_has(row, flight::String("n")) && !flight::json_row_has(row, flight::String("missing")));
  assert(!flight::json_row_set(row, flight::String("u"), flight::Any(flight::undefined)) &&
         !flight::json_row_has(row, flight::String("u")));
  assert(flight::json_row_erase(row, flight::String("n")) && !flight::json_row_has(row, flight::String("n")));

  const flight::String smile = flight::String::from_char_code(0xD83D, 0xDE00);
  assert(flight::json_row_set(row, smile, flight::Any(3.0)) && flight::json_row_has(row, smile));

  const auto array_parsed = flight::Json::parse(flight::String("[10,20]"));
  const auto array_row = flight::json_assertion_row<RowSchema>(array_parsed);
  assert(flight::json_row_is_array(array_row) && flight::json_row_length(array_row) == 2.0);
  assert(flight::json_row_set_index(array_row, 1.0, flight::Any(25.0)) &&
         flight::json_row_index(array_row, 1.0).as_number() == 25.0);
  assert(flight::json_row_insert(array_row, 1.0, flight::Any(15.0)) &&
         flight::json_row_length(array_row) == 3.0 &&
         flight::json_row_index(array_row, 1.0).as_number() == 15.0);
  assert(flight::json_row_erase_index(array_row, 0.0) && flight::json_row_length(array_row) == 2.0);
  assert(flight::named_properties(array_parsed).shared_owner() == array_row.shared_owner());

  const auto nested_parsed = flight::Json::parse(flight::String("{\"meta\":{\"w\":1}}"));
  const auto nested_row = flight::json_assertion_row<RowSchema>(nested_parsed);
  auto meta = flight::json_row_member_view(nested_row, flight::String("meta"));
  assert(meta.has(flight::String("w")));
  assert(meta.set(flight::String("w"), flight::Any(2.0)));
  const auto meta_again = flight::json_row_member_view(nested_row, flight::String("meta"));
  assert(meta_again.get(flight::String("w")).as_number() == 2.0);
}

} // namespace

namespace {

// Any-receiver bridges: nested object/array roots reuse the parsed owner; raw root retained; mutation
// between root construction and discrimination is visible; undefined writes are refused explicitly.
void test_json_any_bridge() {
  using RowSchema = flight::RowReadonly<flight::RowOf<flight::Any>>;

  const auto parsed = flight::Json::parse(flight::String("{\"fr\":30,\"list\":[1,2]}"));
  const flight::Any receiver(parsed);
  const auto row = flight::json_assertion_row<RowSchema>(receiver);
  assert(flight::read_json_scalar_row_member(row, flight::String("fr")).as_native_number() == 30.0);

  const auto raw = flight::json_row_raw(row);
  assert(raw.kind() == flight::AnyKind::object);
  // Same owner: mutate through the recovered root, re-read through the row (live, no copy).
  const auto owner = raw.object_reference_if<flight::JsonObject>();
  assert(static_cast<bool>(owner));
  owner->set(flight::String("fr"), flight::Json::parse(flight::String("31")));
  assert(flight::read_json_scalar_row_member(row, flight::String("fr")).as_native_number() == 31.0);
  owner->set(flight::String("added"), flight::Json::parse(flight::String("true")));
  assert(flight::Json::stringify(raw).to_utf8().find("\"added\":true") != std::string::npos);

  // Undefined write refusal: false, no silent delete/alter, no source-shaped error.
  const auto properties = flight::named_properties(row);
  const bool wrote = properties.set(flight::String("fr"), flight::Any());
  assert(!wrote && owner->get(flight::String("fr"))->as_number() == 31.0);

  // Primitive root through the Any bridge; raw returns the same value; member read is absent.
  const flight::Any primitive(30.0);
  const auto primitive_row = flight::json_assertion_row<RowSchema>(primitive);
  assert(flight::read_json_scalar_row_member(primitive_row, flight::String("x")).is_absent());
  assert(flight::json_row_raw(primitive_row).as_number() == 30.0);

  // Undefined root (Any default): nullish like null, typeof undefined, direct member read throws
  // TypeError at the read, while the ?. lowering can test is_nullish first and yield undefined.
  const flight::Any undefined_root;
  const auto undefined_row = flight::json_assertion_row<RowSchema>(undefined_root);
  assert(flight::json_row_is_nullish(undefined_row));
  assert(flight::json_row_type_of(undefined_row) == "undefined");
  assert(!flight::json_row_to_boolean(undefined_row));
  {
    bool undefined_threw = false;
    try {
      (void)flight::read_json_scalar_row_member(undefined_row, flight::String("x"));
    } catch (const flight::TypeError&) {
      undefined_threw = true;
    }
    assert(undefined_threw);
  }

  // Null root: raw retained as null; member read throws TypeError at the read.
  const flight::Any null_root(flight::null);
  const auto null_row = flight::json_assertion_row<RowSchema>(null_root);
  assert(flight::json_row_raw(null_row).is_null());
  bool threw = false;
  try {
    (void)flight::read_json_scalar_row_member(null_row, flight::String("x"));
  } catch (const flight::TypeError&) {
    threw = true;
  }
  assert(threw);
}

} // namespace


namespace {

// Two conversion cases the Node oracle distinguishes and the runtime must match.
//   Number("\u00A030\u00A0") is 30: ECMAScript trims its own whitespace set, not ASCII isspace.
//   Number({"toString":3}) throws TypeError: an own non-callable toString shadows the prototype and
//   OrdinaryToPrimitive cannot complete; the same happens inside an array join.
void test_json_conversion_edges() {
  const auto cell = [](const char* text) {
    return flight::JsonScalarCell{flight::Any(flight::Json::parse(flight::String(text)))};
  };
  const auto spaced = cell("\"\u00A030\u00A0\"");
  assert(spaced.to_number() == 30.0);
  const auto wide = cell("\"\u300012\uFEFF\"");
  assert(wide.to_number() == 12.0);

  const auto shadow = cell("{\"toString\":3}");
  bool threw = false;
  try {
    (void)shadow.to_number();
  } catch (const flight::TypeError&) {
    threw = true;
  }
  assert(threw);
  threw = false;
  try {
    (void)shadow.to_string();
  } catch (const flight::TypeError&) {
    threw = true;
  }
  assert(threw);

  const auto joined = cell("[{\"toString\":3}]");
  threw = false;
  try {
    (void)joined.to_string();
  } catch (const flight::TypeError&) {
    threw = true;
  }
  assert(threw);

  const auto plain = cell("{}");
  assert(std::isnan(plain.to_number()) && plain.to_string() == flight::String("[object Object]"));
}

} // namespace


namespace {

// A same-owner JSON array reaching a member write. Any(JsonArray) is kind object, so the conversion
// must try the array owner before answering "not representable": object -> array replacement is a
// faithful mutation, not an out-of-profile write.
void test_json_member_array_replacement() {
  using RowSchema = flight::RowReadonly<flight::RowOf<flight::Any>>;
  const auto parsed = flight::Json::parse(flight::String("{\"frames\":{}}"));
  const auto row = flight::json_assertion_row<RowSchema>(parsed);

  const auto empty_array = flight::Any(flight::JsonValue(flight::JsonArray{}));
  assert(flight::json_row_set(row, flight::String("frames"), empty_array));
  const auto member = flight::json_row_member(row, flight::String("frames"));
  assert(member.is_array() && member.array_size() == 0);

  const auto object = flight::Any(flight::JsonValue(flight::JsonObject{}));
  assert(flight::json_row_set(row, flight::String("meta"), object));
  assert(flight::json_row_is_object(row) && flight::json_row_has(row, flight::String("meta")));

  // A represented nested array used again keeps the exact owner it was recovered from.
  const auto nested = flight::json_row_member(row, flight::String("frames"));
  assert(flight::json_row_set(row, flight::String("other"), nested));
  const auto other = flight::json_row_member(row, flight::String("other"));
  assert(other.is_array() && other.array_size() == 0);
}

} // namespace


namespace {

// Native owners reach the same raw/recovery helpers through the owner seam: a native object row
// boxes its exact live object, and a native Array owner is recognized as an array with its real
// length and elements -- no guessed cast and no copy.
void test_json_native_owner_raw() {
  struct NativeDoc : flight::ReferenceEnabled {
    flight::Array<double> items;
  };
  const auto doc = flight::make_ref<NativeDoc>();
  doc->items = flight::Array<double>{1.0, 2.0};
  // Emitted code binds its generated member table; a hand-written test type binds the member here.
  {
    auto owner = flight::detail::owner_for(doc);
    owner->bind_named("items", [doc]() -> decltype(auto) { return (doc->items); });
  }
  using DocRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<NativeDoc>>>>;
  const auto row = DocRow(doc);
  const auto raw = flight::json_row_raw(row);
  assert(raw.kind() == flight::AnyKind::object && !raw.is_array());
  assert(flight::json_row_is_object(row) && !flight::json_row_is_array(row));
  assert(flight::json_row_has(row, flight::String("items")));
  assert(flight::json_row_member(row, flight::String("items")).is_array());

  const auto array = std::make_shared<flight::Array<double>>(flight::Array<double>{3.0, 4.0});
  using ArrayRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<std::shared_ptr<flight::Array<double>>>>>;
  const auto array_row = ArrayRow(array);
  assert(flight::json_row_is_array(array_row) && flight::json_row_length(array_row) == 2.0);
  assert(flight::json_row_index(array_row, 1.0).as_number() == 4.0);
  assert(flight::json_row_raw(array_row).identity() == flight::Any(*array).identity());
}

} // namespace

namespace {

// Number(Symbol) throws TypeError; explicit String(symbol) stays available; implicit concat throws.
void test_json_scalar_symbol() {
  const flight::JsonScalarCell symbol{flight::Any(flight::Symbol(flight::String("s")))};
  assert(symbol.type_of() == "symbol");
  bool number_threw = false;
  try {
    (void)symbol.to_number();
  } catch (const flight::TypeError&) {
    number_threw = true;
  }
  assert(number_threw);
  bool plus_number_threw = false;
  try {
    (void)(symbol + 1.0);
  } catch (const flight::TypeError&) {
    plus_number_threw = true;
  }
  assert(plus_number_threw);
  bool plus_string_threw = false;
  try {
    (void)(symbol + flight::JsonScalarCell{flight::Any(flight::String(""))});
  } catch (const flight::TypeError&) {
    plus_string_threw = true;
  }
  assert(plus_string_threw);
  bool relational_threw = false;
  try {
    (void)(symbol < 1.0);
  } catch (const flight::TypeError&) {
    relational_threw = true;
  }
  assert(relational_threw);
  // Explicit String(symbol) is distinct from the implicit conversions above.
  assert(symbol.to_string() == flight::String("Symbol(s)"));
}

} // namespace

int main() {
  using flight::Any;
  using flight::JsonScalarCell;

  test_row_receiver_projection();
  test_json_structural_row();
  test_json_assertion_row();
  test_json_assertion_row_adapter();
  test_json_composite_operations();
  test_json_row_writes();
  test_json_conversion_edges();
  test_json_member_array_replacement();
  test_json_native_owner_raw();
  test_json_scalar_utf16_relational();
  test_json_any_bridge();
  test_json_scalar_symbol();

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
