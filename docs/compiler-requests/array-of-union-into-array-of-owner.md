# An array of the union value is passed where an array of the concrete owner is declared

## Reproduction

`flight/scene3d_gl/enable_gl_pbr_extension_guards.hpp` at `806d257`, line 95:

```cpp
flight::Array<flight::Ref<flight::types::GlPbrExtensionIssue>> issues =
    flight::scene3d_gl::explain_gl_pbr_extensions(state, extensions);
```

`extensions` is `flight::Array<flight::types::PbrExtensionValue>` -- the union, one variant per element --
while `explain_gl_pbr_extensions` declares `flight::Array<flight::Ref<flight::types::PbrExtension>>`:

```
error: no viable conversion from 'Array<flight::types::PbrExtensionValue>' to
       'Array<flight::Ref<flight::types::PbrExtension>>'
```

An element-wise conversion between two array carriers does not exist and should not: it would copy.

## Required shape

Either the array's declared element is the union everywhere it travels, or the call site unwraps each
element into the concrete owner it names before the call. The emitter already knows both types.

## Affected

One swept header at G1 (`scene3d_gl/enable_gl_pbr_extension_guards.hpp`). Filed as its own cause.

## What this repository carries meanwhile

Nothing: the cluster is open.
