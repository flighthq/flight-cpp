# Exact native owner recovery from a structural union

A generic structural node union retains the original concrete native object, but an assertion to the
registered renderer's concrete owner is refused because the emitter looks for a nominal variant arm.

## Minimal TypeScript

```ts
interface Node { kind: string }
interface Transform2D { x: number; y: number }
type Node2D = Node & Transform2D;
interface TextLabel extends Node, Transform2D { text: string }
interface Cache { kind: string; cached: boolean }
type Renderable = Readonly<Node2D> | Cache;

export function draw(source: Renderable): string {
  return (source as TextLabel).text;
}
```

## Expected C++

```cpp
const auto row = std::get<flight::StructuralRef</* Node2D row */>>(source);
const auto text = flight::structural_ref_cast<flight::Ref<TextLabel>>(row);
return text->text;
```

The runtime check must require the retained native type to be exactly `TextLabel`; a structurally
compatible sibling must throw rather than succeed.

## Affected modules

Canvas BitmapText, ParticleEmitter2D, QuadBatch, RichText, Scale9Shape, Scale9Sprite, Shape, Sprite,
TextInput, TextLabel, and Tilemap renderers.

## Carrying workaround

[`compiler-structural-union-exact-native-owner.patch`](../../source-patches/compiler-structural-union-exact-native-owner.patch)
selects the unique related structural arm and calls the runtime's checked native-owner recovery. It
expires when upstream lowers these assertions through an equivalent exact check.
