# Type-argument deduction must follow interface heritage

`interface Node2D extends Node<Node2DTraits>` makes every `Node2D`, and every `Sprite extends Node2D`, a
`Node<Node2DTraits>`. A generic `addNodeChild<Traits>(target: Node<Traits>, child: Node<Traits>)` called
with a `Node2D` and a `Sprite` therefore has `Traits = Node2DTraits`, and TypeScript infers exactly that.
The emitter's deduction compares the argument's declared owner with the parameter's by name. `Node2D` is
not `Node`, so it stops without walking `extends`, and leaves `Traits` undetermined. Emission then writes
the conversion target with the parameter's own name, which is undeclared at the call site.

## Minimal TypeScript

```ts
export interface Node<Traits extends object = object> { kind: string; traits: Traits }
export interface Node2DTraits { x: number }
export interface Node2D extends Node<Node2DTraits> { rotation: number }
export interface Sprite extends Node2D { frame: number }
export type NodeOf<Traits extends object> = Node<Traits> & Traits;

export function addNodeOf<Traits extends object>(target: NodeOf<Traits>, child: NodeOf<Traits>): void {
  target.kind = child.kind;
}

export function f(t: Node2D, s: Sprite): void {
  addNodeOf(t, s);
}
```

At flight-compiler `2b687ea` with all 38 of this repository's compiler source patches applied, `f` emits:

```cpp
add_node_of(flight::structural_ref_cast<NodeOf<Traits>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Node2D>>>>(t)),
            flight::structural_ref_cast<NodeOf<Traits>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Sprite>>>>(s)));
// error: use of undeclared identifier 'Traits'
```

## Required C++ shape

The substitution an explicit type argument already produces:

```cpp
add_node_of<flight::Ref<Node2DTraits>>(
    flight::structural_ref_cast<NodeOf<flight::Ref<Node2DTraits>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Node2D>>>>(t)),
    flight::structural_ref_cast<NodeOf<flight::Ref<Node2DTraits>>>(flight::StructuralRef<flight::RowWritable<flight::RowOf<flight::Ref<Sprite>>>>(s)));
```

Deduction should match `Node<Traits>` against an argument owner by walking that owner's declared
`extends` clauses to the instantiation of `Node`, and take `Node2DTraits` from there. It should not
conflate unrelated owners that merely share members; see the residual boundary below.

## Pinned boundary

Same scratch compiler, same fixture:

| arguments | parameter | call | result |
| --- | --- | --- | --- |
| `Node2D`, `Sprite` | `NodeOf<Traits>` | inferred | **leaks `NodeOf<Traits>`** |
| `Node2D`, `Sprite` | `NodeOf<Traits>` | `<Node2DTraits>` | substituted |
| `NodeOf<Node2DTraits>`, `Sprite` | `NodeOf<Traits>` | inferred | substituted: one argument carrying `Node2DTraits` directly is enough |
| `Node2D`, `Sprite` | `Readonly<Node<Traits>>` | inferred | **leaks `Node<Traits>`** |
| `Node2D`, `Sprite` | `Readonly<Node<Traits>>` | `<Node2DTraits>` | substituted, compiles |
| `Node<Node2DTraits>`, `Node<Node2DTraits>` | `Readonly<Node<Traits>>` | inferred | substituted |

The leak needs every argument to reach `Traits` only through heritage. The readonly and writable
parameter forms leak alike, so it is not the `Readonly` wrapper that
[compiler-readonly-view-argument-substitution](../../source-patches/compiler-readonly-view-argument-substitution.patch)
already looks through.

Separately, a writable *nominal* parameter (`target: Node<Traits>` lowered as `Ref<Node<Traits>>`) rejects a
`Node2D` argument even with an explicit type argument, because no conversion is emitted at all. That is the
writable-parameter defect recorded in
[writable-structural-out-of-a-wider-owner](writable-structural-out-of-a-wider-owner.md), not this one.

## Related leaks

This is one of three causes of an undetermined type parameter reaching emission. Each needs its own
deduction fix:
- an `any` argument: [node-any-generic-traits-leak](node-any-generic-traits-leak.md), which tabulates all
  three against shared controls;
- a `Readonly<T>` wrapper (fixed by the carried patch), and the residual boundary for an unrelated owner,
  which is the emission step all three share: [compiler-generic-signal-row-projection](compiler-generic-signal-row-projection.md).

Refusing an undetermined parameter at emission (that residual change) would turn this leak into a refusal.
Only heritage-aware deduction would make it compile.

## Affected Flight modules

`@flighthq/movieclip` `spritesheetTimelineSource.ts` at flight `a627849`. Inside the `constructFrame`
callback, where `target` is a `Node2D` and `bitmap` a `Sprite`:
- `addNodeChild(target, bitmap)` (source line 71) emits
  `flight::structural_ref_cast<flight::types::NodeOf<Traits>>(...)` at
  `generated/include/flight/movieclip/spritesheet_timeline_source.hpp:123`;
- `invalidateNodeLocalTransform(bitmap)` (source line 85) does the same at `:146`.

clang reports `use of undeclared identifier 'Traits'` at `123:92` and `146:99`. At that measurement, the same header also
failed independently on the then-unsupplied `scene2d` `create_sprite` (`:122`) and on `textureatlas` conversions
and missing declarations (`:130`, `:134`, `:143`). This request does not cover those, and fixing it alone
does not make the header compile. movieclip is partial at `a627849`, 4/5 modules emitted and 0/5 compiling
in tier.

## Workaround carried

None. The `movieclip-timeline-node-construction` override covers `movie_clip.hpp`, not this header. A
source patch adding `<Node2DTraits>` at the two calls would be equivalent, since it is exactly what
TypeScript infers. It is not carried, because the header has other blockers first.


## G2: the native Sprite absence bridge removes the earlier arity error

At Flight `a627849` / compiler `806d257`, the source still calls `createSprite()` with no argument;
the emitter spells its absent optional argument `create_sprite(std::nullopt)`. The native Sprite
completion previously exposed only `create_sprite()`, causing G1 cluster `other-43523429` (four
Movieclip headers) to fail first at `spritesheet_timeline_source.hpp:122` with expected-zero/have-one.
This is an absent-options binding mismatch, not a callback whose received arguments should be ignored.

The native completion now accepts exactly `std::nullopt_t`, forwarding to its existing no-options
factory. It does not accept arbitrary arguments or supplied partial options. Its override remains
`partial` and explicitly records that present options and custom factory/runtime paths are unavailable.
The project's native owner target builds, and `flight_cpp.generated_scene2d_gl_owners` passes 1/1 with
checks for the new entry's source defaults/texture absence, fresh data/runtime owners, live graph cells,
and compile-time rejection of supplied option rows and integers. Override integrity passes for all
226 entries; no compiler source patch is carried for this API bridge.

The changed header check reaches the existing `Traits` leak at `addNodeChild`, line 123, after the
Sprite call resolves. This does not make Movieclip compile or remove the owner/conversion boundaries
already recorded above. A global `Traits` alias, a copied node, or an arbitrary callable adapter would
supply a meaning the source did not declare. The four Movieclip rows remain scoped to their original
work item; the separate 29-header TextLabel/E0a reservation is not changed by this native API fix.


The exact four-header changed-input gate completed at one worker with **0/4 pass, 4 fail,
0 unattempted**. Every first error is `use of undeclared identifier 'Traits'` at
`flight/movieclip/spritesheet_timeline_source.hpp:123`; the old factory arity error is absent.
The rows for `_internal_index.hpp`, `contract.hpp`, `enable_movie_clip_guards.hpp`, and
`spritesheet_timeline_source.hpp` (all under `flight/movieclip/`) now carry that measured diagnostic,
location, and this request link. They retain work-item ID `other-43523429` while its cause/route is
updated; the separate reserved TextLabel/E0a `Traits` cluster is untouched. No header was promoted,
no package-wide or whole-SDK check was run, and no Movieclip playback behavior is claimed from the
native Sprite constructor regression test.
