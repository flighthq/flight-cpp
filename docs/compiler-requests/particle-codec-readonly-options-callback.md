# Particle codec readonly options callback

Pinned Flight `a62784923f3be814463286c9fb28edfe6ab15789` and compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`.

The complete original Unity parser now compiles and runs. All three particle barrel headers
next fail at `register_built_in_particle_formats.hpp:171`, where the original
`starlingPexParticleFormat.codec.parseToConfig` stores `parseStarlingPex` directly.

The source is structurally valid:

```ts
interface ParseParticleConfigOptions extends ParticleDesignerParseOptions,
  UnityParseOptions, LibgdxParseOptions, StarlingPexParseOptions {}

// ParticleFormatCodec
parseToConfig(text: string, options?: Readonly<ParseParticleConfigOptions>): ParticleEmitterConfig;

// Original exported Starling entrypoint
function parseStarlingPex(xml: string, options?: StarlingPexParseOptions): ParticleEmitterConfig;
```

The generated slot takes
`optional<StructuralRef<RowReadonly<RowOf<Ref<ParseParticleConfigOptions>>>>>`,
but the retained public function takes `optional<Ref<StarlingPexParseOptions>>`.
The function pointer cannot construct that `std::function` slot. The document callback has
the same source boundary. Both original Starling entrypoints only read
`options?.textureSize ?? 1`; neither stores, mutates, compares or returns the options owner.

An arbitrary structural options literal is a source-valid codec argument. It need not own a
native `StarlingPexParseOptions` allocation. Recovering only native Starling allocations would
narrow this domain; constructing a Starling owner would copy fields and change identity and
live reads. Structural similarity alone cannot justify either operation or generic callable
variance. The established canonical callable-declaration fix does not adapt distinct parameter
owners, and GL shared-cell declarations do not supply this options bridge.

Required proof includes absent and present options, absent versus present fields, a live field
updated through another view, the same source owner and callback identity, single argument
evaluation, exact source defaults and errors, native public `Ref` compatibility, and exclusion of
mutating, escaping, identity-observing and opaque callees. A private body specialization or
another existing read-view mechanism must earn those properties. A new public parameter or
callback carrier needs a bounded architecture decision with its exact type and site changes.
No adapter, copied options owner, unchecked cast or public ABI change is supplied by the Unity unit.

The measured Unity unit is one passing header and three failed barrels, all four attempted.
The particle package remains partial at 17/18 emitted modules. Later retained refusals in
registration and the nullable JSON Record helper are separate boundaries; this first error
does not establish that clearing it will qualify the barrels or registry behavior.
