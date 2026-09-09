#include <flight/host_sdl/vulkan.hpp>

#include "detail.hpp"

#include <stdexcept>
#include <utility>

namespace flight::host_sdl {

std::vector<std::string> required_vulkan_instance_extensions(const Window& window) {
  if (window.graphics_api() != GraphicsApi::vulkan) {
    throw std::invalid_argument("Vulkan extensions require a Vulkan window");
  }
  Uint32 count = 0;
  const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&count);
  if (extensions == nullptr) detail::throw_sdl_error("SDL_Vulkan_GetInstanceExtensions");

  std::vector<std::string> result;
  result.reserve(count);
  for (Uint32 index = 0; index < count; ++index) result.emplace_back(extensions[index]);
  return result;
}

VulkanSurface::VulkanSurface(
    const Window& window,
    VkInstance instance,
    const VkAllocationCallbacks* allocator)
    : instance_(instance), allocator_(allocator) {
  if (window.graphics_api() != GraphicsApi::vulkan) {
    throw std::invalid_argument("Vulkan surface requires a Vulkan window");
  }
  if (instance_ == VK_NULL_HANDLE) throw std::invalid_argument("Vulkan instance cannot be null");
  if (!SDL_Vulkan_CreateSurface(window.native_handle(), instance_, allocator_, &surface_)) {
    instance_ = VK_NULL_HANDLE;
    allocator_ = nullptr;
    detail::throw_sdl_error("SDL_Vulkan_CreateSurface");
  }
}

VulkanSurface::~VulkanSurface() noexcept { reset(); }

VulkanSurface::VulkanSurface(VulkanSurface&& other) noexcept
    : instance_(std::exchange(other.instance_, VK_NULL_HANDLE)),
      surface_(std::exchange(other.surface_, VK_NULL_HANDLE)),
      allocator_(std::exchange(other.allocator_, nullptr)) {}

VulkanSurface& VulkanSurface::operator=(VulkanSurface&& other) noexcept {
  if (this == &other) return *this;
  reset();
  instance_ = std::exchange(other.instance_, VK_NULL_HANDLE);
  surface_ = std::exchange(other.surface_, VK_NULL_HANDLE);
  allocator_ = std::exchange(other.allocator_, nullptr);
  return *this;
}

VkSurfaceKHR VulkanSurface::native_handle() const noexcept { return surface_; }

VkSurfaceKHR VulkanSurface::release() noexcept {
  instance_ = VK_NULL_HANDLE;
  allocator_ = nullptr;
  return std::exchange(surface_, VK_NULL_HANDLE);
}

bool VulkanSurface::supports_presentation(
    VkPhysicalDevice physical_device,
    std::uint32_t queue_family_index) const noexcept {
  if (instance_ == VK_NULL_HANDLE || physical_device == VK_NULL_HANDLE) return false;
  return SDL_Vulkan_GetPresentationSupport(instance_, physical_device, queue_family_index);
}

void VulkanSurface::reset() noexcept {
  if (surface_ != VK_NULL_HANDLE) SDL_Vulkan_DestroySurface(instance_, surface_, allocator_);
  instance_ = VK_NULL_HANDLE;
  surface_ = VK_NULL_HANDLE;
  allocator_ = nullptr;
}

} // namespace flight::host_sdl
