# A concretely inferred type argument leaks its parameter name through an intersection-alias parameter

A non-generic function calls a generic one whose parameter is the intersection alias
`NodeOf<Traits> = Node<Traits> & Traits`. The checker infers a **concrete** `Traits` from the argument.
The emitter still omits the template argument, and it spells the argument's structural conversion with
the callee's **unsubstituted** parameter name: `structural_ref_cast<NodeOf<Traits>>(...)`. `Traits` is
undeclared at the call site, so the header does not compile.

This is a sibling of [node-any-generic-traits-leak](node-any-generic-traits-leak.md), not a duplicate.
That request pins its trigger to an **inferred `any`** with a readonly conversion, and its controls record
a concretely inferred argument as substituted. The reduction below has no `any` and no readonly
parameter, and still leaks. The two plausibly share one substitution path in the emitter, so a fix for
either should be checked against both reductions.

Measured at flight-compiler `2b687ea`, with this repository's compiler source patches applied, as built
by `npm run pkg:generate`.

## Minimal TypeScript (measured)

```ts
// nodes.ts
export interface Node<Traits extends object = object> { kind: string; traits: Traits }
export type NodeOf<Traits extends object> = Node<Traits> & Traits;
export interface Node2DTraits { x: number }
export type Node2D = Node<Node2DTraits> & Node2DTraits;
export interface Sprite extends Node2D { label: string }
export function touchOf<Traits extends object>(target: NodeOf<Traits>): void { target.kind = 'touched'; }
export function touchPlain<Traits extends object>(target: Node<Traits>): void { target.kind = 'touched'; }

// consumer.ts
import type { Node2D, Node2DTraits, Sprite } from './nodes.ts';
import { touchOf, touchPlain } from './nodes.ts';
export function inferredIntersectionSprite(sprite: Sprite): void { touchOf(sprite); }
export function inferredIntersectionNode2D(node: Node2D): void { touchOf(node); }
export function inferredPlain(sprite: Sprite): void { touchPlain(sprite); }
export function explicitIntersection(sprite: Sprite): void { touchOf<Node2DTraits>(sprite); }
```

The TypeScript checker resolves every call to `Traits = Node2DTraits`, the same as the explicit form.

## Pinned boundary (emitted `consumer.hpp`)

| call | result |
| --- | --- |
| `touchOf(sprite)`: inferred concrete `Traits`, `NodeOf` parameter, `Sprite` argument | `touch_of(structural_ref_cast<NodeOf<Traits>>(...))`: leaks, no template argument |
| `touchOf(node)`: inferred concrete `Traits`, `NodeOf` parameter, `Node2D` argument | leaks identically |
| `touchPlain(sprite)`: inferred, plain `Node<Traits>` parameter | `touch_plain(sprite)`: no conversion, nothing spelled |
| `touchOf<Node2DTraits>(sprite)`: explicit | `touch_of<Ref<Node2DTraits>>(structural_ref_cast<NodeOf<Ref<Node2DTraits>>>(...))`: correct |

The required shape is the explicit row: substitute the inferred argument into both the template
argument and the conversion's destination type.

## Affected Flight modules

`@flighthq/movieclip` `spritesheetTimelineSource.ts` at flight `a627849`: in the non-generic
`constructFrame` closure, `addNodeChild(target, bitmap)` and `invalidateNodeLocalTransform(bitmap)`
(both `NodeOf<Traits>` parameters, checker-inferred `Traits = Node2DTraits`) emit
`structural_ref_cast<flight::types::NodeOf<Traits>>` at `spritesheet_timeline_source.hpp:123` and `:146`,
and the header fails with `use of undeclared identifier 'Traits'`.

The same call-site spelling appears in non-generic functions of other emitted headers: `@flighthq/shape`
`shape_commands.hpp`, `@flighthq/scene3d` `scene_animation.hpp`, `clone_node3_dsubtree.hpp` and
`scene_node_appearance.hpp`, `@flighthq/scene2d-formats` `lottie_solid_layer.hpp` and
`lottie_image_layer.hpp`, and `@flighthq/physics3d` `node_sync.hpp`. Their trigger (concrete or `any`)
was not classified here.

## Carrying workaround

None. `spritesheet_timeline_source.hpp` stays failing and `@flighthq/movieclip` stays PARTIAL. An explicit
type argument at the two movieclip call sites is the equivalent source patch, if one is wanted later.
