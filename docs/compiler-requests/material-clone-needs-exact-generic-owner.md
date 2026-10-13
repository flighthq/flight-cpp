# Material clone and copy need the concrete generic owner

Flight's open material model clones and copies through its base `Material` interface while preserving
every own field of the concrete source:

```ts
export function cloneMaterial(source: Readonly<Material>): Material {
  const clone = allocateEntity<Material>();
  copyMaterialFields(clone, source, source.kind);
  return finishEntity(clone);
}
```

This is valid for JavaScript objects: the fresh result can receive every enumerable field discovered
on a Standard-PBR, classic, unlit, custom-shader, or vendor material. At source revision `a627849`,
the C++ profile instead allocates the fixed generated `Material` struct, whose declared native members
are only `kind` and `name`. `copyMaterialFields` enumerates a concrete Standard-PBR source and the
first derived-field write fails with:

```text
Flight named-property write rejected key or value
```

Owner-preserving dynamic writes are already emitted correctly; the missing evidence is which native
owner to allocate and retain across this generic operation. Side storage on the base owner would be
wrong because typed readers of the concrete material would not see it, and pointer reinterpretation
would manufacture a native object that was never allocated.

The carried source rewrite makes `cloneMaterial`, `copyMaterial`, and their private field-copy helper
generic in `T extends Material`, allocates `T`, and retains `Readonly<T>` for the source. TypeScript
erases those generic arguments, so JavaScript allocation count, field order, shallow texture/map
identity, alias safety, result identity, and return values are unchanged. In C++, each call now keeps
the concrete owner its input already proves.

This extends the same exact generic allocation remedy used by `createMaterial` and `createMaterial3D`.
It expires when interface-heritage references carry a concrete owner through base-typed allocation and
dynamic writes, or when Flight adopts the generic signatures upstream. Native materials tests pin a
distinct Standard-PBR clone, copied scalar values, authored name, equality, and copy alias safety.
