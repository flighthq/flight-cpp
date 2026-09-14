#include <flight/host_sdl/webgl.hpp>

#include <flight/host_sdl/gl.hpp>

#include <SDL3/SDL_opengles2.h>

#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace flight::host_sdl::detail {

struct GlSurfaceState final {
  explicit GlSurfaceState(WindowOptions options) : window(std::move(options)), context(window) {}

  Window window;
  GlContext context;
  std::map<std::string, SDL_FunctionPointer, std::less<>> functions;
  std::map<
      std::pair<WebGlObjectKind, std::uint32_t>,
      std::weak_ptr<WebGlObjectState>> objects;
  bool unpack_premultiply_alpha{false};
  std::vector<std::pair<String, std::function<void(InputPointerData)>>> pointer_listeners;
  std::vector<std::pair<String, std::function<void()>>> simple_listeners;
};

struct GlImageSourceState final {
  std::size_t width;
  std::size_t height;
  Uint8ClampedArray pixels;
};

SDL_FunctionPointer cached_function(GlSurfaceState& state, std::string_view name) {
  const auto existing = state.functions.find(name);
  if (existing != state.functions.end()) return existing->second;
  const auto function = state.context.function_address(name);
  state.functions.emplace(std::string(name), function);
  return function;
}

WebGlObjectState::WebGlObjectState(
    std::weak_ptr<GlSurfaceState> owner_value,
    WebGlObjectKind kind_value,
    std::uint32_t name_value) noexcept
    : owner(std::move(owner_value)),
      kind(kind_value),
      name(name_value),
      valid(kind_value == WebGlObjectKind::uniform_location || name_value != 0) {}

WebGlObjectState::~WebGlObjectState() noexcept {
  auto surface = owner.lock();
  if (!surface || !valid) return;
  try {
    surface->context.make_current(surface->window);
    const auto delete_one = [&](std::string_view function_name) {
      using DeleteFunction = void(GL_APIENTRYP)(GLsizei, const GLuint*);
      reinterpret_cast<DeleteFunction>(cached_function(*surface, function_name))(1, &name);
    };
    switch (kind) {
      case WebGlObjectKind::buffer: delete_one("glDeleteBuffers"); break;
      case WebGlObjectKind::framebuffer: delete_one("glDeleteFramebuffers"); break;
      case WebGlObjectKind::program:
        reinterpret_cast<PFNGLDELETEPROGRAMPROC>(cached_function(*surface, "glDeleteProgram"))(name);
        break;
      case WebGlObjectKind::renderbuffer: delete_one("glDeleteRenderbuffers"); break;
      case WebGlObjectKind::shader:
        reinterpret_cast<PFNGLDELETESHADERPROC>(cached_function(*surface, "glDeleteShader"))(name);
        break;
      case WebGlObjectKind::texture: delete_one("glDeleteTextures"); break;
      case WebGlObjectKind::vertex_array: delete_one("glDeleteVertexArrays"); break;
      case WebGlObjectKind::uniform_location: break;
    }
    name = 0;
    valid = false;
  } catch (...) {
    // Destruction after context loss cannot report an error and the GL server already owns cleanup.
  }
}

} // namespace flight::host_sdl::detail

namespace flight::host_sdl {

namespace {

using BindVertexArrayFunction = void(GL_APIENTRYP)(GLuint);
using BlitFramebufferFunction = void(GL_APIENTRYP)(
    GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
using ClearBufferfiFunction = void(GL_APIENTRYP)(GLenum, GLint, GLfloat, GLint);
using ClearBufferfvFunction = void(GL_APIENTRYP)(GLenum, GLint, const GLfloat*);
using CompressedTexSubImage3DFunction = void(GL_APIENTRYP)(
    GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const void*);
using DeleteVertexArraysFunction = void(GL_APIENTRYP)(GLsizei, const GLuint*);
using DrawArraysInstancedFunction = void(GL_APIENTRYP)(GLenum, GLint, GLsizei, GLsizei);
using DrawBuffersFunction = void(GL_APIENTRYP)(GLsizei, const GLenum*);
using DrawElementsInstancedFunction =
    void(GL_APIENTRYP)(GLenum, GLsizei, GLenum, const void*, GLsizei);
using GenVertexArraysFunction = void(GL_APIENTRYP)(GLsizei, GLuint*);
using ReadBufferFunction = void(GL_APIENTRYP)(GLenum);
using RenderbufferStorageMultisampleFunction =
    void(GL_APIENTRYP)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
using TexImage3DFunction = void(GL_APIENTRYP)(
    GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
using TexStorage3DFunction =
    void(GL_APIENTRYP)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei);
using VertexAttribDivisorFunction = void(GL_APIENTRYP)(GLuint, GLuint);

struct GlByteRange final {
  const std::byte* data;
  GLsizei size;
};

GlByteRange gl_byte_range(
    const ArrayBufferView& source,
    std::size_t source_offset,
    std::optional<std::size_t> source_length,
    std::string_view operation) {
  const auto element_size = source.bytes_per_element();
  const auto element_count = source.byte_length / element_size;
  if (source_offset > element_count) {
    throw std::range_error(std::string(operation) + " source offset exceeds its view");
  }
  const auto selected_elements = source_length.value_or(element_count - source_offset);
  if (selected_elements > element_count - source_offset ||
      selected_elements > std::numeric_limits<std::size_t>::max() / element_size) {
    throw std::range_error(std::string(operation) + " source length exceeds its view");
  }
  const auto selected_bytes = selected_elements * element_size;
  if (selected_bytes > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
    throw std::range_error(std::string(operation) + " source length exceeds the native GL range");
  }
  return {
      source.data() + (source_offset * element_size),
      static_cast<GLsizei>(selected_bytes),
  };
}

std::size_t checked_texel_bytes(
    int width,
    int height,
    int depth,
    std::uint32_t format,
    std::uint32_t type,
    std::string_view operation) {
  if (width < 0 || height < 0 || depth < 0) {
    throw std::invalid_argument(std::string(operation) + " dimensions cannot be negative");
  }
  std::size_t bytes_per_pixel = 0;
  if (format == WebGl2Context::rgba) {
    switch (type) {
      case WebGl2Context::unsigned_byte: bytes_per_pixel = 4; break;
      case WebGl2Context::half_float: bytes_per_pixel = 8; break;
      case WebGl2Context::float_: bytes_per_pixel = 16; break;
      default:
        throw std::invalid_argument(std::string(operation) + " received an unsupported pixel type");
    }
  } else if (
      format == WebGl2Context::depth_stencil &&
      type == WebGl2Context::unsigned_int_24_8) {
    bytes_per_pixel = 4;
  } else {
    throw std::invalid_argument(std::string(operation) + " received an unsupported pixel layout");
  }
  auto size = static_cast<std::size_t>(width);
  for (const auto factor : {height, depth, static_cast<int>(bytes_per_pixel)}) {
    const auto unsigned_factor = static_cast<std::size_t>(factor);
    if (unsigned_factor != 0 && size > std::numeric_limits<std::size_t>::max() / unsigned_factor) {
      throw std::range_error(std::string(operation) + " pixel span exceeds addressable storage");
    }
    size *= unsigned_factor;
  }
  return size;
}

void require_texel_storage(
    const ArrayBufferView& view,
    int width,
    int height,
    int depth,
    std::uint32_t format,
    std::uint32_t type,
    std::string_view operation) {
  if (view.byte_length < checked_texel_bytes(width, height, depth, format, type, operation)) {
    throw std::range_error(std::string(operation) + " pixel view is too small");
  }
}

GLsizei gl_value_count(std::size_t values, std::size_t width, std::string_view operation) {
  if (values % width != 0 || values / width > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
    throw std::invalid_argument(std::string(operation) + " received an invalid value count");
  }
  return static_cast<GLsizei>(values / width);
}

std::vector<GLfloat> gl_float_values(const Array<double>& values) {
  std::vector<GLfloat> output;
  output.reserve(values.size());
  for (const auto value : values) output.push_back(static_cast<GLfloat>(value));
  return output;
}

template <typename Function>
Function gl_function(const WebGl2Context& context, std::string_view name) {
  context.make_current();
  return reinterpret_cast<Function>(context.function_address(name));
}

} // namespace

GlImageSource GlImageSource::rgba8(
    std::size_t width,
    std::size_t height,
    Uint8ClampedArray pixels) {
  if (width != 0 && height > std::numeric_limits<std::size_t>::max() / width) {
    throw std::length_error("SDL GL image dimensions exceed addressable storage");
  }
  const auto pixel_count = width * height;
  if (pixel_count > std::numeric_limits<std::size_t>::max() / 4 ||
      pixels.size() != pixel_count * 4) {
    throw std::invalid_argument("SDL GL RGBA8 image byte count does not match its dimensions");
  }
  return GlImageSource(
      std::make_shared<detail::GlImageSourceState>(
          detail::GlImageSourceState{width, height, std::move(pixels)}));
}

std::size_t GlImageSource::width() const noexcept { return state_ ? state_->width : 0; }

std::size_t GlImageSource::height() const noexcept { return state_ ? state_->height : 0; }

std::span<const Uint8Clamped> GlImageSource::rgba8_pixels() const noexcept {
  return state_ ? state_->pixels.span() : std::span<const Uint8Clamped>{};
}

std::optional<GlImageSource> GlImageSource::lock_weak(const weak_type& weak) noexcept {
  auto state = weak.lock();
  return state ? std::optional<GlImageSource>(GlImageSource(std::move(state))) : std::nullopt;
}

std::size_t GlImageSourceWeakPolicy::hash(identity_type identity) noexcept {
  return std::hash<const void*>{}(identity);
}

GlParameterValue::operator double() const {
  const auto* value = std::get_if<double>(&value_);
  if (!value) throw std::logic_error("WebGL parameter is not numeric");
  return *value;
}

GlParameterValue::operator bool() const {
  const auto* value = std::get_if<bool>(&value_);
  if (!value) throw std::logic_error("WebGL parameter is not boolean");
  return *value;
}

GlParameterValue::operator Array<double>() const {
  const auto* value = std::get_if<Array<double>>(&value_);
  if (!value) throw std::logic_error("WebGL parameter is not a numeric array");
  return *value;
}

GlParameterValue::operator Array<bool>() const {
  const auto* value = std::get_if<Array<bool>>(&value_);
  if (!value) throw std::logic_error("WebGL parameter is not a boolean array");
  return *value;
}

GlParameterValue::operator std::variant<SequenceView<double>, Null, Undefined>() const {
  const auto* value = std::get_if<Array<double>>(&value_);
  if (!value) throw std::logic_error("WebGL parameter is not a numeric sequence");
  return SequenceView<double>(*value);
}

GlParameterValue::operator std::variant<SequenceView<bool>, Null, Undefined>() const {
  const auto* value = std::get_if<Array<bool>>(&value_);
  if (!value) throw std::logic_error("WebGL parameter is not a boolean sequence");
  return SequenceView<bool>(*value);
}

GlParameterValue::operator std::optional<WebGlFramebuffer>() const {
  const auto* value = std::get_if<std::optional<WebGlFramebuffer>>(&value_);
  if (!value) throw std::logic_error("WebGL parameter is not a framebuffer");
  return *value;
}

GlParameterValue::operator std::optional<WebGlProgram>() const {
  const auto* value = std::get_if<std::optional<WebGlProgram>>(&value_);
  if (!value) throw std::logic_error("WebGL parameter is not a program");
  return *value;
}

GlParameterValue::operator std::optional<WebGlTexture>() const {
  const auto* value = std::get_if<std::optional<WebGlTexture>>(&value_);
  if (!value) throw std::logic_error("WebGL parameter is not a texture");
  return *value;
}

GlParameterValue::operator std::optional<WebGlVertexArrayObject>() const {
  const auto* value = std::get_if<std::optional<WebGlVertexArrayObject>>(&value_);
  if (!value) throw std::logic_error("WebGL parameter is not a vertex array");
  return *value;
}

int WebGl2Context::drawing_buffer_width() const { return require_state().window.pixel_size().width; }

int WebGl2Context::drawing_buffer_height() const { return require_state().window.pixel_size().height; }

SDL_FunctionPointer WebGl2Context::function_address(std::string_view name) const {
  auto& state = require_state();
  return detail::cached_function(state, name);
}

bool WebGl2Context::supports_extension(std::string_view name) const {
  auto& state = require_state();
  state.context.make_current(state.window);
  const std::string terminated(name);
  return SDL_GL_ExtensionSupported(terminated.c_str());
}

std::optional<GlAnisotropyExtension> WebGl2Context::anisotropy_extension() const {
  if (!supports_extension("GL_EXT_texture_filter_anisotropic") &&
      !supports_extension("GL_ARB_texture_filter_anisotropic")) {
    return std::nullopt;
  }
  return GlAnisotropyExtension{};
}

WebGlBuffer WebGl2Context::create_buffer() const {
  GLuint name = 0;
  gl_function<PFNGLGENBUFFERSPROC>(*this, "glGenBuffers")(1, &name);
  return WebGlBuffer(create_object_state(detail::WebGlObjectKind::buffer, name));
}

WebGlFramebuffer WebGl2Context::create_framebuffer() const {
  GLuint name = 0;
  gl_function<PFNGLGENFRAMEBUFFERSPROC>(*this, "glGenFramebuffers")(1, &name);
  return WebGlFramebuffer(create_object_state(detail::WebGlObjectKind::framebuffer, name));
}

WebGlProgram WebGl2Context::create_program() const {
  const auto name = gl_function<PFNGLCREATEPROGRAMPROC>(*this, "glCreateProgram")();
  return WebGlProgram(create_object_state(detail::WebGlObjectKind::program, name));
}

WebGlRenderbuffer WebGl2Context::create_renderbuffer() const {
  GLuint name = 0;
  gl_function<PFNGLGENRENDERBUFFERSPROC>(*this, "glGenRenderbuffers")(1, &name);
  return WebGlRenderbuffer(create_object_state(detail::WebGlObjectKind::renderbuffer, name));
}

std::optional<WebGlShader> WebGl2Context::create_shader(std::uint32_t type) const {
  const auto name = gl_function<PFNGLCREATESHADERPROC>(*this, "glCreateShader")(type);
  if (name == 0) return std::nullopt;
  return WebGlShader(create_object_state(detail::WebGlObjectKind::shader, name));
}

WebGlTexture WebGl2Context::create_texture() const {
  GLuint name = 0;
  gl_function<PFNGLGENTEXTURESPROC>(*this, "glGenTextures")(1, &name);
  return WebGlTexture(create_object_state(detail::WebGlObjectKind::texture, name));
}

WebGlVertexArrayObject WebGl2Context::create_vertex_array() const {
  GLuint name = 0;
  gl_function<GenVertexArraysFunction>(*this, "glGenVertexArrays")(1, &name);
  return WebGlVertexArrayObject(create_object_state(detail::WebGlObjectKind::vertex_array, name));
}

void WebGl2Context::active_texture(std::uint32_t texture) const {
  gl_function<PFNGLACTIVETEXTUREPROC>(*this, "glActiveTexture")(texture);
}

void WebGl2Context::attach_shader(
    const WebGlProgram& program,
    const std::optional<WebGlShader>& shader) const {
  gl_function<PFNGLATTACHSHADERPROC>(*this, "glAttachShader")(
      require_object(program.state_, detail::WebGlObjectKind::program),
      require_object(shader ? shader->state_ : nullptr, detail::WebGlObjectKind::shader));
}

void WebGl2Context::bind_buffer(
    std::uint32_t target,
    const std::optional<WebGlBuffer>& buffer) const {
  const auto name = buffer ? require_object(buffer->state_, detail::WebGlObjectKind::buffer) : 0;
  gl_function<PFNGLBINDBUFFERPROC>(*this, "glBindBuffer")(target, name);
}

void WebGl2Context::bind_buffer(std::uint32_t target, std::nullptr_t) const {
  bind_buffer(target, std::nullopt);
}

void WebGl2Context::bind_framebuffer(
    std::uint32_t target,
    const std::optional<WebGlFramebuffer>& framebuffer) const {
  const auto name = framebuffer
                        ? require_object(framebuffer->state_, detail::WebGlObjectKind::framebuffer)
                        : 0;
  gl_function<PFNGLBINDFRAMEBUFFERPROC>(*this, "glBindFramebuffer")(target, name);
}

void WebGl2Context::bind_framebuffer(std::uint32_t target, std::nullptr_t) const {
  bind_framebuffer(target, std::nullopt);
}

void WebGl2Context::bind_renderbuffer(
    std::uint32_t target,
    const std::optional<WebGlRenderbuffer>& renderbuffer) const {
  const auto name = renderbuffer
                        ? require_object(renderbuffer->state_, detail::WebGlObjectKind::renderbuffer)
                        : 0;
  gl_function<PFNGLBINDRENDERBUFFERPROC>(*this, "glBindRenderbuffer")(target, name);
}

void WebGl2Context::bind_renderbuffer(std::uint32_t target, std::nullptr_t) const {
  bind_renderbuffer(target, std::nullopt);
}

void WebGl2Context::bind_texture(
    std::uint32_t target,
    const std::optional<WebGlTexture>& texture) const {
  const auto name = texture ? require_object(texture->state_, detail::WebGlObjectKind::texture) : 0;
  gl_function<PFNGLBINDTEXTUREPROC>(*this, "glBindTexture")(target, name);
}

void WebGl2Context::bind_texture(std::uint32_t target, std::nullptr_t) const {
  bind_texture(target, std::nullopt);
}

void WebGl2Context::bind_vertex_array(
    const std::optional<WebGlVertexArrayObject>& vertex_array) const {
  const auto name = vertex_array
                        ? require_object(vertex_array->state_, detail::WebGlObjectKind::vertex_array)
                        : 0;
  gl_function<BindVertexArrayFunction>(*this, "glBindVertexArray")(name);
}

void WebGl2Context::bind_vertex_array(std::nullptr_t) const {
  bind_vertex_array(std::nullopt);
}

void WebGl2Context::buffer_data(
    std::uint32_t target,
    std::size_t size,
    std::uint32_t usage) const {
  if (size > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max())) {
    throw std::range_error("WebGL buffer size exceeds the native GL range");
  }
  gl_function<PFNGLBUFFERDATAPROC>(*this, "glBufferData")(
      target, static_cast<GLsizeiptr>(size), nullptr, usage);
}

void WebGl2Context::buffer_data(
    std::uint32_t target,
    const ArrayBufferView& source,
    std::uint32_t usage) const {
  if (source.byte_length > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max())) {
    throw std::range_error("WebGL buffer source exceeds the native GL range");
  }
  gl_function<PFNGLBUFFERDATAPROC>(*this, "glBufferData")(
      target,
      static_cast<GLsizeiptr>(source.byte_length),
      source.data(),
      usage);
}

void WebGl2Context::buffer_sub_data(
    std::uint32_t target,
    std::ptrdiff_t destination_offset,
    const ArrayBufferView& source,
    std::size_t source_offset,
    std::optional<std::size_t> length) const {
  if (destination_offset < 0 ||
      static_cast<std::uintmax_t>(destination_offset) >
          static_cast<std::uintmax_t>(std::numeric_limits<GLintptr>::max())) {
    throw std::range_error("WebGL buffer destination offset exceeds the native GL range");
  }
  const auto element_size = source.bytes_per_element();
  const auto source_elements = source.byte_length / element_size;
  if (source_offset > source_elements) {
    throw std::range_error("WebGL buffer source offset exceeds its view");
  }
  const auto element_count = length.value_or(source_elements - source_offset);
  if (element_count > source_elements - source_offset) {
    throw std::range_error("WebGL buffer source length exceeds its view");
  }
  if (element_count > std::numeric_limits<std::size_t>::max() / element_size) {
    throw std::range_error("WebGL buffer source range exceeds addressable storage");
  }
  const auto byte_count = element_count * element_size;
  const auto byte_offset = source_offset * element_size;
  if (byte_count > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max())) {
    throw std::range_error("WebGL buffer source range exceeds the native GL range");
  }
  gl_function<PFNGLBUFFERSUBDATAPROC>(*this, "glBufferSubData")(
      target,
      static_cast<GLintptr>(destination_offset),
      static_cast<GLsizeiptr>(byte_count),
      source.data() + byte_offset);
}

void WebGl2Context::blend_equation(std::uint32_t mode) const {
  gl_function<PFNGLBLENDEQUATIONPROC>(*this, "glBlendEquation")(mode);
}

void WebGl2Context::blend_equation_separate(std::uint32_t rgb, std::uint32_t alpha) const {
  gl_function<PFNGLBLENDEQUATIONSEPARATEPROC>(*this, "glBlendEquationSeparate")(rgb, alpha);
}

void WebGl2Context::blend_func(std::uint32_t source, std::uint32_t destination) const {
  gl_function<PFNGLBLENDFUNCPROC>(*this, "glBlendFunc")(source, destination);
}

void WebGl2Context::blend_func_separate(
    std::uint32_t source_rgb,
    std::uint32_t destination_rgb,
    std::uint32_t source_alpha,
    std::uint32_t destination_alpha) const {
  gl_function<PFNGLBLENDFUNCSEPARATEPROC>(*this, "glBlendFuncSeparate")(
      source_rgb, destination_rgb, source_alpha, destination_alpha);
}

void WebGl2Context::blit_framebuffer(
    int source_x0,
    int source_y0,
    int source_x1,
    int source_y1,
    int destination_x0,
    int destination_y0,
    int destination_x1,
    int destination_y1,
    std::uint32_t mask,
    std::uint32_t filter) const {
  gl_function<BlitFramebufferFunction>(*this, "glBlitFramebuffer")(
      source_x0,
      source_y0,
      source_x1,
      source_y1,
      destination_x0,
      destination_y0,
      destination_x1,
      destination_y1,
      mask,
      filter);
}

std::uint32_t WebGl2Context::check_framebuffer_status(std::uint32_t target) const {
  return gl_function<PFNGLCHECKFRAMEBUFFERSTATUSPROC>(*this, "glCheckFramebufferStatus")(target);
}

void WebGl2Context::clear_bufferfi(
    std::uint32_t buffer,
    int draw_buffer,
    float depth,
    int stencil) const {
  gl_function<ClearBufferfiFunction>(*this, "glClearBufferfi")(
      buffer, draw_buffer, depth, stencil);
}

void WebGl2Context::clear_bufferfv(
    std::uint32_t buffer,
    int draw_buffer,
    const Float32Array& values,
    std::size_t source_offset) const {
  const auto data = values.span();
  const auto required_values = buffer == color ? 4U : 1U;
  if (source_offset > data.size() || required_values > data.size() - source_offset) {
    throw std::range_error("clearBufferfv source does not contain the required values");
  }
  gl_function<ClearBufferfvFunction>(*this, "glClearBufferfv")(
      buffer, draw_buffer, data.data() + source_offset);
}

void WebGl2Context::clear_bufferfv(
    std::uint32_t buffer,
    int draw_buffer,
    const Array<double>& values,
    std::size_t source_offset) const {
  if (source_offset > values.size()) {
    throw std::range_error("clearBufferfv source offset exceeds its array");
  }
  const Array<double> selected(values.begin() + static_cast<std::ptrdiff_t>(source_offset), values.end());
  const auto data = gl_float_values(selected);
  const auto required_values = buffer == color ? 4U : 1U;
  if (required_values > data.size()) {
    throw std::range_error("clearBufferfv source does not contain the required values");
  }
  gl_function<ClearBufferfvFunction>(*this, "glClearBufferfv")(
      buffer, draw_buffer, data.data());
}

void WebGl2Context::clear_depth(float depth) const {
  gl_function<PFNGLCLEARDEPTHFPROC>(*this, "glClearDepthf")(depth);
}

void WebGl2Context::color_mask(bool red, bool green, bool blue, bool alpha) const {
  gl_function<PFNGLCOLORMASKPROC>(*this, "glColorMask")(red, green, blue, alpha);
}

void WebGl2Context::compile_shader(const std::optional<WebGlShader>& shader) const {
  gl_function<PFNGLCOMPILESHADERPROC>(*this, "glCompileShader")(
      require_object(shader ? shader->state_ : nullptr, detail::WebGlObjectKind::shader));
}

void WebGl2Context::compressed_tex_image2_d(
    std::uint32_t target,
    int level,
    std::uint32_t internal_format,
    int width,
    int height,
    int border,
    const ArrayBufferView& source,
    std::size_t source_offset,
    std::optional<std::size_t> source_length) const {
  const auto range = gl_byte_range(source, source_offset, source_length, "compressedTexImage2D");
  gl_function<PFNGLCOMPRESSEDTEXIMAGE2DPROC>(*this, "glCompressedTexImage2D")(
      target, level, internal_format, width, height, border, range.size, range.data);
}

void WebGl2Context::compressed_tex_sub_image3_d(
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
    std::size_t source_offset,
    std::optional<std::size_t> source_length) const {
  const auto range =
      gl_byte_range(source, source_offset, source_length, "compressedTexSubImage3D");
  gl_function<CompressedTexSubImage3DFunction>(*this, "glCompressedTexSubImage3D")(
      target,
      level,
      x_offset,
      y_offset,
      z_offset,
      width,
      height,
      depth,
      format,
      range.size,
      range.data);
}

void WebGl2Context::cull_face(std::uint32_t mode) const {
  gl_function<PFNGLCULLFACEPROC>(*this, "glCullFace")(mode);
}

void WebGl2Context::delete_buffer(const std::optional<WebGlBuffer>& buffer) const {
  if (!buffer || !buffer->state_ || !buffer->state_->valid) return;
  const auto name = require_object(buffer->state_, detail::WebGlObjectKind::buffer);
  gl_function<PFNGLDELETEBUFFERSPROC>(*this, "glDeleteBuffers")(1, &name);
  invalidate_object(buffer->state_, detail::WebGlObjectKind::buffer);
}

void WebGl2Context::delete_framebuffer(
    const std::optional<WebGlFramebuffer>& framebuffer) const {
  if (!framebuffer || !framebuffer->state_ || !framebuffer->state_->valid) return;
  const auto name = require_object(framebuffer->state_, detail::WebGlObjectKind::framebuffer);
  gl_function<PFNGLDELETEFRAMEBUFFERSPROC>(*this, "glDeleteFramebuffers")(1, &name);
  invalidate_object(framebuffer->state_, detail::WebGlObjectKind::framebuffer);
}

void WebGl2Context::delete_program(const std::optional<WebGlProgram>& program) const {
  if (!program || !program->state_ || !program->state_->valid) return;
  const auto name = require_object(program->state_, detail::WebGlObjectKind::program);
  gl_function<PFNGLDELETEPROGRAMPROC>(*this, "glDeleteProgram")(name);
  invalidate_object(program->state_, detail::WebGlObjectKind::program);
}

void WebGl2Context::delete_renderbuffer(
    const std::optional<WebGlRenderbuffer>& renderbuffer) const {
  if (!renderbuffer || !renderbuffer->state_ || !renderbuffer->state_->valid) return;
  const auto name = require_object(renderbuffer->state_, detail::WebGlObjectKind::renderbuffer);
  gl_function<PFNGLDELETERENDERBUFFERSPROC>(*this, "glDeleteRenderbuffers")(1, &name);
  invalidate_object(renderbuffer->state_, detail::WebGlObjectKind::renderbuffer);
}

void WebGl2Context::delete_shader(const std::optional<WebGlShader>& shader) const {
  if (!shader || !shader->state_ || !shader->state_->valid) return;
  const auto name = require_object(shader->state_, detail::WebGlObjectKind::shader);
  gl_function<PFNGLDELETESHADERPROC>(*this, "glDeleteShader")(name);
  invalidate_object(shader->state_, detail::WebGlObjectKind::shader);
}

void WebGl2Context::delete_texture(const std::optional<WebGlTexture>& texture) const {
  if (!texture || !texture->state_ || !texture->state_->valid) return;
  const auto name = require_object(texture->state_, detail::WebGlObjectKind::texture);
  gl_function<PFNGLDELETETEXTURESPROC>(*this, "glDeleteTextures")(1, &name);
  invalidate_object(texture->state_, detail::WebGlObjectKind::texture);
}

void WebGl2Context::delete_vertex_array(
    const std::optional<WebGlVertexArrayObject>& vertex_array) const {
  if (!vertex_array || !vertex_array->state_ || !vertex_array->state_->valid) return;
  const auto name = require_object(vertex_array->state_, detail::WebGlObjectKind::vertex_array);
  gl_function<DeleteVertexArraysFunction>(*this, "glDeleteVertexArrays")(1, &name);
  invalidate_object(vertex_array->state_, detail::WebGlObjectKind::vertex_array);
}

void WebGl2Context::depth_func(std::uint32_t function) const {
  gl_function<PFNGLDEPTHFUNCPROC>(*this, "glDepthFunc")(function);
}

void WebGl2Context::depth_mask(bool enabled) const {
  gl_function<PFNGLDEPTHMASKPROC>(*this, "glDepthMask")(enabled);
}

void WebGl2Context::disable_vertex_attrib_array(std::uint32_t index) const {
  gl_function<PFNGLDISABLEVERTEXATTRIBARRAYPROC>(*this, "glDisableVertexAttribArray")(index);
}

void WebGl2Context::draw_arrays(std::uint32_t mode, int first, int count) const {
  gl_function<PFNGLDRAWARRAYSPROC>(*this, "glDrawArrays")(mode, first, count);
}

void WebGl2Context::draw_arrays_instanced(
    std::uint32_t mode,
    int first,
    int count,
    int instance_count) const {
  gl_function<DrawArraysInstancedFunction>(*this, "glDrawArraysInstanced")(
      mode, first, count, instance_count);
}

void WebGl2Context::draw_buffers(const Array<double>& buffers) const {
  if (buffers.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
    throw std::range_error("drawBuffers source exceeds the native GL range");
  }
  std::vector<GLenum> native_buffers;
  native_buffers.reserve(buffers.size());
  for (const auto buffer : buffers) {
    if (!std::isfinite(buffer) || buffer < 0.0 ||
        buffer > static_cast<double>(std::numeric_limits<GLenum>::max())) {
      throw std::range_error("drawBuffers received an invalid enum value");
    }
    native_buffers.push_back(static_cast<GLenum>(buffer));
  }
  gl_function<DrawBuffersFunction>(*this, "glDrawBuffers")(
      static_cast<GLsizei>(native_buffers.size()), native_buffers.data());
}

void WebGl2Context::draw_buffers(const Array<std::uint32_t>& buffers) const {
  if (buffers.size() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
    throw std::range_error("drawBuffers source exceeds the native GL range");
  }
  std::vector<GLenum> native_buffers(buffers.begin(), buffers.end());
  gl_function<DrawBuffersFunction>(*this, "glDrawBuffers")(
      static_cast<GLsizei>(native_buffers.size()), native_buffers.data());
}

void WebGl2Context::draw_elements(
    std::uint32_t mode,
    int count,
    std::uint32_t type,
    std::ptrdiff_t offset) const {
  if (offset < 0) throw std::range_error("WebGL element offset cannot be negative");
  gl_function<PFNGLDRAWELEMENTSPROC>(*this, "glDrawElements")(
      mode, count, type, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(offset)));
}

void WebGl2Context::draw_elements_instanced(
    std::uint32_t mode,
    int count,
    std::uint32_t type,
    std::ptrdiff_t offset,
    int instance_count) const {
  if (offset < 0) throw std::range_error("WebGL element offset cannot be negative");
  gl_function<DrawElementsInstancedFunction>(*this, "glDrawElementsInstanced")(
      mode,
      count,
      type,
      reinterpret_cast<const void*>(static_cast<std::uintptr_t>(offset)),
      instance_count);
}

void WebGl2Context::enable_vertex_attrib_array(std::uint32_t index) const {
  gl_function<PFNGLENABLEVERTEXATTRIBARRAYPROC>(*this, "glEnableVertexAttribArray")(index);
}

void WebGl2Context::finish() const { gl_function<PFNGLFINISHPROC>(*this, "glFinish")(); }

void WebGl2Context::flush() const { gl_function<PFNGLFLUSHPROC>(*this, "glFlush")(); }

void WebGl2Context::framebuffer_renderbuffer(
    std::uint32_t target,
    std::uint32_t attachment,
    std::uint32_t renderbuffer_target,
    const std::optional<WebGlRenderbuffer>& renderbuffer) const {
  const auto name = renderbuffer
                        ? require_object(renderbuffer->state_, detail::WebGlObjectKind::renderbuffer)
                        : 0;
  gl_function<PFNGLFRAMEBUFFERRENDERBUFFERPROC>(*this, "glFramebufferRenderbuffer")(
      target, attachment, renderbuffer_target, name);
}

void WebGl2Context::framebuffer_texture2_d(
    std::uint32_t target,
    std::uint32_t attachment,
    std::uint32_t texture_target,
    const std::optional<WebGlTexture>& texture,
    int level) const {
  const auto name = texture ? require_object(texture->state_, detail::WebGlObjectKind::texture) : 0;
  gl_function<PFNGLFRAMEBUFFERTEXTURE2DPROC>(*this, "glFramebufferTexture2D")(
      target, attachment, texture_target, name, level);
}

void WebGl2Context::front_face(std::uint32_t mode) const {
  gl_function<PFNGLFRONTFACEPROC>(*this, "glFrontFace")(mode);
}

void WebGl2Context::generate_mipmap(std::uint32_t target) const {
  gl_function<PFNGLGENERATEMIPMAPPROC>(*this, "glGenerateMipmap")(target);
}

std::optional<WebGlActiveInfo> WebGl2Context::get_active_uniform(
    const WebGlProgram& program,
    int index) const {
  if (index < 0) return std::nullopt;
  const auto name = require_object(program.state_, detail::WebGlObjectKind::program);
  GLint active_count = 0;
  const auto get_program = gl_function<PFNGLGETPROGRAMIVPROC>(*this, "glGetProgramiv");
  get_program(name, GL_ACTIVE_UNIFORMS, &active_count);
  if (index >= active_count) return std::nullopt;
  GLint maximum_length = 0;
  get_program(name, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maximum_length);
  if (maximum_length <= 0) return std::nullopt;
  std::string uniform_name(static_cast<std::size_t>(maximum_length), '\0');
  GLsizei written = 0;
  GLint size = 0;
  GLenum type = 0;
  gl_function<PFNGLGETACTIVEUNIFORMPROC>(*this, "glGetActiveUniform")(
      name,
      static_cast<GLuint>(index),
      maximum_length,
      &written,
      &size,
      &type,
      uniform_name.data());
  uniform_name.resize(written > 0 ? static_cast<std::size_t>(written) : 0);
  return WebGlActiveInfo{
      .name = String(uniform_name),
      .size = static_cast<double>(size),
      .type = static_cast<double>(type),
  };
}

int WebGl2Context::get_attrib_location(const WebGlProgram& program, const String& name) const {
  const auto encoded = name.to_utf8();
  return gl_function<PFNGLGETATTRIBLOCATIONPROC>(*this, "glGetAttribLocation")(
      require_object(program.state_, detail::WebGlObjectKind::program), encoded.c_str());
}

GlExtension WebGl2Context::get_extension(const String& name) const {
  const auto requested = name.to_utf8();
  bool available = false;
  if (requested == "EXT_texture_filter_anisotropic") {
    available =
        supports_extension("GL_EXT_texture_filter_anisotropic") ||
        supports_extension("GL_ARB_texture_filter_anisotropic");
  } else if (requested == "EXT_color_buffer_float") {
    available = supports_extension("GL_EXT_color_buffer_float");
  } else if (requested == "WEBGL_compressed_texture_astc") {
    available =
        supports_extension("GL_KHR_texture_compression_astc_ldr") ||
        supports_extension("GL_KHR_texture_compression_astc_hdr");
  } else if (requested == "EXT_texture_compression_bptc") {
    available = supports_extension("GL_EXT_texture_compression_bptc");
  } else if (requested == "WEBGL_compressed_texture_etc") {
    // ETC2/EAC is part of the OpenGL ES 3.0 core requested by GlCanvas.
    available = true;
  } else if (requested == "WEBGL_compressed_texture_pvrtc") {
    available = supports_extension("GL_IMG_texture_compression_pvrtc");
  } else if (requested == "EXT_texture_compression_rgtc") {
    available = supports_extension("GL_EXT_texture_compression_rgtc");
  } else if (requested == "WEBGL_compressed_texture_s3tc") {
    available = supports_extension("GL_EXT_texture_compression_s3tc");
  } else if (requested == "WEBGL_compressed_texture_s3tc_srgb") {
    available = supports_extension("GL_EXT_texture_compression_s3tc_srgb");
  } else {
    available = supports_extension(requested);
    if (!available && !requested.starts_with("GL_")) {
      available = supports_extension("GL_" + requested);
    }
  }
  return GlExtension(available);
}

GlParameterValue WebGl2Context::get_parameter(std::uint32_t parameter) const {
  if (parameter == unpack_premultiply_alpha_webgl) {
    return GlParameterValue(require_state().unpack_premultiply_alpha);
  }
  if (parameter == color_clear_value) {
    GLfloat values[4]{};
    gl_function<PFNGLGETFLOATVPROC>(*this, "glGetFloatv")(parameter, values);
    return GlParameterValue(Array<double>{values[0], values[1], values[2], values[3]});
  }
  if (parameter == color_writemask) {
    GLboolean values[4]{};
    gl_function<PFNGLGETBOOLEANVPROC>(*this, "glGetBooleanv")(parameter, values);
    return GlParameterValue(Array<bool>{
        values[0] != 0,
        values[1] != 0,
        values[2] != 0,
        values[3] != 0,
    });
  }
  if (parameter == scissor_box || parameter == viewport_constant) {
    GLint values[4]{};
    gl_function<PFNGLGETINTEGERVPROC>(*this, "glGetIntegerv")(parameter, values);
    return GlParameterValue(Array<double>{
        static_cast<double>(values[0]),
        static_cast<double>(values[1]),
        static_cast<double>(values[2]),
        static_cast<double>(values[3]),
    });
  }
  if (parameter == depth_writemask) {
    GLboolean value = 0;
    gl_function<PFNGLGETBOOLEANVPROC>(*this, "glGetBooleanv")(parameter, &value);
    return GlParameterValue(value != 0);
  }
  if (parameter == static_cast<std::uint32_t>(GlAnisotropyExtension::max_texture_max_anisotropy_ext)) {
    GLfloat value = 0.0F;
    gl_function<PFNGLGETFLOATVPROC>(*this, "glGetFloatv")(parameter, &value);
    return GlParameterValue(static_cast<double>(value));
  }

  GLint value = 0;
  gl_function<PFNGLGETINTEGERVPROC>(*this, "glGetIntegerv")(parameter, &value);
  const auto name = static_cast<std::uint32_t>(value);
  if (parameter == framebuffer_binding) {
    return GlParameterValue(
        name == 0
            ? std::optional<WebGlFramebuffer>{}
            : std::optional<WebGlFramebuffer>(WebGlFramebuffer(
                  find_object_state(detail::WebGlObjectKind::framebuffer, name))));
  }
  if (parameter == current_program) {
    return GlParameterValue(
        name == 0
            ? std::optional<WebGlProgram>{}
            : std::optional<WebGlProgram>(WebGlProgram(
                  find_object_state(detail::WebGlObjectKind::program, name))));
  }
  if (parameter == texture_binding_2_d) {
    return GlParameterValue(
        name == 0
            ? std::optional<WebGlTexture>{}
            : std::optional<WebGlTexture>(WebGlTexture(
                  find_object_state(detail::WebGlObjectKind::texture, name))));
  }
  if (parameter == vertex_array_binding) {
    return GlParameterValue(
        name == 0
            ? std::optional<WebGlVertexArrayObject>{}
            : std::optional<WebGlVertexArrayObject>(WebGlVertexArrayObject(
                  find_object_state(detail::WebGlObjectKind::vertex_array, name))));
  }
  return GlParameterValue(static_cast<double>(value));
}

std::optional<String> WebGl2Context::get_program_info_log(const WebGlProgram& program) const {
  const auto name = require_object(program.state_, detail::WebGlObjectKind::program);
  GLint length = 0;
  gl_function<PFNGLGETPROGRAMIVPROC>(*this, "glGetProgramiv")(name, GL_INFO_LOG_LENGTH, &length);
  if (length <= 0) return String();
  std::string log(static_cast<std::size_t>(length), '\0');
  GLsizei written = 0;
  gl_function<PFNGLGETPROGRAMINFOLOGPROC>(*this, "glGetProgramInfoLog")(
      name, length, &written, log.data());
  log.resize(written > 0 ? static_cast<std::size_t>(written) : 0);
  return String(log);
}

double WebGl2Context::get_program_parameter(
    const WebGlProgram& program,
    std::uint32_t parameter) const {
  GLint value = 0;
  gl_function<PFNGLGETPROGRAMIVPROC>(*this, "glGetProgramiv")(
      require_object(program.state_, detail::WebGlObjectKind::program), parameter, &value);
  return static_cast<double>(value);
}

std::optional<String> WebGl2Context::get_shader_info_log(
    const std::optional<WebGlShader>& shader) const {
  const auto name =
      require_object(shader ? shader->state_ : nullptr, detail::WebGlObjectKind::shader);
  GLint length = 0;
  gl_function<PFNGLGETSHADERIVPROC>(*this, "glGetShaderiv")(name, GL_INFO_LOG_LENGTH, &length);
  if (length <= 0) return String();
  std::string log(static_cast<std::size_t>(length), '\0');
  GLsizei written = 0;
  gl_function<PFNGLGETSHADERINFOLOGPROC>(*this, "glGetShaderInfoLog")(
      name, length, &written, log.data());
  log.resize(written > 0 ? static_cast<std::size_t>(written) : 0);
  return String(log);
}

double WebGl2Context::get_shader_parameter(
    const std::optional<WebGlShader>& shader,
    std::uint32_t parameter) const {
  GLint value = 0;
  gl_function<PFNGLGETSHADERIVPROC>(*this, "glGetShaderiv")(
      require_object(shader ? shader->state_ : nullptr, detail::WebGlObjectKind::shader),
      parameter,
      &value);
  return static_cast<double>(value);
}

std::optional<WebGlUniformLocation> WebGl2Context::get_uniform_location(
    const WebGlProgram& program,
    const String& name) const {
  const auto encoded = name.to_utf8();
  const auto location = gl_function<PFNGLGETUNIFORMLOCATIONPROC>(*this, "glGetUniformLocation")(
      require_object(program.state_, detail::WebGlObjectKind::program), encoded.c_str());
  if (location < 0) return std::nullopt;
  return WebGlUniformLocation(std::make_shared<detail::WebGlObjectState>(
      state_, detail::WebGlObjectKind::uniform_location, static_cast<std::uint32_t>(location)));
}

bool WebGl2Context::is_enabled(std::uint32_t capability) const {
  return gl_function<PFNGLISENABLEDPROC>(*this, "glIsEnabled")(capability) != 0;
}

void WebGl2Context::link_program(const WebGlProgram& program) const {
  gl_function<PFNGLLINKPROGRAMPROC>(*this, "glLinkProgram")(
      require_object(program.state_, detail::WebGlObjectKind::program));
}

void WebGl2Context::pixel_storei(std::uint32_t parameter, int value) const {
  if (parameter == unpack_premultiply_alpha_webgl) {
    require_state().unpack_premultiply_alpha = value != 0;
    return;
  }
  gl_function<PFNGLPIXELSTOREIPROC>(*this, "glPixelStorei")(parameter, value);
}

void WebGl2Context::read_buffer(std::uint32_t source) const {
  gl_function<ReadBufferFunction>(*this, "glReadBuffer")(source);
}

void WebGl2Context::read_pixels(
    int x,
    int y,
    int width,
    int height,
    std::uint32_t format,
    std::uint32_t type,
    ArrayBufferView destination) const {
  require_texel_storage(destination, width, height, 1, format, type, "readPixels");
  gl_function<PFNGLREADPIXELSPROC>(*this, "glReadPixels")(
      x, y, width, height, format, type, destination.data());
}

void WebGl2Context::renderbuffer_storage(
    std::uint32_t target,
    std::uint32_t internal_format,
    int width,
    int height) const {
  gl_function<PFNGLRENDERBUFFERSTORAGEPROC>(*this, "glRenderbufferStorage")(
      target, internal_format, width, height);
}

void WebGl2Context::renderbuffer_storage_multisample(
    std::uint32_t target,
    int samples,
    std::uint32_t internal_format,
    int width,
    int height) const {
  gl_function<RenderbufferStorageMultisampleFunction>(*this, "glRenderbufferStorageMultisample")(
      target, samples, internal_format, width, height);
}

void WebGl2Context::shader_source(
    const std::optional<WebGlShader>& shader,
    const String& source) const {
  const auto encoded = source.to_utf8();
  if (encoded.size() > static_cast<std::size_t>(std::numeric_limits<GLint>::max())) {
    throw std::range_error("WebGL shader source exceeds the native GL range");
  }
  const GLchar* pointer = encoded.c_str();
  const auto length = static_cast<GLint>(encoded.size());
  gl_function<PFNGLSHADERSOURCEPROC>(*this, "glShaderSource")(
      require_object(shader ? shader->state_ : nullptr, detail::WebGlObjectKind::shader),
      1,
      &pointer,
      &length);
}

void WebGl2Context::stencil_func(
    std::uint32_t function,
    int reference,
    std::uint32_t mask) const {
  gl_function<PFNGLSTENCILFUNCPROC>(*this, "glStencilFunc")(function, reference, mask);
}

void WebGl2Context::stencil_func_separate(
    std::uint32_t face,
    std::uint32_t function,
    int reference,
    std::uint32_t mask) const {
  gl_function<PFNGLSTENCILFUNCSEPARATEPROC>(*this, "glStencilFuncSeparate")(
      face, function, reference, mask);
}

void WebGl2Context::stencil_mask(std::uint32_t mask) const {
  gl_function<PFNGLSTENCILMASKPROC>(*this, "glStencilMask")(mask);
}

void WebGl2Context::stencil_mask_separate(std::uint32_t face, std::uint32_t mask) const {
  gl_function<PFNGLSTENCILMASKSEPARATEPROC>(*this, "glStencilMaskSeparate")(face, mask);
}

void WebGl2Context::stencil_op(
    std::uint32_t fail,
    std::uint32_t depth_fail,
    std::uint32_t depth_pass) const {
  gl_function<PFNGLSTENCILOPPROC>(*this, "glStencilOp")(fail, depth_fail, depth_pass);
}

void WebGl2Context::stencil_op_separate(
    std::uint32_t face,
    std::uint32_t fail,
    std::uint32_t depth_fail,
    std::uint32_t depth_pass) const {
  gl_function<PFNGLSTENCILOPSEPARATEPROC>(*this, "glStencilOpSeparate")(
      face, fail, depth_fail, depth_pass);
}

void WebGl2Context::tex_image2_d(
    std::uint32_t target,
    int level,
    int internal_format,
    int width,
    int height,
    int border,
    std::uint32_t format,
    std::uint32_t type,
    const ArrayBufferView& pixels) const {
  require_texel_storage(pixels, width, height, 1, format, type, "texImage2D");
  gl_function<PFNGLTEXIMAGE2DPROC>(*this, "glTexImage2D")(
      target, level, internal_format, width, height, border, format, type, pixels.data());
}

void WebGl2Context::tex_image2_d(
    std::uint32_t target,
    int level,
    int internal_format,
    int width,
    int height,
    int border,
    std::uint32_t format,
    std::uint32_t type,
    std::nullptr_t) const {
  static_cast<void>(checked_texel_bytes(width, height, 1, format, type, "texImage2D"));
  gl_function<PFNGLTEXIMAGE2DPROC>(*this, "glTexImage2D")(
      target, level, internal_format, width, height, border, format, type, nullptr);
}

void WebGl2Context::tex_image2_d(
    std::uint32_t target,
    int level,
    int internal_format,
    std::uint32_t format,
    std::uint32_t type,
    const GlImageSource& source) const {
  if (!source) throw std::invalid_argument("texImage2D image source is empty");
  if (format != rgba || type != unsigned_byte) {
    throw std::invalid_argument("texImage2D RGBA8 image source requires RGBA and UNSIGNED_BYTE");
  }
  if (source.width() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()) ||
      source.height() > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
    throw std::range_error("texImage2D image dimensions exceed the native GL range");
  }
  const auto pixels = source.rgba8_pixels();
  static_assert(sizeof(Uint8Clamped) == sizeof(GLubyte));
  const void* pixel_data = pixels.data();
  std::vector<GLubyte> premultiplied;
  if (require_state().unpack_premultiply_alpha) {
    premultiplied.reserve(pixels.size());
    for (std::size_t index = 0; index < pixels.size(); index += 4) {
      const auto alpha = static_cast<std::uint8_t>(pixels[index + 3]);
      for (std::size_t channel = 0; channel < 3; ++channel) {
        const auto color = static_cast<std::uint8_t>(pixels[index + channel]);
        premultiplied.push_back(static_cast<GLubyte>(
            (static_cast<unsigned int>(color) * alpha + 127U) / 255U));
      }
      premultiplied.push_back(alpha);
    }
    pixel_data = premultiplied.data();
  }
  gl_function<PFNGLTEXIMAGE2DPROC>(*this, "glTexImage2D")(
      target,
      level,
      internal_format,
      static_cast<GLsizei>(source.width()),
      static_cast<GLsizei>(source.height()),
      0,
      format,
      type,
      pixel_data);
}

void WebGl2Context::tex_image3_d(
    std::uint32_t target,
    int level,
    int internal_format,
    int width,
    int height,
    int depth,
    int border,
    std::uint32_t format,
    std::uint32_t type,
    const ArrayBufferView& pixels) const {
  require_texel_storage(pixels, width, height, depth, format, type, "texImage3D");
  gl_function<TexImage3DFunction>(*this, "glTexImage3D")(
      target,
      level,
      internal_format,
      width,
      height,
      depth,
      border,
      format,
      type,
      pixels.data());
}

void WebGl2Context::tex_image3_d(
    std::uint32_t target,
    int level,
    int internal_format,
    int width,
    int height,
    int depth,
    int border,
    std::uint32_t format,
    std::uint32_t type,
    std::nullptr_t) const {
  static_cast<void>(checked_texel_bytes(width, height, depth, format, type, "texImage3D"));
  gl_function<TexImage3DFunction>(*this, "glTexImage3D")(
      target, level, internal_format, width, height, depth, border, format, type, nullptr);
}

void WebGl2Context::tex_parameterf(
    std::uint32_t target,
    std::uint32_t parameter,
    float value) const {
  gl_function<PFNGLTEXPARAMETERFPROC>(*this, "glTexParameterf")(target, parameter, value);
}

void WebGl2Context::tex_parameteri(
    std::uint32_t target,
    std::uint32_t parameter,
    int value) const {
  gl_function<PFNGLTEXPARAMETERIPROC>(*this, "glTexParameteri")(target, parameter, value);
}

void WebGl2Context::tex_storage3_d(
    std::uint32_t target,
    int levels,
    std::uint32_t internal_format,
    int width,
    int height,
    int depth) const {
  gl_function<TexStorage3DFunction>(*this, "glTexStorage3D")(
      target, levels, internal_format, width, height, depth);
}

void WebGl2Context::tex_sub_image2_d(
    std::uint32_t target,
    int level,
    int x_offset,
    int y_offset,
    int width,
    int height,
    std::uint32_t format,
    std::uint32_t type,
    const ArrayBufferView& pixels) const {
  require_texel_storage(pixels, width, height, 1, format, type, "texSubImage2D");
  gl_function<PFNGLTEXSUBIMAGE2DPROC>(*this, "glTexSubImage2D")(
      target, level, x_offset, y_offset, width, height, format, type, pixels.data());
}

void WebGl2Context::uniform1f(
    const std::optional<WebGlUniformLocation>& location,
    float x) const {
  gl_function<PFNGLUNIFORM1FPROC>(*this, "glUniform1f")(require_uniform_location(location), x);
}

void WebGl2Context::uniform1i(
    const std::optional<WebGlUniformLocation>& location,
    int x) const {
  gl_function<PFNGLUNIFORM1IPROC>(*this, "glUniform1i")(require_uniform_location(location), x);
}

void WebGl2Context::uniform2f(
    const std::optional<WebGlUniformLocation>& location,
    float x,
    float y) const {
  gl_function<PFNGLUNIFORM2FPROC>(*this, "glUniform2f")(require_uniform_location(location), x, y);
}

void WebGl2Context::uniform3f(
    const std::optional<WebGlUniformLocation>& location,
    float x,
    float y,
    float z) const {
  gl_function<PFNGLUNIFORM3FPROC>(*this, "glUniform3f")(
      require_uniform_location(location), x, y, z);
}

void WebGl2Context::uniform4f(
    const std::optional<WebGlUniformLocation>& location,
    float x,
    float y,
    float z,
    float w) const {
  gl_function<PFNGLUNIFORM4FPROC>(*this, "glUniform4f")(
      require_uniform_location(location), x, y, z, w);
}

void WebGl2Context::uniform1fv(
    const std::optional<WebGlUniformLocation>& location,
    const Float32Array& values) const {
  const auto data = values.span();
  gl_function<PFNGLUNIFORM1FVPROC>(*this, "glUniform1fv")(
      require_uniform_location(location), gl_value_count(data.size(), 1, "uniform1fv"), data.data());
}

void WebGl2Context::uniform1fv(
    const std::optional<WebGlUniformLocation>& location,
    const Array<double>& values) const {
  const auto data = gl_float_values(values);
  gl_function<PFNGLUNIFORM1FVPROC>(*this, "glUniform1fv")(
      require_uniform_location(location), gl_value_count(data.size(), 1, "uniform1fv"), data.data());
}

void WebGl2Context::uniform2fv(
    const std::optional<WebGlUniformLocation>& location,
    const Float32Array& values) const {
  const auto data = values.span();
  gl_function<PFNGLUNIFORM2FVPROC>(*this, "glUniform2fv")(
      require_uniform_location(location), gl_value_count(data.size(), 2, "uniform2fv"), data.data());
}

void WebGl2Context::uniform2fv(
    const std::optional<WebGlUniformLocation>& location,
    const Array<double>& values) const {
  const auto data = gl_float_values(values);
  gl_function<PFNGLUNIFORM2FVPROC>(*this, "glUniform2fv")(
      require_uniform_location(location), gl_value_count(data.size(), 2, "uniform2fv"), data.data());
}

void WebGl2Context::uniform3fv(
    const std::optional<WebGlUniformLocation>& location,
    const Float32Array& values) const {
  const auto data = values.span();
  gl_function<PFNGLUNIFORM3FVPROC>(*this, "glUniform3fv")(
      require_uniform_location(location), gl_value_count(data.size(), 3, "uniform3fv"), data.data());
}

void WebGl2Context::uniform3fv(
    const std::optional<WebGlUniformLocation>& location,
    const Array<double>& values) const {
  const auto data = gl_float_values(values);
  gl_function<PFNGLUNIFORM3FVPROC>(*this, "glUniform3fv")(
      require_uniform_location(location), gl_value_count(data.size(), 3, "uniform3fv"), data.data());
}

void WebGl2Context::uniform4fv(
    const std::optional<WebGlUniformLocation>& location,
    const Float32Array& values) const {
  const auto data = values.span();
  gl_function<PFNGLUNIFORM4FVPROC>(*this, "glUniform4fv")(
      require_uniform_location(location), gl_value_count(data.size(), 4, "uniform4fv"), data.data());
}

void WebGl2Context::uniform4fv(
    const std::optional<WebGlUniformLocation>& location,
    const Array<double>& values) const {
  const auto data = gl_float_values(values);
  gl_function<PFNGLUNIFORM4FVPROC>(*this, "glUniform4fv")(
      require_uniform_location(location), gl_value_count(data.size(), 4, "uniform4fv"), data.data());
}

void WebGl2Context::uniform_matrix3fv(
    const std::optional<WebGlUniformLocation>& location,
    bool transpose,
    const Float32Array& values) const {
  const auto data = values.span();
  gl_function<PFNGLUNIFORMMATRIX3FVPROC>(*this, "glUniformMatrix3fv")(
      require_uniform_location(location),
      gl_value_count(data.size(), 9, "uniformMatrix3fv"),
      transpose,
      data.data());
}

void WebGl2Context::uniform_matrix3fv(
    const std::optional<WebGlUniformLocation>& location,
    bool transpose,
    const Array<double>& values) const {
  const auto data = gl_float_values(values);
  gl_function<PFNGLUNIFORMMATRIX3FVPROC>(*this, "glUniformMatrix3fv")(
      require_uniform_location(location),
      gl_value_count(data.size(), 9, "uniformMatrix3fv"),
      transpose,
      data.data());
}

void WebGl2Context::uniform_matrix4fv(
    const std::optional<WebGlUniformLocation>& location,
    bool transpose,
    const Float32Array& values) const {
  const auto data = values.span();
  gl_function<PFNGLUNIFORMMATRIX4FVPROC>(*this, "glUniformMatrix4fv")(
      require_uniform_location(location),
      gl_value_count(data.size(), 16, "uniformMatrix4fv"),
      transpose,
      data.data());
}

void WebGl2Context::uniform_matrix4fv(
    const std::optional<WebGlUniformLocation>& location,
    bool transpose,
    const Array<double>& values) const {
  const auto data = gl_float_values(values);
  gl_function<PFNGLUNIFORMMATRIX4FVPROC>(*this, "glUniformMatrix4fv")(
      require_uniform_location(location),
      gl_value_count(data.size(), 16, "uniformMatrix4fv"),
      transpose,
      data.data());
}

void WebGl2Context::use_program(const std::optional<WebGlProgram>& program) const {
  const auto name = program ? require_object(program->state_, detail::WebGlObjectKind::program) : 0;
  gl_function<PFNGLUSEPROGRAMPROC>(*this, "glUseProgram")(name);
}

void WebGl2Context::use_program(std::nullptr_t) const { use_program(std::nullopt); }

void WebGl2Context::vertex_attrib4f(
    std::uint32_t index,
    float x,
    float y,
    float z,
    float w) const {
  gl_function<PFNGLVERTEXATTRIB4FPROC>(*this, "glVertexAttrib4f")(index, x, y, z, w);
}

void WebGl2Context::vertex_attrib_divisor(std::uint32_t index, std::uint32_t divisor) const {
  gl_function<VertexAttribDivisorFunction>(*this, "glVertexAttribDivisor")(index, divisor);
}

void WebGl2Context::vertex_attrib_pointer(
    std::uint32_t index,
    int size,
    std::uint32_t type,
    bool normalized,
    int stride,
    std::ptrdiff_t offset) const {
  if (offset < 0) throw std::range_error("WebGL vertex attribute offset cannot be negative");
  gl_function<PFNGLVERTEXATTRIBPOINTERPROC>(*this, "glVertexAttribPointer")(
      index,
      size,
      type,
      normalized,
      stride,
      reinterpret_cast<const void*>(static_cast<std::uintptr_t>(offset)));
}

void WebGl2Context::clear(std::uint32_t mask) const {
  gl_function<PFNGLCLEARPROC>(*this, "glClear")(mask);
}

void WebGl2Context::clear_color(float red, float green, float blue, float alpha) const {
  gl_function<PFNGLCLEARCOLORPROC>(*this, "glClearColor")(red, green, blue, alpha);
}

void WebGl2Context::disable(std::uint32_t capability) const {
  gl_function<PFNGLDISABLEPROC>(*this, "glDisable")(capability);
}

void WebGl2Context::enable(std::uint32_t capability) const {
  gl_function<PFNGLENABLEPROC>(*this, "glEnable")(capability);
}

std::uint32_t WebGl2Context::get_error() const {
  return gl_function<PFNGLGETERRORPROC>(*this, "glGetError")();
}

void WebGl2Context::scissor(int x, int y, int width, int height) const {
  gl_function<PFNGLSCISSORPROC>(*this, "glScissor")(x, y, width, height);
}

void WebGl2Context::viewport(int x, int y, int width, int height) const {
  gl_function<PFNGLVIEWPORTPROC>(*this, "glViewport")(x, y, width, height);
}

void WebGl2Context::make_current() const {
  auto& state = require_state();
  state.context.make_current(state.window);
}

void WebGl2Context::present() const {
  auto& state = require_state();
  state.context.swap(state.window);
}

void WebGl2Context::set_swap_interval(int interval) const {
  require_state().context.set_swap_interval(interval);
}

detail::GlSurfaceState& WebGl2Context::require_state() const {
  if (!state_) throw std::logic_error("SDL WebGL context is empty");
  return *state_;
}

std::shared_ptr<detail::WebGlObjectState> WebGl2Context::create_object_state(
    detail::WebGlObjectKind kind,
    std::uint32_t name) const {
  auto object = std::make_shared<detail::WebGlObjectState>(state_, kind, name);
  if (object->valid) require_state().objects[{kind, name}] = object;
  return object;
}

std::shared_ptr<detail::WebGlObjectState> WebGl2Context::find_object_state(
    detail::WebGlObjectKind kind,
    std::uint32_t name) const {
  auto& objects = require_state().objects;
  const auto existing = objects.find({kind, name});
  if (existing == objects.end()) {
    throw std::logic_error("native GL returned an object outside this WebGL context");
  }
  auto object = existing->second.lock();
  if (!object || !object->valid) {
    objects.erase(existing);
    throw std::logic_error("native GL returned an expired WebGL object");
  }
  return object;
}

std::uint32_t WebGl2Context::require_object(
    const std::shared_ptr<detail::WebGlObjectState>& object,
    detail::WebGlObjectKind kind) const {
  if (!object || !object->valid) throw std::invalid_argument("WebGL object is empty or deleted");
  const auto owner = object->owner.lock();
  if (!owner || owner.get() != state_.get()) {
    throw std::invalid_argument("WebGL object belongs to a different or destroyed context");
  }
  if (object->kind != kind) throw std::invalid_argument("WebGL object has the wrong native kind");
  return object->name;
}

void WebGl2Context::invalidate_object(
    const std::shared_ptr<detail::WebGlObjectState>& object,
    detail::WebGlObjectKind kind) const {
  static_cast<void>(require_object(object, kind));
  require_state().objects.erase({kind, object->name});
  object->name = 0;
  object->valid = false;
}

int WebGl2Context::require_uniform_location(
    const std::optional<WebGlUniformLocation>& location) const {
  if (!location) return -1;
  const auto name = require_object(location->state_, detail::WebGlObjectKind::uniform_location);
  if (name > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
    throw std::range_error("WebGL uniform location exceeds the native GL range");
  }
  return static_cast<int>(name);
}

GlCanvas GlCanvas::create(int width, int height, double pixel_ratio, String title, bool hidden) {
  if (width <= 0 || height <= 0) throw std::invalid_argument("SDL GL canvas dimensions must be positive");
  if (!std::isfinite(pixel_ratio) || pixel_ratio <= 0.0) {
    throw std::invalid_argument("SDL GL canvas pixel ratio must be positive and finite");
  }
  const auto scaled_width = static_cast<double>(width) * pixel_ratio;
  const auto scaled_height = static_cast<double>(height) * pixel_ratio;
  if (scaled_width > static_cast<double>(std::numeric_limits<int>::max()) ||
      scaled_height > static_cast<double>(std::numeric_limits<int>::max())) {
    throw std::range_error("SDL GL canvas pixel dimensions exceed the SDL window range");
  }

  WindowOptions options;
  options.title = title.to_utf8();
  options.width = static_cast<int>(std::lround(scaled_width));
  options.height = static_cast<int>(std::lround(scaled_height));
  options.hidden = hidden;
  options.graphics_api = GraphicsApi::open_gl;
  options.open_gl.major_version = 3;
  options.open_gl.minor_version = 0;
  options.open_gl.profile = OpenGlProfile::es;
  GlCanvas result(std::make_shared<detail::GlSurfaceState>(std::move(options)));
  result.width = static_cast<double>(width);
  result.height = static_cast<double>(height);
  return result;
}

WindowSize GlCanvas::logical_size() const { return require_state().window.size(); }

WindowSize GlCanvas::pixel_size() const { return require_state().window.pixel_size(); }

SDL_Window* GlCanvas::native_window() const noexcept {
  return state_ ? state_->window.native_handle() : nullptr;
}

WebGl2Context GlCanvas::get_context() const {
  static_cast<void>(require_state());
  return WebGl2Context(state_);
}

std::optional<WebGl2Context> GlCanvas::get_context(
    const String& context_id,
    const WebGlContextAttributes&) const {
  if (context_id != String("webgl2")) return std::nullopt;
  return get_context();
}

ClientRect GlCanvas::get_bounding_client_rect() const {
  const auto size = logical_size();
  return ClientRect{
      .bottom = static_cast<double>(size.height),
      .height = static_cast<double>(size.height),
      .right = static_cast<double>(size.width),
      .width = static_cast<double>(size.width),
  };
}

void GlCanvas::add_event_listener(
    const String& type,
    std::function<void(InputPointerData)> callback) {
  if (!callback) throw std::invalid_argument("canvas event callback cannot be empty");
  require_state().pointer_listeners.emplace_back(type, std::move(callback));
}

void GlCanvas::add_event_listener(const String& type, std::function<void()> callback) {
  if (!callback) throw std::invalid_argument("canvas event callback cannot be empty");
  require_state().simple_listeners.emplace_back(type, std::move(callback));
}

void GlCanvas::emit_pointer(const String& emitted_type, InputPointerData event) const {
  const auto& state = require_state();
  for (const auto& [type, callback] : state.pointer_listeners) {
    if (type == emitted_type) callback(event);
  }
  for (const auto& [type, callback] : state.simple_listeners) {
    if (type == emitted_type) callback();
  }
}

detail::GlSurfaceState& GlCanvas::require_state() const {
  if (!state_) throw std::logic_error("SDL GL canvas is empty");
  return *state_;
}

} // namespace flight::host_sdl
