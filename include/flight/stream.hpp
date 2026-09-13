#pragma once

#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

#include <flight/abort.hpp>
#include <flight/task.hpp>

namespace flight {

template <typename Value>
struct ReadableStreamReadResult final {
  bool done{false};
  std::optional<Value> value;
};

template <typename Value>
struct ReadableStreamSource final {
  std::function<Task<ReadableStreamReadResult<Value>>()> read;
  std::function<Task<void>(AbortReason)> cancel;
};

template <typename Value>
struct WritableStreamSink final {
  std::function<Task<void>(const Value&)> write;
  std::function<Task<void>()> close;
  std::function<Task<void>(AbortReason)> abort;
};

namespace detail {

template <typename Result, typename Invocation>
Task<Result> invoke_stream_operation(Invocation&& invocation) {
  try {
    return std::forward<Invocation>(invocation)();
  } catch (...) {
    return Task<Result>::reject(std::current_exception());
  }
}

template <typename Value>
struct ReadableStreamState final {
  explicit ReadableStreamState(ReadableStreamSource<Value> source_value)
      : source(std::move(source_value)) {}

  mutable std::mutex mutex;
  ReadableStreamSource<Value> source;
  bool cancelled{false};
  bool locked{false};
};

template <typename Value>
struct WritableStreamState final {
  explicit WritableStreamState(WritableStreamSink<Value> sink_value)
      : sink(std::move(sink_value)) {}

  mutable std::mutex mutex;
  WritableStreamSink<Value> sink;
  bool closed{false};
  bool locked{false};
};

template <typename State>
class StreamLock final {
 public:
  explicit StreamLock(std::shared_ptr<State> state) : state_(std::move(state)) {}
  ~StreamLock() {
    if (!state_) return;
    const std::scoped_lock lock(state_->mutex);
    state_->locked = false;
  }

  StreamLock(const StreamLock&) = delete;
  StreamLock& operator=(const StreamLock&) = delete;

 private:
  std::shared_ptr<State> state_;
};

} // namespace detail

template <typename Value>
class ReadableStreamDefaultReader final {
 public:
  ReadableStreamDefaultReader() = default;

  [[nodiscard]] Task<ReadableStreamReadResult<Value>> read() const {
    if (!state_) {
      return Task<ReadableStreamReadResult<Value>>::reject(
          std::logic_error("ReadableStream reader is released"));
    }
    {
      const std::scoped_lock lock(state_->mutex);
      if (state_->cancelled || !state_->source.read) {
        return Task<ReadableStreamReadResult<Value>>::resolve(
            ReadableStreamReadResult<Value>{.done = true, .value = std::nullopt});
      }
    }
    return detail::invoke_stream_operation<ReadableStreamReadResult<Value>>(
        [&] { return state_->source.read(); });
  }

  [[nodiscard]] Task<void> cancel() const { return cancel(AbortReason{}); }

  [[nodiscard]] Task<void> cancel(AbortReason reason) const {
    if (!state_) return Task<void>::reject(std::logic_error("ReadableStream reader is released"));
    std::function<Task<void>(AbortReason)> callback;
    {
      const std::scoped_lock lock(state_->mutex);
      if (state_->cancelled) return Task<void>::resolve();
      state_->cancelled = true;
      callback = state_->source.cancel;
    }
    if (!callback) return Task<void>::resolve();
    return detail::invoke_stream_operation<void>([&] { return callback(std::move(reason)); });
  }

  void release_lock() noexcept {
    lease_.reset();
    state_.reset();
  }

 private:
  template <typename>
  friend class ReadableStream;

  explicit ReadableStreamDefaultReader(
      std::shared_ptr<detail::ReadableStreamState<Value>> state,
      std::shared_ptr<detail::StreamLock<detail::ReadableStreamState<Value>>> lease)
      : lease_(std::move(lease)), state_(std::move(state)) {}

  std::shared_ptr<detail::StreamLock<detail::ReadableStreamState<Value>>> lease_;
  std::shared_ptr<detail::ReadableStreamState<Value>> state_;
};

template <typename Value>
class ReadableStream final {
 private:
  using State = detail::ReadableStreamState<Value>;
  std::shared_ptr<State> state_;

 public:
  using weak_type = std::weak_ptr<State>;

  ReadableStream() : ReadableStream(ReadableStreamSource<Value>{}) {}
  explicit ReadableStream(ReadableStreamSource<Value> source)
      : state_(std::make_shared<State>(std::move(source))) {}

  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }
  [[nodiscard]] weak_type weaken() const noexcept { return state_; }

  [[nodiscard]] ReadableStreamDefaultReader<Value> get_reader() const {
    {
      const std::scoped_lock lock(state_->mutex);
      if (state_->locked) throw std::logic_error("ReadableStream is already locked");
      state_->locked = true;
    }
    return ReadableStreamDefaultReader<Value>(
        state_, std::make_shared<detail::StreamLock<State>>(state_));
  }
};

template <typename Value>
class WritableStreamDefaultWriter final {
 public:
  WritableStreamDefaultWriter() = default;

  [[nodiscard]] Task<void> write(const Value& value) const {
    if (!state_) return Task<void>::reject(std::logic_error("WritableStream writer is released"));
    std::function<Task<void>(const Value&)> callback;
    {
      const std::scoped_lock lock(state_->mutex);
      if (state_->closed) return Task<void>::reject(std::logic_error("WritableStream is closed"));
      callback = state_->sink.write;
    }
    if (!callback) return Task<void>::resolve();
    return detail::invoke_stream_operation<void>([&] { return callback(value); });
  }

  [[nodiscard]] Task<void> close() const {
    if (!state_) return Task<void>::reject(std::logic_error("WritableStream writer is released"));
    std::function<Task<void>()> callback;
    {
      const std::scoped_lock lock(state_->mutex);
      if (state_->closed) return Task<void>::resolve();
      state_->closed = true;
      callback = state_->sink.close;
    }
    if (!callback) return Task<void>::resolve();
    return detail::invoke_stream_operation<void>([&] { return callback(); });
  }

  [[nodiscard]] Task<void> abort() const { return abort(AbortReason{}); }

  [[nodiscard]] Task<void> abort(AbortReason reason) const {
    if (!state_) return Task<void>::reject(std::logic_error("WritableStream writer is released"));
    std::function<Task<void>(AbortReason)> callback;
    {
      const std::scoped_lock lock(state_->mutex);
      if (state_->closed) return Task<void>::resolve();
      state_->closed = true;
      callback = state_->sink.abort;
    }
    if (!callback) return Task<void>::resolve();
    return detail::invoke_stream_operation<void>([&] { return callback(std::move(reason)); });
  }

  void release_lock() noexcept {
    lease_.reset();
    state_.reset();
  }

 private:
  template <typename>
  friend class WritableStream;

  explicit WritableStreamDefaultWriter(
      std::shared_ptr<detail::WritableStreamState<Value>> state,
      std::shared_ptr<detail::StreamLock<detail::WritableStreamState<Value>>> lease)
      : lease_(std::move(lease)), state_(std::move(state)) {}

  std::shared_ptr<detail::StreamLock<detail::WritableStreamState<Value>>> lease_;
  std::shared_ptr<detail::WritableStreamState<Value>> state_;
};

template <typename Value>
class WritableStream final {
 private:
  using State = detail::WritableStreamState<Value>;
  std::shared_ptr<State> state_;

 public:
  using weak_type = std::weak_ptr<State>;

  WritableStream() : WritableStream(WritableStreamSink<Value>{}) {}
  explicit WritableStream(WritableStreamSink<Value> sink)
      : state_(std::make_shared<State>(std::move(sink))) {}

  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }
  [[nodiscard]] weak_type weaken() const noexcept { return state_; }

  [[nodiscard]] WritableStreamDefaultWriter<Value> get_writer() const {
    {
      const std::scoped_lock lock(state_->mutex);
      if (state_->locked) throw std::logic_error("WritableStream is already locked");
      state_->locked = true;
    }
    return WritableStreamDefaultWriter<Value>(
        state_, std::make_shared<detail::StreamLock<State>>(state_));
  }
};

template <typename Value>
class AsyncIterable final {
 private:
  struct State final {
    explicit State(std::function<Task<std::optional<Value>>()> next_value)
        : next(std::move(next_value)) {}

    std::function<Task<std::optional<Value>>()> next;
  };

  std::shared_ptr<State> state_;

 public:
  AsyncIterable()
      : AsyncIterable([] { return Task<std::optional<Value>>::resolve(std::nullopt); }) {}
  explicit AsyncIterable(std::function<Task<std::optional<Value>>()> next)
      : state_(std::make_shared<State>(std::move(next))) {
    if (!state_->next) throw std::invalid_argument("AsyncIterable requires a next operation");
  }

  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }
  [[nodiscard]] Task<std::optional<Value>> next() const {
    return detail::invoke_stream_operation<std::optional<Value>>([&] { return state_->next(); });
  }
};

} // namespace flight
