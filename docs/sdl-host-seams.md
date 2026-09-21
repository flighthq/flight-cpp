# SDL host seam survey

What Flight declares, what `host_sdl` implements, and what SDL 3.4.2 can still back. Taken against
flight `7e2fc7d` and the SDL3 headers installed on the build machine; every SDL entry point named
here was checked in `/usr/include/SDL3`, not recalled.

Flight declares **175** `Host*Capability` interfaces. `host_sdl` implements **17** of them: audio
device, clipboard text, device, element fullscreen, haptics, input (drop-file, focus, pointer-lock,
target), platform, screen (change, details, query), soft keyboard (change, info, visibility), and
window visibility.

The gap is not uniform. Most of the 158 remaining are macOS/Windows shell surfaces — dock, login
items, user-model IDs, notification lifecycles — that SDL has no business backing. A much smaller
set maps cleanly onto an SDL subsystem that is already linked and currently unused.

## Ready to implement, in the order I would take them

| # | Capability seam | SDL subsystem | Notes |
| --- | --- | --- | --- |
| 1 | `HostWgpuCapability` | `SDL_gpu.h` | Five methods. See below — this is the one that changes an earlier answer. |
| 2 | `HostFileOpenDialogCapability`, `HostFileSaveDialogCapability`, `HostDirectoryOpenDialogCapability` | `SDL_dialog.h` | `SDL_ShowOpenFileDialog`, `SDL_ShowSaveFileDialog`, `SDL_ShowOpenFolderDialog`. Callback-based and already async-shaped, which is what the Flight seam wants. |
| 3 | `HostMessageDialogCapability`, `HostPromptDialogCapability` | `SDL_messagebox.h` | `SDL_ShowMessageBox` gives button sets and return values; `SDL_ShowSimpleMessageBox` covers the degenerate case. |
| 4 | `HostPowerStatusCapability`, `HostPowerChangeCapability` | `SDL_power.h` | `SDL_GetPowerInfo` returns state, seconds and percent in one call. The smallest real win on the list. |
| 5 | `HostAppLocaleCapability` | `SDL_locale.h` | `SDL_GetPreferredLocales`. Pairs with `SDL_GetDateTimeLocalePreferences` in `SDL_time.h`. |
| 6 | `HostShellExternalCapability`, `HostProtocolLaunchCapability` | `SDL_misc.h` | `SDL_OpenURL` is the whole implementation. |
| 7 | `HostTray*` (18 seams) | `SDL_tray.h` | `SDL_CreateTray`, `SDL_CreateTrayMenu`, `SDL_CreateTraySubmenu`, entry checked/enabled state. The largest capability count for one subsystem, though several of the 18 are event lanes rather than operations. |
| 8 | `HostFileSystemCapability` | `SDL_storage.h`, `SDL_filesystem.h` | `SDL_OpenUserStorage` and the enumerate/read/write/path-info family. Sandboxed by construction, which is the right default for a host seam. |
| 9 | `HostSensorsCapability` | `SDL_sensor.h` | `SDL_GetSensors` / `SDL_OpenSensor` / `SDL_GetSensorData`. |
| 10 | `HostVideoCapability`, `HostPhotoCaptureDialogCapability` | `SDL_camera.h` | `SDL_OpenCamera`, `SDL_AcquireCameraFrame`, and a real permission state (`SDL_GetCameraPermissionState`) rather than an assumed one. |

## `HostWgpuCapability` — and a correction

I previously said implementing WGPU meant selecting Dawn or wgpu-native and taking a dependency
this repository has deliberately avoided. That was wrong on both counts, and the contract is why.

`HostWgpuCapability` is **five methods** — `create`, `acquire`, `attachSurface`, `isSupported`,
`release` — exactly parallel to `HostGlCapability`. It is the *surface and device acquisition*
seam. It is not `device.createBuffer`: those live on the `GPUDevice` object, and the modules that
call them are refused for compiler-side lowering reasons long before any of them is reached (see
the WGPU refusal table in the compiler request document — zero of 104 root refusals name a
device, queue or pass method).

SDL 3.4.2 ships `SDL_gpu.h`, a full GPU abstraction over Vulkan, Metal and D3D12, and SDL is
already `host_sdl`'s dependency. So the five methods map onto `SDL_CreateGPUDevice`,
`SDL_ClaimWindowForGPUDevice`, `SDL_AcquireGPUSwapchainTexture` and
`SDL_ReleaseWindowFromGPUDevice` with **no new dependency at all**, and the 18 object tags in
`flight/host_sdl/wgpu.hpp` already give the handle ABI to hang them on.

`SDL_gpu.h` also carries `SDL_BeginGPURenderPass`, `SDL_BindGPUComputePipeline` and the rest of the
command-submission family, so if the device/queue/pass operations are ever wanted, the same
subsystem backs those too. That is a later question and it is gated on the compiler, not on us.

## What SDL cannot back

**MIDI.** `HostMidiAccessCapability` and `HostMidiPermissionCapability` have no SDL implementation
to reach for: SDL3 has no MIDI API at all — the string does not occur anywhere in its headers.
Backing Web MIDI natively means ALSA sequencer on Linux, CoreMIDI on macOS and winmm on Windows,
which is three platform dependencies and a per-platform build matrix, in a package whose entire
current dependency is SDL. If MIDI is wanted it should be its own optional package with its own
opt-in, not something folded into `host_sdl` — and that is a decision worth taking deliberately
rather than discovering halfway through.

**The shell surfaces.** Dock, login items, recent documents, user-model IDs, notification
lifecycle, status bar, share sheets, IPC. These are OS-integration APIs with no SDL equivalent;
they belong to a platform host, not to SDL.
