#pragma once

#include <optional>
#include <stdexcept>
#include <string>
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
  // Every access names its member by a `flight::RowKey`, which is also what puts the member into the
  // generated SDK structural table that binds an object's real fields by name.
  template <typename Key, typename Member, auto Field>
  [[nodiscard]] MemberValue<Member> read() const {
    const std::string_view name = Key::name.view();
    if (nominal_) return MemberValue<Member>{std::optional<Member>{(*nominal_).*Field}};
    // The recorded owner first (it also holds by-name cells written earlier), then, when it does not bind
    // this member, the object's EXACT allocated type through a transient owner. The transient owner is
    // built for this read, holds no cell beyond the object's generated members, and is registered nowhere,
    // so it changes neither the registry nor the object. It allocates: a by-name read of a non-nominal
    // object costs one transient owner, while the nominal fast path allocates nothing.
    if (auto value = read_from<Member>(detail::find_row_owner(static_cast<const void*>(object_.get())), name)) {
      return std::move(*value);
    }
    const auto identity = detail::recoverable_owner_identity(object_);
    if (identity && identity->view_row_owner) {
      const auto transient = std::static_pointer_cast<RowOwner>(identity->view_row_owner(identity->object));
      if (auto value = read_from<Member>(transient, name)) return std::move(*value);
    }
    return MemberValue<Member>{};
  }

  // Reads a member whose declared type itself admits absence (`T | null`): the declared absent state when
  // the object does not declare the member, which is how the source reads it, so presence tests and
  // narrowing work on the result as on any optional member.
  template <typename Key, typename Member, auto Field>
  [[nodiscard]] Member read_optional() const {
    auto value = read<Key, Member, Field>();
    return value.has_value() ? static_cast<const Member&>(value) : Member{};
  }

  // Writes `name` on the same object. False only when the object has neither a nominal owner nor
  // binding metadata, so there is no same-object storage to write through.
  template <typename Key, typename Member, auto Field>
  bool write(Member value) const {
    const std::string_view name = Key::name.view();
    if (nominal_) {
      (*nominal_).*Field = std::move(value);
      return true;
    }
    // A write needs the object's registered owner, so a by-name cell outlives this access. That owner is
    // bound to the exact allocated type's members, so a declared native member is its real field.
    if (const auto identity = detail::recoverable_owner_identity(object_); identity && identity->own_row_owner) {
      (void)identity->own_row_owner(identity->object);
    }
    if (const auto owner = detail::find_row_owner(static_cast<const void*>(object_.get()));
        owner && owner->named_cell(name) && owner->template named_value_if<Member>(name) == nullptr) {
      // A real member of another C++ type is never replaced by a by-name cell of this one.
      throw_member_type_mismatch(name);
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
  // A member the object really declares, but with a type this carrier cannot read (a caller-declared
  // callback whose parameter is the nominal owner, for one), cannot be reached faithfully. Report it
  // where it is accessed rather than treating it as absent, which would silently skip a call the source
  // makes, or replacing its real cell with a new one.
  [[noreturn]] static void throw_member_type_mismatch(std::string_view name) {
    throw std::logic_error(std::string("flight::NominalOrStructuralView: member '") + std::string(name) +
                           "' is declared with a type the carrier cannot pass through");
  }

  template <typename Member>
  [[nodiscard]] static std::optional<MemberValue<Member>> read_from(const std::shared_ptr<RowOwner>& owner,
                                                                    std::string_view name) {
    if (!owner) return std::nullopt;
    if (auto* value = owner->template named_value_if<Member>(name)) return MemberValue<Member>{*value};
    if (const auto cell = owner->named_cell(name)) {
      // The member exists with another C++ type. Holding null or undefined, it is absent to the source as
      // well (a `?.` call does not happen), so it reads as absent; holding a value, it is unreachable.
      if (const auto erased = cell->as_any(); erased && erased->is_nullish()) return MemberValue<Member>{};
      throw_member_type_mismatch(name);
    }
    return std::nullopt;
  }

  [[nodiscard]] static const void* identity_of(const Ref<Base>& object) noexcept {
    return object ? static_cast<const void*>(object.get()) : nullptr;
  }

  Ref<Base> object_{};
  Ref<Nominal> nominal_{};
};

} // namespace flight
