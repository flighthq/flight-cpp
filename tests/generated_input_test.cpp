// Focused behavior tests for the re-derived @flighthq/input key-repeat timer.
//
// The module's override used to satisfy this interface by calling the callback once and doing nothing
// else, which is what the increment that re-derived it replaced. These checks pin the contract the
// source documents: the callback runs immediately on press, again after the delay, then every interval
// until stop(); and a new start cancels whatever the previous one left behind.
//
// Time is driven by pumping the queue rather than by waiting: the delay and interval are zero, so every
// step is due and the assertions are about order and repetition, not about how long anything took.
#include <flight/host/timers.hpp>
#include <flight/input/input_manager.hpp>

#include <functional>
#include <iostream>
#include <memory>

namespace {

int failures = 0;

bool check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
  return condition;
}

// The row view emitted code takes its options through, built the same way.
flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::InputKeyRepeatOptions>>>> row_of(
    const flight::Ref<flight::types::InputKeyRepeatOptions>& options) {
  return flight::structural_ref_cast<flight::StructuralRef<
      flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::InputKeyRepeatOptions>>>>>(
      flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::InputKeyRepeatOptions>>>>(
          options));
}

}  // namespace

int main() {
  auto options = flight::make_ref<flight::types::InputKeyRepeatOptions>();
  options->delay = 0.0;
  options->interval = 0.0;
  auto timer = flight::input::create_input_key_repeat_timer(row_of(options));

  int calls = 0;
  timer->start([&calls]() { ++calls; });
  check(calls == 1, "start invokes the callback immediately on press");
  check(flight::host::pump_timers() >= 1, "the delay is a scheduled timer, not an inline call");
  check(calls == 2, "the delay fires the callback once and schedules the repeat");

  check(flight::host::pump_timers() >= 1, "the repeat is a scheduled timer");
  check(calls == 3, "the interval repeats while the key is held");
  check(flight::host::pump_timers() >= 1, "the repeat is still scheduled");
  check(calls == 4, "and keeps repeating");

  timer->stop();
  const int calls_at_stop = calls;
  flight::host::pump_timers();
  check(calls == calls_at_stop, "stop cancels the pending repeat");

  // A second press: start cancels whatever the previous start left behind before calling.
  timer->start([&calls]() { ++calls; });
  check(calls == calls_at_stop + 1, "a second start calls immediately again");
  flight::host::pump_timers();
  check(calls == calls_at_stop + 2, "and its own delay runs");

  // Replacement mid-flight: the pending pair from the previous start must not survive into this one.
  timer->start([&calls]() { ++calls; });
  const int replaced_calls = calls;
  check(replaced_calls == calls_at_stop + 3, "a replacement calls immediately");
  flight::host::pump_timers();
  check(calls == replaced_calls + 1, "the replacement's delay runs once, not twice");
  timer->stop();
  const int after_replacement_stop = calls;
  flight::host::pump_timers();
  check(calls == after_replacement_stop, "and its stop cancels it");

  // stop with nothing pending is an ordinary no-op, not a crash and not a callback.
  timer->stop();
  check(calls == after_replacement_stop, "stop with nothing pending does nothing");

  if (failures == 0) std::cout << "generated input: all checks passed\n";
  return failures == 0 ? 0 : 1;
}
