# Nullable and optional storage needs two absence channels

A member declared `T | null` uses `std::optional<T>` for null. Making that member optional as part of
`Partial<Row>` asks the same optional to also represent missing/`undefined`. A read can no longer
distinguish the two source states, causing presence-test refusals and invalid three-way conversions.

## Minimal TypeScript

```ts
interface NodeData { runtime: object | null }
export function state(value: Readonly<Partial<NodeData>>): number {
  if (value.runtime === undefined) return 0;
  if (value.runtime === null) return 1;
  return 2;
}
```

## A second site: the menu validator

`@flighthq/menu`'s `_validateItem` refuses with the same rule at this pin, and its message names the shape
of the fix it wants:

```
cpp emission failed for @flighthq/menu/packages/menu/src/menu.ts: narrowed C++ union projection requires
proven present payload: the narrowing still admits absence, so nothing proved the payload present and
reading the value alternative here would unpack storage that may still be holding the sentinel the test
left reachable. Test both at once with `x != null`, or narrow the value into a present local the same way
```

The tests it refuses are `item.label !== undefined`, `item.accelerator !== undefined`,
`item.submenu !== undefined` and `item.checked !== undefined` on a `Readonly<MenuItemTemplate>` whose
emitted row is a single optional per member. `@flighthq/menu` is `assisted`: the
`menu-recursive-template-validation` override supplies that one declaration, and the validator's rules are
exercised natively by `tests/generated_menu_test.cpp`.

## Expected C++

The partial nullable cell needs all three states:

```cpp
struct NodeDataPartial {
  flight::Presence<flight::Ref<void>> runtime;
};

if (std::holds_alternative<flight::Undefined>(value->runtime)) return 0.0;
if (std::holds_alternative<flight::Null>(value->runtime)) return 1.0;
return 2.0;
```

Members that are only nullable or only optional should remain `std::optional<T>`.

## Affected modules

Current generated refusals span `@flighthq/node`, lighting, loader, mesh, render, texture, WGPU, menu,
and format parsers. The committed refusal taxonomy records this as
`cpp-presence-test-without-absence-storage`.

## Carrying workaround

There is no semantics-preserving downstream representation workaround: converting the existing
`optional<T>` after the read would invent whether absence meant null or undefined. Individual source
patches avoid some optional-chain spellings, while the refusal ledger deliberately keeps the remaining
sites blocked. This request closes when flight-compiler emits `flight::Presence<T>` (or an equivalent
three-state carrier) only for optional-and-nullable cells.

## Parameters: carried by a compiler patch

An optional nullable *parameter* (`f(x?: T | null)`) already has the three-state carrier this request
asks for. The parameter is an outer `std::optional` for its own presence around the declared `T | null`
`std::optional`. Only the projection was refused: after `x !== undefined`, the narrowed `T | null` still
"admits absence" (null), so the generic rule refused it. Reading it as the outer `value()` yields
exactly the inner `T | null` carrier.
[`compiler-nullable-optional-parameter-narrowing.patch`](../../source-patches/compiler-nullable-optional-parameter-narrowing.patch)
does that for this one shape. Fixtures pin the createChildLogContext form, a narrowing that still admits
omission (still refused), and an optional nullable member (still refused). An emitted function
compiled and run under clang returns distinct results for omitted, null and a value.

**Optional nullable members remain open**: they still have a single `std::optional` and still refuse with
`cpp-dual-sentinel-comparison-unrepresented` / `cpp-presence-test-without-absence-storage`.

## A third site: `@flighthq/connectivity`, where the union declares only `| null`

The same message refuses `attachConnectivity` at this pin, on the one test that decides whether a
subscription could be established:

```ts
const unsubscribe = hostConnectivityChange.subscribe(() => { … });
if (unsubscribe === null) return false;
_subscriptions.set(connectivity, unsubscribe);
return true;
```

`subscribe` is declared `(() => void) | null` — **null only, no undefined arm** — and the emitted carrier
is a single `std::optional<std::function<void()>>`. That carrier is what makes the test refuse: `=== null`
is unambiguous in TypeScript for this declaration, but a single optional represents null and absence with
one sentinel, so a test that names only null leaves the other one reachable and the emitter will not read
the value. The emitter's advice (`x != null`) is therefore exactly right *for this union*, and unlike the
`Partial<Row>` sites above it is not a weakening: no `undefined` can reach this value, so `!= null` and
`=== null` select the same set.

`@flighthq/connectivity` is `assisted`: the `connectivity-complete-module` override supplies
`attachConnectivity` (written as `!unsubscribe.has_value()`, the emitted optional's own presence test) and
re-exports the emitter's other ten functions verbatim; the function's semantics are exercised natively by
`tests/generated_connectivity_test.cpp`, including the unknown-as-unmeasured transition the nullable
baseline needs.

Two facts about this site worth recording against the request rather than against the emitter:

- it is the *call-result* form of the family, not a member read, so it is the shape the "affected modules"
  list above does not name;
- the emitter's fix direction is a source spelling that its own message supplies. Where the union really
  is `| null` only, taking that spelling is a semantics-preserving one-line source change; where an
  `undefined` arm exists it is not, and the three-state carrier this request asks for is the only fix.

## A fourth site: gizmo disposal's nullable parent

At flight `a627849`, `@flighthq/gizmo` refuses `disposeGizmoState` at
`if (parent !== null) removeNodeChild(parent, runtime.overlayRoot)`. The parent result is declared
`Node | null`, so its emitted `std::optional` has an unambiguous present/absent interpretation at this
site; the override tests `has_value()` before removal. Disposal remains idempotent, runs cleanups in
reverse order, and clears the four retained external references.

## A third site: equality with a literal (measured)

The same refusal fires when a `T | null` value is compared with a string literal and nothing tests presence
first. Equality with a present literal is itself the proof: `mode === 'add'` can only be true when `mode`
holds a string. Measured at flight-compiler `2b687ea` with all 44 compiler source patches applied: both
reductions below are refused with the message above. The guarded spellings emit, and the first compiles.

```ts
export type Mode = 'add' | 'normal' | 'multiply';
export function toDocMode(mode: Mode | null): 'additive' | 'normal' | 'multiply' | null {
  if (mode === 'add') return 'additive';      // refused
  if (mode === 'multiply') return 'multiply';
  if (mode === 'normal') return 'normal';
  return null;
}

export function toPex(config: Readonly<{ blendMode: 'add' | 'normal' | null }>, existing: Partial<{ src: number; dst: number }>) {
  const blendMode = config.blendMode;
  const src = blendMode === 'add' ? 770 : (existing.src ?? 770);   // refused
  const dst = blendMode === 'add' ? 1 : blendMode === 'normal' ? 771 : (existing.dst ?? 771);
  return { src, dst };
}
```

The emitter should lower `x === 'lit'` over a nullable carrier as "present and equal", which is what
`x !== null && x === 'lit'` already emits. Affected: `@flighthq/particles-formats` `spineSerialize.ts`
(`configToSpineBlendMode`) and `starlingPexSerialize.ts` (`configToDocument`). They are carried by the
`particles-formats-spine-blend-mode-present` and `particles-formats-pex-blend-mode-present` source patches.

## G2 raster shape null-presence refusal

At compiler `806d257` / Flight `a627849`, `drawGlRasterShape` in
`packages/scene2d-gl/src/glRasterShapeRenderer.ts:27` remains refused as
`cpp-presence-test-without-absence-storage`: a presence test against null has no absence channel in
storage for `property`. Both the header comment and refusal ledger omit the exact tested property;
the module-start location is not evidence for selecting one particular expression.

The source checks nullable renderer data, canvas/image capabilities and raster image/surface values.
The declared host capabilities and renderer-data slots here are `T | null`, so the shared rule does
not establish an optional-and-nullable three-state case. Preserve the actual declared absence domain
and emit its null test; do not silently fold it to a constant, replace a strict test with truthiness,
or guess a source patch from the unlocalized message.

The raster strategy replays every registered canvas command into real offscreen pixels. Its revision,
size and pixelRatio checks control surface recreation, GL cache deletion and image invalidation before
texture/quad submission. Those semantics and owner identities survive any presence repair. The existing
native shape-data override explicitly leaves raster surfaces, image upload and teardown unavailable;
no drawing implementation is supplied by this audit. The two headers remain 0/2 and the
[declaration-only candidate](retained-declaration-calls-omitted-callee.md#g2-raster-shape-declaration-witness-is-valid-but-callback-carriers-fail)
also fails before linking.
