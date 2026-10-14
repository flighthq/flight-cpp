# @flighthq/media -- upstream fixture coverage

`@flighthq/media` is `partial` at a627849: 5 of 7 modules emitted, 4 of 7 headers compiling, 3 failing.
`audioMixer.ts` and `videoChannel.ts` are refused (18 and 17 omitted declarations respectively), and there is
no override for either. The package is **not shippable**.

## Inventory

Five upstream fixtures, **127 cases**, enumerated with the brace-depth scanner.

| fixture | cases | reachability |
| --- | --- | --- |
| `audioChannel.test.ts` | 55 | reachable -- covered |
| `audioMixer.test.ts` | 32 | PARTLY reachable: 7 functions emit (the bus surface, covered); the mixer runtime is refused |
| `videoChannel.test.ts` | 29 | UNREACHABLE: `video_channel.hpp` does not compile |
| `mediaChannelSignals.test.ts` | 8 | reachable -- covered |
| `enableAudioMixerGuards.test.ts` | 3 | 2 reachable in principle but NOT observable here; 1 blocked |

## The two boundaries, exact

**`videoChannel` (29 cases) -- a compile failure.** `generated/include/flight/media/video_channel.hpp`
retains `play_video_resource`, whose body calls `get_element`, a module-local function the emitter refused
(`NOT GENERATED: function getElement -- source line 170`). `get_element` is defined nowhere in the emitted
tree, so three headers fail:

```
flight/media/video_channel.hpp: use of undeclared identifier 'get_element'
flight/media/_internal_index.hpp: use of undeclared identifier 'get_element'
flight/media/contract.hpp:       use of undeclared identifier 'get_element'
```
Recorded as a measured site of `docs/compiler-requests/retained-declaration-calls-omitted-callee.md`.

**`enableAudioMixerGuards` (2 of 3 cases) -- a dependency that throws.** The guard pair differs in exactly
one observable: with the guard installed, a gain change on a bus belonging to no mixer emits a warning through
`@flighthq/log`. That path cannot run on this profile: `generated/include/flight/log/log.hpp` is itself PARTIAL
(23 omissions) and its retained `log_info`/`log_error`/`log_debug` call `merge_span_fields`, which is NOT
GENERATED (the source's `_mergeSpanFields`, log.ts line 773). Calling it throws
`flight::log::merge_span_fields was not generated for this profile`. Two mutations of the guard pair survive
with no test able to catch them -- an equivalent pair under this boundary. Recorded as a sixth measured site of
the same request.

## What the re-audit found

The test's own scope note claimed audioMixer, enableAudioMixerGuards and videoChannel "cannot be covered here
at all". That is true of **one** of the three. The note's error was found by mutation, not by reading: seven
mutations of functions the note called unreachable **survived the whole suite** -- `isAudioChannelPlaying`,
`hasAudioChannelFade`, `setAudioChannelPlaybackRate`, `setAudioBusGain`, `setAudioBusPan`, and the guard pair.

A mechanical export map then put 24 exports at zero call sites; tracing the MODULE showed 8 of them are called
internally (`start_audio_channel`, `complete_audio_channel`, `emit_channel_signal`, `destroy_active_source`,
`enable_channel_signals`, `initialize_audio_bus`, `report_unmixed_bus`, `set_audio_bus_mixer_guard`) and are
therefore NOT uncovered. Without the trace the report would have been 24 gaps instead of 16, and 12 instead of
the 9 that survive scrutiny. The map was run with a positive control (38 `flight::media::` call sites).

Of the 16 zero-call-site-and-not-internal exports, 4 are inside the non-compiling `video_channel.hpp`
(unreachable), 2 are the guard pair (unobservable), and the remaining 10 are now covered.

## Mutations

Nine probes, each scoped to one function body with a one-site anchor, each confirmed to hit exactly one site
before its result was read.

| # | mutation | before | after |
| --- | --- | --- | --- |
| A1 | `isAudioChannelPlaying` always true | SURVIVED | caught |
| A2 | `hasAudioChannelFade` always false | SURVIVED | caught |
| A3 | the playback rate is doubled | SURVIVED | caught |
| M1 | the bus gain is never stored | SURVIVED | caught |
| M2 | the pan is no longer clamped | SURVIVED | caught |
| G1 | `disableAudioMixerGuards` never clears the guard | SURVIVED | still survives -- equivalent under the log boundary |
| G2 | `enableAudioMixerGuards` never installs it | SURVIVED | still survives -- equivalent under the log boundary |
| A4 | the target gain is never stored (both arms of the fade) | SURVIVED | caught |

Six of the eight mutants are now caught by the assertion written for each; the guard pair is unobservable on
this profile for the measured reason above. Every catch is a clean `FAIL:` line, not a crash. A4 was
mis-targeted on the first pass (the store appears in both arms of the fade, so a one-site anchor could not
reach it); re-run as a declared two-site mutation, it is caught.
