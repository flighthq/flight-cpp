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

## G2 dedicated GL box-blur cache and shader owner

At Flight `a627849` / compiler `806d257`, effects-gl/glEffectBoxBlur.ts46 declares
const boxBlurShaders = new WeakMap<GlContext, BoxBlurShaderLocations>(). Its exact NOT GENERATED
marker refuses cpp-weak-map-key-representation-unproven [source-portability]; get_box_blur_shader
still reads and writes the missing binding. Original1 undeclared-identifier-0b8d3b44 is measured with
Clang22.1.8 jobs1: **1 attempted / 0 pass / 1 fail / 0 unattempted**,57.1s, at this header115.
The existing native alias, binding and WebGl2ContextWeakPolicy are present; the compiler's ambient-only
policy branch in getWeakMapKeyRepresentationCpp still excludes the Flight GlContext interface.
This is another site of this request, not a new missing native key capability.

Keep one module cache keyed by the original state.gl context's shared identity. Copies of one context
must hit one entry, distinct contexts must remain independent, and key storage must stay weak. A hit
returns the identical cached shader-location object without compilation or new uniform queries. On a
miss, compile the actual BOX_BLUR_FRAGMENT_SRC via compileGlFullscreenProgram, then create the source's
fresh BoxBlurShaderLocations object by copying all base fields (program, locPosition, locTexCoord,
texture, textures) before querying u_texelSize, u_radius, u_direction, u_edgeColor and u_useEdgeColor,
in that order on the same base.program. Preserve the handles and the shared textures array identity;
the source spread creates a fresh record, not a new GL program or deep-copied array. Only after all
queries/construction succeed, cache and return that same fresh owner. Keep existing non-null uniform
assertions and failures; no default location, swallowed error, cache-on-error or extra teardown policy.
The source has no explicit cache clearing or program-destruction operation to invent here.

Two further defects are visible in the committed getter, independently of the first native error.
BoxBlurShaderLocations is a flattened ReferenceEnabled record, while the lookup local and fresh
construction name a separate anonymous struct deriving GlFullscreenProgram. The latter cannot become
the former through a native owner conversion. Preserve the declared cache-value owner through lookup,
construction, rebind, set and return, as required by the existing
[cache-value carrier request](contextual-union-map-value-carrier.md#g2-gl-velocity-program-lookup-and-rebind).
Also, the emitted fresh initializer spells only the five added locations and never copies base:
its inherited program/attribute/sampler fields are left value-initialized. Restore the actual spread,
including its evaluation order and aliases; a zero/default program or base-prefix replacement is not
source behavior. These are inspected emitted defects, not later errors isolated by a synthetic cache.
The same two patterns are already recorded for the sibling blit module above. That partial override
does not define box_blur_shaders or repair this dedicated getter.

An extern cache declaration supplies no actual cache, and two exact function prototypes cannot repair
these owner/spread defects. No new binding, cache-only override, source rewrite, repair engine change,
normal generation or native cache/shader behavior is carried by this disposition. The separate
[blur destination and full recipe](interface-heritage-widening.md#g2-gl-box-blur-pass-destination-and-source-recipe)
and [retained omitted calls](retained-declaration-calls-omitted-callee.md#g2-gl-box-blur-refuses-a-cache-before-its-omitted-pass-calls)
remain independent. Only the original one-header compile is measured; no GL/program/cache call runs.
