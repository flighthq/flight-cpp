#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace flight::host {

class TimerQueue;

class TimerHandle {
 public:
  TimerHandle() = default;

  [[nodiscard]] explicit operator bool() const noexcept { return queue_ != nullptr; }
  [[nodiscard]] friend bool operator==(TimerHandle, TimerHandle) noexcept = default;

 private:
  friend class TimerQueue;

  TimerHandle(TimerQueue* queue, std::uint64_t identity) : queue_(queue), identity_(identity) {}

  TimerQueue* queue_{nullptr};
  std::uint64_t identity_{0};
};

class TimerQueue {
  using Clock = std::chrono::steady_clock;

 public:
  TimerQueue() = default;
  TimerQueue(const TimerQueue&) = delete;
  TimerQueue& operator=(const TimerQueue&) = delete;

  [[nodiscard]] TimerHandle set_timeout(std::function<void()> callback, double delay_ms) {
    return schedule(std::move(callback), delay_ms, false);
  }

  [[nodiscard]] TimerHandle set_interval(std::function<void()> callback, double delay_ms) {
    return schedule(std::move(callback), delay_ms, true);
  }

  void clear(TimerHandle handle) {
    if (handle.queue_ != this) return;
    std::erase_if(entries_, [&](const Entry& entry) { return entry.identity == handle.identity_; });
  }

  // Execute each timer that was due when this host turn began at most once. Callbacks execute on
  // the pumping thread, preserving JavaScript's serialized timer mutation model.
  [[nodiscard]] std::size_t pump() {
    const auto now = Clock::now();
    const auto maximum_identity = next_identity_ - 1;
    std::vector<std::uint64_t> processed;
    while (true) {
      const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.identity <= maximum_identity && entry.deadline <= now &&
               std::find(processed.begin(), processed.end(), entry.identity) == processed.end();
      });
      if (found == entries_.end()) break;
      const auto identity = found->identity;
      auto callback = found->callback;
      if (found->repeating) found->deadline = now + found->delay;
      else entries_.erase(found);
      processed.push_back(identity);
      callback();
    }
    return processed.size();
  }

  [[nodiscard]] std::optional<std::chrono::milliseconds> delay_until_next() const {
    if (entries_.empty()) return std::nullopt;
    const auto deadline = std::min_element(
        entries_.begin(), entries_.end(), [](const Entry& left, const Entry& right) {
          return left.deadline < right.deadline;
        })->deadline;
    const auto now = Clock::now();
    if (deadline <= now) return std::chrono::milliseconds{0};
    return std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
  }

  [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }

 private:
  struct Entry {
    std::uint64_t identity;
    Clock::time_point deadline;
    Clock::duration delay;
    bool repeating;
    std::function<void()> callback;
  };

  [[nodiscard]] TimerHandle schedule(std::function<void()> callback, double delay_ms, bool repeating) {
    if (!callback) throw std::invalid_argument("Flight timer callback cannot be empty");
    if (next_identity_ == std::numeric_limits<std::uint64_t>::max()) {
      throw std::length_error("Flight timer identity space is exhausted");
    }
    const double normalized = !std::isfinite(delay_ms) || delay_ms < 0.0 ? 0.0 : delay_ms;
    const auto delay = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double, std::milli>(normalized));
    const auto identity = next_identity_++;
    entries_.push_back(Entry{identity, Clock::now() + delay, delay, repeating, std::move(callback)});
    return TimerHandle(this, identity);
  }

  std::vector<Entry> entries_;
  std::uint64_t next_identity_{1};
};

inline thread_local TimerQueue timer_queue;

[[nodiscard]] inline TimerHandle set_timeout(std::function<void()> callback, double delay_ms = 0.0) {
  return timer_queue.set_timeout(std::move(callback), delay_ms);
}

[[nodiscard]] inline TimerHandle set_interval(std::function<void()> callback, double delay_ms = 0.0) {
  return timer_queue.set_interval(std::move(callback), delay_ms);
}

inline void clear_timeout(TimerHandle handle) { timer_queue.clear(handle); }
inline void clear_interval(TimerHandle handle) { timer_queue.clear(handle); }
[[nodiscard]] inline std::size_t pump_timers() { return timer_queue.pump(); }

} // namespace flight::host
