# Generated Flight SDK inventory

This directory is generated from `@flighthq/sdk` 0.5.0 at
`1274ec5c923947dc64d5ffedcbd8169fc758cd9f` by `flight-compiler` at `a6895ee00994bce116c9fdff6c3503462437eee8`.
Do not edit it by hand.

No external binding profile is applied; this is the portable floor. Exact profile paths and SHA-256 digests are recorded in `manifest.json`.

The current compiler emitted 947 of 2851 source modules from
154 SDK packages and refused 1904. Emitted headers live under
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
