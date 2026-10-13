# PLAT lane — upstream parity inventory

The PLAT lane is host platform integration — device and OS identity, input, audio and media, windows and menus, and the per-capability facades. This document is an inventory, not a scorecard: it
records, package by package, what the pinned upstream source asserts in its own tests and what
of that this repository covers today. It carries no judgement about which gaps matter.

## Pin and provenance

| | |
| --- | --- |
| `flight` | `a62784923f3be814463286c9fb28edfe6ab15789` |
| `flight-compiler` | `2b687eaf6b631e0fa4d7d6e18dad93b845f99633` |

Every upstream test file, case count, `describe` name, and example status below was read from
the `flight` checkout under the gitignored `.dependencies/` at the commit named above, and from
nowhere else. The *generated* figures are a different measurement: they come from the committed
`generated/manifest.json` and `generated/refusals.json`, whose tree was produced at flight
`7e2fc7d` and which a pin move does not regenerate by itself. A package whose upstream directory was
renamed at the newer pin is listed under the name the approved plan uses and read under the
name the pin ships, which at this pin is the same name.

## How the lane is defined

The lane is the approved fleet plan's package set, given verbatim. It is not a reading of the
upstream package map, which groups packages differently and would put, for instance,
`collision` and `spatial` outside an animation lane.

- `@flighthq/app`
- `@flighthq/host`
- `@flighthq/platform`
- `@flighthq/lifecycle`
- `@flighthq/input`
- `@flighthq/keyboard`
- `@flighthq/interaction`
- `@flighthq/gui`
- `@flighthq/layout`
- `@flighthq/accessibility`
- `@flighthq/audio`
- `@flighthq/media`
- `@flighthq/mediasession`
- `@flighthq/video`
- `@flighthq/webcam`
- `@flighthq/midi`
- `@flighthq/sensors`
- `@flighthq/geolocation`
- `@flighthq/haptics`
- `@flighthq/device`
- `@flighthq/screen`
- `@flighthq/power`
- `@flighthq/clipboard`
- `@flighthq/dialog`
- `@flighthq/menu`
- `@flighthq/tray`
- `@flighthq/statusbar`
- `@flighthq/notification`
- `@flighthq/share`
- `@flighthq/shortcut`
- `@flighthq/shell`
- `@flighthq/ipc`
- `@flighthq/protocol`
- `@flighthq/permissions`
- `@flighthq/filesystem`
- `@flighthq/preferences`
- `@flighthq/updater`
- `@flighthq/useragent`
- `@flighthq/connectivity`

## Method

- **Upstream tests** are the colocated `packages/<name>/src/**/*.test.ts` vitest files at the
  pin. The case count per file is the number of `it(`/`test(` blocks in it; the behaviors listed
  are that file's `describe` blocks, which upstream names after the exported function or object
  under test. A file with more than 12 blocks lists that many and names the
  remainder as a count; the file itself is one open away.
- **Existing CTest coverage** is read from `tests/CMakeLists.txt`, `examples/CMakeLists.txt`, and
  `examples/upstream/CMakeLists.txt`, and attributed by which of the package's *generated* (or
  hand-written override) headers a registered test source includes directly. A runtime header
  that merely shares a package's directory name — `flight/host/console.hpp` against
  `@flighthq/host` — is not SDK package coverage and is not counted. A test whose registered
  name also matches the package is marked *dedicated*; anything else reaches the package through
  the closure it links, and is *incidental*.
- **Web-only exclusions** are read from `flight.environment` in each package's own
  `package.json`, which is how this repository derives applicability (`deferred-packages.json`
  lists `applicableEnvironments: []`, so any declaration excludes the package). They are never
  inferred from a package name, and a web symbol in a refusal is evidence to read, not a
  classification — `@flighthq/webcam` was nearly parked for its name and is shippable, and
  `@flighthq/render-wgpu`'s `HTMLImageElement[value]` refusals are SDL image sources.

## Lane roll-up

| package | upstream modules | test files | cases | local CTest | web-only declared | generated emitted / refused |
| --- | ---: | ---: | ---: | --- | --- | --- |
| `@flighthq/app` | 6 | 5 | 201 | dedicated | none | 4 / 2 |
| `@flighthq/host` | 6 | 4 | 141 | dedicated | none | 6 / 0 |
| `@flighthq/platform` | 3 | 1 | 37 | none | none | 3 / 0 |
| `@flighthq/lifecycle` | 3 | 2 | 44 | dedicated | none | 2 / 1 |
| `@flighthq/input` | 3 | 2 | 66 | dedicated | none | 3 / 0 |
| `@flighthq/keyboard` | 3 | 1 | 23 | dedicated | none | 3 / 0 |
| `@flighthq/interaction` | 19 | 17 | 212 | dedicated | none | 8 / 11 |
| `@flighthq/gui` | 20 | 18 | 115 | none | none | 16 / 4 |
| `@flighthq/layout` | 8 | 7 | 73 | none | none | 5 / 3 |
| `@flighthq/accessibility` | 3 | 1 | 6 | dedicated | none | 3 / 0 |
| `@flighthq/audio` | 8 | 6 | 109 | none | none | 5 / 3 |
| `@flighthq/media` | 7 | 5 | 127 | none | none | 5 / 2 |
| `@flighthq/mediasession` | 3 | 1 | 19 | none | none | 3 / 0 |
| `@flighthq/video` | 5 | 3 | 79 | none | none | 4 / 1 |
| `@flighthq/webcam` | 2 | 1 | 1 | none | none | 2 / 0 |
| `@flighthq/midi` | 7 | 5 | 40 | none | none | 5 / 2 |
| `@flighthq/sensors` | 3 | 2 | 67 | none | none | 3 / 0 |
| `@flighthq/geolocation` | 4 | 2 | 13 | none | none | 4 / 0 |
| `@flighthq/haptics` | 3 | 1 | 19 | none | none | 3 / 0 |
| `@flighthq/device` | 3 | 1 | 12 | none | none | 3 / 0 |
| `@flighthq/screen` | 3 | 1 | 46 | none | none | 3 / 0 |
| `@flighthq/power` | 3 | 1 | 31 | none | none | 2 / 1 |
| `@flighthq/clipboard` | 3 | 1 | 31 | none | none | 3 / 0 |
| `@flighthq/dialog` | 5 | 3 | 23 | dedicated | none | 4 / 1 |
| `@flighthq/menu` | 4 | 2 | 58 | none | none | 3 / 1 |
| `@flighthq/tray` | 4 | 2 | 31 | none | none | 3 / 1 |
| `@flighthq/statusbar` | 3 | 1 | 18 | dedicated | none | 2 / 1 |
| `@flighthq/notification` | 3 | 1 | 38 | none | none | 2 / 1 |
| `@flighthq/share` | 3 | 2 | 14 | dedicated | none | 2 / 1 |
| `@flighthq/shortcut` | 4 | 2 | 27 | dedicated | none | 3 / 1 |
| `@flighthq/shell` | 3 | 1 | 14 | none | none | 3 / 0 |
| `@flighthq/ipc` | 3 | 1 | 12 | none | none | 3 / 0 |
| `@flighthq/protocol` | 3 | 2 | 23 | none | none | 3 / 0 |
| `@flighthq/permissions` | 3 | 5 | 31 | none | none | 3 / 0 |
| `@flighthq/filesystem` | 3 | 1 | 48 | incidental | none | 2 / 1 |
| `@flighthq/preferences` | 4 | 2 | 45 | none | none | 3 / 1 |
| `@flighthq/updater` | 3 | 1 | 6 | dedicated | none | 2 / 1 |
| `@flighthq/useragent` | 4 | 2 | 104 | dedicated | none | 3 / 1 |
| `@flighthq/connectivity` | 3 | 1 | 17 | dedicated | none | 2 / 1 |

`upstream modules` counts the `.ts` files under `packages/<name>/src`; the generated manifest
counts its own `sourceModules`, and the two differ where declaration files or generated outputs
are involved.

**39** packages, **117** upstream test files, **2021** cases.
16 of 39 packages are reached by at least one local CTest executable.
14 by a test dedicated to the package: `lifecycle`, `interaction`, `app`, `host`, `dialog`, `statusbar`, `share`, `updater`, `useragent`, `connectivity`, `input`, `keyboard`, `shortcut`, `accessibility`.

## `@flighthq/app`

Application identity, lifecycle control, the frame loop, windows, and render views over a swappable web/native backend

6 upstream modules under `packages/app/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/app.test.ts` | 42 | … `addAppRecentDocument`, `attachAppActivate`, `attachAppAllWindowsClosed`, `attachAppEvents`, `attachAppOpenFile`, `attachAppQuitRequest`, `attachAppReady`, `attachAppSecondInstance`, `bounceAppDock`, `cancelAppAttention`, `cancelAppDockBounce`, `clearAppRecentDocuments` (+30 more describe blocks in the file) |
| `src/appHost.test.ts` | 3 | `app explicit Host ownership` |
| `src/appLoop.test.ts` | 60 | … `attachAppLoopExit`, `attachAppLoopLifecycle`, `createAppLoop`, `detachAppLoopExit`, `disposeAppLoop`, `enableAppLoopLifecycleSignals`, `forEachAppLoopWindow`, `getAppLoopFrameRate`, `getAppLoopMainWindow`, `getAppLoopWindows`, `initializeAppLoop`, `isAppLoopRunning` (+10 more describe blocks in the file) |
| `src/appRenderView.test.ts` | 6 | `attachAppRenderView`, `createAppRenderView`, `detachAppRenderView`, `initializeAppRenderView`, `synchronizeAppRenderView` |
| `src/appWindow.test.ts` | 90 | … `attachWindow`, `attachWindowClose`, `attachWindowDropFile`, `attachWindowFocus`, `attachWindowFullscreen`, `attachWindowMove`, `attachWindowOrientation`, `attachWindowRenderContext`, `attachWindowRenderState`, `attachWindowResize`, `attachWindowVisibility`, `centerWindow` (+48 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/app/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_dialog` — `tests/generated_dialog_test.cpp` (incidental: includes the package as part of a larger closure)
- `flight_cpp.generated_app_loop` — `tests/generated_app_loop_test.cpp` (dedicated), NEW. It ports the whole
  `appLoop.test.ts` suite -- 60 of the package's 201 upstream cases, and the package's largest tractable surface --
  and the loop is where this package's real logic lives, so what is covered is a scheduler rather than getters: the
  first tick is worth zero because `lastTime` starts at the not-a-time sentinel; a stall is clamped by
  `maxDeltaTime` (250ms by default, replaced when given); a target frame rate throttles by skipping whole ticks and
  folding the skipped time into the emitted delta; the frame interval is re-read from the LIFECYCLE provider on every
  tick, so a loop started while backgrounded keeps the background rate until the state actually changes; a fixed
  timestep runs whole steps, keeps the remainder as the interpolation alpha, caps updates per frame so a stall cannot
  spiral, and carries the remainder across calls -- including across a change of step size and across a call that ran
  no fixed step at all; pause stops emission WITHOUT cancelling the requested frame, while resume resets the baseline
  so the pause's gap does not land in one frame; the window registry keeps registration order, ignores repeats, and
  drops a window's main-window claim when it is unregistered; and the exit listener is routed through the host, with
  the previous host released when it is replaced or detached. Nine mutations against the override that supplies the
  module, all nine with teeth and one of them by crashing: the first tick measuring the gap 4 failures, the clamp
  dropped 4, the throttle skipping nothing 3, the fixed accumulator not carrying 6, the update cap removed 1, pause
  cancelling the frame 1, resume keeping the baseline 1, the main-window read put back on the bounds-checked accessor
  0 failures but an ABORT, and unregister keeping the main-window claim 1. No equivalence was found on this surface
  and none is claimed.

**An override bug this unit fixed.** `getAppLoopMainWindow` is `_mainWindows.get(app) ?? app.windows[0] ?? null`. The
override's own comment described that fallback correctly -- "the fallback is an array element that may be absent" --
while the code called `Array::element(0.0)`, the bounds-CHECKED accessor, which throws `std::range_error` on an empty
registry instead of producing the absent read. The upstream suite names that case exactly: "returns null with no
windows". The fix is one line to `.get(0.0)`, the accessor that means an absent index. It is the same divergence
already filed as `docs/compiler-requests/out-of-range-array-index-widened-to-null.md` -- there for an emitted
lowering, here in hand-written supply code that contradicted its own comment. The mutation that restores the throwing
accessor aborts, which is how the fix is now covered.

**A member with no row key, again.** `HostAppLoopCapability` declares `now`, and the generated row-member table has no
key that reaches it, so a capability row cannot carry one. Nothing in the module needs it -- the time arrives as the
frame callback's own argument -- and the table is deliberately not broadened for it. The test's row builder says so.
- `flight_cpp.generated_app_window` — `tests/generated_app_window_test.cpp` (dedicated), NEW, and the second half of
  this arc. `appWindow.test.ts` is 90 cases over roughly sixty functions, and most of them are one-line delegations;
  what the file covers is the part that is not a delegation, plus a representative sample of the delegations so the
  routing shape is pinned. The terminal-close rule: `closeWindow` closes and announces onClose, a SECOND close answers
  true without reaching the host again, a host's own terminal notification is guarded the same way, and a successful
  re-attach clears the terminal state so the window can be closed again -- which is also what makes a native callback
  racing an explicit close converge on ONE close. Close is pinned to the host that ATTACHED the window, so closing
  through a different host closes through the origin and the other host sees no calls. `requestWindowClose` reports
  whether a listener vetoed. The events carry change-guards: `onMove` fires only when the position actually moved,
  `showWindow` does nothing to an already-visible window, `setWindowFullscreen` does nothing when the value is
  unchanged, and `restoreWindow` returns early unless the window is minimized or maximized. And the two pieces of
  arithmetic -- a uniform devicePixelRatio scale with the stale translation cleared, and bounds filled into the
  caller's own object and returned by identity. Eleven mutations against the override that supplies the module, ten
  with teeth: the close terminal guard dropped 1 failure, the notify guard dropped 2, the re-attach's terminal clear
  dropped 1, the attach's backend pin dropped 2, the close request not emitted 2, the show guard dropped 3, the
  restore guard dropped 2, the transform scale not applied 1, bounds returning a fresh object 1, and the move guard
  dropped 2. The eleventh is equivalent by construction and is recorded rather than counted: dropping
  `notifyWindowClosed`'s `window_backends.erase(win)` changes nothing on any reachable path, because every reader of
  that map consults the terminal set first and returns early, and the only way to clear the terminal set is a
  successful attach, which re-sets the backend in the same step.

  The second pass added the six window functions whose host parameter is a SINGLE capability row and which the first
  pass had left unsampled: `openWindow` (which applies the options to the entity, each field only when the option is
  present, and then delegates), `setWindowMinimumSize` and `setWindowMaximumSize` (which record the constraint on the
  window and delegate), `setWindowContentProtection`, `requestWindowAttention`, and `setWindowParent` (which passes
  an absent parent THROUGH as absent rather than substituting a placeholder). Seven more mutations, all seven with
  teeth: the open width option dropped 1 failure, the always-on-top option dropped 1, the minimum constraint not
  recorded 2, the maximum not recorded 3, the parent substituted with a placeholder 1, attention inverted 1, and
  content protection inverted 1.

  Coverage boundary, now MEASURED rather than assumed: the families whose host parameter is a MERGED row
  (`RowMerge` of a capability row and a required companion row -- `centerWindow`, `setWindowOpacity`,
  `setWindowResizable`, `setWindowIcon`, `setWindowMenuBarVisible`, `setWindowSkipTaskbar`, `flashWindowFrame`, and
  the `attachWindowResize`/`DropFile`/`Fullscreen`/`Orientation`/`RenderContext`/`RenderState` families) are NOT
  covered, because a merged row over these capabilities cannot be built from the runtime types available to a caller
  here. Both construction paths were tried and both fail, in different ways:

  - Writing one cell by cell does not compile. `flight::row_set` on a row allocated as the merged schema resolves
    that schema's members to `void` -- "structural row schema does not contain the requested property" -- even
    though the schema itself declares them: `flight::detail::schema_member_t` finds `getBounds` and `center` on the
    merged schema, so the lookup is sound and the WRITE site is what cannot reach it.
  - Projecting an existing capability row into the merged schema compiles and then throws `std::bad_cast` at run
    time, which is the cast refusing to hand over a member the source row's schema does not name.

  The emitter's own two merged-row idioms do not help either: the allocate-as-merged form (`node.hpp`) needs that
  same write path, and the cast-a-wider-row form (`command.hpp`) needs ONE existing row carrying BOTH halves --
  `NodeAny` there genuinely contains all the merged members, while here the merged member `center` lives on a
  companion type that nothing wider combines with the geometry row. So the merged families are left uncovered rather
  than covered through an invented row, the signatures are not weakened, and `structuralMemberTable` is not
  broadened to reach them.

- `flight_cpp.generated_app_core` — `tests/generated_app_core_test.cpp` (dedicated), NEW, completing the arc. It
  covers `app.ts`'s one piece of real machinery and samples its delegations, plus the ownership property that is the
  whole of `appHost.test.ts`. The machinery is the application event entity: `createAppEvents` allocates the six
  signals, each `attachApp*` family routes one host provider to one signal, `attachAppQuitRequest` is the only family
  that carries a value in both directions (the host hands it a cancel callback, and a listener that CANCELS the signal
  must call that callback -- that is how a JS-side veto reaches the native quit, and a listener that does not cancel
  must leave it alone), `attachAppEvents` attaches all six at once and releases every origin it replaced, and
  `disposeAppEvents` is teardown in the full sense -- it releases the providers AND clears the listeners, so a
  provider callback already in hand that fires afterwards reaches nobody. The delegations pinned are the ones where
  the shape matters: `isAppHidden` derives from the LIFECYCLE provider rather than one of its own, `setAppBadgeCount`
  returns the provider's task result, the single-instance lock routes three verbs to three separate entries, and the
  opening/provider pair carries a path and an argv array. The ownership property -- the appHost subject -- is asserted
  by handing two providers of the same kind and showing that only the selected one is ever touched, which is the
  behavioural content of that suite's "no ambient backend API" absence case; the test says it is a translation.
  Seven mutations against the override, all seven with teeth: activate emitting the ready signal 1 failure, the quit
  request not emitted 2, attaching all six while skipping one family 4, dispose keeping a listener 1, the lock
  request inverted 1, the hidden query checking the wrong lifecycle state 2, and the badge count not forwarded 1.

  `appHost.test.ts`'s third case asserts an ABSENCE (no `setAppBackend` and five more), covered here by that
  behavioural content rather than by an export-list check, as noted above.

  The RowMerge boundary the previous pass measured is now filed upstream as
  `docs/compiler-requests/merged-row-not-constructible.md`, so the uncovered window families above have a request
  pointing at the cause rather than only a note here. app core and appHost are now covered; the arc is complete.

**Four more members with no row key.** The same table gap the appLoop unit recorded for `HostAppLoopCapability.now`
applies to `HostWindowLifecycleCapability.close` and the `subscribeClose`/`subscribeMove`/`subscribeVisibility`
members of three other capabilities: no key reaches any of them, so a capability row assembled from `row_field`
cells cannot carry one, and a row mixing them with keyed fields does not compile. The module reads those four
through the capability REFERENCE rather than by key, which is why passing a `Ref` works; the test says so where the
hosts are built. Recorded, not broadened -- the table is not touched.

### Local generation status

4 of 6 modules emitted, 2 refused; 4 refusing modules (2 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `app-using-declaration-order-and-any-iteration`, `app-loop-unqualified-types-and-missing-declarations`, `app-render-view-unqualified-types-and-member-access`, `app-window-ref-member-access-and-optional-calls`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/host`

Portable host interface: host construction, capability queries, and host diagnostics

6 upstream modules under `packages/host/src`.
 Depends on `@flighthq/entity`, `@flighthq/log`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/enableHostGuards.test.ts` | 5 | `enableHostGuards` |
| `src/host.test.ts` | 6 | `createHost`, `initializeHost` |
| `src/hostExplain.test.ts` | 18 | `explainHost`, `explainHostAudioDevice`, `explainHostAudioMixer`, `explainHostImage`, `explainHostInputIngress`, `explainHostTextSegmenter`, `explainHostTextShaper`, `explainHostVideo` |
| `src/hostQuery.test.ts` | 112 | … `getHostAudioDevice`, `getHostAudioMixer`, `getHostBitmapEncode`, `getHostBitmapReadback`, `getHostClipboardFormats`, `getHostDevice`, `getHostFileSystem`, `getHostFontLoading`, `getHostGeolocation`, `getHostGlyphRasterizer`, `getHostHaptics`, `getHostImage` (+44 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/host/`: `status.md`, `charter.md`.

### Two measured defects, both filed

This package is the diagnostic surface -- it exists so a MALFORMED host can be understood -- and both of its worst
behaviours are on exactly that input. Neither is worked around; each is filed under `docs/compiler-requests/`.

- **Every accessor in `hostQuery.ts` dereferences its capability GROUP unconditionally.** The source is
  `host.audio?.device ?? null`, and its own comment says the group access is optional-chained because "a structurally
  built stand-in with a group missing must produce `null` rather than a TypeError"; the emitted C++ is
  `return flight::row_get<RowKey<"audio">>(host)->device;`. Measured: a host row with no `audio` group PANICS with
  "member access within null pointer of type 'HostAudioCapabilities'" where the source returns null. Filed as
  [host-group-optional-chain-dropped](../compiler-requests/host-group-optional-chain-dropped.md).
- **`createHost`/`initializeHost` do not compile for any instantiation.** `capabilities.audio ?? {}` is lowered to
  `.value_or(...)` on a read that yields a `Ref` rather than an optional, so the whole of `host.ts` has no callable
  surface and its six upstream cases cannot be ported. Filed as
  [host-initialize-optional-fallback-on-a-required-read](../compiler-requests/host-initialize-optional-fallback-on-a-required-read.md).

A third boundary is environmental rather than a defect: `enableHostGuards` routes every warning through
`@flighthq/log`, and that package's override declares throwing stubs, so enabling the guards aborts with
`flight::Error: flight::log::merge_span_fields was not generated for this profile`. The guard tier is therefore left
uncovered rather than asserted through a stubbed seam; the slot-level tables it consumes are covered by the test
below. The package is tier *assisted* for an emission repair, not an override.

### Existing CTest coverage

- `flight_cpp.generated_host` — `tests/generated_host_test.cpp` (dedicated), NEW. It covers what the two defects leave
  reachable, building hosts by hand group by group because the factory cannot be called: each accessor returns the
  capability it names BY IDENTITY, not a copy; a group that is present with an absent slot reads as an absent optional
  rather than an error; `explainHost` enumerates only the groups the host actually carries, with each group's filled
  slot names and none for a group carrying nothing; its capability census names every covered slot with its group,
  slot and presence; `isSlotPresent` answers for a group/slot pair; and a per-capability explanation reports presence
  or, when absent, the missing slot and a message. The absent-group panic is recorded at the point of use rather than
  asserted, since a panic cannot be asserted in-process. Five mutations against the emitted modules, all five with
  teeth: the slot-presence test inverted 6 failures, every capability reported present 1, the mixer explained as the
  device 2, the device accessor always absent 1, and the video accessor always absent 1.

### Local generation status

6 of 6 modules emitted, 0 refused; 3 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/platform`

Platform identification seam for the desktop/mobile integration suite: OS/kind/arch detection with a swappable host backend

3 upstream modules under `packages/platform/src`.
 Depends on `@flighthq/entity`, `@flighthq/types`, `@flighthq/useragent`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/platform.test.ts` | 37 | … `comparePlatformVersions`, `createPlatformInfo`, `getPlatformEngine`, `getPlatformInfo`, `getPlatformKind`, `getPlatformName`, `getPlatformRuntime`, `initializePlatformInfo`, `isPlatformDesktop`, `isPlatformMobile`, `isPlatformNative`, `isPlatformTouch` (+3 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/platform/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/lifecycle`

Application foreground/background lifecycle state and resume/pause/back signals over a swappable web/native backend

3 upstream modules under `packages/lifecycle/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/lifecycle.test.ts` | 37 | … `attachAppLifecycle`, `createAppLifecycle`, `detachAppLifecycle`, `disposeAppLifecycle`, `explainLifecycleOperation`, `getAppLaunchKind`, `getAppLifecycleState`, `hasLifecycleOperation`, `initializeAppLifecycle`, `isAppActive`, `isAppBackground`, `isAppInactive` (+1 more describe blocks in the file) |
| `src/lifecycleHost.test.ts` | 7 | `getAppLaunchKind`, `getAppLifecycleState`, `isAppActive`, `isAppBackground`, `isAppInactive` |

Upstream carries per-package records at `flight/agents/packages/lifecycle/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

Two dedicated tests. The package was already reached before this unit; the second file is new and leaves the first
untouched.

- `flight_cpp.generated_lifecycle` — `tests/generated_lifecycle_test.cpp` (dedicated), PRE-EXISTING. It pins the
  shape of the module: the seven signals, the operation explanations and their host/sentinel layer, launch-kind
  delegation and its `warm` fallback, one raw/paused/resumed/saved/restored sequence, replacement and idempotent
  detach with release counts, the state helpers, a cancelled back request, and dispose clearing the saved bag.
- `flight_cpp.generated_lifecycle_transitions` — `tests/generated_lifecycle_transitions_test.cpp` (dedicated), NEW.
  It takes the EDGE TABLE the first file samples rather than pins, and the save/restore handoff:
  `onStateChange` is raw, so a repeat notification of the same state is reported again; `onPause`/`onResume` are
  deduped edges across the `active` boundary only, so `active`→`inactive` (the interruption edge) pauses and
  `inactive`↔`background` fires neither in either direction; a repeat while already away from `active` does not
  pause twice; attaching records the provider's state AT ATTACH as the baseline without emitting, so the first
  move into `active` is a genuine edge from that baseline; the bag handed to `onSaveState` is the very bag a later
  `onRestoreState` receives (asserted by writing in the save listener and reading it back in the restore listener,
  which a fresh bag would lose), and a second save replaces what the second restore carries; re-attaching releases
  the replaced subscription exactly once and the registry keeps one per app; a provider that omits the memory
  capability is never asked to subscribe to it and still detaches cleanly; the state queries read the provider at
  call time; and `requestAppBack` answers true unless a listener actually cancelled, with no listener at all being
  the default path rather than a veto. It drives the signals through connected listeners rather than by replacing
  each signal's `emit`, so the signal's own slot machinery is on the path.

Mutations against the override that supplies the module, eight with all eight resolved and seven with teeth:
dropping the resume edge's dedup 2 failures, dropping the pause edge's 3, emitting `onStateChange` only on edges
5, handing the restore a fresh bag 2, hard-coding the attach baseline to `active` 1, keeping the registration
across a detach 1, and inverting the back veto 3. The eighth is equivalent by construction and is recorded rather
than counted: rewriting the back request's `!present || !cancelled` as `!(present && cancelled)` is De Morgan on
the same two operands, so it changes nothing.

### Local generation status

2 of 3 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `lifecycle-complete-module`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/input`

Portable input normalization for keyboard, pointer, wheel, gamepad, and text events, with a held-state snapshot and per-frame edge queries

3 upstream modules under `packages/input/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/index.test.ts` | 3 | `input exports` |
| `src/inputManager.test.ts` | 63 | … `applyGamepadAxisDeadZone`, `applyGamepadStickDeadZone`, `attachGamepadInput`, `attachKeyboardInput`, `attachPointerInput`, `attachRelativePointerInput`, `attachTextInput`, `attachWheelInput`, `connectInputStateToInputManager`, `createInputKeyRepeatTimer`, `createInputManager`, `createInputSignals` (+23 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/input/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_input` — `tests/generated_input_test.cpp` (dedicated). PRE-EXISTING and untouched: it pins the
  key-repeat timer's contract — the callback runs immediately on press, again after the delay, then every interval
  until stop(), and a new start cancels what the previous one left behind — driven by pumping a queue rather than by
  waiting.
- `flight_cpp.generated_input_manager` — `tests/generated_input_manager_test.cpp` (dedicated), added by the unit that
  audited this package for the half the timer test does not reach: the axis and radial stick dead zones, ported from
  `inputManager.test.ts`'s own cases for them. The axis maps a value inside `[-deadZone, deadZone]` to 0 and rescales
  the rest linearly onto `[-1, 1]` with full deflection still exactly 1 and a zero zone passing the raw value through;
  the stick compares the RADIAL magnitude rather than either axis, preserves direction, rescales onto `[0, 1]`, and is
  asserted alias-safe by passing the output object's own fields back in as the inputs. Six mutations against the
  override that supplies the module: five with teeth (zero-zone pass-through 2 failures, axis rescale 3, per-axis stick
  magnitude 1, stick scale 3, output-instead-of-input 5) and one observationally equivalent -- making the axis boundary
  exclusive changes nothing, because the rescale is continuous and both spellings answer 0 exactly at the zone's edge.

- `flight_cpp.generated_input_state` — `tests/generated_input_state_test.cpp` (dedicated), added by the unit that took
  the 63 upstream event/state cases the two tests above leave. It drives the manager through the ingress sink it hands
  a host (`getInputIngressSink`) and reads the results back through the state queries: held keys/pointer buttons/gamepad
  buttons and axes; the up→down edge arming once per genuine transition, so a re-reported held key or gamepad button
  does NOT re-arm the press edge; a press and a release inside one frame both surviving; `endInputStateFrame` clearing
  the four edge sets while held state — a snapshot of the device, not of the frame — outlives the boundary; the payload
  passing through unchanged (modifier flags, pointer coordinates and deltas); pointer buttons as a per-pointer bitmask
  with a cancel clearing one pointer and not another; gamepad buttons and axes keyed by device so two pads never alias;
  a disabled manager emitting nothing and resuming; the sink memoized per manager; and unknown pointers/gamepads reading
  as absent rather than throwing. Seven mutations against the override that supplies the module, one per category the
  work named: dropping the key-down repeat guard 1 failure, dropping the gamepad button-down repeat guard 1, ignoring
  the pointer button index in the press bitmask 3, leaving the cancelled pointer's buttons in place 2, keying gamepad
  buttons by button without the device 4, and dropping the pressed-key clear from `endInputStateFrame` 2. The seventh
  is observationally equivalent and is recorded as such rather than counted as a kill: relaxing `onPointerUp`'s
  `next === 0` to `next <= 0` changes nothing, because the mask is built by OR-ing non-negative powers of two and is
  therefore never negative, so both spellings agree on every value the pointer path can produce.

- `flight_cpp.generated_input_binding` — `tests/generated_input_binding_test.cpp` (dedicated), added by the unit that
  took `inputManager.test.ts`'s "explicit input ingress parameter" block, the ingress the state machine does not consume,
  and the repeat timer wired to the manager. It drives all six `attach*Input`/`detach*Input` families against a recording
  host capability and pins what the binding registry owes: each family calls its own capability entry point, with its own
  source identity, and hands every one of them the manager's single sink; a detach invokes that kind's release exactly
  once, and repeating it — or detaching a kind or a source that was never bound — is silent; the binding is selected by
  source *and* kind, so detaching one kind on a shared source leaves the others bound and vice versa;
  re-attaching the same source with a different backend releases the origin it replaced exactly once and never again;
  the registry holds exactly one reference per source however many kinds are bound to it, and drops it when the last kind
  detaches; an options row is handed through as given, absence included; and detaching a source does not disable the
  manager, which stays enabled and still normalizes what reaches its sink. It also covers relative pointer move (asserted
  against every field the row can carry), wheel, and text input/edit for payload pass-through, per-signal order, and the
  absence of any held-state effect, and wires `createInputKeyRepeatTimer` to the manager — which is where the
  auto-repeat guard earns its place: the timer's own keydowns must not re-arm the press edge on each tick, the timer
  keeps ticking while the manager is disabled and resumes delivering when it is re-enabled, and restarting the timer
  cancels the cycle it replaced so one key cannot drive two repeat streams. Six mutations against the override supply
  the module, all six with teeth: dropping the previous-release in `setInputBinding` 2 failures, detaching with the
  wrong kind symbol 8, keeping a cleared registration 4, not cancelling on restart 1, emitting a relative move on the
  absolute signal 1, and never dropping the emptied source entry 1.

Still not covered, and not claimed: `getGamepadButtonName`/`getGamepadAxisName` are sampled rather than exhaustive (3
of 18 button indices and 2 of 4 axes), and the sink's `isEnabled` probe is asserted only through a disabled manager
rather than called directly.

### Measured divergences from upstream

Both are pinned by `tests/generated_input_state_test.cpp` rather than asserted as correct, so the gap is visible and
the check flips when the fix lands.

- **The disposer throws.** `connectInputStateToInputManager` returns a disposer that disconnects the ten handlers it
  connected, but those handlers are bare lambdas erased into `std::function`, and `disconnectSignal` compares slots by
  callable identity, which that erasure destroys — the runtime refuses to guess and throws rather than detaching some
  other closure. Upstream's "returns a disposer that stops tracking" therefore cannot hold at this pin. This is the
  already-filed `docs/compiler-requests/local-handler-callable-identity.md` (a newly measured instance, not a new
  defect); the same check shows an identity-bearing `flight::Function` slot disconnects cleanly, so the missing piece
  is the emitter's erasure, not a runtime operation. Note the blunt edge: because the check inspects every stored slot
  and gives up on the first erased one, a manager's signal cannot be disconnected at all once a state is connected.
- **An out-of-range gamepad index throws.** `getGamepadButtonName('standard', 99)` returns `null` upstream and throws
  `std::range_error` here, because `table[i] ?? null` lowers to the bounds-checked `Array::element` rather than
  `Array::get`. Filed as `docs/compiler-requests/out-of-range-array-index-widened-to-null.md`.

### Members a row cannot carry

The generated row-member table maps a camelCase key to the snake_case field of the same name, and it has no entry for
several members these types declare. A structural row over them therefore cannot carry those members at all:

| type | declared members with no row key |
| --- | --- |
| `InputKeyboardData`, `InputPointerData` | `timeStamp` (the table's `timestamp` key maps to a field spelled `timestamp`, which none of these types has) |
| `InputPointerData` | `tiltX`, `tiltY`, `twist`, `pointerType`, `wheelMode` |
| `InputTextData` | `isComposing` |
| `InputGamepadConnectData` | `mapping` |
| `AttachInputOptions` | `preventDefault` |

The payload assertions in `tests/generated_input_binding_test.cpp` are exhaustive over what *can* be carried and say
so; the members above are named here rather than worked around, because the gap is in the generated table rather than
in the module. `preventDefault` is the one with a behavioural edge: an options row can be handed through and observed
as present, but its contents cannot be constructed to check that the module forwards them unchanged.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules. (The row read 2 of 3 with one refusal before this audit; the package is fully emitted at the current pin.) Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `input-manager-refused-state-and-timer-boundaries`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/keyboard`

On-screen (soft) keyboard visibility, height, and show/hide/resize signals over a swappable web/native backend

3 upstream modules under `packages/keyboard/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/keyboard.test.ts` | 23 | … `attachSoftKeyboard`, `createSoftKeyboard`, `detachSoftKeyboard`, `disposeSoftKeyboard`, `getSoftKeyboardHeight`, `getSoftKeyboardInfo`, `hideSoftKeyboard`, `initializeSoftKeyboard`, `isSoftKeyboardVisible`, `setSoftKeyboardAccessoryBarVisible`, `setSoftKeyboardResizeMode`, `setSoftKeyboardScrollAssistEnabled` (+2 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/keyboard/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Scope

This package is the **soft (on-screen) keyboard** facade, not the hardware keyboard. It exposes visibility, height and
the show/hide/resize signals over a swappable host backend, and nothing else: there is no key code, no key-to-name
mapping, no modifier state, no repeat or chord handling and no composition in it, and its `index.ts` exports none.
Those concepts live in `@flighthq/input` (key data, `KeyCode`, modifier flags) and `@flighthq/shortcut` (chords), and
are covered from those packages. A coverage request written in hardware-keyboard vocabulary therefore names the wrong
axis for this package; the axes below are the ones it actually declares.

### Existing CTest coverage

- `flight_cpp.generated_keyboard` — `tests/generated_keyboard_test.cpp` (dedicated), added by the unit that took this
  package. It ports all 23 upstream cases and extends them where the emitted module has structure the upstream suite
  only samples: the six delegating setters (show/hide plus accessory bar, resize mode, scroll assist and style) are
  each asserted to reach their own host entry point and to hand their argument through by value, with a backend's
  `operation-failed` and `operation-unavailable` results reaching the caller unchanged and a rejected backend task
  staying a rejection rather than becoming a value; `getSoftKeyboardInfo` is asserted to write the caller's object and
  return **that very object** (reference identity, not a copy) while the two scratch-backed convenience reads leave a
  caller's object untouched; and the attach state machine is pinned as a table, including the two arms upstream does
  not test — a reported change that does not move the height fires nothing, and the baseline is the height captured
  at **attach** rather than at the first change, so a keyboard that is already visible resizes rather than shows, with
  a hide resetting that baseline so a later rise is a fresh show. Attach re-subscribing (releasing what it replaced,
  exactly once) and a refused acquisition registering nothing are asserted too. Seven mutations against the generated
  module, six with teeth: dropping the show arm's edge test 6 failures, making the visible test inclusive 5, keeping a
  cleared registration 2, not replacing on re-attach 1, returning a copy from `getSoftKeyboardInfo` 3, and dropping
  the style setter's argument 1. The seventh is equivalent by construction and is recorded as such rather than counted
  as a kill: replacing the literal `"ok"` in attach's result test with the named constant `soft_keyboard_attach_ok_kind`
  compares the same string, so it changes nothing.

No other local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/interaction`

Interaction: pointer event dispatch, hit testing, focus, and cursor management

19 upstream modules under `packages/interaction/src`.
 Depends on `@flighthq/bitmap`, `@flighthq/clip`, `@flighthq/entity`, `@flighthq/geometry`, `@flighthq/log`, `@flighthq/node`, `@flighthq/path`, `@flighthq/registry`, `@flighthq/shape`, `@flighthq/signals`, `@flighthq/spatial`, `@flighthq/text`, `@flighthq/textlayout`, `@flighthq/tilemap`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/displayHitTests.test.ts` | 9 | `defaultHtmlViewHitTestHandler`, `defaultMovieClipHitTestHandler`, `defaultNode2DHitTestHandler`, `defaultRichTextHitTestHandler`, `defaultShapeHitTestHandler`, `defaultTextHitTestHandler` |
| `src/displayObjectOverlap.test.ts` | 8 | `containsNode2D`, `getNode2DOverlapRectangle`, `hitTestNode2DsShape` |
| `src/enableInteractionGuards.test.ts` | 5 | `disableInteractionGuards`, `enableInteractionGuards`, `explainInteractionHitEligibility` |
| `src/focusManager.test.ts` | 20 | `clearFocus`, `connectFocusNavigation`, `createFocusManager`, `focusNextNode`, `focusNodeInDirection`, `focusPreviousNode`, `getFocusedNode`, `getFocusOrder`, `initializeFocusManager`, `isNodeFocused`, `setFocusedNode` |
| `src/hitTests.test.ts` | 35 | `describeGraphHit`, `findGraphHitTarget`, `findGraphHitTargetPrecise`, `findGraphHitTargets`, `findGraphHitTargetsPrecise`, `hitTestGraphLocalBounds`, `hitTestGraphPoint`, `hitTestGraphPointPrecise`, `hitTestNode2Ds`, `hitTestNodeRegion`, `registerHitTest`, `registerHitTestPrecise` |
| `src/interactionManager.test.ts` | 76 | … `captureInteractionPointer`, `connectInputToInteraction`, `connectInteractionDispatchLayer`, `connectInteractionSignal`, `createInteractionManager`, `createInteractionSignals`, `disconnectInteractionSignal`, `dispatchInteractionContextMenu`, `dispatchInteractionKeyDown`, `dispatchInteractionKeyUp`, `dispatchInteractionPointerCancel`, `dispatchInteractionPointerDown` (+10 more describe blocks in the file) |
| `src/interactionSpatialIndex.test.ts` | 6 | `findSpatialInteractionTarget`, `refreshInteractionSpatialIndex` |
| `src/nodeInteractionState.test.ts` | 18 | … `areNodeChildrenHitTestEnabled`, `createNodeInteractionState`, `enableNodeInteractionState`, `getNodeCursor`, `getNodeHitArea`, `getNodeInteractionState`, `getNodeTabIndex`, `initializeNodeInteractionState`, `isNodeFocusable`, `isNodeHitTestEnabled`, `isNodePointerDoubleClickEnabled`, `setNodeChildrenHitTestEnabled` (+6 more describe blocks in the file) |
| `src/nodeInteractiveStateBinding.test.ts` | 9 | `applyNodeInteractiveStates`, `createNodeInteractiveStateBinding`, `disposeNodeInteractiveStateBinding`, `explainNodeInteractiveStateBinding` |
| `src/registerDefaultHitTests.test.ts` | 2 | `registerDefaultHitTests` |
| `src/registerQuadBatchHitTest.test.ts` | 5 | `registerQuadBatchHitTest` |
| `src/registerShapeHitTest.test.ts` | 2 | `registerShapeHitTest` |
| `src/registerSpriteHitTest.test.ts` | 1 | `registerSpriteHitTest` |
| `src/registerTextHitTest.test.ts` | 2 | `registerTextHitTest` |
| `src/registerTilemapHitTest.test.ts` | 3 | `registerTilemapHitTest` |
| `src/spatialQuery.test.ts` | 3 | `hitTestAreaQuery`, `hitTestAreaQueryCircle` |
| `src/spriteHitTests.test.ts` | 8 | `defaultQuadBatchHitTestHandler`, `defaultSpriteHitTestHandler`, `defaultTilemapHitTestHandler` |

Upstream carries per-package records at `flight/agents/packages/interaction/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_interaction` — `tests/generated_interaction_test.cpp` (dedicated)

### Local generation status

8 of 19 modules emitted, 11 refused; 17 refusing modules (11 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `interaction-node-interaction-state`, `interaction-state-binding-runtime-boundary`, `interaction-display-overlap-bounds-boundary`, `interaction-spatial-query-bounds-boundary`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/gui`

Controller-only behavior for caller-authored 2D interface visuals

20 upstream modules under `packages/gui/src`.
 Depends on `@flighthq/interaction`, `@flighthq/node`, `@flighthq/signals`, `@flighthq/text`, `@flighthq/textinput`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/buttonController.test.ts` | 7 | `createButtonController`, `disposeButtonController`, `getButtonControllerSignals`, `isButtonControllerDisabled`, `setButtonControllerDisabled` |
| `src/comboBoxController.test.ts` | 5 | `createComboBoxController`, `disposeComboBoxController`, `getComboBoxControllerSignals`, `isComboBoxControllerOpen`, `setComboBoxControllerOpen` |
| `src/guiController.test.ts` | 15 | … `clampGuiValue`, `configureGuiHitArea`, `connectGuiInteraction`, `connectGuiSignal`, `createGuiController`, `createGuiControllerRuntime`, `disposeGuiController`, `getGuiControllerRuntime`, `getGuiLength`, `getGuiPosition`, `setGuiPosition`, `setGuiScale` (+3 more describe blocks in the file) |
| `src/guiDialog.test.ts` | 11 | `closeGuiDialog`, `createGuiDialog`, `disposeGuiDialog`, `enqueueGuiDialog`, `getActiveGuiDialogEntry`, `getGuiDialogEntries`, `getGuiDialogSignals`, `removeGuiDialogEntry` |
| `src/guiTreeShaking.test.ts` | 2 | `GUI controller tree shaking` |
| `src/listController.test.ts` | 7 | `createListController`, `disposeListController`, `getListControllerSelectedIndex`, `getListControllerSignals`, `setListControllerSelectedIndex`, `setListControllerVisible` |
| `src/progressBarController.test.ts` | 4 | `createProgressBarController`, `disposeProgressBarController`, `getProgressBarControllerValue`, `setProgressBarControllerValue` |
| `src/radioGroupController.test.ts` | 5 | `createRadioGroupController`, `disposeRadioGroupController`, `getRadioGroupControllerSelectedIndex`, `getRadioGroupControllerSignals`, `setRadioGroupControllerSelectedIndex` |
| `src/scrollBarController.test.ts` | 6 | `createScrollBarController`, `disposeScrollBarController`, `getScrollBarControllerSignals`, `getScrollBarControllerValue`, `setScrollBarControllerValue` |
| `src/scrollViewController.test.ts` | 6 | `createScrollViewController`, `disposeScrollViewController`, `getScrollViewControllerSignals`, `getScrollViewControllerX`, `getScrollViewControllerY`, `setScrollViewControllerPosition` |
| `src/sliderController.test.ts` | 6 | `createSliderController`, `disposeSliderController`, `getSliderControllerSignals`, `getSliderControllerValue`, `setSliderControllerValue` |
| `src/splitPaneController.test.ts` | 5 | `createSplitPaneController`, `disposeSplitPaneController`, `getSplitPaneControllerPosition`, `getSplitPaneControllerSignals`, `setSplitPaneControllerPosition` |
| `src/tabBarController.test.ts` | 5 | `createTabBarController`, `disposeTabBarController`, `getTabBarControllerSelectedIndex`, `getTabBarControllerSignals`, `setTabBarControllerSelectedIndex` |
| `src/textInputController.test.ts` | 7 | `blurTextInputController`, `createTextInputController`, `dispatchTextInputControllerKeyDown`, `dispatchTextInputControllerText`, `disposeTextInputController`, `focusTextInputController`, `getTextInputControllerSignals` |
| `src/toggleController.test.ts` | 5 | `createToggleController`, `disposeToggleController`, `getToggleControllerSignals`, `isToggleControllerChecked`, `setToggleControllerChecked` |
| `src/tooltipController.test.ts` | 4 | `createTooltipController`, `disposeTooltipController`, `hideTooltipController`, `showTooltipController` |
| `src/treeViewController.test.ts` | 9 | `createTreeViewController`, `disposeTreeViewController`, `getTreeViewControllerSelectedItem`, `getTreeViewControllerSignals`, `isTreeViewControllerItemExpanded`, `setTreeViewControllerItemExpanded`, `setTreeViewControllerSelectedItem`, `toggleTreeViewControllerItem` |
| `src/windowController.test.ts` | 6 | `createWindowController`, `disposeWindowController`, `getWindowControllerSignals`, `setWindowControllerPosition`, `setWindowControllerSize` |

Upstream carries per-package records at `flight/agents/packages/gui/`: `status.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_accessibility` — `tests/generated_accessibility_test.cpp` (dedicated), NEW. The package had no
  local test of any kind. It is six functions that only route an operation to the Host provider the caller selected,
  so what this file pins is what the module passes through UNCHANGED: `announceAccessibility` supplies the declared
  `polite` default when liveness is omitted and passes an explicit `assertive` through rather than replacing it; every
  operation returns the provider's own outcome with BOTH its reason and its variant arm preserved (an `ok` and a
  blocked answer are asserted not to collapse into one arm); each function reaches exactly its own entry point, with a
  call log asserting the six in order and a second provider standing untouched so the Host parameter is shown to be
  explicit rather than ambient; a published node arrives whole, including role, description, parentId and the nested
  `bounds` and `states` rows, with a cleared state preserved as present-and-false rather than absent; the object the
  provider recovers is the very object published rather than a copy; an empty id is forwarded as given because the
  module validates nothing; and a provider that throws propagates instead of being absorbed. Seven mutations against
  the emitted module, all seven with teeth: the default liveness changed 1 failure, the default emptied 2, the message
  dropped 1, `setFocus` routed to `removeNode` 2, `destroy` routed to `clear` 3, `setNode` publishing a fresh node
  instead of the caller's 8, and a rebuilt outcome replacing the provider's 1. An eighth is equivalent by
  construction and is recorded rather than counted: inlining the `value_or` default into the call, rather than
  assigning it to the by-value parameter first, passes the same value and mutates nothing observable.

### A member with no row key

The generated row-member table has no entry for `AccessibilityNode`'s `role`, `description` or `parentId`, so those
members cannot be read by key (`flight::row_get<flight::RowKey<"role">>` does not compile) even though a
`Ref`-backed row carries them faithfully to the callee -- `test_a_published_node_arrives_whole` reads them through
the recovered object for that reason. The same table has no key for `AccessibilityState`'s `level` and the
`valueMin`/`valueMax`/`valueNow` numeric range. Recorded rather than worked around: deciding whether those keys
should exist is a table-generation question, not this package's.

### Local generation status

16 of 20 modules emitted, 4 refused; 20 refusing modules (4 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/layout`

Renderer-neutral anchor, flex, and grid rectangle layout over flat trees

8 upstream modules under `packages/layout/src`.
 Depends on `@flighthq/entity`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/anchorLayout.test.ts` | 7 | `registerAnchorLayoutResolver` |
| `src/enableLayoutGuards.test.ts` | 1 | `enableLayoutGuards` |
| `src/flexLayout.test.ts` | 30 | `registerFlexLayoutResolver` |
| `src/gridLayout.test.ts` | 15 | `registerGridLayoutResolver` |
| `src/layoutState.test.ts` | 3 | `createLayoutState`, `initializeLayoutState`, `registerLayoutResolver` |
| `src/layoutTreeShaking.test.ts` | 2 | `layout resolver tree shaking` |
| `src/resolveLayoutTree.test.ts` | 15 | `explainLayoutResolution`, `resolveLayoutTree` |

Upstream carries per-package records at `flight/agents/packages/layout/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

5 of 8 modules emitted, 3 refused; 5 refusing modules (3 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/accessibility`

Host-explicit assistive-technology bridge for mirrored semantic nodes, focus, and announcements

3 upstream modules under `packages/accessibility/src`.
 Depends on `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/accessibility.test.ts` | 6 | `announceAccessibility`, `clearAccessibilityTree`, `destroyAccessibility`, `removeAccessibilityNode`, `setAccessibilityFocus`, `setAccessibilityNode` |

Upstream carries per-package records at `flight/agents/packages/accessibility/`: `status.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/audio`

AudioResource lifecycle — create/clone/dispose, sample-tier and decoded-bytes constructors, URL/Blob/Base64 loaders, codec negotiation, and container MIME detection

8 upstream modules under `packages/audio/src`.
 Depends on `@flighthq/entity`, `@flighthq/net`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/audioDecodeSlot.test.ts` | 6 | `getAudioDecodeSlot`, `hasAudioDecodeSlot` |
| `src/audioFormat.test.ts` | 29 | `canPlayAudioType`, `detectAudioMimeType`, `getAudioMimeTypeEssence`, `getAudioMimeTypeParameter`, `inferAudioMimeType` |
| `src/audioResource.test.ts` | 16 | `cloneAudioResource`, `createAudioResource`, `disposeAudioResource`, `getAudioResourceByteSize`, `getAudioResourceChannelCount`, `getAudioResourceChannelData`, `getAudioResourceDuration`, `getAudioResourceSampleRate`, `hasAudioResourceBuffer`, `initializeAudioResource`, `isAudioResourceEmpty` |
| `src/audioResourceFrom.test.ts` | 26 | `createAudioResourceFromSamples`, `loadAudioResourceFromBase64`, `loadAudioResourceFromBlob`, `loadAudioResourceFromBytes`, `loadAudioResourceFromUrl`, `loadAudioResourceFromUrls`, `selectAudioResourceUrl` |
| `src/audioResourceReference.test.ts` | 22 | `createAudioResourceFailure`, `createEmbeddedAudioResourceReference`, `createExternalAudioResourceReference`, `explainAudioResourceReferenceResolution`, `findAudioResourceReferenceByName`, `initializeEmbeddedAudioResourceReference`, `initializeExternalAudioResourceReference`, `resetFailedAudioResourceReference`, `resolveAudioResourceReference` |
| `src/decodeAudioResourceBytes.test.ts` | 10 | `decodeAudioResourceBytes` |

Upstream carries per-package records at `flight/agents/packages/audio/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

5 of 8 modules emitted, 3 refused; 7 refusing modules (3 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `audio-variant-member-access-and-enum-qualification`, `audio-from-nullopt-return-type-and-missing-infer`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/media`

Audio and video playback channels

7 upstream modules under `packages/media/src`.
 Depends on `@flighthq/audio`, `@flighthq/entity`, `@flighthq/log`, `@flighthq/math`, `@flighthq/signals`, `@flighthq/types`, `@flighthq/video`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/audioChannel.test.ts` | 55 | … `clearAudioChannelLoopRegion`, `destroyAudioChannel`, `fadeAudioChannelGain`, `getAudioChannelCurrentTime`, `getAudioChannelDuration`, `getAudioChannelSourceHandle`, `hasAudioChannelFade`, `isAudioChannelMuted`, `isAudioChannelPlaying`, `pauseAudioChannel`, `playAudioResource`, `resumeAudioChannel` (+8 more describe blocks in the file) |
| `src/audioMixer.test.ts` | 32 | … `addAudioBusToMixer`, `createAudioBus`, `createAudioMixer`, `destroyAudioMixer`, `explicit provider-first mixer operations`, `fadeAudioBusGain`, `getAudioMixerActiveChannels`, `initializeAudioBus`, `pauseAllAudioMixerChannels`, `resumeAllAudioMixerChannels`, `resumeAllAudioMixerChannels scope`, `routeAudioChannelToMixerBus` (+8 more describe blocks in the file) |
| `src/enableAudioMixerGuards.test.ts` | 3 | `disableAudioMixerGuards`, `enableAudioMixerGuards` |
| `src/mediaChannelSignals.test.ts` | 8 | `enableAudioChannelSignals`, `enableVideoChannelSignals`, `getAudioChannelSignals`, `getVideoChannelSignals` |
| `src/videoChannel.test.ts` | 29 | … `destroyVideoChannel`, `getVideoChannelCurrentTime`, `getVideoChannelDuration`, `getVideoChannelHeight`, `getVideoChannelWidth`, `isVideoChannelMuted`, `isVideoChannelPlaying`, `pauseVideoChannel`, `playVideoResource`, `resumeVideoChannel`, `setVideoChannelCurrentTime`, `setVideoChannelGain` (+3 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/media/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

5 of 7 modules emitted, 2 refused; 5 refusing modules (2 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/mediasession`

Explicit-Host OS media-session commands and per-action transport event entities

3 upstream modules under `packages/mediasession/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/mediasession.test.ts` | 19 | `attachMediaSessionAction`, `clearMediaSessionMetadata`, `clearMediaSessionPositionState`, `createMediaSessionActionSignal`, `destroyMediaSession`, `detachMediaSessionAction`, `disposeMediaSessionActionSignal`, `initializeMediaSessionActionSignal`, `setMediaSessionMetadata`, `setMediaSessionPlaybackState`, `setMediaSessionPositionState` |

Upstream carries per-package records at `flight/agents/packages/mediasession/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/video`

Video resources and URL constructors

5 upstream modules under `packages/video/src`.
 Depends on `@flighthq/entity`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/videoFormat.test.ts` | 32 | `canPlayVideoType`, `detectVideoMimeType`, `inferVideoMimeType`, `selectVideoResourceUrl` |
| `src/videoResource.test.ts` | 35 | `createVideoResource`, `destroyVideoResource`, `disposeVideoResource`, `getVideoResourceDuration`, `getVideoResourceHeight`, `getVideoResourceWidth`, `hasVideoResourceElement`, `initializeVideoResource`, `isVideoResourceEmpty`, `isVideoResourceReady` |
| `src/videoResourceFrom.test.ts` | 12 | `loadVideoResourceFromBlob`, `loadVideoResourceFromUrl`, `loadVideoResourceFromUrls` |

Upstream carries per-package records at `flight/agents/packages/video/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

4 of 5 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

Refusal text naming a browser surface (1 module) — listed to be read, not to classify:
- `packages/video/src/videoResourceFrom.ts` — cpp emission failed for @flighthq/video/packages/video/src/videoResourceFrom.ts: C++ runtime contract maps distinct union domains to one carrier without a discriminator or checked projection (HTMLImageElement | HTMLVideoElement | ImageBitmap | OffscreenCanvas | SVGImageElement | VideoFrame -> flight::host_sdl::ImageSource); give the domains distinguishable key, element, or member types so each reaches a carrier of its own -- a union of two declarations that differ only in name is one carrier whatever the emitter does -- or provide that runtime contract

## `@flighthq/webcam`

Future explicit-host live webcam streaming, device enumeration, and stream lifecycle

2 upstream modules under `packages/webcam/src`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/missing.test.ts` | 1 | — (no `describe`; the cases assert the module surface directly) |

Upstream carries per-package records at `flight/agents/packages/webcam/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

2 of 2 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/midi`

Explicit MIDI access, port resources, and lifecycle subscriptions

7 upstream modules under `packages/midi/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/midiAccess.test.ts` | 7 | `createMidiAccessResource`, `disposeMidiAccess`, `getMidiAccessInputPorts`, `getMidiAccessOutputPorts`, `initializeMidiAccessResource`, `requestMidiAccess` |
| `src/midiPermission.test.ts` | 1 | `getMidiPermission` |
| `src/midiPort.test.ts` | 11 | `closeMidiPort`, `createMidiInputPortResource`, `createMidiOutputPortResource`, `disposeMidiPort`, `getMidiPortConnection`, `getMidiPortState`, `initializeMidiInputPortResource`, `initializeMidiOutputPortResource`, `openMidiPort`, `sendMidiMessage` |
| `src/midiResource.test.ts` | 5 | `getMidiAccessResourceState`, `getMidiPortResourceState`, `retainMidiAccessResourceState`, `retainMidiInputPortResourceState`, `retainMidiOutputPortResourceState` |
| `src/midiSubscription.test.ts` | 16 | … `attachMidiAccessStateSubscription`, `attachMidiInputMessageSubscription`, `attachMidiPortStateSubscription`, `createMidiAccessStateSubscription`, `createMidiInputMessageSubscription`, `createMidiPortStateSubscription`, `detachMidiAccessStateSubscription`, `detachMidiInputMessageSubscription`, `detachMidiPortStateSubscription`, `disposeMidiAccessStateSubscription`, `disposeMidiInputMessageSubscription`, `disposeMidiPortStateSubscription` (+3 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/midi/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

5 of 7 modules emitted, 2 refused; 5 refusing modules (2 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/sensors`

Device motion and orientation sensor readings and signals over a swappable web/native backend

3 upstream modules under `packages/sensors/src`.
 Depends on `@flighthq/entity`, `@flighthq/math`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/sensors.test.ts` | 62 | … `attachSensors`, `computeEulerFromQuaternion`, `computeGravityFromOrientation`, `computeQuaternionFromOrientationReading`, `computeRotationMatrixFromQuaternion`, `computeScreenRelativeOrientation`, `computeWorldAccelerationFromDeviceAcceleration`, `createAmbientLightReading`, `createMotionReading`, `createOrientationReading`, `createPressureReading`, `createProximityReading` (+25 more describe blocks in the file) |
| `src/sensorsHost.test.ts` | 5 | `getSensorsPermissionState`, `hasAccelerometer`, `hasBarometer`, `isSensorsSupported` |

Upstream carries per-package records at `flight/agents/packages/sensors/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/geolocation`

Device geolocation (current position, watch, permission) over a swappable web/native backend

4 upstream modules under `packages/geolocation/src`.
 Depends on `@flighthq/entity`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/geolocation.test.ts` | 8 | `clearGeolocationWatch`, `createGeolocationPosition`, `getCurrentGeolocationPosition`, `getCurrentGeolocationPositionResult`, `initializeGeolocationPosition`, `isGeolocationAvailable`, `watchGeolocationPosition` |
| `src/geolocationAccess.test.ts` | 5 | `promptForGeolocationAccess` |

Upstream carries per-package records at `flight/agents/packages/geolocation/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

4 of 4 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/haptics`

Haptic feedback (vibrate, impact, notification, selection) over a swappable web/native backend

3 upstream modules under `packages/haptics/src`.
 Depends on `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/haptics.test.ts` | 19 | `cancelDeviceVibration`, `getHapticsCapabilities`, `isHapticsSupported`, `prepareHaptics`, `triggerHapticImpact`, `triggerHapticNotification`, `triggerHapticSelection`, `vibrateDevice`, `vibrateDevicePattern`, `vibrateDeviceWaveform` |

Upstream carries per-package records at `flight/agents/packages/haptics/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/device`

Device identity, hardware, and safe-area insets over a swappable web/native backend

3 upstream modules under `packages/device/src`.
 Depends on `@flighthq/entity`, `@flighthq/types`, `@flighthq/useragent`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/device.test.ts` | 12 | `createDeviceCapabilities`, `createDeviceDisplayMetrics`, `createDeviceInfo`, `createSafeAreaInsets`, `getDeviceCapabilities`, `getDeviceDisplayMetrics`, `getDeviceId`, `getDeviceInfo`, `getSafeAreaInsets`, `R3 boundary`, `refreshDeviceInfo` |

Upstream carries per-package records at `flight/agents/packages/device/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/screen`

Host-explicit display enumeration, geometry, work area, scale factor, and display events

3 upstream modules under `packages/screen/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/screen.test.ts` | 46 | … `attachScreenPermissionChange`, `attachScreenSignals`, `createScreenInfo`, `createScreenMode`, `createScreenPermissionChange`, `createScreenSignals`, `detachScreenPermissionChange`, `detachScreenSignals`, `dipToScreenPoint`, `dipToScreenRect`, `disposeScreenPermissionChange`, `disposeScreenSignals` (+19 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/screen/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/power`

Battery level, charging state, low-power signals and screen keep-awake over a swappable web/native backend

3 upstream modules under `packages/power/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/power.test.ts` | 31 | … `acquirePowerKeepAwake`, `attachPower`, `createPower`, `destroyPowerKeepAwake`, `detachPower`, `disposePower`, `enablePowerSignals`, `getPowerBatteryHealth`, `getPowerIdlePollingIntervalMs`, `getPowerStatus`, `getPowerSystemIdleState`, `getPowerSystemIdleTime` (+7 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/power/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

2 of 3 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/clipboard`

System clipboard operations over explicit Host capability vectors for text, images, formats, bookmarks, and change subscriptions

3 upstream modules under `packages/clipboard/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/clipboard.test.ts` | 31 | … `attachClipboardWatch`, `clearClipboard`, `clipboard`, `ClipboardWatch`, `createClipboardWatch`, `detachClipboardWatch`, `disposeClipboardWatch`, `getClipboardFormats`, `hasClipboardBookmark`, `hasClipboardFormat`, `hasClipboardHtml`, `hasClipboardImage` (+17 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/clipboard/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/dialog`

Host-explicit open/save file, message, confirm, and prompt dialog operations

5 upstream modules under `packages/dialog/src`.
 Depends on `@flighthq/entity`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/dialog.test.ts` | 8 | `showConfirmDialog`, `showErrorBox`, `showErrorDialog`, `showInfoDialog`, `showMessageDialog`, `showPromptDialog`, `showWarningDialog` |
| `src/fileDialog.test.ts` | 10 | `createFileDialogHandle`, `getFileDialogHandleOperations`, `initializeFileDialogHandle`, `showOpenDirectoryDialog`, `showOpenFileDialog`, `showSaveFileDialog` |
| `src/mediaDialog.test.ts` | 5 | `showCapturePhotoDialog`, `showCaptureVideoDialog`, `showOpenImageDialog` |

Upstream carries per-package records at `flight/agents/packages/dialog/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_dialog` — `tests/generated_dialog_test.cpp` (dedicated)

### Local generation status

4 of 5 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `dialog-file-handle-runtime-authority`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/menu`

Native application and context menus over a swappable web/native backend

4 upstream modules under `packages/menu/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/menu-templates.test.ts` | 18 | `createDefaultAppMenuTemplate`, `createDefaultEditMenuTemplate`, `createDefaultFileMenuTemplate`, `createDefaultHelpMenuTemplate`, `createDefaultViewMenuTemplate`, `createDefaultWindowMenuTemplate` |
| `src/menu.test.ts` | 40 | … `attachMenuHighlight`, `attachMenuSelect`, `cloneMenuTemplate`, `createMenuHighlight`, `createMenuItemTemplate`, `createMenuSelect`, `destroyAppMenu`, `detachMenuHighlight`, `detachMenuSelect`, `disposeMenuHighlight`, `disposeMenuSelect`, `enableMenuSignals` (+6 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/menu/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

Two dedicated tests. One was already here and is extended by one case; the other is new, because the emitted
half of the package had no local coverage at all.

- `flight_cpp.generated_shortcut` — `tests/generated_shortcut_test.cpp`, PRE-EXISTING (commit `a68224c7`) and left
  otherwise intact: it ports the explicit-dependency lifecycle suite against the override that supplies that
  module — create (normalized accelerator, its own signal, parse failure), attach (attached / already-attached /
  already-registered / native-refused / trigger-provider-failed), the provider trigger reaching the shortcut's
  signal, detach (creator-pinned provider, a failed release retained for an exact retry), dispose, the query pair's
  parse-before-ask, and destroy. This unit changed two things in it, both additive. First, its header recorded one
  upstream case as unported — "reports an in-progress same-chord acquisition without invoking the provider twice",
  which it judged to need "a hand-rolled unsettled task rather than a port"; `flight::Task::create` builds exactly
  that deferred task, so the case is now ported directly and the note replaced. Second, its dispose block asserted
  that the consumer signal was cleared, but nothing had ever been CONNECTED to that signal in the block, so the
  signal carried no data either way and the assertion held whether or not dispose cleared anything — a mutation
  that drops `clearSignal` from dispose was undetected. The block now connects a listener, asserts the signal is
  live while attached, and only then asserts it is cleared, so the clearing is the thing actually under test.
- `flight_cpp.generated_shortcut_accelerator` — `tests/generated_shortcut_accelerator_test.cpp`, NEW. The emitted
  accelerator module (the other 13 upstream cases) had no local test of any kind: chord parsing, alias resolution
  and canonical key naming, the input-independent normalized modifier order, the separator rule that keeps
  `CommandOrControl++` and `CommandOrControl+-` parseable as the Plus and Minus keys, the out-object contract
  (cleared and refilled in place and returned by identity; untouched when the parse fails), the per-platform
  display form with CommandOrControl resolved before the label, the label tables, equality and conflict, and every
  rejection reason with the token that caused it.

Mutations, both modules, against the override that supplies the lifecycle and against the emitted accelerator
header. Accelerator module: seven mutations, all seven with teeth — the canonical modifier order with Alt and
Shift transposed 2 failures, a separator that always separates 5, `copyParsed` not clearing `out` 1, the conflict
probe answering with the query's spelling 1, equality comparing raw input 3, the macOS display using a separator
5, and the modifier read clearing `out` before it knows the parse failed 1 — plus one observationally equivalent
mutation, the early return in `formatNormalized` removed, which changes nothing because joining a one-element
array with `'+'` yields that element. Lifecycle module: eight mutations, seven with teeth — the already-registered
guard dropped 2, the subscribe arm inverted 18, detach using the passed provider instead of the pinned one 4,
detach erasing before the release 4, dispose keeping the signal 1 (only after the fix above), the query not
parsing first 1, and the pending guard dropped, which is detected as a HANG rather than as a failed check because
the mutant subscribes twice for one chord and never settles — plus one observationally equivalent mutation, the
ownership guard around the accelerator erase removed, which cannot fail because the registration map only ever
maps a chord to the shortcut that owns it.

### Local generation status

3 of 4 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `menu-recursive-template-validation`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/tray`

Tray icon Entities over explicitly injected operation providers

4 upstream modules under `packages/tray/src`.
 Depends on `@flighthq/entity`, `@flighthq/log`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/enableTrayGuards.test.ts` | 2 | `disableTrayGuards`, `enableTrayGuards` |
| `src/tray.test.ts` | 29 | … `createTrayIcon`, `destroyTrayIcon`, `displayTrayBalloon`, `getTrayIconBounds`, `getTrayIcons`, `getTrayIconTitle`, `getTrayIconTooltip`, `initializeTrayCreateFailedResult`, `initializeTrayCreateProviderFailureResult`, `initializeTrayCreateSuccessResult`, `isTrayDestroyed`, `isTrayIconAnimating` (+16 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/tray/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 4 modules emitted, 1 refused; 4 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/statusbar`

Mobile status bar style, visibility, and color over a swappable web/native backend

3 upstream modules under `packages/statusbar/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/statusbar.test.ts` | 18 | … `attachStatusBar`, `clearStatusBarStyleStack`, `createStatusBar`, `createStatusBarInfo`, `detachStatusBar`, `disposeStatusBar`, `getStatusBarHeight`, `getStatusBarInfo`, `hasStatusBarStyleEntry`, `initializeStatusBar`, `initializeStatusBarInfo`, `packedRgbaToHexColor` (+6 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/statusbar/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_statusbar` — `tests/generated_statusbar_test.cpp` (dedicated)

### Local generation status

2 of 3 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `statusbar-complete-module`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/notification`

System notifications over a swappable web (Notification API) / native backend

3 upstream modules under `packages/notification/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/notification.test.ts` | 38 | … `attachNotificationActionSubscription`, `attachNotificationClickSubscription`, `attachNotificationDismissSubscription`, `attachNotificationReceivedSubscription`, `attachNotificationReplySubscription`, `bindNotificationClose`, `bindScheduledNotificationCancel`, `cancelScheduledNotification`, `closeAllNotifications`, `closeNotification`, `createNotificationActionSubscription`, `createNotificationClickSubscription` (+24 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/notification/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

2 of 3 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `notification-subscription-lifecycle`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/share`

Explicit host share-sheet commands for content and portable files

3 upstream modules under `packages/share/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/share.test.ts` | 13 | … `attachShareSignals`, `canShareContent`, `canShareFiles`, `detachShareSignals`, `disposeShareSignals`, `enableShareSignals`, `hasShareContentFields`, `initializeShareSignals`, `isShareFileValid`, `Share contract surface`, `shareContent`, `shareContentWithResult` (+3 more describe blocks in the file) |
| `src/shareExplicit.test.ts` | 1 | `explicit Share host isolation` |

Upstream carries per-package records at `flight/agents/packages/share/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_share` — `tests/generated_share_test.cpp` (dedicated)

### Local generation status

2 of 3 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `share-complete-module`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/shortcut`

Global OS hotkey registration over a swappable web/native backend

4 upstream modules under `packages/shortcut/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/shortcut.test.ts` | 13 | … `equalsAccelerator`, `findAcceleratorConflict`, `formatAcceleratorForDisplay`, `getAcceleratorKey`, `getAcceleratorKeyLabel`, `getAcceleratorModifierLabel`, `getAcceleratorModifiers`, `isAcceleratorValid`, `makeParsedAccelerator`, `normalizeAccelerator`, `parseAccelerator`, `parseAcceleratorDetailed` (+1 more describe blocks in the file) |
| `src/shortcutExplicitDependency.test.ts` | 14 | `attachGlobalShortcut`, `createGlobalShortcut`, `destroyShortcutTrigger`, `detachGlobalShortcut`, `disposeGlobalShortcut`, `initializeGlobalShortcut`, `queryGlobalShortcutConflict`, `queryGlobalShortcutRegistration` |

Upstream carries per-package records at `flight/agents/packages/shortcut/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 4 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `shortcut-explicit-dependency-structural-assertions`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/shell`

OS shell commands over seven explicit Host capability slots with required external URL policy

3 upstream modules under `packages/shell/src`.
 Depends on `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/shell.test.ts` | 14 | `isShellUrlAllowed`, `moveShellItemsToTrash`, `moveShellItemToTrash`, `openShellExternalUrl`, `openShellPath`, `readShellShortcutLink`, `revealShellPath`, `shellBeep`, `spawnShellProcess`, `writeShellShortcutLink` |

Upstream carries per-package records at `flight/agents/packages/shell/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/ipc`

Inter-process messaging over explicit operation-tight host capabilities

3 upstream modules under `packages/ipc/src`.
 Depends on `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/ipc.test.ts` | 12 | `invokeIpc`, `onceIpcMessage`, `onIpcInvoke`, `onIpcMessage`, `sendIpcMessage`, `sendIpcMessageTo` |

Upstream carries per-package records at `flight/agents/packages/ipc/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/protocol`

Custom URI scheme registration and deep-link open-URL signals over a swappable web/native backend

3 upstream modules under `packages/protocol/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/protocol.test.ts` | 20 | … `attachProtocolHandler`, `createProtocolHandler`, `createProtocolUrl`, `detachProtocolHandler`, `disposeProtocolHandler`, `getProtocolLaunchUrl`, `getRegisteredProtocolSchemes`, `initializeProtocolHandler`, `isProtocolSchemeDefault`, `isProtocolSchemeRegistered`, `isValidProtocolScheme`, `parseProtocolUrl` (+6 more describe blocks in the file) |
| `src/protocolHost.test.ts` | 3 | `protocol explicit Host ownership` |

Upstream carries per-package records at `flight/agents/packages/protocol/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/permissions`

Explicit-Host permission query/request facade over capability-owned permission seams

3 upstream modules under `packages/permissions/src`.
 Depends on `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/geolocationDelegation.test.ts` | 9 | `requestPermission` |
| `src/getPermissionState.test.ts` | 5 | `getPermissionState` |
| `src/getPermissionStates.test.ts` | 6 | `getPermissionStates` |
| `src/permission.test.ts` | 3 | `getPermissionState`, `getPermissionStates`, `requestPermission` |
| `src/requestPermission.test.ts` | 8 | `requestPermission` |

Upstream carries per-package records at `flight/agents/packages/permissions/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/filesystem`

Read/write files and directories over a swappable web (OPFS) / native backend

3 upstream modules under `packages/filesystem/src`.
 Depends on `@flighthq/dialog`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/filesystem.test.ts` | 48 | … `appendTextFile`, `canAccessFile`, `copyFile`, `createFileSymlink`, `directoryExists`, `fileExists`, `findFiles`, `getFileBaseName`, `getFileDirectoryName`, `getFileExtensionName`, `getFilePermissions`, `getFileRealPath` (+28 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/filesystem/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_dialog` — `tests/generated_dialog_test.cpp` (incidental: includes the package as part of a larger closure)

### Local generation status

2 of 3 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `filesystem-module-indexed-abort-event-map`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/preferences`

Synchronous key/value persistence over explicit Host capabilities

4 upstream modules under `packages/preferences/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/storage.test.ts` | 43 | … `attachStorage`, `clearStorage`, `clearStorageNamespace`, `createStorageSignals`, `destroyStorage`, `detachStorage`, `disposeStorage`, `getNamespacedStorageByteSize`, `getNamespacedStorageEntries`, `getNamespacedStorageItem`, `getNamespacedStorageItemPresence`, `getNamespacedStorageKeys` (+25 more describe blocks in the file) |
| `src/storagePersistence.test.ts` | 2 | `getStoragePersistence`, `requestStoragePersistence` |

Upstream carries per-package records at `flight/agents/packages/preferences/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 4 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `preferences-storage-outcome-dispatch`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/updater`

Awaited application update checks and provider-pinned downloaded-update installation transactions

3 upstream modules under `packages/updater/src`.
 Depends on `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/updater.test.ts` | 6 | `checkForAppUpdate`, `destroyUpdater`, `installDownloadedUpdate` |

Upstream carries per-package records at `flight/agents/packages/updater/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_updater` — `tests/generated_updater_test.cpp` (dedicated)

### Local generation status

2 of 3 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `updater-complete-module`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/useragent`

Pure user-agent string parsing to device and platform identity tokens.

4 upstream modules under `packages/useragent/src`.
 Depends on `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/userAgent.test.ts` | 72 | … `detectEndianness`, `legacy EdgeHTML`, `parseUserAgentArch`, `parseUserAgentEngine`, `parseUserAgentEngine for Opera on iOS`, `parseUserAgentEngine on iOS`, `parseUserAgentEngineVersion`, `parseUserAgentKind`, `parseUserAgentName`, `parseUserAgentPointerWidth`, `parseUserAgentRuntime`, `parseUserAgentVersion` (+1 more describe blocks in the file) |
| `src/userAgentParse.test.ts` | 32 | `parseUserAgentFormFactor`, `parseUserAgentFormFactor desktop-mode iPad`, `parseUserAgentOsName`, `parseUserAgentOsVersion` |

Upstream carries per-package records at `flight/agents/packages/useragent/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_useragent` — `tests/generated_useragent_test.cpp` (dedicated)

### Local generation status

3 of 4 modules emitted, 1 refused; 4 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `useragent-platform-parsers`, `useragent-os-and-form-factor-parsers`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/connectivity`

Explicit connectivity status, change signals, and reachability operations

3 upstream modules under `packages/connectivity/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/connectivity.test.ts` | 17 | `attachConnectivity`, `createConnectivity`, `destroyConnectivity`, `detachConnectivity`, `detectConnectivityReachability`, `disposeConnectivity`, `getConnectivityOnline`, `getConnectivityStatus`, `hasConnectivityStatusChanged`, `initializeConnectivity`, `isConnectivityMetered`, `isConnectivitySaveDataEnabled` |

Upstream carries per-package records at `flight/agents/packages/connectivity/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_connectivity` — `tests/generated_connectivity_test.cpp` (dedicated)

### Local generation status

2 of 3 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `connectivity-complete-module`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## Lane notes

### Upstream examples

6 of the 34 upstream examples are ported and smoke-tested.
 None of the ported ones belongs to this lane.

A further 3 examples are in that inventory whose subject is a package in this lane, and are listed as awaiting a faithful SDL port: `interaction`, `sound`, `video`. The mapping is by example name, or by an import of the package where the name differs (`flowstates` imports `@flighthq/flow`); the 30 other examples whose imports go through the `@flighthq/sdk` barrel are not attributed to any lane here.

This lane's largest upstream suites are `interaction` (212 cases), `app` (201 cases), `host` (141 cases).
None of them has an example port behind it; whatever local coverage the lane has is per package, and is listed below.

### Upstream functional scenes

Upstream also asserts behaviour visually, as image-diff cells under `flight/functional/scenes/`
(383 scene files at the pin). No scene takes a package in this lane as its subject, so
there is no upstream image-diff evidence about one.

Some scene files do import a lane package directly, always incidentally — for a type or a
helper the renderer scene needs, never as its subject:

- `@flighthq/app` — 5 files (`application-render-view.webgl.ts`, `render-pass-shared-context.webgl.ts`, `render-pass-viewport.webgl.ts`)

### Local substrate shared by every lane

These CTest executables are the repository's own parity surface. They cover the language
runtime and the emitted-SDK contracts rather than any one lane package, and they are listed
here so that a missing lane CTest is not read as an absence of local evidence about the runtime
the lane's emitted code calls.

| CTest | source | what it covers |
| --- | --- | --- |
| `flight_cpp.runtime` | `tests/runtime_test.cpp` | `Array`, `Map`, `Set`, `String`, `Number`, `Date`, `RegExp`, `Task`, `Blob`, typed arrays, `Any`, structural rows, Intl carriers |
| `flight_cpp.runtime_conformance` | `tests/runtime_conformance_test.cpp` + `conformance/portable-typescript-v1.json` | the committed profile semantics |
| `flight_cpp.generated_runtime` | `tests/generated_runtime_test.cpp` + `tests/generated/semantic_runtime.ts` | semantics of compiled-from-TypeScript containers, closures, and iteration |
| `flight_cpp.structural_row` | `tests/structural_row_test.cpp` | structural rows and the named-property view |
| `flight_cpp.c_api`, `flight_cpp.c_api_threads` | `tests/c_api_test.c`, `tests/c_api_thread_test.cpp` | the `Flight::C` foreign ABI |
| header self-containment | `tests/CMakeLists.txt` `flight_cpp_public_headers` | each installed `flight/*.hpp` compiles alone |
| `flight_cpp.host_sdl` | `tests/host_sdl_test.cpp` | the SDL host seam |
| `flight_cpp.upstream_math_geometry_parity` | `tests/generated/upstream_math_geometry_parity.cpp`, `tests/upstream_test_harness.hpp` | a pilot that lowers `@flighthq/math` and `@flighthq/geometry` upstream tests to native C++ (18 tests); it is the first local CTest sourced from the upstream suites this document inventories |

Outside CTest, `scripts/*Oracle.mjs` (`npm run <surface>:oracle`) compares individual runtime
surfaces against Node directly. Those are gates about the language runtime, not tests of a lane
package.
