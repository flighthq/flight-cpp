// The exact allocation descriptor: every flight::make_ref allocation records the type it was allocated as,
// in its shared_ptr control block, and nothing else ever sets it. Recovery is the SAME allocation or an honest
// "no provenance"; the escape form reports a mismatch through the shared out-of-profile error.
#include "allocation_descriptor_peer.hpp"

#include <flight/out_of_profile.hpp>
#include <flight/reference.hpp>
#include <flight/render/render_state.hpp>

#include <cstdio>
#include <memory>
#include <type_traits>

namespace {

struct Plain : flight::ReferenceEnabled {
  double x{0.0};
  double y{0.0};
};
struct Grandchild : peer_package::Screen {
  using flight_native_base = peer_package::Screen;
  double extra{0.0};
};

// The provenance lives in the control block, not the object: the universal base stays empty and a record
// keeps its exact size (option A-prime, chosen by the rule in docs/runtime-semantics.md).
static_assert(std::is_empty_v<flight::ReferenceEnabled>);
static_assert(sizeof(Plain) == 2 * sizeof(double));
// Records stay aggregates and keep designated initialization.
static_assert(std::is_aggregate_v<Plain>);
static_assert(std::is_aggregate_v<Grandchild>);
constexpr Plain designated{.x = 1.0, .y = 2.0};
static_assert(designated.y == 2.0);

// The descriptor is a constant expression, so it exists before any dynamic initializer runs: generated
// headers call make_ref from namespace-scope initializers, in no guaranteed order relative to it.
static_assert(flight::detail::allocation_descriptor<Plain>.type == &typeid(Plain));
static_assert(flight::detail::allocation_descriptor<Grandchild>.cast != nullptr);
// A namespace-scope record, as a generated `const` built by a factory is.
const auto early = flight::make_ref<Plain>(Plain{.x = 7.0});

int failures = 0;
void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

template <typename T>
bool same_allocation(const std::shared_ptr<T>& a, const std::shared_ptr<void>& b) {
  return !a.owner_before(b) && !b.owner_before(a);
}

} // namespace

int main() {
  const auto early_identity = flight::detail::recoverable_owner_identity(early);
  check(early_identity && early_identity->type == std::type_index(typeid(Plain)) && early->x == 7.0,
        "a record made during static initialization is recorded as itself");

  // A plain record with no heritage edge is recorded as exactly itself.
  auto plain = flight::make_ref<Plain>(Plain{.x = 3.0});
  const auto plain_identity = flight::detail::recoverable_owner_identity(plain);
  check(plain_identity && plain_identity->type == std::type_index(typeid(Plain)), "a plain make_ref record is recorded as itself");
  check(plain_identity && plain_identity->object.get() == plain.get(), "its identity is the same object");

  // The plain RenderState a real factory builds is recorded as RenderState (createRenderState builds it
  // through allocateEntity and the materializing cast, which allocates the finished entity by make_ref).
  auto state = flight::render::create_render_state();
  const auto state_identity = flight::detail::recoverable_owner_identity(state);
  check(state_identity && state_identity->type == std::type_index(typeid(flight::types::RenderState)),
        "createRenderState's finished entity is recorded as RenderState");

  // Cross-package derived, recovered through the cast chain from a base reference: the SAME allocation,
  // same control block, with no change to the reference count once the recovery is dropped.
  auto grandchild = flight::make_ref<Grandchild>(Grandchild{});
  grandchild->presentation = 4.0;
  const flight::Ref<base_package::Target> as_base = grandchild;
  const auto uses = grandchild.use_count();
  {
    const auto screen = flight::recover_owner<peer_package::Screen>(as_base);
    check(screen && screen.get() == static_cast<peer_package::Screen*>(grandchild.get()), "recovery across packages is the same object");
    check(screen && same_allocation(screen, grandchild), "recovery shares the allocation's control block");
    check(screen && screen->presentation == 4.0, "recovery reads the live cell");
    screen->presentation = 5.0;
    const auto exact = flight::recover_owner<Grandchild>(as_base);
    check(exact && exact == grandchild, "recovery to the exact type is the original reference");
    const auto identity = flight::detail::recoverable_owner_identity(as_base);
    check(identity && identity->type == std::type_index(typeid(Grandchild)) && identity->object.get() == grandchild.get(),
          "a base reference reports the exact allocated type and complete object");
  }
  check(grandchild->presentation == 5.0, "a write through the recovered owner lands on the original");
  check(grandchild.use_count() == uses, "recovery leaves the reference count unchanged");

  // A base-only allocation does not recover as a derived type; the escape form says so with the shared
  // out-of-profile error, never an empty reference or a copy.
  const flight::Ref<base_package::Target> base_only = flight::make_ref<base_package::Target>(base_package::Target{});
  check(!flight::recover_owner<peer_package::Screen>(base_only), "a base-only allocation is not a derived owner");
  bool escaped = false;
  try {
    (void)flight::require_owner<peer_package::Screen>(base_only);
  } catch (const flight::NativeScalarEscapeError&) {
    escaped = true;
  }
  check(escaped, "the escape form reports a mismatch through NativeScalarEscapeError");
  check(flight::require_owner<peer_package::Screen>(as_base) == flight::recover_owner<peer_package::Screen>(as_base),
        "the escape form returns the same owner when it exists");

  // Provenance belongs to the allocation, not the value: an object allocated any other way has none, a
  // copy carries none, and assignment never overwrites the target's own.
  const auto unrecorded = std::make_shared<Plain>(Plain{});
  check(!flight::detail::recoverable_owner_identity(unrecorded), "an object not made by make_ref has no provenance");
  const auto copied = std::make_shared<Grandchild>(*grandchild);
  check(!flight::detail::recoverable_owner_identity(copied), "a copy of a recorded object carries no provenance");
  check(!flight::recover_owner<peer_package::Screen>(std::shared_ptr<base_package::Target>(copied)), "a copy does not recover");
  auto assigned = flight::make_ref<Plain>(Plain{});
  *assigned = *plain;
  const auto assigned_identity = flight::detail::recoverable_owner_identity(assigned);
  check(assigned_identity && assigned_identity->object.get() == assigned.get(), "assignment keeps the target's own provenance");
  bool not_recorded_escape = false;
  try {
    (void)flight::require_owner<Plain>(unrecorded);
  } catch (const flight::NativeScalarEscapeError&) {
    not_recorded_escape = true;
  }
  check(not_recorded_escape, "the escape form refuses an object with no provenance");

  // An aliasing reference (to a member, sharing the control block) and a converted reference keep the
  // allocation's provenance: both name the same allocation and report its exact type.
  {
    const std::shared_ptr<double> member(grandchild, &grandchild->extra);
    const auto via_member = flight::detail::recoverable_owner_identity(member);
    check(via_member && via_member->type == std::type_index(typeid(Grandchild)) && via_member->object.get() == grandchild.get(),
          "an aliasing reference keeps the allocation's provenance");
    const std::shared_ptr<void> erased = as_base;
    const auto via_erased = flight::detail::recoverable_owner_identity(erased);
    check(via_erased && via_erased->type == std::type_index(typeid(Grandchild)), "an erased reference keeps the provenance");
  }

  // Lifetime: the record holds no reference, so the object dies with its last one.
  std::weak_ptr<Grandchild> watch = grandchild;
  grandchild.reset();
  check(!watch.expired(), "a base reference still keeps the object alive");
  const_cast<flight::Ref<base_package::Target>&>(as_base).reset();
  check(watch.expired(), "nothing else keeps a recorded object alive");

  std::printf("%s (%d failures)\n", failures ? "FAIL" : "allocation descriptor ok", failures);
  return failures;
}
