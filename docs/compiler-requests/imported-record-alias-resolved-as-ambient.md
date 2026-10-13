# An imported `Record` type alias is refused as an unbound ambient type

`@flighthq/types` declares `export type ParticleDesignerRawDict = Record<string, string | number | boolean>;`
and re-exports it from `contract.ts` with `export * from './ParticleDesignerSchema.ts'`. `types` itself emits it
correctly, as `using ParticleDesignerRawDict = flight::Record<flight::String, std::variant<bool, double,
flight::String>>;` in `flight/types/particle_designer_schema.hpp`. When `@flighthq/particles-formats` imports the
same name with `import type { ParticleDesignerRawDict } from '@flighthq/types/contract'`, every declaration that
mentions it is refused:

```
cpp emission failed for @flighthq/particles-formats/packages/particles-formats/src/particleDesignerParse.ts:
imported type ParticleDesignerRawDict names an ambient type this runtime profile does not bind, so the target
has no representation for it. Add the binding to the runtime contract, or declare the value with a type the
target provides.
```

Interfaces imported from the same file through the same `import type`, such as `ParticleDesignerDocument` and
`ParticleDesignerParseOptions`, resolve. The alias is not ambient: it is a declared, emitted alias of a
represented `Record`.

Measured in the emitted `@flighthq/particles-formats` at flight `a627849` and flight-compiler `2b687ea`, with this
repository's compiler patches applied (`generated/refusals.json`). A single-directory reduction cannot import
across packages, so the emitted ledger is the measurement.

## Expected behavior

An imported type alias resolves to its declaration, here the `types` alias, or equivalently its expansion
`flight::Record<flight::String, std::variant<bool, double, flight::String>>`. It is not treated as ambient.

## Affected modules

`@flighthq/particles-formats` `particleDesignerParse.ts`: `parsePlistRawDict`, `num`, `str`, `rawDictToConfig`,
`parseParticleDesignerPlist`, `collectParticleDesignerDiagnostics` and `parseParticleDesignerPlistDocument`. That is
the whole Particle Designer parser.

## Carrying workaround

The `particles-formats-pd-raw-dict-local-alias` source patch declares the identical alias locally,
`type ParticleDesignerRawDict = Record<string, string | number | boolean>`, in place of the import. The alias is
erased by TypeScript, and in C++ both spellings name the same `flight::Record` instantiation, so every value,
signature and lookup is unchanged.
