#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include <flight/version.hpp>

namespace flight {

enum class RuntimeCapabilityStatus : std::uint8_t {
  initial,
  planned,
  unavailable,
};

struct RuntimeCapability {
  std::string_view name;
  RuntimeCapabilityStatus status;
};

struct RuntimeContract {
  std::string_view compiler_contract;
  std::uint32_t cpp_abi;
  std::string_view task_contract;
};

inline constexpr RuntimeContract runtime_contract{
    .compiler_contract = "flight-runtime-contract/2",
    .cpp_abi = abi_version,
    .task_contract = "flight-runtime-task-capability-abi/1",
};

inline constexpr std::array runtime_capabilities{
    RuntimeCapability{"abort-signal", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"array", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"array-buffer", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"array-buffer-like", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"array-buffer-view", RuntimeCapabilityStatus::initial},
    // Symbol-keyed properties attached to an object by identity, shared by every projection of it.
    RuntimeCapability{"attached-properties", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"async-iterable", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"base64", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"blob", RuntimeCapabilityStatus::initial},
    // The Canvas 2D drawing-state stack, transform, and path construction. Rasterization is a host
    // provider's, so this deliberately does not claim a Canvas 2D implementation.
    RuntimeCapability{"canvas-2d-state", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"data-view", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"date", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"executor", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"error", RuntimeCapabilityStatus::initial},
    // The erased dynamic value for the `any` and `unknown` type positions the SDK leaves
    // deliberately unconstrained.
    RuntimeCapability{"erased-value", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"float32-array", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"float64-array", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"int8-array", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"int16-array", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"int32-array", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"internationalization", RuntimeCapabilityStatus::planned},
    RuntimeCapability{"json", RuntimeCapabilityStatus::planned},
    RuntimeCapability{"map", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"number-parsing", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"object", RuntimeCapabilityStatus::planned},
    RuntimeCapability{"regexp", RuntimeCapabilityStatus::planned},
    RuntimeCapability{"readable-stream", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"record", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"set", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"sequence-view", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"string", RuntimeCapabilityStatus::initial},
    // Deep copy over the runtime's own value domain, with a customization point for host types.
    RuntimeCapability{"structured-clone", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"symbol", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"task-coroutine", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"typed-array", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"text-decoder", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"text-encoder", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"uint8-array", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"uint8-clamped-array", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"uint16-array", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"uint32-array", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"uri-component", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"url", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"weak-map", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"weak-set", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"writable-stream", RuntimeCapabilityStatus::initial},
    RuntimeCapability{"unicode-case-service", RuntimeCapabilityStatus::planned},
    RuntimeCapability{"task-promise-operations", RuntimeCapabilityStatus::initial},
    // The separate fulfilled and rejected arms of a settled task result.
    RuntimeCapability{"task-settlement-arms", RuntimeCapabilityStatus::initial},
};

constexpr RuntimeCapabilityStatus runtime_capability_status(std::string_view name) noexcept {
  for (const auto& capability : runtime_capabilities) {
    if (capability.name == name) return capability.status;
  }
  return RuntimeCapabilityStatus::unavailable;
}

} // namespace flight
