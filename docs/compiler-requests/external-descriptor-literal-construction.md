# Object literals passed to bound WebGPU methods need the binding's field contract

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, an object literal passed to a method of a bound WebGPU
object is emitted as a bare braced initializer: no type, keys in TypeScript source order, and
anonymous structural owners for nested literals. This happens even where `bindings/sdl-wgpu.json`
binds the descriptor type and declares its `objectConstruction` field contract. The host methods are
constrained variadic templates (`include/flight/host_sdl/wgpu.hpp`), and a braced list cannot deduce
a template argument, so every such call fails with no matching function. This is the
`call-has-no-matching` cluster led by `begin_render_pass`.

## Source

```ts
const pass = encoder.beginRenderPass({
  colorAttachments: [{ view: presentationView, loadOp: 'clear', storeOp: 'store', clearValue: { r: 0, g: 0, b: 0, a: 0 } }],
});
```

(`packages/render-wgpu/src/wgpuScreenAntialias.ts`)

## Emitted mismatch

```cpp
auto pass = encoder.begin_render_pass({.color_attachments = flight::Array{
    flight::make_ref<view_load_op_store_op_clear_value_5f69e0cae64406d6>(...)}});
```

```
error: no matching function for call to 'WgpuObject<WgpuCommandEncoderTag>::begin_render_pass(<brace-enclosed initializer list>)'
error: no matching function for call to 'WgpuObject<WgpuDeviceTag>::create_pipeline_layout(<brace-enclosed initializer list>)'
```

The same shape reaches every descriptor-taking method. Braced-literal call sites in `generated/include`
at this pin: `create_bind_group_layout` 20, `create_shader_module` 18, `create_buffer` 13,
`create_bind_group` 10, `create_pipeline_layout` 10, `create_render_pipeline` 9, `create_texture` 9,
`begin_render_pass` 5, `create_view` 5, `write_texture` 4, `create_sampler` 3.

## Why the runtime cannot absorb it

`create_texture` already has a typed `WgpuTextureDescriptor` overload, and it works only where the
literal's key order happens to match the struct. Three properties of the emission rule out the same
fix for the rest:

- **Key order is source order.** C++ designated initializers must follow declaration order. The
  sampler site emits `.min_filter, .mag_filter, .address_mode_u, .address_mode_v`, while
  `WgpuSamplerDescriptor` declares the address modes first. No single declaration order matches every
  call site.
- **Nested literals are anonymous owners.** Attachment, vertex-state, fragment-state and
  layout-entry literals become `make_ref<anon_hash>`, even though `GPUVertexState`, `GPUFragmentState`
  and `GPUBindGroupLayoutEntry` are bound with field contracts. A typed descriptor would have to accept
  every anonymous shape through a templated conversion, which no longer type-checks the contract.
- **Unbound descriptors have no target at all.** `GPURenderPassDescriptor`,
  `GPURenderPassColorAttachment`, `GPURenderPassDepthStencilAttachment`, `GPUPipelineLayoutDescriptor`,
  `GPUShaderModuleDescriptor` and `GPUTextureViewDescriptor` are not in the profile. Those can be added
  in our lane once the emitter uses a declared contract, but adding them does not change the emission
  above.

## Expected lowering

When the parameter's contextual type resolves to a bound external type that declares
`objectConstruction: field-assignment`, construct that target type from the literal through the
declared fields, independent of source key order. Do the same recursively for nested literals whose
contextual type is bound. This is equivalent to:

```cpp
flight::host_sdl::WgpuSamplerDescriptor descriptor{};
descriptor.min_filter = flight::String("linear");
descriptor.mag_filter = flight::String("linear");
descriptor.address_mode_u = flight::String("clamp-to-edge");
descriptor.address_mode_v = flight::String("clamp-to-edge");
device.create_sampler(descriptor);
```

Where the contextual type is unbound, refuse the call rather than emit an untyped initializer, so the
profile gap is reported as a missing binding instead of a C++ overload failure.

Runtime follow-up once this lands: typed descriptor overloads beside each variadic method, matching
`create_texture`, and bindings for the six unbound descriptors above. They remain `unavailable`
bodies; this request is about construction, not a backend.

## G2 IBL BRDF attachment: a typed local does not restore the bound member domain

At `806d257` / Flight `a627849`, `scene3d_wgpu/wgpu_environment_ibl_bake.hpp` fails before the render
pass call: the shared anonymous color-attachment struct types its `view` member as `flight::Any`, so
`texture.createView()` cannot enter it.

```text
no viable conversion from 'WgpuObject<WgpuTextureViewTag>' to 'flight::Any'
```

`GPUTextureView` is bound in `bindings/sdl-wgpu.json` (type space, shared, target
`flight::host_sdl::WgpuTextureView`), and the positive control is `render_wgpu/wgpu_render_pass.hpp`:
the identical `view_clear_value_load_op_store_op` shape there emits `WgpuTextureView view;` because the
value is a `GPUTextureView` **parameter**. Two faithful source rewrites were measured and did not
change the emission: calling `texture.createView()` inline in the member, and hoisting it into
`const view: GPUTextureView = texture.createView()` before the literal. Both still produced
`flight::Any view;`, so the member domain follows the call result rather than the local's declared
external type. The regenerated headers were compared and both rewrites were discarded.

Required: when typing a contextual attachment member, resolve the source expression's declared or
inferred external type — the annotated local, and the bound return domain of a method call — exactly
as the emitter already does for a parameter. The sibling header is the parameter-form control. This
does not need a new profile entry; the binding exists. No bake, provider, copied owner or narrowed
domain is supplied by this repository pending the fix.

## G3 root cause and carried fix

The root was located at flight-compiler `806d257` with no patch applied, and in this repository's profile
before the change. It has two halves, and neither is an override or emission repair of ours.

**1. The profile declared no typed WebGPU members (our lane).** The pinned backend already constructs a
bound value type from a literal argument, field by field and independent of key order:
- the call argument's expected type comes from `getCppExternalInstanceCallParameterExpectedTypeCpp`
  (`cppCompilerBackend.ts:21929`, consulted first at `:23079`);
- the object case hands that type to `emitCppExternalObjectConstructionCpp` (`:5635`, defined at `:21962`);
- the parameter type itself is read by `getCompilerRuntimeExternalInstanceMemberParameterTypeCpp`
  (`cppRuntimeExternalSymbolBinding.ts:245`) from a member's `parameters` entry;
- upstream's own fixture "constructs an erased external object parameter by source-ordered field
  assignment" (`cppCompilerBackend.test.ts:21509`) shows the mechanism working.

`bindings/sdl-wgpu.json` declared `members` only on the five value-space flag namespaces, with zero
`parameters`. So `createSampler`, `createBuffer`, `beginRenderPass` and their siblings had no contextual
type, and fell to the untyped source-order braced initializer above. A member the profile leaves undeclared
keeps that fallback.

**2. Nested field values had no type (pinned backend).** Inside a bound construction, every field value is
emitted with no expected type (`cppCompilerBackend.ts:22009`:
`${target}.${field.targetName} = ${emitExpression(property.value, context)}`). The field contract
(`CppCompilerExternalFieldBinding`, `compilerBackendContract.ts:160`) is only `sourceField` to `targetName`.
So a nested literal (attachments, vertex, fragment, primitive and depth-stencil state, bind-group and layout
entries, buffers, attributes) still became an anonymous `make_ref` owner. The IBL BRDF `view: Any` member
above is the same gap.

**Carried fix (three parts, one commit):**
- **Compiler:** `source-patches/compiler-external-object-field-literal-type.patch`
  (`compiler-external-object-field-literal-type`) adds optional `literalType` and `sequence` to the field
  contract.
  - It validates them: a malformed field, a sequence without a type, or an unbound literal type is a
    manifest error.
  - It gives a field value that bound type, or an array of it, only when the value is itself an object or
    array literal.
  - Fields stay in source order and an omitted field is never assigned. A non-literal value keeps exactly
    its old emission, so no array is rebuilt and no handle is recast.
  - Six focused fixtures accompany it.
- **Profile:** `bindings/sdl-wgpu.json` changes in three ways:
  - Typed member parameters for `GPUDevice` (`createBindGroup`, `createBindGroupLayout`, `createBuffer`,
    `createPipelineLayout`, `createRenderPipeline`, `createSampler`, `createShaderModule`,
    `createTexture`), `GPUCommandEncoder.beginRenderPass`, `GPUTexture.createView`, and
    `GPUQueue.writeTexture` (positions 0 and 2).
  - Literal types on the nested descriptor fields. A union slot names only its single dictionary arm:
    `resource` names `GPUBufferBinding`, `size` names `GPUExtent3DDict`, and `origin` names
    `GPUOrigin3DDict` or `GPUOrigin2DDict`.
  - Bindings for the six descriptors that were missing: `GPURenderPassDescriptor`, the color and
    depth-stencil attachments, `GPUPipelineLayoutDescriptor`, `GPUShaderModuleDescriptor` and
    `GPUTextureViewDescriptor`.
- **Runtime:** `flight/host_sdl/wgpu.hpp` gains the render-pass, color- and depth-stencil-attachment,
  shader-module and texture-view descriptor values. `flight/iterable.hpp` gains a live `Array<T>` to
  `Iterable<std::optional<T>>` view, which only adds absence; it is used for `targets`, `buffers` and
  `colorAttachments`.

Every method remains the existing `unavailable` body. A compiling call proves boundary availability, not a
successful GPU operation.
