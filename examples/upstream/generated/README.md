# Generated upstream Flight examples

This directory is generated from every package under Flight `examples/packages` at
`7e2fc7df5378313ceaf21ba8d6ef39cc18a6f4ad` by `flight-compiler` at `ef60fb6d3181ce5c12d5ce8d75c8ef6518c9b168`. Do not edit it by hand.

The SDL/GL native profile selected 103 of 184
upstream modules across 34 example packages and emitted 0.
The selection mirrors Flight's `RENDER=webgl` alias by remapping each `render.ts` selector to its WebGL source;
the DOM-only cross-backend-embed example records its fallback explicitly. Every dependency-closed refusal is retained
in `refusals.json`.
`frontier-refusals.json` compiles each example without its package dependencies to expose the next direct source,
compiler, or host boundary hidden by dependency propagation. `initialization.json` retains any module-evaluation
plans available for emitted examples. Generated headers, as they become available,
live under `include/flight/examples/` and include the one shared SDK tree at the repository's top-level
`generated/include/`; this directory never contains another SDK copy.

Regenerate or verify this inventory from the repository root:

```sh
npm run rehydrate
npm ci --prefix .dependencies/flight-compiler
npm run examples:generate
npm run examples:generate:check
```
