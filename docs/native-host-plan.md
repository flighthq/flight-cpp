# A complete native host for Flight

Flight declares **177** `Host*Capability` seams. `host_sdl` implements **17**. This is the honest
accounting of the rest: what SDL 3.4.2 can back, what it cannot, and what a complete native host
would have to bring with it. Every SDL entry point named here was checked in the installed headers.

## First, the audio question, because it sets the pattern

**We do not bundle SDL_mixer, and we should not start.** `src/host_sdl/audio.cpp` uses SDL3's core
audio directly — `SDL_OpenAudioDeviceStream`, `SDL_PutAudioStreamData`,
`SDL_ResumeAudioStreamDevice` — which gives device enumeration, stream lifetime and format
conversion. That is the whole of what SDL3 core audio offers, and it is exactly the whole of
`HostAudioDeviceCapability`, which is implemented.

It is not the whole of Flight's audio contract. Two further seams are declared and neither is
reachable from SDL:

- `HostAudioCodecCapability` — decoding mp3/ogg/aac/flac. SDL3 decodes WAV and nothing else.
- `HostAudioMixerCapability` — a mixing graph with gain, routing and spatialisation. SDL3 gives one
  stream per device and no graph at all.

This is the shape of the whole problem: **SDL is a platform abstraction, not a media stack.** It
gets you a window, a device, an input event and a GPU. Everything above that — codecs, rasterisers,
text shaping — is somebody else's library, and a host that pretends otherwise is the dishonest kind.

## What SDL 3.4.2 can back, by domain

| Domain | Seams | SDL subsystem | State |
| --- | ---: | --- | --- |
| Window | 16 | `SDL_video.h` | 1 implemented; geometry, state, focus, fullscreen, z-order, constraints all reachable |
| Tray | 15 | `SDL_tray.h` | none implemented; `SDL_CreateTray` + menu/submenu/entry state covers most |
| Input | 5 | `SDL_events.h`, `SDL_mouse.h`, `SDL_gamepad.h` | 4 implemented |
| Clipboard | 5 | `SDL_clipboard.h` | text implemented; formats and image reachable |
| Screen | 4 | `SDL_video.h` | 3 implemented |
| Dialogs | 6 | `SDL_dialog.h`, `SDL_messagebox.h` | none implemented; all six reachable |
| Power | 8 | `SDL_power.h` | none; `SDL_GetPowerInfo` covers status/change only — thermal, idle, keep-awake and session-lock are platform APIs |
| SoftKeyboard | 7 | `SDL_keyboard.h` text input | 3 implemented |
| Audio | 3 | `SDL_audio.h` | device implemented; codec and mixer are not SDL's to give |
| Surface | 2 | `SDL_video.h`, `SDL_gpu.h` | reachable |
| Sensors | 1 | `SDL_sensor.h` | reachable |
| FileSystem | 1 | `SDL_storage.h`, `SDL_filesystem.h` | reachable, sandboxed by construction |
| Video/capture | 3 | `SDL_camera.h` | reachable, with a real permission state |
| Locale | 1 | `SDL_locale.h`, `SDL_time.h` | reachable |
| Gl / Wgpu | 2 | `SDL_opengles2.h`, `SDL_gpu.h` | GL implemented; WGPU is five methods over `SDL_gpu.h` |
| Haptics, Device, Platform | 3 | `SDL_haptic.h`, `SDL_cpuinfo.h`, `SDL_platform.h` | implemented |

Roughly **75 of 177** are SDL-reachable. That is the real ceiling of a pure-SDL host, and it is
worth stating as a number rather than implying the other 102 are merely unwritten.

## What a complete host needs beyond SDL

This is where Lime is the right reference. Lime is a capable native host precisely because it does
not pretend SDL is enough: it composes SDL with OpenAL, freetype, harfbuzz, libvorbis, libpng,
libjpeg, cairo, curl and zlib. Flight's contract is wider than Lime's, so the list is longer, but
the principle is identical.

| Flight seams | Needs | Lime's answer |
| --- | --- | --- |
| `TextShaper`, `GlyphRasterizer`, `FontLoading`, `TextSegmenter` | **harfbuzz + freetype + ICU** | freetype + harfbuzz |
| `AudioCodec`, `AudioMixer` | decoders + a mixing graph | libvorbis + OpenAL |
| `BitmapEncode`, `Image` | png/jpeg/webp encode-decode | libpng + libjpeg |
| `Canvas` | a 2D rasteriser | cairo |
| `Net`, `Socket`, `Connectivity` | an HTTP/socket stack | curl |
| `Midi` (2) | ALSA seq / CoreMIDI / winmm | Lime does not do MIDI either |
| `Notification` (11), `MediaSession`, `Share`, `Shortcut`, `Geolocation`, `Permissions`, `Updater`, `Ipc` (5) | per-OS platform APIs | Lime does not do these |
| `StatusBar` (6) | iOS/Android only | n/a on desktop |
| `App` shell (dock, login items, recent documents, user-model id) | per-OS platform APIs | partial |

## How this should be structured

Not as one growing `host_sdl`. Two reasons, both already in this repository's contract.

The runtime is dependency-free and `host_sdl`'s entire dependency is SDL. Folding freetype,
harfbuzz, a codec set and an HTTP stack into it would make the optional host package a heavier
dependency than the thing it hosts, and would make "do you have a Flight host?" mean nine different
things depending on what was available at configure time.

And a capability is already a **seam passed explicitly through call boundaries** — that is how
Flight's host contract is defined, and it is what makes composition the natural unit. An
application assembles the host it needs:

```
Flight::HostSdl        SDL only, honest, no optional pieces   (window, input, audio device,
                                                               clipboard, dialogs, tray, power,
                                                               sensors, filesystem, gl, wgpu)
Flight::HostText       freetype + harfbuzz + ICU              (shaper, rasteriser, font loading,
                                                               segmenter)
Flight::HostCodec      decoders + mixing graph                (audio codec, audio mixer)
Flight::HostImage      png/jpeg/webp                          (bitmap encode, image)
Flight::HostNet        an HTTP/socket stack                   (net, socket, connectivity)
Flight::HostPlatform   per-OS, built per target               (notifications, media session,
                                                               share, shortcuts, geolocation,
                                                               permissions, app shell)
```

Each is independently buildable, each declares its own dependency, and an application that wants
none of them still gets a working SDL host. That is the same shape `Flight::HostSdlGl`,
`Flight::HostSdlWgpu` and `Flight::HostSdlVulkan` already have — this proposal is the existing
pattern continued, not a new one.

The name matters less than the boundary. If the composed thing wants a name of its own —
`Flight::HostNative`, a Lime-equivalent that pulls the set in — that should be a thin aggregate
target over the packages above, so that it is a convenience and never a place where a dependency
can hide.

## Recommended order

1. **`HostWgpuCapability`** over `SDL_gpu.h` — five methods, no new dependency, already asked for.
2. **Dialogs** (6 seams) — `SDL_dialog.h` + `SDL_messagebox.h`, no new dependency.
3. **Tray** (15 seams) — `SDL_tray.h`, the largest seam count for one subsystem.
4. **Window** (15 remaining) — `SDL_video.h`, the widest-used domain in emitted code.
5. **Power, locale, sensors, filesystem, camera** — small, self-contained, no new dependency.

Items 1-5 are all SDL-only and would take `host_sdl` from 17 to roughly 75 seams without adding a
single dependency. Everything past that is a dependency decision that should be taken one package
at a time, deliberately, with the dependency named in the package that carries it.
