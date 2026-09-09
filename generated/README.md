# Generated Flight SDK inventory

This directory is generated from `@flighthq/sdk` 0.5.0 at
`1274ec5c923947dc64d5ffedcbd8169fc758cd9f` by `flight-compiler` at `4b44279c9a47786f003f2360841c4a19da7a9499`.
Do not edit it by hand.

The current compiler emitted 1322 of 2851 source modules from
154 SDK packages and refused 1529. Emitted headers live under
`include/flight/<package>/`; every refusal and its owning module is recorded in `refusals.json`.

This is a bring-up inventory. It is intentionally committed before it forms a compilable SDK closure, and CMake and
Bazel do not publish it as `Flight::Sdk` yet. Current generated namespaces still reflect the npm publisher scope.
The manifest records the pending remap to the public C++ `flight` namespace.

Regenerate and verify the tree from the repository root:

```sh
npm run rehydrate
npm ci --prefix .dependencies/flight-compiler
npm run sdk:generate
npm run sdk:check
```
