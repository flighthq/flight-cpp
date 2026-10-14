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
| `enableAudioMixerGuards.test.ts` | 3 | 2 NOT observable before the Log override; both **observable and covered** since it landed; 1 blocked |

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

**`enableAudioMixerGuards` (2 of 3 cases) -- a dependency that USED to throw.** The guard pair differs in exactly
one observable: with the guard installed, a gain change on a bus belonging to no mixer emits a warning through
`@flighthq/log`. When this section was written that path could not run on this profile: the generated
`flight/log/log.hpp` was PARTIAL (23 omissions) and its retained `log_info`/`log_error`/`log_debug` called
`merge_span_fields`, which was NOT GENERATED (the source's `_mergeSpanFields`, log.ts line 773), so calling it
threw `flight::log::merge_span_fields was not generated for this profile`. Two mutations of the guard pair
survived with no test able to catch them -- an equivalent pair **under that boundary**. Recorded as a sixth
measured site of the same request.

**That boundary is gone.** The `@flighthq/log` override supplied `merge_span_fields` and the rest of the
non-console surface, and both Log suites pass, so the guard warning now reaches a sink and can be observed.
The guard pair is therefore no longer an equivalence; see **Post-Log re-audit** below, where both guard
mutants fail by check.

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
| G1 | `disableAudioMixerGuards` never clears the guard | SURVIVED | caught, once the Log override landed -- see below |
| G2 | `enableAudioMixerGuards` never installs it | SURVIVED | caught, once the Log override landed -- see below |
| A4 | the target gain is never stored (both arms of the fade) | SURVIVED | caught |

Eight of the eight mutants are now caught by the assertion written for each -- the guard pair's row was
"survives, equivalent" only while the Log boundary denied them an observable, and the Post-Log re-audit below
re-runs both to a clean catch. Every catch is a clean `FAIL:` line, not a crash. A4 was
mis-targeted on the first pass (the store appears in both arms of the fade, so a one-site anchor could not
reach it); re-run as a declared two-site mutation, it is caught.

## Post-Log re-audit (2026-10-14)

This section was written after the `@flighthq/log` override landed, because the earlier audit's two
"unobservable" findings rested on a boundary that no longer holds. **It is an addition to an existing
test, not a new test**, and it consumes the Log work rather than overlapping it.

### Dependency -- read this before applying the diff

**The Media work in this arc depends on Integration applying Log `763ee241` (the promoted override and the
restored TextBidi/Media log assertions) followed by `ad082c7a` (the Log audit repair), in that order, BEFORE
the Media tip.** It is not independently applicable against pre-Log state: the restored guard-pair block
reads the guard warning through a `flight::types::LogSink` recording sink, and against the pre-Log tree that
call throws `merge_span_fields was not generated for this profile` instead of reaching the sink. Applying the
Media tip alone would not fail loudly -- it would fail the guard assertions, which is the same shape as a real
regression. No commit in this arc edits a file that `763ee241` or `ad082c7a` touches; it only consumes them.

### The two motivating guard mutants, re-run first

Both were re-run before any other work, against the restored block, and both fail by **check** (a clean `FAIL:`
line), not by crash:

| # | mutation | result |
| --- | --- | --- |
| G2 | `enableAudioMixerGuards` never installs the guard | caught |
| G1 | `disableAudioMixerGuards` never clears the guard | caught |

G2 emits `FAIL: enabling the guards makes the next unmixed gain change warn` and `FAIL: and the warning names
the operation that would have fixed it`; G1 emits `FAIL: disabling the guards returns the silent no-op to
silence`. The pair therefore moved from **equivalent under the log boundary** to **observable and covered**,
which is the whole reason this arc exists.

### The 127-case map, re-verified

The five fixture counts were re-derived with the brace-depth scanner and match the Inventory table above
exactly: audioChannel 55, audioMixer 32, enableAudioMixerGuards 3, mediaChannelSignals 8, videoChannel 29.
No case moved and no count drifted; the earlier map stands.

### The battery, and three measured holes

Six mutations were diff-confirmed (each shown to hit exactly one site before its result was read), targeting
the provider path, the guards, state, and absence:

| # | mutation | before | after |
| --- | --- | --- | --- |
| S1 | the channel stores the wrong gain | caught | caught |
| D1 | a no-buffer source still reports playing | caught-by-crash | not re-run |
| D2 | channel signals are reported before enabling | caught-by-crash | not re-run |
| **P1** | `setAudioChannelGain` never tells the host | **SURVIVED** | **caught** |
| **S2** | a MUTED channel still reaches the host | **SURVIVED** | **caught** |
| **S3** | an INACTIVE source still reaches the host | **SURVIVED** | **caught** |

P1, S2 and S3 all sit in the same place: `set_audio_channel_gain`'s provider call, guarded by
`runtime !== undefined && runtime.sourceHandle !== INVALID_SOURCE && !channel.muted`. The suite asserted the
gain's **stored** value and nothing about whether it reached the host, so deleting the call, or the mute
guard, or the invalid-source guard, changed no observable. D1 and D2 were already caught, but only by crash
(a segfault on the wrong input is a real detector yet a poor one -- it proves the code is wrong, not that the
suite understands *why*), which is why they are listed separately rather than folded into "caught".

### The fix

`MockDevice` gained a `gain_calls` vector (source handle, value) and a recording `set_source_gain`. Three
assertions were added, all additions-only:

- the gain reaches the host, with the requested value, against **the channel's own source handle** (P1);
- a **MUTED** channel stores the gain without reaching the host, and still stores it (S2);
- an **INACTIVE** source (`sourceHandle == INVALID_SOURCE`) stores the gain without reaching the host (S3).

All three now fail by check. Re-running P1, S2 and S3 against the committed tree gives **CAUGHT-BY-CHECK** for
each.

Two traps the additions had to route around, recorded because a later reader will meet them. `play` sets the
initial gain, so `gain_calls` is 2 immediately after start, not 0 -- the counters are cleared at the points
the assertions need. And `set_audio_channel_muted` itself pushes the mute through as a gain of 0, so the
muted-channel case must clear **after** muting, not before; clearing before leaves the mute's own call in the
buffer and the assertion fails for the wrong reason. The INACTIVE-source case uses a **separate** channel so
that the surrounding playing-state assertions are untouched -- sharing one channel made an earlier
`pause_audio_channel` break the later "muting does not pause the channel" case.

### Package state, unchanged

`@flighthq/media` remains `partial` at `a627849`: 5 of 7 modules emitted, 7 of 7 emitted headers compiled,
3 failing (`video_channel.hpp` and the two headers that include it), 0 unattempted. Still **not shippable**;
this arc closed coverage holes in a package that is not shippable, and did not move the tier.
