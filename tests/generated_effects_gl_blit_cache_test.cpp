// The per-context blit caches of @flighthq/effects-gl's glEffectBlitShader.ts at a627849, which the
// emitter refused and overrides/include/flight/effects_gl/gl_effect_blit_shader.hpp supplies.
//
// The source keeps three module-scope `WeakMap<GlContext, ...>`s, one per program. What it observes:
// a context and every copy of it are one key; distinct contexts never share an entry; the three maps
// are independent; the getter answers a hit with the identical cached object, without compiling; and a
// cache never keeps its context alive, nor its program once the context is gone.
//
// The ownership checks control the weak-key lifetime without calling GL. One additional, actual
// offscreen GlApplication context exercises the offset cache miss (shader compile, program link and
// attribute/uniform lookup), followed by a hit through a copy of that context. No apply-pass is used.
#include <flight/effects_gl/gl_effect_blit_shader.hpp>
#include <flight/host_sdl/gl_app.hpp>

#include <exception>
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

void check_live_miss_and_hit() {
  flight::host_sdl::GlApplication app({
      .title = "Flight effects-gl cache miss", .width = 32, .height = 32, .hidden = true});
  const auto gl = app.context();
  check(!effects_gl::blit_offset_shaders.has(gl), "live offset cache was not initially empty");

  const auto offset = effects_gl::get_gl_blit_offset_shader(state_over(gl));
  check(effects_gl::blit_offset_shaders.get(gl) == std::optional{offset},
        "the live miss did not cache its compiled owner");
  check(static_cast<bool>(offset->program) && gl.get_program_parameter(offset->program, gl.link_status) == 1,
        "the live miss did not produce a linked program");
  check(offset->loc_position >= 0 && offset->loc_tex_coord >= 0 &&
            offset->loc_position == gl.get_attrib_location(offset->program, flight::String("a_position")) &&
            offset->loc_tex_coord == gl.get_attrib_location(offset->program, flight::String("a_texCoord")),
        "the offset spread lost the compiled attribute locations");
  const auto sampler = gl.get_uniform_location(offset->program, flight::String("u_texture"));
  const auto displacement = gl.get_uniform_location(offset->program, flight::String("u_offset"));
  check(sampler && displacement && offset->texture && offset->loc_offset &&
            offset->texture.native_name() == sampler->native_name() &&
            offset->loc_offset.native_name() == displacement->native_name() &&
            offset->textures.size() == 1 && offset->textures[0] == offset->texture,
        "the live miss lost its sampler/offset locations or copied texture array");
  gl.use_program(offset->program);
  gl.uniform1i(offset->texture, 0);
  gl.uniform2f(offset->loc_offset, 0.25F, -0.5F);
  check(gl.get_error() == gl.no_error, "the live program rejected its actual uniform locations");

  const WebGl2Context copy = gl;
  const auto hit = effects_gl::get_gl_blit_offset_shader(state_over(copy));
  check(hit == offset && hit->program == offset->program && hit->loc_offset == offset->loc_offset &&
            hit->textures.identity() == offset->textures.identity(),
        "a live context copy did not hit the identical program and locations");
  check(!effects_gl::blit_shaders.has(gl) && !effects_gl::erase_shaders.has(gl),
        "the live offset miss populated a different cache");

  gl.use_program(std::nullopt);
  check(effects_gl::blit_offset_shaders.erase(gl), "could not release the live cache entry");
  gl.delete_program(offset->program);
  check(gl.get_error() == gl.no_error, "live program teardown failed");
}

}  // namespace

int main() {
  try {
    check_carrier();
    check_hits_and_identity();
    check_weakness();
    check_live_miss_and_hit();
    if (failures != 0) {
      std::cerr << failures << " effects-gl blit cache check(s) failed\n";
      return 1;
    }
    std::cout << "effects-gl blit cache checks passed; live offset miss/link/locations/hit verified\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "effects-gl live cache check failed: " << error.what() << '\n';
    return 1;
  }
}
