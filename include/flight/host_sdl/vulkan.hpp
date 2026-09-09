#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>
#include <SDL3/SDL_vulkan.h>

#include <flight/host_sdl/export.hpp>
#include <flight/host_sdl/window.hpp>

namespace flight::host_sdl {

FLIGHT_HOST_SDL_VULKAN_API std::vector<std::string> required_vulkan_instance_extensions(
    const Window& window);

class FLIGHT_HOST_SDL_VULKAN_API VulkanSurface final {
 public:
  VulkanSurface(
      const Window& window,
      VkInstance instance,
      const VkAllocationCallbacks* allocator = nullptr);
  ~VulkanSurface() noexcept;

  VulkanSurface(const VulkanSurface&) = delete;
  VulkanSurface& operator=(const VulkanSurface&) = delete;
  VulkanSurface(VulkanSurface&& other) noexcept;
  VulkanSurface& operator=(VulkanSurface&& other) noexcept;

  [[nodiscard]] VkSurfaceKHR native_handle() const noexcept;
  [[nodiscard]] VkSurfaceKHR release() noexcept;
  [[nodiscard]] bool supports_presentation(
      VkPhysicalDevice physical_device,
      std::uint32_t queue_family_index) const noexcept;

 private:
  void reset() noexcept;

  VkInstance instance_{VK_NULL_HANDLE};
  VkSurfaceKHR surface_{VK_NULL_HANDLE};
  const VkAllocationCallbacks* allocator_{nullptr};
};

} // namespace flight::host_sdl
