# Generated Flight SDK inventory

This directory is generated from `@flighthq/sdk` during a pin transition, so its packages come from
different Flight revisions, each recorded on its row in `manifest.json`:

- `7e2fc7df5378313ceaf21ba8d6ef39cc18a6f4ad` (65 packages)
- `a62784923f3be814463286c9fb28edfe6ab15789` (91 packages)

Every package was emitted by `flight-compiler` at `2b687eaf6b631e0fa4d7d6e18dad93b845f99633`.
Do not edit it by hand.

Binding profiles differ between packages during a profile transition, so each package row names the profile
set it was generated under:

- `sha256:070fadeb2a079809e8f46c9c1e64d8daf796b8fe8682247181b0d7d502005bdb` (45 packages)
- `sha256:0a2183634245fa5f44e24c7d7c885f934624a2130d5fb4f6649fc9a1932ba335` (111 packages)

Exact profile paths and SHA-256 digests are recorded per set in `manifest.json`.

The current compiler emitted 2453 of 3033 source modules from
156 SDK packages and refused 580. Emitted headers live under
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
