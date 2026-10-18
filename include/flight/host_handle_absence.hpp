#pragma once

namespace flight {

// An opaque host handle that a host profile declares can stand for a SOURCE-ABSENT value at a read or
// return boundary. This exists for one source shape: a checked member read that finds no member yields
// JavaScript `undefined`, while the declared return slot is a non-optional external handle
// (`GPUTextureView`). JavaScript fails later, when the value is used; the native result has to fail at
// the same place rather than at the read.
//
// A handle type qualifies only by an explicit specialization in its host profile header, and only when
// its default state is genuinely "no handle": no allocation, no provider call, no fabricated object, and
// a false boolean conversion, so every later use is the host operation's own failure. Nothing qualifies
// by default -- not an arbitrary reference, value class, or `flight::Any`.
template <typename Handle>
struct host_handle_absence final {
  static constexpr bool enabled = false;
};

template <typename Handle>
[[nodiscard]] Handle absent_host_handle() noexcept {
  static_assert(host_handle_absence<Handle>::enabled,
                "this type is not a declared opaque host-handle absence carrier; a missing member of it "
                "has no native representation");
  return Handle{};
}

} // namespace flight
