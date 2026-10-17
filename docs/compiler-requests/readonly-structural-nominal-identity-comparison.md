# Strict identity between a readonly view and its nominal owner

A readonly structural reference and the nominal reference from which it was projected retain the same
object owner, but strict equality emits a direct comparison between their different C++ carriers. When
both values are nullable, that becomes `optional<StructuralRef<...>> == optional<Ref<...>>`, for which
there is no operator. The emitter keeps the declaration and marks no refusal, so the generated header
claims to be complete but does not compile.

Measured at Flight `a627849` / flight-compiler `2b687ea` in
`@flighthq/spritesheet` regenerated under binding profile set `070fadeb2a07`.

## Minimal TypeScript

```ts
interface Animation { frames: number[] }
interface Player { animation: Animation | null; elapsed: number }

export function play(player: Player, animation: Readonly<Animation> | null, restart = true): void {
  if (!restart && animation === player.animation) return;
  player.animation = animation;
  player.elapsed = 0;
}
```

The comparison is JavaScript object identity. `Readonly` changes write authority at the type boundary;
it does not create a second object or change identity.

## Observed C++

`spritesheet_player.hpp` gives the parameter a readonly structural carrier while the entity member keeps
the exact nominal owner:

```cpp
inline void play_spritesheet_animation(
    flight::Ref<flight::types::SpritesheetPlayer> player,
    std::optional<flight::StructuralRef<flight::RowReadonly<flight::RowOf<
        flight::Ref<flight::types::SpritesheetAnimation>>>>> animation,
    std::optional<bool> restart = std::nullopt) {
  restart = restart.value_or(true);
  if ((!restart.value() && (animation == player->animation))) {
    return;
  }
```

Clang rejects the comparison as invalid operands between the two optional carriers. The header has no
`PARTIAL` banner or `NOT GENERATED` line. In the package-scoped compile, this is the independent failure
of `flight/spritesheet/spritesheet_player.hpp`; three other failing headers stop first in the separately
partial `@flighthq/textureatlas` dependency.

A second measured site at the same pins: `@flighthq/render` `enableColorAdjustments.ts`, in
`resolveInheritedColorScaleBias` (`flight/render/enable_color_adjustments.hpp:94`). `previous` is the
nominal `optional<Ref<ColorScaleBias>>` parameter, while `parent` and `local` are readonly structural
optionals, so the emitted `previous != parent.value()` and `previous != local.value()` compare the two
carriers directly. The same header carries three further families (a nominal-owner return recovery, a
readonly-to-writable projection at `concat_color_scale_bias`, and omitted matrix helpers), so this
comparison is not its only blocker.

## Required lowering

Strict equality must compare the retained owners, without copying or recovering mutable authority:

```cpp
const bool same =
    !animation.has_value() && !player->animation.has_value() ||
    animation.has_value() && player->animation.has_value() &&
    animation.value().owner_identity() == player->animation.value();
```

The spelling of the identity accessor is illustrative; the runtime already uses retained owner identity
for structural-reference equality and weak keys. Null equals null, null differs from a present owner, and
two present values compare equal only when they retain the same object. A shape-compatible sibling must
not compare equal, and no materialized nominal copy is valid.

## Distinguish from existing owner requests

[`readonly-structural-owner-to-nominal-union.md`](readonly-structural-owner-to-nominal-union.md) asks the
emitter to select and construct a nominal union arm. This request stores nothing and recovers no mutable
view; it only needs the identity that both operands already retain. The source-patched local proves the
same point: when both operands are contextualized as readonly structural views, their existing equality
operation compiles without changing either owner.

## Carrying workaround

`source-patches/spritesheet-player-readonly-identity-comparison.patch` binds `player.animation` to
`Readonly<SpritesheetAnimation> | null` inside the existing `restart === false` branch, then performs the
same strict comparison. The read count and short-circuit order are unchanged, and the local retains the
identical object or null. The workaround expires when mixed readonly-structural/nominal equality lowers
through retained identity, or Flight adopts the explicit local.
