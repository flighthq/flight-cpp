# Work queue

Status: **DRAFT — awaiting user approval** (see `index.md`).

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

## Phase 0 — Re-pin (serialized; blocks generated-tree work)

**Q0 · GEN · Move the flight pin to upstream main.** Lock flight at a62784923f3be814463286c9fb28edfe6ab15789
(or main at the time, recorded). Rehydrate, regenerate the SDK, and fix regressions so that headers,
tiers and tests are **no worse** than baseline at 846fa56 (2,173 headers; 61 shippable; CTest 39/39)
— or every regression is explained and listed. Re-validate every source patch against the new base
(drop obsolete, re-derive broken); refresh the override drift list; re-inventory upstream examples
(the count may have changed from 34). Pin move + regenerated output in their own commit(s), per `AGENTS.md`.
Publish the new baseline numbers, per-package tier table and override drift list as
`docs/baseline-<pin>.md` for every lane to start from.
This is the one builder task exempt from the whole-SDK restriction; still `-j4` max.
Accept: lock moved; `sdk:check`, `overrides:check` report reviewed; baseline doc committed.

### Runs in parallel with Q0 (does not depend on regenerated output)

**Q1 · GEN · Consolidate compiler requests.** Split `docs/upstream-flight-compiler-request.md`
and `docs/compiler-message-b05f4df.md` into `docs/compiler-requests/<slug>.md`, one defect each, with
the minimal TS repro, expected C++, and the patches/overrides/repairs carrying it. Add an index that
maps every `source-patches/compiler-*.patch` to its request. This index is what the user sends upstream.

**Q2 · each lane · Parity-test inventory.** For each lane, read the upstream package tests at the
new pin (read-only, from `.dependencies` once Q0 rehydrates, or a separate read-only checkout) and
write `docs/parity/<lane>.md`: per package, the upstream test files, the behaviors they assert, which
are already covered by our CTest, and which are web-only and not applicable. No builds.
This is the work list for Phase 2.

**Q3 · GEN · Investigate mechanical parity tests.** Determine whether upstream `*.test.ts` files (or a
subset of them) can be lowered by the pinned compiler into C++ tests against the generated SDK, with
a small native harness for the test framework's `describe/it/expect` surface. Report the feasible
fraction on two pilot packages (suggest math, geometry). If it works, it replaces most of the
hand-porting in Phase 2. Prototype only; no whole-SDK runs.

**Q4 · CPU hygiene tooling.** Add scoped helpers so staying in lane is the easy path: e.g.
`npm run pkg:compile -- <package>` (headers of one package), `npm run pkg:tier -- <package>`, and
a scoped CMake/CTest target per package's tests. Default to `-j2`.

---

## Phase 1 — Packages to green (per lane, after Q0)

For each lane: drive every package to **done** (definition in `index.md`, points 1–3), largest
refusal count first. Foreman may split a lane into one item per package or package group. Each item
updates the baseline doc's row for its package on completion (via its commit; integration republishes the totals).

- **P-GL1 · GL ·** render, render-gl, texture, shading, materials, lighting (unblocks 2D and 3D).
- **P-GL2 · GL ·** effects, effects-gl (54/61 refused at old pin; needed by D2 and Scene3D presentation).
- **P-GL3 · GL ·** image, image-codec, texture-formats, surface, capture, snapshot, quadbatch, color, adjustments.
- **P-2D1 · 2D ·** scene2d, scene2d-gl (D2: the native 2D renderer). Needs P-GL1.
- **P-2D2 · 2D ·** node, shape, path, path-boolean, bitmap, clip, camera, camera-controls, entity.
- **P-2D3 · 2D ·** scene2d-formats, scene2d-resources, scene-document, shape-formats, path-formats.
- **P-2D4 · 2D ·** tilemap, textureatlas, spritesheet (+ formats), binpack, gizmo, selection.
- **P-3D1 · 3D ·** scene3d, scene3d-gl: materials beyond StandardPbr, instancing, particles path, picking. Needs P-GL1.
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
- **P-PL3 · PLAT ·** the desktop/service packages (dialog … connectivity in the lane table): native implementation, or an honest fail-loud capability where SDL has none, recorded in `docs/`.
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

## Phase 3 — Faithful example ports (e2e acceptance)

Each item: a faithful SDL port per `index.md`, added to `UPSTREAM_EXAMPLES`, with a `--smoke` CTest.
Owned by the lane of the example's subject; starts when that lane's packages for it are done.
Q0's re-inventory adds any new upstream example here.

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

## Phase 4 — WGPU and hardening (after Phases 1–3 are substantially green)

- **W1 · WGPU ·** Choose and integrate a device provider (wgpu-native or Dawn) behind the optional SDL host. The runtime stays dependency-free.
- **W2 · WGPU ·** render-wgpu, effects-wgpu, scene2d-wgpu, scene3d-wgpu to done; examples run on both backends.
- **H1 · integration ·** Debug, Release, ASan+UBSan CTest runs, staggered to stay within the CPU budget.
- **H2 · integration ·** Clang and GCC consumer builds; Bazel inventory parity with CMake.
- **H3 · GEN ·** Pin-move drill: document and script a repeatable re-pin, so the next upstream move is routine.

---

## Standing duties (not queued items)

- **Integration** owns all whole-SDK runs. After each merge batch: scoped checks for what changed;
  periodically, the full header sweep + tiers + CTest, published to `docs/baseline-<pin>.md` (totals
  only) so lanes read results instead of recomputing them.
- **Auditor** (on principal's request): faithfulness audits of example ports, override honesty,
  and whether parity tests actually assert upstream behavior.
