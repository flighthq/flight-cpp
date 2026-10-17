# A readonly structural view is cast to a writable one

## Reproduction

`flight/scene3d_gl/gl_blinn_phong_mesh_material_renderer.hpp` at `806d257`, line 126:

```cpp
flight::scene3d_gl::draw_gl_mesh_subset(state,
  flight::structural_ref_cast<flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::GlMeshProgram>>>>(
    flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<flight::types::GlMeshProgram>>>>(program.value())),
  ...);
```

The inner conversion builds a `StructuralRef<RowWritable<...>>` from a value that is then handed to a
`StructuralRef<RowReadonly<...>>` parameter through a second cast:

```
error: no matching conversion for functional-style cast from
       'StructuralRef<RowReadonly<RowOf<shared_ptr<GlMeshProgram>>>>' to
       'StructuralRef<RowWritable<RowOf<shared_ptr<GlMeshProgram>>>>'
```

## Required shape

A readonly view cannot be manufactured into a writable one; the writable carrier has to come from the
declared owner, or the parameter has to receive the readonly view it actually declares. The emitter already
knows which one the callee takes.

## Affected

One swept header at G1 (`scene3d_gl`'s `_internal_index.hpp`, which includes the file above). Filed because
it is a distinct emitter defect, not because of its size.

## What this repository carries meanwhile

Nothing: the cluster is open.
