# A readonly structural row cannot yet enter `unknown`

`unknown` uses `flight::Any`, whose object alternative retains an exact nominal reference. A
`Readonly<T>` value is instead a structural row: it may be a view over a wider owner, and that owner
cannot be recovered by extracting or copying the row's visible members. The compiler therefore
refuses the erasure rather than losing identity or native-owner evidence.

## Minimal TypeScript

```ts
interface Widget {
  value: number;
}

export function erase(value: Readonly<Widget>): unknown {
  return value;
}
```

At flight-compiler `2b687ea` and flight `a627849`, this refuses with rule
`cpp-erased-structural-row-construction-unrepresented`:

```text
flight-cpp cannot erase structural row Readonly into unknown
```

## Pinned boundary

| source shape | result |
| --- | --- |
| return `Readonly<Widget>` as `unknown` | refuses the structural-row erasure rule |
| pass `Readonly<Widget>` to an `unknown` parameter | refuses the same rule |
| place a readonly row in `Readonly<Record<string, unknown>>` | refuses the same rule |
| call an `unknown`-accepting type predicate with a readonly row | refuses the same rule |
| pass the row to a predicate typed as `Readonly<Widget>` | emits and keeps the structural carrier |
| return writable `Widget` as `unknown` | emits `flight::Any::object(value)` |
| return a class instance as `unknown` | emits `flight::Any::object(value)` |

The writable-interface and class controls retain an exact `flight::Ref<Widget>` owner, so the
existing `Any::object` alternative is sufficient. The readonly cases are not rejected merely because
the destination is `unknown`; they are rejected because the source is a row view whose retained
native owner may be wider than its visible schema.

## Required carrier behavior

A general erasure needs an `Any` structural-object alternative constructed from the row owner plus
its retained native type. It must preserve the one source object, its projection identity, and enough
type evidence for checked recovery. In particular, a widened row may intentionally have no
`shared_object()` for the narrower projected type, so that accessor cannot be used as a nominal
fallback.

Copying the visible members would create a second object; boxing only the row schema would lose the
native owner; and an unchecked native cast would manufacture recovery evidence. Until the runtime
and compiler share a carrier satisfying those constraints, the current refusal is sound. A typed
pre-erasure predicate is a valid narrower lowering only when the API is actually allowed to keep that
type.

## Existing compiler coverage

This is not a missing unit reduction. The pinned compiler already has focused cases for direct
returns, calls, computed Record access, generic rows, and `NodeAny`, plus imported-module cases for
the command and Scene3D predicate shapes. This page records their current Flight corpus reach and the
runtime contract they still require; it does not claim a new untested emitter rule.

The read-side requests are separate. [An erased Error read](compiler-erased-error-view-and-instanceof-guard.md)
starts with an `unknown` carrier and asks for a checked Error projection. A
[checked erased WeakMap view](compiler-erased-ref-weak-map-view.md) recovers exact nominal key and
value references from a collection with its own type evidence. This request constructs the erased
carrier from a structural row in the first place.

## P-3D2 reach and wider corpus

`packages/scene3d-resources/src/revealScene3DResourcesOnResolve.ts` passes a
`Readonly<Node3D>` row to `isMesh(source: unknown)`. Its structural capability test is valid in
TypeScript, but the call first has to erase the row, so the module is a committed direct refusal.

The current full inventory has two other direct examples of the same carrier gap:

- `packages/command/src/command.ts` erases `Readonly<NodeAny>` before dynamic Record lookup.
- `packages/textureatlas/src/enableTextureAtlasGuards.ts` puts a readonly `TextureAtlas` row into a
  `Record<string, unknown>` log payload.

Those sites ask different questions after erasure, so clearing this construction rule is not a claim
that their later computed access, predicate, or logging paths all emit. No source rewrite, compiler
patch, or generated repair is carried for the Scene3D site.

For textureatlas this is the only refused body in `enableTextureAtlasGuards.ts`, but it is not safe
to "repair" by dropping the `atlas` field from the warning data or copying the visible atlas members.
The upstream warning includes the original atlas object alongside `message`, `regionId`, and
`status`; the existing `Any::object` carrier requires an exact nominal `Ref`, while the callback is
allowed to receive any `Readonly<TextureAtlas>` structural owner. Until `Any` can retain that row
owner, the built-in enabling path must remain explicitly partial rather than claim a complete guard
whose observable log payload differs.
