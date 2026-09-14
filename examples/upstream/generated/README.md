# Generated upstream Flight examples

This directory is generated from every package under Flight `examples/packages` at
`1274ec5c923947dc64d5ffedcbd8169fc758cd9f` by `flight-compiler` at `56496423fd865a51023e72966190d10d886eae40`. Do not edit it by hand.

The SDL/GL native profile emitted 0 of 181 modules from
33 example packages. Every dependency-closed refusal is retained in `refusals.json`.
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
