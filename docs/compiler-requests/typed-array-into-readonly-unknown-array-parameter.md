# A typed array passed to a `readonly unknown[]` parameter needs a readonly element view

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, a `number[]` local is passed where the parameter is declared
`readonly unknown[]`. TypeScript accepts it, because readonly arrays are covariant. The emitter lowers the
argument as `flight::Array<double>` and the parameter as `flight::Array<flight::Any>`, which are unrelated
carriers, so the call has no match.

## Source

```ts
// packages/scene3d-gl/src/glScene3DTestHelper.ts
setError(code: number, call: string, args: readonly unknown[]): void { ... `${call}(${args.join(', ')})` ... }

const args = instanceCount === undefined ? [mode, first, count] : [mode, first, count, instanceCount];
errorState.setError(GL_INVALID_ENUM, call, args);
```

## Emitted mismatch

```
flight/scene3d_gl/_internal_gl_scene_3d_test_helper.hpp:373: error: no match for call to
'(std::function<void(double, flight::String, flight::Array<flight::Any>)>) (const double&, flight::String&, flight::Array<double>&)'
```

GCC 15.2 gives the same error at each `setError` call of the fake WebGL validation helpers (lines 377, 386,
and so on).

## Expected lowering

A `readonly unknown[]` parameter only reads its elements. The lowering should receive any typed
`flight::Array<T>` through a readonly element view that boxes each element as `flight::Any` when it is read,
and keeps the caller's array identity. The other option is a parameter carrier generic over the element
type.

Materializing a new `Array<Any>` at the call happens to be harmless in this helper, because `setError` only
joins the elements into a message. But it is not a general lowering: a callee that retains the array, or
compares it by identity, would see a different object. So the emitter must not pick that silently.

## Why nothing is carried

The site is a test fixture, the fake GL error audit, and its behavior is correct TypeScript. Neither the
runtime nor the fixture is changed to fit the emitter. A repair cannot add the conversion. A source patch that
annotates `args` as `unknown[]` would avoid the boundary only here, and only by asserting that a copy is
acceptable.

## Affected

Cluster `call-has-no-matching-fee2f93f`: `scene3d_gl/_internal_gl_scene_3d_test_helper.hpp`.
