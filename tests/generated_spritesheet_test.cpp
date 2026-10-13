// Native evidence for the emitted @flighthq/spritesheet surface at a627849. The cases are ported
// from the package's upstream constructor, clone, frame-lookup, player and runtime-validation suites.
// Hydration and descriptor validation are intentionally absent: their declarations remain compiler
// refusals, not substitutes.
//
// MUTATION SENSITIVITY, measured 2026-10-13 against isolated shadow headers: three one-at-a-time
// mutations were compiled and run, then discarded. The suite killed inversion of restart=false's
// animation identity guard, LIFO rather than FIFO queue consumption, and promotion of the empty-
// animation validation warning to an error.
#include <flight/signals/slot.hpp>
#include <flight/spritesheet/spritesheet.hpp>
#include <flight/spritesheet/spritesheet_animation.hpp>
#include <flight/spritesheet/spritesheet_data.hpp>
#include <flight/spritesheet/spritesheet_frame.hpp>
#include <flight/spritesheet/spritesheet_player.hpp>
#include <flight/spritesheet/spritesheet_validation.hpp>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>

namespace {

namespace spritesheet = flight::spritesheet;

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}

template <typename Type>
auto readonly(const std::shared_ptr<Type>& value) {
  using View = flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<Type>>>>;
  return flight::structural_ref_cast<View>(value);
}

flight::Ref<flight::types::SpritesheetAnimation> animation(
    std::initializer_list<double> frames, double frame_duration = 100.0,
    double repeat_count = -1.0, const char* direction = "forward") {
  auto value = spritesheet::create_spritesheet_animation();
  value->frames = flight::Array<double>(frames);
  value->frame_duration = frame_duration;
  value->repeat_count = repeat_count;
  value->direction = flight::String(direction);
  return value;
}

void construction_and_clone() {
  auto frame = spritesheet::create_spritesheet_frame();
  check(frame->id == 0.0 && frame->offset_x == 0.0 && frame->offset_y == 0.0,
        "frame defaults are zero");
  check(!frame->pivot_x.has_value() && !frame->pivot_y.has_value() && !frame->rotated,
        "frame pivot and rotation defaults are preserved");

  auto anim_a = spritesheet::create_spritesheet_animation();
  auto anim_b = spritesheet::create_spritesheet_animation();
  check(anim_a != anim_b && anim_a->direction == flight::String("forward"),
        "animation construction returns distinct forward animations");
  check(anim_a->frames.empty() && !anim_a->frame_durations.has_value() &&
            anim_a->frame_duration == 0.0 && anim_a->repeat_count == 0.0,
        "animation defaults match the source");
  anim_a->frames.push(99.0);
  check(anim_b->frames.empty(), "default frame arrays are not shared");

  auto data_a = spritesheet::create_spritesheet_data();
  auto data_b = spritesheet::create_spritesheet_data();
  check(data_a->animations.empty() && data_a->frames.empty() &&
            data_a->image_file == flight::String("") && data_a->scale == 1.0,
        "data defaults match the source");
  data_a->frames.push(spritesheet::create_spritesheet_frame_data());
  check(data_b->frames.empty(), "data default arrays are not shared");

  auto sheet = spritesheet::create_spritesheet();
  frame->id = 7.0;
  frame->pivot_x = 2.0;
  frame->rotated = true;
  sheet->frames.push(frame);
  sheet->animations.set(flight::String("walk"), anim_b);
  auto clone = spritesheet::clone_spritesheet(readonly(sheet));
  check(clone != sheet && clone->frames.size() == 1 && clone->frames.element(0) != frame,
        "clone copies the sheet and every frame");
  check(clone->frames.element(0)->id == 7.0 && clone->frames.element(0)->pivot_x == 2.0 &&
            clone->frames.element(0)->rotated,
        "clone preserves frame values");
  check(clone->animations.get(flight::String("walk")).value() == anim_b,
        "clone keeps animation object identity");
  check(!spritesheet::get_spritesheet_animation(sheet, flight::String("missing")).has_value() &&
            spritesheet::get_spritesheet_animation(sheet, flight::String("walk")).value() == anim_b,
        "animation lookup distinguishes missing and present labels");
}

void frame_lookup_and_controls() {
  auto sheet = spritesheet::create_spritesheet();
  for (double id = 0.0; id < 3.0; id += 1.0) {
    auto frame = spritesheet::create_spritesheet_frame();
    frame->id = id;
    sheet->frames.push(frame);
  }
  auto anim = animation({0.0, 1.0, 2.0});
  auto player = spritesheet::create_spritesheet_player();
  check(player->complete && !player->animation.has_value() && player->speed == 1.0,
        "player defaults to a complete idle state");
  check(!spritesheet::get_spritesheet_player_frame(readonly(player), readonly(sheet)).has_value(),
        "an idle player has no current frame");

  spritesheet::play_spritesheet_animation(player, readonly(anim));
  check(player->animation.value() == anim && !player->complete && player->elapsed == 0.0,
        "play installs an animation and resets playback");
  check(spritesheet::get_spritesheet_player_frame(readonly(player), readonly(sheet)).value()->id == 0.0,
        "current frame resolves through the animation");
  check(spritesheet::get_spritesheet_player_frame_at(readonly(player), readonly(sheet), -1.0).value()->id == 2.0,
        "neighbor lookup wraps backward");
  check(spritesheet::get_spritesheet_player_frame_at(readonly(player), readonly(sheet), 4.0).value()->id == 1.0,
        "neighbor lookup wraps forward without mutation");
  check(player->frame_index == 0.0 && player->elapsed == 0.0,
        "neighbor lookup leaves playback state unchanged");

  spritesheet::pause_spritesheet_player(player);
  check(!spritesheet::update_spritesheet_player(player, 100.0) && player->elapsed == 0.0,
        "paused players do not advance");
  spritesheet::resume_spritesheet_player(player);
  check(spritesheet::update_spritesheet_player(player, 100.0) && player->frame_index == 1.0,
        "resumed players advance");
  spritesheet::play_spritesheet_animation(player, readonly(anim), false);
  check(player->elapsed == 100.0 && player->frame_index == 1.0,
        "restart=false preserves state for the identical animation");
  auto other = animation({2.0, 1.0, 0.0});
  spritesheet::play_spritesheet_animation(player, readonly(other), false);
  check(player->animation.value() == other && player->elapsed == 0.0 && player->frame_index == 0.0,
        "restart=false still resets state for a distinct animation identity");
  spritesheet::seek_spritesheet_player_to_frame(player, 99.0);
  check(player->frame_index == 2.0 && player->elapsed == 200.0,
        "frame seek clamps and synchronizes elapsed time");
  spritesheet::seek_spritesheet_player_to_time(player, 50.0);
  check(player->elapsed == 50.0 && player->frame_index == 0.0,
        "time seek synchronizes the displayed frame");
  spritesheet::stop_spritesheet_player(player);
  check(player->complete && player->elapsed == 0.0 && player->frame_index == 0.0,
        "stop restores the completed origin state");
}

void directions_and_variable_timing() {
  auto player = spritesheet::create_spritesheet_player();
  auto reverse = animation({0.0, 1.0, 2.0, 3.0}, 100.0, -1.0, "reverse");
  spritesheet::play_spritesheet_animation(player, readonly(reverse));
  spritesheet::update_spritesheet_player(player, 0.0);
  check(player->frame_index == 3.0, "reverse starts at the last display frame");
  spritesheet::update_spritesheet_player(player, 100.0);
  check(player->frame_index == 2.0, "reverse advances backward");

  auto pingpong = animation({0.0, 1.0, 2.0}, 100.0, -1.0, "pingpong");
  spritesheet::play_spritesheet_animation(player, readonly(pingpong));
  spritesheet::update_spritesheet_player(player, 300.0);
  check(player->frame_index == 1.0, "pingpong follows the backward virtual leg");
  spritesheet::update_spritesheet_player(player, 100.0);
  check(player->frame_index == 0.0, "pingpong wraps after four virtual frames");

  auto variable = animation({0.0, 1.0, 2.0});
  variable->frame_durations = flight::Array<double>{50.0, 200.0, 150.0};
  spritesheet::play_spritesheet_animation(player, readonly(variable));
  spritesheet::update_spritesheet_player(player, 50.0);
  check(player->frame_index == 1.0, "per-frame timing enters the second frame at 50ms");
  spritesheet::update_spritesheet_player(player, 200.0);
  check(player->frame_index == 2.0, "per-frame timing enters the third frame at 250ms");
}

void validation() {
  auto valid = spritesheet::create_spritesheet();
  check(!spritesheet::validate_spritesheet(readonly(valid)).has_value(),
        "an empty runtime sheet is structurally valid");

  auto atlas = flight::make_ref<flight::types::TextureAtlas>();
  auto region = flight::make_ref<flight::types::TextureAtlasRegion>();
  region->id = 4.0;
  atlas->regions.push(region);

  auto frame = spritesheet::create_spritesheet_frame();
  frame->id = 9.0;
  valid->atlas = atlas;
  valid->frames.push(frame);
  valid->animations.set(flight::String("empty"), animation({}));
  valid->animations.set(flight::String("bad"), animation({2.0}));

  auto diagnostics = spritesheet::validate_spritesheet(readonly(valid));
  check(diagnostics.has_value() && diagnostics->size() == 3,
        "validation reports atlas, empty-animation and range defects independently");
  const auto missing_region = diagnostics->element(0.0);
  const auto empty_animation = diagnostics->element(1.0);
  const auto bad_animation = diagnostics->element(2.0);
  check(missing_region->severity == flight::String("error") &&
            !missing_region->animation_name.has_value() && missing_region->frame_index == 0.0 &&
            missing_region->message == flight::String(
                "Frame 0 references atlas region id 9 which does not exist in the atlas."),
        "validation emits the exact missing-region diagnostic first");
  check(empty_animation->severity == flight::String("warning") &&
            empty_animation->animation_name == flight::String("empty") &&
            !empty_animation->frame_index.has_value() &&
            empty_animation->message == flight::String("Animation \"empty\" has no frames."),
        "validation retains animation insertion order and the exact empty warning");
  check(bad_animation->severity == flight::String("error") &&
            bad_animation->animation_name == flight::String("bad") &&
            bad_animation->frame_index == 0.0 &&
            bad_animation->message == flight::String(
                "Animation \"bad\" references frame index 2 which is out of range (sheet has 1 frames)."),
        "validation emits the exact out-of-range diagnostic after earlier animations");
}

void queue_signals_and_pool() {
  auto first = animation({0.0, 1.0}, 100.0, 0.0);
  auto second = animation({2.0}, 100.0, 0.0);
  auto third = animation({3.0}, 100.0, 0.0);
  auto player = spritesheet::create_spritesheet_player();
  int loops = 0;
  int completions = 0;
  flight::signals::connect_signal(player->on_loop, std::function<void()>([&]() { ++loops; }));
  flight::signals::connect_signal(player->on_complete, std::function<void()>([&]() { ++completions; }));

  spritesheet::play_spritesheet_animation(player, readonly(first));
  spritesheet::queue_spritesheet_animation(player, readonly(second));
  spritesheet::queue_spritesheet_animation(player, readonly(third));
  auto clone = spritesheet::clone_spritesheet_player(readonly(player));
  check(clone != player && clone->animation.value() == first && clone->queue.size() == 2 &&
            clone->queue.element(0.0) == second && clone->queue.element(1.0) == third &&
            clone->on_complete != player->on_complete && clone->on_loop != player->on_loop,
        "player clone preserves animation and FIFO identities with independent signals");
  static_cast<void>(clone->queue.pop());
  check(clone->queue.size() == 1 && player->queue.size() == 2,
        "player clone owns an independent queue");
  spritesheet::update_spritesheet_player(player, 200.0);
  check(player->animation.value() == second && player->queue.size() == 1 &&
            player->queue.element(0.0) == third && !player->complete,
        "exhaustion advances to the first queued animation without reordering the remainder");
  check(completions == 0, "queue advancement does not emit completion");
  spritesheet::update_spritesheet_player(player, 100.0);
  check(player->animation.value() == third && player->queue.empty() && !player->complete &&
            completions == 0,
        "a second exhaustion consumes the queue in FIFO order without completion");
  spritesheet::update_spritesheet_player(player, 100.0);
  check(player->complete && completions == 1 && loops == 0,
        "the final finite animation completes exactly once without a loop event");
  check(!spritesheet::update_spritesheet_player(player, 100.0) && completions == 1,
        "completed playback is idempotent");

  auto released = player;
  released->speed = 4.0;
  released->paused = true;
  spritesheet::release_spritesheet_player(released);
  released->on_complete->emit();
  check(completions == 1, "release disconnects player signals");
  auto acquired = spritesheet::acquire_spritesheet_player();
  check(acquired == released && !acquired->animation.has_value() && acquired->complete &&
            !acquired->paused && acquired->speed == 1.0 && acquired->queue.empty(),
        "acquire reuses a released player in a clean idle state");
}

}  // namespace

int main() {
  construction_and_clone();
  frame_lookup_and_controls();
  directions_and_variable_timing();
  validation();
  queue_signals_and_pool();
  return failures == 0 ? 0 : 1;
}
