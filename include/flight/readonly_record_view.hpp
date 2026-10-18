#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <variant>

#include <flight/structural_ref.hpp>

namespace flight {

namespace detail {

template <typename Value>
inline constexpr bool readonly_record_primitive =
    std::same_as<Value, bool> || std::same_as<Value, double> || std::same_as<Value, String>;

template <typename Value>
struct ReadonlyRecordClosedDomain : std::bool_constant<readonly_record_primitive<Value>> {};

template <typename... Values>
struct ReadonlyRecordClosedDomain<std::variant<Values...>>
    : std::bool_constant<(readonly_record_primitive<Values> && ...)> {};

template <typename Target, typename Source>
struct ReadonlyRecordContains : std::bool_constant<std::same_as<Target, Source>> {};

template <typename... Targets, typename Source>
struct ReadonlyRecordContains<std::variant<Targets...>, Source>
    : std::bool_constant<(std::same_as<Targets, Source> || ...)> {};

template <typename Target, typename Source>
struct ReadonlyRecordDomainIncludes : ReadonlyRecordContains<Target, Source> {};

template <typename Target, typename... Sources>
struct ReadonlyRecordDomainIncludes<Target, std::variant<Sources...>>
    : std::bool_constant<(ReadonlyRecordContains<Target, Sources>::value && ...)> {};

} // namespace detail

// A typed readonly facade over the existing named-property owner. Converting a narrow record
// retains its shared entry store; it never constructs a wider Record or grants wider write access.
template <typename Value>
class ReadonlyRecordView {
  static_assert(detail::ReadonlyRecordClosedDomain<Value>::value);
  template <typename>
  friend class ReadonlyRecordView;

 public:
  ReadonlyRecordView() : ReadonlyRecordView(Record<String, Value>{}) {}

  template <typename Source>
    requires(detail::ReadonlyRecordClosedDomain<Source>::value &&
             detail::ReadonlyRecordDomainIncludes<Value, Source>::value)
  ReadonlyRecordView(const Record<String, Source>& source)
      : properties_(std::make_shared<detail::TypedRecordRowOwner<Source>>(
            std::make_shared<Record<String, Source>>(source))),
        object_(source.shared_owner()) {}

  template <typename Source>
    requires(detail::ReadonlyRecordDomainIncludes<Value, Source>::value)
  ReadonlyRecordView(const ReadonlyRecordView<Source>& source)
      : properties_(source.properties_), object_(source.object_) {}

  [[nodiscard]] const void* identity() const noexcept { return object_.get(); }
  [[nodiscard]] ErasedRef erased_ref() const { return ErasedRef(object_); }

  [[nodiscard]] bool has(const String& key) const { return properties_.has(key); }
  [[nodiscard]] bool has(double key) const { return has(String::from_number(key)); }

  [[nodiscard]] std::optional<Value> get(const String& key) const {
    if (!has(key)) return std::nullopt;
    auto value = detail::any_to<Value>(properties_.get(key));
    if (!value) throw BadAnyAccess();
    return value;
  }
  [[nodiscard]] std::optional<Value> get(double key) const { return get(String::from_number(key)); }

  [[nodiscard]] Array<String> enumerable_keys() const {
    const auto keys = properties_.keys();
    return Array<String>(keys.begin(), keys.end());
  }
  [[nodiscard]] Array<std::tuple<String, Value>> enumerable_entries() const {
    Array<std::tuple<String, Value>> result;
    for (const auto& key : properties_.keys()) {
      if (auto value = get(key)) result.push(std::tuple<String, Value>(key, std::move(*value)));
    }
    return result;
  }

  template <typename Other>
  [[nodiscard]] bool operator==(const ReadonlyRecordView<Other>& other) const noexcept {
    return identity() == other.identity();
  }
  template <typename Other>
  [[nodiscard]] bool operator==(const Record<String, Other>& other) const noexcept {
    return identity() == other.identity();
  }

 private:
  NamedProperties properties_;
  std::shared_ptr<void> object_;
};

template <typename Value>
[[nodiscard]] Array<String> object_keys(const ReadonlyRecordView<Value>& value) {
  return value.enumerable_keys();
}

template <typename Value>
[[nodiscard]] Array<std::tuple<String, Value>> object_entries(const ReadonlyRecordView<Value>& value) {
  return value.enumerable_entries();
}

template <typename Value>
[[nodiscard]] Array<Value> object_values(const ReadonlyRecordView<Value>& value) {
  Array<Value> result;
  for (const auto& [key, entry] : value.enumerable_entries()) {
    static_cast<void>(key);
    result.push(entry);
  }
  return result;
}

} // namespace flight
