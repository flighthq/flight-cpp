#pragma once

#include <cmath>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include <flight/array.hpp>
#include <flight/typed_array.hpp>

namespace flight {

template <typename Value>
class SequenceView {
 public:
  using size_type = std::size_t;
  using value_type = Value;

  class Length {
   public:
    explicit Length(const SequenceView* owner) : owner_(owner) {}
    [[nodiscard]] operator size_type() const { return owner_->size(); }
    [[nodiscard]] size_type operator()() const { return owner_->size(); }

   private:
    const SequenceView* owner_;
  };

  class const_iterator {
   public:
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::input_iterator_tag;
    using value_type = Value;

    const_iterator() = default;

    [[nodiscard]] Value operator*() const { return (*owner_)[index_]; }
    const_iterator& operator++() {
      ++index_;
      return *this;
    }
    const_iterator operator++(int) {
      auto previous = *this;
      ++*this;
      return previous;
    }
    [[nodiscard]] friend bool operator==(const const_iterator&, const const_iterator&) noexcept = default;

   private:
    friend class SequenceView;
    const_iterator(const SequenceView* owner, size_type index) : owner_(owner), index_(index) {}

    const SequenceView* owner_{nullptr};
    size_type index_{0};
  };

  Length length{this};

  SequenceView()
      : size_([] { return size_type{0}; }),
        get_([](size_type) -> Value { throw std::out_of_range("empty Flight sequence view"); }) {}

  SequenceView(const SequenceView& other)
      : length(this), identity_(other.identity_), size_(other.size_), get_(other.get_) {}

  SequenceView(SequenceView&& other) noexcept
      : length(this),
        identity_(std::exchange(other.identity_, nullptr)),
        size_(std::move(other.size_)),
        get_(std::move(other.get_)) {}

  SequenceView& operator=(const SequenceView& other) {
    identity_ = other.identity_;
    size_ = other.size_;
    get_ = other.get_;
    return *this;
  }

  SequenceView& operator=(SequenceView&& other) noexcept {
    identity_ = std::exchange(other.identity_, nullptr);
    size_ = std::move(other.size_);
    get_ = std::move(other.get_);
    return *this;
  }

  template <typename SourceValue>
    requires std::constructible_from<Value, const SourceValue&>
  SequenceView(Array<SourceValue> source)
      : identity_(source.identity()),
        size_([source] { return source.size(); }),
        get_([source](size_type index) { return Value(source[index]); }) {}

  template <typename SourceValue>
    requires std::constructible_from<Value, const SourceValue&>
  SequenceView(TypedArray<SourceValue> source)
      : identity_(source.identity()),
        size_([source] { return source.size(); }),
        get_([source](size_type index) { return Value(source[index]); }) {}

  template <typename Source>
    requires requires(const Source& source, size_type index) {
      { source.size() } -> std::convertible_to<size_type>;
      Value(source[index]);
    }
  [[nodiscard]] static SequenceView from_shared(std::shared_ptr<Source> source) {
    if (!source) throw std::invalid_argument("Flight sequence view requires a shared source owner");
    SequenceView result;
    result.identity_ = source.get();
    result.size_ = [source] { return static_cast<size_type>(source->size()); };
    result.get_ = [source](size_type index) { return Value((*source)[index]); };
    return result;
  }

  [[nodiscard]] Value operator[](size_type index) const { return get_(index); }

  [[nodiscard]] std::optional<Value> at(std::ptrdiff_t index) const {
    const auto normalized = normalize_index(index);
    return normalized ? std::optional<Value>((*this)[*normalized]) : std::nullopt;
  }

  [[nodiscard]] Value element(double index) const {
    if (!std::isfinite(index) || index < 0.0 || std::trunc(index) != index ||
        index >= static_cast<double>(size())) {
      throw std::range_error("Flight sequence view index is outside the view");
    }
    return (*this)[static_cast<size_type>(index)];
  }

  [[nodiscard]] std::optional<Value> get(double index) const {
    if (!std::isfinite(index) || index < 0.0 || std::trunc(index) != index ||
        index >= static_cast<double>(size())) {
      return std::nullopt;
    }
    return (*this)[static_cast<size_type>(index)];
  }

  [[nodiscard]] size_type size() const { return size_(); }
  [[nodiscard]] bool empty() const { return size() == 0; }
  [[nodiscard]] const void* identity() const noexcept { return identity_; }
  [[nodiscard]] const_iterator begin() const noexcept { return const_iterator(this, 0); }
  [[nodiscard]] const_iterator end() const { return const_iterator(this, size()); }

 private:
  [[nodiscard]] std::optional<size_type> normalize_index(std::ptrdiff_t index) const {
    const auto count = size();
    if (index < 0) {
      const auto magnitude = static_cast<size_type>(-(index + 1)) + 1;
      if (magnitude > count) return std::nullopt;
      return count - magnitude;
    }
    const auto positive = static_cast<size_type>(index);
    return positive < count ? std::optional<size_type>(positive) : std::nullopt;
  }

  const void* identity_{nullptr};
  std::function<size_type()> size_;
  std::function<Value(size_type)> get_;
};

} // namespace flight
