# An indexed-access type naming another package's anonymous member mints a second struct

`LibgdxParticleDocument['spawnShape']` names the anonymous member type that `@flighthq/types` declares for
`spawnShape`. When a local in another package is annotated with that indexed-access type, the emitter
declares a new struct in the consuming package, `shape_edges_side_b64ec89da7389999`. The member it names is
`flight::types::shape_edges_side_ad397097fb67afdb`. The two C++ bodies are identical, but the structural
hashes differ, so the alias repair, which requires one name, cannot join them. Assigning the local into the
document then does not compile. Within a single module the same indexed-access local resolves to the member's
own struct and compiles, so the defect is the cross-package resolution of the indexed-access type.

Measured in the emitted `@flighthq/particles-formats` at flight `a627849` and flight-compiler `2b687ea`, with
this repository's compiler patches applied. The single-module reduction below emits and compiles; it is the
control.

## Source (flight `a627849`)

```ts
// @flighthq/types LibgdxParticleSchema.ts
export interface LibgdxParticleDocument {
  // ...
  spawnShape: {
    shape: 'point' | 'line' | 'square' | 'ellipse';
    edges: boolean;
    side: 'both' | 'top' | 'bottom';
  };
}

// @flighthq/particles-formats libgdxSerialize.ts
let spawnShape: LibgdxParticleDocument['spawnShape'] = { shape: 'point', edges: false, side: 'both' };
// ...
return { /* ... */ spawnShape, /* ... */ };
```

## Observed

```cpp
// flight/particles_formats/libgdx_serialize.hpp
struct shape_edges_side_b64ec89da7389999 : public flight::ReferenceEnabled {
  flight::String shape;
  bool edges;
  flight::String side;
};
// flight/types/libgdx_particle_schema.hpp
struct shape_edges_side_ad397097fb67afdb : public flight::ReferenceEnabled { /* same three members */ };
```

```text
libgdx_serialize.hpp:229: error: no viable conversion from 'Ref<shape_edges_side_b64ec89da7389999>' to 'Ref<shape_edges_side_ad397097fb67afdb>'
```

## Single-module control (measured: emits and compiles)

```ts
export interface Doc {
  name: string;
  shape: { kind: 'point' | 'line'; edges: boolean };
}

export function makeDoc(line: boolean): Doc {
  let shape: Doc['shape'] = { kind: 'point', edges: false };
  if (line) shape = { kind: 'line', edges: true };
  return { name: 'd', shape };
}
```

## Expected behavior

`T['member']` resolves to the member's declared carrier, here `flight::types::shape_edges_side_ad397097fb67afdb`,
whichever package spells the indexed access. No new struct is minted for it.

## Affected modules

`@flighthq/particles-formats` `libgdxSerialize.ts`, in `configToDocument`'s spawn shape.

## Carrying workaround

The `particles-formats-libgdx-spawn-shape-kind` source patch keeps only the shape's kind in a local, typed
`LibgdxParticleDocument['spawnShape']['shape']`, and writes the object once, in the document literal, where its
contextual type is the member itself. `edges` and `side` are `false` and `'both'` in all three of the source's
assignments, and the object was referenced only by the document, so the document holds the same values in one
fresh object.
