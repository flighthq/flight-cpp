# The generic `??` fallback evaluates its right operand even when the left is present

`a ?? b` evaluates `b` only when `a` is null or undefined. The pinned C++ backend's generic fallback lowers
it to `a.value_or(b)`, and `std::optional::value_or` evaluates its argument before the call. Every right
operand therefore runs every time: a call, a getter, an assignment, a throw.

## Minimal TypeScript

```ts
export interface Screen { presentationView: View | null; acquireView: (() => View) | null }
export function make(): View { throw new Error('make'); }
export function f(s: Screen): View | undefined { return s.acquireView?.() ?? make(); }
```

With a present `acquireView`, JavaScript never calls `make()`. The pinned output calls it, and throws:

```cpp
return (/* optional call */).value_or(make());
```

The real case is render-wgpu's `bindWgpuScreenRenderTarget`:
`target.acquireCaptureTexture?.(target) ?? target.context.getCurrentTexture()` calls `getCurrentTexture()`
even when a capture texture exists, and on a host with no WebGPU provider it throws where JavaScript does not.

## Expected C++

The right operand behind the presence test, evaluated at most once, with the left evaluated exactly once:

```cpp
return flight::nullish_or<host::View>(/* left */, [&]() -> host::View { return make(); });
```

A literal or `undefined` fallback has no evaluation and may keep `value_or`. An identifier or member read
may not: a getter, a proxy or an ordering can observe it. A fallback that can itself be absent keeps the
result optional (`Target` is `std::optional<payload>`), so nothing is flattened into a value that does not
exist.

## Carrying workaround

`source-patches/compiler-lazy-nullish-fallback.patch`. It uses the existing shared lazy primitive
`flight::nullish_or<Target>` in `flight/presence.hpp`, so no second helper is added. Entering a slot declared
as an opaque host handle, an absent result goes through `flight::host_handle_or_absent`, which compiles only
for a declared absence carrier. The primitive coalesce paths, which already test presence first, are unchanged.
