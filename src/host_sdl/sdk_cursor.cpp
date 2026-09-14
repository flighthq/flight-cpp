#include <flight/host_sdl/sdk_cursor.hpp>

#include <flight/host_sdl/cursor.hpp>
#include <flight/types/cursor.hpp>

#include <stdexcept>
#include <utility>

namespace flight::host_sdl {

struct SdkCursorBackend::State final {
  SdlCursorBackend native;
};

SdkCursorBackend::SdkCursorBackend() : state_(std::make_shared<State>()) {}

SdkCursorBackend::~SdkCursorBackend() noexcept = default;

SdkCursorBackend::SdkCursorBackend(const SdkCursorBackend&) noexcept = default;

SdkCursorBackend& SdkCursorBackend::operator=(const SdkCursorBackend&) noexcept = default;

SdkCursorBackend::SdkCursorBackend(SdkCursorBackend&&) noexcept = default;

SdkCursorBackend& SdkCursorBackend::operator=(SdkCursorBackend&&) noexcept = default;

flight::types::CursorBackend SdkCursorBackend::backend() const {
  if (state_ == nullptr) throw std::logic_error("moved-from SDL SDK cursor backend");
  flight::types::CursorBackend result;
  const auto state = state_;
  result.set_cursor = [state](std::optional<String> cursor) {
    state->native.set_cursor(std::move(cursor));
  };
  return result;
}

std::optional<String> SdkCursorBackend::current_cursor() const {
  return state_ == nullptr ? std::nullopt : state_->native.current_cursor();
}

} // namespace flight::host_sdl
