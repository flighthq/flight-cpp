// The signals throttle module: connectSignalAtFrameRate as emitted (with its two corrections) and the
// connectSignalDebounced / connectSignalThrottled the override supplies. Expectations follow
// packages/signals/src/throttle.ts and its tests at the pinned Flight revision.
#include <flight/signals/throttle.hpp>

#include <chrono>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>

#include <flight/signals/emitter.hpp>
#include <flight/signals/signal.hpp>

namespace {

bool check(bool condition, const char* message) {
  if (!condition) std::cerr << message << '\n';
  return condition;
}

using Slot = std::function<void(double)>;

// Runs every timer due after `milliseconds` of real time; the host queue uses a steady clock.
void advance(int milliseconds) {
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
  static_cast<void>(flight::host::timer_queue.pump());
}

bool frame_rate() {
  auto source = flight::signals::create_signal<Slot>();
  std::vector<double> fired;
  const auto cleanup = flight::signals::connect_signal_at_frame_rate(source, 10.0, [&](double elapsed) { fired.push_back(elapsed); });
  flight::signals::emit_signal(source, 60.0);
  flight::signals::emit_signal(source, 60.0);
  flight::signals::emit_signal(source, 90.0);
  bool ok = check(fired == std::vector<double>{120.0, 110.0}, "frame rate accumulates and keeps the remainder");
  // The emitted handler was a bare lambda, so this disconnect threw before the override.
  cleanup();
  flight::signals::emit_signal(source, 500.0);
  return ok && check(fired.size() == 2, "frame rate cleanup disconnects");
}

bool debounce_trailing() {
  auto source = flight::signals::create_signal<Slot>();
  std::vector<double> fired;
  const auto cleanup = flight::signals::connect_signal_debounced(source, 20.0, [&](double value) { fired.push_back(value); });
  flight::signals::emit_signal(source, 1.0);
  flight::signals::emit_signal(source, 2.0);
  bool ok = check(fired.empty(), "debounce waits for quiet");
  advance(40);
  ok = check(fired == std::vector<double>{2.0}, "debounce fires once with the most recent args") && ok;
  flight::signals::emit_signal(source, 3.0);
  cleanup();
  advance(40);
  return check(fired.size() == 1, "debounce cleanup clears the pending timer") && ok;
}

bool debounce_leading() {
  auto source = flight::signals::create_signal<Slot>();
  std::vector<double> fired;
  auto options = flight::make_ref<flight::types::SignalThrottleOptions>();
  options->leading = true;
  options->trailing = false;
  const auto cleanup = flight::signals::connect_signal_debounced(
      source, 20.0, [&](double value) { fired.push_back(value); },
      flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::SignalThrottleOptions>>>>(options));
  flight::signals::emit_signal(source, 1.0);
  flight::signals::emit_signal(source, 2.0);
  bool ok = check(fired == std::vector<double>{1.0}, "leading debounce fires the first call only");
  advance(40);
  ok = check(fired.size() == 1, "leading-only debounce has no trailing call") && ok;
  flight::signals::emit_signal(source, 3.0);
  ok = check(fired == std::vector<double>{1.0, 3.0}, "leading debounce re-arms after the window") && ok;
  cleanup();
  return ok;
}

bool throttle() {
  auto source = flight::signals::create_signal<Slot>();
  std::vector<double> fired;
  const auto cleanup = flight::signals::connect_signal_throttled(source, 50.0, [&](double value) { fired.push_back(value); });
  flight::signals::emit_signal(source, 1.0);
  flight::signals::emit_signal(source, 2.0);
  flight::signals::emit_signal(source, 3.0);
  bool ok = check(fired == std::vector<double>{1.0}, "throttle fires the leading call immediately");
  advance(80);
  ok = check(fired == std::vector<double>{1.0, 3.0}, "throttle trailing call carries the latest args") && ok;
  flight::signals::emit_signal(source, 4.0);
  cleanup();
  advance(80);
  flight::signals::emit_signal(source, 5.0);
  return check(fired == std::vector<double>{1.0, 3.0}, "throttle cleanup disconnects and clears the trailing timer") && ok;
}

}  // namespace

int main() {
  bool ok = frame_rate();
  ok = debounce_trailing() && ok;
  ok = debounce_leading() && ok;
  ok = throttle() && ok;
  return ok ? 0 : 1;
}
