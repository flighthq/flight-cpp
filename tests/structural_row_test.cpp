// Row-projection behaviour against the committed generated member table, which is what emitted code
// actually resolves named members through. The runtime test cannot cover this: it links the runtime
// alone, so `generated_row_member_t` is void there and no named member resolves.
#include <flight/runtime.hpp>

#include <functional>
#include <iostream>
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

  if (failures == 0) std::cout << "structural row projections behave as specified\n";
  return failures == 0 ? 0 : 1;
}
