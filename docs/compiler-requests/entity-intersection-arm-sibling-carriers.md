# An entity intersection arm and the value that fills it are sibling C++ carriers

`@flighthq/tray` is refused for a result union whose failure arm is an intersection of `Entity` with a
distributed union, while the value that fills that arm re-spells the same members inline. The two are
emitted as sibling structs, so the value cannot enter its own arm. A second, independent defect keeps
even the header the emitter wrote from compiling.

Measured at `2b687ea`, flight `a627849` with `pkg:compile -- tray`: **0 of 4 headers compile**, and
`tray.ts` refuses at emission.

## Blocker 1 — the refusal

`@flighthq/types`, `packages/types/src/Tray.ts:65`:

```ts
export type TrayCreateResult<Tray extends TrayIcon = TrayIcon> =
  | (Entity & { readonly outcome: 'created'; readonly tray: Tray })
  | (Entity & Exclude<TrayCreateCapabilityResult, { readonly outcome: 'created' }>);
```

and `@flighthq/tray`, `packages/tray/src/tray.ts:79`, which builds the failure value itself:

```ts
const out = allocateEntity<Entity & { error?: unknown; outcome: 'tray-create-failed' }>();
initializeTrayCreateFailedResult(out, error, 'tray-create-failed');
return finishEntity(out);
```

The emitter writes the union's second arm as

```cpp
struct entity_runtime_key_outcome_error_789f203c1f963b68 : public flight::types::Entity {
  flight::String outcome;
  std::optional<flight::Any> error;
};
```

and the value's own intersection as `flight::Ref<entity_runtime_key_error_outcome_84d7b91e76b58a2a>`.
Two sibling structs describe one conceptual arm, so the return cannot convert:

```
cpp emission failed for @flighthq/types/packages/types/src/Tray.ts: contextual union value type
flight::Ref<entity_runtime_key_error_outcome_84d7b91e76b58a2a> has a distinct intersection owner from
its matching union arm over nominal base Entity. The source intersection and destination arm are sibling
C++ object carriers; converting between them would require sibling retyping or copying the object. Name
one shared declared arm type in the union contract and use that same arm declaration in both the union
and value storage
```

The compiler's own advice is the remedy, and it is currently not expressible: the arm is
`Entity & Exclude<…>`, a distributed intersection with no single declaration to name, and the value site
re-spells the members inline rather than referring to the arm.

## Blocker 2 — the emitted runtime accessor

Independent of the refusal, and present in the partial header the emitter did write:

```
flight/tray/tray.hpp:212: no matching conversion for static_cast from
  'std::optional<flight::Ref<flight::types::EntityRuntime>>'
  to 'std::optional<flight::Ref<flight::TrayRuntime>>'
```

The line is the emitted accessor for `tray.ts:414`:

```cpp
return static_cast<std::optional<flight::Ref<TrayRuntime>>>(tray->entity_runtime_key);
```

`TrayRuntime` is source-local — `interface TrayRuntime extends ReturnType<typeof createEntityRuntime>` —
so its members live in an emitted struct that is unrelated to the `EntityRuntime` the erased slot holds,
and the cast has nothing to follow. The same idiom was carried for `@flighthq/node` by
[`node-exact-runtime-and-owner-paths`](../../source-patches/manifest.json) (construct the exact owner
directly and read the slot without erasure); tray.ts has the same two sites — the
`createEntityRuntime() as TrayRuntime` construction at line 89 and the accessor at line 415.

## Affected modules

`@flighthq/tray` in full: `contract.ts`, `enableTrayGuards.ts` and `index.ts` are dependency-refused
from `tray.ts`, and all four headers fail to compile.

Ten functions in `tray.ts` are refused, which is why the remedy is a unit of work rather than one
expression: `createTrayIcon`, `initializeTrayCreateFailedResult`,
`initializeTrayCreateProviderFailureResult`, `initializeTrayCreateSuccessResult`, `destroyTrayRuntime`,
`isTrayIconAnimating`, `attachTrayEvent`, `startTrayIconAnimation`, `queueAnimationWrite` and
`invokeUpdate`, under `cpp-intersection-member-shapeless` (four), plus
`cpp-uninitialized-variable-missing-type-evidence` and `cpp-reference-assertion-without-heritage`.
Only the first refusal is reported per module, so the set is a floor.

## Carrying workaround

[`tray-create-result-and-runtime-owner.patch`](../../source-patches/tray-create-result-and-runtime-owner.patch)
names the create-result members once in `types/Tray.ts` (`TrayCreateSuccessResult`,
`TrayCreateFailedResult`, `TrayCreateProviderFailureResult`), uses those declarations in the union and at
every value site in `tray.ts`, declares `TrayRuntime`/`TrayReleaseRuntime` beside `TrayIcon` and narrows
the entity's runtime slot to it — the shape `@flighthq/types` already gives Node — constructs the
complete runtime owner instead of allocating and asserting one, and reads the narrowed slot without a
cast. It applies and reverses cleanly against `a627849`.

**It advances the refusal without clearing it.** Measured in a scratch inventory (nothing committed):
the module-level refusal moves from the sibling-owner error above to

```
cpp emission failed for @flighthq/tray/packages/tray/src/tray.ts: narrowed member structural switch case
must identify one C++ variant alternative
```

which is emitted from `tray.ts:85`, `'error' in result ? result.error : undefined` — a member-presence
test whose narrowed shape matches three of the four arms, so it does not identify one C++ alternative.
That is a second, independent defect in the same module; the remainder of the chain is not yet walked.
`@flighthq/tray` is therefore still `partial` with 3 of 4 modules emitted and no header compiling, and
`pkg:generate -- tray` must not be run against the committed inventory until the `types` row is refreshed
on top of this patch.

## Same defect in physics2d (a627849)

`@flighthq/physics2d` `colliderTransform.ts` `createPhysics2DColliderWorldShape` returns
`CollisionBuiltInShape2D & Entity`, and allocates each arm as its own intersection, such as
`allocateEntity<(CollisionCircle2D & { kind: 'circle' }) & Entity>()`. Two boundaries follow.
- Inside the function, the allocated intersection and the return union's arm are sibling owners.
  physics2d-collider-world-shape-construction carries this by constructing the declared arm directly, which is
  the same allocation with no cast and no copy.
- `world.ts` then stores the result in `Physics2DCollider.world`, declared as the plain
  `CollisionBuiltInShape2D`. `createPhysics2DCollider` (line 177) and `invalidatePhysics2DCollider` (line 399) are
  refused, because every `…_kind_entity_runtime_key_…` arm pairs uniquely with a plain arm only as a sibling.
  A cast would retype the object, and a copy would split the collider's world shape from the entity the step
  rewrites in place.

No patch is carried for `world.ts`. The only source-level fix is to change `Physics2DCollider.world`'s declared
type in `@flighthq/types`, which is a cross-package carrier change. So natively a collider cannot be created
through `createPhysics2DCollider`, and `hydratePhysics2DWorld` is refused with it.
