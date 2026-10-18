// The DragonBones section counters are a public consumer of the erased-array length lowering: after an
// `Array.isArray` guard the section length is the runtime's live array size, not a named-property read.
// These counters were previously blocked by the emitted Any->double length reads. The cases here use
// real erased arrays: a non-empty section counts its live length, an empty section and a non-array
// receiver take the source's early return and set no key, the timeline tally accumulates the live
// length, and the animation walk feeds the same counters through the erased for-of.
//
// The absent-key case is the typed Record missing-key root: a key the dictionary does not hold reads
// undefined and the counter returns early, rather than throwing on an empty optional.
#include <flight/skeleton2d_formats/dragon_bones_section_counts.hpp>

#include <flight/any.hpp>
#include <flight/array.hpp>
#include <flight/types/bone_2d.hpp>

#include <iostream>
#include <variant>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

using flight::Any;
using flight::Array;
using flight::Map;
using flight::Record;
using flight::Ref;
using flight::String;
namespace formats = flight::skeleton2d_formats;
namespace types = flight::types;

using Counts = Map<String, double>;

}  // namespace

int main() {
  Counts counts;
  Record<String, Any> armature;
  armature.set(String("bone"), Any(Array<Any>{Any(1.0), Any(2.0), Any(3.0)}));
  armature.set(String("slot"), Any(Array<Any>{}));
  formats::count_section_flight_value_function_count_u000053_ection_flight_private_fee4bf3fb73047fb(
      counts, armature, String("bone"), String("bones"));
  formats::count_section_flight_value_function_count_u000053_ection_flight_private_fee4bf3fb73047fb(
      counts, armature, String("slot"), String("slots"));
  formats::count_section_flight_value_function_count_u000053_ection_flight_private_fee4bf3fb73047fb(
      counts, armature, String("missing"), String("missing"));
  armature.set(String("undefined"), Any(flight::undefined));
  formats::count_section_flight_value_function_count_u000053_ection_flight_private_fee4bf3fb73047fb(
      counts, armature, String("undefined"), String("undefined"));
  armature.set(String("null"), Any(flight::null));
  formats::count_section_flight_value_function_count_u000053_ection_flight_private_fee4bf3fb73047fb(
      counts, armature, String("null"), String("null"));

  check(counts.get(String("bones")).value_or(-1.0) == 3.0, "a non-empty section counts its live length");
  check(!counts.get(String("slots")).has_value(), "an empty section takes the early return and sets no key");
  check(!counts.get(String("missing")).has_value(), "an absent section key reads undefined and sets no key");
  check(!counts.get(String("undefined")).has_value(), "a present undefined section sets no key");
  check(!counts.get(String("null")).has_value(), "a present null section sets no key");

  formats::tally_timeline_flight_value_function_tally_u000054_imeline_flight_private_fee4bf3fb73047fb(
      counts, Any(Array<Any>{Any(1.0), Any(2.0), Any(3.0), Any(4.0)}), String("tallied"));
  formats::tally_timeline_flight_value_function_tally_u000054_imeline_flight_private_fee4bf3fb73047fb(
      counts, Any(5.0), String("not-array"));
  check(counts.get(String("tallied")).value_or(-1.0) == 4.0, "the timeline tally accumulates the live length");
  check(!counts.get(String("not-array")).has_value(), "a non-array timeline sets no key");

  const Ref<types::Bone2D> entry = flight::make_ref<types::Bone2D>(types::Bone2D{
      .name = std::variant<String, flight::Null, flight::Undefined>{std::in_place_type<String>, String("root")}});
  Counts animation_counts;
  formats::count_animation_timelines_flight_value_function_count_u000041_nimation_u000054_imelines_flight_private_fee4bf3fb73047fb(
      animation_counts, Any(Array<Any>{Any::object(entry)}));
  check(animation_counts.get(types::dragon_bones_section_kind->animations).value_or(-1.0) == 1.0,
        "the animation walk counts its erased entries");

  if (failures == 0) std::cout << "DragonBones section counters match the source at their boundaries\n";
  return failures == 0 ? 0 : 1;
}
