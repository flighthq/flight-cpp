// Native behavior for `key in state` on a NOMINAL reference typed as a base, the form the render package's
// getRenderRegistriesMissMessage writes (`'device' in state` with `state: RenderState`). The emitter lowers
// it to flight::has_own_named_property<RowKey<key>>, which asks the allocation's exact type -- recorded by
// make_ref -- rather than the reference's static type. These fixtures pin the source contract on real SDK
// types: a derived object reached through its base reports its own members, a sibling or base object does
// not, an unset optional member whose presence the runtime cannot tell is refused, an allocation make_ref did
// not record is refused rather than answered, and `in` on null throws.
#include <flight/types/blur_effect.hpp>
#include <flight/types/gl_render_state.hpp>
#include <flight/types/render_state.hpp>
#include <flight/types/vignette_effect.hpp>
#include <flight/types/wgpu_render_state.hpp>

#include <iostream>
#include <string>

namespace {

namespace types = flight::types;

int failures = 0;

void check(bool condition, const std::string& message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

template <typename Query>
bool refused(Query&& query) {
  try {
    static_cast<void>(query());
  } catch (const flight::OutOfProfileError&) {
    return true;
  }
  return false;
}

template <typename Query>
bool throws_type_error(Query&& query) {
  try {
    static_cast<void>(query());
  } catch (const flight::TypeError&) {
    return true;
  }
  return false;
}

} // namespace

int main() {
  // A derived effect reached through Ref<Effect>: its own members are visible; a sibling's are not.
  {
    auto vignette = flight::make_ref<types::VignetteEffect>();
    vignette->intensity = 0.5;
    const flight::Ref<types::Effect> as_effect = vignette;
    check(flight::has_own_named_property<flight::RowKey<"intensity">>(as_effect),
          "'intensity' in effect is true for a Vignette that wrote it, through its base reference");
    // `radius` is an optional carrier bound without a presence policy and left unset: it may be a written
    // null or an omitted member, so the query refuses rather than answering either.
    check(refused([&] { return flight::has_own_named_property<flight::RowKey<"radius">>(as_effect); }),
          "an unset optional member with no presence policy is refused, not answered absent");
    check(flight::has_own_named_property<flight::RowKey<"kind">>(as_effect),
          "a base member is present on the derived object");
    const flight::Ref<types::Effect> blur = flight::make_ref<types::BlurEffect>();
    check(!flight::has_own_named_property<flight::RowKey<"intensity">>(blur),
          "a sibling effect type does not declare 'intensity'");
    check(flight::has_own_named_property<flight::RowKey<"toString">>(blur), "Object.prototype names stay present");
  }

  // The render states the guard distinguishes: only the WGPU state declares `device`.
  {
    check(!flight::has_own_named_property<flight::RowKey<"device">>(flight::make_ref<types::RenderState>()),
          "a base RenderState has no device");
    check(!flight::has_own_named_property<flight::RowKey<"device">>(flight::make_ref<types::GlRenderState>()),
          "a GL render state has no device");
    check(flight::has_own_named_property<flight::RowKey<"device">>(flight::make_ref<types::WgpuRenderState>()),
          "a WGPU render state has a device");
  }

  // No provenance: refused rather than answered from the static type.
  {
    const flight::Ref<types::Effect> foreign = std::make_shared<types::VignetteEffect>();
    check(refused([&] { return flight::has_own_named_property<flight::RowKey<"intensity">>(foreign); }),
          "an allocation make_ref did not record is refused, never answered false");
    check(throws_type_error([] { return flight::has_own_named_property<flight::RowKey<"device">>(flight::Ref<types::RenderState>()); }),
          "`in` on a null reference throws TypeError");
  }

  if (failures != 0) return 1;
  std::cout << "render registry membership behaves as specified\n";
  return 0;
}
