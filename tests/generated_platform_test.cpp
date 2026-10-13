// Native parity test for @flighthq/platform, ported from its own platform.test.ts (37 cases).
//
// The package has two halves. One is a pure comparison -- `comparePlatformVersions` -- with more corners than a
// version comparator usually has, and the other is a host-explicit identity read: every function that answers
// "what platform is this" takes the provider as an argument and asks it, with no ambient backend to set and no
// cached answer to go stale.
//
//   * The comparator's corners: identical strings short-circuit; the empty string sorts BELOW any version rather
//     than being an error; segments compare numerically, so '10' beats '9' and '2.10' beats '2.9'; missing trailing
//     segments are zero, so '10.0' equals '10.0.0'; and a segment that is not a number counts as zero -- which is
//     why 'abc' beats '-1' rather than tying with it.
//   * The identity record: fourteen fields, each with its own default, and the two functions that produce it agree
//     because one is the other's construction initializer.
//   * The reads: `getPlatformInfo` fills the caller's object and returns THAT object, while the scalar conveniences
//     read through one module-level scratch, which is why a caller's object is never aliased by them.
//   * The predicates: desktop/mobile/web are exact kind tests, `native` is a runtime test that is true for every
//     host shell and deliberately false for both 'web' and 'unknown', and the version floor is false when the
//     reported version is unknown rather than being satisfied by an empty string.
//
// The upstream suite's last block asserts an ABSENCE -- that the module exports no ambient-state API
// (`setPlatformBackend`, `getPlatformBackend`, and five more). C++ has no export list to inspect, so that case is
// covered here by its behavioural content instead: the only influence on a read is the provider passed in, which
// the "no ambient state" check asserts by answering two different providers in turn. That is a translation, not the
// original check, and it is labelled as such rather than claimed as the same assertion.
#include <flight/platform/platform.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace platform = flight::platform;
namespace types = flight::types;

using Info = types::PlatformInfo;
using HostRow = flight::RowReadonly<flight::RowOf<flight::Ref<types::HostPlatformCapability>>>;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

void check_close(double value, double expected, const std::string& message) {
  if (value == expected) return;
  std::cerr << "FAIL: " << message << " (got " << value << ", want " << expected << ")\n";
  ++failures;
}

std::string utf8(const flight::String& value) { return value.to_utf8(); }

void check_string(const flight::String& actual, const char* expected, const std::string& message) {
  if (utf8(actual) == expected) return;
  std::cerr << "FAIL: " << message << " (got '" << utf8(actual) << "', want '" << expected << "')\n";
  ++failures;
}

// The upstream fake writes a full zeroed record and then the fields the case cares about, so a host built here
// answers for every field rather than leaving the rest to chance. The holder outlives the row it hands out, since
// the row only borrows the capability's callable.
struct Host {
  flight::Ref<Info> record = platform::create_platform_info();
  flight::Ref<types::HostPlatformCapability> capability;

  void apply(const std::string& field, const flight::String& value) {
    if (field == "arch") record->arch = value;
    else if (field == "distro") record->distro = value;
    else if (field == "distroVersion") record->distro_version = value;
    else if (field == "endianness") record->endianness = value;
    else if (field == "engine") record->engine = value;
    else if (field == "engineVersion") record->engine_version = value;
    else if (field == "kind") record->kind = value;
    else if (field == "locale") record->locale = value;
    else if (field == "name") record->name = value;
    else if (field == "osBuild") record->os_build = value;
    else if (field == "pointerWidth") record->pointer_width = flight::parse_int(value, 10.0);
    else if (field == "runtime") record->runtime = value;
    else if (field == "version") record->version = value;
    else if (field == "isTouch") record->is_touch = value == flight::String("\x01");
  }
};

std::vector<std::shared_ptr<Host>> holders;

flight::StructuralRef<HostRow> host_of(std::initializer_list<std::pair<const char*, const char*>> fields) {
  auto holder = std::make_shared<Host>();
  for (const auto& [field, value] : fields) holder->apply(field, flight::String(value));
  holders.push_back(holder);
  holder->capability = flight::make_ref<types::HostPlatformCapability>();
  const auto record = holder->record;
  holder->capability->get_info = [record](flight::Ref<Info> out) {
    *out = *record;
    return out;
  };
  return flight::make_structural_ref<HostRow>(flight::row_field<flight::RowKey<"getInfo">>(holder->capability->get_info));
}

double compare(const char* a, const char* b) {
  return platform::compare_platform_versions(flight::String(a), flight::String(b));
}

// --- comparePlatformVersions -----------------------------------------------------------------------------------

void test_identical_versions_compare_equal() {
  check_close(compare("10.15.7", "10.15.7"), 0.0, "an identical version compares equal");
  check_close(compare("", ""), 0.0, "and so do two empty versions");
}

void test_the_empty_version_sorts_below_any_other() {
  check_close(compare("", "1.0"), -1.0, "an unknown version sorts below a known one");
  check_close(compare("1.0", ""), 1.0, "and a known one sorts above an unknown one");
}

void test_a_lower_version_compares_less() {
  check_close(compare("10.15.6", "10.15.7"), -1.0, "a lower patch compares less");
  check_close(compare("9", "10"), -1.0, "a lower major compares less");
  check_close(compare("10.0", "10.0.1"), -1.0, "and a version with one fewer part is lower when the extra part is real");
}

void test_a_higher_version_compares_greater() {
  check_close(compare("10.15.7", "10.15.6"), 1.0, "a higher patch compares greater");
  check_close(compare("11", "10"), 1.0, "a higher major compares greater");
  check_close(compare("14", "13.0.1"), 1.0, "and a shorter higher major beats a longer lower one");
}

void test_segments_compare_numerically_not_lexicographically() {
  check_close(compare("10", "9"), 1.0, "ten is above nine, which string order would get backwards");
  check_close(compare("2.10", "2.9"), 1.0, "and so is a tenth minor over a ninth");
}

void test_missing_trailing_segments_are_zero() {
  check_close(compare("10.0", "10.0.0"), 0.0, "an absent trailing segment counts as zero");
  check_close(compare("10", "10.0.0"), 0.0, "however many of them are absent");
}

void test_a_non_numeric_segment_counts_as_zero() {
  // 'abc' is not a number, so its segment is zero -- which is why it sits ABOVE '-1' rather than tying with it.
  check_close(compare("abc", "-1"), 1.0, "a segment that is not a number counts as zero");
  check_close(compare("abc", "0"), 0.0, "and so ties with an explicit zero");
}

// --- The identity record ---------------------------------------------------------------------------------------

void test_a_fresh_record_carries_every_declared_default() {
  const auto info = platform::create_platform_info();
  check_string(info->arch, "", "arch");
  check_string(info->distro, "", "distro");
  check_string(info->distro_version, "", "distroVersion");
  check_string(info->endianness, "unknown", "endianness");
  check_string(info->engine, "unknown", "engine");
  check_string(info->engine_version, "", "engineVersion");
  check(!info->is_touch, "isTouch");
  check_string(info->kind, "unknown", "kind");
  check_string(info->locale, "", "locale");
  check_string(info->name, "unknown", "name");
  check_string(info->os_build, "", "osBuild");
  check_close(info->pointer_width, -1.0, "pointerWidth is the unknown sentinel, not zero");
  check_string(info->runtime, "unknown", "runtime");
  check_string(info->version, "", "version");
}

void test_the_initializer_and_the_factory_agree() {
  // Upstream asserts only that initializePlatformInfo is a function; the contract it names is that it IS
  // createPlatformInfo's initializer, so this builds one through the initializer directly and compares.
  auto construction = flight::entity::allocate_entity<flight::Ref<Info>>();
  platform::initialize_platform_info(construction);
  const auto initialized = flight::entity::finish_entity<flight::Ref<Info>>(construction);
  const auto created = platform::create_platform_info();

  check(initialized->arch == created->arch && initialized->distro == created->distro &&
            initialized->distro_version == created->distro_version &&
            initialized->endianness == created->endianness && initialized->engine == created->engine &&
            initialized->engine_version == created->engine_version && initialized->is_touch == created->is_touch &&
            initialized->kind == created->kind && initialized->locale == created->locale &&
            initialized->name == created->name && initialized->os_build == created->os_build &&
            initialized->pointer_width == created->pointer_width && initialized->runtime == created->runtime &&
            initialized->version == created->version,
        "initializing through the initializer produces the same record the factory does");
}

void test_get_info_fills_the_callers_object_and_returns_it() {
  const auto row = host_of({{"arch", "arm64"}, {"kind", "mobile"}, {"name", "ios"}});
  const auto out = platform::create_platform_info();
  const auto returned = platform::get_platform_info(row, out);

  check(returned.get() == out.get(), "the caller's object is returned itself, not a copy");
  check_string(out->name, "ios", "and the name the provider reported landed on it");
  check_string(out->arch, "arm64", "as did the architecture");
  check_string(out->kind, "mobile", "and the kind");
}

// --- The scalar reads ------------------------------------------------------------------------------------------

void test_each_scalar_convenience_reads_its_own_field() {
  const auto row = host_of({{"kind", "desktop"}, {"name", "macos"}, {"runtime", "electron"}, {"engine", "blink"},
                            {"version", "14.2.1"}, {"isTouch", "\x01"}});

  check_string(platform::get_platform_kind(row), "desktop", "the kind read");
  check_string(platform::get_platform_name(row), "macos", "the name read");
  check_string(platform::get_platform_runtime(row), "electron", "the runtime read");
  check_string(platform::get_platform_engine(row), "blink", "the engine read");
  check_string(platform::get_platform_info(row, platform::create_platform_info())->version, "14.2.1",
               "and the version is on the record the caller can read");
  check(platform::is_platform_touch(row), "and the touch flag");
}

void test_every_runtime_the_shells_report_is_read_back() {
  for (const char* runtime : {"web", "electron", "tauri", "capacitor", "native"}) {
    const auto row = host_of({{"runtime", runtime}});
    check_string(platform::get_platform_runtime(row), runtime, std::string("the runtime read for ") + runtime);
  }
  for (const char* engine : {"blink", "gecko", "webkit", "unknown"}) {
    const auto row = host_of({{"engine", engine}});
    check_string(platform::get_platform_engine(row), engine, std::string("the engine read for ") + engine);
  }
}

void test_the_only_influence_on_a_read_is_the_provider_passed_in() {
  // The upstream suite's last case asserts that no ambient-state API exists (setPlatformBackend and six more).
  // There is no export list to inspect here, so the behaviour behind that absence is what is asserted: answering
  // two different providers in turn gives each its own answer, which no cached or last-set backend would.
  const auto first = host_of({{"kind", "desktop"}, {"name", "windows"}});
  const auto second = host_of({{"kind", "mobile"}, {"name", "android"}});

  check_string(platform::get_platform_name(first), "windows", "the first provider answers for itself");
  check_string(platform::get_platform_name(second), "android", "and the second does not inherit it, nor the first it");
  check_string(platform::get_platform_name(first), "windows", "and asking the first again still answers for the first");
}

void test_the_scratch_reads_do_not_alias_a_callers_object() {
  const auto row = host_of({{"kind", "desktop"}, {"name", "linux"}, {"version", "6.1"}});
  const auto caller = platform::create_platform_info();
  platform::get_platform_info(row, caller);

  // The conveniences share one module-level scratch; the caller's object is a different one.
  check_string(platform::get_platform_kind(host_of({{"kind", "web"}})), "web",
               "a following convenience read answers for its own provider");
  check_string(caller->name, "linux", "and leaves the object a previous call filled in untouched");
}

// --- The predicates --------------------------------------------------------------------------------------------

void test_the_kind_predicates_are_exact() {
  check(platform::is_platform_desktop(host_of({{"kind", "desktop"}})), "desktop reports desktop");
  check(!platform::is_platform_desktop(host_of({{"kind", "web"}})) &&
            !platform::is_platform_desktop(host_of({{"kind", "mobile"}})) &&
            !platform::is_platform_desktop(host_of({{"kind", "unknown"}})),
        "and nothing else does");
  check(platform::is_platform_mobile(host_of({{"kind", "mobile"}})) &&
            !platform::is_platform_mobile(host_of({{"kind", "desktop"}})),
        "mobile reports mobile, and only mobile");
  check(platform::is_platform_web(host_of({{"kind", "web"}})) && !platform::is_platform_web(host_of({{"kind", "desktop"}})),
        "web reports web, and only web");
}

void test_native_is_a_runtime_test_that_excludes_web_and_unknown() {
  for (const char* shell : {"electron", "tauri", "capacitor", "native"}) {
    check(platform::is_platform_native(host_of({{"runtime", shell}})),
          std::string("a ") + shell + " runtime is a native host");
  }
  check(!platform::is_platform_native(host_of({{"runtime", "web"}})), "a plain web page is not");
  check(!platform::is_platform_native(host_of({{"runtime", "unknown"}})),
        "and neither is an unknown runtime, which is the case a bare 'not web' test would get wrong");
}

void test_touch_reflects_the_provider_flag_both_ways() {
  check(platform::is_platform_touch(host_of({{"isTouch", "\x01"}})), "a touch device reports touch");
  check(!platform::is_platform_touch(host_of({{"isTouch", "\x00"}})), "and a non-touch one does not");
}

void test_the_version_floor() {
  check(platform::is_platform_version_at_least(host_of({{"version", "14.0"}}), flight::String("14.0")),
        "a version equal to the floor meets it");
  check(platform::is_platform_version_at_least(host_of({{"version", "15.0"}}), flight::String("14.0")),
        "an older floor is met");
  check(!platform::is_platform_version_at_least(host_of({{"version", "13.0"}}), flight::String("14.0")),
        "and a newer one is not");

  check(platform::is_platform_version_at_least(host_of({{"version", "10.15.7"}}), flight::String("10.15.6")),
        "a newer patch meets an older floor");
  check(!platform::is_platform_version_at_least(host_of({{"version", "10.15.7"}}), flight::String("10.15.8")),
        "and an older patch does not meet a newer one");

  // An unknown version is not "at least" anything -- including an empty floor, which the comparison alone would
  // call equal. That is the case the explicit empty-version guard exists for.
  check(!platform::is_platform_version_at_least(host_of({{"version", ""}}), flight::String("1.0")),
        "an unknown version does not meet a real floor");
  check(!platform::is_platform_version_at_least(host_of({{"version", ""}}), flight::String("")),
        "and it does not meet an empty floor either, rather than comparing equal to it");
}

} // namespace

int main() {
  test_identical_versions_compare_equal();
  test_the_empty_version_sorts_below_any_other();
  test_a_lower_version_compares_less();
  test_a_higher_version_compares_greater();
  test_segments_compare_numerically_not_lexicographically();
  test_missing_trailing_segments_are_zero();
  test_a_non_numeric_segment_counts_as_zero();

  test_a_fresh_record_carries_every_declared_default();
  test_the_initializer_and_the_factory_agree();
  test_get_info_fills_the_callers_object_and_returns_it();

  test_each_scalar_convenience_reads_its_own_field();
  test_every_runtime_the_shells_report_is_read_back();
  test_the_only_influence_on_a_read_is_the_provider_passed_in();
  test_the_scratch_reads_do_not_alias_a_callers_object();

  test_the_kind_predicates_are_exact();
  test_native_is_a_runtime_test_that_excludes_web_and_unknown();
  test_touch_reflects_the_provider_flag_both_ways();
  test_the_version_floor();

  if (failures == 0) {
    std::cout << "platform parity: all checks passed\n";
    return 0;
  }
  std::cerr << failures << " check(s) failed\n";
  return 1;
}
