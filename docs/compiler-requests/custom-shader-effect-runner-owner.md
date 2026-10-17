# A refused binding and resolver are still referenced by the emitted registration call

## Reproduction

`flight/effects_gl/gl_custom_shader_effect.hpp` at `806d257`. The file refuses two declarations:

```
// NOT GENERATED: variable (binding) -- source line 78
// refusal: cpp-structural-assertion-writable-capability-unproven [source-portability]
// NOT GENERATED: function isGlCustomShaderEffectResolvable -- source line 85
```

and then emits the call that passes both of them:

```cpp
inline void register_gl_custom_shader_effect(flight::Ref<flight::types::GlRenderState> state) {
  flight::effects_gl::register_gl_effect(state, flight::String("CustomShaderEffect"),
      gl_custom_shader_effect_runner, is_gl_custom_shader_effect_resolvable);
}
```

Neither name exists, so four `effects_gl` headers fail with
`use of undeclared identifier 'gl_custom_shader_effect_runner'`.

## Cause

`glCustomShaderEffectRunner` is refused because its body asserts `effect as CustomShaderEffect` to pass a
readonly parameter, and the source's writable claim is not carried by the derived owner. The refusal text
states the required shape itself:

> Preserve `CustomShaderEffect` in the callback parameter through a named typed runner or carrier, or make an
> intentionally erased registry validate and recover that owner before invoking the typed runner.

`isGlCustomShaderEffectResolvable` is refused separately. Either way, a module that refuses a binding must
not emit a later call that references it: the call is what turns a declared refusal into an undeclared name in
every importer.

## Required shape

Emit the runner with the owner-preserving carrier the refusal asks for, and the resolver it pairs with, or
refuse `registerGlCustomShaderEffect` as well so no call references either. Emitting the call without the
declarations is the one answer that is neither.

## What this repository carries meanwhile

Nothing, deliberately. A runtime definition would have to fake the effect runner or pass the input through
unchanged, which is exactly the behaviour the refusal says must not be invented. A declaration-only repair
was considered -- the symbols have no definition anywhere, so the call would be a genuine negative link --
but the repair mechanism declares *functions*; these are a variable binding and a second function, and adding
a variable-declaration kind is new apparatus rather than a cheap fix. The cluster stays open on this request.

## Affected

4 headers in the G1 sweep, all `effects_gl` (`gl_custom_shader_effect` and its importers).
