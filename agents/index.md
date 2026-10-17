# flight-cpp fleet plan

Status: **APPROVED** by the user, 2026-10-11. `queue.md` is authorized work.

This directory is the contract for the execution fleet. `index.md` holds the goal, decisions and
standing rules; `queue.md` holds the work items. Repository rules in `AGENTS.md` still apply and win
on anything technical.

## Goal

Every environment-agnostic upstream Flight package exists in C++ and **behaves as the original**.
"Behaves" means proven by tests, not by compiling. The SDK is the target. Upstream example ports
are secondary evidence, and they count only when they genuinely run on the SDK packages they demonstrate.

Last full baseline (builder audit, base 846fa56, old flight pin 7e2fc7d). Not to be recomputed until Phase 5:

- 154 packages; 150 applicable; 4 web-only (effects-canvas, scene2d-canvas, scene2d-dom, textshaper-canvas).
- 61/150 shippable (25 ready, 36 assisted), 89 partial.
- 2,173/2,911 public headers compile; 561 modules refused; 54 overrides drifted.
- 6/34 upstream examples have faithful ports; 23 await a port; 5 are blocked on Scene3D GL.
- ~25 packages have behavioral tests.

## Decisions (user, 2026-10-11)

- **D1 — Pin.** The flight pin is 445 commits behind upstream main (7e2fc7d → a627849). Move it
  first (Q0), then each lane regenerates its own packages at the new pin. flight-compiler is current
  (2b687ea) and stays pinned.
- **D2 — 2D rendering.** Native 2D renders through **scene2d-gl** (+ effects-gl). A 2D example port
  may change its renderer selection from Canvas to GL. That is the only permitted deviation from the
  upstream source. No native Canvas2D backend is planned.
- **D3 — Compiler defects.** Work stays inside flight-cpp: runtime extension, external bindings,
  source patches, repairs, overrides, in the order `AGENTS.md` prescribes. Every compiler defect
  found is also written up as a request (see "Compiler requests"). The fleet does not move the
  flight-compiler pin on its own; the user decides each move (the 806d257 move is authorized as D8).
- **D4 — WGPU.** GL first. WGPU packages (render-wgpu, scene2d-wgpu, scene3d-wgpu, effects-wgpu)
  are queued after the GL stack is green, as an optional host library. Until then they stay partial
  with honest fail-loud boundaries.
- **D5 — Platform packages.** Ported like any other; the SDL host supplies only the seams SDL
  supports. See "Host seams, not stubs".
- **D6 — Global audits.** No whole-SDK runs during ordinary work, by anyone. Rare auditor runs at
  foreman's request; the full audit is finalization work (Phase 5).
## Decisions (user, 2026-10-15) — current priority

- **D7 — E2E first.** The top priority is a working end-to-end C++ example, built and run natively, as
  existed before the re-pin. Nothing has been built or run natively since the re-pin. Track A in
  `queue.md` owns this, and everything else ranks below it.
- **D8 — Compiler move is a mechanical migration, not a rewrite.** flight-compiler moves
  2b687ea → 806d257 (26 commits: a deterministic identifier rename, about 10 emitter fixes, and
  opt-in C ABI output). The move is done once, by one agent, whole-tree (Track M). Prior work
  carries forward by rename and three-way merge, never by re-deriving from scratch. A package that
  worked before and gets worse under the new compiler keeps its prior emission, so the move never
  makes anything worse. C ABI output stays off during the move; it is a separate later item.
- **D9 — Evidence is lean.** Effort goes to progress, not proof. See "Evidence" below. This
  supersedes the release-slot, archive and proof habits that grew up during the re-pin.
- **D6 amended.** Track M may run whole-tree generation and header sweeps while the rest of the
  fleet is frozen (see `queue.md`). Everyone else stays package-scoped.
- **Toolchains.** Any agent may install toolchains and dependencies it needs (cmake, a C++20
  compiler, SDL3, Mesa/EGL, …). Install only what your item needs.

- **D10 — Non-compiling headers are defects (user, 2026-10-17).** A public header that fails to
  compile is fixed here, by cause, not just reported. `generated/header-status.json` is the
  worklist, and the target is zero failures. Upstream-test porting is on hold pending the user's
  test strategy.

## Definition of done for a package

A package is **done** when all hold, verified by its lane:

1. The package is regenerated at the current pin (`--package=`) and every public header compiles
   independently (`sdk:compile --headers=` scoped to the package).
2. Its tier, computed for that package alone, is `ready`, or `assisted` with zero drifted overrides
   and no `incomplete` override.
3. No refused module lacks a faithful supply; remaining refusals are recorded in `docs/` with a
   compiler request.
4. Behavioral parity tests exist (CTest) covering the package's upstream test intent: absence,
   identity, equality, ordering, exceptions and task settlement where the package exercises them.
5. Any example that exercises the package as its subject runs faithfully (or is listed as blocked
   with the reason).

## Standing rules

### CPU budget — mandatory

The machine is shared by ~13 agents (16 cores, 23 GB). Builds of the whole SDK take hours under contention.

- **Everything is package-scoped.** Generate with `sdkGeneration.mjs --package=<pkg>` (it brings
  the package's dependency closure; add `--emit-path-prefix=` to narrow the expensive C++ emission),
  compile with `sdk:compile --headers=…`, build and test with named CMake targets and `ctest -R`.
- **Max `-j2`** for any build or test command.
- **Nobody runs whole-SDK operations during ordinary work** — no full `sdk:generate`/`sdk:check`,
  unscoped `sdk:compile`, full `sdk:tiers`, full CMake build or full CTest. Not builders, not
  integration. Integration runs scoped checks for what it merges and keeps merging; it must never
  become a queue behind a long run.
- **Full audits are rare and late.** Foreman may occasionally ask the auditor for one when a real
  question needs a whole-tree answer. Otherwise the full sweep belongs to finalization (Phase 5).
  Lanes make progress from package-scoped evidence; they do not wait for, or ask for, global numbers.
- Use a per-agent build directory and keep it, so builds stay incremental. Do not wipe it to "be sure".

### Evidence — mandatory (D9)

A change is shown by: the commit, one line in its message with the numbers that matter (headers
compiling before → after for the touched packages, tests run/passed, example smoke result), and
one `./agent.sh attest` for the check actually run. That is all.

Do **not** produce or relay: tarball archives, payload SHA manifests, proof JSON files, "foreign
proofs", content re-verification of already-landed commits, repeated baseline/candidate reruns of
an unchanged check, or evidence relays between agents. Do not re-run a check whose inputs did not
change. When something fails, say what failed in one line and fix it or record it; do not build
apparatus around it.

Integration merges anything whose stated gate passed and that does not break a build. It does not
re-run the author's checks and does not hold merges in serial "release slots", except during the
Track M freeze, where Track M is the only writer of `generated/`, `overrides/` and `repairs/`.

### Lanes

Each work item belongs to a lane (see `queue.md`). A lane owns its packages' generated output,
overrides, patches and tests. Touching another lane's package is allowed when unavoidable; keep the
change minimal and say so in the commit message. Foreman assigns builders to lanes. One builder per
lane at a time is the default, so two builders don't fight over the same files.

### Host seams, not stubs

Port every applicable package, including platform ones (dialog, tray, notification, menu, …). A
package's host-facing capability reaches the platform only through a host seam. The SDL host
implements the seams SDL actually supports, and nothing else. An unsupported capability is signalled
by the **missing seam**, not by a stub, fake or per-OS implementation. Those packages are complete
when their generated code is correct and their seams are declared; they wait for a future C++ host
that supplies the seam. Never fake a seam to make a package look working.

### Examples serve the SDK

The SDK is the target; examples are evidence that it works. Rank example work below package work.
What matters is that an example **actually exercises the packages it is named for**: a tilemap
example draws through `flight::tilemap`, a movieclip example plays through `flight::movieclip`, a 3D
example renders a Scene3D scene. An example that runs but bypasses its subject is worse than no
example, because it reports working code that does not exist.

A port is faithful when its state and algorithms come from the generated Flight APIs the TypeScript
source uses. SDL may replace only the browser host: window, input events, presentation, and the
Canvas→GL renderer selection (D2). It may not recreate a subsystem locally or substitute a look-alike.
Renderer selection means choosing scene2d-gl where upstream chose scene2d-canvas. It does **not**
permit replacing the upstream scene graph (Node2D, Shape, TextLabel, prepare/render calls) with an
immediate-mode painter: an example renders through the same scene and render pipeline as its source.
Application state uses the generated Flight types the source uses, not local mirrors of them.
Each port names its TypeScript authority at the top and lists any feature it cannot yet show and why.

### Compiler requests

Record each flight-compiler defect as one file `docs/compiler-requests/<short-slug>.md`: minimal
TypeScript reproduction, observed vs expected C++, affected modules, and the workaround that carries
it here (patch/override/repair name). One file per defect avoids collisions between lanes. Q1
consolidates the existing `docs/upstream-flight-compiler-request.md` into that form.

### Honesty

Never mark a tier, test or example green by weakening the check. A gap is recorded, not hidden.
`result: fail` with a reason beats a false pass.
