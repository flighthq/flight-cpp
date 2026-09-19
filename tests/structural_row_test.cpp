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

  if (failures == 0) std::cout << "structural row projections behave as specified\n";
  return failures == 0 ? 0 : 1;
}
