# A texture-cache local must retain the effective key carrier and shared map

At Flight `a627849` / compiler `806d257`, generated `scene2d_gl/gl_scale9_shape.hpp:119` copies a
context cache into a local whose key type differs from the effective native slot. Using `Entry` for
the unchanged reference to `texture_version_2f2f82ef5fff624b`, the two carriers are:

```cpp
// overrides/include/flight/types/gl_context_runtime.hpp
flight::WeakMap<flight::types::TextureSourceValue, Entry> texture_source_premultiplied_texture_cache;

// generated destroy_gl_scale9_shape_data
flight::WeakMap<flight::Ref<flight::types::TextureSource>, Entry> cache =
    get_gl_render_state_runtime(state)->context->texture_source_premultiplied_texture_cache;
```

The native representation deliberately retains seven source-owner arms: VoxelGrid, the open
TextureSource base, ExternalTexture, Bitmap, CompressedImageResource, RenderTarget and ImageResource.
The generated local instead reconstructs the pinned TypeScript slot's nominal TextureSource key.
This provenance matters: the variant comes from the approved native context override, not from a
claim that the unmodified TypeScript declares a variant-key map.

An exact declaration-only candidate for the independently refused private data getter makes the
actual unused-header compile reach this assignment. Clang22.1.8 rejects it at candidate121 (stock119):
`no viable conversion from WeakMap<TextureSourceValue, ...> to WeakMap<Ref<TextureSource>, ...>`.
No link is reached. The production original1 still fails at missing getter114,0/1 pass.

Reconcile the represented slot and its consumer so the local retains the same map storage, exact
source-owner keys, entry references and weak lifetimes. Deriving this local from the effective field
would keep its carrier rather than manufacture a different base-key map. Native WeakMap copies at
the same carrier share their entries; rebuilding a map, projecting each key to a copied TextureSource,
narrowing the shared cache to ImageResource, or allowing unchecked cross-key constructors would not
preserve that contract. Use the original ImageResource key to find and delete its GPU texture, erase that
entry, then release the host surface, in source order.

This differs from the [checked erased WeakMap view](compiler-erased-ref-weak-map-view.md): the source
here has a known variant of concrete reference keys and a typed value, not an object/object erased
map. No new view, conversion, local override, binding or runtime behavior is supplied by this request.
Fixing the local would not implement the [private data recovery](flattened-interface-refinement-identity.md#g2-gl-scale9-shape-private-raster-data)
or the separately refused draw/provider pipeline. No positive unused-header link/run, qualified
negative link, generation or actual GL raster/teardown result is claimed.
