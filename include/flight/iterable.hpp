#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include <flight/array.hpp>
#include <flight/map.hpp>
#include <flight/set.hpp>

namespace flight {

// Type-erased synchronous iterable used when a TypeScript API accepts Iterable<T> rather than one
// concrete collection. Flight containers retain their shared backing storage in the factory, so a
// fresh begin() observes the current collection and an active iterator follows JavaScript's live
// Array, Map, and Set traversal rules.
template <typename Value>
class Iterable final {
 public:
  using Next = std::function<std::optional<Value>()>;
  using Factory = std::function<Next()>;

  class iterator final {
   public:
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::input_iterator_tag;
    using iterator_concept = std::input_iterator_tag;
    using pointer = const Value*;
    using reference = const Value&;
    using value_type = Value;

    iterator() = default;

    [[nodiscard]] reference operator*() const { return *current_; }
    [[nodiscard]] pointer operator->() const { return std::addressof(*current_); }

    iterator& operator++() {
      current_ = next_ ? next_() : std::nullopt;
      return *this;
    }

    void operator++(int) { ++*this; }

    [[nodiscard]] friend bool operator==(const iterator& value, std::default_sentinel_t) noexcept {
      return !value.current_.has_value();
    }

   private:
    friend class Iterable;

    explicit iterator(Next next) : next_(std::move(next)) { ++*this; }

    Next next_;
    std::optional<Value> current_;
  };

  Iterable() : factory_([] { return Next{}; }) {}

  Iterable(Array<Value> values)
      : factory_([values = std::move(values)] {
          return Next([values, index = std::size_t{0}]() mutable -> std::optional<Value> {
            if (index >= values.size()) return std::nullopt;
            return values[index++];
          });
        }) {}

  template <typename Key, typename Mapped, typename Equal>
    requires std::constructible_from<Value, typename Map<Key, Mapped, Equal>::Entry>
  Iterable(Map<Key, Mapped, Equal> values)
      : factory_([values = std::move(values)] {
          auto entries = values.entries();
          return Next([entries = std::move(entries)]() mutable -> std::optional<Value> {
            auto result = entries.next();
            if (result.done) return std::nullopt;
            return Value(std::move(*result.value));
          });
        }) {}

  template <typename Equal>
  Iterable(Set<Value, Equal> values)
      : factory_([values = std::move(values)] {
          auto entries = values.values();
          return Next([entries = std::move(entries)]() mutable -> std::optional<Value> {
            auto result = entries.next();
            if (result.done) return std::nullopt;
            return std::move(result.value);
          });
        }) {}

  [[nodiscard]] iterator begin() const { return iterator(factory_()); }
  [[nodiscard]] std::default_sentinel_t end() const noexcept { return {}; }

 private:
  Factory factory_;
};

} // namespace flight
