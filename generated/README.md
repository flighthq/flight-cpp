# Generated Flight SDK inventory

This directory is generated from `@flighthq/sdk` 0.5.0 at
`1274ec5c923947dc64d5ffedcbd8169fc758cd9f` by `flight-compiler` at `3292a16d81d34e29a2906b27950a491cc0ac358e`.
Do not edit it by hand.

The current compiler emitted 314 of 2851 source modules from
154 SDK packages and refused 2537. Emitted headers live under
`include/flight/<package>/`; every refusal and its owning module is recorded in `refusals.json`.

This is a bring-up inventory. It is intentionally committed before it forms a compilable SDK closure, and CMake and
Bazel do not publish it as `Flight::Sdk` yet. The package graph applies the public C++ `flight` namespaces and
installed include prefixes. `initialization.json` records the compiler's dependency and module-evaluation plan.

Regenerate and verify the tree from the repository root:

```sh
npm run rehydrate
npm ci --prefix .dependencies/flight-compiler
npm run sdk:generate
npm run sdk:check
```
