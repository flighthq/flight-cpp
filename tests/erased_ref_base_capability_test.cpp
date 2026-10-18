#include <flight/erased_ref.hpp>

#include <iostream>
#include <memory>
#include <typeindex>

namespace {
struct Padding { double padding = 5.0; };
struct Base { int value = 7; };
struct ApplicationOwner : Padding, Base { int application_state = 11; };
struct Unrelated { int value = 7; };
class PrivateOwner : private Base {};

template <typename Parent, typename Owner>
concept CanDeclareBase = requires(std::shared_ptr<Owner> owner) {
  flight::ErasedRef::with_declared_base<Parent>(owner);
};
static_assert(CanDeclareBase<Base, ApplicationOwner>);
static_assert(CanDeclareBase<Base, Base>);
static_assert(!CanDeclareBase<Unrelated, ApplicationOwner>);
static_assert(!CanDeclareBase<Base, PrivateOwner>);

int failures = 0;
void check(bool value, const char* message) {
  if (!value) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
} // namespace

int main() {
  std::weak_ptr<ApplicationOwner> lifetime;
  flight::ErasedRef retained;
  {
    const auto owner = std::make_shared<ApplicationOwner>();
    lifetime = owner;
    const flight::ErasedRef ordinary(owner);
    check(ordinary.as<ApplicationOwner>() == owner && !ordinary.as<Base>() &&
              !ordinary.declared_base_as<Base>(),
          "ordinary erasure acquired an undeclared base capability");
    auto declared = flight::ErasedRef::with_declared_base<Base>(owner);
    const auto base = declared.declared_base_as<Base>();
    const auto exact = declared.as<ApplicationOwner>();
    check(exact == owner && declared == ordinary && declared.identity() == owner.get() &&
              declared.held_type() == std::type_index(typeid(ApplicationOwner)) &&
              declared.holds<ApplicationOwner>() && !declared.holds<Base>() &&
              !declared.as<Base>(),
          "declaring a base changed exact recovery, identity or held type");
    check(base && base.get() == static_cast<Base*>(owner.get()) &&
              !base.owner_before(owner) && !owner.owner_before(base),
          "the declared base lost its adjusted pointer or original control block");
    check(!declared.declared_base_as<Unrelated>() &&
              !declared.declared_base_as<ApplicationOwner>(),
          "base recovery inferred an unregistered relationship");
    if (base) base->value = 19;
    check(owner->value == 19 && exact->application_state == 11,
          "the base capability copied the owner or its live cell");
    retained = declared;
    declared = {};
    check(retained.as<ApplicationOwner>() == owner &&
              retained.declared_base_as<Base>().get() == static_cast<Base*>(owner.get()),
          "copying and resetting an erased reference lost its declared capability");
  }
  check(!lifetime.expired(), "a retained capability released its owner too early");
  auto moved = std::move(retained);
  check(!retained && !retained.declared_base_as<Base>() &&
            moved.as<ApplicationOwner>() && moved.declared_base_as<Base>(),
        "moving a capability left an owner behind or lost its proof");
  retained = std::move(moved);
  retained = {};
  check(lifetime.expired(), "a declared capability kept its owner after final release");
  const auto empty = flight::ErasedRef::with_declared_base<Base>(
      std::shared_ptr<ApplicationOwner>{});
  check(!empty && !empty.as<ApplicationOwner>() && !empty.declared_base_as<Base>() &&
            empty.held_type() == std::type_index(typeid(void)),
        "a null reference created an owner or capability");
  const auto base_owner = std::make_shared<Base>();
  const auto same = flight::ErasedRef::with_declared_base<Base>(base_owner);
  check(same.as<Base>() == base_owner && same.declared_base_as<Base>() == base_owner,
        "an exact base owner did not retain both explicit and exact access");
  return failures == 0 ? 0 : 1;
}
