# A typed readonly callback needs to retain its derived owner

A kind-specific bounds callback is stored through a base runtime hook and later receives the same
kind-specific node. The callback parameter can state that exact readonly owner, but erasing it to the
base row leaves no sound way to recover the derived node or runtime inside the callback.

## Minimal TypeScript

```ts
declare const RuntimeKey: unique symbol;

interface NodeRuntime { revision: number }
interface SpriteRuntime extends NodeRuntime { textureVersion: number }
interface Node { [RuntimeKey]: NodeRuntime | undefined }
interface Sprite extends Node {
  [RuntimeKey]: SpriteRuntime | undefined;
  textureVersion: number;
}

interface SpriteRuntimeRunner {
  isCurrent(source: Readonly<Sprite>): boolean;
}

const spriteRuntimeRunner: SpriteRuntimeRunner = {
  isCurrent(source: Readonly<Sprite>): boolean {
    return source[RuntimeKey]!.textureVersion === source.textureVersion;
  },
};
```

The runner is installed only on `SpriteRuntime` objects. The generic bounds machinery later invokes
that stored callback with the node owning the runtime. Lowering must preserve the runner's declared
`Readonly<Sprite>` parameter and its exact `SpriteRuntime` slot across that storage boundary.

## Refused erased form

Flight's original callback instead receives `Readonly<BoundsNode<Node2DTraits>>`, asserts it to
`Readonly<Sprite>`, and obtains `Readonly<Node2DRuntime>` before asserting that value to writable
`SpriteRuntime`. At flight-compiler `2b687ea` and Flight `a627849`, `sprite.ts` refuses with the
following two-part diagnosis:

```text
a structural assertion from the readonly row Readonly<Node2DRuntime> to the writable row
SpriteRuntime claims mutation ... Spell the assertion as Readonly<SpriteRuntime> to remove the
writable-capability claim, but that alone cannot recover the derived owner: Readonly<Node2DRuntime>
does not declare localBoundsTexture and localBoundsTextureVersion
```

The node assertion fails for the same reason: the flattened `BoundsNode<Node2DTraits>` row does not
retain the `Sprite` owner or its `data` refinement. Removing only the writable claim is therefore a
necessary negative control, not a complete fix.

## Required C++ property

The generated callback carrier must keep one `flight::Ref<Sprite>` and read that same object's
`EntityRuntimeKey` slot as `flight::Ref<SpriteRuntime>`. It must not use a native pointer cast, copy a
structural row, allocate a replacement node/runtime, or recover identity from a side table. If a
generic base callback ABI cannot carry the exact owner, it needs a checked dispatch contract that
validates the owner before entering the typed runner.

This is deliberately narrower than allowing arbitrary readonly base-to-derived assertions. Without
the named runner or an equivalent checked dispatch contract, the source provides no proof that a
base node is a Sprite.

## Flight reach and carrying workaround

`packages/scene2d/src/sprite.ts` uses the callback to cache a Sprite texture's identity and version in
the Sprite's own runtime. The source patch
[`scene2d-sprite-exact-runtime-runner.patch`](../../source-patches/scene2d-sprite-exact-runtime-runner.patch)
declares the existing `EntityRuntimeKey` slot as `SpriteRuntime`, passes the Sprite through a named
typed runner, and reads that exact slot directly. Type declarations erase in JavaScript, so the node,
runtime, callback objects, cache writes, and identities remain unchanged.

The workaround expires when Flight carries the exact slot and runner upstream, or when the compiler
can preserve and validate the same readonly derived owner through the generic callback boundary.
