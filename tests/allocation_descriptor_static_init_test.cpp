// Regression: the make_ref allocation record must be usable DURING static initialization.
//
// Generated headers build namespace-scope constants through entity factories -- `const auto ZERO =
// create_vector2(0, 0)` -- and allocateEntity reaches the row owner, which reads the allocation's descriptor
// while initializers are still running. A dynamically initialized descriptor could still be zero then, and
// 43 generated tests aborted at startup on `member call on null pointer of type 'std::type_info'`. This is
// that path in one translation unit: a namespace-scope entity built through the original generated
// allocate_entity/finish_entity, whose construction asks for the row owner.
#include <flight/geometry/vector2.hpp>
#include <flight/reference.hpp>
#include <flight/structural_ref.hpp>

#include <cstdio>
#include <typeindex>

namespace {

const auto early_vector = flight::geometry::create_vector2(3.0, 4.0);

} // namespace

int main() {
  int failures = 0;
  const auto identity = flight::detail::recoverable_owner_identity(early_vector);
  if (!identity || identity->object.get() != early_vector.get() || early_vector->x != 3.0 || early_vector->y != 4.0) {
    std::printf("FAIL a namespace-scope entity built during static initialization is recorded as its own allocation\n");
    ++failures;
  }
  std::printf("%s (%d failures)\n", failures ? "FAIL" : "allocation descriptor static init ok", failures);
  return failures;
}
