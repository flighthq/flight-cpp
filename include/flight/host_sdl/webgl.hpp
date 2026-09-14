#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <variant>

#include <SDL3/SDL_video.h>

#include <flight/array.hpp>
#include <flight/array_buffer_view.hpp>
#include <flight/host_sdl/export.hpp>
#include <flight/host_sdl/input.hpp>
#include <flight/host_sdl/web_platform_types.hpp>
#include <flight/host_sdl/window.hpp>
#include <flight/presence.hpp>
#include <flight/sequence_view.hpp>
#include <flight/string.hpp>
#include <flight/typed_array.hpp>

namespace flight::host_sdl {

class WebGl2Context;

namespace detail {

enum class WebGlObjectKind {
  buffer,
  framebuffer,
  program,
  renderbuffer,
  shader,
  texture,
  uniform_location,
  vertex_array,
};

struct GlSurfaceState;

struct FLIGHT_HOST_SDL_GL_API WebGlObjectState final {
  WebGlObjectState(
      std::weak_ptr<GlSurfaceState> owner,
      WebGlObjectKind kind,
      std::uint32_t name) noexcept;
  ~WebGlObjectState() noexcept;

  WebGlObjectState(const WebGlObjectState&) = delete;
  WebGlObjectState& operator=(const WebGlObjectState&) = delete;

  std::weak_ptr<GlSurfaceState> owner;
  WebGlObjectKind kind;
  std::uint32_t name;
  bool valid;
};

struct GlImageSourceState;

} // namespace detail

template <typename Tag>
class WebGlHandle final {
 public:
  using weak_type = std::weak_ptr<detail::WebGlObjectState>;

  WebGlHandle() noexcept = default;

  [[nodiscard]] explicit operator bool() const noexcept {
    return state_ != nullptr && state_->valid;
  }
  [[nodiscard]] friend bool operator==(
      const WebGlHandle&,
      const WebGlHandle&) noexcept = default;
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }
  [[nodiscard]] std::uint32_t native_name() const noexcept { return state_ ? state_->name : 0; }
  [[nodiscard]] weak_type weaken() const noexcept { return state_; }

  [[nodiscard]] static std::optional<WebGlHandle> lock_weak(const weak_type& weak) noexcept {
    auto state = weak.lock();
    if (!state) return std::nullopt;
    return WebGlHandle(std::move(state));
  }

 private:
  friend class WebGl2Context;

  explicit WebGlHandle(std::shared_ptr<detail::WebGlObjectState> state) noexcept
      : state_(std::move(state)) {}

  std::shared_ptr<detail::WebGlObjectState> state_;
};

struct WebGlBufferTag;
struct WebGlFramebufferTag;
struct WebGlProgramTag;
struct WebGlRenderbufferTag;
struct WebGlShaderTag;
struct WebGlTextureTag;
struct WebGlUniformLocationTag;
struct WebGlVertexArrayObjectTag;

using WebGlBuffer = WebGlHandle<WebGlBufferTag>;
using WebGlFramebuffer = WebGlHandle<WebGlFramebufferTag>;
using WebGlProgram = WebGlHandle<WebGlProgramTag>;
using WebGlRenderbuffer = WebGlHandle<WebGlRenderbufferTag>;
using WebGlShader = WebGlHandle<WebGlShaderTag>;
using WebGlTexture = WebGlHandle<WebGlTextureTag>;
using WebGlUniformLocation = WebGlHandle<WebGlUniformLocationTag>;
using WebGlVertexArrayObject = WebGlHandle<WebGlVertexArrayObjectTag>;

class FLIGHT_HOST_SDL_GL_API GlParameterValue final {
 public:
  [[nodiscard]] explicit operator double() const;
  [[nodiscard]] explicit operator bool() const;
  [[nodiscard]] explicit operator Array<double>() const;
  [[nodiscard]] explicit operator Array<bool>() const;
  [[nodiscard]] explicit operator std::variant<SequenceView<double>, Null, Undefined>() const;
  [[nodiscard]] explicit operator std::variant<SequenceView<bool>, Null, Undefined>() const;
  [[nodiscard]] explicit operator std::optional<WebGlFramebuffer>() const;
  [[nodiscard]] explicit operator std::optional<WebGlProgram>() const;
  [[nodiscard]] explicit operator std::optional<WebGlTexture>() const;
  [[nodiscard]] explicit operator std::optional<WebGlVertexArrayObject>() const;

  [[nodiscard]] friend bool operator==(const GlParameterValue& value, bool expected) {
    return static_cast<bool>(value) == expected;
  }
  [[nodiscard]] friend bool operator==(bool expected, const GlParameterValue& value) {
    return value == expected;
  }

 private:
  friend class WebGl2Context;

  using Value = std::variant<
      double,
      bool,
      Array<double>,
      Array<bool>,
      std::optional<WebGlFramebuffer>,
      std::optional<WebGlProgram>,
      std::optional<WebGlTexture>,
      std::optional<WebGlVertexArrayObject>>;

  template <typename ValueType>
  explicit GlParameterValue(ValueType value) : value_(std::move(value)) {}

  Value value_;
};

struct WebGlContextAttributes final {
  bool alpha{true};
  bool antialias{true};
  bool depth{true};
  bool desynchronized{false};
  bool fail_if_major_performance_caveat{false};
  String power_preference{"default"};
  bool premultiplied_alpha{true};
  bool preserve_drawing_buffer{false};
  bool stencil{false};
};

// Native carrier for the only WebGL extension object retained by Flight's shared GL runtime.
// The enum values are fixed by EXT_texture_filter_anisotropic and are the same for its desktop
// ARB alias. Availability remains a property of a live context and is queried below.
struct GlAnisotropyExtension final {
  static constexpr double texture_max_anisotropy_ext = 0x84FE;
  static constexpr double max_texture_max_anisotropy_ext = 0x84FF;
};

// Presence-bearing result for WebGL getExtension calls. Flight currently uses extension objects
// either as availability tokens or for the anisotropy enums; compressed-format member lookup remains
// a compiler-owned Record<String, number> conversion.
class FLIGHT_HOST_SDL_GL_API GlExtension final {
 public:
  static constexpr double texture_max_anisotropy_ext =
      GlAnisotropyExtension::texture_max_anisotropy_ext;
  static constexpr double max_texture_max_anisotropy_ext =
      GlAnisotropyExtension::max_texture_max_anisotropy_ext;

  [[nodiscard]] bool has_value() const noexcept { return available_; }
  [[nodiscard]] explicit operator bool() const noexcept { return available_; }

 private:
  friend class WebGl2Context;
  explicit GlExtension(bool available) noexcept : available_(available) {}

  bool available_;
};

struct WebGlActiveInfo final {
  String name;
  double size{0.0};
  double type{0.0};
};

class FLIGHT_HOST_SDL_GL_API GlImageSource final {
 public:
  using weak_type = std::weak_ptr<detail::GlImageSourceState>;

  GlImageSource() noexcept = default;

  [[nodiscard]] static GlImageSource rgba8(
      std::size_t width,
      std::size_t height,
      Uint8ClampedArray pixels);

  [[nodiscard]] explicit operator bool() const noexcept { return state_ != nullptr; }
  [[nodiscard]] friend bool operator==(
      const GlImageSource&,
      const GlImageSource&) noexcept = default;
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }
  [[nodiscard]] std::size_t width() const noexcept;
  [[nodiscard]] std::size_t height() const noexcept;
  [[nodiscard]] std::span<const Uint8Clamped> rgba8_pixels() const noexcept;
  [[nodiscard]] weak_type weaken() const noexcept { return state_; }

  [[nodiscard]] static std::optional<GlImageSource> lock_weak(const weak_type& weak) noexcept;

 private:
  explicit GlImageSource(std::shared_ptr<detail::GlImageSourceState> state) noexcept
      : state_(std::move(state)) {}

  std::shared_ptr<detail::GlImageSourceState> state_;
};

struct GlImageSourceWeakPolicy final {
  using key_type = GlImageSource;
  using weak_type = GlImageSource::weak_type;
  using identity_type = const void*;

  [[nodiscard]] static weak_type weaken(const key_type& key) noexcept { return key.weaken(); }
  [[nodiscard]] static std::optional<key_type> lock(const weak_type& key) noexcept {
    return key_type::lock_weak(key);
  }
  [[nodiscard]] static identity_type identity(const key_type& key) noexcept { return key.identity(); }
  [[nodiscard]] static std::size_t hash(identity_type identity) noexcept;
  [[nodiscard]] static bool equal(identity_type left, identity_type right) noexcept {
    return left == right;
  }
};

// A copyable WebGL-shaped view over one SDL window and its OpenGL context. The context and window
// share one owner so a canvas, context, or GPU handle can never observe a half-destroyed surface.
// Rendering remains in Flight's generated render-gl packages; this class supplies only the host
// context, object identities, and presentation boundary.
class FLIGHT_HOST_SDL_GL_API WebGl2Context final {
 public:
  static constexpr std::uint32_t no_error = 0;
  static constexpr std::uint32_t active_texture_constant = 34016;
  static constexpr std::uint32_t active_uniforms = 35718;
  static constexpr std::uint32_t always = 519;
  static constexpr std::uint32_t array_buffer = 0x8892;
  static constexpr std::uint32_t back = 1029;
  static constexpr std::uint32_t blend = 3042;
  static constexpr std::uint32_t blend_dst_alpha = 32970;
  static constexpr std::uint32_t blend_dst_rgb = 32968;
  static constexpr std::uint32_t blend_equation_alpha = 34877;
  static constexpr std::uint32_t blend_equation_rgb = 32777;
  static constexpr std::uint32_t blend_src_alpha = 32971;
  static constexpr std::uint32_t blend_src_rgb = 32969;
  static constexpr std::uint32_t ccw = 2305;
  static constexpr std::uint32_t clamp_to_edge = 33071;
  static constexpr std::uint32_t color = 6144;
  static constexpr std::uint32_t color_attachment0 = 36064;
  static constexpr std::uint32_t color_buffer_bit = 16384;
  static constexpr std::uint32_t color_clear_value = 3106;
  static constexpr std::uint32_t color_writemask = 3107;
  static constexpr std::uint32_t compile_status = 0x8B81;
  static constexpr std::uint32_t cull_face_constant = 2884;
  static constexpr std::uint32_t cull_face_mode = 2885;
  static constexpr std::uint32_t current_program = 35725;
  static constexpr std::uint32_t cw = 2304;
  static constexpr std::uint32_t decr_wrap = 34056;
  static constexpr std::uint32_t depth24_stencil8 = 35056;
  static constexpr std::uint32_t depth_buffer_bit = 256;
  static constexpr std::uint32_t depth_func_constant = 2932;
  static constexpr std::uint32_t depth_stencil = 34041;
  static constexpr std::uint32_t depth_stencil_attachment = 33306;
  static constexpr std::uint32_t depth_test = 2929;
  static constexpr std::uint32_t depth_writemask = 2930;
  static constexpr std::uint32_t draw_framebuffer = 36009;
  static constexpr std::uint32_t dst_color = 774;
  static constexpr std::uint32_t dynamic_draw = 35048;
  static constexpr std::uint32_t element_array_buffer = 34963;
  static constexpr std::uint32_t equal = 514;
  static constexpr std::uint32_t float_ = 5126;
  static constexpr std::uint32_t float_mat2 = 35674;
  static constexpr std::uint32_t float_mat3 = 35675;
  static constexpr std::uint32_t float_mat4 = 35676;
  static constexpr std::uint32_t float_vec2 = 35664;
  static constexpr std::uint32_t float_vec3 = 35665;
  static constexpr std::uint32_t float_vec4 = 35666;
  static constexpr std::uint32_t fragment_shader = 0x8B30;
  static constexpr std::uint32_t framebuffer = 36160;
  static constexpr std::uint32_t framebuffer_binding = 36006;
  static constexpr std::uint32_t framebuffer_complete = 36053;
  static constexpr std::uint32_t front = 1028;
  static constexpr std::uint32_t front_face_constant = 2886;
  static constexpr std::uint32_t func_add = 32774;
  static constexpr std::uint32_t func_reverse_subtract = 32779;
  static constexpr std::uint32_t half_float = 5131;
  static constexpr std::uint32_t incr_wrap = 34055;
  static constexpr std::uint32_t invert = 5386;
  static constexpr std::uint32_t keep = 7680;
  static constexpr std::uint32_t less = 513;
  static constexpr std::uint32_t linear = 9729;
  static constexpr std::uint32_t linear_mipmap_linear = 9987;
  static constexpr std::uint32_t linear_mipmap_nearest = 9985;
  static constexpr std::uint32_t lines = 1;
  static constexpr std::uint32_t line_strip = 3;
  static constexpr std::uint32_t link_status = 0x8B82;
  static constexpr std::uint32_t max = 32776;
  static constexpr std::uint32_t max_samples = 36183;
  static constexpr std::uint32_t max_texture_image_units = 34930;
  static constexpr std::uint32_t min = 32775;
  static constexpr std::uint32_t mirrored_repeat = 33648;
  static constexpr std::uint32_t nearest = 9728;
  static constexpr std::uint32_t nearest_mipmap_linear = 9986;
  static constexpr std::uint32_t nearest_mipmap_nearest = 9984;
  static constexpr std::uint32_t none = 0;
  static constexpr std::uint32_t notequal = 517;
  static constexpr std::uint32_t one = 1;
  static constexpr std::uint32_t one_minus_src_alpha = 771;
  static constexpr std::uint32_t one_minus_src_color = 769;
  static constexpr std::uint32_t points = 0;
  static constexpr std::uint32_t read_framebuffer = 36008;
  static constexpr std::uint32_t renderbuffer = 36161;
  static constexpr std::uint32_t repeat = 10497;
  static constexpr std::uint32_t rgba = 6408;
  static constexpr std::uint32_t rgba16_f = 34842;
  static constexpr std::uint32_t rgba32_f = 34836;
  static constexpr std::uint32_t rgba8 = 32856;
  static constexpr std::uint32_t scissor_box = 3088;
  static constexpr std::uint32_t scissor_test = 0x0C11;
  static constexpr std::uint32_t srgb8_alpha8 = 35907;
  static constexpr std::uint32_t static_draw = 0x88E4;
  static constexpr std::uint32_t stencil_back_fail = 34817;
  static constexpr std::uint32_t stencil_back_func = 34816;
  static constexpr std::uint32_t stencil_back_pass_depth_fail = 34818;
  static constexpr std::uint32_t stencil_back_pass_depth_pass = 34819;
  static constexpr std::uint32_t stencil_back_ref = 36003;
  static constexpr std::uint32_t stencil_back_value_mask = 36004;
  static constexpr std::uint32_t stencil_back_writemask = 36005;
  static constexpr std::uint32_t stencil_buffer_bit = 1024;
  static constexpr std::uint32_t stencil_fail = 2964;
  static constexpr std::uint32_t stencil_func_constant = 2962;
  static constexpr std::uint32_t stencil_pass_depth_fail = 2965;
  static constexpr std::uint32_t stencil_pass_depth_pass = 2966;
  static constexpr std::uint32_t stencil_ref = 2967;
  static constexpr std::uint32_t stencil_test = 2960;
  static constexpr std::uint32_t stencil_value_mask = 2963;
  static constexpr std::uint32_t stencil_writemask = 2968;
  static constexpr std::uint32_t stream_draw = 35040;
  static constexpr std::uint32_t texture0 = 33984;
  static constexpr std::uint32_t texture1 = 33985;
  static constexpr std::uint32_t texture2 = 33986;
  static constexpr std::uint32_t texture3 = 33987;
  static constexpr std::uint32_t texture_2_d = 3553;
  static constexpr std::uint32_t texture_2_d_array = 35866;
  static constexpr std::uint32_t texture_3_d = 32879;
  static constexpr std::uint32_t texture_binding_2_d = 32873;
  static constexpr std::uint32_t texture_cube_map = 34067;
  static constexpr std::uint32_t texture_cube_map_positive_x = 34069;
  static constexpr std::uint32_t texture_mag_filter = 10240;
  static constexpr std::uint32_t texture_max_level = 33085;
  static constexpr std::uint32_t texture_min_filter = 10241;
  static constexpr std::uint32_t texture_wrap_r = 32882;
  static constexpr std::uint32_t texture_wrap_s = 10242;
  static constexpr std::uint32_t texture_wrap_t = 10243;
  static constexpr std::uint32_t triangles = 4;
  static constexpr std::uint32_t triangle_fan = 6;
  static constexpr std::uint32_t triangle_strip = 5;
  static constexpr std::uint32_t unpack_premultiply_alpha_webgl = 37441;
  static constexpr std::uint32_t unsigned_byte = 5121;
  static constexpr std::uint32_t unsigned_int = 5125;
  static constexpr std::uint32_t unsigned_int_24_8 = 34042;
  static constexpr std::uint32_t unsigned_short = 5123;
  static constexpr std::uint32_t vertex_array_binding = 34229;
  static constexpr std::uint32_t vertex_shader = 0x8B31;
  static constexpr std::uint32_t viewport_constant = 2978;
  static constexpr std::uint32_t zero = 0;

  WebGl2Context() noexcept = default;

  [[nodiscard]] explicit operator bool() const noexcept { return state_ != nullptr; }
  [[nodiscard]] friend bool operator==(
      const WebGl2Context&,
      const WebGl2Context&) noexcept = default;
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }
  [[nodiscard]] int drawing_buffer_width() const;
  [[nodiscard]] int drawing_buffer_height() const;
  [[nodiscard]] SDL_FunctionPointer function_address(std::string_view name) const;
  [[nodiscard]] bool supports_extension(std::string_view name) const;
  [[nodiscard]] std::optional<GlAnisotropyExtension> anisotropy_extension() const;

  [[nodiscard]] WebGlBuffer create_buffer() const;
  [[nodiscard]] WebGlFramebuffer create_framebuffer() const;
  [[nodiscard]] WebGlProgram create_program() const;
  [[nodiscard]] WebGlRenderbuffer create_renderbuffer() const;
  [[nodiscard]] std::optional<WebGlShader> create_shader(std::uint32_t type) const;
  [[nodiscard]] WebGlTexture create_texture() const;
  [[nodiscard]] WebGlVertexArrayObject create_vertex_array() const;

  void active_texture(std::uint32_t texture) const;
  void attach_shader(
      const WebGlProgram& program,
      const std::optional<WebGlShader>& shader) const;
  void bind_buffer(std::uint32_t target, const std::optional<WebGlBuffer>& buffer) const;
  void bind_buffer(std::uint32_t target, std::nullptr_t) const;
  void bind_framebuffer(
      std::uint32_t target,
      const std::optional<WebGlFramebuffer>& framebuffer) const;
  void bind_framebuffer(std::uint32_t target, std::nullptr_t) const;
  void bind_renderbuffer(
      std::uint32_t target,
      const std::optional<WebGlRenderbuffer>& renderbuffer) const;
  void bind_renderbuffer(std::uint32_t target, std::nullptr_t) const;
  void bind_texture(std::uint32_t target, const std::optional<WebGlTexture>& texture) const;
  void bind_texture(std::uint32_t target, std::nullptr_t) const;
  void bind_vertex_array(const std::optional<WebGlVertexArrayObject>& vertex_array) const;
  void bind_vertex_array(std::nullptr_t) const;
  void buffer_data(std::uint32_t target, std::size_t size, std::uint32_t usage) const;
  void buffer_data(
      std::uint32_t target,
      const ArrayBufferView& source,
      std::uint32_t usage) const;
  void buffer_sub_data(
      std::uint32_t target,
      std::ptrdiff_t destination_offset,
      const ArrayBufferView& source,
      std::size_t source_offset = 0,
      std::optional<std::size_t> length = std::nullopt) const;
  void blend_equation(std::uint32_t mode) const;
  void blend_equation_separate(std::uint32_t rgb, std::uint32_t alpha) const;
  void blend_func(std::uint32_t source, std::uint32_t destination) const;
  void blend_func_separate(
      std::uint32_t source_rgb,
      std::uint32_t destination_rgb,
      std::uint32_t source_alpha,
      std::uint32_t destination_alpha) const;
  void blit_framebuffer(
      int source_x0,
      int source_y0,
      int source_x1,
      int source_y1,
      int destination_x0,
      int destination_y0,
      int destination_x1,
      int destination_y1,
      std::uint32_t mask,
      std::uint32_t filter) const;
  [[nodiscard]] std::uint32_t check_framebuffer_status(std::uint32_t target) const;
  void clear_bufferfi(
      std::uint32_t buffer,
      int draw_buffer,
      float depth,
      int stencil) const;
  void clear_bufferfv(
      std::uint32_t buffer,
      int draw_buffer,
      const Float32Array& values,
      std::size_t source_offset = 0) const;
  void clear_bufferfv(
      std::uint32_t buffer,
      int draw_buffer,
      const Array<double>& values,
      std::size_t source_offset = 0) const;
  void clear_depth(float depth) const;
  void color_mask(bool red, bool green, bool blue, bool alpha) const;
  void compile_shader(const std::optional<WebGlShader>& shader) const;
  void compressed_tex_image2_d(
      std::uint32_t target,
      int level,
      std::uint32_t internal_format,
      int width,
      int height,
      int border,
      const ArrayBufferView& source,
      std::size_t source_offset = 0,
      std::optional<std::size_t> source_length = std::nullopt) const;
  void compressed_tex_sub_image3_d(
      std::uint32_t target,
      int level,
      int x_offset,
      int y_offset,
      int z_offset,
      int width,
      int height,
      int depth,
      std::uint32_t format,
      const ArrayBufferView& source,
      std::size_t source_offset = 0,
      std::optional<std::size_t> source_length = std::nullopt) const;
  void cull_face(std::uint32_t mode) const;
  void delete_buffer(const std::optional<WebGlBuffer>& buffer) const;
  void delete_framebuffer(const std::optional<WebGlFramebuffer>& framebuffer) const;
  void delete_program(const std::optional<WebGlProgram>& program) const;
  void delete_renderbuffer(const std::optional<WebGlRenderbuffer>& renderbuffer) const;
  void delete_shader(const std::optional<WebGlShader>& shader) const;
  void delete_texture(const std::optional<WebGlTexture>& texture) const;
  void delete_vertex_array(const std::optional<WebGlVertexArrayObject>& vertex_array) const;
  void depth_func(std::uint32_t function) const;
  void depth_mask(bool enabled) const;
  void disable_vertex_attrib_array(std::uint32_t index) const;
  void draw_arrays(std::uint32_t mode, int first, int count) const;
  void draw_arrays_instanced(
      std::uint32_t mode,
      int first,
      int count,
      int instance_count) const;
  void draw_buffers(const Array<std::uint32_t>& buffers) const;
  void draw_buffers(const Array<double>& buffers) const;
  void draw_elements(std::uint32_t mode, int count, std::uint32_t type, std::ptrdiff_t offset) const;
  void draw_elements_instanced(
      std::uint32_t mode,
      int count,
      std::uint32_t type,
      std::ptrdiff_t offset,
      int instance_count) const;
  void enable_vertex_attrib_array(std::uint32_t index) const;
  void finish() const;
  void flush() const;
  void framebuffer_renderbuffer(
      std::uint32_t target,
      std::uint32_t attachment,
      std::uint32_t renderbuffer_target,
      const std::optional<WebGlRenderbuffer>& renderbuffer) const;
  void framebuffer_texture2_d(
      std::uint32_t target,
      std::uint32_t attachment,
      std::uint32_t texture_target,
      const std::optional<WebGlTexture>& texture,
      int level) const;
  void front_face(std::uint32_t mode) const;
  void generate_mipmap(std::uint32_t target) const;
  [[nodiscard]] std::optional<WebGlActiveInfo> get_active_uniform(
      const WebGlProgram& program,
      int index) const;
  [[nodiscard]] int get_attrib_location(const WebGlProgram& program, const String& name) const;
  [[nodiscard]] GlExtension get_extension(const String& name) const;
  [[nodiscard]] GlParameterValue get_parameter(std::uint32_t parameter) const;
  [[nodiscard]] std::optional<String> get_program_info_log(const WebGlProgram& program) const;
  [[nodiscard]] double get_program_parameter(
      const WebGlProgram& program,
      std::uint32_t parameter) const;
  [[nodiscard]] std::optional<String> get_shader_info_log(
      const std::optional<WebGlShader>& shader) const;
  [[nodiscard]] double get_shader_parameter(
      const std::optional<WebGlShader>& shader,
      std::uint32_t parameter) const;
  [[nodiscard]] std::optional<WebGlUniformLocation> get_uniform_location(
      const WebGlProgram& program,
      const String& name) const;
  [[nodiscard]] bool is_enabled(std::uint32_t capability) const;
  void link_program(const WebGlProgram& program) const;
  void pixel_storei(std::uint32_t parameter, int value) const;
  void read_buffer(std::uint32_t source) const;
  void read_pixels(
      int x,
      int y,
      int width,
      int height,
      std::uint32_t format,
      std::uint32_t type,
      ArrayBufferView destination) const;
  void renderbuffer_storage(
      std::uint32_t target,
      std::uint32_t internal_format,
      int width,
      int height) const;
  void renderbuffer_storage_multisample(
      std::uint32_t target,
      int samples,
      std::uint32_t internal_format,
      int width,
      int height) const;
  void shader_source(const std::optional<WebGlShader>& shader, const String& source) const;
  void stencil_func(std::uint32_t function, int reference, std::uint32_t mask) const;
  void stencil_func_separate(
      std::uint32_t face,
      std::uint32_t function,
      int reference,
      std::uint32_t mask) const;
  void stencil_mask(std::uint32_t mask) const;
  void stencil_mask_separate(std::uint32_t face, std::uint32_t mask) const;
  void stencil_op(std::uint32_t fail, std::uint32_t depth_fail, std::uint32_t depth_pass) const;
  void stencil_op_separate(
      std::uint32_t face,
      std::uint32_t fail,
      std::uint32_t depth_fail,
      std::uint32_t depth_pass) const;
  void tex_image2_d(
      std::uint32_t target,
      int level,
      int internal_format,
      int width,
      int height,
      int border,
      std::uint32_t format,
      std::uint32_t type,
      const ArrayBufferView& pixels) const;
  void tex_image2_d(
      std::uint32_t target,
      int level,
      int internal_format,
      int width,
      int height,
      int border,
      std::uint32_t format,
      std::uint32_t type,
      std::nullptr_t) const;
  void tex_image2_d(
      std::uint32_t target,
      int level,
      int internal_format,
      std::uint32_t format,
      std::uint32_t type,
      const GlImageSource& source) const;
  void tex_image3_d(
      std::uint32_t target,
      int level,
      int internal_format,
      int width,
      int height,
      int depth,
      int border,
      std::uint32_t format,
      std::uint32_t type,
      const ArrayBufferView& pixels) const;
  void tex_image3_d(
      std::uint32_t target,
      int level,
      int internal_format,
      int width,
      int height,
      int depth,
      int border,
      std::uint32_t format,
      std::uint32_t type,
      std::nullptr_t) const;
  void tex_parameterf(std::uint32_t target, std::uint32_t parameter, float value) const;
  void tex_parameteri(std::uint32_t target, std::uint32_t parameter, int value) const;
  void tex_storage3_d(
      std::uint32_t target,
      int levels,
      std::uint32_t internal_format,
      int width,
      int height,
      int depth) const;
  void tex_sub_image2_d(
      std::uint32_t target,
      int level,
      int x_offset,
      int y_offset,
      int width,
      int height,
      std::uint32_t format,
      std::uint32_t type,
      const ArrayBufferView& pixels) const;
  void uniform1f(const std::optional<WebGlUniformLocation>& location, float x) const;
  void uniform1i(const std::optional<WebGlUniformLocation>& location, int x) const;
  void uniform2f(const std::optional<WebGlUniformLocation>& location, float x, float y) const;
  void uniform3f(
      const std::optional<WebGlUniformLocation>& location,
      float x,
      float y,
      float z) const;
  void uniform4f(
      const std::optional<WebGlUniformLocation>& location,
      float x,
      float y,
      float z,
      float w) const;
  void uniform1fv(const std::optional<WebGlUniformLocation>& location, const Float32Array& values) const;
  void uniform1fv(const std::optional<WebGlUniformLocation>& location, const Array<double>& values) const;
  void uniform2fv(const std::optional<WebGlUniformLocation>& location, const Float32Array& values) const;
  void uniform2fv(const std::optional<WebGlUniformLocation>& location, const Array<double>& values) const;
  void uniform3fv(const std::optional<WebGlUniformLocation>& location, const Float32Array& values) const;
  void uniform3fv(const std::optional<WebGlUniformLocation>& location, const Array<double>& values) const;
  void uniform4fv(const std::optional<WebGlUniformLocation>& location, const Float32Array& values) const;
  void uniform4fv(const std::optional<WebGlUniformLocation>& location, const Array<double>& values) const;
  void uniform_matrix3fv(
      const std::optional<WebGlUniformLocation>& location,
      bool transpose,
      const Float32Array& values) const;
  void uniform_matrix3fv(
      const std::optional<WebGlUniformLocation>& location,
      bool transpose,
      const Array<double>& values) const;
  void uniform_matrix4fv(
      const std::optional<WebGlUniformLocation>& location,
      bool transpose,
      const Float32Array& values) const;
  void uniform_matrix4fv(
      const std::optional<WebGlUniformLocation>& location,
      bool transpose,
      const Array<double>& values) const;
  void use_program(const std::optional<WebGlProgram>& program) const;
  void use_program(std::nullptr_t) const;
  void vertex_attrib4f(std::uint32_t index, float x, float y, float z, float w) const;
  void vertex_attrib_divisor(std::uint32_t index, std::uint32_t divisor) const;
  void vertex_attrib_pointer(
      std::uint32_t index,
      int size,
      std::uint32_t type,
      bool normalized,
      int stride,
      std::ptrdiff_t offset) const;

  void clear(std::uint32_t mask) const;
  void clear_color(float red, float green, float blue, float alpha) const;
  void disable(std::uint32_t capability) const;
  void enable(std::uint32_t capability) const;
  [[nodiscard]] std::uint32_t get_error() const;
  void scissor(int x, int y, int width, int height) const;
  void viewport(int x, int y, int width, int height) const;

  void make_current() const;
  void present() const;
  void set_swap_interval(int interval) const;

 private:
  friend class GlCanvas;

  explicit WebGl2Context(std::shared_ptr<detail::GlSurfaceState> state) noexcept
      : state_(std::move(state)) {}

  [[nodiscard]] detail::GlSurfaceState& require_state() const;
  [[nodiscard]] std::shared_ptr<detail::WebGlObjectState> create_object_state(
      detail::WebGlObjectKind kind,
      std::uint32_t name) const;
  [[nodiscard]] std::shared_ptr<detail::WebGlObjectState> find_object_state(
      detail::WebGlObjectKind kind,
      std::uint32_t name) const;
  [[nodiscard]] std::uint32_t require_object(
      const std::shared_ptr<detail::WebGlObjectState>& object,
      detail::WebGlObjectKind kind) const;
  [[nodiscard]] int require_uniform_location(
      const std::optional<WebGlUniformLocation>& location) const;
  void invalidate_object(
      const std::shared_ptr<detail::WebGlObjectState>& object,
      detail::WebGlObjectKind kind) const;

  std::shared_ptr<detail::GlSurfaceState> state_;
};

// Native replacement for the HTMLCanvasElement returned by GlRenderSurfaceProvider. Logical size
// and drawable pixel size stay separate, matching Flight's width/height/pixelRatio contract.
class FLIGHT_HOST_SDL_GL_API GlCanvas final {
 public:
  GlCanvas() noexcept = default;

  DomStyle style;
  double height{0.0};
  double width{0.0};

  [[nodiscard]] static GlCanvas create(
      int width,
      int height,
      double pixel_ratio = 1.0,
      String title = String("Flight"),
      bool hidden = false);

  [[nodiscard]] explicit operator bool() const noexcept { return state_ != nullptr; }
  [[nodiscard]] friend bool operator==(const GlCanvas&, const GlCanvas&) noexcept = default;
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }
  [[nodiscard]] WindowSize logical_size() const;
  [[nodiscard]] WindowSize pixel_size() const;
  [[nodiscard]] SDL_Window* native_window() const noexcept;
  [[nodiscard]] WebGl2Context get_context() const;
  [[nodiscard]] std::optional<WebGl2Context> get_context(
      const String& context_id,
      const WebGlContextAttributes& attributes) const;
  [[nodiscard]] ClientRect get_bounding_client_rect() const;
  void add_event_listener(const String& type, std::function<void(InputPointerData)> callback);
  void add_event_listener(const String& type, std::function<void()> callback);
  void add_event_listener(
      const String& type,
      std::function<void(InputPointerData)> callback,
      const EventListenerOptions&) {
    add_event_listener(type, std::move(callback));
  }

  template <typename Callback, typename Options>
  void add_event_listener(const String& type, Callback callback, const Options&) {
    add_event_listener(type, std::move(callback));
  }

  void emit_pointer(const String& type, InputPointerData event) const;
  void set_pointer_capture(double) const noexcept {}
  void release_pointer_capture(double) const noexcept {}

 private:
  explicit GlCanvas(std::shared_ptr<detail::GlSurfaceState> state) noexcept
      : state_(std::move(state)) {}

  [[nodiscard]] detail::GlSurfaceState& require_state() const;

  std::shared_ptr<detail::GlSurfaceState> state_;
};

} // namespace flight::host_sdl

namespace std {

template <typename Tag>
struct hash<flight::host_sdl::WebGlHandle<Tag>> {
  [[nodiscard]] size_t operator()(const flight::host_sdl::WebGlHandle<Tag>& value) const noexcept {
    return hash<const void*>{}(value.identity());
  }
};

template <>
struct hash<flight::host_sdl::GlImageSource> {
  [[nodiscard]] size_t operator()(const flight::host_sdl::GlImageSource& value) const noexcept {
    return hash<const void*>{}(value.identity());
  }
};

} // namespace std
