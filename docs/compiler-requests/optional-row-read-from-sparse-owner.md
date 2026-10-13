# Optional row reads must accept absent sparse-owner storage

An object literal supplied as `Readonly<Partial<T>>` is represented by a sparse `RowOwner`: only
authored properties have storage. When that same owner is passed to a function accepting an
interface whose properties are themselves optional, an omitted property is still ordinary
TypeScript `undefined` and must read as an empty optional.

At Flight revision `a627849`, materials exposes the failure without an assertion or cast:

```ts
function createMaterial3D(kind: Kind, opts?: Readonly<Material3DOptions>) {
  material.alphaCutoff = opts?.alphaCutoff ?? DEFAULT_ALPHA_CUTOFF;
}

function createExtendedPbrMaterial(opts?: Readonly<Partial<ExtendedPbrMaterial>>) {
  return createMaterial3D(ExtendedPbrMaterialKind, opts);
}
```

`Material3DOptions.alphaCutoff` is optional, and `Partial<ExtendedPbrMaterial>` is assignable to
that option shape. Native lowering correctly keeps one sparse owner, but converts the view to
`RowOf<Material3DOptions>`. `row_get` then treats the complete absence of an
`std::optional<double>` cell as a required-property failure and throws:

```text
std::out_of_range: required structural row property is absent
```

The runtime repair returns an immutable empty optional when an ordinary readonly (neither
`Required` nor `Partial`) row's declared value type is optional and its sparse owner has no cell.
Required projections, writable rows, and non-optional members retain their existing failure. The structural-row test
constructs a sparse partial owner, widens the same owner to its ordinary optional-property row, and
asserts both empty value and preserved identity. The materials native test then exercises the real
partial Extended-PBR, Standard-PBR, shader, and extension option paths.

The request expires if lowering retains enough optional-property information to make the same
absence decision at the access site instead. It must not allocate or populate a second owner: doing
so would lose authored-property presence, object identity, and preset override ordering.
