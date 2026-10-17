# Work queue

Status: **APPROVED** by the user, 2026-10-11; **NEXT** section (2026-10-17) takes precedence (see `index.md` D7–D9).

Foreman divides and dispatches; builders own implementation. Rules, CPU budget and the package
definition of done are in `index.md`. Items are ordered by priority within each phase. `Needs:`
names items that must land first.

## Lanes

| Lane | Packages |
| --- | --- |
| **GEN** — generation & pins | pin, `scripts/`, emission repairs, source-patch hygiene, compiler requests, tier tooling |
| **GL** — render core | render, render-gl, texture, texture-formats, shading, materials, lighting, effects, effects-gl, quadbatch, surface, image, image-codec, color, adjustments, capture, snapshot |
| **2D** — scene graph & 2D content | scene2d, scene2d-gl, scene2d-formats, scene2d-resources, node, shape, shape-formats, path, path-formats, path-boolean, bitmap, clip, camera, camera-controls, entity, scene-document, gizmo, selection, tilemap, tilemap-formats, textureatlas, textureatlas-formats, spritesheet, spritesheet-formats, binpack |
| **3D** — Scene3D | scene3d, scene3d-gl, scene3d-formats, scene3d-resources, mesh, skeleton3d, physics3d, physics3d-abi, picking |
| **TEXT** — text & fonts | text, text-markup, textlayout, textshaper, textbidi, textsegment, textinput, font, font-formats, glyphatlas, bitmapfont, bitmapfont-formats, bitmaptext, intl, tokens |
| **ANIM** — motion & simulation | animation, timeline, tween, spring, easing, velocity, motionpath, clock, movieclip, swf, skeleton2d, skeleton2d-formats, particles, particleemitter, particles-formats, flow, statechart, physics2d, physics2d-abi, collision, spatial |
| **PLAT** — host, app & devices | app, host, platform, lifecycle, input, keyboard, interaction, gui, layout, accessibility, audio, media, mediasession, video, webcam, midi, sensors, geolocation, haptics, device, screen, power, clipboard, dialog, menu, tray, statusbar, notification, share, shortcut, shell, ipc, protocol, permissions, filesystem, preferences, updater, useragent, connectivity |
| **CORE** — data & IO | math, geometry, types, signals, command, encoding, compression, xml, abc, net, socket, loader, assets, registry, registry-catalog, registry-codegen, log, debug, requirements, importdiagnostics |
| **WGPU** (phase 4) | render-wgpu, scene2d-wgpu, scene3d-wgpu, effects-wgpu |

The table is exhaustive. A package's lane is the row it is listed in, whatever its name suggests:
renderers belong to their dimension (scene2d-gl → 2D, scene3d-gl → 3D), while GL owns the shared
render core. Mixed packages are placed as listed: camera, camera-controls, path → 2D; collision,
spatial, motionpath, clock → ANIM; geometry, math → CORE.

Any package Q0 finds at the new pin that is missing from this table goes to the lane closest in kind;
Q0 reports the assignment.

---

## NEXT — current priority (user, 2026-10-17). Supersedes NOW, which is complete.

NOW is done: Track A (6/6 examples build and smoke, camera2d and scene3d on GL) and Track M
(compiler 806d257, `docs/compiler-806d257.md`) both landed. Evidence rule D9 still applies.

**Upstream-test porting is ON HOLD** (user, 2026-10-17): no work on lowering `*.test.ts`, extending
`upstreamTestParityPrototype`, or new parity-test infrastructure until the user settles test strategy.
Existing tests keep running.

### G · Every public header compiles — the main fleet work

A header that does not compile is a defect to fix here, not a status to publish (D10).

G1 · GEN (builder6). One full header sweep at the current base (`sdk:compile:sdl`, `-j8`, one-off;
    other agents stay out of heavy builds while it runs). Commit:
    - `generated/header-status.json`: every public header, pass/fail, and for failures the first
      diagnostic and a **root-cause cluster** id. Compact, sorted, deterministic.
    - `docs/header-failures.md`: clusters ranked by headers affected, each with its cause, an example
      diagnostic, owning lane(s), and the fix route (runtime / binding / compiler / override).
    Clustering is the point: group by cause (e.g. "`variant<ArrayBuffer, Uint8Array>` cast",
    "refused function still called", "missing runtime helper X"), not by package. Retry `-j8` crashes
    serially before classifying. The `-j8` crash, not the header, is the issue there.
    Refresh only the touched rows when a cluster is fixed; no second full sweep until all of G2 is done.

G2 · all builders, foreman assigns one cluster per builder, largest first. A cluster is done when
    every header in it compiles, shown by recompiling **only that cluster's headers**. Fix order
    is `AGENTS.md`'s: runtime extension → external binding → source patch / repair → override. Never
    make a header compile by deleting or stubbing behavior: a refused function stays declared-and-
    unlinkable (declare-refused-function), not faked.
    Compiler-shaped clusters: write one `docs/compiler-requests/<slug>.md` per cluster and tell
    foreman. Principal batches them for the user to land in flight-compiler. Carry a source patch
    only if it's cheap and clearly equivalent; otherwise leave the cluster open and move to the next one.
    The 12 `partial` carry overrides from the 806d257 move (`docs/compiler-806d257.md`, carry table) are
    in scope as their own clusters.
    Accept: `header-status.json` shows zero failures, or each remaining failure names a filed
    compiler request.

### H · Public SDL GL app surface (builder4) — before E

H1. Move the window / GL context / frame loop / present code that `camera2d` and `scene3d` use out of
    `examples/upstream/sdl_host.cpp` into the installed `Flight::HostSdlGl` library as a small public
    C++ API: create a window with a GL surface, run a per-frame callback with dt, resize, present,
    shut down. It must be enough for a downstream binding (flight-hx) to open a window and render a
    Flight scene without example code. The painter `Canvas` is **not** promoted. Port `camera2d` and
    `scene3d` onto it; their smoke tests and PNGs must not change.

### E · Remaining 2D examples onto Scene2D GL (builder4, after H1)

E0a. Move clock, collision, motionpath and spatial onto Scene2D GL and the H1 surface, the way
     `b95afc2a` did camera2d: the upstream Node2D/Shape/TextLabel scene and render pass, not a painter.
     Then delete the painter's drawing code from `sdl_host.cpp`. E0b–E0d (faithfulness fixes and
     smoke tests that check the subject was driven) ride along per example. Save a PNG frame per example
     in `artifacts/e2e/`. Accept: all six examples run through GL, smoke tests pass, and the painter is gone.

### Staffing

builder6 → G1, then a G2 cluster. builder4 → H1 then E0a. Other builders → G2 clusters as G1
publishes them; idle until then. Integration merges per D9.

---

## NOW — COMPLETE (2026-10-16). Kept for record.

Two tracks, in this order of priority. Everything else waits or idles. Idle is fine: it frees CPU.
Evidence follows `index.md` "Evidence" (D9): commit, one numbers line, one attest. Nothing more.

### Staffing (principal's recommendation; foreman may substitute an equivalent free agent)

| Agent | Work |
| --- | --- |
| builder4 | **Track A** (e2e). Then rebuilds examples on Track M's candidate (M6). |
| builder6 | **Track M** (compiler move), sole writer of `generated/`, `overrides/`, `repairs/` during the freeze. |
| builder7 | Finish Snapshot landing now, then idle. |
| builder3 | Stop scene2d-resources work. Hand its **source patches + new test only** to builder6 as files; M3 regenerates with them. No re-proof. |
| builder (SceneDocument), effects-wgpu, scene3d-wgpu prep | **Dropped / parked.** M3 regenerates every package anyway; WGPU is Phase 4. Re-evaluate after M lands. |
| everyone else | Idle until M lands. No builds. |
| integration | Merges Snapshot, then Track A fixes, then the single Track M parcel. Scoped merge only. |

### Track A · E2E native examples (builder4) — top priority, starts now

A1. Install a native toolchain in your sandbox: cmake, ninja, a C++20 compiler (gcc or clang), SDL3
    development package, Mesa (EGL + software GL / llvmpipe). Use the distro packages; build SDL3 from
    source only if no package exists.
A2. At the current base, configure as in `examples/upstream/README.md` (`BUILD_TESTING`,
    `FLIGHT_CPP_BUILD_EXAMPLES`, `FLIGHT_CPP_BUILD_HOST_SDL`), build `flight_cpp_upstream_examples`
    at `-j4` in a persistent build dir, and run the six `--smoke` CTests.
A3. **scene3d first.** Make it build and smoke-pass at the current pin. Then camera2d, clock, collision,
    motionpath, spatial. Fix breakage by the normal `AGENTS.md` order (runtime → binding → patch →
    repair → override). Touch any lane's files as needed (cross-lane is authorized for Track A).
    Do not weaken a smoke assertion to pass.
A4. Capture one rendered frame of scene3d to `artifacts/e2e/scene3d.png` (software GL offscreen) so the
    user can see it. Same for one 2D example.
A5. Land fixes as they go, small commits, so Track M builds on them. Report to foreman: which of the 6
    build, which smoke-pass, with the PNG path. That list is the **E2E baseline** that M6 must not regress.
Accept: scene3d builds, smoke passes, and the PNG exists. Report the other five honestly either way.
Not in scope: E0a–E0d rework (painter → scene2d-gl). That comes after.

### Track M · flight-compiler 2b687ea → 806d257 (builder6) — starts now, in parallel

Principle: **rename first, then regenerate, so that the diff shows only real emitter changes**, and
carry every prior workaround forward by three-way merge. The new compiler changes the names of
dimensional identifiers (`camera2_d` → `camera_2d`, `scene3_dresource` → `scene_3d_resource`,
`tex_image2_d` → `tex_image_2d`); the old `snakeCase` and new `convertCppIdentifierToSnakeCase` in
`compiler-backend-cpp/src/cppCompilerIdentity.ts` are both deterministic. The remaining ~10 commits are
targeted emitter fixes; C ABI output is opt-in CLI and stays **off**.

M1. **Rename tool** (no tree writes yet; do this while Track A works). `scripts/` tool that computes
    old→new identifier pairs by applying the old and new conversion to every identifier in the pinned
    flight TypeScript (and to synthesized compounds the emitter builds from those names), and rewrites
    any working tree with it: file contents **and** file names, across `generated/`, `overrides/`,
    `repairs/`, `examples/`, `tests/`, `src/`, `include/`, `bindings/`, `abi/`. Not `source-patches/`
    (they patch TypeScript). Whole-identifier matches only; a token like `foo2_data` that is not in
    the map must not be touched. The tool must be re-runnable on any agent's branch, so in-flight work
    can be renamed rather than redone.
M2. **Freeze and baseline.** Once Snapshot and the Track A fixes so far have landed, the freeze begins: no one
    else writes `generated/`, `overrides/`, `repairs/`. Run one full header sweep (`sdk:compile:sdl`,
    `-j8` allowed during the freeze) at the base to get **per-package headers-compiling counts**. Reuse
    an existing report at this exact base if one exists.
M3. **Apply the rename** to the base tree as one mechanical commit (pin unchanged). Then move the
    flight-compiler pin to 806d257, rehydrate, and run **one** full `sdk:generate` (with builder3's
    scene2d-resources patches) as a second commit.
M4. **Read the diff.** `git diff` between the renamed-old tree and the new one, summarized per package.
    Any leftover rename-shaped difference means the map is incomplete: fix the tool and redo M3 (cheap).
    A package with no diff is done. For the rest, the diff is the list of emitter changes to check.
M5. **Carry workarounds forward mechanically.**
    - Overrides whose generated source changed: `git merge-file <override> <renamed-old generated>
      <new generated>`. A clean merge is accepted and `derivedFrom` refreshed. A conflict is the
      "one minor tweak" case: resolve it by hand, from the diff, in minutes. Never rewrite from scratch.
    - Repairs that now match nothing (`sdk:check` reports them): drop them if the emitter now emits the
      declaration, otherwise re-anchor them to the new text.
    - Source patches: leave them alone. Report any whose module the new compiler now lowers unpatched,
      as a later cleanup list.
M6. **Never regress.** Re-run the header sweep. Any package whose headers-compiling count dropped,
    and which a quick fix from the diff does not restore, **keeps its prior emission**: commit the
    renamed-old header as an override of the new one (`status` `partial`, note "prior emission kept
    pending compiler 806d257 regression", `derivedFrom` = new generated hash) and file a one-file
    compiler request. builder4 then rebuilds the examples on the candidate. The E2E baseline from A5
    must still hold.
M7. Land as **one parcel** (rename commit, pin+regen commit, merge/repair commit). Write
    `docs/compiler-806d257.md`: per-package headers before → after, overrides auto-merged vs
    hand-resolved vs kept-prior, repairs dropped, examples result, source patches now obsolete. The
    pin move gets its own commit, per `AGENTS.md`. The freeze ends when it lands; idle agents then
    rebase and run the M1 tool on any unmerged work.
Accept: pin at 806d257; `sdk:check` and `overrides:check` pass; no package compiles fewer headers than
at M2; the E2E baseline holds.

---

## Phase 0 — Re-pin

**Q0 · GEN · Move the flight pin to upstream main.** Lock flight at a62784923f3be814463286c9fb28edfe6ab15789
(or main at the time, recorded) and rehydrate. Do **not** regenerate the whole SDK. Instead:
- Produce `docs/repin-<short-sha>.md`: the upstream packages and examples changed between the old and
  new pin (from `git diff --stat` of the two commits, no builds), new or removed packages and examples,
  and which lane each belongs to. Lanes start from this.
- Regenerate, package-scoped, only the foundation packages most others depend on (e.g. types, math,
  geometry, signals; whatever `--package=` closure order shows is shared) and fix what breaks, so lanes
  regenerate on a sound base.
- Check every source patch for whether it still applies at the new pin (an apply check, not a build).
  Drop the obsolete ones, mark the broken ones for their owning lane, and list both in the repin doc.
Pin move in its own commit, per `AGENTS.md`. After Q0, each lane regenerates its own packages as part
of Phase 1.
Accept: lock moved; repin doc committed; foundation packages regenerated and compiling.

### Runs in parallel with Q0 (does not depend on regenerated output)

**Q1 · GEN · Consolidate compiler requests.** Split `docs/upstream-flight-compiler-request.md`
and `docs/compiler-message-b05f4df.md` into `docs/compiler-requests/<slug>.md`, one defect each, with
the minimal TS repro, expected C++, and the patches/overrides/repairs carrying it. Add an index that
maps every `source-patches/compiler-*.patch` to its request. This index is what the user sends upstream.

**Q2 · each lane · Parity-test inventory.** For each lane, read the upstream package tests at the
new pin (read-only, from `.dependencies` once Q0 rehydrates) and
write `docs/parity/<lane>.md`: per package, the upstream test files, the behaviors they assert, which
are already covered by our CTest, and which are web-only and not applicable. No builds.
This is the work list for Phase 2.

**Q3 · GEN · Investigate mechanical parity tests.** Determine whether upstream `*.test.ts` files (or a
subset of them) can be lowered by the pinned compiler into C++ tests against the generated SDK, with
a small native harness for the test framework's `describe/it/expect` surface. Report the feasible
fraction on two pilot packages (suggest math, geometry). If it works, it replaces most of the
hand-porting in Phase 2. Prototype only; no whole-SDK runs.

**Q4 · GEN · Package-scoped tooling.** Make staying in lane the easy path:
`npm run pkg:generate -- <package>` regenerates one package and replaces **only that package's
subtree and manifest rows** in `generated/`. Its dependency closure is used for resolution but not
written over other lanes' output. Add `npm run pkg:compile -- <package>`, `npm run pkg:tier -- <package>`,
and a per-package CTest label. Default to `-j2`. Highest priority of the parallel items, because every
lane uses it.

---

## Phase 1 — Packages to green (per lane, after Q0)

For each lane: drive every package to **done** (definition in `index.md`, points 1–3), largest
refusal count first. Foreman may split a lane into one item per package or package group. Each item
starts by regenerating its packages at the new pin with `pkg:generate`. Completion is shown by
package-scoped evidence in the commit message (headers compiled, tier, tests), not global totals.

- **P-GL1 · GL ·** render, render-gl, texture, shading, materials, lighting (unblocks 2D and 3D).
- **P-GL2 · GL ·** effects, effects-gl (54/61 refused at old pin; needed by D2 and Scene3D presentation).
- **P-GL3 · GL ·** image, image-codec, texture-formats, surface, capture, snapshot, quadbatch, color, adjustments.
- **P-2D1 · 2D ·** scene2d, scene2d-gl (D2: the native 2D renderer). Needs P-GL1.
- **P-2D2 · 2D ·** node, shape, path, path-boolean, bitmap, clip, camera, camera-controls, entity.
- **P-2D3 · 2D ·** scene2d-formats, scene2d-resources, scene-document, shape-formats, path-formats.
- **P-2D4 · 2D ·** tilemap, textureatlas, spritesheet (+ formats), binpack, gizmo, selection.
- **P-3D1 · 3D ·** scene3d, scene3d-gl: materials beyond StandardPbr, instancing, particles path. Needs P-GL1.
- **P-3D2 · 3D ·** scene3d-formats, scene3d-resources (glTF, OBJ, AWD2…), mesh, skeleton3d.
- **P-3D3 · 3D ·** physics3d, physics3d-abi, picking.
- **P-TX1 · TEXT ·** font, font-formats, glyphatlas, textshaper, textbidi, textsegment, intl.
- **P-TX2 · TEXT ·** text, text-markup, textlayout, textinput, bitmapfont(+formats), bitmaptext (currently recorded functional gaps), tokens.
- **P-AN1 · ANIM ·** animation, timeline, tween, spring, easing, velocity, motionpath, clock.
- **P-AN2 · ANIM ·** movieclip, swf, skeleton2d(+formats).
- **P-AN3 · ANIM ·** particles, particleemitter, particles-formats, flow, statechart.
- **P-AN4 · ANIM ·** physics2d(+abi), collision, spatial.
- **P-PL1 · PLAT ·** app, host, platform, lifecycle, input, keyboard, interaction, gui, layout, accessibility.
- **P-PL2 · PLAT ·** audio, media, mediasession, video, webcam, midi (SDL-backed where a host is needed).
- **P-PL3 · PLAT ·** the desktop/service packages (dialog … connectivity in the lane table): port them
  faithfully; the SDL host implements only the seams SDL supports, and the rest are declared and left
  unsupplied (`index.md`, "Host seams, not stubs"). Record in `docs/host-sdl.md` which seams SDL
  supplies and which await another host.
- **P-CO1 · CORE ·** math, geometry, types, signals, command, encoding, compression (carry upstream's LZMA fix), xml, abc.
- **P-CO2 · CORE ·** net, socket, loader, assets, registry, registry-catalog, registry-codegen, log, debug, requirements, importdiagnostics.

**P-DRIFT · owning lane ·** Every drifted override is re-derived from its current generated source
or dropped. Each lane clears its own; `overrides:check` must pass when all are done.

---

## Phase 2 — Behavioral parity (per lane, from the Q2 inventory; Q3 decides the method)

**T-<lane> ·** Add CTest coverage for each package's upstream test intent (definition of done, point 4).
Prioritize: (a) semantics this runtime exists to preserve — absence, identity, equality, ordering,
exceptions, task settlement; (b) data formats and decoders (byte-exact against upstream fixtures);
(c) math/geometry numerics; (d) everything else. Tests run scoped per package. One item per lane;
foreman may split by package. Can start for any package already done in Phase 1.

**T-GOLD · GL ·** Native render golden images: an offscreen GL capture harness (the existing
software-GL path) and committed reference images for each faithful example's first N frames, with a
tolerance. Keeps rendering from silently regressing. References come from our own verified runs;
mark any frame whose correctness was not checked by eye.

---

## Phase 3 — Example ports (secondary to Phase 1–2)

Examples serve the SDK (`index.md`). An example item starts only when its lane has no package work
outstanding that would be more valuable. Each port must visibly **exercise its subject package**: the
tilemap draws through `flight::tilemap`, movieclip plays through `flight::movieclip`, the 3D examples
render Scene3D. Each port lands in `UPSTREAM_EXAMPLES` with a `--smoke` CTest whose assertion checks the
subject package was actually driven (e.g. frames advanced, tiles drawn, nodes rendered), not just that
the process exited 0. A port that can't use its subject yet stays out, with the blocker recorded.

**E0 · existing ports · rework after the audit of 2026-10-11.** None of the six is synthetic, but:
- **E0a · 2D · Needs P-2D1.** camera2d, clock, collision, motionpath and spatial draw through an
  immediate-mode painter in `examples/upstream/sdl_host.cpp` instead of the upstream
  Node2D/Shape/TextLabel scene and its render pass. Move them onto scene2d + scene2d-gl, then retire
  the painter's shape and text code. Use the upstream surface size (800×500 for clock, motionpath and spatial).
- **E0b · ANIM ·** collision: keep collider state in the generated CollisionCircle2D/Aabb2D/Polygon2D
  records, not a local struct; use upstream's convex hit test. spatial: `Math.random` for interactive
  runs and the seeded generator only in capture; dt clamp 0.05; latched input mode; capture mouse at
  (400,250). camera2d: drive the smoke capture through `zoom_camera2_dat_screen_point`, not a direct
  field write; draw the returned visible-bounds rectangle.
- **E0c · 3D ·** scene3d: call `enable_flight_diagnostics`; render once after the dt=1 controller
  update, before the first timed update, as upstream does.
- **E0d · all ·** Smoke tests assert the subject was driven, e.g. clock elapsed and pause state,
  motion-path progress and position, collision hit count and manifold, spatial query hits, camera
  bounds. A test that only exits 0 doesn't count. E0b–E0d don't wait for P-2D1.

| Item | Example | Lane | Needs |
| --- | --- | --- | --- |
| E1 | shapes | 2D | P-2D1, P-2D2 |
| E2 | tween | ANIM | P-AN1, P-2D1 |
| E3 | spring | ANIM | P-AN1, P-2D1 |
| E4 | text | TEXT | P-TX2, P-2D1 |
| E5 | textinput | TEXT | P-TX2, P-PL1 |
| E6 | bitmapfont_generate | TEXT | P-TX2 |
| E7 | interaction | PLAT | P-PL1, P-2D1 |
| E8 | pathboolean | 2D | P-2D2 |
| E9 | spritesheet | 2D | P-2D4 |
| E10 | tilemap | 2D | P-2D4 |
| E11 | movieclip | ANIM | P-AN2 |
| E12 | flowstates | ANIM | P-AN3 |
| E13 | particles | ANIM | P-AN3 |
| E14 | particleeditor | ANIM | P-AN3, P-PL1 (gui) |
| E15 | platformer | ANIM | P-AN4, P-2D4 |
| E16 | adjustments | GL | P-GL2, P-GL3 |
| E17 | crossfade | GL | P-GL2 |
| E18 | snapshot | GL | P-GL3 |
| E19 | formatloading | CORE | P-CO2, P-2D3 |
| E20 | sound | PLAT | P-PL2 |
| E21 | video | PLAT | P-PL2 |
| E22 | benchmark | 2D | P-2D1 |
| E23 | cross_backend_embed | 2D | P-2D1, P-3D1 |
| E24 | materialshowcase | 3D | P-3D1 |
| E25 | instanced_mesh | 3D | P-3D1 |
| E26 | scene_fire | 3D | P-3D1, P-AN3 |
| E27 | scene_picking | 3D | P-3D3 |
| E28 | awd2loading | 3D | P-3D2 |

The existing tree's `examples/sound` and `examples/tween` are not upstream ports; reconcile them with E2/E20
(fold in or remove).

---

## Phase 4 — WGPU (after the GL stack is green)

- **W1 · WGPU ·** Choose and integrate a device provider (wgpu-native or Dawn) behind the optional SDL host. The runtime stays dependency-free.
- **W2 · WGPU ·** render-wgpu, effects-wgpu, scene2d-wgpu, scene3d-wgpu to done; examples run on both backends.

---

## Phase 5 — Finalization (far out; not before Phases 1–3 are substantially done)

- **F1 ·** Full-tree reproduction: whole `sdk:generate` / `sdk:check`, full header sweep, tiers, full
  CMake build and CTest. Reconcile any cross-lane inconsistency that package-scoped work missed.
- **F2 ·** Debug, Release, ASan+UBSan CTest; Clang and GCC consumer builds; Bazel inventory parity with CMake.
- **F3 ·** Pin-move drill: script a repeatable re-pin so the next upstream move is routine.
- **F4 ·** Full independent audit (auditor): tiers, override honesty, example faithfulness.

---

## Standing duties (not queued items)

- **Integration** merges continuously and runs only scoped checks for what each merge touches. It does
  not run whole-SDK operations and never holds merges behind a long run.
- **Auditor**, at principal's request or occasionally foreman's: faithfulness audits of example ports,
  override honesty, whether parity tests assert real upstream behavior, and, rarely, a whole-tree audit
  when a specific question needs one.
