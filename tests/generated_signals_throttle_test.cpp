// The signals throttle module: connectSignalAtFrameRate as emitted (with its two corrections) and the
// connectSignalDebounced / connectSignalThrottled the override supplies. Expectations follow
// packages/signals/src/throttle.ts and its tests at the pinned Flight revision.
#include <flight/signals/throttle.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
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


flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::SignalThrottleOptions>>>> throttle_options(std::optional<bool> leading,
                                                                                                                           std::optional<bool> trailing) {
  auto options = flight::make_ref<flight::types::SignalThrottleOptions>();
  options->leading = leading;
  options->trailing = trailing;
  return flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::SignalThrottleOptions>>>>(options);
}

// The upstream cases the scenarios above do not already pin, at upstream's own values. Upstream drives
// time with vi.advanceTimersByTime; here `advance` sleeps and pumps the host queue, which only ever
// overshoots, so a "has fired by" expectation is exact and a "not yet" expectation holds with the
// upstream margin (50 ms or more). Upstream's one 1 ms-margin intermediate expectation -- nothing has
// fired 99 ms into a 100 ms trailing-only throttle -- cannot be pinned against a real clock and is
// checked only at 0 ms; the host timer queue has no injectable clock.
bool upstream_cases() {
  bool ok = true;
  {
    auto source = flight::signals::create_signal<Slot>();
    std::vector<double> fired;
    flight::signals::connect_signal_at_frame_rate(source, 10.0, [&](double delta) { fired.push_back(delta); });
    source->emit(40.0);
    source->emit(40.0);
    const bool none = fired.empty();
    source->emit(40.0);
    ok = check(none && fired.size() == 1 && std::abs(fired[0] - 120.0) < 0.005,
               "connectSignalAtFrameRate fires the slot only when accumulated delta reaches the period") && ok;
  }
  {
    auto source = flight::signals::create_signal<Slot>();
    std::vector<double> fired;
    flight::signals::connect_signal_at_frame_rate(source, 10.0, [&](double delta) { fired.push_back(delta); });
    source->emit(110.0);
    source->emit(110.0);
    ok = check(fired.size() == 2, "connectSignalAtFrameRate accumulates remainder across firings") && ok;
  }
  {
    auto source = flight::signals::create_signal<std::function<void()>>();
    int count = 0;
    const auto detach = flight::signals::connect_signal_debounced(source, 100.0, [&] { ++count; });
    source->emit();
    advance(50);
    source->emit();
    advance(50);
    const bool not_yet = count == 0;
    advance(50);
    ok = check(not_yet && count == 1, "connectSignalDebounced resets the timer on each emission") && ok;
    detach();
  }
  {
    auto source = flight::signals::create_signal<Slot>();
    std::vector<double> fired;
    const auto detach = flight::signals::connect_signal_throttled(source, 100.0, [&](double value) { fired.push_back(value); });
    source->emit(1.0);
    advance(50);
    source->emit(2.0);
    advance(50);
    const auto contains = [&](double value) { return std::find(fired.begin(), fired.end(), value) != fired.end(); };
    ok = check(contains(1.0) && contains(2.0), "connectSignalThrottled suppresses emissions within the interval") && ok;
    detach();
  }
  {
    auto source = flight::signals::create_signal<std::function<void(double, double)>>();
    std::vector<std::vector<double>> fired;
    const auto detach = flight::signals::connect_signal_throttled(source, 100.0, [&](double x, double y) { fired.push_back({x, y}); });
    source->emit(3.0, 7.0);
    ok = check(fired == std::vector<std::vector<double>>{{3.0, 7.0}}, "connectSignalThrottled preserves the original args (payload-preserving)") && ok;
    detach();
  }
  {
    auto source = flight::signals::create_signal<Slot>();
    std::vector<double> fired;
    const auto detach = flight::signals::connect_signal_throttled(source, 100.0, [&](double value) { fired.push_back(value); }, throttle_options(false, true));
    source->emit(3.0);
    const bool none = fired.empty();
    advance(100);
    ok = check(none && fired == std::vector<double>{3.0}, "connectSignalThrottled fires the first emission on the trailing edge when leading is disabled") && ok;
    detach();
  }
  {
    auto source = flight::signals::create_signal<Slot>();
    std::vector<double> fired;
    const auto detach = flight::signals::connect_signal_throttled(source, 100.0, [&](double value) { fired.push_back(value); }, throttle_options(std::nullopt, false));
    source->emit(3.0);
    advance(50);
    source->emit(7.0);
    advance(100);
    ok = check(fired == std::vector<double>{3.0}, "connectSignalThrottled ignores cooldown emissions when trailing is disabled") && ok;
    detach();
  }
  {
    auto source = flight::signals::create_signal<Slot>();
    std::vector<double> fired;
    const auto detach = flight::signals::connect_signal_throttled(source, 100.0, [&](double value) { fired.push_back(value); }, throttle_options(false, false));
    source->emit(3.0);
    advance(100);
    ok = check(fired.empty(), "connectSignalThrottled does not fire when both leading and trailing are disabled") && ok;
    detach();
  }
  return ok;
}
}  // namespace

int main() {
  bool ok = frame_rate();
  ok = debounce_trailing() && ok;
  ok = debounce_leading() && ok;
  ok = throttle() && ok;
  ok = upstream_cases() && ok;
  return ok ? 0 : 1;
}
