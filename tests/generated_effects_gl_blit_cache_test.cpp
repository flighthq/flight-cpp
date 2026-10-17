// The per-context blit caches of @flighthq/effects-gl's glEffectBlitShader.ts at a627849, which the
// emitter refused and overrides/include/flight/effects_gl/gl_effect_blit_shader.hpp supplies.
//
// The source keeps three module-scope `WeakMap<GlContext, ...>`s, one per program. What it observes:
// a context and every copy of it are one key; distinct contexts never share an entry; the three maps
// are independent; the getter answers a hit with the identical cached object, without compiling; and a
// cache never keeps its context alive, nor its program once the context is gone.
//
// A live GL context cannot be made here, so the miss branch (compile, uniform lookup) is not run. The
// keys are real contexts nonetheless: `WebGl2Context::lock_weak` over an owner whose lifetime the test
// controls, which is exactly what the weak-key policy holds and compares. No context member that
// reaches the GL state is called.
#include <flight/effects_gl/gl_effect_blit_shader.hpp>

#include <iostream>
#include <memory>
#include <type_traits>

namespace {

namespace effects_gl = flight::effects_gl;
namespace types = flight::types;
using flight::host_sdl::WebGl2Context;

int failures = 0;

void check(bool condition, const char* message) {
  if (condition) return;
  std::cerr << "FAIL: " << message << '\n';
  ++failures;
}

struct ContextOwner {
  std::shared_ptr<int> lifetime = std::make_shared<int>(0);
  std::shared_ptr<flight::host_sdl::detail::GlSurfaceState> surface{
      lifetime, reinterpret_cast<flight::host_sdl::detail::GlSurfaceState*>(lifetime.get())};

  [[nodiscard]] WebGl2Context context() const {
    return WebGl2Context::lock_weak(std::weak_ptr<flight::host_sdl::detail::GlSurfaceState>(surface)).value();
  }
};

flight::Ref<types::GlRenderState> state_over(const WebGl2Context& gl) {
  auto state = flight::make_ref<types::GlRenderState>();
  state->gl = gl;
  return state;
}

flight::Ref<effects_gl::BlitOffsetShaderLocations> offset_program() {
  return flight::make_ref<effects_gl::BlitOffsetShaderLocations>();
}

void check_carrier() {
  static_assert(std::is_same_v<decltype(effects_gl::get_gl_blit_offset_shader(std::declval<flight::Ref<types::GlRenderState>>())),
                               flight::Ref<effects_gl::BlitOffsetShaderLocations>>);
  static_assert(std::is_base_of_v<types::GlFullscreenProgram, effects_gl::BlitOffsetShaderLocations>,
                "GlFullscreenProgram & { locOffset } must still be a GlFullscreenProgram");
}

void check_hits_and_identity() {
  ContextOwner first_owner;
  ContextOwner second_owner;
  const WebGl2Context first = first_owner.context();
  const WebGl2Context first_copy = first_owner.context();
  const WebGl2Context second = second_owner.context();

  const auto offset = offset_program();
  const auto blit = flight::make_ref<types::GlFullscreenProgram>();
  const auto erase = flight::make_ref<types::GlFullscreenProgram>();
  effects_gl::blit_offset_shaders.set(first, offset);
  effects_gl::blit_shaders.set(first, blit);
  effects_gl::erase_shaders.set(first, erase);

  check(effects_gl::get_gl_blit_offset_shader(state_over(first_copy)) == offset,
        "a copy of the context did not hit the offset program cached for it");
  check(effects_gl::get_gl_blit_shader(state_over(first)) == blit, "the blit cache did not return its own program");
  check(effects_gl::get_gl_erase_shader(state_over(first_copy)) == erase,
        "the erase cache did not return its own program");
  check(effects_gl::get_gl_blit_offset_shader(state_over(first)) == effects_gl::get_gl_blit_offset_shader(state_over(first)),
        "two hits for one context returned different objects");
  check(!effects_gl::blit_offset_shaders.has(second) && !effects_gl::blit_shaders.has(second) &&
            !effects_gl::erase_shaders.has(second),
        "a distinct context saw another context's programs");

  const auto other_blit = flight::make_ref<types::GlFullscreenProgram>();
  effects_gl::blit_shaders.set(second, other_blit);
  check(effects_gl::get_gl_blit_shader(state_over(second)) == other_blit &&
            effects_gl::get_gl_blit_shader(state_over(first)) == blit && !effects_gl::erase_shaders.has(second),
        "the three caches or two contexts were not independent");
}

void check_weakness() {
  std::weak_ptr<int> lifetime;
  std::weak_ptr<effects_gl::BlitOffsetShaderLocations> cached;
  ContextOwner survivor;
  {
    ContextOwner owner;
    lifetime = owner.lifetime;
    auto program = offset_program();
    cached = program;
    effects_gl::blit_offset_shaders.set(owner.context(), program);
  }
  check(lifetime.expired(), "the offset cache kept its context alive");
  check(!effects_gl::blit_offset_shaders.has(survivor.context()) && cached.expired(),
        "a dead context's program outlived the next lookup");
}

}  // namespace

int main() {
  check_carrier();
  check_hits_and_identity();
  check_weakness();
  if (failures != 0) {
    std::cerr << failures << " effects-gl blit cache check(s) failed\n";
    return 1;
  }
  std::cout << "effects-gl blit cache checks passed\n";
  return 0;
}
