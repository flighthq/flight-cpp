// The generated @flighthq/skeleton2d slot-attachment binder at a627849, exercised natively through the
// installed public header. Ported from the source contract of
// packages/skeleton2d/src/skeleton2dAnimationTarget.ts#bindSkeleton2DSlotAttachment:
// - the sampled track value is a STEP index into the target's attachment table, never interpolated;
// - a present element keeps its original owner (identity, not a copy);
// - a stored null element clears the slot to null, and so does an index outside the table or the -1
//   Spine writes for "no attachment" -- the source's own `: null` fallback;
// - an absent (undefined or null) table, an empty time track, or a time before the first keyframe
//   leaves the slot untouched;
// - the step walk clamps a time before the first keyframe to keyframe 0;
// - a non-Step track still steps, and reports the coercion through the guard.
//
// This is the public consumer the header could not compile before the indexed-element lift: the
// conditional used to read `table.element(index)`, which throws instead of yielding the stored null that
// JavaScript reads for an in-range index whose element is null.
#include <flight/animation/animation_track.hpp>
#include <flight/skeleton2d/skeleton2d_animation_target.hpp>
#include <flight/types/attachment_2d.hpp>
#include <flight/types/skeleton_2d_slot_animation_target.hpp>
#include <flight/types/slot_2d.hpp>

#include <iostream>
#include <optional>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

template <typename Pointer>
auto readonly(const Pointer& value) {
  using Element = typename Pointer::element_type;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Element>>>>(value);
}

namespace anim = flight::animation;
namespace skeleton2d = flight::skeleton2d;
using flight::types::AnimationChannel;
using flight::types::AnimationTrack;
using flight::types::Attachment2D;
using flight::types::Skeleton2DSlotAnimationTarget;
using flight::types::Slot2D;

using TrackOptions = anim::times_values_components_interpolation_quaternion_easing_segment_easings_9b0eadfed5e06042;
using Table = flight::Array<std::optional<flight::Ref<Attachment2D>>>;
using SlotAttachment = std::variant<flight::Ref<Attachment2D>, flight::Null, flight::Undefined>;

flight::Ref<Attachment2D> attachment(const char* name) {
  auto value = flight::make_ref<Attachment2D>();
  value->kind = flight::String("region");
  value->name = flight::String(name);
  return value;
}

flight::Ref<Slot2D> slot() {
  auto value = flight::make_ref<Slot2D>();
  value->bone_index = 0.0;
  return value;
}

flight::Ref<Skeleton2DSlotAnimationTarget> target(const std::variant<Table, flight::Null, flight::Undefined>& table) {
  auto value = flight::make_ref<Skeleton2DSlotAnimationTarget>();
  value->attachments = table;
  value->kind = flight::String("Slot");
  value->path = flight::String("Attachment");
  value->slot_index = 0.0;
  return value;
}

flight::Ref<AnimationChannel> channel(flight::Array<double> values, std::optional<flight::String> interpolation) {
  auto opts = flight::make_ref<TrackOptions>();
  opts->times = flight::SequenceView<double>(flight::Array<double>{0.0, 1.0, 2.0});
  opts->values = flight::SequenceView<double>(values);
  if (interpolation.has_value()) opts->interpolation = *interpolation;
  auto track = anim::create_animation_track(opts);
  auto value = flight::make_ref<AnimationChannel>();
  value->track = track;
  return value;
}

void run_binder(const flight::Ref<AnimationChannel>& source, const flight::Ref<Slot2D>& poseSlot,
          const flight::Ref<Skeleton2DSlotAnimationTarget>& poseTarget, double time) {
  skeleton2d::bind_skeleton_2d_slot_attachment(readonly(source), poseSlot, readonly(poseTarget), time);
}

const flight::Ref<Attachment2D>* present(const std::variant<flight::Ref<Attachment2D>, flight::Null, flight::Undefined>& value) {
  return std::get_if<flight::Ref<Attachment2D>>(&value);
}

bool is_null(const std::variant<flight::Ref<Attachment2D>, flight::Null, flight::Undefined>& value) {
  return std::holds_alternative<flight::Null>(value);
}

bool is_undefined(
    const std::variant<flight::Ref<Attachment2D>, flight::Null, flight::Undefined>& value) {
  return std::holds_alternative<flight::Undefined>(value);
}

} // namespace

int main() {
  const auto a0 = attachment("a0");
  const auto a2 = attachment("a2");
  Table table;
  table.push(std::optional<flight::Ref<Attachment2D>>{a0});
  table.push(std::optional<flight::Ref<Attachment2D>>{std::nullopt}); // stored null element
  table.push(std::optional<flight::Ref<Attachment2D>>{a2});

  const auto poseTarget = target(std::variant<Table, flight::Null, flight::Undefined>{std::in_place_type<Table>, table});

  // A present element keeps the source owner: the same attachment, not a copy or a replacement.
  {
    auto pose = slot();
    run_binder(channel(flight::Array<double>{0.0, 1.0, 2.0}, flight::String("Step")), pose, poseTarget, 0.5);
    const auto* selected = present(pose->attachment);
    check(selected != nullptr, "in-range index selects a present attachment");
    check(selected != nullptr && (*selected).get() == a0.get(), "the present attachment keeps its owner");
    check(selected != nullptr && (*selected)->kind == flight::String("region"), "the owner is intact");
  }

  // A stored null element is null -- not undefined, and not the previous attachment.
  {
    auto pose = slot();
    pose->attachment = SlotAttachment{std::in_place_type<flight::Ref<Attachment2D>>, a2};
    run_binder(channel(flight::Array<double>{0.0, 1.0, 2.0}, flight::String("Step")), pose, poseTarget, 1.5);
    check(is_null(pose->attachment), "a stored null element clears the slot to null");
    check(!is_undefined(pose->attachment), "a stored null element is not undefined");
  }

  // Index 2 selects the third element, so the step lookup did not stop at the first keyframe.
  {
    auto pose = slot();
    run_binder(channel(flight::Array<double>{0.0, 1.0, 2.0}, flight::String("Step")), pose, poseTarget, 2.5);
    const auto* selected = present(pose->attachment);
    check(selected != nullptr && (*selected).get() == a2.get(), "the step walk selects the later element");
  }

  // -1 is Spine's "no attachment": the source's own null fallback, not element -1.
  {
    auto pose = slot();
    pose->attachment = SlotAttachment{std::in_place_type<flight::Ref<Attachment2D>>, a2};
    run_binder(channel(flight::Array<double>{-1.0, -1.0, -1.0}, flight::String("Step")), pose, poseTarget, 0.5);
    check(is_null(pose->attachment), "-1 clears the slot to null");
  }

  // An index past the end is the same null fallback; it is never read as an out-of-range element.
  {
    auto pose = slot();
    pose->attachment = SlotAttachment{std::in_place_type<flight::Ref<Attachment2D>>, a0};
    run_binder(channel(flight::Array<double>{0.0, 3.0, 3.0}, flight::String("Step")), pose, poseTarget, 1.5);
    check(is_null(pose->attachment), "an index past the end clears the slot to null");
  }

  // The sampled value is rounded before the table read, exactly as the source does.
  {
    auto pose = slot();
    run_binder(channel(flight::Array<double>{0.0, 1.49, 1.51}, flight::String("Step")), pose, poseTarget, 1.5);
    check(is_null(pose->attachment), "1.49 rounds to 1, the stored null element");
    run_binder(channel(flight::Array<double>{0.0, 1.49, 1.51}, flight::String("Step")), pose, poseTarget, 2.5);
    const auto* selected = present(pose->attachment);
    check(selected != nullptr && (*selected).get() == a2.get(), "1.51 rounds to 2, the present element");
  }

  // A time before the first keyframe clamps to keyframe 0, as findSkeleton2DStepKeyframe does.
  {
    auto pose = slot();
    run_binder(channel(flight::Array<double>{2.0, 1.0, 2.0}, flight::String("Step")), pose, poseTarget, -5.0);
    const auto* selected = present(pose->attachment);
    check(selected != nullptr && (*selected).get() == a2.get(), "a time before the first keyframe uses keyframe 0");
  }

  // An empty time track finds no keyframe and leaves the slot untouched.
  {
    auto opts = flight::make_ref<TrackOptions>();
    opts->times = flight::SequenceView<double>(flight::Array<double>{});
    opts->values = flight::SequenceView<double>(flight::Array<double>{0.0});
    auto source = flight::make_ref<AnimationChannel>();
    source->track = anim::create_animation_track(opts);
    auto pose = slot();
    pose->attachment = SlotAttachment{std::in_place_type<flight::Ref<Attachment2D>>, a0};
    run_binder(source, pose, poseTarget, 0.5);
    const auto* unchanged = present(pose->attachment);
    check(unchanged != nullptr && (*unchanged).get() == a0.get(), "an empty time track leaves the slot untouched");
  }

  // An absent table is not an empty table: both undefined and null leave the slot untouched.
  {
    auto pose = slot();
    pose->attachment = SlotAttachment{std::in_place_type<flight::Ref<Attachment2D>>, a0};
    run_binder(channel(flight::Array<double>{0.0, 1.0, 2.0}, flight::String("Step")), pose,
         target(std::variant<Table, flight::Null, flight::Undefined>{std::in_place_type<flight::Undefined>,
                                                                     flight::undefined}),
         0.5);
    const auto* unchanged = present(pose->attachment);
    check(unchanged != nullptr && (*unchanged).get() == a0.get(), "an undefined table leaves the slot untouched");
    run_binder(channel(flight::Array<double>{0.0, 1.0, 2.0}, flight::String("Step")), pose,
         target(std::variant<Table, flight::Null, flight::Undefined>{std::in_place_type<flight::Null>,
                                                                     flight::null}),
         0.5);
    unchanged = present(pose->attachment);
    check(unchanged != nullptr && (*unchanged).get() == a0.get(), "a null table leaves the slot untouched");
  }

  // A non-Step track is still stepped (never interpolated between table indices), and the coercion is
  // reported through the guard rather than silently accepted.
  {
    auto pose = slot();
    run_binder(channel(flight::Array<double>{0.0, 1.0, 2.0}, flight::String("Linear")), pose, poseTarget, 2.5);
    const auto* selected = present(pose->attachment);
    check(selected != nullptr && (*selected).get() == a2.get(), "a non-Step track still steps");
  }

  if (failures != 0) {
    std::cerr << failures << " slot attachment check(s) failed\n";
    return 1;
  }
  std::cout << "slot attachment binder matches the pinned source at its boundaries\n";
  return 0;
}
