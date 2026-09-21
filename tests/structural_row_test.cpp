// Row-projection behaviour against the committed generated member table, which is what emitted code
// actually resolves named members through. The runtime test cannot cover this: it links the runtime
// alone, so `generated_row_member_t` is void there and no named member resolves.
#include <flight/runtime.hpp>

#include <flight/types/ambient_light.hpp>
#include <flight/types/ambient_light_options.hpp>

#include <algorithm>
#include <concepts>
#include <functional>
#include <vector>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>

namespace {

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

// Derived to base, and nothing else. The trait the runtime declares answers no by default and the
// generated table specializes it to yes only where the proof holds, so each of these is a statement
// about which half answered.
static_assert(flight::detail::generated_row_widening_proven_v<TestBaseTarget, TestDerivedTarget>);
static_assert(!flight::detail::generated_row_widening_proven_v<TestDerivedTarget, TestBaseTarget>);
static_assert(!flight::detail::generated_row_widening_proven_v<TestBaseTarget, TestUnrelatedTarget>);
static_assert(!flight::detail::generated_row_widening_proven_v<TestUnrelatedTarget, TestBaseTarget>);
static_assert(!flight::detail::generated_row_widening_proven_v<TestBaseTarget, TestRetypedTarget>);

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
  flight::Array<double> value;
  double height{};
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

} // namespace

int main() {
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

  // The view is read-only: it has no way to write a property back.
  static_assert(!writes_named_properties<flight::NamedProperties>,
                "the dynamic named view never writes");

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

  if (failures == 0) std::cout << "structural row projections behave as specified\n";
  return failures == 0 ? 0 : 1;
}
