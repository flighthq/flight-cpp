# `typeof` on a member read through an assertion is folded to a constant

`typeof (value as T).member === 'function'` with `value: unknown` asks the real object at run time. The assertion
`as T` validates nothing, so the member may be missing, or may hold any type. The emitter instead answers from the
asserted declaration and folds the comparison to a constant.

Measured at flight-compiler `806d257` with the current patch stack (`render/renderCache.ts`, `isRenderCacheAdapter`):

```ts
typeof value === 'object' && value !== null &&
  typeof (value as RenderCacheAdapter).adapt === 'function' &&
  'cache' in (value as RenderCacheAdapter)
```

was emitted (before the membership refusal took the function) as:

```cpp
(value.type_of() == flight::String("object")) && !value.is_null() &&
  (flight::String("function") == flight::String("function")) && ...
```

The third conjunct is always true, so an object without a callable `adapt` would pass the guard.

## Requested lowering

A `typeof` of a member read through an assertion of an erased value must read the member from the erased value
itself: the named view of the object's own row owner, then `type_of()` on the `Any` it returns. A missing member reads
as `undefined`, and the result is `"undefined"`. It must not fold from the asserted type. Until that lowering exists,
the emitter should refuse the `typeof`, rather than emit a constant.

## Interaction with membership

The same function's `'cache' in (value as RenderCacheAdapter)` is refused by `compiler-membership-test-presence` (an
asserted receiver). Both conjuncts need the erased value's own owner, and the `in` also needs the prototype-chain model
described in [membership-test-lowered-literally](membership-test-lowered-literally.md). Fixing either one alone would
not make `isRenderCacheAdapter` faithful.
