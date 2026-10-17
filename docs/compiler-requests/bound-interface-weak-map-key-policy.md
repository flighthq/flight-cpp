# A Flight interface bound to an external key type never reaches its weak-key policy

`new WeakMap<GlContext, V>()` is the most common weak map in Flight, and it is refused with
`cpp-weak-map-key-representation-unproven` although every native ingredient exists.

Measured at Flight `a627849` / flight-compiler `806d257`.

## Minimal TypeScript

```ts
// @flighthq/types
export interface GlContext extends Pick<WebGL2RenderingContext, GlContextMember> {}

// a consumer
const programs = new WeakMap<GlContext, GlFullscreenProgram>();
```

## What exists

- The SDL GL binding maps `WebGL2RenderingContext` to `flight::host_sdl::WebGl2Context` with
  `"weakKeyPolicyTargetName": "flight::host_sdl::WebGl2ContextWeakPolicy"` (`bindings/sdl-gl.json`).
- The profiled types tree emits `using GlContext = flight::host_sdl::WebGl2Context;`
  (`generated/include/flight/types/gl_context.hpp`).
- `WebGl2Context` carries the weak-key surface: identity is the shared surface owner, so two copies of
  one context are one key, and `weaken`/`lock_weak` hold that owner weakly
  (`include/flight/host_sdl/webgl.hpp`, `WebGl2ContextWeakPolicy`).

## Why it is refused

`getWeakMapKeyRepresentationCpp` consults the external weak-key policy only when
`type.reference.kind === 'ambient'`. `GlContext` is a Flight-declared interface, so its reference kind is
`binding`, and the policy branch is never reached. The fallbacks don't recover it: the representation plan
does not report `flightReference` for a type that maps to an external shared value, and `resolveAlias`
returns nothing because an interface is not an alias. No binding entry can change this. A `GlContext`
type binding carrying the same policy was measured to change nothing (`docs/generation-state.md`, "the
weak-key policy is only consulted for ambient types").

## Requested

When the key is a named Flight binding whose emitted native type is an external binding target, read
that target's `weakKeyPolicyTargetName` and emit
`flight::WeakMap<Key, Value, Policy>`, exactly as it already does for the ambient spelling. The
`GlContext` -> `WebGL2RenderingContext` -> `WebGl2ContextWeakPolicy` chain is the case to prove. Refuse,
as today, when no policy is declared or when the interface maps to more than one target.

This adds no new runtime surface: the map, the policy and the key type are already in use by
`render_gl/gl_render_state.hpp`. That module reaches them only because
`render-state-runtime-owner-and-initialization.patch` re-spells its key as `WebGL2RenderingContext`.

## Sites

- `@flighthq/effects-gl` `glEffectBlitShader.ts:46-48`: `blitOffsetShaders`, `blitShaders`, `eraseShaders`.
  Supplied meanwhile by the partial override `effects-gl-blit-shader-context-caches`; G2 cluster
  `undeclared-identifier-3e3cccce` (8 headers).
- `@flighthq/scene2d-gl` `glClipContours.ts:122`: `clipPrograms`. Supplied meanwhile by the partial override
  `scene2d-gl-clip-contours-context-cache`; G2 cluster `undeclared-identifier-644a9b33` (2 headers).
- `@flighthq/scene2d-gl` `glVelocity.ts:390`: `_velocityPrograms`. Not supplied: the program
  factory separately refuses its [cache value-owner rebind](contextual-union-map-value-carrier.md#g2-gl-velocity-program-lookup-and-rebind),
  and traversal/writers remain refused. G2 original1 stays0/1; no cache-only override or drawing claim.
- The other `GlContext`-keyed refusals counted in `docs/generation-state.md` (16 at the time of that count).

## Emission defects in the same module

The override also corrects these; they are listed so that a fix for the key doesn't leave them behind.

- `{ ...base, locOffset }` is emitted as `{.loc_offset = …}` on a struct deriving from
  `GlFullscreenProgram`, so the spread is dropped and the program, attribute locations and texture
  uniforms are left value-initialized.
- `type BlitOffsetShaderLocations = GlFullscreenProgram & { locOffset }` gets two unrelated carriers: a
  flattened named struct and the derived structural one. The getter returns the first but builds and
  caches the second.
