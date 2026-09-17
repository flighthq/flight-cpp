# Generated Flight SDK inventory

This directory is generated from `@flighthq/sdk` 0.5.0 at
`903f3289590358eacc20698525e6982da2ab0e9d` by `flight-compiler` at `fbfcc1102d2afb5fa5ea942fcdad27d520f21408`.
Do not edit it by hand.

No external binding profile is applied; this is the portable floor. Exact profile paths and SHA-256 digests are recorded in `manifest.json`.

The current compiler emitted 985 of 2900 source modules from
155 SDK packages and refused 1915. Emitted headers live under
`include/flight/<package>/`; every refusal and its owning module is recorded in `refusals.json`.

This is a bring-up inventory. It is intentionally committed before it forms a completely compilable SDK closure.
CMake exposes the full inventory as `Flight::SdkPreview`, and Bazel exposes `//:sdk_preview`; the preview name
keeps the remaining native compile failures visible. The package graph applies the public C++ `flight` namespaces
and installed include prefixes. `initialization.json` records the compiler's dependency and module-evaluation plan.

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
