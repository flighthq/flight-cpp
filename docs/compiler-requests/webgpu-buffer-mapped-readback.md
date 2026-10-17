# A bound WebGPU buffer needs real mapped readback and its lifetime

At Flight `a627849` / compiler `806d257`, `GPUBuffer` maps to shared native `WgpuBuffer`, an alias
of `WgpuObject<WgpuBufferTag>`. Retained screen readback constructs Uint8Array from
`buffer.value().get_mapped_range()`, but that member is absent on the native carrier.

## Measured boundary

Stock `flight/render_wgpu/wgpu_screen_capture.hpp` is1 attempted/0 pass/1 fail/0 unattempted,
Clang22.1.8 jobs1,12s, missing acquire at122. A scratch actual unused header supplies only the exact
two callback-value declarations and Task<void> map prototype through existing guarded mechanisms.
It then fails compilation at candidate175/raw169: no member get_mapped_range in WgpuBufferTag.
No positive unused-header link/run or qualified caller negative object/link is reached; the candidate
is rejected. No repair, source/binding/runtime body or normal generation is carried.

## Required provider contract

Expose the actual mapped range through a typed byte view retaining the same buffer/map lifetime,
with provider-backed asynchronous map, state and unmap semantics. Do not synthesize pixels, return
an empty range, make an immediate successful Task or leave a dangling pointer after unmap. Current
map_async/map-state/encoder-copy gaps are inspection only; get_mapped_range is the measured first error,
and the existing native unmap method explicitly reports unavailable.

One retained capture buffer is shared by frame copy and reader. Pending or mapped state excludes
frame writes; unknown state deliberately allows the copy. Encode on the frame's existing encoder,
not a later task. Preserve original texture/buffer/target/device, logical AA-adjusted extent,
256-byte row stride, lazy resize and resource destroy/reallocate order. Readback snapshots that same
buffer and dimensions before mapping, removes row padding and swizzles BGRA only for bgra8unorm and
bgra8unorm-srgb; stored premultiplied RGB/alpha remain unchanged. Normal completion unmaps, not destroys.

Preserve absent-buffer error,10000ms default, <=0 indefinite map, exact timeout/rejection and shared
timer cleanup. Timeout may leave a later map live and is not cancellation or a retry guarantee.
Source lacks a readback-wide finally; no stronger cleanup is supplied. See the independent
[texture presence proofs](guarded-external-optional-member-view.md#g2-wgpu-capture-guards-the-exact-retained-texture),
[timer cell](captured-referent-mutation.md#g2-wgpu-capture-timer-initial-storage) and
[declaration qualification](retained-declaration-calls-omitted-callee.md#g2-wgpu-capture-three-exact-declarations-still-fail-native-readback).
No GPU provider/capture/readback, copied owner, narrowed format/domain or pixel/timeout proof is claimed.
