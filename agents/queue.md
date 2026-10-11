# Work queue

Status: **APPROVED** by the user, 2026-10-11 (see `index.md`).

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

Any package Q0 finds at the new pin that is missing from this table goes to the lane closest in kind;
Q0 reports the assignment.

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
