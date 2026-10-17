# A bare `return;` inside `try … finally` in a void function emits `std::optional<void>`

The emitter lowers an early return inside a `try` that has a `finally` as a stored return slot, so that the
`finally` block runs before the function returns. In a function returning `void`, the slot is declared
`std::optional<void>`, which C++ does not allow. The module emits, and its header does not compile.

Measured at flight-compiler `2b687ea` with this repository's 44 compiler patches applied. Both modules emit. The
reduction's header fails in `<optional>`; the control, which puts the remaining work inside an `if` instead of
returning early, compiles.

## Minimal TypeScript (measured)

```ts
export function fill(out: number[], count: number): void {
  const scratch = acquire();
  try {
    if (count <= 0) return;
    for (let i = 0; i < count; i++) scratch.push(i);
    for (const value of scratch) out.push(value);
  } finally {
    release(scratch);
  }
}
```

```text
<optional>:378: static assertion failed due to requirement 'is_object_v<void>':
  instantiation of optional with a non-object type is undefined behavior
```

Control (measured: compiles): `if (count > 0) { …the two loops… }` inside the same `try`.

## Expected behavior

For a void function the return slot needs only a flag: `bool finally_returned`, or the `finally` body run on
every exit path, as the emitter already does for a value-returning function.

## Affected modules

- `@flighthq/physics2d` `worldQueries.ts` `queryPhysics2DShapeCast` (line 274), carried by
  `physics2d-shape-cast-guarded-body`.
- The emitted `@flighthq/physics3d` `world_queries.hpp` declares the same `std::optional<void>` at a627849 (line
  446). That package is not changed here.

## G2 generic GL render-texture bracket with void result

At Flight `a627849` / compiler `806d257`, the emitted with_gl_render_textures<T> in
render_gl/gl_render_texture_pool.hpp134 declares std::optional<T> finally_return. After the
[qualified exact acquisition declaration](retained-declaration-calls-omitted-callee.md#g2-gl-render-texture-pool-acquisition-declaration-qualified),
the actual unused header and a real double-result bracket object compile, but a separate actual
with_gl_render_textures<void> instantiation fails (Clang22.1.8, jobs1, exit1). Its first diagnostic
is libcxx optional378: is_object_v<void> fails; instantiation of optional with a non-object type is
undefined behavior. The template trace points to the retained finally return slot and real caller.
This extends the existing void/finally lowering problem to a generic result whose source callback
legitimately returns void; no acquisition body or link is reached in this probe.

Preserve the complete T domain, callback result/error and source cleanup order. Acquire descriptors
in order and release accumulated, still-leased handles in reverse order on every normal/exceptional
exit. A callback can release manually or destroy the pool; a release exception may replace the
pending return/error. Void needs control-flow state without a void value object; do not remove
finally, copy pool owners, invent successful cleanup, narrow T or add a replacement native body.
No bracket, callback, lease, GL cache or GPU operation was executed by these compile/link probes.
