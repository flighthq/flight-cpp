# A typed-array alternative must reach the SequenceView alternative of a contextual union

A typed array is assignable to the readonly typed-array alternatives of an options record, and the
flight-cpp backend represents both concrete typed arrays with one `flight::SequenceView<double>`
carrier. When the source expression is a union that includes a typed array and a sentinel, a
contextual union conversion still demands an exact destination alternative for `Uint32Array`, and
`SequenceView<double>` does not name one.

## Minimal TypeScript

```ts
interface Options {
  readonly indices?: Readonly<Uint16Array<ArrayBuffer>> | Readonly<Uint32Array<ArrayBuffer>> | null;
  readonly vertices: Float32Array<ArrayBuffer>;
}

declare function create(options: Readonly<Options>): void;

export function build(indices: number[] | null, vertices: Float32Array<ArrayBuffer>): void {
  const indexArray = indices !== null ? new Uint32Array(indices) : undefined;
  create({ indices: indexArray, vertices });
}
```

At Flight `a627849` / compiler `806d257`, this reports:

```text
contextual C++ union conversion requires equivalent source union evidence: target
dualSentinelVariant carriers [flight::SequenceView<double>] from identifier source
optionalSingle carriers [flight::Uint32Array]
```

## Measured site

`packages/scene3d-formats/src/awd2GeometryHandler.ts` in `parseTriangleGeometryBlock` builds the
index buffer exactly this way and passes it to `createMeshGeometry`. The runtime already supplies
the conversion the emitter needs: `SequenceView` has a constructor taking `TypedArray<SourceValue>`
(`include/flight/sequence_view.hpp`), and `TypedArray::from`/the range constructor already perform
JavaScript element conversion. The compiler reaches the same union conversion directly in
`mesh_geometry_operations.hpp`, where a `SequenceView<double>` local is initialized from the
`Uint16Array | Uint32Array` variant, so this is the contextual-union form of a conversion the
lowering already performs outside a union.

## Required lowering

When the source alternative is a concrete typed array and the destination alternative is
`SequenceView<double>`, select the sequence view and construct it through the typed array's implicit
conversion, preserving element order and the array's own storage (a view, not a copy). The sentinel
alternatives keep their existing Null/Undefined arms. The compiler must not re-spell the value as a
`std::variant<Uint16Array, Uint32Array>` alternative the destination does not name, copy the
elements, or erase the value into `Any`.

## Relationship to the other causes at this site

The same function also calls `Uint32Array.from`, which has no ambient binding
([typed-array static from](typed-array-static-from-binding.md)). A faithful source rewrite to
`new Uint32Array(indices)` removes that first refusal and exposes this one, which is why the
declaration repair currently carried for `awd2_geometry_handler.hpp` names both causes and supplies
no behavior.
