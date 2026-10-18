// Row-projection behaviour against the committed generated member table, which is what emitted code
// actually resolves named members through. The runtime test cannot cover this: it links the runtime
// alone, so `generated_row_member_t` is void there and no named member resolves.
#include <flight/runtime.hpp>

#include <flight/json.hpp>
#include <flight/types/ambient_light.hpp>
#include <flight/types/ambient_light_options.hpp>

#include <algorithm>
#include <concepts>
#include <functional>
#include <variant>
#include <vector>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <typeinfo>

namespace {

// A base and a derived record with native heritage, so make_ref records the derived allocation.
struct TestProvenanceBase : public flight::ReferenceEnabled {
  double radius = 0.0;
};
struct TestProvenanceDerived : public TestProvenanceBase {
  using flight_native_base = TestProvenanceBase;
  double intensity = 1.0;
};

// A class-like aggregate with a prototype method and accessor, registered as the emitter registers an
// emitted class, plus a base that an emitted class extends.
struct TestPrototypeClass : public flight::ReferenceEnabled {
  double value = 1.0;
  double twice() const { return value * 2.0; }
};
struct TestPrototypeBase : public flight::ReferenceEnabled {};
inline const bool test_prototype_class_registered = flight::register_class_prototype<TestPrototypeClass>(
    flight::ClassPrototype{{"twice"}, {"size"}, std::nullopt, false});
inline const bool test_prototype_base_registered = flight::register_class_prototype<TestPrototypeBase>(
    flight::ClassPrototype{{"shared"}, {}, std::nullopt, true});

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

// The shape that had nothing to lower onto: a host provider whose capability members are optional,
// reached through a projection that says they are present.
// `subscribe` is a key the committed member table carries; `unsubscribe` is not, which is itself
// worth knowing -- the table only contains keys the emitted SDK actually projects.
struct TestClipboardChangeProvider final : public flight::ReferenceEnabled {
  std::optional<std::function<double(double)>> subscribe;
};

// The compiler's entity-construction lane: an empty bag that is filled and then cast to the
// nominal type it was standing in for. The bag has no members, which is exactly why the cast has to
// mint the object rather than reinterpret one.
struct TestConstructionBag final : public flight::ReferenceEnabled {};

struct TestFinishedLight final : public flight::ReferenceEnabled {
  double intensity{};
  bool enabled{};
};

// A populated object is never replaced: two unrelated objects that both carry state are a different
// question from a bag with none.
struct TestPopulatedSource final : public flight::ReferenceEnabled {
  double intensity{};
};

// The widening pair, in the shape the SDK has it: `GlTextureRenderTarget extends GlRenderTarget`
// flattens to two separate C++ structs, so the relationship has to be proven from the members
// rather than read off a base class.
struct TestBaseTarget final : public flight::ReferenceEnabled {
  double width{};
  double height{};
};

struct TestDerivedTarget final : public flight::ReferenceEnabled {
  double width{};
  double height{};
  double intensity{};
};

struct TestUnrelatedTarget final : public flight::ReferenceEnabled {
  double color{};
};

// An Array-valued member, to pin what a row WRITE does to array identity. `advances` is a key the
// committed table carries; the table dispatches on the key name and returns whatever member the subject
// declares, so the member's type is ours to choose.
struct TestArrayBearing final : public flight::ReferenceEnabled {
  flight::Array<double> advances;
};

// The absence-channel shape: `clip: ClipRegion | null` on a possibly-absent property, which is three
// TypeScript states reaching C++ as one `std::optional<std::shared_ptr<...>>`.
struct TestClipRegion final : public flight::ReferenceEnabled {
  double radius{};
};

struct TestClipBearing final : public flight::ReferenceEnabled {
  std::optional<flight::Ref<TestClipRegion>> clip;
};

// A present reference cell viewed through a readonly nullable schema, as with a texture target's
// framebuffer viewed through GlRenderTarget. The original concrete cell remains non-optional.
struct TestPresentClipBearing final : public flight::ReferenceEnabled {
  flight::Ref<TestClipRegion> clip;
};
struct TestNestedClipBearing final : public flight::ReferenceEnabled {
  std::optional<std::optional<flight::Ref<TestClipRegion>>> clip;
};

// A SECOND subject widening onto the same base. The SDK's motivating case is a union -- an audio
// reference is embedded or external -- where both alternatives declare the key the common row wants,
// so a row over that shape has to be satisfiable by either one. The proof is computed from declared
// keys, so this needs no further declaration to be provable against `TestBaseTarget`.
struct TestOtherDerivedTarget final : public flight::ReferenceEnabled {
  double width{};
  double height{};
  flight::String label;
};

// Same key names, different types: a match by spelling alone is not assignability.
struct TestRetypedTarget final : public flight::ReferenceEnabled {
  flight::String width;
  double height{};
};

using BaseTargetRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<TestBaseTarget>>>>;
using DerivedTargetRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<TestDerivedTarget>>>>;
using WritableBaseTargetRow = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestBaseTarget>>>>;
using WritableDerivedTargetRow = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestDerivedTarget>>>>;
using UnrelatedTargetRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<TestUnrelatedTarget>>>>;
using RetypedTargetRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<TestRetypedTarget>>>>;

// A nullable structural member uses the optional itself as its null channel. Unlike a shared pointer,
// StructuralRef has no inner null value, so an emitted `nullptr` write has to disengage the optional.
struct TestNullableRowBearing final : public flight::ReferenceEnabled {
  std::optional<BaseTargetRow> subscribe;
};

// Derived to base, and nothing else. The trait the runtime declares answers no by default and the
// generated table specializes it to yes only where the proof holds, so each of these is a statement
// about which half answered.
static_assert(flight::detail::generated_row_widening_proven_v<TestBaseTarget, TestDerivedTarget>);
static_assert(!flight::detail::generated_row_widening_proven_v<TestDerivedTarget, TestBaseTarget>);
static_assert(!flight::detail::generated_row_widening_proven_v<TestBaseTarget, TestUnrelatedTarget>);
static_assert(!flight::detail::generated_row_widening_proven_v<TestUnrelatedTarget, TestBaseTarget>);
static_assert(!flight::detail::generated_row_widening_proven_v<TestBaseTarget, TestRetypedTarget>);
static_assert(flight::detail::generated_row_widening_proven_v<TestBaseTarget, TestOtherDerivedTarget>);
static_assert(!flight::detail::generated_row_widening_proven_v<TestOtherDerivedTarget, TestDerivedTarget>,
              "two subjects that both widen onto a base do not thereby widen onto each other");

// A type the generated table has no keys for falls to the conservative primary rather than failing
// to compile, which is what makes an absent or older table safe rather than fatal. `width` is a key
// the SDK uses; `only_owner_knows_this` is not, so this subject proves nothing against anything --
// including itself, because the proof requires at least one matching key.
struct TestOutsideTheKeySet final : public flight::ReferenceEnabled {
  double only_owner_knows_this{};
};
static_assert(!flight::detail::GeneratedRowWidening<TestBaseTarget, int>::value);
static_assert(!flight::detail::generated_row_widening_proven_v<int, double>);
static_assert(!flight::detail::generated_row_widening_proven_v<TestOutsideTheKeySet, TestOutsideTheKeySet>,
              "a subject with no key in the table proves nothing, not even against itself");

// EXACT-OWNER views are answered by the same-subject rule before the trait is reached, so a
// readonly view of the owner's own type works on the DEFAULT-FALSE path -- which the subject above
// is chosen to sit on.
using ExactWritable = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestOutsideTheKeySet>>>>;
using ExactReadonly = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<TestOutsideTheKeySet>>>>;
static_assert(std::convertible_to<ExactWritable, ExactReadonly>,
              "the exact-owner readonly view converts without any widening proof");
static_assert(!std::convertible_to<ExactReadonly, ExactWritable>,
              "and still does not gain mutation by doing so");

static_assert(std::convertible_to<DerivedTargetRow, BaseTargetRow>,
              "a derived subject satisfies the base row");
static_assert(!std::convertible_to<BaseTargetRow, DerivedTargetRow>,
              "a base subject cannot satisfy the derived row, which asks for more");
static_assert(!std::convertible_to<UnrelatedTargetRow, BaseTargetRow>,
              "unrelated rows are not convertible in either direction");
static_assert(!std::convertible_to<BaseTargetRow, UnrelatedTargetRow>);
static_assert(!std::convertible_to<RetypedTargetRow, BaseTargetRow>,
              "matching key names at different types is not assignability");

// COMPUTED CELLS. A cell is a member reached through a `Symbol` rather than through a string key,
// so no `RowKey` names it and the generated key table cannot see it. These four subjects agree on
// `width` and `height` at the same type -- everything the key table looks at -- and differ only
// behind `Symbol.for('EntityRuntime')`. Before this, all of them widened onto each other, and the
// widened row then answered `row_has(EntityRuntime)` with false over an object whose cell was
// engaged: same object, same owner, opposite answer.
//
// Two different fixes, for two different halves of that. A cell only one subject declares is FINE
// and still widens -- the cell is declared optional in the source, `{x, y, width, height}` really
// is a `Readonly<Rectangle>`, and the owner-bound cell now answers honestly either way. A cell
// both subjects declare at DIFFERENT types is refused, because the owner holds one cell of one
// type and the other row's read would be handed a default instead of the value on the object.
struct TestRuntimeSlotA final : public flight::ReferenceEnabled {
  double tag{};
};

struct TestRuntimeSlotB final : public flight::ReferenceEnabled {
  double tag{};
};

struct TestCellBearing final : public flight::ReferenceEnabled {
  double width{};
  double height{};
  std::optional<flight::Ref<TestRuntimeSlotA>> entity_runtime_key;
};

struct TestCellBearingPeer final : public flight::ReferenceEnabled {
  double width{};
  double height{};
  std::optional<flight::Ref<TestRuntimeSlotA>> entity_runtime_key;
};

struct TestCellRetyped final : public flight::ReferenceEnabled {
  double width{};
  double height{};
  std::optional<flight::Ref<TestRuntimeSlotB>> entity_runtime_key;
};

template <typename Subject>
using CellRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Subject>>>>;

// Both halves of the contract refuse independently, and each is asserted on its own so a
// regression in one cannot hide behind the other. The runtime compares the cells it can reach
// through a Symbol; the generated table compares every cell the emitted SDK declares, including
// ones the runtime cannot yet reach.
// Both halves of the contract refuse the type disagreement independently, and each is asserted on
// its own so a regression in one cannot hide behind the other. The runtime compares the cells it
// can reach through a Symbol; the generated table compares every cell the emitted SDK declares,
// including ones the runtime cannot yet reach.
static_assert(!flight::detail::computed_cells_agree<TestCellBearing, TestCellRetyped>,
              "the runtime's own comparison refuses two cells at different types");
static_assert(!flight::detail::generated_row_widening_proven_v<TestCellRetyped, TestCellBearing>,
              "and so does the generated table");
static_assert(!std::convertible_to<CellRow<TestCellBearing>, CellRow<TestCellRetyped>>,
              "so the conversion is refused");
static_assert(!std::convertible_to<CellRow<TestCellRetyped>, CellRow<TestCellBearing>>,
              "in both directions");

static_assert(flight::detail::computed_cells_agree<TestBaseTarget, TestCellBearing>,
              "a cell only one subject declares is not a disagreement");
static_assert(std::convertible_to<CellRow<TestCellBearing>, BaseTargetRow>,
              "so it still widens -- this is Adjustment read as its own anonymous {kind} bag");
static_assert(std::convertible_to<CellRow<TestCellBearing>, CellRow<TestCellBearingPeer>>,
              "and subjects that agree about the cell widen as before");

// The exact-owner path is answered before the cells are consulted, so a cell-bearing subject keeps
// its own readonly view.
static_assert(std::convertible_to<flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestCellBearing>>>>,
                                  CellRow<TestCellBearing>>,
              "a cell-bearing subject still has a readonly view of itself");

// Readonly widening must not grant mutation the source refused.
static_assert(std::convertible_to<WritableDerivedTargetRow, BaseTargetRow>,
              "a writable derived row may be read as a readonly base row");
static_assert(!std::convertible_to<DerivedTargetRow, WritableBaseTargetRow>,
              "a readonly row does not become writable by widening");
static_assert(!std::convertible_to<BaseTargetRow, WritableBaseTargetRow>,
              "nor by projecting the same subject");

// `x as Partial<T>` asks nothing of x: a readonly partial row reads every member as an optional and
// answers an absent one for a key the subject does not declare. A writable partial row is a
// different claim and stays rejected -- a write for a member the subject lacks would land in cell
// storage the object never reads.
using ReadonlyPartialTargetRow =
    flight::StructuralRef<flight::RowReadonly<flight::RowPartial<flight::RowOf<flight::Ref<TestBaseTarget>>>>>;
using WritablePartialTargetRow =
    flight::StructuralRef<flight::RowWritable<flight::RowPartial<flight::RowOf<flight::Ref<TestBaseTarget>>>>>;
static_assert(std::convertible_to<UnrelatedTargetRow, ReadonlyPartialTargetRow>,
              "an unrelated subject may still be probed through a readonly partial row");
static_assert(!std::convertible_to<UnrelatedTargetRow, WritablePartialTargetRow>,
              "but never through a writable one");

// hostExplain-style capability objects. `explainHost` enumerates a host, keeps the members that are
// objects, and then enumerates each of those for its filled slots -- all without knowing either
// type. These stand in for that shape.
//
// The slot names are declared width, color, alpha on purpose: alphabetically they are alpha, color,
// width, so a view that reported binding order rather than declaration order would be caught here.
struct TestExplainCapability final : public flight::ReferenceEnabled {
  std::optional<std::function<double(double)>> subscribe;
};

struct TestExplainSlots final : public flight::ReferenceEnabled {
  std::optional<flight::Ref<TestExplainCapability>> width;
  std::optional<flight::Ref<TestExplainCapability>> color;
  std::optional<flight::Ref<TestExplainCapability>> alpha;
};

struct TestExplainHost final : public flight::ReferenceEnabled {
  flight::Ref<TestExplainSlots> slots;
  double intensity{};
};

// A member whose type the runtime has no erased reading of.
struct TestUnrepresentedMember final : public flight::ReferenceEnabled {
  std::vector<double> value;
  double height{};
};

// A source union whose alternatives all have exact erased language values. Material.name uses this
// carrier, and cloneMaterial reaches it through the same dynamic named read/write path.
struct TestRepresentedUnionMember final : public flight::ReferenceEnabled {
  std::variant<flight::String, flight::Null, flight::Undefined> name;
};

// Asked as a template so the answer is a constraint rather than a hard error on a known type.
template <typename View>
concept writes_named_properties =
    requires(View& view, flight::String key) { view.set(key, flight::Any()); };

using Subject = flight::Ref<TestClipboardChangeProvider>;
using SubjectRow = flight::RowOf<Subject>;
using RequiredView = flight::StructuralRef<flight::RowRequired<SubjectRow>>;
using PartialView = flight::StructuralRef<flight::RowPartial<SubjectRow>>;
using ReadonlyRequiredView = flight::StructuralRef<flight::RowReadonly<flight::RowRequired<SubjectRow>>>;
using WritableView = flight::StructuralRef<flight::RowWritable<SubjectRow>>;

using SubscribeKey = flight::RowKey<"subscribe">;

// A required row reads the member as its value; a partial row reads it as an optional. The subject's
// own storage is the same optional in both cases.
static_assert(std::same_as<flight::detail::schema_member_t<SubscribeKey, flight::RowRequired<SubjectRow>>,
                           std::function<double(double)>>);
static_assert(std::same_as<flight::detail::schema_member_t<SubscribeKey, flight::RowPartial<SubjectRow>>,
                           std::optional<std::function<double(double)>>>);
static_assert(std::same_as<flight::detail::schema_member_t<SubscribeKey, SubjectRow>,
                           std::optional<std::function<double(double)>>>);

// Required is the inverse of partial rather than its absence, so it overrides an inner partial row.
static_assert(std::same_as<
              flight::detail::schema_member_t<SubscribeKey, flight::RowRequired<flight::RowPartial<SubjectRow>>>,
              std::function<double(double)>>);
static_assert(!flight::detail::schema_partial<flight::RowRequired<flight::RowPartial<SubjectRow>>>);
static_assert(flight::detail::schema_required<flight::RowRequired<SubjectRow>>);
static_assert(flight::detail::schema_required<flight::RowReadonly<flight::RowRequired<SubjectRow>>>);
static_assert(flight::detail::schema_readonly<flight::RowReadonly<flight::RowRequired<SubjectRow>>>);
static_assert(!flight::detail::schema_required<SubjectRow>);


// A subject and an anonymous carrier that derives from it -- the shape createShapedRun allocates -- plus an
// unrelated type with the same field. docs/compiler-requests/structural-view-of-a-derived-carrier-is-invisible.md
struct TestRunBase : public flight::ReferenceEnabled {
  double width = 0.0;
};
struct TestRunCarrier : public TestRunBase {
  std::optional<double> extra;
};
struct TestRunUnrelated : public flight::ReferenceEnabled {
  double width = 0.0;
};

void test_derived_carrier_view() {
  using BaseView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<TestRunBase>>>>;
  using CarrierView = flight::StructuralRef<flight::RowOf<flight::Ref<TestRunCarrier>>>;
  using UnrelatedView = flight::StructuralRef<flight::RowOf<flight::Ref<TestRunUnrelated>>>;

  auto carrier = flight::make_ref<TestRunCarrier>();
  carrier->width = 42.0;
  // The carrier enters structural storage as itself first, so its shared owner records the carrier type.
  const CarrierView as_carrier(carrier);

  const flight::Ref<TestRunBase> declared = carrier;
  const BaseView from_declared(declared);
  check(from_declared.shared_object().get() == carrier.get() && from_declared->width == 42.0,
        "a base view built from the carrier held as its declared base type sees the carrier");
  const BaseView from_derived(carrier);
  check(from_derived.shared_object().get() == carrier.get(),
        "a base view built from the derived pointer itself sees the carrier");

  check(from_declared == as_carrier && from_declared.identity() == as_carrier.identity() &&
            from_derived.identity() == as_carrier.identity(),
        "every view of the carrier shares one row owner, so identity is the carrier's");
  carrier->width = 7.0;
  check(from_declared->width == 7.0, "a write to the carrier is visible through the base view, which is not a copy");

  // Views whose pointer does not come from a typed pointer keep the exact-owner check.
  check(!BaseView::from_owner(as_carrier.shared_owner()).shared_object(),
        "a base view rebuilt from the carrier's owner does not cast across owners");
  check(!UnrelatedView::from_owner(as_carrier.shared_owner()).shared_object(),
        "an unrelated type never sees the carrier through its owner");
  check(!from_declared.shared_exact_object<TestRunUnrelated>() &&
            from_declared.shared_exact_object<TestRunCarrier>().get() == carrier.get(),
        "exact recovery still names only the carrier's own type");
  bool converted_back = false;
  try {
    converted_back = flight::structural_ref_cast<flight::Ref<TestRunBase>>(from_declared).get() == carrier.get();
  } catch (const std::bad_cast&) {
    converted_back = false;
  }
  check(converted_back, "a proven base view converts back to a reference to the very carrier");
  check(!BaseView::from_owner(as_carrier.shared_owner()).shared_exact_object<TestRunBase>(),
        "an owner-rebuilt base view still cannot recover a base reference across owners");

  // A base view keeps the carrier alive, like the shared_ptr it was built from, and only while it lives.
  std::weak_ptr<TestRunCarrier> watched;
  std::optional<BaseView> lasting;
  {
    auto scoped = flight::make_ref<TestRunCarrier>();
    scoped->width = 3.0;
    const CarrierView first(scoped);
    watched = scoped;
    lasting.emplace(flight::Ref<TestRunBase>(scoped));
  }
  check(!watched.expired() && (*lasting)->width == 3.0,
        "a base view is the carrier's last holder and keeps it alive");
  lasting.reset();
  check(watched.expired(), "the carrier is released when its last view goes");
}

} // namespace

void check_readonly_nullable_cells() {
  using Clip = flight::Ref<TestClipRegion>;
  using Key = flight::RowKey<"clip">;
  using NullableSchema = flight::RowReadonly<flight::RowOf<flight::Ref<TestClipBearing>>>;
  using NullableView = flight::StructuralRef<NullableSchema>;
  using PresentView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<TestPresentClipBearing>>>>;
  using NestedView = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<TestNestedClipBearing>>>>;
  using WritableNullable = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestClipBearing>>>>;
  static_assert(std::convertible_to<PresentView, NullableView>);
  static_assert(!std::convertible_to<PresentView, WritableNullable>);
  static_assert(!std::convertible_to<NullableView, WritableNullable>);
  static_assert(std::same_as<decltype(flight::row_get<Key>(std::declval<NullableView>())), std::optional<Clip>>);
  static_assert(std::same_as<decltype(flight::row_get<Key>(std::declval<PresentView>())), const Clip&>);

  auto first = flight::make_ref<TestClipRegion>();
  auto second = flight::make_ref<TestClipRegion>();
  first->radius = 3.0;
  second->radius = 7.0;
  std::weak_ptr<TestPresentClipBearing> weak;
  {
    auto owner = flight::make_ref<TestPresentClipBearing>();
    owner->clip = first;
    weak = owner;
    const PresentView present(owner);
    const NullableView widened(present);
    check(widened.identity() == present.identity() && widened.shared_owner() == present.shared_owner(),
          "nullable read widening must retain the identical concrete owner");
    check(flight::row_get<Key>(widened) == std::optional<Clip>{first},
          "a present reference cell reads as an engaged optional of the identical reference");
    auto projection = flight::row_get<Key>(widened);
    projection.reset();
    check(owner->clip == first, "resetting a readonly projection must not write null into the present cell");
    owner->clip = second;
    check(flight::row_get<Key>(widened) == std::optional<Clip>{second},
          "nullable reads must observe updates to the original present cell");
    owner.reset();
    check(!weak.expired() && flight::row_get<Key>(widened).value()->radius == 7.0,
          "the widened view must retain the owner and keep its live member readable");
  }
  check(weak.expired(), "nullable projection must not introduce an owner retention cycle");

  auto exact_owner = flight::make_ref<TestClipBearing>();
  const NullableView exact(exact_owner);
  check(!flight::row_get<Key>(exact), "an empty optional cell must stay empty");
  exact_owner->clip = Clip{};
  const auto null_value = flight::row_get<Key>(exact);
  check(null_value.has_value() && !null_value.value(), "an engaged null reference must stay distinct from absence");
  exact_owner->clip = first;
  check(flight::row_get<Key>(exact) == std::optional<Clip>{first}, "an exact optional must retain reference identity");
  const NestedView nested(exact);
  check(flight::row_get<Key>(nested).value() == std::optional<Clip>{first}, "nested projection lifts only one optional layer");
  exact_owner->clip.reset();
  const auto nested_absent = flight::row_get<Key>(nested);
  check(nested_absent.has_value() && !nested_absent.value(), "a present empty inner optional must remain present at the outer layer");

  bool exact_lookup_rejected = false;
  try { (void)exact.shared_owner()->template named_value<Clip>("clip"); }
  catch (const std::bad_cast&) { exact_lookup_rejected = true; }
  check(exact_lookup_rejected, "exact erased lookup must still reject a different storage shape");
  // Deliberately corrupt a disposable row's cell to prove this is not a permissive erased cast.
  exact.shared_owner()->set_named_cell("clip", std::make_shared<flight::OwnedRowCell<double>>(1.0));
  bool projection_rejected = false;
  try { (void)flight::row_get<Key>(exact); }
  catch (const std::bad_cast&) { projection_rejected = true; }
  check(projection_rejected, "nullable projection must reject unrelated stored types rather than report absence");
}

// Typed row_set(nullopt) is a source undefined ASSIGNMENT (own key, undefined sentinel), while the
// explicit typed delete removes the own key from the same owner.
void test_typed_nullopt_presence_and_delete() {
  using Options = flight::types::AmbientLightOptions;
  const auto options = flight::make_ref<Options>(Options{});
  const flight::StructuralRef<flight::RowOf<flight::Ref<Options>>> row(options);
  flight::mark_constructed_own_named_properties(
      options,
      {flight::ConstructedOwnKey{flight::String("color"), flight::ConstructedKeySentinel::Value}});
  flight::row_set<flight::RowKey<"color">>(row, std::nullopt);
  auto properties = flight::named_properties(row);
  check(properties.has(flight::String("color")) &&
            properties.get(flight::String("color")).is_undefined(),
        "typed row_set(nullopt) keeps the key own with the undefined sentinel");
  check(properties.keys() == std::vector<flight::String>{flight::String("color")},
        "typed undefined assignment keeps the authored own-key order");
  check(flight::row_delete<flight::RowKey<"color">>(row) &&
            !flight::named_properties(row).has(flight::String("color")),
        "typed delete removes the own key on the same owner");
}

int main() {
  check_readonly_nullable_cells();
  test_typed_nullopt_presence_and_delete();
  test_derived_carrier_view();
  // A Partial<T> object literal is an option bag: its supplied field is present, its omitted field
  // is absent, and constructing it must not collide optional row storage with T's bare native
  // members. This is the exact shape used by generated material factory options.
  {
    const auto options = flight::make_structural_ref<
        flight::RowReadonly<flight::RowPartial<
            flight::RowOf<flight::Ref<TestBaseTarget>>>>>(
        flight::row_field<flight::RowKey<"width">>(
            std::optional<double>{3.0}));
    check(!options.shared_exact_object<TestBaseTarget>(),
          "a partial object literal stays an option bag rather than allocating its subject");
    check(flight::row_has<flight::RowKey<"width">>(options) &&
              !flight::row_has<flight::RowKey<"height">>(options),
          "a partial object literal distinguishes supplied and omitted properties");
    check(flight::row_get<flight::RowKey<"width">>(options) ==
              std::optional<double>{3.0},
          "a supplied partial field keeps its optional storage shape");

    const auto sparse_optional_options = flight::make_structural_ref<
        flight::RowReadonly<flight::RowPartial<SubjectRow>>>();
    const flight::StructuralRef<flight::RowReadonly<SubjectRow>> optional_view =
        sparse_optional_options;
    check(optional_view.identity() == sparse_optional_options.identity() &&
              !flight::row_get<SubscribeKey>(optional_view).has_value(),
          "an omitted optional property stays absent when the sparse options owner is read "
          "through its ordinary row");
  }

  auto provider = flight::make_ref<TestClipboardChangeProvider>();
  const RequiredView required(provider);
  const PartialView partial(provider);

  check(!flight::row_has<SubscribeKey>(required),
        "a required member that holds no value is not present");
  check(flight::row_has<SubscribeKey>(partial),
        "the same member is present on a partial row, which reads it as an absent optional");
  check(!flight::row_get<SubscribeKey>(partial).has_value(),
        "a partial row reads the unset member as an empty optional");

  bool absent_read_reported = false;
  try {
    static_cast<void>(flight::row_get<SubscribeKey>(required));
  } catch (const std::out_of_range&) {
    absent_read_reported = true;
  }
  check(absent_read_reported,
        "reading a required member that holds no value reports it instead of handing back an "
        "empty optional to invoke");

  int subscribe_calls = 0;
  provider->subscribe = [&](double weight) {
    ++subscribe_calls;
    return weight * 2.0;
  };

  check(flight::row_has<SubscribeKey>(required), "a required member holding a value is present");
  check(flight::row_get<SubscribeKey>(required)(21.0) == 42.0 && subscribe_calls == 1,
        "a required member is invoked as the callable rather than as the optional holding it");
  check(flight::row_get<SubscribeKey>(partial).has_value(),
        "the partial projection of the same member sees the same value");

  // Writing through a required row keeps the subject's own storage shape.
  flight::row_set<SubscribeKey>(required, std::function<double(double)>([](double weight) {
                                  return weight + 1.0;
                                }));
  check(provider->subscribe.has_value() && (*provider->subscribe)(1.0) == 2.0,
        "a write through a required row lands in the optional the subject declared");
  check(flight::row_get<SubscribeKey>(required)(1.0) == 2.0,
        "the required row reads back what was written through it");

  const ReadonlyRequiredView readonly = required;
  static_assert(std::is_const_v<std::remove_reference_t<decltype(flight::row_get<SubscribeKey>(
                    std::declval<const ReadonlyRequiredView&>()))>>,
                "a readonly required row yields a const reference");
  check(readonly == required && readonly.shared_owner() == required.shared_owner(),
        "a readonly projection of a required row keeps one row owner");
  check(flight::row_get<SubscribeKey>(readonly)(2.0) == 3.0,
        "a readonly required row reads the value the writable projection wrote");

  const auto recovered = required.shared_object();
  check(recovered == provider, "a required row recovers the subject it projects");

  // A `set` trap over a plain named key. The Entity runtime slot is a computed symbol, but a guard
  // over a declared field -- `prop === 'binding'` in createGuardedEntityRuntime -- names the key
  // directly, and that is a separate property space from a symbol of the same spelling.
  auto guarded_subject = flight::make_ref<TestClipboardChangeProvider>();
  const WritableView writable(guarded_subject);
  int named_trap_calls = 0;
  const auto named_guarded = flight::make_structural_write_proxy<WritableView::schema_type>(
      writable, std::string("subscribe"), [&] { ++named_trap_calls; });

  check(named_guarded != writable,
        "a named-key write proxy is its own reference, as a JavaScript Proxy is its own object");
  check(named_guarded.shared_object() == guarded_subject &&
            writable.shared_object() == guarded_subject,
        "the proxy reaches the very object it proxies rather than minting a second one");

  flight::row_set<SubscribeKey>(named_guarded, std::function<double(double)>([](double weight) {
                                  return weight * 3.0;
                                }));
  check(named_trap_calls == 1,
        "a named-key write proxy reports the write it intercepts");
  check(guarded_subject->subscribe.has_value() && (*guarded_subject->subscribe)(2.0) == 6.0,
        "the intercepted write is forwarded to the subject unchanged");
  const auto forwarded = flight::row_get<SubscribeKey>(writable);
  check(forwarded.has_value() && (*forwarded)(2.0) == 6.0,
        "the write is visible through the unproxied projection of the same subject");

  // The two key spaces stay separate in both directions.
  const auto subscribe_symbol = flight::Symbol::for_key("subscribe");
  flight::row_set(named_guarded, subscribe_symbol, 7);
  check(named_trap_calls == 1,
        "a named interception does not fire for a symbol key of the same spelling");
  check(flight::row_get<int>(writable, subscribe_symbol) == 7,
        "the symbol write still reaches the shared row storage");

  int symbol_trap_calls = 0;
  const auto symbol_guarded = flight::make_structural_write_proxy<WritableView::schema_type>(
      writable, subscribe_symbol, [&] { ++symbol_trap_calls; });
  flight::row_set<SubscribeKey>(symbol_guarded, std::function<double(double)>([](double weight) {
                                  return weight;
                                }));
  check(symbol_trap_calls == 0,
        "a symbol interception does not fire for a named key of the same spelling");
  flight::row_set(symbol_guarded, subscribe_symbol, 9);
  check(symbol_trap_calls == 1 && flight::row_get<int>(writable, subscribe_symbol) == 9,
        "the symbol interception fires for its own key and forwards the write");

  // The materializing cast: `const out = {} as EntityConstruction<T>` filled and finished.
  using BagRow = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestConstructionBag>>>>;
  using LightRow = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestFinishedLight>>>>;
  const BagRow bag(flight::make_ref<TestConstructionBag>());
  const auto bag_symbol = flight::Symbol::for_key("EntityRuntime");
  flight::row_set(bag, bag_symbol, 5);
  // A named field cannot be written through the bag's own schema -- the bag declares none, and the
  // emitted lane writes fields only after the cast. Writing the cell directly is how a value can
  // already be in the row when the cast arrives, and it must not be dropped.
  bag.shared_owner()->set_named_value("intensity", 3.0);

  const auto finished_row = flight::structural_ref_cast<LightRow>(bag);
  const auto finished = flight::structural_ref_cast<flight::Ref<TestFinishedLight>>(finished_row);
  check(finished != nullptr,
        "finishing a construction bag yields the object the row stood for, not a null reference");
  check(finished->intensity == 3.0,
        "a field written before the cast lands on the materialized object");
  check(flight::row_get<int>(finished_row, bag_symbol) == 5,
        "a symbol property written before the cast survives materialization");

  flight::row_set<flight::RowKey<"enabled">>(finished_row, true);
  check(finished->enabled,
        "a field written after the cast goes to the object's own member");
  flight::row_set<flight::RowKey<"intensity">>(finished_row, 4.0);
  check(finished->intensity == 4.0 && flight::row_get<flight::RowKey<"intensity">>(finished_row) == 4.0,
        "the row and the object are one value after materialization, not two that agree");

  // Casting a row that already has an object of the target type is unchanged: no new object.
  const auto same = flight::structural_ref_cast<LightRow>(finished_row);
  check(same.shared_object() == finished,
        "casting a row that already holds the target object returns that object");

  // A populated source is not a bag, so nothing is minted: the cast is rejected outright rather
  // than replacing an object that holds state or handing back a null reference.
  using PopulatedRow = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestPopulatedSource>>>>;
  static_assert(flight::detail::row_materializes_from<TestFinishedLight, TestConstructionBag>);
  static_assert(!flight::detail::row_materializes_from<TestFinishedLight, TestPopulatedSource>);
  static_assert(!flight::detail::row_materializes_from<TestFinishedLight, TestFinishedLight>);
  const PopulatedRow populated(flight::make_ref<TestPopulatedSource>());
  check(populated.shared_object() != nullptr,
        "a row over a populated object keeps the object it was built from");

  // A widened row reads the REAL derived object, not a copy of it.
  auto derived_target = flight::make_ref<TestDerivedTarget>();
  derived_target->width = 1920.0;
  derived_target->height = 1080.0;
  derived_target->intensity = 0.5;
  const WritableDerivedTargetRow derived_row(derived_target);
  const BaseTargetRow widened = derived_row;

  check(flight::row_get<flight::RowKey<"width">>(widened) == 1920.0 &&
            flight::row_get<flight::RowKey<"height">>(widened) == 1080.0,
        "a widened base row reads the derived subject's own properties");
  derived_target->width = 2560.0;
  check(flight::row_get<flight::RowKey<"width">>(widened) == 2560.0,
        "the widened row is a view of the derived object, not a snapshot of it");
  flight::row_set<flight::RowKey<"width">>(derived_row, 3840.0);
  check(derived_target->width == 3840.0 && flight::row_get<flight::RowKey<"width">>(widened) == 3840.0,
        "a write through the derived row is seen through the widened base row");
  check(widened == derived_row && widened.shared_owner() == derived_row.shared_owner(),
        "widening keeps one row owner, so the two rows are one subject");
  check(widened.shared_object() == nullptr,
        "a widened base row does not claim to hold a base object, because there is none");

  const auto recovered_derived =
      flight::structural_ref_cast<flight::Ref<TestDerivedTarget>>(widened);
  check(recovered_derived == derived_target,
        "a widened row can recover its exact retained native owner without a cast or copy");
  bool rejected_wrong_owner = false;
  try {
    static_cast<void>(flight::structural_ref_cast<flight::Ref<TestBaseTarget>>(widened));
  } catch (const std::bad_cast&) {
    rejected_wrong_owner = true;
  }
  check(rejected_wrong_owner,
        "exact owner recovery rejects a structurally compatible but different native owner");

  // Optional wrapping: absence stays absence, and a present row still widens.
  const std::optional<DerivedTargetRow> absent_target;
  const std::optional<BaseTargetRow> widened_absent =
      absent_target.has_value() ? std::optional<BaseTargetRow>(*absent_target) : std::nullopt;
  check(!widened_absent.has_value(), "widening an absent optional row preserves absence");
  const std::optional<DerivedTargetRow> present_target(derived_row);
  const std::optional<BaseTargetRow> widened_present =
      present_target.has_value() ? std::optional<BaseTargetRow>(*present_target) : std::nullopt;
  check(widened_present.has_value() && flight::row_get<flight::RowKey<"width">>(*widened_present) == 3840.0,
        "widening a present optional row keeps the subject and its values");

  // The widened reference keeps the subject alive on its own.
  std::weak_ptr<TestDerivedTarget> observer = derived_target;
  {
    const BaseTargetRow only_reference = derived_row;
    derived_target.reset();
    check(!observer.expired(),
          "a widened reference keeps the derived subject alive for its own lifetime");
    check(flight::row_get<flight::RowKey<"height">>(only_reference) == 1080.0,
          "and the subject is still readable through it");
  }

  // An explicit structural assertion is rebuilt from the shared row owner. The resulting view is
  // still an owning reference, not a borrowed façade whose native object disappears with the source
  // spelling used to create it.
  auto asserted_subject = flight::make_ref<TestDerivedTarget>();
  asserted_subject->width = 73.0;
  using AssertedWritable =
      flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestDerivedTarget>>>>;
  std::optional<AssertedWritable> asserted_source{AssertedWritable(asserted_subject)};
  const DerivedTargetRow asserted_view =
      flight::structural_ref_cast<DerivedTargetRow>(*asserted_source);
  std::weak_ptr<TestDerivedTarget> asserted_observer = asserted_subject;
  asserted_subject.reset();
  asserted_source.reset();
  check(!asserted_observer.expired() &&
            flight::row_get<flight::RowKey<"width">>(asserted_view) == 73.0,
        "a view rebuilt from its row owner keeps the native object alive");

  // Structural views are object identities too. WeakMap must key them by their common owner, keep
  // no strong reference of its own, and compose with nominal references inside an emitted union.
  flight::WeakMap<DerivedTargetRow, int> structural_weak_map;
  structural_weak_map.set(asserted_view, 17);
  const DerivedTargetRow asserted_alias(asserted_view.shared_object());
  check(structural_weak_map.get(asserted_alias).value_or(0) == 17,
        "a WeakMap lookup through another structural view reaches the same owner identity");

  using MixedWeakKey = std::variant<flight::Ref<TestUnrelatedTarget>, DerivedTargetRow>;
  flight::WeakMap<MixedWeakKey, int> mixed_weak_map;
  const MixedWeakKey mixed_key(std::in_place_index<1>, asserted_view);
  mixed_weak_map.set(mixed_key, 29);
  const MixedWeakKey mixed_alias(std::in_place_index<1>, asserted_alias);
  check(mixed_weak_map.get(mixed_alias).value_or(0) == 29,
        "a WeakMap union preserves and resolves its structural-reference alternative");

  DerivedTargetRow::weak_type expired_structural_key;
  {
    auto temporary_subject = flight::make_ref<TestDerivedTarget>();
    const DerivedTargetRow temporary_view(temporary_subject);
    expired_structural_key = temporary_view.weaken();
    check(DerivedTargetRow::lock_weak(expired_structural_key).has_value(),
          "a live structural reference can be locked from its weak identity");
  }
  check(!DerivedTargetRow::lock_weak(expired_structural_key).has_value(),
        "a structural weak identity does not resurrect an expired native object");

  // A computed-symbol subject used to stay alive through TWO generated captures: the native cell
  // accessor and its presence callback. Both must be non-owning once binding is complete.
  std::weak_ptr<TestCellBearing> computed_cell_observer;
  {
    auto computed_cell_subject = flight::make_ref<TestCellBearing>();
    computed_cell_observer = computed_cell_subject;
    const CellRow<TestCellBearing> computed_cell_view(computed_cell_subject);
    check(computed_cell_view.shared_object() == computed_cell_subject,
          "a computed-cell row owns its subject while the view is live");
  }
  check(computed_cell_observer.expired(),
        "computed-cell binding does not retain its native subject after the last view");

  // Native cells retain their member address rather than their object-capturing accessor, so the
  // dynamic property view must be one of the public views that owns the object it can read.
  auto named_subject = flight::make_ref<TestDerivedTarget>();
  named_subject->width = 81.0;
  const auto retained_named_view = flight::named_properties(named_subject);
  std::weak_ptr<TestDerivedTarget> named_observer = named_subject;
  named_subject.reset();
  check(!named_observer.expired() &&
            retained_named_view.get(flight::String("width")).as_number() == 81.0,
        "a named-property view keeps its native member storage alive");

  // Probing an unrelated subject through a readonly partial row answers absence, not an exception.
  const UnrelatedTargetRow unrelated_row(flight::make_ref<TestUnrelatedTarget>());
  const ReadonlyPartialTargetRow probed = unrelated_row;
  check(!flight::row_get<flight::RowKey<"width">>(probed).has_value(),
        "a partial probe of a subject without the key reads absent rather than throwing");
  const ReadonlyPartialTargetRow present_probe = derived_row;
  check(flight::row_get<flight::RowKey<"width">>(present_probe).value_or(0.0) == 3840.0,
        "and reads the value when the subject does declare it");

  // ---- the read-only dynamic named-property view ----

  auto explain_present = flight::make_ref<TestExplainCapability>();
  auto explain_slots = flight::make_ref<TestExplainSlots>();
  explain_slots->width = explain_present;
  explain_slots->alpha = explain_present;
  auto explain_host = flight::make_ref<TestExplainHost>();
  explain_host->slots = explain_slots;
  explain_host->intensity = 4.0;

  const auto host_view = flight::named_properties(explain_host);
  check(host_view.keys() == std::vector<flight::String>({flight::String("slots"),
                                                         flight::String("intensity")}),
        "own string keys come back in source declaration order");

  const auto slot_view = flight::named_properties(explain_slots);
  check(slot_view.keys() == std::vector<flight::String>({flight::String("width"),
                                                         flight::String("color"),
                                                         flight::String("alpha")}),
        "declaration order is reported even where it disagrees with alphabetical binding order");

  // The explainHost walk itself: keep the members that are objects, then report each group's
  // filled slots.
  std::vector<flight::String> filled;
  for (const auto& group_key : host_view.keys()) {
    const auto group = host_view.get(group_key);
    if (group.kind() != flight::AnyKind::object) continue;
    const auto group_slots = flight::named_properties(group.object_if<TestExplainSlots>());
    for (const auto& slot_key : group_slots.keys()) {
      const auto slot = group_slots.get(slot_key);
      if (slot.kind() == flight::AnyKind::undefined || slot.kind() == flight::AnyKind::null) continue;
      filled.push_back(slot_key);
    }
  }
  check(filled == std::vector<flight::String>({flight::String("width"), flight::String("alpha")}),
        "a hostExplain-style walk reaches the filled slots of an enumerated capability group");

  check(slot_view.get(flight::String("color")).kind() == flight::AnyKind::undefined,
        "an empty optional slot reads as undefined, which is what an absent property is");
  check(slot_view.has(flight::String("color")),
        "and the key is still present, which is the question `has` answers");
  check(slot_view.get(flight::String("width")).object_if<TestExplainCapability>() == explain_present,
        "a filled slot reads back as the very capability object it holds");
  check(host_view.get(flight::String("intensity")).kind() == flight::AnyKind::number,
        "a number member reads as a number, so the typeof test can skip it");

  check(!host_view.has(flight::String("absent")) &&
            host_view.get(flight::String("absent")).kind() == flight::AnyKind::undefined,
        "a key that is not there reads as undefined and reports absent");

  // Named string properties and symbol attachments are separate spaces.
  const auto width_symbol = flight::Symbol::for_key("width");
  const flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestExplainSlots>>>>
      slot_row(explain_slots);
  flight::row_set(slot_row, width_symbol, 11);
  check(slot_view.keys().size() == 3,
        "a symbol property does not become an own enumerable string key");
  check(slot_view.get(flight::String("width")).object_if<TestExplainCapability>() == explain_present,
        "and it does not shadow the named property that shares its spelling");
  check(flight::row_has(slot_row, width_symbol) &&
            flight::row_get<int>(slot_row, width_symbol) == 11,
        "while the symbol space still holds its own property under that spelling");

  // The same view, reached from a structural row rather than from the object: one owner, so one set
  // of properties.
  const auto row_view = flight::named_properties(slot_row);
  check(row_view.keys() == slot_view.keys() && row_view.shared_owner() == slot_view.shared_owner(),
        "the view of a row and the view of its subject are the same view");

  // A key written after construction has no member to order against, so it follows the declared
  // ones -- as a property added later does in JavaScript.
  slot_row.shared_owner()->set_named_value("caller-added", flight::String("late"));
  check(slot_view.keys() == std::vector<flight::String>({flight::String("width"),
                                                         flight::String("color"),
                                                         flight::String("alpha"),
                                                         flight::String("caller-added")}),
        "a later-written key follows the declared members");
  check(slot_view.get(flight::String("caller-added")) == flight::Any(flight::String("late")),
        "and reads back the value it was written with");

  // A member the runtime has no erased reading of is reported, not invented.
  auto unrepresented = flight::make_ref<TestUnrepresentedMember>();
  const auto unrepresented_view = flight::named_properties(unrepresented);
  check(unrepresented_view.has(flight::String("value")) &&
            !unrepresented_view.is_represented(flight::String("value")),
        "a member with no erased reading is present but not representable");
  check(unrepresented_view.is_represented(flight::String("height")),
        "while a member beside it still reads");
  bool unrepresented_reported = false;
  try {
    static_cast<void>(unrepresented_view.get(flight::String("value")));
  } catch (const flight::UnrepresentedProperty& reported) {
    unrepresented_reported = reported.key() == "value";
  }
  check(unrepresented_reported, "reading it reports the property rather than inventing a value");

  auto represented_union = flight::make_ref<TestRepresentedUnionMember>();
  const auto represented_union_view = flight::named_properties(represented_union);
  represented_union->name = flight::String("canopy");
  check(represented_union_view.is_represented(flight::String("name")) &&
            represented_union_view.get(flight::String("name")) ==
                flight::Any(flight::String("canopy")),
        "a dynamic read visits the active represented union arm");
  check(represented_union_view.set(flight::String("name"), flight::Any(flight::null)) &&
            std::holds_alternative<flight::Null>(represented_union->name),
        "a dynamic write selects the exact null union arm");
  check(represented_union_view.set(flight::String("name"), flight::Any()) &&
            std::holds_alternative<flight::Undefined>(represented_union->name),
        "a dynamic write preserves undefined separately from null");
  check(!represented_union_view.set(flight::String("name"), flight::Any(1.0)) &&
            std::holds_alternative<flight::Undefined>(represented_union->name),
        "a dynamic write still refuses a value outside every union arm");
  // `row[key]` with a key known only at run time, read as the stated member type.
  unrepresented->height = 7.5;
  const auto keyed_row = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<TestUnrepresentedMember>>>>(unrepresented);
  check(flight::row_get<double>(keyed_row, flight::String("height")) == 7.5, "a runtime key reads the live member");
  unrepresented->height = -1.0;
  check(flight::row_get<double>(keyed_row, flight::String("height")) == -1.0, "and sees a later write to it");
  check(flight::row_get<flight::Any>(keyed_row, flight::String("height")) == flight::Any(-1.0),
        "read as Any, it is the member's erased value");
  bool wrong_type_refused = false;
  try {
    static_cast<void>(flight::row_get<bool>(keyed_row, flight::String("height")));
  } catch (const flight::BadAnyAccess&) {
    wrong_type_refused = true;
  }
  check(wrong_type_refused, "a member of another type is refused, not coerced");
  bool absent_refused = false;
  try {
    static_cast<void>(flight::row_get<double>(keyed_row, flight::String("depth")));
  } catch (const flight::BadAnyAccess&) {
    absent_refused = true;
  }
  check(absent_refused, "a key the row does not have reads undefined and is refused, not defaulted");
  bool keyed_unrepresented_reported = false;
  try {
    static_cast<void>(flight::row_get<double>(keyed_row, flight::String("value")));
  } catch (const flight::UnrepresentedProperty&) {
    keyed_unrepresented_reported = true;
  }
  check(keyed_unrepresented_reported, "a member with no erased reading is reported through the keyed read too");

  // The same walk over a REAL generated type, so this is not only exercising hand-written fixtures:
  // AmbientLightOptions is emitted from AmbientLightOptions.ts, whose properties are declared
  // color, enabled, intensity, intensityUnit.
  auto generated_options = flight::make_ref<flight::types::AmbientLightOptions>();
  generated_options->intensity = 2.0;
  const auto generated_view = flight::named_properties(generated_options);
  check(generated_view.keys() == std::vector<flight::String>({flight::String("color"),
                                                              flight::String("enabled"),
                                                              flight::String("intensity"),
                                                              flight::String("intensityUnit")}),
        "a real generated type enumerates in the order its source declares");
  check(generated_view.get(flight::String("intensity")).kind() == flight::AnyKind::number &&
            generated_view.get(flight::String("color")).kind() == flight::AnyKind::undefined,
        "and its optional members read as their value or as undefined");

  // The entity runtime slot is a SYMBOL key in the source, and the generated struct spells it as a
  // member named entity_runtime_key. It must not surface as an own enumerable string key, because
  // Object.keys does not report a symbol-keyed property. It does not, because the compiler never
  // reaches it through a RowKey -- it writes it through the symbol -- so nothing binds it as a
  // named cell. This asserts that invariant on a real generated entity rather than trusting it.
  auto generated_entity = flight::make_ref<flight::types::AmbientLight>();
  const auto entity_keys = flight::named_properties(generated_entity).keys();
  check(std::find(entity_keys.begin(), entity_keys.end(), flight::String("entityRuntimeKey")) ==
            entity_keys.end(),
        "the entity runtime slot is a symbol property and is not an own enumerable string key");
  check(std::find(entity_keys.begin(), entity_keys.end(), flight::String("kind")) !=
            entity_keys.end(),
        "while the entity's own string properties are enumerated");

  // The exact-owner readonly view reaches its subject, having taken the default-false path.
  auto exact_subject = flight::make_ref<TestOutsideTheKeySet>();
  exact_subject->only_owner_knows_this = 64.0;
  const ExactWritable exact_writable(exact_subject);
  const ExactReadonly exact_readonly = exact_writable;
  check(exact_readonly.shared_object() == exact_subject &&
            exact_readonly.shared_owner() == exact_writable.shared_owner(),
        "an exact-owner readonly view reaches its own subject without any widening proof");

  // The view now WRITES, but only where it can do so exactly. `mergeTextFormat` walks
  // `keyof TextFormat` copying each value from one TextFormat into another, which is the shape
  // this exists for.
  static_assert(writes_named_properties<flight::NamedProperties>,
                "the dynamic named view supports a checked keyed write");

  auto merge_target = flight::make_ref<flight::types::AmbientLightOptions>();
  const auto merge_view = flight::named_properties(merge_target);
  const flight::StructuralRef<
      flight::RowWritable<flight::RowOf<flight::Ref<flight::types::AmbientLightOptions>>>>
      merge_typed(merge_target);

  check(merge_view.set(flight::String("intensity"), flight::Any(2.5)),
        "a keyed write of the member's own type succeeds");
  check(flight::row_get<flight::RowKey<"intensity">>(merge_typed).value() == 2.5,
        "and lands on the subject, visible to a typed projection through the shared owner");

  // Refused, not coerced. JavaScript would happily turn "3" into 3 here, and that is exactly the
  // conversion that would hide the mistake from the member's typed readers.
  check(!merge_view.set(flight::String("intensity"), flight::Any(flight::String("3"))),
        "a keyed write of the wrong type is refused rather than coerced");
  check(flight::row_get<flight::RowKey<"intensity">>(merge_typed).value() == 2.5,
        "and leaves the member exactly as it was");

  // A key the subject does not declare has nowhere to go: accepting it would make the value
  // visible to this view and invisible to every typed reader of the same object.
  check(!merge_view.set(flight::String("noSuchProperty"), flight::Any(1.0)),
        "a keyed write to an undeclared key is refused");
  const auto merge_keys = merge_view.keys();
  check(std::find(merge_keys.begin(), merge_keys.end(), flight::String("noSuchProperty")) ==
            merge_keys.end(),
        "and does not invent the key");

  // A permitted widening reaches the computed cell. The cell is bound on the OWNER, which the
  // conversion keeps, so the read does not depend on the widened row's static subject matching the
  // object's type -- which it does not, and which is why the subject-typed path alone answered
  // false here.
  const auto entity_runtime = flight::Symbol::for_key(flight::String("EntityRuntime"));
  auto cell_subject = flight::make_ref<TestCellBearing>();
  const CellRow<TestCellBearing> own_row(cell_subject);
  const CellRow<TestCellBearingPeer> widened_row = own_row;
  check(!flight::row_has(own_row, entity_runtime) && !flight::row_has(widened_row, entity_runtime),
        "a disengaged computed cell is absent through both rows");

  auto slot = flight::make_ref<TestRuntimeSlotA>();
  slot->tag = 7.0;
  cell_subject->entity_runtime_key = slot;
  check(flight::row_has(own_row, entity_runtime) && flight::row_has(widened_row, entity_runtime),
        "engaging it makes it present through both rows, because there is one object and one owner");
  check(flight::row_get<std::optional<flight::Ref<TestRuntimeSlotA>>>(widened_row, entity_runtime)
                .value() == slot,
        "and the widened row reads the subject's own member rather than an attachment entry");

  const flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestCellBearing>>>>
      writable_cell_row(cell_subject);
  auto replacement = flight::make_ref<TestRuntimeSlotA>();
  flight::row_set(writable_cell_row, entity_runtime,
                  std::optional<flight::Ref<TestRuntimeSlotA>>(replacement));
  check(cell_subject->entity_runtime_key.value() == replacement,
        "a computed-symbol write lands on the subject's member, not beside it");

  // WRITTEN PRESENCE. `key in object` for a computed key separates the three source states the
  // disengaged optional collapses: never written (missing), written with undefined (present-undefined,
  // as allocateEntity writes it), and written with a value (present).
  {
    auto missing_subject = flight::make_ref<TestCellBearing>();
    const flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestCellBearing>>>>
        missing_row(missing_subject);
    check(!flight::row_has_own_property(missing_row, entity_runtime) &&
              !flight::has_own_property(missing_subject, entity_runtime),
          "a computed cell nobody wrote is missing, though the owner binds the cell");

    auto undefined_subject = flight::make_ref<TestCellBearing>();
    const flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestCellBearing>>>>
        undefined_row(undefined_subject);
    flight::row_set(undefined_row, entity_runtime, std::nullopt);
    check(!undefined_subject->entity_runtime_key.has_value(), "writing undefined leaves the optional disengaged");
    check(flight::row_has_own_property(undefined_row, entity_runtime) &&
              flight::has_own_property(undefined_subject, entity_runtime),
          "but the key is present: it was written with undefined");
    check(!flight::row_has(undefined_row, entity_runtime),
          "row_has keeps answering engagement, so it is not the presence question");
    const CellRow<TestCellBearingPeer> widened_undefined = CellRow<TestCellBearing>(undefined_subject);
    check(flight::row_has_own_property(widened_undefined, entity_runtime),
          "a widened row shares the one owner and sees the same write");

    auto value_subject = flight::make_ref<TestCellBearing>();
    value_subject->entity_runtime_key = flight::make_ref<TestRuntimeSlotA>();
    check(flight::has_own_property(value_subject, entity_runtime),
          "an engaged computed cell is present even with no recorded write");

    auto literal_subject = flight::make_ref<TestCellBearing>();
    check(flight::mark_constructed_own_property(literal_subject, entity_runtime) == literal_subject,
          "the construction mark hands back the same object");
    check(flight::has_own_property(literal_subject, entity_runtime) &&
              !flight::has_own_property(missing_subject, entity_runtime),
          "a literal that named the key marks only its own object");
  }

  // THE ERASED NAMED VIEW. An erased-member probe -- `'width' in value` followed by a read of
  // `value.width`, with nothing static saying what `value` is -- is answered through the object's
  // one row owner, so it reports the object's own members rather than a copy of them.
  auto erased_subject = flight::make_ref<flight::types::AmbientLightOptions>();
  const flight::Any erased = flight::Any::object(erased_subject);
  const auto erased_view = flight::named_properties(erased);
  check(static_cast<bool>(erased_view), "an Any holding an object yields a named view");
  check(erased_view.keys() == flight::named_properties(erased_subject).keys(),
        "reporting exactly what the typed view of the same object reports");
  check(erased_view.has(flight::String("intensity")) &&
            !erased_view.has(flight::String("notAKeyOfThis")),
        "and answering `in` over its own string keys only");
  check(erased_view.get(flight::String("intensity")).kind() == flight::AnyKind::undefined,
        "an optional member that holds nothing reads as undefined, not as a missing key");

  // NOMINAL PRESENCE FROM ALLOCATION PROVENANCE. A base-typed reference to a recorded derived allocation
  // answers from the exact type; without a record it refuses rather than answering from the base.
  {
    const flight::Ref<TestProvenanceBase> recorded = flight::make_ref<TestProvenanceDerived>();
    check(flight::has_own_named_property(recorded, flight::String("intensity")) &&
              flight::has_own_named_property(recorded, flight::String("radius")) &&
              !flight::has_own_named_property(recorded, flight::String("absent")) &&
              flight::has_own_named_property(recorded, flight::String("toString")),
          "a recorded allocation reached through its base reports the derived object's own members");
    bool unrecorded_refused = false;
    try {
      const flight::Ref<TestProvenanceBase> unrecorded = std::make_shared<TestProvenanceDerived>();
      static_cast<void>(flight::has_own_named_property(unrecorded, flight::String("intensity")));
    } catch (const flight::OutOfProfileError&) {
      unrecorded_refused = true;
    }
    check(unrecorded_refused, "an allocation make_ref did not record is refused, never answered false");
    bool null_threw = false;
    try {
      static_cast<void>(flight::has_own_named_property(flight::Ref<TestProvenanceBase>(), flight::String("x")));
    } catch (const flight::TypeError&) {
      null_threw = true;
    }
    check(null_threw, "`in` on a null reference throws TypeError");
  }

  // ERASED PRESENCE AND TYPEOF THROUGH THE PROTOTYPE CHAIN. `key in value` and `typeof value.key` on an
  // erased object ask its own cells, then its registered class prototype, then Object.prototype.
  {
    check(flight::has_property(erased, flight::String("intensity")) &&
              !flight::has_property(erased, flight::String("notAKeyOfThis")) &&
              flight::has_property(erased, flight::String("toString")),
          "an erased plain object answers own keys, missing keys, and Object.prototype names");
    check(flight::member_typeof(erased, flight::String("intensity")) == flight::String("undefined") &&
              flight::member_typeof(erased, flight::String("notAKeyOfThis")) == flight::String("undefined") &&
              flight::member_typeof(erased, flight::String("toString")) == flight::String("function"),
          "typeof reads the own value, a missing key, and an inherited Object.prototype method");
    bool threw_type_error = false;
    try {
      static_cast<void>(flight::has_property(flight::Any(1.0), flight::String("x")));
    } catch (const flight::TypeError&) {
      threw_type_error = true;
    }
    check(threw_type_error, "`in` on a primitive throws TypeError, as it does in JavaScript");
    bool threw_null_read = false;
    try {
      static_cast<void>(flight::member_typeof(flight::Any(flight::null), flight::String("x")));
    } catch (const flight::TypeError&) {
      threw_null_read = true;
    }
    check(threw_null_read, "reading a member of null throws TypeError");

    const flight::Any instance = flight::Any::object(flight::make_ref<TestPrototypeClass>());
    check(test_prototype_class_registered && flight::has_property(instance, flight::String("twice")) &&
              flight::has_property(instance, flight::String("size")) &&
              !flight::has_property(instance, flight::String("absent")),
          "a registered class reports its prototype methods and accessors, and not other names");
    check(flight::member_typeof(instance, flight::String("twice")) == flight::String("function"),
          "an inherited method is a function");
    bool accessor_refused = false;
    try {
      static_cast<void>(flight::member_typeof(instance, flight::String("size")));
    } catch (const flight::OutOfProfileError&) {
      accessor_refused = true;
    }
    check(accessor_refused, "typeof of an inherited accessor would run its getter, so it is refused");
    bool subclassed_refused = false;
    try {
      static_cast<void>(flight::has_property(flight::Any::object(flight::make_ref<TestPrototypeBase>()),
                                             flight::String("shared")));
    } catch (const flight::OutOfProfileError&) {
      subclassed_refused = true;
    }
    check(test_prototype_base_registered && subclassed_refused,
          "a class with subclasses is refused without the object's exact type");
  }

  // One owner, not two: a write through the typed row is visible through the erased view, and the
  // read that was undefined a moment ago is now the value the object actually holds.
  const flight::StructuralRef<
      flight::RowWritable<flight::RowOf<flight::Ref<flight::types::AmbientLightOptions>>>>
      erased_writable(erased_subject);
  flight::row_set<flight::RowKey<"intensity">>(erased_writable, 3.5);
  const auto after_write = erased_view.get(flight::String("intensity"));
  check(after_write.kind() == flight::AnyKind::number && after_write.same_value(flight::Any(3.5)),
        "and the erased view reads the object's own member, not a snapshot of it");

  check(!flight::named_properties(flight::Any(42.0)),
        "a primitive Any has no own string keys and reports an empty view rather than throwing");
  check(!flight::named_properties(flight::Any()), "and so does undefined");

  // JSON.stringify over a structural row. Emitted code reaches this through colorLutRunSignature,
  // which builds a CACHE KEY from `JSON.stringify(run)`. Before the row branch existed every row
  // serialized as `{}` through append_json_value's fallback, so two different runs produced the same
  // signature and the cache returned the wrong baked LUT. That makes this a correctness property, not
  // a formatting one, which is why distinct rows are asserted to give distinct text.
  auto json_light = flight::make_ref<flight::types::AmbientLight>();
  json_light->kind = flight::String("ambient");
  json_light->color = 255.0;
  json_light->enabled = true;
  json_light->intensity = 0.5;
  using AmbientRow = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::AmbientLight>>>>;
  const auto light_json = flight::Json::stringify(AmbientRow(json_light));
  check(light_json != flight::String("{}"), "a structural row serializes its members rather than an empty object");
  check(light_json.index_of(flight::String("\"kind\":\"ambient\"")) >= 0,
        "and names each own enumerable string key with its value");
  check(light_json.index_of(flight::String("entityRuntimeKey")) < 0,
        "while the entity runtime slot stays out, being a symbol property rather than a string key");

  // `JSON.stringify(value, replacer, space)` with an absent space indents by nothing, and emitted code
  // writes that third argument as std::nullopt.
  check(flight::Json::stringify(AmbientRow(json_light), std::nullopt, std::nullopt) == light_json,
        "an absent space argument means no indentation, the same as omitting it");
  check(flight::Json::stringify(AmbientRow(json_light), std::nullopt, 2.0) != light_json,
        "and a numeric space does indent");

  auto other_light = flight::make_ref<flight::types::AmbientLight>();
  other_light->kind = flight::String("ambient");
  other_light->color = 128.0;
  other_light->enabled = true;
  other_light->intensity = 0.5;
  check(flight::Json::stringify(AmbientRow(other_light)) != light_json,
        "two rows differing in one member serialize differently, so a signature built from this cannot collide");

  const flight::Array<AmbientRow> run{AmbientRow(json_light), AmbientRow(other_light)};
  const flight::Array<AmbientRow> reversed{AmbientRow(other_light), AmbientRow(json_light)};
  const auto run_json = flight::Json::stringify(run, std::nullopt, std::nullopt);
  check(run_json.index_of(flight::String("[{")) == 0, "a sequence of rows serializes as an array of objects");
  check(run_json != flight::Json::stringify(reversed, std::nullopt, std::nullopt),
        "and order is preserved, so a reordered run is a different signature");

  // A union satisfies a row when EVERY alternative widens onto the row's subject.
  //
  // This is the branch the SDK actually takes. TypeScript hands a value of union type where a row over
  // the union's common shape is wanted; C++ sees a `std::variant` of unrelated pointer types and no
  // conversion. The constraint cannot be `std::constructible_from`, because the shared_ptr constructor
  // is viable for any pointee and validates with a static_assert inside the runtime -- so a fold over it
  // admits alternatives that then hard-error in structural_ref.hpp instead of at the call site.
  {
    using Alternatives = std::variant<flight::Ref<TestDerivedTarget>, flight::Ref<TestOtherDerivedTarget>>;
    static_assert(std::constructible_from<BaseTargetRow, const Alternatives&>,
                  "a union whose every alternative widens onto the base satisfies a row over it");
    static_assert(
        !std::constructible_from<
            BaseTargetRow, const std::variant<flight::Ref<TestDerivedTarget>, flight::Ref<TestUnrelatedTarget>>&>,
        "one alternative that cannot produce the row refuses the whole union");
    static_assert(!std::constructible_from<BaseTargetRow, const std::variant<flight::Ref<TestRetypedTarget>>&>,
                  "an alternative matching by key spelling but not by type is not a widening");
    // Direction is preserved here too: the base row is the one a union of derived subjects can fill.
    static_assert(!std::constructible_from<DerivedTargetRow, const std::variant<flight::Ref<TestBaseTarget>>&>,
                  "and widening through a union still goes derived-to-base only");

    auto first = flight::make_ref<TestDerivedTarget>();
    first->width = 640.0;
    auto second = flight::make_ref<TestOtherDerivedTarget>();
    second->width = 800.0;

    const Alternatives holds_first(first);
    const Alternatives holds_second(second);
    const BaseTargetRow from_first{holds_first};
    const BaseTargetRow from_second{holds_second};
    check(flight::row_get<flight::RowKey<"width">>(from_first) == 640.0,
          "the row reads the subject the union holds");
    check(flight::row_get<flight::RowKey<"width">>(from_second) == 800.0,
          "and selects by the active alternative, not by its position");

    // A view of the live object, exactly as the non-union widening above: no copy is taken, and the
    // row's subject IS the held object rather than a base-shaped mint of it.
    second->width = 1024.0;
    check(flight::row_get<flight::RowKey<"width">>(from_second) == 1024.0,
          "a row built through a union is a view of the held object, not a snapshot");
    // One owner, one subject -- the same invariant the non-union widening above pins. A structurally
    // widened row holds no typed object pointer at all: `flatten_ref` cannot cast between two unrelated
    // structs, so the subject stays behind the owner and the member table reads it from there. That is
    // pre-existing row-to-row behaviour, and the point of these two assertions is that building the row
    // through a union lands on exactly it rather than on some second mechanism.
    const flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestOtherDerivedTarget>>>>
        directly(second);
    check(from_second.shared_owner() == directly.shared_owner() && from_second == directly,
          "a row built through a union shares the held object's one row owner");
    check(from_second.shared_object() == nullptr,
          "and will not hand its subject back AS the base type, because the relation is structural");
    check(from_second.shared_native_object().get() == static_cast<const void*>(second.get()),
          "while still holding that very object, erased -- the null above is a type check, not an absence");

    // The inheritance branch is the other dispatch, and it does convert the pointer, so it keeps one.
    // Asserting both directions here is what would catch the two branches being collapsed into one.
    check(from_first.shared_object() == nullptr,
          "the structural branch is taken for both alternatives of this union");
  }

  // A row write stores THE ARRAY, not a copy of it.
  //
  // `flight::Array` holds `shared_ptr<vector<Value>>`, so it is a handle with JS reference semantics and
  // array identity is a real, observable thing. TypeScript's `subject.advances = values` leaves
  // `subject.advances === values` true and a later `values.push(...)` visible through the subject. This
  // pins that the row write lane does the same, because a repair was queued that would have broken it.
  {
    using ArrayRow = flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<TestArrayBearing>>>>;
    auto subject = flight::make_ref<TestArrayBearing>();
    const ArrayRow row(subject);

    flight::Array<double> values;
    values.push(1.0);
    flight::row_set<flight::RowKey<"advances">>(row, values);
    check(static_cast<double>(subject->advances.size()) == 1.0, "the write reached the subject");

    values.push(2.0);
    check(static_cast<double>(subject->advances.size()) == 2.0,
          "a push through the written array is visible through the subject, so the row stored THAT array");
    check(static_cast<double>(flight::row_get<flight::RowKey<"advances">>(row).size()) == 2.0,
          "and reading the member back through the row sees it too");
    subject->advances.push(3.0);
    check(static_cast<double>(values.size()) == 3.0, "identity holds in the other direction as well");

    // The contrast that makes the withdrawn repair concrete. `flight::array_of` keeps ELEMENT identity
    // but returns a SEPARATE array handle, so writing its result through a row silently breaks the
    // invariant just asserted -- a later push through the source is invisible to the subject. That is why
    // array_of is correct where TypeScript itself produces a fresh array, and wrong at an assignment.
    flight::Array<double> source;
    source.push(1.0);
    flight::row_set<flight::RowKey<"advances">>(row, flight::array_of<double>(source));
    check(static_cast<double>(subject->advances.size()) == 1.0, "the projected array was written");
    source.push(2.0);
    check(static_cast<double>(subject->advances.size()) == 1.0,
          "a push through the SOURCE is invisible, which is the identity change that disqualifies "
          "array_of at a row write");
  }

  // Absence, null and a value are THREE distinguishable states through a partial row read.
  //
  // This contradicts what docs/generation-state.md recorded: that `row_get` over a `RowPartial` has
  // already collapsed null into absent and the information is unrecoverable at the call site. It is
  // recoverable WHEN the member type can itself represent null, which a pointer can -- `nullopt` is the
  // absent property, a held null pointer is TypeScript `null`, and a held non-null pointer is the value.
  // The collapse is real only for a member that has no null of its own, such as `optional<double>`.
  {
    using ClipRow = flight::StructuralRef<flight::RowReadonly<flight::RowPartial<
        flight::RowOf<flight::Ref<TestClipBearing>>>>>;

    auto absent = flight::make_ref<TestClipBearing>();
    const ClipRow absent_row(absent);
    const auto absent_read = flight::row_get<flight::RowKey<"clip">>(absent_row);
    check(!absent_read.has_value(), "an absent property reads as nullopt");

    auto nulled = flight::make_ref<TestClipBearing>();
    nulled->clip = flight::Ref<TestClipRegion>{};
    const ClipRow nulled_row(nulled);
    const auto nulled_read = flight::row_get<flight::RowKey<"clip">>(nulled_row);
    check(nulled_read.has_value() && *nulled_read == nullptr,
          "an explicit null reads as a PRESENT optional holding a null pointer, not as nullopt");

    auto region = flight::make_ref<TestClipRegion>();
    region->radius = 4.0;
    auto valued = flight::make_ref<TestClipBearing>();
    valued->clip = region;
    const ClipRow valued_row(valued);
    const auto valued_read = flight::row_get<flight::RowKey<"clip">>(valued_row);
    check(valued_read.has_value() && *valued_read != nullptr && (*valued_read)->radius == 4.0,
          "and a value reads as the region itself");

    // The three are pairwise distinct, which is the whole claim: a lossless re-encoding into the
    // emitter's `variant<V, Null, Undefined>` is therefore possible for a nullable member.
    check(absent_read.has_value() != nulled_read.has_value(),
          "absent and null are not the same state");
    check(static_cast<bool>(*nulled_read) != static_cast<bool>(*valued_read),
          "null and a value are not the same state");
  }

  // Null for a structural value cannot live inside the optional: StructuralRef is a value handle, not
  // a nullable pointer. The optional is therefore the null representation used by emitted SDK fields.
  // Keep the pointer-valued three-state behavior above as the contrast; it must not be collapsed.
  {
    using NullableRow = flight::StructuralRef<
        flight::RowWritable<flight::RowOf<flight::Ref<TestNullableRowBearing>>>>;
    auto target = flight::make_ref<TestNullableRowBearing>();
    auto base = flight::make_ref<TestBaseTarget>();
    base->width = 320.0;
    target->subscribe = BaseTargetRow(base);
    const NullableRow row(target);

    flight::row_set<flight::RowKey<"subscribe">>(row, nullptr);
    check(!target->subscribe.has_value(),
          "nullptr disengages a nullable structural member whose value type has no inner null");
    check(!flight::row_get<flight::RowKey<"subscribe">>(row).has_value(),
          "the nullable structural member reads back as absent/null");
  }

  // LIFETIME: a structurally widened row DOES keep its subject alive.
  //
  // Worth pinning because the obvious reading of the assertions above is wrong. A widened row's
  // `shared_object()` returns null, and `NativeRowOwner` holds its object only WEAKLY -- so it looks as
  // though nothing in the row owns the subject and a row built from a temporary would dangle. It does
  // not. The row's own erased `object_` is a strong `shared_ptr<void>` and the row-to-row conversion
  // carries it across; `shared_object()` returns null only because it first checks
  // `owner_->native_type() != typeid(object_type)`, which fails when the subject is structurally rather
  // than nominally related. The handle is populated, it is just not retrievable AS `object_type`, and
  // `shared_native_object()` is the accessor that still returns it.
  //
  // So this is the test that distinguishes "holds nothing" from "will not hand it back at that type",
  // and it is what makes writing a row over a freshly minted object safe.
  {
    using Alternatives = std::variant<flight::Ref<TestDerivedTarget>, flight::Ref<TestOtherDerivedTarget>>;
    std::weak_ptr<TestOtherDerivedTarget> observer;
    BaseTargetRow widened_from_temporary;
    {
      auto subject = flight::make_ref<TestOtherDerivedTarget>();
      subject->width = 512.0;
      observer = subject;
      // The variant AND the only named reference both die at the end of this scope.
      widened_from_temporary = BaseTargetRow{Alternatives(subject)};
      check(flight::row_get<flight::RowKey<"width">>(widened_from_temporary) == 512.0,
            "the widened row reads its subject while the original reference is alive");
    }
    const bool subject_alive = !observer.expired();
    std::cout << (subject_alive ? "LIFETIME: widened row keeps its subject alive\n"
                                : "LIFETIME: widened row does NOT keep its subject alive\n");
    if (subject_alive) {
      check(flight::row_get<flight::RowKey<"width">>(widened_from_temporary) == 512.0,
            "and keeps reading it after every other reference is gone");
    }
  }

  // What agreement between two member types means, and why one direction only.
  //
  // The generated table enumerates keys; `row_member_widens_v` decides whether the two declared types
  // agree. Identical types agree. A base key declared `std::optional<T>` is also satisfied by a derived
  // member of plain `T`, which is TypeScript's rule -- an optional property is `T | undefined` and `T`
  // is assignable to it -- and reads honestly, since such a subject answers every read with a value.
  static_assert(flight::detail::row_member_widens_v<double, double>);
  static_assert(flight::detail::row_member_widens_v<std::optional<bool>, bool>,
                "an optional base key is satisfied by a plain derived member");
  static_assert(!flight::detail::row_member_widens_v<bool, std::optional<bool>>,
                "but NOT the reverse: a plain base key promises a value the optional source may not have");
  static_assert(!flight::detail::row_member_widens_v<std::optional<bool>, double>,
                "and the relaxation does not loosen the value type itself");
  static_assert(!flight::detail::row_member_widens_v<double, flight::String>);
  // Nested optionals are not collapsed: optional<optional<T>> is satisfied by optional<T>, one level.
  static_assert(flight::detail::row_member_widens_v<std::optional<std::optional<bool>>, std::optional<bool>>);
  static_assert(!flight::detail::row_member_widens_v<std::optional<std::optional<bool>>, bool>,
                "one level of relaxation, not an unwrap to the bottom");

  if (failures == 0) std::cout << "structural row projections behave as specified\n";
  return failures == 0 ? 0 : 1;
}
