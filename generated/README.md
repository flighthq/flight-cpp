# Generated Flight SDK inventory

This directory is generated from `@flighthq/sdk` during a pin transition, so its packages come from
different Flight revisions, each recorded on its row in `manifest.json`:

- `7e2fc7df5378313ceaf21ba8d6ef39cc18a6f4ad` (153 packages)
- `a62784923f3be814463286c9fb28edfe6ab15789` (1 package)

Every package was emitted by `flight-compiler` at `2b687eaf6b631e0fa4d7d6e18dad93b845f99633`.
Do not edit it by hand.

Applied binding profiles: `flighthq/flight-cpp/runtime-carriers/1`, `flighthq/flight-cpp/headless/1`, `flighthq/flight-cpp/web-types/1`, `flighthq/flight-cpp/sdl-image/1`, `flighthq/flight-cpp/sdl-gl/1`, `flighthq/flight-cpp/sdl-wgpu/1`, `flighthq/flight-cpp/sdl-app/1`. Exact profile paths and SHA-256 digests are recorded in `manifest.json`.

The current compiler emitted 2376 of 2937 source modules from
154 SDK packages and refused 561. Emitted headers live under
`include/flight/<package>/`; every refusal and its owning module is recorded in `refusals.json`.

This is a bring-up inventory. It is intentionally committed before it forms a completely compilable SDK closure.
CMake exposes the full inventory as `Flight::SdkPreview`, and Bazel exposes `//:sdk_preview`; the preview name
keeps the remaining native compile failures visible. The package graph applies the public C++ `flight` namespaces
and installed include prefixes. Package-local imports are written with their canonical installed paths so nested
includes honor the override directory's precedence. `initialization.json` records the compiler's dependency and
module-evaluation plan.

Regenerate and verify the tree from the repository root:

```sh
npm run rehydrate
npm ci --prefix .dependencies/flight-compiler
npm run sdk:generate
npm run sdk:check
```

Generate a profile-specific inventory outside the committed portable tree with:

```sh
node scripts/sdkGeneration.mjs --binding-profile=bindings/runtime.json --binding-profile=bindings/headless.json --output=out/sdk-headless
```

Add `--progress` to print the compiler phase and module before every synchronous lowering or
emission step. If an interrupted run stops advancing, its final line identifies the work in flight.
