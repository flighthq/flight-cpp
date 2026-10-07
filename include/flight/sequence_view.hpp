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
#include <flight/structural_ref.hpp>
#include <flight/typed_array.hpp>

namespace flight {

namespace detail {

template <typename Target, typename Source>
[[nodiscard]] Target sequence_view_project(const Source& source) {
  using SourceType = std::remove_cvref_t<Source>;
  if constexpr (requires { typename Target::schema_type; } && is_shared_ptr<SourceType>) {
    return structural_ref_cast<Target>(
        StructuralRef<RowWritable<RowOf<SourceType>>>(source));
  } else {
    return Target(source);
  }
}

}  // namespace detail

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
      : length(this),
        identity_(other.identity_),
        float32_array_backed_(other.float32_array_backed_),
        size_(other.size_),
        get_(other.get_) {}

  SequenceView(SequenceView&& other) noexcept
      : length(this),
        identity_(std::exchange(other.identity_, nullptr)),
        float32_array_backed_(std::exchange(other.float32_array_backed_, false)),
        size_(std::move(other.size_)),
        get_(std::move(other.get_)) {}

  SequenceView& operator=(const SequenceView& other) {
    identity_ = other.identity_;
    float32_array_backed_ = other.float32_array_backed_;
    size_ = other.size_;
    get_ = other.get_;
    return *this;
  }

  SequenceView& operator=(SequenceView&& other) noexcept {
    identity_ = std::exchange(other.identity_, nullptr);
    float32_array_backed_ = std::exchange(other.float32_array_backed_, false);
    size_ = std::move(other.size_);
    get_ = std::move(other.get_);
    return *this;
  }

  template <typename SourceValue>
    requires std::constructible_from<Value, const SourceValue&>
  SequenceView(Array<SourceValue> source)
      : identity_(source.identity()),
        size_([source] { return source.size(); }),
        get_([source](size_type index) {
          return detail::sequence_view_project<Value>(source[index]);
        }) {}

  template <typename SourceValue>
    requires std::constructible_from<Value, const SourceValue&>
  SequenceView(TypedArray<SourceValue> source)
      : identity_(source.identity()),
        float32_array_backed_(std::same_as<std::remove_cv_t<SourceValue>, float>),
        size_([source] { return source.size(); }),
        get_([source](size_type index) { return Value(source[index]); }) {}

  template <typename Source>
    requires requires(const Source& source, size_type index) {
      { source.size() } -> std::convertible_to<size_type>;
      Value(source[index]);
    }
  SequenceView(std::shared_ptr<Source> source) {
    if (!source) throw std::invalid_argument("Flight sequence view requires a shared source owner");
    identity_ = source.get();
    size_ = [source] { return static_cast<size_type>(source->size()); };
    get_ = [source](size_type index) { return Value((*source)[index]); };
  }

  template <typename Source>
    requires requires(const Source& source, size_type index) {
      { source.size() } -> std::convertible_to<size_type>;
      Value(source[index]);
    }
  [[nodiscard]] static SequenceView from_shared(std::shared_ptr<Source> source) {
    return SequenceView(std::move(source));
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
  // ArrayLike<number> erases its concrete Array/typed-array carrier into SequenceView<double>.
  // AnimationTrack's clone contract nevertheless promises to preserve Float32Array backing, so
  // retain that one observable carrier fact alongside the already-retained identity. This is a
  // query only: it introduces none of the optional- or index-like names used by generic probes.
  [[nodiscard]] bool is_float32_array_backed() const noexcept {
    return float32_array_backed_;
  }
  [[nodiscard]] const_iterator begin() const noexcept { return const_iterator(this, 0); }
  [[nodiscard]] const_iterator end() const { return const_iterator(this, size()); }

  // `Array.prototype.map` over a read-only view, returning a real Array as JavaScript does.
  //
  // A SequenceView does not own its elements -- it reads them through `get_`, which is how a row cell or
  // a typed-array window presents a sequence without copying it. `map` is still the right shape for it:
  // the result is a new Array and the view is untouched, so nothing here depends on ownership.
  //
  // Mirrors `Array::map` exactly, including passing the INDEX as the callback's second argument and
  // deducing the element type from what the callback returns, so emitted code that works over an Array
  // works unchanged over a view. `detail::invoke_array_callback` is what makes a one-argument callback
  // acceptable too, which is the common case in emitted code.
  template <typename Transform>
  [[nodiscard]] auto map(Transform&& transform) const
      -> Array<std::remove_cvref_t<decltype(detail::invoke_array_callback(
          transform, std::declval<const Value&>(), std::declval<size_type>()))>> {
    using Result = std::remove_cvref_t<decltype(detail::invoke_array_callback(
        transform, std::declval<const Value&>(), std::declval<size_type>()))>;
    Array<Result> result;
    for (size_type index = 0; index < size(); ++index) {
      const Value element = (*this)[index];
      result.push(detail::invoke_array_callback(transform, element, index));
    }
    return result;
  }

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
  bool float32_array_backed_{false};
  std::function<size_type()> size_;
  std::function<Value(size_type)> get_;
};

} // namespace flight
