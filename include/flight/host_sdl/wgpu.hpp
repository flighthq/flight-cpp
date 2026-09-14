#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>

#include <SDL3/SDL_video.h>

#include <flight/host_sdl/export.hpp>
#include <flight/host_sdl/window.hpp>
#include <flight/array.hpp>
#include <flight/record.hpp>
#include <flight/set.hpp>
#include <flight/string.hpp>

namespace flight::host_sdl {

using WgpuInstanceHandle = void*;
using WgpuSurfaceHandle = void*;

using WgpuObjectRelease = void (*)(void* userdata, void* object) noexcept;

struct WgpuObjectCallbacks final {
  void* userdata{nullptr};
  WgpuObjectRelease release{nullptr};
};

namespace detail {

struct WgpuObjectState final {
  WgpuObjectState(void* native_object, WgpuObjectCallbacks native_callbacks)
      : object(native_object), callbacks(native_callbacks) {}

  ~WgpuObjectState() noexcept {
    if (object != nullptr && callbacks.release != nullptr) {
      callbacks.release(callbacks.userdata, object);
    }
  }

  WgpuObjectState(const WgpuObjectState&) = delete;
  WgpuObjectState& operator=(const WgpuObjectState&) = delete;

  void* object;
  WgpuObjectCallbacks callbacks;
};

} // namespace detail

// Shared, typed ownership for reference-counted handles supplied by Dawn, wgpu-native, or another
// WebGPU implementation. The adapter that adopts a handle supplies its matching release function;
// copying preserves WebGPU object identity and invokes that release exactly once.
template <typename Tag>
class WgpuObject final {
 public:
  using weak_type = std::weak_ptr<detail::WgpuObjectState>;

  WgpuObject() noexcept = default;

  [[nodiscard]] static WgpuObject adopt(void* object, WgpuObjectCallbacks callbacks) {
    if (object == nullptr) throw std::invalid_argument("WebGPU object cannot be null");
    if (callbacks.release == nullptr) {
      throw std::invalid_argument("WebGPU object requires a release callback");
    }
    return WgpuObject(std::make_shared<detail::WgpuObjectState>(object, callbacks));
  }

  [[nodiscard]] explicit operator bool() const noexcept { return state_ != nullptr; }
  [[nodiscard]] friend bool operator==(const WgpuObject&, const WgpuObject&) noexcept = default;
  [[nodiscard]] const void* identity() const noexcept { return state_.get(); }
  [[nodiscard]] void* native_handle() const noexcept { return state_ ? state_->object : nullptr; }
  [[nodiscard]] weak_type weaken() const noexcept { return state_; }

  [[nodiscard]] static std::optional<WgpuObject> lock_weak(const weak_type& weak) noexcept {
    auto state = weak.lock();
    if (!state) return std::nullopt;
    return WgpuObject(std::move(state));
  }

 private:
  explicit WgpuObject(std::shared_ptr<detail::WgpuObjectState> state) noexcept
      : state_(std::move(state)) {}

  std::shared_ptr<detail::WgpuObjectState> state_;
};

template <typename Tag>
struct WgpuObjectWeakPolicy final {
  using key_type = WgpuObject<Tag>;
  using weak_type = typename key_type::weak_type;
  using identity_type = const void*;

  [[nodiscard]] static weak_type weaken(const key_type& key) noexcept { return key.weaken(); }
  [[nodiscard]] static std::optional<key_type> lock(const weak_type& key) noexcept {
    return key_type::lock_weak(key);
  }
  [[nodiscard]] static identity_type identity(const key_type& key) noexcept {
    return key.identity();
  }
  [[nodiscard]] static std::size_t hash(identity_type identity) noexcept {
    return std::hash<const void*>{}(identity);
  }
  [[nodiscard]] static bool equal(identity_type left, identity_type right) noexcept {
    return left == right;
  }
};

struct WgpuApiTag;
struct WgpuAdapterTag;
struct WgpuBindGroupTag;
struct WgpuBindGroupLayoutTag;
struct WgpuBufferTag;
struct WgpuCanvasContextTag;
struct WgpuCommandBufferTag;
struct WgpuCommandEncoderTag;
struct WgpuDeviceTag;
struct WgpuExternalImageSourceTag;
struct WgpuPipelineLayoutTag;
struct WgpuQueueTag;
struct WgpuRenderPassEncoderTag;
struct WgpuRenderPipelineTag;
struct WgpuSamplerTag;
struct WgpuShaderModuleTag;
struct WgpuTextureTag;
struct WgpuTextureViewTag;

using WgpuApi = WgpuObject<WgpuApiTag>;
using WgpuBindGroup = WgpuObject<WgpuBindGroupTag>;
using WgpuBindGroupLayout = WgpuObject<WgpuBindGroupLayoutTag>;
using WgpuBuffer = WgpuObject<WgpuBufferTag>;
using WgpuCanvasContext = WgpuObject<WgpuCanvasContextTag>;
using WgpuCommandBuffer = WgpuObject<WgpuCommandBufferTag>;
using WgpuCommandEncoder = WgpuObject<WgpuCommandEncoderTag>;
using WgpuDevice = WgpuObject<WgpuDeviceTag>;
using WgpuExternalImageSource = WgpuObject<WgpuExternalImageSourceTag>;
using WgpuPipelineLayout = WgpuObject<WgpuPipelineLayoutTag>;
using WgpuQueue = WgpuObject<WgpuQueueTag>;
using WgpuRenderPassEncoder = WgpuObject<WgpuRenderPassEncoderTag>;
using WgpuRenderPipeline = WgpuObject<WgpuRenderPipelineTag>;
using WgpuSampler = WgpuObject<WgpuSamplerTag>;
using WgpuShaderModule = WgpuObject<WgpuShaderModuleTag>;
using WgpuTexture = WgpuObject<WgpuTextureTag>;
using WgpuTextureView = WgpuObject<WgpuTextureViewTag>;

struct WgpuSupportedLimits final {
  std::optional<double> max_texture_dimension2_d;
};

class WgpuAdapter final {
 public:
  Set<String> features;
  WgpuSupportedLimits limits;

  WgpuAdapter() noexcept = default;

  [[nodiscard]] static WgpuAdapter adopt(
      void* object,
      WgpuObjectCallbacks callbacks,
      Set<String> features = {},
      WgpuSupportedLimits limits = {}) {
    return WgpuAdapter(
        WgpuObject<WgpuAdapterTag>::adopt(object, callbacks),
        std::move(features),
        std::move(limits));
  }

  [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(object_); }
  [[nodiscard]] friend bool operator==(const WgpuAdapter& left, const WgpuAdapter& right) noexcept {
    return left.object_ == right.object_;
  }
  [[nodiscard]] const void* identity() const noexcept { return object_.identity(); }
  [[nodiscard]] void* native_handle() const noexcept { return object_.native_handle(); }

 private:
  WgpuAdapter(
      WgpuObject<WgpuAdapterTag> object,
      Set<String> adapter_features,
      WgpuSupportedLimits adapter_limits)
      : features(std::move(adapter_features)),
        limits(std::move(adapter_limits)),
        object_(std::move(object)) {}

  WgpuObject<WgpuAdapterTag> object_;
};

using WgpuApiWeakPolicy = WgpuObjectWeakPolicy<WgpuApiTag>;
using WgpuBindGroupWeakPolicy = WgpuObjectWeakPolicy<WgpuBindGroupTag>;
using WgpuBindGroupLayoutWeakPolicy = WgpuObjectWeakPolicy<WgpuBindGroupLayoutTag>;
using WgpuBufferWeakPolicy = WgpuObjectWeakPolicy<WgpuBufferTag>;
using WgpuCanvasContextWeakPolicy = WgpuObjectWeakPolicy<WgpuCanvasContextTag>;
using WgpuCommandBufferWeakPolicy = WgpuObjectWeakPolicy<WgpuCommandBufferTag>;
using WgpuCommandEncoderWeakPolicy = WgpuObjectWeakPolicy<WgpuCommandEncoderTag>;
using WgpuDeviceWeakPolicy = WgpuObjectWeakPolicy<WgpuDeviceTag>;
using WgpuExternalImageSourceWeakPolicy = WgpuObjectWeakPolicy<WgpuExternalImageSourceTag>;
using WgpuPipelineLayoutWeakPolicy = WgpuObjectWeakPolicy<WgpuPipelineLayoutTag>;
using WgpuQueueWeakPolicy = WgpuObjectWeakPolicy<WgpuQueueTag>;
using WgpuRenderPassEncoderWeakPolicy = WgpuObjectWeakPolicy<WgpuRenderPassEncoderTag>;
using WgpuRenderPipelineWeakPolicy = WgpuObjectWeakPolicy<WgpuRenderPipelineTag>;
using WgpuSamplerWeakPolicy = WgpuObjectWeakPolicy<WgpuSamplerTag>;
using WgpuShaderModuleWeakPolicy = WgpuObjectWeakPolicy<WgpuShaderModuleTag>;
using WgpuTextureWeakPolicy = WgpuObjectWeakPolicy<WgpuTextureTag>;
using WgpuTextureViewWeakPolicy = WgpuObjectWeakPolicy<WgpuTextureViewTag>;

struct WgpuColor final {
  double a{0.0};
  double b{0.0};
  double g{0.0};
  double r{0.0};
};

// Provider-neutral WebGPU pipeline values. Optional members preserve dictionary presence so the
// selected Dawn or wgpu-native adapter can apply the WebGPU defaults at its ABI boundary.
struct WgpuBlendComponent final {
  std::optional<String> src_factor;
  std::optional<String> dst_factor;
  std::optional<String> operation;
};

struct WgpuBlendState final {
  WgpuBlendComponent color;
  WgpuBlendComponent alpha;
};

struct WgpuStencilFaceState final {
  std::optional<String> compare;
  std::optional<String> pass_op;
  std::optional<String> fail_op;
  std::optional<String> depth_fail_op;
};

struct WgpuSamplerDescriptor final {
  std::optional<String> address_mode_u;
  std::optional<String> address_mode_v;
  std::optional<String> address_mode_w;
  std::optional<String> mag_filter;
  std::optional<String> min_filter;
  std::optional<String> mipmap_filter;
  std::optional<double> lod_min_clamp;
  std::optional<double> lod_max_clamp;
  std::optional<String> compare;
  std::optional<double> max_anisotropy;
  std::optional<String> label;
};

struct WgpuDeviceLostInfo final {
  String message;
  String reason;
};

struct WgpuDeviceDescriptor final {
  std::optional<Array<String>> required_features;
  std::optional<Record<String, double>> required_limits;
};

struct WgpuOrigin3DDictionary final {
  double x{0.0};
  double y{0.0};
  double z{0.0};
};

using WgpuOrigin3D = std::variant<WgpuOrigin3DDictionary, Array<double>>;

struct WgpuOrigin2DDictionary final {
  double x{0.0};
  double y{0.0};
};

using WgpuOrigin2D = std::variant<WgpuOrigin2DDictionary, Array<double>>;

struct WgpuExternalImageSourceInfo final {
  WgpuExternalImageSource source;
  WgpuOrigin2D origin;
  bool flip_y{false};
};

struct WgpuExternalImageDestinationInfo final {
  WgpuTexture texture;
  WgpuOrigin3D origin;
  double mip_level{0.0};
  String aspect{"all"};
  String color_space{"srgb"};
  bool premultiplied_alpha{false};
};

struct WgpuVertexAttribute final {
  String format;
  double offset{0.0};
  double shader_location{0.0};
};

struct WgpuVertexBufferLayout final {
  Array<WgpuVertexAttribute> attributes;
  double array_stride{0.0};
  String step_mode{"vertex"};
};

inline constexpr double wgpu_buffer_usage_map_read = 0x0001;
inline constexpr double wgpu_buffer_usage_map_write = 0x0002;
inline constexpr double wgpu_buffer_usage_copy_src = 0x0004;
inline constexpr double wgpu_buffer_usage_copy_dst = 0x0008;
inline constexpr double wgpu_buffer_usage_index = 0x0010;
inline constexpr double wgpu_buffer_usage_vertex = 0x0020;
inline constexpr double wgpu_buffer_usage_uniform = 0x0040;
inline constexpr double wgpu_buffer_usage_storage = 0x0080;
inline constexpr double wgpu_buffer_usage_indirect = 0x0100;
inline constexpr double wgpu_buffer_usage_query_resolve = 0x0200;

inline constexpr double wgpu_texture_usage_copy_src = 0x01;
inline constexpr double wgpu_texture_usage_copy_dst = 0x02;
inline constexpr double wgpu_texture_usage_texture_binding = 0x04;
inline constexpr double wgpu_texture_usage_storage_binding = 0x08;
inline constexpr double wgpu_texture_usage_render_attachment = 0x10;

inline constexpr double wgpu_shader_stage_vertex = 0x1;
inline constexpr double wgpu_shader_stage_fragment = 0x2;
inline constexpr double wgpu_shader_stage_compute = 0x4;

inline constexpr double wgpu_color_write_red = 0x1;
inline constexpr double wgpu_color_write_green = 0x2;
inline constexpr double wgpu_color_write_blue = 0x4;
inline constexpr double wgpu_color_write_alpha = 0x8;
inline constexpr double wgpu_color_write_all = 0xf;

inline constexpr double wgpu_map_mode_read = 0x1;
inline constexpr double wgpu_map_mode_write = 0x2;

struct WgpuSurfaceCallbacks {
  void* userdata{nullptr};
  WgpuSurfaceHandle (*create)(void* userdata, WgpuInstanceHandle instance, SDL_Window* window){nullptr};
  void (*destroy)(void* userdata, WgpuInstanceHandle instance, WgpuSurfaceHandle surface) noexcept{nullptr};
};

class FLIGHT_HOST_SDL_WGPU_API WgpuSurface final {
 public:
  WgpuSurface(const Window& window, WgpuInstanceHandle instance, WgpuSurfaceCallbacks callbacks);
  ~WgpuSurface() noexcept;

  WgpuSurface(const WgpuSurface&) = delete;
  WgpuSurface& operator=(const WgpuSurface&) = delete;
  WgpuSurface(WgpuSurface&& other) noexcept;
  WgpuSurface& operator=(WgpuSurface&& other) noexcept;

  [[nodiscard]] WgpuInstanceHandle instance() const noexcept;
  [[nodiscard]] WgpuSurfaceHandle native_handle() const noexcept;
  [[nodiscard]] WgpuSurfaceHandle release() noexcept;

 private:
  void reset() noexcept;

  WgpuInstanceHandle instance_{nullptr};
  WgpuSurfaceHandle surface_{nullptr};
  WgpuSurfaceCallbacks callbacks_{};
};

} // namespace flight::host_sdl
