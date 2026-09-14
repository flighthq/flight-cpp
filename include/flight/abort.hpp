#pragma once

#include <algorithm>
#include <any>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include <flight/callable.hpp>
#include <flight/string.hpp>

namespace flight {

class AbortReason final {
 public:
  AbortReason() = default;

  template <typename Value>
    requires(!std::same_as<std::remove_cvref_t<Value>, AbortReason>)
  AbortReason(Value&& value) : value_(std::forward<Value>(value)) {}

  [[nodiscard]] bool has_value() const noexcept { return value_.has_value(); }

  template <typename Value>
  [[nodiscard]] const Value& get() const {
    const auto* value = std::any_cast<Value>(&value_);
    if (value == nullptr) throw std::bad_any_cast();
    return *value;
  }

 private:
  std::any value_;
};

class AbortError final : public std::runtime_error {
 public:
  explicit AbortError(AbortReason reason)
      : std::runtime_error("Flight operation was aborted"), reason_(std::move(reason)) {}

  [[nodiscard]] const AbortReason& reason() const noexcept { return reason_; }

 private:
  AbortReason reason_;
};

struct AbortEventListenerOptions final {
  bool once{false};
};

namespace detail {

struct AbortListener final {
  const void* source_identity;
  Function<void()> callback;
  bool once;
};

struct AbortState final {
  mutable std::mutex mutex;
  bool aborted{false};
  AbortReason reason;
  std::vector<AbortListener> listeners;
};

} // namespace detail

class AbortBooleanProperty final {
 public:
  AbortBooleanProperty() = default;
  explicit AbortBooleanProperty(std::shared_ptr<detail::AbortState> state) noexcept
      : state_(std::move(state)) {}

  [[nodiscard]] operator bool() const noexcept {
    if (!state_) return false;
    const std::scoped_lock lock(state_->mutex);
    return state_->aborted;
  }

 private:
  std::shared_ptr<detail::AbortState> state_;
};

class AbortReasonProperty final {
 public:
  AbortReasonProperty() = default;
  explicit AbortReasonProperty(std::shared_ptr<detail::AbortState> state) noexcept
      : state_(std::move(state)) {}

  [[nodiscard]] AbortReason snapshot() const {
    if (!state_) return {};
    const std::scoped_lock lock(state_->mutex);
    return state_->reason;
  }

  [[nodiscard]] bool has_value() const {
    if (!state_) return false;
    const std::scoped_lock lock(state_->mutex);
    return state_->reason.has_value();
  }

  template <typename Value>
  [[nodiscard]] const Value& get() const {
    if (!state_) throw std::bad_any_cast();
    const std::scoped_lock lock(state_->mutex);
    return state_->reason.get<Value>();
  }

 private:
  std::shared_ptr<detail::AbortState> state_;
};

class AbortController;

class AbortSignal final {
 private:
  std::shared_ptr<detail::AbortState> state_;

 public:
  using weak_type = std::weak_ptr<detail::AbortState>;

  AbortSignal()
      : AbortSignal(std::make_shared<detail::AbortState>()) {}

  [[nodiscard]] friend bool operator==(const AbortSignal& left, const AbortSignal& right) noexcept {
    return left.state_ == right.state_;
  }
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }
  [[nodiscard]] weak_type weaken() const noexcept { return state_; }

  [[nodiscard]] static std::optional<AbortSignal> lock_weak(const weak_type& weak) noexcept {
    auto state = weak.lock();
    return state ? std::optional<AbortSignal>(AbortSignal(std::move(state))) : std::nullopt;
  }

  void add_event_listener(
      const String& type,
      const std::function<void()>& callback,
      AbortEventListenerOptions options = {}) const {
    add_event_listener_impl(
        type,
        Function<void()>(callback),
        std::addressof(callback),
        options);
  }

  void add_event_listener(
      const String& type,
      Function<void()> callback,
      AbortEventListenerOptions options = {}) const {
    const auto identity = callback.identity();
    add_event_listener_impl(type, std::move(callback), identity, options);
  }

  template <typename Implementation>
    requires(
        !std::same_as<std::remove_cvref_t<Implementation>, Function<void()>> &&
        !std::same_as<std::remove_cvref_t<Implementation>, std::function<void()>> &&
        std::invocable<std::remove_reference_t<Implementation>&>)
  void add_event_listener(
      const String& type,
      Implementation&& callback,
      AbortEventListenerOptions options = {}) const {
    add_event_listener(type, Function<void()>(std::forward<Implementation>(callback)), options);
  }

  void remove_event_listener(const String& type, const std::function<void()>& callback) const {
    remove_event_listener_impl(type, std::addressof(callback));
  }

  void remove_event_listener(const String& type, const Function<void()>& callback) const {
    remove_event_listener_impl(type, callback.identity());
  }

  void throw_if_aborted() const {
    AbortReason current_reason;
    {
      const std::scoped_lock lock(state_->mutex);
      if (!state_->aborted) return;
      current_reason = state_->reason;
    }
    throw AbortError(std::move(current_reason));
  }

  AbortBooleanProperty aborted;
  AbortReasonProperty reason;

 private:
  friend class AbortController;

  explicit AbortSignal(std::shared_ptr<detail::AbortState> state) noexcept
      : state_(std::move(state)), aborted(state_), reason(state_) {}

  void add_event_listener_impl(
      const String& type,
      Function<void()> callback,
      const void* identity,
      AbortEventListenerOptions options) const {
    if (type != String("abort")) return;
    const std::scoped_lock lock(state_->mutex);
    if (state_->aborted) return;
    if (std::ranges::any_of(state_->listeners, [&](const detail::AbortListener& listener) {
          return listener.source_identity == identity;
        })) {
      return;
    }
    state_->listeners.push_back(
        detail::AbortListener{identity, std::move(callback), options.once});
  }

  void remove_event_listener_impl(const String& type, const void* identity) const {
    if (type != String("abort")) return;
    const std::scoped_lock lock(state_->mutex);
    std::erase_if(state_->listeners, [&](const detail::AbortListener& listener) {
      return listener.source_identity == identity;
    });
  }

  void abort(AbortReason reason_value) const {
    std::vector<Function<void()>> callbacks;
    {
      const std::scoped_lock lock(state_->mutex);
      if (state_->aborted) return;
      state_->aborted = true;
      state_->reason = std::move(reason_value);
      callbacks.reserve(state_->listeners.size());
      for (auto& listener : state_->listeners) callbacks.push_back(std::move(listener.callback));
      state_->listeners.clear();
    }
    for (auto& callback : callbacks) {
      try {
        callback();
      } catch (...) {
        // EventTarget listener exceptions are reported independently and do not make abort throw.
      }
    }
  }
};

class AbortController final {
 public:
  AbortController() = default;

  void abort() const { signal.abort(AbortReason(String("This operation was aborted"))); }

  void abort(const AbortReasonProperty& reason) const { signal.abort(reason.snapshot()); }

  template <typename Reason>
    requires(!std::same_as<std::remove_cvref_t<Reason>, AbortReasonProperty>)
  void abort(Reason&& reason) const {
    signal.abort(AbortReason(std::forward<Reason>(reason)));
  }

  AbortSignal signal;
};

} // namespace flight
