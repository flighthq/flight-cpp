# flight-cpp fleet plan

Status: **DRAFT — awaiting user approval.** Nothing in `queue.md` is authorized until this line reads APPROVED.

This directory is the contract for the execution fleet. `index.md` holds the goal, decisions and
standing rules; `queue.md` holds the work items. Repository rules in `AGENTS.md` still apply and win
on anything technical.

## Goal

Every environment-agnostic upstream Flight package exists in C++ and **behaves as the original**.
"Behaves" means proven by tests, not by compiling. Upstream examples run natively as faithful,
end-to-end ports that use the same Flight APIs as the TypeScript source.

Baseline (builder audit, base 846fa56, flight pin 7e2fc7d):

- 154 packages; 150 applicable; 4 web-only (effects-canvas, scene2d-canvas, scene2d-dom, textshaper-canvas).
- 61/150 shippable (25 ready, 36 assisted), 89 partial.
- 2,173/2,911 public headers compile; 561 modules refused; 54 overrides drifted.
- 6/34 upstream examples have faithful ports; 23 await a port; 5 are blocked on Scene3D GL.
- ~25 packages have behavioral tests.

## Decisions (user, 2026-10-11)

- **D1 — Pin.** The flight pin is 445 commits behind upstream main (7e2fc7d → a627849). Move it
  first, in one serialized item (Q0). flight-compiler is current (2b687ea) and stays pinned.
- **D2 — 2D rendering.** Native 2D renders through **scene2d-gl** (+ effects-gl). A 2D example port
  may change its renderer selection from Canvas to GL. That is the only permitted deviation from the
  upstream source. No native Canvas2D backend is planned.
- **D3 — Compiler defects.** Work stays inside flight-cpp: runtime extension, external bindings,
  source patches, repairs, overrides, in the order `AGENTS.md` prescribes. Every compiler defect
  found is also written up as a request (see "Compiler requests"). The fleet does not move the
  flight-compiler pin; the user does, after landing fixes upstream.
- **D4 — WGPU.** GL first. WGPU packages (render-wgpu, scene2d-wgpu, scene3d-wgpu, effects-wgpu)
  are queued after the GL stack is green, as an optional host library. Until then they stay partial
  with honest fail-loud boundaries.

## Definition of done for a package

A package is **done** when all hold, verified by its lane:

1. Every public header compiles independently (`sdk:compile --headers=` scoped to the package).
2. Tier is `ready`, or `assisted` with zero drifted overrides and no `incomplete` override.
3. No refused module lacks a faithful supply; remaining refusals are recorded in `docs/` with a
   compiler request.
4. Behavioral parity tests exist (CTest) covering the package's upstream test intent: absence,
   identity, equality, ordering, exceptions and task settlement where the package exercises them.
5. Any example that exercises the package as its subject runs faithfully (or is listed as blocked
   with the reason).

## Standing rules

### CPU budget — mandatory

The machine is shared by ~13 agents (16 cores, 23 GB). Builds of the whole SDK take hours under contention.

- **Stay in your package.** Compile only the headers, tests and targets of the package(s) you are
  working on: `npm run sdk:compile -- --headers=@list`, `sdkGeneration.mjs --package=…`,
  `cmake --build build --target <target>`.
- **Max `-j2`** for any build or test command a builder runs.
- **No whole-SDK operations by builders.** Full `sdk:generate`, an unscoped `sdk:compile`, full
  `sdk:tiers` regeneration, full CMake build and full CTest belong to **integration** only (and
  to Q0's owner during the pin move). Integration runs them at most once per merge batch, `-j4` max,
  never two at once.
- Use a per-agent build directory and keep it, so builds stay incremental. Do not wipe it to "be sure".
- A builder that needs a whole-tree answer (e.g. reverse-include closure) asks integration for the
  latest report instead of producing one.

### Lanes

Each work item belongs to a lane (see `queue.md`). A lane owns its packages' generated output,
overrides, patches and tests. Touching another lane's package is allowed when unavoidable; keep the
change minimal and say so in the commit message. Foreman assigns builders to lanes. One builder per
lane at a time is the default, so two builders don't fight over the same files.

### Faithful examples

A port is faithful when its state and algorithms come from the generated Flight APIs the TypeScript
source uses. SDL may replace only the browser host: window, input events, presentation, and the
Canvas→GL renderer selection (D2). Recreating a subsystem locally, skipping features the original
shows, or substituting a look-alike is not a port. Each port names its TypeScript authority at the
top and lists in its header comment any feature it cannot yet show and why. A port with such
gaps stays out of `UPSTREAM_EXAMPLES` until the gaps are closed.

### Compiler requests

Record each flight-compiler defect as one file `docs/compiler-requests/<short-slug>.md`: minimal
TypeScript reproduction, observed vs expected C++, affected modules, and the workaround that carries
it here (patch/override/repair name). One file per defect avoids collisions between lanes. Q14
consolidates the existing `docs/upstream-flight-compiler-request.md` into that form.

### Honesty

Never mark a tier, test or example green by weakening the check. A gap is recorded, not hidden.
`result: fail` with a reason beats a false pass.
