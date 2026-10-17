# A Map value built from a nullable enum loses its carrier when it is read back

`new Map(keys.map((key) => [key, read(key)]))` -- a map whose values come from a call returning `T | null` -- refuses
at emission when the map is later iterated and its value is handed to a call, because the rebound value's carrier no
longer matches the carrier the destination declares.

The family is `cpp-contextual-union-inequivalent [source-portability]`, whose message asks for one thing: every
alternative of the source expression must name exactly one alternative of the destination union, and the declared
collection's value owner must be preserved before flow expansion rather than re-derived from the lookup.

## Minimal TypeScript

```ts
enum Level { A = 0, B = 1 }
function readLevel(key: string): Level | null { void key; return Level.A; }
function writeLevel(value: Level): void { void value; }

export function f(channels: string[]): void {
  const saved = new Map(channels.map((channel) => [channel, readLevel(channel)]));
  for (const [channel, value] of saved) {
    if (value !== null) writeLevel(value);
    void channel;
  }
}
```

Reduced from this at flight-compiler `2b687ea`; the same shape with the write inside the `null` branch
(`if (value === null) writeLevel(channel, null); else writeLevel(channel, value);`) refuses identically. Both report:

```
contextual C++ union conversion requires equivalent source union evidence: target optionalSingle carriers [double]
from call source optionalSingle carriers [Level].
```

Building the same map with `new Map<string, Level>()` and `set` in a loop emits, so it is the tuple-mapped
construction (or the lookup that follows it) that loses the value owner, not the map itself.

## Measured at the real site

`@flighthq/debug`'s `debug.ts` refuses for the same family and is the only module of that package that does:
`enableDebug` (source line 41) is marked `NOT GENERATED ... cpp-contextual-union-inequivalent [source-portability]`
while every other function of the module emits. Its reported carriers are the same disagreement one step wider:

```
target multiVariant carriers [double, flight::Any] from binary source singleValue carriers [flight::types::LogLevel]
```

That is the map of saved per-channel levels -- `new Map(channels.map((channel) => [channel, getLogChannelLevel(channel)]))`,
with `getLogChannelLevel` returning `LogLevel | null` -- read back in the rollback loop and handed to
`setLogChannelLevel`. I did not reduce the exact carrier pair to a standalone repro; the reduced shapes above are the
same family one enum narrower, and the site is named by the emitter itself.

## Affected modules

- `enableDebug` in `@flighthq/debug` (`debug.ts`): refused, not supplied. Nothing else in the module is blocked, so one
  faithful lowering here would let the whole module emit.
- The reduced shapes above have no module of their own; they are the family's smallest forms found so far.

## Required shape

The rebound value keeps the declared collection's value owner: a map built from `T | null` reads back as `T | null`
with `T`'s carrier intact, so a call taking `T` after the null check receives it without a re-derived `Any`.

## Carried by no patch, and the attempt is recorded

Filed only, with one measured attempt behind it. Rewriting `enableDebug`'s map construction into the shape this file
shows emitting -- `const savedChannelLevels = new Map<string, LogLevel | null>();` filled by a `set` loop -- was
registered as a source patch, applied by generation, and **did not clear the refusal**: the module still reports 3 of 4
emitted with the identical reason, same line (41) and same carrier lists. The patch was withdrawn rather than carried,
because a patch that does not remove the refusal it names is exactly the debt this repository refuses to accumulate.
That result is itself evidence: the tuple-mapped construction is necessary to the reduced reproduction but is not the
trigger at this site, so the remaining candidates inside `enableDebug` are the other expressions that mix the declared
`LogLevel` with a wider carrier.

A second attempt is not made here for an unrelated reason: at this base the package's headers cannot compile at all --
`flight/render/render_state.hpp` (the `@flighthq/render` row, still at 7e2fc7d with 11 of 25 modules emitted) includes
`flight/types/renderer.hpp`, which the current `@flighthq/types` row does not emit. Until that row migrates, no
emission change to this package can be verified by a build, and none should be claimed as one. The measured state is
recorded in `docs/parity/core.md`.

## G2 GL velocity program lookup and rebind

At Flight `a627849` / compiler `806d257`, `scene2d-gl/glVelocity.ts:324` declares
`ensureGlVelocityProgram(state: GlRenderState): GlVelocityProgram`. It reads the context WeakMap into
`let program`, returns the present hit, then constructs the program/buffer/three-location literal,
sets the same cache entry and returns that same miss owner. The local's source domain is
`GlVelocityProgram | undefined`; the source creates only one new owner on a miss.

The refusal is `cpp-contextual-union-inequivalent [target-runtime]`: target optionalSingle carrier
`Ref<program_quad_buffer_loc_corner_loc_clip_rect_loc_velocity_858a12a86c0566f4>` versus call-source
optionalSingle `Ref<GlVelocityProgram>`. This is a named cache owner versus anonymous rebound owner,
not the enum/number case above. Keep the collection's declared program owner through lookup, fresh
construction, rebind, cache write and return, preserving hit identity and the actual program, quad
buffer and locations. A copied sibling record or default program is not a conversion.

The cache key independently hits the [bound GlContext policy request](bound-interface-weak-map-key-policy.md).
The retained draw helper then names the refused program function; its exact declaration candidate
fails the actual unused-header compile at the [VIEWPORT property binding](external-property-method-name-collision.md),
so no repair, source rewrite, normal generation or actual program/cache behavior pass is claimed here.
