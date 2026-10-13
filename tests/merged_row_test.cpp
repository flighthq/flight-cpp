// Merged rows (`A & B`) as the SDK constructs and reads them, against the committed generated member
// table. Two emitted shapes motivate it:
//
//  * `@flighthq/host` builds its Host as `{} as EntityConstruction<Host & Capabilities>`, which lowers
//    to a row OF a row over a merge, `RowWritable<RowOf<StructuralRef<RowMerge<...>>>>`. That resolved
//    no members, so every `row_set` into it failed to compile, and the finished entity was an empty
//    bag no native Host could be recovered from.
//  * `@flighthq/app` reads a capability narrowed by `Required<Pick<Capability, 'center'>>`. The merge
//    says `center` is present; the capability stores it as `std::optional`, so the read asked the
//    owner for the bare type and threw `std::bad_cast`.
#include <flight/runtime.hpp>

#include <flight/app/app_window.hpp>
#include <flight/entity/entity.hpp>
#include <flight/types/app_window.hpp>
#include <flight/types/host.hpp>

#include <concepts>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>

namespace {

using namespace flight::types;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Schema>
using Row = flight::StructuralRef<Schema>;

// `Host & Capabilities` at Capabilities = Host, exactly as `create_host<Ref<Host>>` spells it.
using HostMerge = flight::RowMerge<flight::RowOf<flight::Ref<Host>>, flight::RowOf<flight::Ref<Host>>>;
using HostEntity = Row<HostMerge>;

// The same intersection with Capabilities itself a row type, `Host & Readonly<Host>`: the second
// constituent is a row of a row.
using ReadonlyHostRow = Row<flight::RowReadonly<flight::RowOf<flight::Ref<Host>>>>;
using HostRowMerge = flight::RowMerge<flight::RowOf<flight::Ref<Host>>, flight::RowOf<ReadonlyHostRow>>;

// `HostWindowGeometryCapability & Required<Pick<HostWindowGeometryCapability, 'center'>>`, as the app
// package spells it for `centerWindow`.
using CenterPick = flight::app::center_90209bd6a085864a;
using GeometryMerge = flight::RowMerge<flight::RowOf<flight::Ref<HostWindowGeometryCapability>>,
                                       flight::RowRequired<flight::RowOf<flight::Ref<CenterPick>>>>;
using CenteredGeometry = Row<flight::RowReadonly<GeometryMerge>>;
using WritableCenteredGeometry = Row<flight::RowWritable<GeometryMerge>>;

// Negative shapes. Two subjects with no key in common answer for neither.
using UnrelatedMerge = flight::RowMerge<flight::RowOf<flight::Ref<HostAudioCapabilities>>,
                                        flight::RowOf<flight::Ref<HostClipboardCapabilities>>>;

// Two distinct structs that each answer for the other: either could be the object, so neither is.
struct CenterA final : public flight::ReferenceEnabled {
  std::optional<std::function<void(flight::Ref<AppWindow>)>> center;
};
struct CenterB final : public flight::ReferenceEnabled {
  std::optional<std::function<void(flight::Ref<AppWindow>)>> center;
};
using AmbiguousMerge = flight::RowMerge<flight::RowOf<flight::Ref<CenterA>>, flight::RowOf<flight::Ref<CenterB>>>;

// A constituent the candidate does not answer for: Host has no `codec` or `mixer`, and its `device` is
// another group.
using UnansweredMerge = flight::RowMerge<flight::RowOf<flight::Ref<Host>>,
                                         flight::RowOf<flight::Ref<HostAudioCapabilities>>>;

static_assert(std::same_as<flight::detail::merge_subject_t<HostMerge>, Host>);
static_assert(std::same_as<flight::detail::merge_subject_t<HostRowMerge>, Host>);
static_assert(std::same_as<flight::detail::merge_subject_t<GeometryMerge>, HostWindowGeometryCapability>,
              "the capability answers for the picked `center`; the pick does not answer for the capability");
static_assert(std::is_void_v<flight::detail::merge_subject_t<UnrelatedMerge>>);
static_assert(std::is_void_v<flight::detail::merge_subject_t<AmbiguousMerge>>);
static_assert(std::is_void_v<flight::detail::merge_subject_t<UnansweredMerge>>);

// Construction from a pointer exists exactly where a subject does, and only for that subject.
static_assert(std::constructible_from<HostEntity, flight::Ref<Host>>);
static_assert(std::constructible_from<CenteredGeometry, flight::Ref<HostWindowGeometryCapability>>);
static_assert(!std::constructible_from<CenteredGeometry, flight::Ref<CenterPick>>);
static_assert(!std::constructible_from<CenteredGeometry, flight::Ref<HostAudioCapabilities>>);
static_assert(!std::constructible_from<Row<UnrelatedMerge>, flight::Ref<HostAudioCapabilities>>);
static_assert(!std::constructible_from<Row<AmbiguousMerge>, flight::Ref<CenterA>>);
static_assert(!std::constructible_from<Row<UnansweredMerge>, flight::Ref<Host>>);

// A row of a row reads its members from the inner row.
static_assert(std::same_as<flight::detail::schema_member_t<flight::RowKey<"audio">,
                                                           flight::RowOf<HostEntity>>,
                           flight::Ref<HostAudioCapabilities>>);
static_assert(flight::detail::schema_readonly<flight::RowOf<ReadonlyHostRow>>);
static_assert(!flight::detail::schema_readonly<flight::RowWritable<flight::RowOf<ReadonlyHostRow>>>);

// The narrowing is per member: `center` is required through the merge, `subscribeResize` stays optional.
static_assert(flight::detail::schema_member_required<flight::RowKey<"center">, CenteredGeometry::schema_type>);
static_assert(!flight::detail::schema_member_required<flight::RowKey<"subscribeResize">, CenteredGeometry::schema_type>);
static_assert(!flight::detail::schema_member_required<flight::RowKey<"audio">, HostMerge>);

// The emitted factory, `createHost` over `initializeHost`, for two of Host's groups.
template <typename Merge>
Row<Merge> create_host(flight::Ref<HostAudioCapabilities> audio, flight::Ref<HostClipboardCapabilities> clipboard) {
  flight::types::EntityConstruction<Row<Merge>> out = flight::entity::allocate_entity<Row<Merge>>();
  flight::row_set<flight::RowKey<"audio">>(out, audio);
  flight::row_set<flight::RowKey<"clipboard">>(out, clipboard);
  return flight::entity::finish_entity<Row<Merge>>(out);
}

template <typename Merge>
void test_entity_construction(const char* label) {
  const auto audio = flight::make_ref<HostAudioCapabilities>();
  const auto clipboard = flight::make_ref<HostClipboardCapabilities>();
  auto host = create_host<Merge>(audio, clipboard);

  check(flight::row_get<flight::RowKey<"audio">>(host) == audio, label);
  check(flight::row_get<flight::RowKey<"clipboard">>(host) == clipboard, label);
  check(!flight::row_get<flight::RowKey<"bitmap">>(host), "an unwritten required group is the null it was left as");

  // The finished entity IS a Host: the bag was minted as one, so the native object is recoverable and
  // carries what was written into the construction row.
  ReadonlyHostRow projected = host;
  flight::Ref<Host> native;
  try {
    native = flight::structural_ref_cast<flight::Ref<Host>>(projected);
  } catch (const std::bad_cast&) {
    check(false, "the finished merged entity recovers its native Host");
    return;
  }
  check(native && native->audio == audio && native->clipboard == clipboard,
        "construction writes landed on the Host's own members");
  check(!native->entity_runtime_key.has_value(), "allocate_entity's runtime slot write reached the Host member");

  // One object, one identity, writes visible both ways.
  check(projected == host, "the projection is the same row identity");
  const auto replacement = flight::make_ref<HostAudioCapabilities>();
  native->audio = replacement;
  check(flight::row_get<flight::RowKey<"audio">>(host) == replacement, "a native write is visible through the merge");
  const auto clipboard2 = flight::make_ref<HostClipboardCapabilities>();
  flight::row_set<flight::RowKey<"clipboard">>(Row<flight::RowWritable<flight::RowOf<HostEntity>>>(host), clipboard2);
  check(native->clipboard == clipboard2, "a merge write is visible on the native object");

  // Lifetime: the rows keep the Host alive, and releasing the last one releases it.
  std::weak_ptr<Host> weak = native;
  native.reset();
  check(!weak.expired(), "the merged row owns the constructed Host");
  host = Row<Merge>{};
  check(!weak.expired(), "a projection keeps it as well");
  projected = ReadonlyHostRow{};
  check(weak.expired(), "releasing every row releases the Host");
}

void test_pointer_view() {
  auto host = flight::make_ref<Host>();
  const auto audio = flight::make_ref<HostAudioCapabilities>();
  host->audio = audio;
  const HostEntity view(host);
  check(flight::row_get<flight::RowKey<"audio">>(view) == audio, "a merged view reads the object's member");
  check(view == ReadonlyHostRow(host), "a merged view shares the object's one owner");
  check(flight::structural_ref_cast<flight::Ref<Host>>(ReadonlyHostRow(view)) == host,
        "the same Host, not a copy, is recovered from the view");
  const HostEntity empty(flight::Ref<Host>{});
  check(!empty, "a null pointer is an empty merged row");
}

void test_narrowed_member() {
  auto geometry = flight::make_ref<HostWindowGeometryCapability>();
  const auto win = flight::make_ref<AppWindow>();
  flight::Ref<AppWindow> centered;
  geometry->center = [&](flight::Ref<AppWindow> target) { centered = target; };

  const CenteredGeometry view(geometry);
  try {
    flight::app::center_window(view, win);
    check(centered == win, "centerWindow calls the capability's own center with the window");
  } catch (const std::bad_cast&) {
    check(false, "a member the merge narrows to present reads from optional storage");
  }

  // The required member of the subject itself still reads as its bare storage.
  bool bounds_called = false;
  geometry->get_bounds = [&](flight::Ref<AppWindow>, flight::Ref<WindowBounds> out) {
    bounds_called = true;
    return out;
  };
  flight::row_get<flight::RowKey<"getBounds">>(view)(win, flight::make_ref<WindowBounds>());
  check(bounds_called, "a member no constituent narrows reads directly");

  // Optional members the merge leaves optional: present and absent.
  check(!flight::row_get<flight::RowKey<"subscribeResize">>(view).has_value(), "an absent optional member reads absent");
  geometry->subscribe_resize = [](flight::Ref<WindowResizeTargetHandle>, std::function<void(double, double, double)>) {
    return std::function<void()>{};
  };
  check(flight::row_get<flight::RowKey<"subscribeResize">>(view).has_value(), "a present optional member reads present");

  // The merge promised `center`; a capability without one is reported, not defaulted or mis-cast.
  geometry->center = std::nullopt;
  check(!flight::row_has<flight::RowKey<"center">>(view), "an empty narrowed member is not present");
  bool threw_absent = false;
  try {
    flight::app::center_window(view, win);
  } catch (const std::out_of_range&) {
    threw_absent = true;
  } catch (const std::bad_cast&) {
  }
  check(threw_absent, "an absent narrowed member throws out_of_range, never bad_cast");

  // A write through the narrowed member lands in the subject's optional storage.
  int writes = 0;
  const WritableCenteredGeometry writable(geometry);
  flight::row_set<flight::RowKey<"center">>(writable, std::function<void(flight::Ref<AppWindow>)>(
                                                          [&](flight::Ref<AppWindow>) { ++writes; }));
  check(geometry->center.has_value(), "the narrowed write engages the native optional");
  (*geometry->center)(win);
  check(writes == 1 && flight::row_has<flight::RowKey<"center">>(view), "the written member is the one read back");
}

void test_narrowed_construction() {
  // `{} as EntityConstruction<Readonly<Capability & Required<Pick<...>>>>` filled and finished: the bag becomes
  // the capability, and the narrowed member lands in the capability's own optional.
  int calls = 0;
  const auto win = flight::make_ref<AppWindow>();
  auto out = flight::entity::allocate_entity<CenteredGeometry>();
  flight::row_set<flight::RowKey<"center">>(
      out, std::function<void(flight::Ref<AppWindow>)>([&](flight::Ref<AppWindow> target) { calls += target == win; }));
  const auto view = flight::entity::finish_entity<CenteredGeometry>(out);
  flight::app::center_window(view, win);
  check(calls == 1, "a constructed narrowed view calls the center it was given");
  const auto native = flight::structural_ref_cast<flight::Ref<HostWindowGeometryCapability>>(
      Row<flight::RowReadonly<flight::RowOf<flight::Ref<HostWindowGeometryCapability>>>>(view));
  check(native && native->center.has_value(), "the constructed view is a native capability with center engaged");
  // A cast from a row over the capability is the other route into the same merge.
  const auto as_row = Row<flight::RowWritable<flight::RowOf<flight::Ref<HostWindowGeometryCapability>>>>(native);
  flight::app::center_window(flight::structural_ref_cast<CenteredGeometry>(as_row), win);
  check(calls == 2, "an asserted merge over the capability reads its center");
}

void test_unrelated_construction_stays_schema_only() {
  // With no subject the merge is what it was: a bag holding the written cells, which no nominal type is
  // minted from. Asking for one is refused observably rather than answered with a guess.
  auto out = flight::entity::allocate_entity<Row<UnrelatedMerge>>();
  const auto device = std::optional<flight::Ref<HostAudioDeviceCapability>>(flight::make_ref<HostAudioDeviceCapability>());
  flight::row_set<flight::RowKey<"device">>(out, device);
  const auto finished = flight::entity::finish_entity<Row<UnrelatedMerge>>(out);
  check(flight::row_get<flight::RowKey<"device">>(finished) == device, "the bag keeps what was written");
  bool refused = false;
  try {
    (void)flight::structural_ref_cast<flight::Ref<HostAudioCapabilities>>(
        Row<flight::RowReadonly<flight::RowOf<flight::Ref<HostAudioCapabilities>>>>(finished));
  } catch (const std::bad_cast&) {
    refused = true;
  }
  check(refused, "no native subject is invented for an unrelated merge");
}

} // namespace

int main() {
  test_entity_construction<HostMerge>("Host & Host construction keeps written groups");
  test_entity_construction<HostRowMerge>("Host & Readonly<Host> construction keeps written groups");
  test_pointer_view();
  test_narrowed_member();
  test_narrowed_construction();
  test_unrelated_construction_stays_schema_only();
  if (failures == 0) std::cout << "merged row tests passed\n";
  return failures == 0 ? 0 : 1;
}
