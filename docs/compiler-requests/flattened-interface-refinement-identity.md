# Identity-preserving flattened interface refinements

TypeScript interface extension and intersection relationships are emitted as independent C++ owner
structs. An assertion that refines a base interface to a derived interface therefore has no C++ heritage
path, even when the same object satisfies the refined fields.

## Minimal TypeScript

```ts
interface Source { kind: string }
interface RenderTarget extends Source { kind: 'render-target'; width: number }
interface Texture2D { readonly dimension: '2d'; source: Source | null }
interface RenderTexture extends Texture2D { source: RenderTarget }

declare function createTexture(source: Source): Texture2D;
export function createRenderTexture(target: RenderTarget): RenderTexture {
  return createTexture(target) as RenderTexture;
}
```

## Expected C++

The emitted ownership model must make the refinement a checked view of the existing object, or preserve
real interface heritage. Conceptually:

```cpp
auto texture = create_texture(target);
return flight::structural_ref_cast<RenderTextureView>(texture);
```

The returned value must share the exact reference identity and lifetime of `texture`. A native pointer
cast between unrelated structs, a copied `RenderTexture`, or an auxiliary identity table is not an
equivalent lowering.

## Affected modules

`createRenderTexture` in `@flighthq/texture` is refused at the `Texture2D` → `RenderTexture` assertion.
The same representation boundary also omits the `Texture` → `CubeTexture` and `TextureCube` →
`CubeTexture` assertions in the package's cube-texture placeholder.

## Carrying workaround

There is no downstream workaround: materializing the refined struct would replace the source object and
break reference equality. The generated placeholders deliberately retain the refusal until the compiler
and runtime agree on an owner-preserving interface-refinement carrier.
