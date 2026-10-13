# ANIM lane — upstream parity inventory

The ANIM lane is animation and simulation — timelines, tweening, springs, particles, 2D skeletal playback, and the 2D physics and collision layers. This document is an inventory, not a scorecard: it
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

- `@flighthq/animation`
- `@flighthq/timeline`
- `@flighthq/tween`
- `@flighthq/spring`
- `@flighthq/easing`
- `@flighthq/velocity`
- `@flighthq/motionpath`
- `@flighthq/clock`
- `@flighthq/movieclip`
- `@flighthq/swf`
- `@flighthq/skeleton2d`
- `@flighthq/skeleton2d-formats`
- `@flighthq/particles`
- `@flighthq/particleemitter`
- `@flighthq/particles-formats`
- `@flighthq/flow`
- `@flighthq/statechart`
- `@flighthq/physics2d`
- `@flighthq/physics2d-abi`
- `@flighthq/collision`
- `@flighthq/spatial`

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
| `@flighthq/animation` | 13 | 11 | 144 | dedicated | none | 12 / 1 |
| `@flighthq/timeline` | 3 | 1 | 63 | none | none | 3 / 0 |
| `@flighthq/tween` | 10 | 7 | 121 | none | none | 7 / 3 |
| `@flighthq/spring` | 6 | 4 | 45 | none | none | 6 / 0 |
| `@flighthq/easing` | 23 | 22 | 165 | none | none | 22 / 1 |
| `@flighthq/velocity` | 5 | 3 | 58 | none | none | 4 / 1 |
| `@flighthq/motionpath` | 3 | 1 | 29 | dedicated | none | 3 / 0 |
| `@flighthq/clock` | 4 | 2 | 37 | dedicated | none | 4 / 0 |
| `@flighthq/movieclip` | 5 | 3 | 60 | dedicated | none | 4 / 1 |
| `@flighthq/swf` | 58 | 51 | 441 | dedicated | none | 46 / 12 |
| `@flighthq/skeleton2d` | 25 | 22 | 220 | dedicated | none | 19 / 6 |
| `@flighthq/skeleton2d-formats` | 59 | 60 | 533 | none | none | 10 / 5 |
| `@flighthq/particles` | 13 | 11 | 119 | none | none | 12 / 1 |
| `@flighthq/particleemitter` | 12 | 12 | 199 | none | none | 7 / 5 |
| `@flighthq/particles-formats` | 18 | 15 | 274 | none | none | 11 / 6 |
| `@flighthq/flow` | 4 | 2 | 32 | none | none | 4 / 0 |
| `@flighthq/statechart` | 5 | 3 | 39 | dedicated | none | 5 / 0 |
| `@flighthq/physics2d` | 26 | 25 | 457 | none | none | 21 / 5 |
| `@flighthq/physics2d-abi` | 8 | 6 | 85 | none | none | 6 / 2 |
| `@flighthq/collision` | 38 | 37 | 500 | dedicated | none | 32 / 6 |
| `@flighthq/spatial` | 11 | 9 | 148 | dedicated | none | 10 / 1 |

`upstream modules` counts the `.ts` files under `packages/<name>/src`; the generated manifest
counts its own `sourceModules`, and the two differ where declaration files or generated outputs
are involved.

**21** packages, **307** upstream test files, **3769** cases.
6 of 21 packages are reached by at least one local CTest executable.
6 by a test dedicated to the package: `animation`, `motionpath`, `clock`, `statechart`, `collision`, `spatial`.

## `@flighthq/animation`

Target-free animation core: keyframe sampler (step/linear/cubic, quaternion slerp), clips, and an explicit playhead

13 upstream modules under `packages/animation/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/animationAdvance.test.ts` | 1 | `advanceAnimationPlayers` |
| `src/animationBlend.test.ts` | 12 | `accumulateAnimationSample`, `addAnimationSample`, `blendAnimationSamples`, `createAnimationSampleAccumulator`, `finishAnimationSample`, `initializeAnimationSampleAccumulator`, `resetAnimationSampleAccumulator` |
| `src/animationBlendTree.test.ts` | 14 | `advanceAnimationBlendTree`, `createAnimationBlendTree`, `createAnimationBlendTreeInput`, `initializeAnimationBlendTree`, `initializeAnimationBlendTreeInput`, `sampleAnimationBlendTree`, `sampleAnimationBlendTreeChannel`, `setAnimationBlendTreeInputWeight` |
| `src/animationClip.test.ts` | 13 | `cloneAnimationClip`, `createAnimationChannel`, `createAnimationClip`, `createAnimationClipEvent`, `getAnimationClipDuration`, `initializeAnimationChannel`, `initializeAnimationClip`, `initializeAnimationClipEvent`, `sampleAnimationClip` |
| `src/animationCrossfade.test.ts` | 15 | `advanceAnimationCrossfade`, `createAnimationCrossfade`, `initializeAnimationCrossfade`, `isAnimationCrossfadeComplete`, `sampleAnimationCrossfade` |
| `src/animationLayerStack.test.ts` | 10 | `advanceAnimationLayerStack`, `createAnimationBlendTreeLayer`, `createAnimationLayerStack`, `createAnimationStateMachineLayer`, `initializeAnimationLayerStack`, `sampleAnimationLayerStack`, `sampleAnimationLayerStackChannel`, `setAnimationLayerWeight` |
| `src/animationPlayer.test.ts` | 29 | `advanceAnimationPlayer`, `cloneAnimationPlayer`, `createAnimationPlayer`, `enableAnimationPlayerSignals`, `getAnimationPlayerNormalizedTime`, `initializeAnimationPlayer`, `playAnimationPlayer`, `seekAnimationPlayer`, `stopAnimationPlayer` |
| `src/animationRootMotion.test.ts` | 7 | `createAnimationRootMotionExtractor`, `extractAnimationRootMotion`, `initializeAnimationRootMotionExtractor` |
| `src/animationStateMachine.test.ts` | 13 | `advanceAnimationStateMachine`, `createAnimationStateMachine`, `createAnimationStateMachineState`, `getAnimationStateMachineCurrentState`, `initializeAnimationStateMachine`, `initializeAnimationStateMachineState`, `isAnimationStateMachineTransitioning`, `sampleAnimationStateMachine`, `sampleAnimationStateMachineChannel`, `transitionAnimationStateMachine` |
| `src/animationStateMachineAdvance.test.ts` | 1 | `advanceAnimationStateMachineWithScratch` |
| `src/animationTrack.test.ts` | 29 | `cloneAnimationTrack`, `createAnimationTrack`, `initializeAnimationTrack`, `sampleAnimationTrack`, `trimAnimationTrack`, `validateAnimationTrack` |

Upstream carries per-package records at `flight/agents/packages/animation/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_animation` — `tests/generated_animation_test.cpp` (dedicated)

### Local generation status

12 of 13 modules emitted, 1 refused; 9 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `animation-track-complete-sampling-and-copy`, `animation-blend-tree-array-structural-ref-mismatch`, `animation-crossfade-channels-array-structural-ref-mismatch`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/timeline`

Timeline-based animation sequencing

3 upstream modules under `packages/timeline/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/timeline.test.ts` | 63 | … `addTimelineFrameScript`, `clearTimelineFrameScripts`, `createTimeline`, `createTimelineSource`, `disposeTimelineSignals`, `enableTimelineSignals`, `findTimelineLabel`, `getTimelineCurrentLabel`, `getTimelineFrameScript`, `getTimelineFrameScriptFrames`, `getTimelineLabels`, `gotoAndPlayTimeline` (+9 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/timeline/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/tween`

Tween animation system with easing functions

10 upstream modules under `packages/tween/src`.
 Depends on `@flighthq/easing`, `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/colorTween.test.ts` | 7 | `createColorTween` |
| `src/timer.test.ts` | 5 | `createTweenTimer` |
| `src/tween.test.ts` | 40 | … `applyTween`, `createTween`, `getActiveTweenCount`, `getTweensOf`, `hasTweensOf`, `killTweensOfProperty`, `pauseAllTweens`, `pauseTween`, `pauseTweens`, `resetAllTweens`, `resumeAllTweens`, `resumeTween` (+4 more describe blocks in the file) |
| `src/tweenManager.test.ts` | 7 | `createTweenManager`, `defaultEase`, `defaultManager`, `initializeTweenManager` |
| `src/tweenProgress.test.ts` | 22 | `getTweenProgress`, `invalidateTween`, `restartTween`, `seekTween`, `setTweenProgress` |
| `src/tweenStagger.test.ts` | 10 | `createTweenStagger` |
| `src/updateTweens.test.ts` | 30 | `completeTween`, `smartRotation`, `updateTweens` |

Upstream carries per-package records at `flight/agents/packages/tween/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

7 of 10 modules emitted, 3 refused; 5 refusing modules (3 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/spring`

Spring-physics animation — plain-data damped-harmonic springs (frequency + damping-ratio config, physical stiffness/damping/mass constructor) stepped by a numerically stable analytic integrator that stays finite for stiff springs at any deltaTime; scalar springs plus componentwise 2D/3D vector springs, settle detection, and reset

6 upstream modules under `packages/spring/src`.
 Depends on `@flighthq/entity`, `@flighthq/math`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/spring.test.ts` | 25 | `addSpringImpulse`, `createSpring`, `initializeSpring`, `isSpringSettled`, `resetSpring`, `updateSpring`, `updateSpringAngle` |
| `src/spring2D.test.ts` | 8 | `addSpringImpulse2D`, `createSpring2D`, `initializeSpring2D`, `isSpring2DSettled`, `resetSpring2D`, `updateSpring2D` |
| `src/spring3D.test.ts` | 7 | `addSpringImpulse3D`, `createSpring3D`, `initializeSpring3D`, `isSpring3DSettled`, `resetSpring3D`, `updateSpring3D` |
| `src/springConfig.test.ts` | 5 | `createSpringConfig`, `createSpringConfigFromPhysical`, `spring presets` |

Upstream carries per-package records at `flight/agents/packages/spring/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

6 of 6 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/easing`

Easing functions for animation

23 upstream modules under `packages/easing/src`.
 Depends on `@flighthq/log`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/createEasingSamples.test.ts` | 9 | `createEasingSamples` |
| `src/easeBack.test.ts` | 5 | `easeInBack`, `easeInOutBack`, `easeOutBack` |
| `src/easeBounce.test.ts` | 5 | `easeInBounce`, `easeInOutBounce`, `easeOutBounce` |
| `src/easeCircular.test.ts` | 7 | `easeInCircular`, `easeInOutCircular`, `easeOutCircular` |
| `src/easeCombinators.test.ts` | 21 | `easeClamp`, `easeClampOutput`, `easeInvert`, `easeMirror`, `easeReverse`, `easeScaleOutput` |
| `src/easeCubic.test.ts` | 6 | `easeInCubic`, `easeInOutCubic`, `easeOutCubic` |
| `src/easeCubicBezier.test.ts` | 6 | `easeCubicBezier` |
| `src/easeDampedSine.test.ts` | 12 | `easeInDampedSine`, `easeInOutDampedSine`, `easeOutDampedSine` |
| `src/easeElastic.test.ts` | 5 | `easeInElastic`, `easeInOutElastic`, `easeOutElastic` |
| `src/easeExponential.test.ts` | 6 | `easeInExponential`, `easeInOutExponential`, `easeOutExponential` |
| `src/easeLinear.test.ts` | 3 | `easeLinear` |
| `src/easePiecewise.test.ts` | 8 | `easePiecewise` |
| `src/easePower.test.ts` | 12 | `easeInOutPower`, `easeInPower`, `easeOutPower` |
| `src/easeQuadratic.test.ts` | 6 | `easeInOutQuadratic`, `easeInQuadratic`, `easeOutQuadratic` |
| `src/easeQuartic.test.ts` | 6 | `easeInOutQuartic`, `easeInQuartic`, `easeOutQuartic` |
| `src/easeQuintic.test.ts` | 6 | `easeInOutQuintic`, `easeInQuintic`, `easeOutQuintic` |
| `src/easeSine.test.ts` | 6 | `easeInOutSine`, `easeInSine`, `easeOutSine` |
| `src/easeSmoothstep.test.ts` | 14 | `easeSmootherstep`, `easeSmoothstep`, `easeSmoothstepRange` |
| `src/easeSteps.test.ts` | 9 | `easeSteps`, `setEasingStepsGuard` |
| `src/easingFamilies.test.ts` | 1 | `easing family distinctness`, `easing family endpoints`, `easing family halves`, `easing family reflection`, `easing family shape` |
| `src/enableEasingGuards.test.ts` | 3 | `disableEasingGuards`, `enableEasingGuards` |
| `src/getEasingDerivative.test.ts` | 9 | `getEasingDerivative` |

Upstream carries per-package records at `flight/agents/packages/easing/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

22 of 23 modules emitted, 1 refused; 4 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `easing-create-easing-samples`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/velocity`

Generic per-node velocity field and contributors (transform-delta baseline + explicit overrides)

5 upstream modules under `packages/velocity/src`.
 Depends on `@flighthq/entity`, `@flighthq/geometry`, `@flighthq/node`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/transformVelocity.test.ts` | 6 | `contributeTransformVelocity` |
| `src/velocityField.test.ts` | 46 | … `addVelocity`, `beginVelocityFrame`, `clampVelocity`, `contributeVelocity`, `copyVelocity`, `createVelocityField`, `dampVelocity`, `ensureVelocitySample`, `explainVelocity`, `getVelocity`, `hasVelocity`, `initializeVelocityField` (+8 more describe blocks in the file) |
| `src/velocitySample.test.ts` | 6 | `getVelocitySampleAt` |

Upstream carries per-package records at `flight/agents/packages/velocity/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

4 of 5 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/motionpath`

Path-following animation — advances a marker along a @flighthq/path by arc length at a controlled speed and reports its world position, tangent, and heading angle, with clamp/loop/pingpong end behaviors and distance/progress seek; plain-data MotionPath state stepped by small out-param functions over path's arc-length sampling

3 upstream modules under `packages/motionpath/src`.
 Depends on `@flighthq/entity`, `@flighthq/geometry`, `@flighthq/path`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/motionPath.test.ts` | 29 | `createMotionPath`, `getMotionPathHeading`, `getMotionPathPosition`, `getMotionPathProgress`, `initializeMotionPath`, `isMotionPathAtEnd`, `setMotionPathDirection`, `setMotionPathDistance`, `setMotionPathProgress`, `updateMotionPath` |

Upstream carries per-package records at `flight/agents/packages/motionpath/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp_upstream_motionpath_smoke` — `examples/upstream/motionpath.cpp` (dedicated)

### Local generation status

3 of 3 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/clock`

Hierarchical, pausable, scalable time primitive for time-driven systems

4 upstream modules under `packages/clock/src`.
 Depends on `@flighthq/entity`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/clock.test.ts` | 35 | … `addClockChild`, `advanceClock`, `createChildClock`, `createClock`, `disposeClock`, `getClockEffectiveScale`, `getClockParent`, `initializeClock`, `isClockEffectivelyPaused`, `pauseClock`, `removeClockChild`, `resetClock` (+2 more describe blocks in the file) |
| `src/clockSignals.test.ts` | 2 | `enableClockSignals` |

Upstream carries per-package records at `flight/agents/packages/clock/`: `status.md`, `assessment.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp_upstream_clock_smoke` — `examples/upstream/clock.cpp` (dedicated)

### Local generation status

4 of 4 modules emitted, 0 refused; 0 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/movieclip`

MovieClip display-object composition layer over @flighthq/timeline

5 upstream modules under `packages/movieclip/src`.
 Depends on `@flighthq/entity`, `@flighthq/log`, `@flighthq/scene2d`, `@flighthq/node`, `@flighthq/timeline`, `@flighthq/textureatlas`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/enableMovieClipGuards.test.ts` | 3 | `areMovieClipGuardsEnabled`, `disableMovieClipGuards`, `enableMovieClipGuards` |
| `src/movieClip.test.ts` | 50 | … `addMovieClipFrameScript`, `createMovieClip`, `createMovieClipData`, `createMovieClipRuntime`, `disposeMovieClipSignals`, `enableMovieClipSignals`, `getMovieClipCurrentFrame`, `getMovieClipCurrentLabel`, `getMovieClipFrameScript`, `getMovieClipRuntime`, `getMovieClipSignals`, `getMovieClipTotalFrames` (+11 more describe blocks in the file) |
| `src/spritesheetTimelineSource.test.ts` | 7 | `createSpritesheetTimelineSource`, `explainSpritesheetTimelineSource`, `initializeSpritesheetTimelineSource`, `setSpritesheetTimelineSourceGuard` |

Upstream carries per-package records at `flight/agents/packages/movieclip/`: `status.md`, `assessment.md`, `charter.md`.

### Existing CTest coverage

`flight_cpp.generated_movieclip` (dedicated) -- `tests/generated_movieclip_test.cpp`, which exercises the package
through its committed override `flight/movieclip/movie_clip.hpp` (status: partial) because no generated header of the
package compiles: `contract.hpp`, `_internal_index.hpp`, `enable_movie_clip_guards.hpp` and
`spritesheet_timeline_source.hpp` all stop on `use of undeclared identifier 'initialize_sprite_renderer_data'` from
`@flighthq/scene2d`'s refused `sprite.ts`. The test covers the clip as a real timeline target (which the timeline
package's own test cannot do): construction defaults and the MovieClip kind, the null-timeline no-ops and idle
defaults, source binding with the timeline targeting the clip itself and realizing frame 1, timeline reuse, playback
and seeking, labels, the frame-script registry and its firing, the constructFrame callback, and the signal group's
idempotence, re-arming and disposal. Not covered, exactly as the override records: the `createMovieClip` options form
and the two runtime accessors (`createMovieClipRuntime`, `getMovieClipRuntime`) -- 4 of the file's 50 upstream cases,
excluded by the refusal below rather than by the test's scope.

### Local generation status

4 of 5 modules emitted, 1 refused; 5 refusing modules (1 of them a refusal root). Read from the committed generated tree, not from the pin above. The root is `movieClip.ts` itself, on the closed
flattened-interface decision (`Ref<Node2DRuntime>` asserted to `Ref<MovieClipRuntime>`); the other four are cascades --
`contract.ts` and `index.ts` behind it, `enableMovieClipGuards.ts` behind `@flighthq/log`'s refused `log.ts`, and
`spritesheetTimelineSource.ts` behind `@flighthq/scene2d`'s refused `sprite.ts`. Hand-written overrides shadow
generated headers here: `movieclip-timeline-node-construction`, which is what the test above includes.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/swf`

SWF structure import into renderer-neutral Scene2D documents and named slot manifests

58 upstream modules under `packages/swf/src`.
 Depends on `@flighthq/abc`, `@flighthq/adjustments`, `@flighthq/audio`, `@flighthq/bitmap`, `@flighthq/clip`, `@flighthq/color`, `@flighthq/compression`, `@flighthq/effects`, `@flighthq/entity`, `@flighthq/geometry`, `@flighthq/image`, `@flighthq/image-codec`, `@flighthq/importdiagnostics`, `@flighthq/log`, `@flighthq/math`, `@flighthq/movieclip`, `@flighthq/node`, `@flighthq/path`, `@flighthq/requirement`, `@flighthq/scene2d`, `@flighthq/scene2d-resources`, `@flighthq/shape`, `@flighthq/text`, `@flighthq/text-markup`, `@flighthq/texture`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/enableSwfGuards.test.ts` | 3 | `areSwfGuardsEnabled`, `disableSwfGuards`, `enableSwfGuards` |
| `src/expandSwfTagHandlerDispatch.test.ts` | 4 | `expandSwfTagHandlerDispatch` |
| `src/mergeSwfParseOptions.test.ts` | 6 | `mergeSwfParseOptions` |
| `src/swfAllTagHandlers.test.ts` | 6 | `swfAllTagHandlers` |
| `src/swfAppearance.test.ts` | 15 | `EMPTY_ADJUSTMENTS`, `EMPTY_EFFECTS`, `joinSwfColorAdjustments`, `readSwfColorTransform`, `resolveSwfAdvancedBlendMode`, `resolveSwfBlendMode` |
| `src/swfBitmap.test.ts` | 9 | `createSwfJpegAlphaBitmap`, `createSwfLosslessBitmap` |
| `src/swfBitmapTagFamily.test.ts` | 2 | `swfBitmapTagFamily` |
| `src/swfBoundedDefinition.test.ts` | 6 | `readSwfBoundedDefinitionHeader` |
| `src/swfContentCapabilities.test.ts` | 8 | `collectSwfContentCapabilities` |
| `src/swfControlHandler.test.ts` | 2 | `swfControlHandler` |
| `src/swfControlTagFamily.test.ts` | 2 | `swfControlTagFamily` |
| `src/swfDefineMorphShapeHandler.test.ts` | 3 | `swfDefineMorphShapeHandler` |
| `src/swfDefineShapeHandler.test.ts` | 3 | `swfDefineShapeHandler` |
| `src/swfDocument.test.ts` | 151 | `createGlyphOutlineSourcesFromSwf`, `createScene2DFromSwf`, `createScene2DFromSwf import diagnostics`, `createScene2DFromSwf morph bounds`, `createScene2DFromSwf morph shapes`, `createScene2DImportFromSwf`, `createScene2DSymbolFromSwf`, `initializeSwfDocumentImport`, `readSwfExportedSymbolNames`, `registerSwfScene2DDocumentImporter`, `uncompressSwfSource` |
| `src/swfEditText.test.ts` | 11 | `readSwfEditTextFactory` |
| `src/swfEditTextHandler.test.ts` | 3 | `swfEditTextHandler` |
| `src/swfFilter.test.ts` | 14 | `readSwfFilterList`, `setSwfFilterListGuard` |
| `src/swfFontHandler.test.ts` | 3 | `swfFontHandler` |
| `src/swfFontTagFamily.test.ts` | 2 | `swfFontTagFamily` |
| `src/swfFrameAction.test.ts` | 10 | `readSwfAbcFrameScripts`, `readSwfFrameActions` |
| `src/swfHeader.test.ts` | 7 | `parseSwfHeader` |
| `src/swfHeaderReader.test.ts` | 7 | `readSwfHeaderRectangle`, `readSwfHeaderUint32`, `SWF_HEADER_PREFIX_LENGTH` |
| `src/swfImageDecoder.test.ts` | 3 | `decodeSwfImage` |
| `src/swfImageResources.test.ts` | 1 | `SWF image resources` |
| `src/swfImageTexture.test.ts` | 5 | `acquireSwfImageTexture` |
| `src/swfJpegBitmapHandler.test.ts` | 3 | `swfJpegBitmapHandler` |
| `src/swfKnownTags.test.ts` | 1 | `isKnownSwfTag` |
| `src/swfLosslessBitmapHandler.test.ts` | 2 | `swfLosslessBitmapHandler` |
| `src/swfMorphShape.test.ts` | 7 | `createSwfMorphShape` |
| `src/swfNode.test.ts` | 15 | `applySwfAuthoredBounds`, `applySwfMorphBounds`, `computeSwfLocalBoundsRectangle`, `createSwfDisplayObject`, `createSwfMorphShapeTarget`, `createSwfMovieClip`, `createSwfTexturedSprite` |
| `src/swfPlacementTagFamily.test.ts` | 2 | `swfPlacementTagFamily` |
| `src/swfPlaceObject3Handler.test.ts` | 2 | `swfPlaceObject3Handler` |
| `src/swfPlaceObjectHandler.test.ts` | 2 | `swfPlaceObjectHandler` |
| `src/swfPrimitive.test.ts` | 18 | `mergeSwfRectangles`, `readBigEndianUint16`, `readBigEndianUint32`, `readSwfMatrix`, `readSwfRectangle`, `resolveSwfDimensionBounds`, `transformSwfRectangle` |
| `src/swfReader.test.ts` | 6 | `SwfReader` |
| `src/swfRequirements.test.ts` | 11 | `parseSwfRequirements` |
| `src/swfScriptHandler.test.ts` | 3 | `swfScriptHandler` |
| `src/swfScriptTagFamily.test.ts` | 2 | `swfScriptTagFamily` |
| `src/swfShape.test.ts` | 21 | `createSwfGlyphShape`, `createSwfShape`, `readSwfMorphShapePaths` |
| `src/swfShapeTagFamily.test.ts` | 2 | `swfShapeTagFamily` |
| `src/swfSoundHandler.test.ts` | 5 | `initializeTimelineAudioCue`, `initializeTimelineStreamAudioCue`, `swfSoundHandler` |
| `src/swfSoundTagFamily.test.ts` | 2 | `swfSoundTagFamily` |
| `src/swfSpriteHandler.test.ts` | 3 | `swfSpriteHandler` |
| `src/swfSpriteTagFamily.test.ts` | 2 | `swfSpriteTagFamily` |
| `src/swfStaticTextHandler.test.ts` | 3 | `swfStaticTextHandler` |
| `src/swfTagCensus.test.ts` | 8 | `collectSwfTagCounts`, `getSwfTagName` |
| `src/swfText.test.ts` | 12 | `createSwfTextShape`, `readSwfFontGlyphOutlineSource`, `readSwfFontGlyphs`, `resolveSwfFontUnitsPerEm` |
| `src/swfTextTagFamily.test.ts` | 2 | `swfTextTagFamily` |
| `src/swfTimelineParse.test.ts` | 16 | `addSwfTimelineLabel`, `readSwfTimeline` |
| `src/swfVideoHandler.test.ts` | 3 | `swfVideoHandler` |
| `src/swfVideoTagFamily.test.ts` | 2 | `swfVideoTagFamily` |

Upstream carries per-package records at `flight/agents/packages/swf/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

`flight_cpp.generated_swf_reader` (dedicated) -- `tests/generated_swf_reader_test.cpp`, the native port of
`swfReader.test.ts`: the reader holds the caller's own byte buffer, starts where it was told, stops at `end` even when
the buffer continues past it, and the rest of that file's expectations.

`flight_cpp.generated_swf` (dedicated) -- `tests/generated_swf_test.cpp`, covering the reachable non-document surface
that no other native test asserted: `expandSwfTagHandlerDispatch` (every claimed tag maps to its handler, an empty
array claims nothing, and a contested tag resolves to the last handler named, in either order -- the caller-named half
of that module's cases, since the family presets it also tests are refused), `swfKnownTags` (the three authored 32-bit
masks as format facts, the code space bounded at 94, and membership agreeing with the masks across the whole range,
which is what catches an off-by-one in the emitted indexing) and `swfImageMimeType`. Mutation-checked on the additions:
four of five mutations detected, the fifth observationally equivalent because the masks already make code 95 unknown.

Not covered, because they do not compile or do not emit: everything that reads a document through `swfDocument`, and
the shape, text, bitmap, video, control, script and filter families behind it. 37 of the package's 58 modules are
refused, and of the headers that do emit, 17 of 58 compile -- the rest are blocked by dependencies outside this package
(`@flighthq/scene2d` and `@flighthq/image` four times each, `@flighthq/effects` four, `@flighthq/text-markup` two,
`@flighthq/texture` three) and by the closed flattened-interface decision at the handler call sites, all recorded in
`docs/compiler-requests/`.

### Local generation status

46 of 58 modules emitted, 12 refused; 37 refusing modules (12 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `swf-explain-portable-uncompress-boundary`, `swf-tag-registry-refused-handler-boundaries`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/skeleton2d`

2D skeleton: bone hierarchy with 2D affine transforms, world propagation, inverse-bind, and weighted CPU mesh deformation (Spine/DragonBones model)

25 upstream modules under `packages/skeleton2d/src`.
 Depends on `@flighthq/animation`, `@flighthq/entity`, `@flighthq/geometry`, `@flighthq/log`, `@flighthq/node`, `@flighthq/math`, `@flighthq/path`, `@flighthq/registry`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/applyAnimationClipToSkeleton2D.test.ts` | 14 | `applyAnimationClipToSkeleton2D`, `applyAnimationClipToSkeleton2D per-axis bone channels`, `applyAnimationClipToSkeleton2D slot channels` |
| `src/boundingBoxAttachment2D.test.ts` | 4 | `computeSkeleton2DBoundingBoxAttachmentVertices` |
| `src/clippingAttachment2D.test.ts` | 6 | `computeSkeleton2DClippingAttachmentVertices`, `getSkeleton2DClippingAttachmentSlotRange` |
| `src/deformAnimationTarget2D.test.ts` | 5 | `registerSkeleton2DDeformAnimationTarget` |
| `src/deformMeshAttachment2D.test.ts` | 8 | `deformSkeleton2DMeshAttachment` |
| `src/deformPathAttachment2D.test.ts` | 8 | `deformSkeleton2DPathAttachment` |
| `src/enableSkeleton2DGuards.test.ts` | 6 | `disableSkeleton2DGuards`, `enableSkeleton2DGuards` |
| `src/explainSkeleton2DChannel.test.ts` | 6 | `explainSkeleton2DChannelInterpolation`, `isSkeleton2DSteppedChannelSubject` |
| `src/explainSkeleton2DDeformLength.test.ts` | 7 | `explainSkeleton2DDeformLength` |
| `src/ikConstraint2D.test.ts` | 19 | `registerSkeleton2DIkConstraintSolver`, `solveSkeleton2DIkConstraint` |
| `src/pathConstraint2D.test.ts` | 15 | `registerSkeleton2DPathConstraintSolver`, `solveSkeleton2DPathConstraint` |
| `src/pointAttachment2D.test.ts` | 5 | `computeSkeleton2DPointAttachmentPosition`, `computeSkeleton2DPointAttachmentRotation` |
| `src/regionAttachment2D.test.ts` | 3 | `computeSkeleton2DRegionAttachmentVertices` |
| `src/skeleton2d.test.ts` | 36 | … `cloneSkeleton2D`, `computeSkeleton2DBoneMatrices`, `computeSkeleton2DBoneWorldTransform`, `computeSkeleton2DWorldTransforms`, `createSkeleton2D`, `disposeSkeleton2D`, `equalsSkeleton2D`, `getSkeleton2DBoneIndexByName`, `getSkeleton2DBoneWorldMatrix`, `getSkeleton2DSkin`, `initializeSkeleton2D`, `resetSkeleton2DToSetup` (+3 more describe blocks in the file) |
| `src/skeleton2dAnimationTarget.test.ts` | 14 | `createSkeleton2DBoneAnimationTarget`, `createSkeleton2DSlotAnimationTarget`, `findSkeleton2DStepKeyframe`, `getSkeleton2DAnimationTargetBinder`, `getSkeleton2DAnimationTargetBinderKinds`, `initializeSkeleton2DBoneAnimationTarget`, `initializeSkeleton2DSlotAnimationTarget`, `registerSkeleton2DAnimationTargetBinder`, `unregisterSkeleton2DAnimationTargetBinder` |
| `src/skeleton2dConstraint.test.ts` | 6 | `getSkeleton2DConstraintSolver`, `registerSkeleton2DConstraintSolver`, `solveSkeleton2DConstraints`, `unregisterSkeleton2DConstraintSolver` |
| `src/skeleton2dDrawOrderTarget.test.ts` | 18 | `createSkeleton2DDrawOrderAnimationTarget`, `createSkeleton2DDrawOrderChannel`, `initializeSkeleton2DDrawOrderAnimationTarget`, `registerSkeleton2DDrawOrderAnimationBinder`, `unregisterSkeleton2DDrawOrderAnimationBinder` |
| `src/skeleton2dGuards.test.ts` | 7 | `reportSkeleton2DCoercedInterpolation`, `reportSkeleton2DDeformLengthMismatch`, `setSkeleton2DCoercedInterpolationGuard`, `setSkeleton2DDeformLengthGuard` |
| `src/skin2D.test.ts` | 3 | `createSkin2D`, `initializeSkin2D` |
| `src/skinAttachment2DPoints.test.ts` | 10 | `skinSkeleton2DAttachmentPoints` |
| `src/slotDeform2D.test.ts` | 10 | `getSkeleton2DSlotDeformOffsets`, `setSkeleton2DSlotDeform` |
| `src/transformConstraint2D.test.ts` | 10 | `registerSkeleton2DTransformConstraintSolver`, `solveSkeleton2DTransformConstraint` |

Upstream carries per-package records at `flight/agents/packages/skeleton2d/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

`flight_cpp.generated_skeleton2d` (dedicated) -- `tests/generated_skeleton2d_test.cpp`, ported from
`skeleton2d.test.ts` and `slotDeform2D.test.ts`: construction and buffer sizing, the world-transform walk and its
inherit modes, bone lookup, cloning (bones deep-copied, slots copied, skins shared), equality, setup reset and its
aliasing guard, bind pose and the skin palette, skins, validation, disposal, and the slot deform record. Three of those
paths run through this repository's source patches and are asserted directly here: `createSkeleton2D`'s default slots
store null rather than undefined, `cloneSkeleton2D`'s slot copy keeps an attachment, and the slot deform copy owns its
offsets.

Mutation-checked in the unit that audited this row, which the test had no record of: four mutations against the
emitted `skeleton2d.hpp`. Changing the buffer stride from `matrix_stride` to 5 floats per bone fails 4 checks;
dropping the bone world transform's upper bound and widening the equality walk past the end are both caught, each as a
`std::range_error` from the out-of-range element access the guard exists to prevent. The fourth -- making the skin
palette ignore its slot-index bound -- changes nothing the test observes, and that is a gap rather than a redundancy:
no case in the file drives a skin entry whose `slot_index` is out of range, so the guard on that path is unexercised.
Naming it here so the next pass can close it rather than rediscover it.

### Local generation status

19 of 25 modules emitted, 6 refused; 10 refusing modules (6 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/skeleton2d-formats`

Import 2D skeletal rigs from industry formats (Spine .json/.skel, DragonBones) into skeleton2d + animation clips

59 upstream modules under `packages/skeleton2d-formats/src`.
 Depends on `@flighthq/animation`, `@flighthq/easing`, `@flighthq/entity`, `@flighthq/importdiagnostics`, `@flighthq/registry`, `@flighthq/requirement`, `@flighthq/skeleton2d`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/dragonBonesAnimationsHandler.test.ts` | 2 | `dragonBonesAnimationsSectionHandler`, `dragonBonesAnimationsSectionReader` |
| `src/dragonBonesBonesHandler.test.ts` | 2 | `dragonBonesBonesSectionHandler`, `dragonBonesBonesSectionReader` |
| `src/dragonBonesBoneTimelineHandler.test.ts` | 2 | `dragonBonesBoneTimelineHandler`, `dragonBonesBoneTimelineReader` |
| `src/dragonBonesFull.test.ts` | 2 | `parseDragonBonesSkeleton` |
| `src/dragonBonesHandlers.test.ts` | 1 | `registerAllDragonBonesHandlers` |
| `src/dragonBonesParse.test.ts` | 65 | … `dragonBones document parse`, `dragonBones document parse type resilience`, `dragonBones document parse version gate`, `dragonBonesAnimationsSectionReader`, `dragonBonesBonesSectionReader`, `dragonBonesBoneTimelineReader`, `dragonBonesDeformTimelineReader`, `dragonBonesIkConstraintsSectionReader`, `dragonBonesIkTimelineReader`, `dragonBonesSkinsSectionReader`, `dragonBonesSlotsSectionReader`, `dragonBonesSlotTimelineReader` (+2 more describe blocks in the file) |
| `src/dragonBonesParseHelpers.test.ts` | 8 | `buildDragonBonesSegmentEasings`, `dragonBonesFrames`, `dragonBonesFrameTimes`, `dragonBonesInterpolation`, `numberOr`, `parseDragonBonesBoneTransform`, `skipCrumbDragonBonesGroup` |
| `src/dragonBonesRegistry.test.ts` | 9 | `createDragonBonesRegistry`, `getDragonBonesSectionHandler`, `getDragonBonesTimelineHandler`, `registerDragonBonesSectionHandler`, `registerDragonBonesTimelineHandler`, `unregisterDragonBonesSectionHandler`, `unregisterDragonBonesTimelineHandler` |
| `src/dragonBonesRequirements.test.ts` | 10 | `DRAGONBONES_REQUIREMENT_KEY_NAMESPACE`, `isReadableDragonBones`, `parseDragonBonesRequirements` |
| `src/dragonBonesSectionCounts.test.ts` | 14 | `collectDragonBonesSectionCounts` |
| `src/dragonBonesSectionHandlers.test.ts` | 7 | `dragonBonesAllSectionHandlers`, `dragonBonesAnimationsSectionHandler`, `dragonBonesBonesSectionHandler`, `dragonBonesIkConstraintsSectionHandler`, `dragonBonesSkinsSectionHandler`, `dragonBonesSlotsSectionHandler`, `registerDragonBonesSectionHandlers` |
| `src/dragonBonesSkinsHandler.test.ts` | 2 | `dragonBonesSkinsSectionHandler`, `dragonBonesSkinsSectionReader` |
| `src/dragonBonesSlotsHandler.test.ts` | 2 | `dragonBonesSlotsSectionHandler`, `dragonBonesSlotsSectionReader` |
| `src/dragonBonesSlotTimelineHandler.test.ts` | 2 | `dragonBonesSlotTimelineHandler`, `dragonBonesSlotTimelineReader` |
| `src/dragonBonesStubHandlers.test.ts` | 8 | `dragonBonesDeformTimelineHandler`, `dragonBonesDeformTimelineReader`, `dragonBonesIkConstraintsSectionHandler`, `dragonBonesIkConstraintsSectionReader`, `dragonBonesIkTimelineHandler`, `dragonBonesIkTimelineReader`, `dragonBonesZOrderTimelineHandler`, `dragonBonesZOrderTimelineReader` |
| `src/dragonBonesTimelineHandlers.test.ts` | 7 | `dragonBonesAllTimelineHandlers`, `dragonBonesBoneTimelineHandler`, `dragonBonesDeformTimelineHandler`, `dragonBonesIkTimelineHandler`, `dragonBonesSlotTimelineHandler`, `dragonBonesZOrderTimelineHandler`, `registerDragonBonesTimelineHandlers` |
| `src/dragonBonesTreeShaking.test.ts` | 1 | `DragonBones handler atom tree shaking`, `DragonBones handler export lanes` |
| `src/skeleton2dJsonAnalyzer.test.ts` | 9 | `analyzeSkeleton2DJson`, `registerSkeleton2DJsonAnalyzer`, `unregisterSkeleton2DJsonAnalyzer` |
| `src/skeletonDetect.test.ts` | 7 | `getSkeleton2DFormatKinds`, `parseSkeleton2D`, `registerSkeleton2DFormat`, `unregisterSkeleton2DFormat` |
| `src/spineBinaryAnimationsHandler.test.ts` | 3 | `skipSpineBinaryAnimationsSection`, `spineBinaryAnimationsSectionHandler`, `spineBinaryAnimationsSectionReader` |
| `src/spineBinaryBonesHandler.test.ts` | 3 | `skipSpineBinaryBonesSection`, `spineBinaryBonesSectionHandler`, `spineBinaryBonesSectionReader` |
| `src/spineBinaryBoneTimelineHandler.test.ts` | 4 | `buildSpineBinarySegmentEasings`, `skipSpineBinaryBoneTimelines`, `spineBinaryBoneTimelineHandler`, `spineBinaryBoneTimelineReader` |
| `src/spineBinaryDrawOrderTimelineHandler.test.ts` | 3 | `skipSpineBinaryDrawOrderTimeline`, `spineBinaryDrawOrderTimelineHandler`, `spineBinaryDrawOrderTimelineReader` |
| `src/spineBinaryFull.test.ts` | 1 | `parseSpineSkeletonBinary` |
| `src/spineBinaryHandlers.test.ts` | 1 | `registerAllSpineBinaryHandlers` |
| `src/spineBinaryParse.test.ts` | 32 | `parseSpineSkeletonBinary`, `parseSpineSkeletonBinaryWithRegistry`, `registerAllSpineBinaryHandlers`, `registerSpineBinarySectionHandlers`, `registerSpineBinaryTimelineHandlers` |
| `src/spineBinaryParseHelpers.test.ts` | 23 | … `clampSpineBinaryUnit`, `readSpineBinaryStringReference`, `reportSpineBinaryCrumb`, `skipSpineBinaryCurveFrames`, `skipSpineBinaryCurveTag`, `SPINE_BINARY_ATTACHMENT_SEQUENCE`, `SPINE_BINARY_ATTACHMENT_TYPES`, `SPINE_BINARY_BOUNDS_BYTES`, `SPINE_BINARY_COLOR_BYTES`, `SPINE_BINARY_CURVE_BEZIER`, `SPINE_BINARY_CURVE_EPSILON`, `SPINE_BINARY_DEFAULT_SKIN_NAME` (+11 more describe blocks in the file) |
| `src/spineBinaryReader.test.ts` | 22 | `createSpineBinaryReader`, `hasSpineBinaryBytes`, `isSpineBinaryReaderOverrun`, `readSpineBinaryBoolean`, `readSpineBinaryByte`, `readSpineBinaryFloat`, `readSpineBinaryInt`, `readSpineBinarySignedVarint`, `readSpineBinaryString`, `readSpineBinaryUnsignedShort`, `readSpineBinaryVarint`, `skipSpineBinaryBytes` |
| `src/spineBinaryRegistry.test.ts` | 9 | `createSpineBinaryRegistry`, `getSpineBinarySectionHandler`, `getSpineBinaryTimelineHandler`, `registerSpineBinarySectionHandler`, `registerSpineBinaryTimelineHandler`, `unregisterSpineBinarySectionHandler`, `unregisterSpineBinaryTimelineHandler` |
| `src/spineBinaryRequirements.test.ts` | 9 | `isReadableSpineBinary`, `parseSpineBinaryRequirements`, `SPINE_BINARY_REQUIREMENT_KEY_NAMESPACE` |
| `src/spineBinarySectionCounts.test.ts` | 11 | `collectSpineBinarySectionCounts` |
| `src/spineBinarySectionHandlers.test.ts` | 9 | `registerSpineBinarySectionHandlers`, `spineBinaryAnimationsSectionHandler`, `spineBinaryBonesSectionHandler`, `spineBinaryEventsSectionHandler`, `spineBinaryIkConstraintsSectionHandler`, `spineBinaryPathConstraintsSectionHandler`, `spineBinarySkinsSectionHandler`, `spineBinarySlotsSectionHandler`, `spineBinaryTransformConstraintsSectionHandler` |
| `src/spineBinarySkinsHandler.test.ts` | 3 | `skipSpineBinarySkinsSection`, `spineBinarySkinsSectionHandler`, `spineBinarySkinsSectionReader` |
| `src/spineBinarySlotsHandler.test.ts` | 3 | `skipSpineBinarySlotsSection`, `spineBinarySlotsSectionHandler`, `spineBinarySlotsSectionReader` |
| `src/spineBinarySlotTimelineHandler.test.ts` | 3 | `skipSpineBinarySlotTimelines`, `spineBinarySlotTimelineHandler`, `spineBinarySlotTimelineReader` |
| `src/spineBinaryStubHandlers.test.ts` | 12 | `skipSpineBinaryEventsSection`, `skipSpineBinaryIkConstraintsSection`, `skipSpineBinaryPathConstraintsSection`, `skipSpineBinaryTransformConstraintsSection`, `spineBinaryEventsSectionHandler`, `spineBinaryEventsSectionReader`, `spineBinaryIkConstraintsSectionHandler`, `spineBinaryIkConstraintsSectionReader`, `spineBinaryPathConstraintsSectionHandler`, `spineBinaryPathConstraintsSectionReader`, `spineBinaryTransformConstraintsSectionHandler`, `spineBinaryTransformConstraintsSectionReader` |
| `src/spineBinaryStubTimelineHandlers.test.ts` | 15 | … `skipSpineBinaryDeformTimelines`, `skipSpineBinaryEventTimelines`, `skipSpineBinaryIkTimelines`, `skipSpineBinaryPathTimelines`, `skipSpineBinaryTransformTimelines`, `spineBinaryDeformTimelineHandler`, `spineBinaryDeformTimelineReader`, `spineBinaryEventTimelineHandler`, `spineBinaryEventTimelineReader`, `spineBinaryIkTimelineHandler`, `spineBinaryIkTimelineReader`, `spineBinaryPathTimelineHandler` (+3 more describe blocks in the file) |
| `src/spineBinaryTimelineHandlers.test.ts` | 9 | `registerSpineBinaryTimelineHandlers`, `spineBinaryBoneTimelineHandler`, `spineBinaryDeformTimelineHandler`, `spineBinaryDrawOrderTimelineHandler`, `spineBinaryEventTimelineHandler`, `spineBinaryIkTimelineHandler`, `spineBinaryPathTimelineHandler`, `spineBinarySlotTimelineHandler`, `spineBinaryTransformTimelineHandler` |
| `src/spineBinaryTreeShaking.test.ts` | 1 | `Spine binary handler atom tree shaking`, `Spine binary handler export lanes` |
| `src/spineBinaryVersion.test.ts` | 9 | `explainSpineBinaryVersionFailure`, `getSpineBinaryVersion` |
| `src/spineBinaryVersioned.test.ts` | 14 | `parseSpineSkeletonBinaryVersioned`, `registerSpineSkeletonBinaryParser`, `toSpineBinaryLayoutKey`, `unsupported-but-recognized Spine layouts` |
| `src/spineDrawOrder.test.ts` | 8 | `resolveSpineDrawOrdering` |
| `src/spineJsonAnimationsHandler.test.ts` | 2 | `spineJsonAnimationsSectionHandler`, `spineJsonAnimationsSectionReader` |
| `src/spineJsonBonesHandler.test.ts` | 2 | `spineJsonBonesSectionHandler`, `spineJsonBonesSectionReader` |
| `src/spineJsonBoneTimelineHandler.test.ts` | 2 | `spineJsonBoneTimelineHandler`, `spineJsonBoneTimelineReader` |
| `src/spineJsonDrawOrderTimelineHandler.test.ts` | 2 | `spineJsonDrawOrderTimelineHandler`, `spineJsonDrawOrderTimelineReader` |
| `src/spineJsonFull.test.ts` | 3 | `parseSpineSkeleton` |
| `src/spineJsonHandlers.test.ts` | 1 | `registerAllSpineJsonHandlers` |
| `src/spineJsonRegistry.test.ts` | 9 | `createSpineJsonRegistry`, `getSpineJsonSectionHandler`, `getSpineJsonTimelineHandler`, `registerSpineJsonSectionHandler`, `registerSpineJsonTimelineHandler`, `unregisterSpineJsonSectionHandler`, `unregisterSpineJsonTimelineHandler` |
| `src/spineJsonRequirements.test.ts` | 10 | `isReadableSpineJson`, `parseSpineJsonRequirements`, `SPINE_JSON_REQUIREMENT_KEY_NAMESPACE` |
| `src/spineJsonSectionCounts.test.ts` | 19 | `collectSpineJsonSectionCounts` |
| `src/spineJsonSectionHandlers.test.ts` | 9 | `registerSpineJsonSectionHandlers`, `spineJsonAnimationsSectionHandler`, `spineJsonBonesSectionHandler`, `spineJsonEventsSectionHandler`, `spineJsonIkConstraintsSectionHandler`, `spineJsonPathConstraintsSectionHandler`, `spineJsonSkinsSectionHandler`, `spineJsonSlotsSectionHandler`, `spineJsonTransformConstraintsSectionHandler` |
| `src/spineJsonSkinsHandler.test.ts` | 2 | `spineJsonSkinsSectionHandler`, `spineJsonSkinsSectionReader` |
| `src/spineJsonSlotsHandler.test.ts` | 2 | `spineJsonSlotsSectionHandler`, `spineJsonSlotsSectionReader` |
| `src/spineJsonSlotTimelineHandler.test.ts` | 2 | `spineJsonSlotTimelineHandler`, `spineJsonSlotTimelineReader` |
| `src/spineJsonStubHandlers.test.ts` | 18 | … `spineJsonDeformTimelineHandler`, `spineJsonDeformTimelineReader`, `spineJsonEventsSectionHandler`, `spineJsonEventsSectionReader`, `spineJsonEventTimelineHandler`, `spineJsonEventTimelineReader`, `spineJsonIkConstraintsSectionHandler`, `spineJsonIkConstraintsSectionReader`, `spineJsonIkTimelineHandler`, `spineJsonIkTimelineReader`, `spineJsonPathConstraintsSectionHandler`, `spineJsonPathConstraintsSectionReader` (+6 more describe blocks in the file) |
| `src/spineJsonTimelineHandlers.test.ts` | 9 | `registerSpineJsonTimelineHandlers`, `spineJsonBoneTimelineHandler`, `spineJsonDeformTimelineHandler`, `spineJsonDrawOrderTimelineHandler`, `spineJsonEventTimelineHandler`, `spineJsonIkTimelineHandler`, `spineJsonPathTimelineHandler`, `spineJsonSlotTimelineHandler`, `spineJsonTransformTimelineHandler` |
| `src/spineJsonTreeShaking.test.ts` | 1 | `Spine JSON handler atom tree shaking`, `Spine JSON handler export lanes` |
| `src/spineParse.test.ts` | 64 | … `parseSpineDrawOrderTimeline`, `parseSpineSkeletonWithRegistry`, `spine json document parse`, `spine json document parse type resilience`, `spineJsonAnimationsSectionReader`, `spineJsonBonesSectionReader`, `spineJsonBoneTimelineReader`, `spineJsonDeformTimelineReader`, `spineJsonDrawOrderTimelineReader`, `spineJsonEventsSectionReader`, `spineJsonEventTimelineReader`, `spineJsonIkConstraintsSectionReader` (+8 more describe blocks in the file) |
| `src/spineParseHelpers.test.ts` | 9 | `buildSpineSegmentEasings`, `clampUnit`, `indexOfBone`, `indexOfSpineSlot`, `numberOr`, `parseSpineColor`, `skipCrumbSpineTimelineGroup`, `SPINE_DEFAULT_SKIN_NAME`, `SPINE_NO_ATTACHMENT_INDEX` |

Upstream carries per-package records at `flight/agents/packages/skeleton2d-formats/`: `status.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

10 of 15 modules emitted, 5 refused; 11 refusing modules (5 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/particles`

Particle system: emitter simulation and typed-array-backed particle primitives

13 upstream modules under `packages/particles/src`.
 Depends on `@flighthq/entity`, `@flighthq/geometry`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/applyParticleCollisions.test.ts` | 17 | `applyParticleCollisions`, `circle`, `integration`, `plane (floor)`, `plane (3D normal)`, `sphere`, `rect`, `applyParticleObjectCollisions` |
| `src/applyParticleForces.test.ts` | 15 | `applyParticleForces`, `applyParticleObjectForces` |
| `src/curve.test.ts` | 27 | `buildParticleColorCurve`, `buildParticleCurve`, `lerpHsvDirect`, `lerpHsvInPlace`, `particleColorCurveFromKeyframes`, `particleColorCurveToKeyframes`, `particleCurveFromKeyframes`, `particleCurveToKeyframes`, `sampleParticleColorCurve`, `sampleParticleCurve` |
| `src/particleEmitterConfig.test.ts` | 3 | `createParticleEmitterConfig` |
| `src/particleEmitterSignals.test.ts` | 6 | `createParticleEmitterSignals`, `enableParticleEmitterSignals`, `getParticleEmitterSignals`, `initializeParticleEmitterSignals` |
| `src/particleEmitterState.test.ts` | 8 | `createParticleEmitterState`, `ensureParticleEmitterStateCapacity`, `initializeParticleEmitterState` |
| `src/particleObjectsState.test.ts` | 5 | `createParticleObjectsState`, `ensureParticleObjectsStateCapacity`, `initializeParticleObjectsState` |
| `src/particleSpawnOffset.test.ts` | 3 | `writeParticleSpawnOffset` |
| `src/stepParticleObjects.test.ts` | 1 | `stepParticleObjects` |
| `src/updateParticleObjects.test.ts` | 23 | `updateParticleObjects`, `isParticleObjectsComplete` |
| `src/validateParticleEmitterConfig.test.ts` | 11 | `normalizeParticleEmitterConfig`, `validateParticleEmitterConfig` |

Upstream carries per-package records at `flight/agents/packages/particles/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

12 of 13 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `particles-validate-particle-emitter-config`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/particleemitter`

ParticleEmitter 2D and 3D composition layer over @flighthq/particles

12 upstream modules under `packages/particleemitter/src`.
 Depends on `@flighthq/entity`, `@flighthq/geometry`, `@flighthq/node`, `@flighthq/particles`, `@flighthq/scene2d`, `@flighthq/scene3d`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/deterministic.test.ts` | 1 | `deterministic replay` |
| `src/emitParticleBurst2D.test.ts` | 9 | `emitParticleBurst2D` |
| `src/emitParticleBurst3D.test.ts` | 8 | `emitParticleBurst3D` |
| `src/particleEmitter.test.ts` | 51 | … `appendParticleEmitter2DParticle`, `clearParticleEmitter2D`, `cloneParticleEmitter2D`, `compactParticleEmitter2D`, `computeParticleEmitter2DLocalBoundsRectangle`, `createParticleEmitter2D`, `createParticleEmitter2DRuntime`, `createParticleEmitterData`, `getParticleEmitter2DCapacity`, `getParticleEmitter2DParticleAlpha`, `getParticleEmitter2DParticleId`, `getParticleEmitter2DParticleVelocity` (+9 more describe blocks in the file) |
| `src/particleEmitter3D.test.ts` | 41 | … `appendParticleEmitter3DParticle`, `clearParticleEmitter3D`, `cloneParticleEmitter3D`, `compactParticleEmitter3D`, `computeParticleEmitter3DLocalBoundsAabb`, `createParticleEmitter3D`, `getParticleEmitter3DCapacity`, `getParticleEmitter3DParticleAlpha`, `getParticleEmitter3DParticleId`, `getParticleEmitter3DParticleVelocity`, `getParticleEmitter3DRuntime`, `isParticleEmitter3D` (+7 more describe blocks in the file) |
| `src/prewarmParticleEmitter2D.test.ts` | 6 | `prewarmParticleEmitter2D` |
| `src/prewarmParticleEmitter3D.test.ts` | 2 | `prewarmParticleEmitter3D` |
| `src/stepParticleEmitter2D.test.ts` | 5 | `stepParticleEmitter2D` |
| `src/stepParticleEmitter3D.test.ts` | 3 | `stepParticleEmitter3D` |
| `src/updateParticleEmitter2D.test.ts` | 36 | `isParticleEmitter2DComplete`, `updateParticleEmitter2D` |
| `src/updateParticleEmitter2DIntegration.test.ts` | 13 | `attractor forces via updateParticleEmitter2D`, `collision settling via updateParticleEmitter2D`, `lifetime curves in updateParticleEmitter2D`, `normalized config simulates via updateParticleEmitter2D`, `onEmitterComplete signal`, `onParticleDeath signal`, `onParticleSpawn signal`, `seeded state determinism via updateParticleEmitter2D` |
| `src/updateParticleEmitter3D.test.ts` | 24 | `isParticleEmitter3DComplete`, `updateParticleEmitter3D` |

Upstream carries per-package records at `flight/agents/packages/particleemitter/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

7 of 12 modules emitted, 5 refused; 12 refusing modules (5 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/particles-formats`

Import/export Particle Designer, Spine, Unity, libGDX, and Starling PEX particle configs, plus parse-only Pixi support

18 upstream modules under `packages/particles-formats/src`.
 Depends on `@flighthq/importdiagnostics`, `@flighthq/math`, `@flighthq/particles`, `@flighthq/requirement`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/detect.test.ts` | 17 | `detectParticleFormat` |
| `src/formatRegistry.test.ts` | 15 | `detectRegisteredParticleFormat`, `getParticleFormatCodec`, `getRegisteredParticleFormats`, `parseRegisteredParticleFormat`, `registerParticleFormat`, `unregisterParticleFormat` |
| `src/libgdxParse.test.ts` | 27 | `parseLibgdxParticle`, `parseLibgdxParticleDocument` |
| `src/libgdxSerialize.test.ts` | 10 | `serializeLibgdxParticle`, `serializeLibgdxParticleDocument` |
| `src/parseParticleConfig.test.ts` | 18 | `parseParticleConfig`, `parseParticleConfigDocument` |
| `src/particleDesignerParse.test.ts` | 33 | `parseParticleDesignerPlist`, `color variance and blend mode`, `lightweight, returns config directly`, `malformed input`, `parseParticleDesignerPlistDocument`, `full round-trip, returns { config, document }`, `import warnings`, `serializeParticleDesignerPlist` |
| `src/particleDesignerSerialize.test.ts` | 7 | `serializeParticleDesignerPlist`, `serializeParticleDesignerPlistDocument` |
| `src/pixiParse.test.ts` | 22 | `parsePixiParticle`, `parsePixiParticleDocument` |
| `src/registerBuiltInParticleFormats.test.ts` | 9 | `applyParticleImportOptions`, `particleAllFormats`, `registerBuiltInParticleFormats` |
| `src/spineParse.test.ts` | 37 | `parseSpineParticle`, `blend mode`, `lightweight, returns config directly`, `malformed input`, `multi-stop timelines bake into curves`, `parseSpineParticleDocument`, `full round-trip, returns { config, document }`, `import warnings`, `serializeSpineParticle`, `serializeSpineParticle — curve round-trip` |
| `src/spineSerialize.test.ts` | 6 | `serializeSpineParticle`, `serializeSpineParticleDocument` |
| `src/starlingPexParse.test.ts` | 25 | `parseStarlingPex`, `parseStarlingPexDocument` |
| `src/starlingPexSerialize.test.ts` | 10 | `serializeStarlingPex`, `serializeStarlingPexDocument` |
| `src/unityParse.test.ts` | 31 | `parseUnityParticle`, `gradient / size curves bake into curves`, `lightweight, returns config directly`, `malformed input`, `parseUnityParticleDocument`, `full round-trip, returns { config, document }`, `import warnings`, `serializeUnityParticle`, `serializeUnityParticle — curve round-trip` |
| `src/unitySerialize.test.ts` | 7 | `serializeUnityParticle`, `serializeUnityParticleDocument` |

Upstream carries per-package records at `flight/agents/packages/particles-formats/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

11 of 17 modules emitted, 6 refused; 8 refusing modules (6 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/flow`

Application mode/screen flow-state stack — a plain-data stack of application states (boot, menu, play, pause, game-over) with a paired enter/exit/pause/resume/update lifecycle driven by push/pop/replace/clear transitions, transparent-overlay update-below/render-below flags, an update pass that ticks the active state and any opted-in states beneath it, and active-state/depth/visible-states queries; direct lifecycle callbacks (single-owner, deterministic order), not signals

4 upstream modules under `packages/flow/src`.
 Depends on `@flighthq/entity`, `@flighthq/log`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/flow.test.ts` | 29 | `clearFlowStack`, `createFlowStack`, `getActiveFlowState`, `getFlowStackDepth`, `getFlowStackVisibleStates`, `initializeFlowStack`, `popFlowState`, `pushFlowState`, `replaceFlowState`, `updateFlowStack` |
| `src/flowGuards.test.ts` | 3 | `disableFlowGuards`, `enableFlowGuards`, `reportFlowGuard` |

Upstream carries per-package records at `flight/agents/packages/flow/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

4 of 4 modules emitted, 0 refused; 4 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/statechart`

Plain-data concurrent statecharts with typed inputs, guarded transitions, and reported blend progress

5 upstream modules under `packages/statechart/src`.
 Depends on `@flighthq/entity`, `@flighthq/log`, `@flighthq/signals`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/enableStatechartGuards.test.ts` | 5 | `areStatechartGuardsEnabled`, `disableStatechartGuards`, `enableStatechartGuards` |
| `src/statechart.test.ts` | 28 | `advanceStatechartInstance`, `createStatechartInstance`, `explainStatechartTransition`, `fireStatechartTrigger`, `getStatechartInputIndex`, `getStatechartRegionBlend`, `getStatechartRegionState`, `initializeStatechartInstance`, `setStatechartBooleanInput`, `setStatechartNumberInput`, `setStatechartRegionDuration` |
| `src/statechartSignals.test.ts` | 6 | `enableStatechartSignals`, `getStatechartSignals`, `initializeStatechartSignals` |

Upstream carries per-package records at `flight/agents/packages/statechart/`: `status.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_socket_statechart` — `tests/generated_socket_statechart_test.cpp` (dedicated)

### Local generation status

5 of 5 modules emitted, 0 refused; 3 refusing modules (0 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `statechart-guard-identity-and-reporting`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/physics2d`

2D rigid-body dynamics — deterministic sequential-impulse solver over collision manifolds and a spatial broadphase, with explicit stepping and no hidden per-frame allocation

26 upstream modules under `packages/physics2d/src`.
 Depends on `@flighthq/collision`, `@flighthq/entity`, `@flighthq/log`, `@flighthq/math`, `@flighthq/node`, `@flighthq/spatial`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/broadphase.test.ts` | 5 | `synchronizePhysics2DBroadphase`, `synchronizePhysics2DSweptBroadphase` |
| `src/colliderTransform.test.ts` | 22 | `createPhysics2DColliderWorldShape`, `initializeCollisionCapsule2D`, `initializeCollisionCircle2D`, `initializeCollisionObb2D`, `initializeCollisionPoint2D`, `initializeCollisionPolygon2D`, `initializeCollisionSegment2D`, `updatePhysics2DColliderWorldShape`, `writePhysics2DColliderBounds` |
| `src/debugGeometry.test.ts` | 10 | `createPhysics2DDebugGeometry`, `initializePhysics2DDebugGeometry`, `writePhysics2DDebugGeometry` |
| `src/enablePhysics2DGuards.test.ts` | 14 | `arePhysics2DGuardsEnabled`, `disablePhysics2DGuards`, `enablePhysics2DGuards` |
| `src/explainPhysics2DCollision.test.ts` | 5 | `explainPhysics2DCollision` |
| `src/explainPhysics2DJoints.test.ts` | 4 | `explainPhysics2DJoints` |
| `src/explainPhysics2DStep.test.ts` | 3 | `explainPhysics2DStep` |
| `src/islands.test.ts` | 25 | `buildPhysics2DSolveIslands`, `isRigidBody2DPairAwake`, `updatePhysics2DSleep`, `wakePhysics2DBody` |
| `src/jointCollisionSuppression.test.ts` | 2 | `isPhysics2DPairJointSuppressed`, `rebuildPhysics2DJointCollisionSuppressions` |
| `src/jointFactories.test.ts` | 18 | … `createPhysics2DDistanceJoint`, `createPhysics2DGearJoint`, `createPhysics2DMouseJoint`, `createPhysics2DPrismaticJoint`, `createPhysics2DPulleyJoint`, `createPhysics2DRevoluteJoint`, `createPhysics2DRopeJoint`, `createPhysics2DWeldJoint`, `createPhysics2DWheelJoint`, `initializePhysics2DDistanceJoint`, `initializePhysics2DGearJoint`, `initializePhysics2DMouseJoint` (+6 more describe blocks in the file) |
| `src/jointReactions.test.ts` | 15 | `createPhysics2DJointReaction`, `initializePhysics2DJointReaction`, `writePhysics2DJointReaction` |
| `src/jointRegistry.test.ts` | 22 | `addPhysics2DJoint`, `getPhysics2DJointSolver`, `invalidatePhysics2DJoint`, `registerPhysics2DJointSolver`, `removePhysics2DJoint` |
| `src/jointRows.test.ts` | 9 | `writePhysics2DSoftRowParameters` |
| `src/joints.test.ts` | 75 | … `canonical end ordering and direction-bearing joint state`, `joint warm starting`, `joints built without their impulse accumulators`, `physics2DDistanceJointSolver`, `physics2DGearJointSolver`, `physics2DMouseJointSolver`, `physics2DPrismaticJointSolver`, `physics2DPulleyJointSolver`, `physics2DRevoluteJointSolver`, `physics2DRevoluteJointSolver motor and limits`, `physics2DRopeJointSolver`, `physics2DWeldJointSolver` (+1 more describe blocks in the file) |
| `src/massProperties.test.ts` | 16 | `computePhysics2DColliderMassData`, `updateRigidBody2DMassData` |
| `src/material.test.ts` | 4 | `mixPhysics2DFriction`, `mixPhysics2DRestitution` |
| `src/nodeSync.test.ts` | 4 | `syncPhysics2DBodyToNode2D` |
| `src/ownership.test.ts` | 2 | `assertPhysics2DBodyNotStepping`, `assertPhysics2DWorldNotStepping` |
| `src/registerBuiltInPhysics2DJointSolvers.test.ts` | 3 | `registerBuiltInPhysics2DJointSolvers` |
| `src/solver.test.ts` | 17 | `applyPhysics2DImpulse`, `relativeNormalVelocity`, `solvePhysics2DContactIndicesOnce`, `solvePhysics2DContacts`, `solvePhysics2DContactsOnce`, `warmStartPhysics2DContactIndices`, `warmStartPhysics2DContacts` |
| `src/step.test.ts` | 88 | … `a body the step declines leaves the broadphase`, `bullet continuous collision detection`, `centre of mass integration`, `collision filtering`, `contact solve hooks`, `sensor reporting between immovable bodies`, `setPhysics2DContactIntakeGuard`, `setPhysics2DJointResolutionGuard`, `setPhysics2DStepGuard`, `stepPhysics2D`, `stepPhysics2D contact cache identity`, `stepPhysics2D contact events` (+4 more describe blocks in the file) |
| `src/stepValidation.test.ts` | 8 | `isPhysics2DBodyStateValid`, `isPhysics2DContactStateValid`, `isPhysics2DContactValid`, `isPhysics2DGravityValid`, `isPhysics2DJointStateValid`, `isPhysics2DPreviousTimestepValid`, `isPhysics2DSolverConfigValid`, `isPhysics2DTimestepValid` |
| `src/stress.test.ts` | 4 | `physics2d stress qualification` |
| `src/world.test.ts` | 50 | … `addPhysics2DBody`, `addPhysics2DCollider`, `applyPhysics2DForce`, `applyPhysics2DForceAtPoint`, `applyPhysics2DLinearImpulse`, `applyPhysics2DLinearImpulseAtPoint`, `applyPhysics2DTorque`, `createPhysics2DCollider`, `createPhysics2DSolverConfig`, `createPhysics2DWorld`, `createRigidBody2D`, `findPhysics2DBody` (+13 more describe blocks in the file) |
| `src/worldQueries.test.ts` | 32 | `createPhysics2DQueryFilter`, `createPhysics2DQueryResult`, `createPhysics2DRayResult`, `createPhysics2DShapeCastResult`, `initializePhysics2DQueryResult`, `initializePhysics2DRayResult`, `initializePhysics2DShapeCastResult`, `queryPhysics2DPoint`, `queryPhysics2DRay`, `queryPhysics2DRayClosest`, `queryPhysics2DRegion`, `queryPhysics2DShapeCast` |

Upstream carries per-package records at `flight/agents/packages/physics2d/`: `status.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

21 of 26 modules emitted, 5 refused; 16 refusing modules (5 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `physics2d-collider-world-shape-construction`, `physics2d-step-validation-refusals`, `physics2d-node-transform-invalidation`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/physics2d-abi`

Persistent handle and packed-buffer execution ABI for Physics2D, with an executable TypeScript reference backend

8 upstream modules under `packages/physics2d-abi/src`.
 Depends on `@flighthq/collision`, `@flighthq/entity`, `@flighthq/physics2d`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/physics2DAbi.test.ts` | 16 | `createPhysics2DAbi`, `createPhysics2DAbiWorld`, `destroyPhysics2DAbiWorld`, `executePhysics2DAbiCommands`, `getPhysics2DAbiWorldStatus`, `readPhysics2DAbiBodies`, `readPhysics2DAbiContacts`, `readPhysics2DAbiJoints`, `stepPhysics2DAbiWorld` |
| `src/physics2DAbiBuffer.test.ts` | 15 | … `clearPhysics2DAbiCommandBuffer`, `createPhysics2DAbiBodyBuffer`, `createPhysics2DAbiCommandBuffer`, `createPhysics2DAbiContactBuffer`, `createPhysics2DAbiExecutionResult`, `createPhysics2DAbiJointBuffer`, `createPhysics2DAbiQueryBuffer`, `getPhysics2DAbiCommandBufferRemainingByteLength`, `initializePhysics2DAbiBodyBuffer`, `initializePhysics2DAbiContactBuffer`, `initializePhysics2DAbiExecutionResult`, `initializePhysics2DAbiJointBuffer` (+1 more describe blocks in the file) |
| `src/physics2DAbiCommand.test.ts` | 21 | … `getPhysics2DAbiSetColliderCommandByteLength`, `writePhysics2DAbiApplyForceAtPointCommand`, `writePhysics2DAbiApplyForceCommand`, `writePhysics2DAbiApplyLinearImpulseAtPointCommand`, `writePhysics2DAbiApplyLinearImpulseCommand`, `writePhysics2DAbiApplyTorqueCommand`, `writePhysics2DAbiDestroyBodyCommand`, `writePhysics2DAbiDestroyColliderCommand`, `writePhysics2DAbiDestroyJointCommand`, `writePhysics2DAbiSetBodyCommand`, `writePhysics2DAbiSetColliderCommand`, `writePhysics2DAbiSetGravityCommand` (+3 more describe blocks in the file) |
| `src/physics2DAbiLayout.test.ts` | 14 | … `Physics2DAbiBodyFlag`, `Physics2DAbiBodyValue`, `Physics2DAbiCapability`, `Physics2DAbiCommandByteLength`, `Physics2DAbiCommandKind`, `Physics2DAbiCommandMagic`, `Physics2DAbiContactFlag`, `Physics2DAbiContactValue`, `Physics2DAbiJointFlag`, `Physics2DAbiJointKind`, `Physics2DAbiMaxContactPoints`, `Physics2DAbiQueryValue` (+2 more describe blocks in the file) |
| `src/physics2DAbiQuery.test.ts` | 9 | `queryPhysics2DAbiPoint`, `queryPhysics2DAbiRay`, `queryPhysics2DAbiRayClosest`, `queryPhysics2DAbiRegion`, `queryPhysics2DAbiShapeCast` |
| `src/referencePhysics2DAbi.test.ts` | 10 | `createReferencePhysics2DAbi`, `initializeReferencePhysics2DAbi` |

Upstream carries per-package records at `flight/agents/packages/physics2d-abi/`: `status.md`, `charter.md`.

### Existing CTest coverage

None. No local test, example, or benchmark includes a header of this package.

### Local generation status

6 of 8 modules emitted, 2 refused; 5 refusing modules (2 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `physics2d-abi-variant-shape-encoding`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/collision`

2D narrow-phase collision — discrete contact manifolds, exact convex linear sweeps, and point/segment/ray queries

38 upstream modules under `packages/collision/src`.
 Depends on `@flighthq/entity`, `@flighthq/geometry`, `@flighthq/log`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/capsuleContact2D.test.ts` | 14 | `collideCapsuleAabbContactManifold2D`, `collideCapsuleCapsuleContactManifold2D`, `collideCapsuleObbContactManifold2D`, `collideCapsulePolygonContactManifold2D`, `collideCircleCapsuleContactManifold2D` |
| `src/collideContactManifold2D.test.ts` | 10 | `collideContactManifold2D` |
| `src/collideContactManifold3D.test.ts` | 13 | `collideContactManifold3D` |
| `src/collisionFace3D.test.ts` | 19 | `getCollisionFaceQuery3D`, `queryCollisionAabbFace3D`, `queryCollisionBoxFace3D`, `queryCollisionCapsuleFace3D`, `queryCollisionConvexFace3D`, `registerBuiltInCollisionFaceQueries3D`, `registerCollisionFaceQuery3D` |
| `src/collisionShapeValidation2D.test.ts` | 5 | `getCollisionPolygonValidationStatus2D`, `getCollisionShapeValidationStatus2D` |
| `src/collisionShapeValidation3D.test.ts` | 13 | `getCollisionConvexValidationStatus3D`, `getCollisionShapeValidationStatus3D` |
| `src/collisionSupport2D.test.ts` | 15 | `getCollisionPairTest2D`, `getCollisionSupport2D`, `registerBuiltInCollisionSupports2D`, `registerCollisionPairTest2D`, `registerCollisionSupport2D`, `supportCollisionAabb2D`, `supportCollisionCircle2D`, `supportCollisionObb2D`, `supportCollisionPolygon2D`, `writeVertexListSupport2D` |
| `src/collisionSupport3D.test.ts` | 27 | … `clearCollisionPairTests3D`, `clearCollisionSupports3D`, `getCollisionPairTest3D`, `getCollisionSupport3D`, `registerBuiltInCollisionSupports3D`, `registerCollisionPairTest3D`, `registerCollisionSupport3D`, `supportCollisionAabb3D`, `supportCollisionBox3D`, `supportCollisionCapsule3D`, `supportCollisionCone3D`, `supportCollisionConvex3D` (+3 more describe blocks in the file) |
| `src/contactFeatureId.test.ts` | 5 | `packContactFeatureId` |
| `src/contactManifold2D.test.ts` | 6 | `clearCollisionContactManifold2D`, `createCollisionContactManifold2D`, `initializeCollisionContactManifold2D` |
| `src/contactManifold3D.test.ts` | 5 | `clearCollisionContactManifold3D`, `createCollisionContactManifold3D`, `initializeCollisionContactManifold3D` |
| `src/convexHull3D.test.ts` | 9 | `writeCollisionConvexHullFaces3D` |
| `src/convexVertices2D.test.ts` | 5 | `writeAabbVertices`, `writeObbVertices` |
| `src/cylinderCone3D.test.ts` | 13 | `getCollisionShapeContainsPoint3D for cylinder and cone`, `getCollisionShapeValidationStatus3D for cylinder and cone`, `raycastCollisionShape3D for cylinder and cone` |
| `src/enableCollisionGuards.test.ts` | 9 | `areCollisionGuardsEnabled`, `disableCollisionGuards`, `enableCollisionGuards` |
| `src/explainCollisionTest2D.test.ts` | 4 | `explainCollisionTest2D` |
| `src/explainCollisionTest3D.test.ts` | 5 | `explainCollisionTest3D` |
| `src/gjk2D.test.ts` | 8 | `testCollisionSupport2D`, `testCollisionSupportOverlap2D` |
| `src/gjk3D.test.ts` | 14 | `testCollisionSupport3D`, `testCollisionSupportOverlap3D` |
| `src/gjkDistance3D.test.ts` | 24 | `createCollisionDistance3D`, `initializeCollisionDistance3D`, `writeCollisionDistance3D` |
| `src/manifold2D.test.ts` | 4 | `clearCollisionManifold2D`, `createCollisionManifold2D`, `initializeCollisionManifold2D` |
| `src/manifold3D.test.ts` | 4 | `clearCollisionManifold3D`, `createCollisionManifold3D`, `initializeCollisionManifold3D` |
| `src/pointContainment2D.test.ts` | 12 | `getCollisionShapeContainsPoint2D` |
| `src/pointContainment3D.test.ts` | 12 | `getCollisionShapeContainsPoint3D` |
| `src/raycastCollisionShape2D.test.ts` | 9 | `createCollisionRaycastHit2D`, `initializeCollisionRaycastHit2D`, `raycastCollisionShape2D` |
| `src/raycastCollisionShape3D.test.ts` | 28 | `createCollisionRaycastHit3D`, `initializeCollisionRaycastHit3D`, `raycastCollisionShape3D` |
| `src/registerBuiltInCollisionPairTests2D.test.ts` | 3 | `registerBuiltInCollisionPairTests2D` |
| `src/registerBuiltInCollisionPairTests3D.test.ts` | 4 | `registerBuiltInCollisionPairTests3D` |
| `src/segmentCollision2D.test.ts` | 32 | `testSegmentAabbCollision2D`, `testSegmentCapsuleCollision2D`, `testSegmentCircleCollision2D`, `testSegmentObbCollision2D`, `testSegmentPolygonCollision2D`, `testSegmentSegmentCollision2D` |
| `src/shapeCollision2D.test.ts` | 43 | `testAabbAabbCollision2D`, `testAabbObbCollision2D`, `testAabbPolygonCollision2D`, `testCircleAabbCollision2D`, `testCircleCircleCollision2D`, `testCircleObbCollision2D`, `testCirclePolygonCollision2D`, `testObbObbCollision2D`, `testObbPolygonCollision2D`, `testPolygonPolygonCollision2D` |
| `src/shapeCollision3D.test.ts` | 26 | `shapeCollision3D agreement with the generic support floor`, `testAabbAabbCollision3D`, `testBoxBoxCollision3D`, `testCapsuleCapsuleCollision3D`, `testSphereAabbCollision3D`, `testSphereBoxCollision3D`, `testSphereCapsuleCollision3D`, `testSphereSphereCollision3D` |
| `src/shapeContact2D.test.ts` | 31 | `collideAabbAabbContactManifold2D`, `collideAabbObbContactManifold2D`, `collideAabbPolygonContactManifold2D`, `collideCircleAabbContactManifold2D`, `collideCircleCircleContactManifold2D`, `collideCircleObbContactManifold2D`, `collideCirclePolygonContactManifold2D`, `collideObbObbContactManifold2D`, `collideObbPolygonContactManifold2D`, `collidePolygonPolygonContactManifold2D` |
| `src/sweepCollisionShape2D.test.ts` | 9 | `createCollisionTimeOfImpact2D`, `initializeCollisionTimeOfImpact2D`, `sweepCollisionShape2D` |
| `src/sweepCollisionShape3D.test.ts` | 21 | `createCollisionTimeOfImpact3D`, `initializeCollisionTimeOfImpact3D`, `sweepCollisionShape3D` |
| `src/testCollision2D.test.ts` | 11 | `setCollisionTestGuard2D`, `testCollision2D` |
| `src/testCollision3D.test.ts` | 9 | `setCollisionTestGuard3D`, `testCollision3D` |
| `src/triangleMesh3D.test.ts` | 19 | … `collideCollisionHeightfield3D`, `collideCollisionTriangleMesh3D`, `createCollisionHeightfield3D`, `createCollisionTriangleMesh3D`, `getCollisionHeightfieldValidationStatus3D`, `getCollisionTriangleMeshValidationStatus3D`, `initializeCollisionHeightfield3D`, `initializeCollisionTriangleMesh3D`, `invalidateCollisionHeightfield3D`, `invalidateCollisionTriangleMesh3D`, `raycastCollisionHeightfield3D`, `raycastCollisionTriangleMesh3D` (+4 more describe blocks in the file) |

Upstream carries per-package records at `flight/agents/packages/collision/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp.generated_collision` — `tests/generated_collision_test.cpp` (dedicated)
- `flight_cpp_upstream_collision_smoke` — `examples/upstream/collision.cpp` (dedicated)

### Local generation status

32 of 38 modules emitted, 6 refused; 21 refusing modules (6 of them refusal roots). Read from the committed generated tree, not from the pin above.
 Hand-written overrides shadow generated headers here: `collision-face3d-kind-dispatch`, `collision-support2d-exact-shape-registry`, `collision-support3d-exact-shape-registry`, `collision-enable-guards-structural-validation`, `collision-point-containment3d-variant-dispatch`, `collision-sweep2d-continuous-convex-dispatch`, `collision-triangle-mesh3d-retained-acceleration`.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## `@flighthq/spatial`

2D broadphase spatial index over object AABBs — candidate-pair, region, point, and ray queries behind a swappable index seam (uniform-grid default), feeding @flighthq/collision narrow-phase

11 upstream modules under `packages/spatial/src`.
 Depends on `@flighthq/entity`, `@flighthq/geometry`, `@flighthq/types`.

### Upstream tests

| test file | cases | behaviors asserted (upstream `describe` blocks) |
| --- | ---: | --- |
| `src/bvh3D.test.ts` | 13 | `createBvhSpatialBackend3D`, `initializeBvhSpatialBackend3D` |
| `src/explainSpatialIndexing2D.test.ts` | 8 | `explainSpatialIndexing2D` |
| `src/explainSpatialIndexing3D.test.ts` | 5 | `explainSpatialIndexing3D` |
| `src/formatSpatialIndexingNotice.test.ts` | 6 | `formatSpatialIndexingNotice` |
| `src/spatialIndex.test.ts` | 11 | `clearSpatialIndex2D`, `createSpatialIndex2D`, `initializeSpatialIndex2D`, `insertSpatialObject2D`, `querySpatialPairs2D`, `querySpatialPoint2D`, `querySpatialRay2D`, `querySpatialRegion2D`, `removeSpatialObject2D`, `updateSpatialObject2D` |
| `src/spatialIndex3D.test.ts` | 21 | `clearSpatialIndex3D`, `createSpatialIndex3D`, `initializeSpatialIndex3D`, `insertSpatialObject3D`, `querySpatialFrustum3D`, `querySpatialPairs3D`, `querySpatialPoint3D`, `querySpatialRay3D`, `querySpatialRegion3D`, `querySpatialSphere3D`, `removeSpatialObject3D`, `updateSpatialObject3D` |
| `src/spatialIndexingGuard.test.ts` | 11 | `reportSpatialIndexing`, `setSpatialIndexingGuard` |
| `src/uniformGrid.test.ts` | 41 | `brute-force property coverage`, `cell range across every transition that can strand it`, `createUniformGridSpatialBackend2D`, `initializeUniformGridSpatialBackend2D`, `MAX_INDEXED_CELLS_PER_OBJECT`, `non-finite bounds`, `oversized region query`, `oversized-extent bound`, `ray edge cases`, `updateSpatialObject2D` |
| `src/uniformGrid3D.test.ts` | 32 | `createUniformGridSpatialBackend3D`, `initializeUniformGridSpatialBackend3D`, `querySpatialPairs3D`, `querySpatialPoint3D`, `querySpatialRay3D`, `querySpatialRegion3D`, `updateSpatialObject3D` |

Upstream carries per-package records at `flight/agents/packages/spatial/`: `status.md`, `assessment.md`, `review.md`, `charter.md`.

### Existing CTest coverage

- `flight_cpp_upstream_spatial_smoke` — `examples/upstream/spatial.cpp` (dedicated)

### Local generation status

10 of 11 modules emitted, 1 refused; 3 refusing modules (1 of them refusal roots). Read from the committed generated tree, not from the pin above.

### Web-only exclusions

None declared. The package ships no `flight.environment` declaration, so it is environment-agnostic and applicable to the native SDL host; nothing in it is excluded for being web-shaped.

## Lane notes

### Upstream examples

6 of the 34 upstream examples are ported and smoke-tested.
 4 belong to this lane: `clock` (`flight_cpp_upstream_clock_smoke`), `motionpath` (`flight_cpp_upstream_motionpath_smoke`), `collision` (`flight_cpp_upstream_collision_smoke`), `spatial` (`flight_cpp_upstream_spatial_smoke`) — each port's source includes the lane package's own headers.

A further 5 examples are in that inventory whose subject is a package in this lane, and are listed as awaiting a faithful SDL port: `flowstates`, `movieclip`, `particles`, `spring`, `tween`. The mapping is by example name, or by an import of the package where the name differs (`flowstates` imports `@flighthq/flow`); the 30 other examples whose imports go through the `@flighthq/sdk` barrel are not attributed to any lane here.

This lane's largest upstream suites are `skeleton2d-formats` (533 cases), `collision` (500 cases), `physics2d` (457 cases).
Only the ported examples above have a local counterpart, so the rest of the lane is backed by upstream tests alone.

### Upstream functional scenes

Upstream also asserts some of these packages visually, as image-diff cells under
`flight/functional/scenes/`. A cell is a browser-backend scene, so it is evidence of the
assertion that exists upstream, not coverage this repository can run; it is listed because it
names behaviours the unit suite does not.

| scene | what its header says it proves |
| --- | --- |
| `particle-emitter.canvas.ts / .webgl.ts / .webgpu.ts` | batched `ParticleEmitter` draw: each particle draws its own region, position, scale, and rotation from the emitter's arrays, with the gaps between particles left as background |
| `particle-emitter-color.webgl.ts / .webgpu.ts` | the per-particle colour-tint path, which Canvas 2D cannot honour |
| `particle-emitter-3d.webgl.ts / .webgpu.ts` | the 3D particle emitter draw |
| `particle-motion-blur.webgl.ts / .webgpu.ts` | particles under a motion-blur pass |

Beyond those subjects, scene files import these lane packages incidentally:

- `@flighthq/particles` — 3 files (`particle-emitter.canvas.ts`, `particle-emitter.webgl.ts`, `particle-emitter.webgpu.ts`)

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
