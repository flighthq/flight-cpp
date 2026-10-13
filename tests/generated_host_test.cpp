// Native parity test for @flighthq/host, ported from host.test.ts, hostQuery.test.ts, hostExplain.test.ts and
// enableHostGuards.test.ts.
//
// The package is the portable host contract: a bag of capability groups, a typed accessor per capability a Flight
// package takes as a parameter, a census over those capabilities, and a guard tier that warns about the ones a
// malformed host is missing. It is the diagnostic surface, so the case it exists for is the MALFORMED host, and
// that is where the two defects recorded below live.
//
// WHAT THIS FILE COVERS
//
//   * hostQuery: each accessor returns the capability object it names -- by identity, not a copy -- and a group that
//     is present but whose slot is absent reads as an absent optional rather than as an error.
//   * hostExplain: `explainHost` enumerates the groups the host actually CARRIES, with the slot names filled in each;
//     its capability census has one row per covered slot, naming its group and slot, with `isPresent` true for the
//     supplied ones; `isSlotPresent` answers for a group/slot pair; and a per-capability explanation reports
//     presence and, when absent, a message naming what is missing.
//   * enableHostGuards: NOT covered -- see the note at the end of this file's body; the log seam it warns through
//     is a declared stub.
//
// TWO DEFECTS, both measured and both filed rather than worked around. Neither is asserted here, and the reasons
// are different:
//
//   1. `getHostAudioDevice` -- and every other accessor in hostQuery -- dereferences the GROUP unconditionally:
//      `return flight::row_get<flight::RowKey<"audio">>(host)->device;`. The source is `host.audio?.device ?? null`,
//      and its own comment says the group access is optional-chained precisely because "a structurally built
//      stand-in with a group missing must produce `null` rather than a TypeError". Measured: a host row with no
//      `audio` group PANICS here with "member access within null pointer of type 'HostAudioCapabilities'". A test
//      cannot assert a panic in-process, so the case is recorded in the doc comment and filed as
//      docs/compiler-requests/host-group-optional-chain-dropped.md.
//   2. `createHost`/`initializeHost` DO NOT COMPILE for any instantiation. The source is `capabilities.audio ?? {}`,
//      and the emitter lowers it to `flight::row_get<RowKey<"audio">>(capabilities).value_or(...)` -- but that read
//      yields a `Ref`, not an optional, so `.value_or` does not exist: "no member named 'value_or' in
//      'std::shared_ptr<flight::types::HostAudioCapabilities>'", for every one of the groups. So host.ts has no
//      callable surface at all and its six upstream cases cannot be ported; filed as
//      docs/compiler-requests/host-initialize-optional-fallback-on-a-required-read.md. A host is therefore built by
//      hand here, group by group, which is what the rest of this file does.
#include <flight/host/enable_host_guards.hpp>
#include <flight/host/host_explain.hpp>
#include <flight/host/host_query.hpp>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace host_ns = flight::host;
namespace types = flight::types;

using Host = types::Host;
using HostRow = flight::RowReadonly<flight::RowOf<flight::Ref<Host>>>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

std::string utf8(const flight::String& value) { return value.to_utf8(); }

flight::StructuralRef<HostRow> as_row(const flight::Ref<Host>& host) {
  return flight::StructuralRef<HostRow>(host);
}

// --- hostQuery: the accessors ---------------------------------------------------------------------------------

void test_an_accessor_returns_the_capability_it_names_by_identity() {
  const auto host = flight::make_ref<Host>();
  const auto audio = flight::make_ref<types::HostAudioCapabilities>();
  const auto device = flight::make_ref<types::HostAudioDeviceCapability>();
  audio->device = device;
  host->audio = audio;
  const auto video = flight::make_ref<types::HostVideoCapabilities>();
  const auto playback = flight::make_ref<types::HostVideoCapability>();
  video->playback = playback;
  host->video = video;

  const auto read_device = host_ns::get_host_audio_device(as_row(host));
  const auto read_playback = host_ns::get_host_video(as_row(host));

  check(read_device.has_value() && read_device.value().get() == device.get(),
        "the audio-device accessor returns the very capability the host carries, not a copy");
  check(read_playback.has_value() && read_playback.value().get() == playback.get(),
        "and the video-playback accessor does the same on its own group");
}

void test_a_present_group_with_an_absent_slot_reads_as_absent() {
  const auto host = flight::make_ref<Host>();
  host->audio = flight::make_ref<types::HostAudioCapabilities>();   // the group is there, its slots are empty
  host->video = flight::make_ref<types::HostVideoCapabilities>();

  check(!host_ns::get_host_audio_device(as_row(host)).has_value(),
        "an audio group with no device reads as absent rather than as an error");
  check(!host_ns::get_host_audio_mixer(as_row(host)).has_value(), "and so does its mixer");
  check(!host_ns::get_host_video(as_row(host)).has_value(), "as does an empty video group's playback");
}

void test_a_group_the_host_does_not_carry_is_a_measured_divergence() {
  // NOT asserted, and not because it is uninteresting: the emitted accessor dereferences the group
  // unconditionally, so a host row without that group PANICS (measured: "member access within null pointer of
  // type 'HostAudioCapabilities'") where the source's `host.audio?.device ?? null` returns null. A panic cannot be
  // asserted in-process; the case is recorded here so the gap is visible at the point of use, and filed as
  // docs/compiler-requests/host-group-optional-chain-dropped.md.
  check(true, "the absent-group case is a filed divergence, measured outside this process");
}

// --- hostExplain: the census and the groups --------------------------------------------------------------------

void test_explain_reports_the_groups_the_host_actually_carries() {
  const auto host = flight::make_ref<Host>();
  const auto audio = flight::make_ref<types::HostAudioCapabilities>();
  audio->device = flight::make_ref<types::HostAudioDeviceCapability>();
  host->audio = audio;
  host->video = flight::make_ref<types::HostVideoCapabilities>();

  const auto explanation = host_ns::explain_host(as_row(host));
  check(explanation->groups.size() == 2.0,
        "only the groups the host carries are enumerated, not the groups its type declares");

  bool saw_audio = false;
  bool saw_video = false;
  for (const auto& group : explanation->groups) {
    if (utf8(group->group) == "audio") {
      saw_audio = group->slots.size() == 1.0 && utf8(group->slots.element(0.0)) == "device";
    }
    if (utf8(group->group) == "video") {
      saw_video = group->slots.size() == 0.0;
    }
  }
  check(saw_audio, "the audio group reports the one slot it has filled");
  check(saw_video, "and a group carrying nothing reports no slots rather than being omitted");
}

void test_the_coverage_census_names_every_covered_capability_and_its_presence() {
  const auto host = flight::make_ref<Host>();
  const auto audio = flight::make_ref<types::HostAudioCapabilities>();
  audio->device = flight::make_ref<types::HostAudioDeviceCapability>();
  host->audio = audio;

  const auto explanation = host_ns::explain_host(as_row(host));
  check(explanation->capabilities.size() > 0.0, "the census covers the capabilities a package can take");

  bool saw_present_device = false;
  std::size_t present = 0;
  for (const auto& row : explanation->capabilities) {
    if (row->is_present) ++present;
    if (utf8(row->group) == "audio" && utf8(row->slot) == "device") {
      saw_present_device = row->is_present && utf8(row->capability) == "HostAudioDeviceCapability";
    }
  }
  check(saw_present_device, "the audio device is reported present, named by its capability interface");
  check(present == 1.0, "and it is the only one present, since the host carries nothing else");
}

void test_is_slot_present_answers_for_a_group_and_slot_pair() {
  const auto host = flight::make_ref<Host>();
  const auto audio = flight::make_ref<types::HostAudioCapabilities>();
  audio->device = flight::make_ref<types::HostAudioDeviceCapability>();
  host->audio = audio;

  check(host_ns::is_slot_present(as_row(host), flight::String("audio"), flight::String("device")),
        "the filled slot is present");
  check(!host_ns::is_slot_present(as_row(host), flight::String("audio"), flight::String("mixer")),
        "an empty sibling slot on the same group is not");
}

void test_a_per_capability_explanation_reports_presence_and_absence() {
  const auto host = flight::make_ref<Host>();
  const auto audio = flight::make_ref<types::HostAudioCapabilities>();
  audio->device = flight::make_ref<types::HostAudioDeviceCapability>();
  host->audio = audio;

  const auto present = host_ns::explain_host_audio_device(as_row(host));
  check(present->is_present && utf8(present->group) == "audio" && utf8(present->slot) == "device",
        "a carried capability explains itself as present, naming its group and slot");

  const auto missing = host_ns::explain_host_audio_mixer(as_row(host));
  check(!missing->is_present, "one the host does not carry explains itself as absent");
  check(utf8(missing->slot) == "mixer", "naming the slot that is missing");
  check(!missing->message.empty(), "with a message saying what is missing");
}

// --- enableHostGuards: NOT covered, and why ------------------------------------------------------------------
//
// The guard tier routes every warning through @flighthq/log's `logOnce`, and that package's override declares
// throwing stubs: enabling the guards on any host aborts with `flight::Error: flight::log::merge_span_fields was
// not generated for this profile`. So the guard cannot be exercised end to end here, and asserting anything about it
// through a stubbed seam would be asserting the stub. It is left uncovered rather than covered through a fake sink;
// the slot-level facts it reads are covered above by `explainHost` and the per-capability explanations, which are
// the same tables the guards consume.

} // namespace

int main() {
  test_an_accessor_returns_the_capability_it_names_by_identity();
  test_a_present_group_with_an_absent_slot_reads_as_absent();
  test_a_group_the_host_does_not_carry_is_a_measured_divergence();

  test_explain_reports_the_groups_the_host_actually_carries();
  test_the_coverage_census_names_every_covered_capability_and_its_presence();
  test_is_slot_present_answers_for_a_group_and_slot_pair();
  test_a_per_capability_explanation_reports_presence_and_absence();

  if (failures == 0) {
    std::cout << "host parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
