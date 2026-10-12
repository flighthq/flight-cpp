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

None. No patch and no override is carried for `@flighthq/tray` today; `pkg:tier -- tray` reports it
`partial` with 3 of 4 modules emitted and all four headers attempted and failing, which is the honest
reading rather than a promotion. The shape of a remedy is visible from the two precedents — a `flight`
source patch that names the failure arm once in `types/Tray.ts` and uses that name at the value site,
plus the node-style exact runtime accessor — but it spans two packages and has not been authored.
