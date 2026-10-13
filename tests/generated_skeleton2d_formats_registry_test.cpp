// The three generated @flighthq/skeleton2d-formats handler registries at a627849 -- Spine binary, Spine
// JSON and DragonBones -- exercised natively. Ported from spineBinaryRegistry.test.ts, spineJsonRegistry.test.ts
// and dragonBonesRegistry.test.ts, which are the same nine cases over three families, so one template runs them
// over each. Upstream checks handler identity with toBe; a std::function has no identity to compare, so each
// handler here records that it ran, and "returns the second handler" is checked as "calling the result runs
// the second handler and not the first". Registration order is checked directly, because last-write-wins must
// replace an entry in place rather than move its key to the end.
#include <flight/skeleton2d_formats/dragon_bones_registry.hpp>
#include <flight/skeleton2d_formats/spine_binary_registry.hpp>
#include <flight/skeleton2d_formats/spine_json_registry.hpp>

#include <iostream>
#include <string>

namespace {

int failures = 0;
std::string family;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL [" << family << "]: " << message << '\n';
    ++failures;
  }
}

namespace formats = flight::skeleton2d_formats;
namespace types = flight::types;

template <typename Pointer>
auto view(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

struct SpineBinary {
  static constexpr const char* name = "spine-binary";
  using SectionHandler = types::SpineBinarySectionHandler;
  using TimelineHandler = types::SpineBinaryTimelineHandler;
  static auto create() { return formats::create_spine_binary_registry(); }
  static flight::String section_kind() { return types::spine_binary_section_kind->bones; }
  static flight::String other_section_kind() { return types::spine_binary_section_kind->slots; }
  static flight::String timeline_kind() { return types::spine_binary_timeline_kind->bone; }
  static SectionHandler section(int& runs) {
    return [&runs](flight::Ref<types::SpineBinarySectionContext>) { ++runs; };
  }
  static TimelineHandler timeline(int& runs) {
    return [&runs](flight::Ref<types::SpineBinaryTimelineContext>) { ++runs; };
  }
  static void run(const SectionHandler& handler) { handler(nullptr); }
  static void run(const TimelineHandler& handler) { handler(nullptr); }
  static auto get_section(const auto& registry, const flight::String& kind) {
    return formats::get_spine_binary_section_handler(view(registry), kind);
  }
  static auto get_timeline(const auto& registry, const flight::String& kind) {
    return formats::get_spine_binary_timeline_handler(view(registry), kind);
  }
  static void register_section(const auto& registry, const flight::String& kind, SectionHandler handler) {
    formats::register_spine_binary_section_handler(registry, kind, handler);
  }
  static void register_timeline(const auto& registry, const flight::String& kind, TimelineHandler handler) {
    formats::register_spine_binary_timeline_handler(registry, kind, handler);
  }
  static bool unregister_section(const auto& registry, const flight::String& kind) {
    return formats::unregister_spine_binary_section_handler(registry, kind);
  }
  static bool unregister_timeline(const auto& registry, const flight::String& kind) {
    return formats::unregister_spine_binary_timeline_handler(registry, kind);
  }
};

struct SpineJson {
  static constexpr const char* name = "spine-json";
  using SectionHandler = types::SpineJsonSectionHandler;
  using TimelineHandler = types::SpineJsonTimelineHandler;
  static auto create() { return formats::create_spine_json_registry(); }
  static flight::String section_kind() { return types::spine_json_section_kind->bones; }
  static flight::String other_section_kind() { return types::spine_json_section_kind->slots; }
  static flight::String timeline_kind() { return types::spine_json_timeline_kind->bone; }
  static SectionHandler section(int& runs) {
    return [&runs](flight::Ref<types::SpineJsonSectionContext>) { ++runs; };
  }
  static TimelineHandler timeline(int& runs) {
    return [&runs](flight::Ref<types::SpineJsonTimelineContext>, flight::String, flight::Record<flight::String, flight::Any>) {
      ++runs;
    };
  }
  static void run(const SectionHandler& handler) { handler(nullptr); }
  static void run(const TimelineHandler& handler) { handler(nullptr, flight::String("name"), {}); }
  static auto get_section(const auto& registry, const flight::String& kind) {
    return formats::get_spine_json_section_handler(view(registry), kind);
  }
  static auto get_timeline(const auto& registry, const flight::String& kind) {
    return formats::get_spine_json_timeline_handler(view(registry), kind);
  }
  static void register_section(const auto& registry, const flight::String& kind, SectionHandler handler) {
    formats::register_spine_json_section_handler(registry, kind, handler);
  }
  static void register_timeline(const auto& registry, const flight::String& kind, TimelineHandler handler) {
    formats::register_spine_json_timeline_handler(registry, kind, handler);
  }
  static bool unregister_section(const auto& registry, const flight::String& kind) {
    return formats::unregister_spine_json_section_handler(registry, kind);
  }
  static bool unregister_timeline(const auto& registry, const flight::String& kind) {
    return formats::unregister_spine_json_timeline_handler(registry, kind);
  }
};

struct DragonBones {
  static constexpr const char* name = "dragon-bones";
  using SectionHandler = types::DragonBonesSectionHandler;
  using TimelineHandler = types::DragonBonesTimelineHandler;
  static auto create() { return formats::create_dragon_bones_registry(); }
  static flight::String section_kind() { return types::dragon_bones_section_kind->bones; }
  static flight::String other_section_kind() { return types::dragon_bones_section_kind->slots; }
  static flight::String timeline_kind() { return types::dragon_bones_timeline_kind->bone; }
  static SectionHandler section(int& runs) {
    return [&runs](flight::Ref<types::DragonBonesSectionContext>) { ++runs; };
  }
  static TimelineHandler timeline(int& runs) {
    return [&runs](flight::Ref<types::DragonBonesTimelineContext>, flight::String,
                   flight::Record<flight::String, flight::Any>) { ++runs; };
  }
  static void run(const SectionHandler& handler) { handler(nullptr); }
  static void run(const TimelineHandler& handler) { handler(nullptr, flight::String("name"), {}); }
  static auto get_section(const auto& registry, const flight::String& kind) {
    return formats::get_dragon_bones_section_handler(view(registry), kind);
  }
  static auto get_timeline(const auto& registry, const flight::String& kind) {
    return formats::get_dragon_bones_timeline_handler(view(registry), kind);
  }
  static void register_section(const auto& registry, const flight::String& kind, SectionHandler handler) {
    formats::register_dragon_bones_section_handler(registry, kind, handler);
  }
  static void register_timeline(const auto& registry, const flight::String& kind, TimelineHandler handler) {
    formats::register_dragon_bones_timeline_handler(registry, kind, handler);
  }
  static bool unregister_section(const auto& registry, const flight::String& kind) {
    return formats::unregister_dragon_bones_section_handler(registry, kind);
  }
  static bool unregister_timeline(const auto& registry, const flight::String& kind) {
    return formats::unregister_dragon_bones_timeline_handler(registry, kind);
  }
};

template <typename F>
void exercise() {
  family = F::name;
  int a = 0;
  int b = 0;

  // createXRegistry: independent, and both families start empty.
  auto first = F::create();
  auto second = F::create();
  check(first->section_handlers.size() == 0 && first->timeline_handlers.size() == 0, "a new registry is empty");
  F::register_section(first, F::section_kind(), F::section(a));
  check(first->section_handlers.size() == 1 && second->section_handlers.size() == 0 &&
            first->timeline_handlers.size() == 0,
        "registries do not share their handler lists");

  // getX*Handler: null when nothing is registered.
  check(!F::get_section(F::create(), F::section_kind()).has_value(), "an unregistered section is null");
  check(!F::get_timeline(F::create(), F::timeline_kind()).has_value(), "an unregistered timeline is null");

  // registerX*Handler: last-write-wins without moving the registered key.
  {
    auto registry = F::create();
    a = b = 0;
    F::register_section(registry, F::section_kind(), F::section(a));
    F::register_section(registry, F::other_section_kind(), F::section(a));
    F::register_section(registry, F::section_kind(), F::section(b));
    check(registry->section_handlers.size() == 2, "a re-registered section replaces its entry");
    check(registry->section_handlers.element(0)->kind == F::section_kind() &&
              registry->section_handlers.element(1)->kind == F::other_section_kind(),
          "the replaced section keeps its position");
    auto found = F::get_section(registry, F::section_kind());
    if (found.has_value()) F::run(*found);
    check(found.has_value() && a == 0 && b == 1, "the last section handler wins");
  }
  {
    auto registry = F::create();
    a = b = 0;
    F::register_timeline(registry, F::timeline_kind(), F::timeline(a));
    F::register_timeline(registry, F::timeline_kind(), F::timeline(b));
    check(registry->timeline_handlers.size() == 1, "a re-registered timeline replaces its entry");
    auto found = F::get_timeline(registry, F::timeline_kind());
    if (found.has_value()) F::run(*found);
    check(found.has_value() && a == 0 && b == 1, "the last timeline handler wins");
  }

  // Source extensions outside the built-in vocabularies.
  {
    auto registry = F::create();
    a = b = 0;
    F::register_section(registry, flight::String("vendorSection"), F::section(a));
    F::register_timeline(registry, flight::String("vendorTimeline"), F::timeline(b));
    auto section = F::get_section(registry, flight::String("vendorSection"));
    auto timeline = F::get_timeline(registry, flight::String("vendorTimeline"));
    if (section.has_value()) F::run(*section);
    if (timeline.has_value()) F::run(*timeline);
    check(a == 1 && b == 1, "vendor section and timeline kinds register and resolve");
    check(!F::get_section(registry, flight::String("vendorTimeline")).has_value(),
          "the section and timeline families are separate namespaces");
  }

  // unregisterX*Handler: true once for a present handler, then false.
  {
    auto registry = F::create();
    F::register_section(registry, F::section_kind(), F::section(a));
    F::register_section(registry, F::other_section_kind(), F::section(a));
    check(F::unregister_section(registry, F::section_kind()), "removes a present section");
    check(!F::unregister_section(registry, F::section_kind()), "a second removal finds nothing");
    check(registry->section_handlers.size() == 1 && registry->section_handlers.element(0)->kind == F::other_section_kind(),
          "removal leaves the other section in place");
    F::register_timeline(registry, F::timeline_kind(), F::timeline(a));
    check(F::unregister_timeline(registry, F::timeline_kind()), "removes a present timeline");
    check(!F::unregister_timeline(registry, F::timeline_kind()), "a second timeline removal finds nothing");
    check(!F::unregister_timeline(F::create(), F::timeline_kind()), "removing from an empty registry is false");
  }
}

}  // namespace

int main() {
  exercise<SpineBinary>();
  exercise<SpineJson>();
  exercise<DragonBones>();
  return failures == 0 ? 0 : 1;
}
