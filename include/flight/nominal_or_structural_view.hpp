#pragma once

#include <string_view>
#include <utility>

#include <flight/reference.hpp>
#include <flight/structural_ref.hpp>

namespace flight {

// A value of a derived interface type reached through its native base, as TypeScript lets it flow after
// a predicate narrows `target: Base` to `Nominal`. JavaScript passes and stores the SAME object, and reads
// and writes its members by name, whether it was allocated as a `Nominal` or is any other object that
// declares the same members. This carrier keeps that object as its identity, and reaches its members
// in one of two ways that are observably identical:
//   * fast path: the allocation IS a `Nominal` (or a subclass), recovered as that same owner; member
//     reads and writes are its native cells;
//   * structural path: any other owner, read and written by source name through the same object's
//     StructuralMemberView. A member the object does not declare reads as absence, and only a USE of that
//     absence fails, which is the source's own `undefined` timing.
// It never casts, copies, replaces, or validates early. A `Ref<Nominal>` converts to it implicitly, so a
// native caller that holds the nominal owner keeps passing it unchanged.
template <typename Nominal, typename Base>
class NominalOrStructuralView final {
 public:
  NominalOrStructuralView() = default;
  NominalOrStructuralView(Ref<Nominal> nominal)  // NOLINT(google-explicit-constructor): source compatibility
      : object_(nominal), nominal_(std::move(nominal)) {}

  // A narrowed base reference: the same object, with its exact owner recovered when it has one.
  [[nodiscard]] static NominalOrStructuralView of(Ref<Base> object) {
    NominalOrStructuralView view;
    view.nominal_ = recover_owner<Nominal>(object);
    view.object_ = std::move(object);
    return view;
  }

  [[nodiscard]] const Ref<Base>& object() const noexcept { return object_; }
  [[nodiscard]] const Ref<Nominal>& nominal() const noexcept { return nominal_; }
  [[nodiscard]] explicit operator bool() const noexcept { return object_ != nullptr; }

  // Reads `name`; `Field` names the same member on the nominal owner for the fast path.
  template <typename Member, auto Field>
  [[nodiscard]] MemberValue<Member> read(std::string_view name) const {
    if (nominal_) return MemberValue<Member>{std::optional<Member>{(*nominal_).*Field}};
    const StructuralMemberView<Base> recorded(object_);
    if (recorded.has_binding_metadata()) return recorded.template read<Member>(name);
    // No recorded owner yet: read the object's own members through its EXACT allocated type, with an
    // owner that is built for this read and registered nowhere. An object with no exact-type evidence
    // reads every member as absent, as before.
    const auto identity = detail::recoverable_owner_identity(object_);
    if (!identity || !identity->view_row_owner) return MemberValue<Member>{};
    const auto owner = std::static_pointer_cast<RowOwner>(identity->view_row_owner(identity->object));
    if (!owner) return MemberValue<Member>{};
    if (auto* value = owner->template named_value_if<Member>(name)) return MemberValue<Member>{*value};
    return MemberValue<Member>{};
  }

  // Writes `name` on the same object. False only when the object has neither a nominal owner nor
  // binding metadata, so there is no same-object storage to write through.
  template <typename Member, auto Field>
  bool write(std::string_view name, Member value) const {
    if (nominal_) {
      (*nominal_).*Field = std::move(value);
      return true;
    }
    // A write needs the object's registered owner so a by-name cell outlives this access; create it from
    // the exact allocated type if the object has none yet. A declared native member is its real field.
    if (!StructuralMemberView<Base>(object_).has_binding_metadata()) {
      const auto identity = detail::recoverable_owner_identity(object_);
      if (identity && identity->own_row_owner) (void)identity->own_row_owner(identity->object);
    }
    return StructuralMemberView<Base>(object_).template write<Member>(name, std::move(value));
  }

  // Identity is the object's: two views of one object are equal whichever path either took.
  [[nodiscard]] friend bool operator==(const NominalOrStructuralView& left, const NominalOrStructuralView& right) noexcept {
    return identity_of(left.object_) == identity_of(right.object_);
  }
  [[nodiscard]] friend bool operator==(const NominalOrStructuralView& view, std::nullptr_t) noexcept {
    return view.object_ == nullptr;
  }

 private:
  [[nodiscard]] static const void* identity_of(const Ref<Base>& object) noexcept {
    return object ? static_cast<const void*>(object.get()) : nullptr;
  }

  Ref<Base> object_{};
  Ref<Nominal> nominal_{};
};

} // namespace flight
