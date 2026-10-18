# A call to a generic function converts its argument to the callee's unsubstituted parameter type

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, the generic callee is emitted correctly as a C++ template.
But a non-generic caller materializes or converts the argument to the callee's parameter type as
written, with the callee's own type parameter left in it. That parameter is not in scope at the call
site, so the header fails on an undeclared identifier.

## Source

```ts
// packages/shape/src/shapeBoundsRegistry.ts
export function registerShapeBoundsCommand<K extends ShapeCommandKey>(command: ShapeBoundsCommand<K>): void { ... }

// packages/shape/src/registerDefaultShapeBoundsCommands.ts
registerShapeBoundsCommand({ fillBounds: defaultShapeBoundsFlush, key: 'beginTextureFill', strokeBounds: defaultShapeBoundsFlush });
```

## Emitted mismatch

The callee is a proper template (`shape_bounds_registry.hpp:43`, which compiles):

```cpp
template <typename K>
inline void register_shape_bounds_command(flight::Ref<flight::types::ShapeBoundsCommand<K>> command) { ... }
```

The non-generic caller builds the literal's owner with that `K` still in it:

```cpp
inline void register_default_shape_bounds_commands() {
  flight::shape::register_shape_bounds_command(([&]() { ...
    return flight::make_ref<flight::types::ShapeBoundsCommand<K>>(flight::types::ShapeBoundsCommand<K>{.key = object_member_key, ...});
  }()));
```

```
flight/shape/register_default_shape_bounds_commands.hpp:17: error: use of undeclared identifier 'K'
```

The same defect appears with a conversion rather than a construction. A non-generic caller of a
`Traits`-generic node helper emits `flight::structural_ref_cast<flight::types::NodeOf<Traits>>(...)`
(`flight/scene3d/scene_document.hpp:106`).

## Expected lowering

At the call site, substitute the type arguments TypeScript inferred for the call before naming the
parameter type, then construct or convert to that instantiation. Here the inferred `K` is the
`'beginTextureFill'` literal, whose carrier is the constraint `ShapeCommandKey`, which is the same
instantiation the registry already stores:

```cpp
flight::make_ref<flight::types::ShapeBoundsCommand<flight::types::ShapeCommandKey>>(
    flight::types::ShapeBoundsCommand<flight::types::ShapeCommandKey>{...})
```

Alternatively, leave the template argument to deduction from an argument whose type is already
instantiated. Neither the runtime nor a repair can choose the argument: it is the checker's
inference, and a fixed constant would be a guess.

## Affected clusters

- `undeclared-identifier-a74215ca`, `K`, 6 headers: `shape/register_default_shape_bounds_commands.hpp` and
  the five `scene2d_formats` SVG headers that include it.
- `undeclared-identifier-993e82f1`, `Traits`, 29 headers. That cluster belongs to builder4 (E0a), and its
  rows are not changed here.

## G2 Lottie mask handler refuses before emitting the call

At compiler `806d257` / Flight `a627849`, `scene2d-formats/src/lottieMask.ts:21`
is refused as `cpp-contextual-union-inequivalent [target-runtime]`. This site stops during conversion
planning rather than emitting an undeclared type parameter into C++. Its diagnostic compares:

```text
target optionalVariant carriers
[Ref<LottieAnimatedProperty<T>>, Ref<LottieStaticProperty<T>>]
from property source multiVariant carriers
[Ref<LottieAnimatedProperty<Ref<LottieShapePath>>>,
 Ref<LottieStaticProperty<Ref<LottieShapePath>>>]
```

The actual source `LottieMask.pt` is `LottieAnimatable<LottieShapePath>`. The non-generic handler
calls `initialLottieValue(first.pt)` and `isAnimatedLottieProperty(first.pt)`; their declarations
are generic in `T`. The diagnostic leaves that callee parameter in the destination arm instead of
using the concrete shape-path instantiation. Substitute the checked call's type arguments before
contextual union conversion, retaining the same animated/static property owners and their nested
keyframes/path values. Optional lifting must retain the same active owner; a new sibling property,
path or keyframe collection, an erased Any conversion or a cast asserting unrelated native owners
is not an implementation of this call.

The complete source context holds masks as readonly views in document order, the original Node2D
target and the original import context. The body declines an absent first mask, non-additive mode,
inversion or multiple active masks, then declines an unreadable initial path. It installs a clip from
the actual initial outline. For animation, it retains that target and one current numeric array in
the update callback, rebuilds the clip from each sample and passes the same import context to channel
construction. The predicate identifies keyframes structurally, including exports with no animated
flag; neither a flag-only substitute nor a signature narrowed to static paths preserves the domain.

Fixing call specialization alone does not supply the independently refused helpers:
`initialLottieValue` still refuses at its optional generic return, `isAnimatedLottieProperty` at
unguarded variant `length`, and `toLottieShapePath` at parsed/erased readonly path recovery. The
[existing readonly recovery request](erased-value-readonly-row-recovery.md#affected-flight-modules)
already distinguishes that parsed JSON case from exact nominal-owner recovery; no exact-owner cast
is assumed here. These are inspected refusal markers, not additional measured header failures.

The retained [mask family](retained-declaration-calls-omitted-callee.md#g2-lottie-mask-family-requires-the-refused-function-during-initialization)
still requires the omitted callback during global initialization. Its original one-header check is
**0/1 pass**, and a scratch exact declaration fails the unused positive link on this same handler.
No declaration, owner copy, fake mask/handler/body, source rewrite or narrowed mask domain is carried.

## G3 shared compiler substitution carry (qualification pending)

[`compiler-inferred-invocation-type-arguments.patch`](../../source-patches/compiler-inferred-invocation-type-arguments.patch)
retains the checker's complete resolved type arguments on each invocation, separately from explicit
source syntax. Missing C++ substitutions use that checked list before a source-declared generic
default. Existing direct native-owner inference remains first, preserving named callable-result owners
rather than manufacturing their expanded structural shapes. Overload implementation positions are not
assumed to match the selected signature; incomplete checked lists are omitted entirely.

The focused regression has a concrete owner inheriting `Node<Extra>`, a distinct declared `Fallback`,
and a caller-local `Caller`. Without the patch it emits free `Callee` or chooses `Fallback`; with the
patch it retains `Extra` and the exact lexical `Caller` binding. Fresh local/imported command literals,
explicit and nongeneric calls, type traversal, malformed evidence and foreign lexical bindings are
also checked. All **11 cases pass**, including three native syntax fixtures; a pin-only control fails
nine. Root strict TypeScript, focused format/lint, composition with the existing 67 compiler patches,
and that composed production build pass.

This initial carry claims compiler evidence only. SDK generation awaits an explicit Foreman capacity
grant. Assigned qualification is **51 historical rows**: 30 `Traits`, six `K`, four Movieclip, five
Billboard and six scene2d-formats rows whose effective primary request is heritage argument deduction.
The other six command-trait rows and the separate out-of-trait-owner cluster are outside this scope.
No header status or runtime capability is promoted until scoped compilation and an actual previously
blocked original public consumer compile/link/run have been measured.

The same carry now follows imported function declarations through the existing exact value-owner
lookup used for emitted names. Named re-exports, local forwarding, star exports, diamonds and cycles
therefore use the generic parameters of the actual declared function. Three facade cases construct an
anonymous argument owner under the prior direct-only lookup and retain `Command<string>` with the fix;
direct, star and diamond controls pass either way. The updated focused run passes **21 cases** (16 for
this cause plus five existing owner/default/template regressions), including the emitted named-facade
header graph compiled through its actual includes. Scoped SDK/public-consumer qualification remains
pending in the now-granted generation slot.
