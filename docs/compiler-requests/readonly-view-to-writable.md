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

## G2 Gaussian blur RenderTexture source target

At Flight `a627849` / compiler `806d257`, gl_blur_effect.hpp112 is the same defect as the mesh
case above. Source applyGaussianBlurToGlRenderTextures reads sourceTarget=getGlRenderTextureTarget
and passes it to applyGaussianBlurToGl's Readonly<GlTextureRenderTarget> source. The source getter
explicitly returns Readonly<GlRenderTextureEntry['target']>|null; both its generated signature and the
native override return optional<readonly GlTextureRenderTarget row>. There is no writable authority
at this call and no disagreement caused by the override's public carrier.

A scratch header adds only the exact refused Gaussian helper prototype and refused runner-slot
extern through existing guarded declaration mechanisms. Actual unused-header native compilation
(Clang22.1.8, one worker) fails at candidate116/original112:

```text
no matching conversion for functional-style cast from
const StructuralRef<RowReadonly<RowOf<shared_ptr<GlTextureRenderTarget>>>>
to StructuralRef<RowWritable<RowOf<Ref<GlTextureRenderTarget>>>>
```

The runtime constructor's row_convertible_to constraint rejects readonly-to-writable correctly.
The emitter wraps source_target.value() in RowWritable only to cast it back to the identical
RowReadonly schema expected by the callee. Pass the already-proven readonly view directly; do not
loosen runtime readonly checks, change the source getter's contract, recover mutable authority, copy
the target or add side storage. Destination/scratch callbacks legitimately receive mutable target
owners; that does not grant the independently resolved source target the same authority.

Stock original1 remains0/1 at the earlier omitted helper93 (75.1s), while the changed candidate fails
before any link gate and is rejected. No declaration/body/runtime/source/binding/override or normal
generation is carried. The [qualification record](retained-declaration-calls-omitted-callee.md#g2-gaussian-blur-declaration-candidate-fails-the-unused-header)
and [full Gaussian/RenderTexture contract](interface-heritage-widening.md#g2-plain-gaussian-blur-preserves-two-compositing-passes)
separate actual source-target authority from program-cache and refused-runner debt. No GL execution.
