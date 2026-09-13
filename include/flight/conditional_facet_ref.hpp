#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include <flight/reference.hpp>

namespace flight {

namespace detail {

struct ConditionalFacetAcquisition final {};

template <typename Value>
inline constexpr bool is_optional = false;

template <typename Value>
inline constexpr bool is_optional<std::optional<Value>> = true;

template <typename Value>
struct path_value {
  using type = std::remove_cvref_t<Value>;
};

template <typename Value>
struct path_value<std::shared_ptr<Value>> {
  using type = Value;
};

template <typename Value>
using path_value_t = typename path_value<std::remove_cvref_t<Value>>::type;

template <typename Value, auto Accessor, auto... Remaining>
consteval bool has_required_member_path() {
  if constexpr (!requires(Value& value) { Accessor(value); }) {
    return false;
  } else {
    using Member = std::remove_cvref_t<decltype(Accessor(std::declval<Value&>()))>;
    if constexpr (is_optional<Member>) {
      return false;
    } else if constexpr (sizeof...(Remaining) == 0) {
      return true;
    } else {
      return has_required_member_path<path_value_t<Member>, Remaining...>();
    }
  }
}

template <typename Rule, typename Host>
concept ActiveFacetRule = Rule::path_type::template present_in<Host>;

} // namespace detail

template <auto... Accessors>
struct MemberPath {
  static_assert(sizeof...(Accessors) > 0, "a conditional facet member path cannot be empty");

  template <typename Host>
  static constexpr bool present_in = detail::has_required_member_path<Host, Accessors...>();
};

template <typename Tag, typename Path>
struct RequiredMemberFacet {
  using tag_type = Tag;
  using path_type = Path;
};

template <typename Base, typename Tag>
class FacetRef {
 public:
  using base_type = Base;
  using reference_type = Ref<Base>;
  using tag_type = Tag;

  FacetRef() = delete;

  [[nodiscard]] Base* operator->() const noexcept { return reference_.get(); }
  [[nodiscard]] Base& operator*() const noexcept { return *reference_; }
  [[nodiscard]] const Ref<Base>& shared_reference() const noexcept { return reference_; }
  [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(reference_); }

 private:
  template <typename OtherBase, typename Host, typename... Rules>
  friend class ConditionalFacetRef;

  explicit FacetRef(detail::ConditionalFacetAcquisition, Ref<Base> reference)
      : reference_(std::move(reference)) {}

  Ref<Base> reference_;
};

template <typename Target>
concept ConditionalFacetTarget = requires {
  typename Target::base_type;
  typename Target::host_type;
  typename Target::reference_type;
} && std::same_as<typename Target::reference_type, Ref<typename Target::base_type>>;

template <ConditionalFacetTarget Target>
[[nodiscard]] Target assume_conditional_facets(typename Target::reference_type reference);

template <typename Base, typename Host, typename... Rules>
class ConditionalFacetRef {
 public:
  using base_type = Base;
  using host_type = Host;
  using reference_type = Ref<Base>;

  ConditionalFacetRef() = delete;

  [[nodiscard]] Base* operator->() const noexcept { return reference_.get(); }
  [[nodiscard]] Base& operator*() const noexcept { return *reference_; }
  [[nodiscard]] const Ref<Base>& shared_reference() const noexcept { return reference_; }
  [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(reference_); }

  template <typename Tag>
    requires((std::same_as<Tag, typename Rules::tag_type> && detail::ActiveFacetRule<Rules, Host>) || ...)
  [[nodiscard]] operator FacetRef<Base, Tag>() const {
    return FacetRef<Base, Tag>(detail::ConditionalFacetAcquisition{}, reference_);
  }

 private:
  template <ConditionalFacetTarget Target>
  friend Target assume_conditional_facets(typename Target::reference_type);

  explicit ConditionalFacetRef(detail::ConditionalFacetAcquisition, Ref<Base> reference)
      : reference_(std::move(reference)) {}

  Ref<Base> reference_;
};

template <ConditionalFacetTarget Target>
[[nodiscard]] Target assume_conditional_facets(typename Target::reference_type reference) {
  return Target(detail::ConditionalFacetAcquisition{}, std::move(reference));
}

} // namespace flight
