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

## G2 resource reveal keeps scene resource and callback identity

At Flight a627849 / compiler806d257, collectPendingTextureOwners in
scene3d-resources/revealScene3DResourcesOnResolve.ts:82 still refuses at the Readonly<Node3D> argument
to isMesh(unknown). The retained public reveal then fails at missing collector167. The
[exact declaration candidate](retained-declaration-calls-omitted-callee.md#g2-scene3d-resource-reveal-collector-declaration-fails-before-link)
passes the existing matcher but fails actual unused-header compilation at the later tween call; no
positive link/run, qualified negative link, repair or changed normal generation is carried.

The source guard recognizes an object with non-null geometry, including custom Mesh kinds. Replacing
it with MeshKind equality, copying a Node3D row, recovering a narrower native pointer via shared_object,
boxing only visible fields or manufacturing writable authority loses the source domain or owner.
Any already preserves exact native object references, and StructuralRef retains shared row/native
owners, but the compiler/runtime do not share the required erasure of a readonly view over a wider
owner. A typed pre-erasure guard needs its own faithful source/API and owner proof; none is carried.

The complete recipe must preserve these boundaries:

- Visit scene.root before its descendants in the existing traversal order. For each mesh use the same
  original node, iterate materials in order, skip null, clear the reusable slots before each material,
  and invoke the resolver registry's actual kind lister. Custom/overridden listers and their append
  order remain supported; an unregistered kind contributes nothing. Do not replace this reveal-only
  ownership walk with global scene-resource discovery or restrict material families.
- For every listed Texture, find the first scene.resources reference whose textures back-edge includes
  that exact Texture. Skip absent refs, textures already carrying any source, and refs in Failed state.
  Do not replace the rule with an Unresolved/Loading-only filter: a source-less ref in another state
  still follows the actual test. Preserve all Texture alternatives and resource identities; two sampler
  Texture objects sharing one image reference remain separate pending keys. This collector neither
  fetches/decodes data nor constructs a provider or source.
- Use one shared RevealOwner per original node, created on its first pending texture. Deduplicate
  repeated texture slots across that node's materials with its Set, and retain the same RevealOwner
  in the owners array and each Texture's owner list. A shared Texture can name multiple nodes. Keep
  node/material/slot order, original Map/Set key identity and handle aliasing; no copied mesh, fabricated
  owner list, universal empty collector or narrowed registry gives equivalent writes.
- Finish collection before setting each tracked node.alpha to options.from ?? 0. Preserve default
  fadeSeconds0.4, custom values and optional ease. Untracked/already sourced/failed/unlisted nodes retain
  their existing alpha. Collection errors occur before the alpha loop and before signal enabling;
  do not hide nodes eagerly while a lister is still running or catch errors as empty success.
- Enable the resolver's existing signal group after hiding and connect the **same** slot first to
  onResourceResolved then onResourceFailed. The slot looks up event.texture by identity; ignore
  untracked textures, erase the tracked Texture entry before visiting its owners, remove it from each
  pending Set, and createTween with the caller's manager/original node/duration/alpha1/ease only when
  that Set becomes empty. Repeated/forwarded terminal events cannot schedule duplicate fades. The
  slot does not use event.ref to substitute an owner key or perform state validation.
- Wait for **all** pending textures on each owner, including mixed resolved/failed events. The source
  comment saying an object fades when the first resolves is stale: pending.size === 0 and the pinned
  two-texture tests require the last tracked texture. Preserve failure as settlement and cancellation
  as pending until a later actual resolution; inventing a failure/resolve event on cancel changes this
  recipe. The resolver updates/binds resources before its terminal events; reveal only subscribes.
- Return a disposer that disconnects this same slot from resolved then failed, retaining the original
  signals. It does not cancel loads, remove caller-owned in-flight tweens, restore alpha or tick the
  manager. The application advances tweens and needs a node-opacity-honoring renderer; no immediate
  visibility flip, default-manager substitution or fake animation is a resource reveal implementation.

Pinned source tests inspect default/custom initial alpha, duration/completed fade, multiple textures,
failure-plus-success, bound/failed exclusions, unlisted/mixed material behavior, shared Texture owners,
stray events and disposal. They were read as source requirements, not run as native recipe/provider/
resource/scene behavior. The existing scene, resolver/runtime/signal, Texture, node and local owner
carriers are reused by the candidate; none is copied or replaced. Header/functionality gain remains0,
and package/init/G1/global products are not recounted by this scoped request disposition.
