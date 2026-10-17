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

## G2 GL bitmap text needs its own runtime owner

At compiler `806d257` / Flight `a627849`, `scene2d-gl/glBitmapText.ts:36` reads
`(getNode2DRuntime(source) as BitmapTextRuntime).pages` inside refused `submitGlBitmapText`.
The refusal is `cpp-structural-assertion-writable-capability-unproven`: the readonly base runtime
has no declared `pages`, `glyphLayoutVersion`, `lineCount` or `truncated` cells. Changing only the
assertion to readonly removes the false write claim but does not recover the full owner. The current
native getter's `NodeRuntime<Ref<Node2DTraits>>` is also distinct from flattened `BitmapTextRuntime`.

The same owner requirement above applies: initialization must establish the complete typed runtime
in the BitmapText's slot, and the typed renderer must read that same owner and its page arrays. The
source factory currently asserts `createNode2DRuntime(defaultMethods)` to `BitmapTextRuntime` before
writing its extra fields, so the storage/construction boundary must be declared as well. Keep the
exact Node2D traits and readonly read authority; no replacement runtime, page copy or side storage.

Its GL callback also takes `GlRenderState`, whereas `SpriteRenderer.submit` declares `RenderState`.
That later domain difference is visible from the types, not a measured follow-on diagnostic. The
callback needs the exact backend owner or checked dispatch described above, preserving the public
carrier instead of asserting every base state to GL. Real page textures, glyph transforms/UV rotation,
alpha, material/color records, post-flush batch offsets and valid-glyph draw counts must survive.

The original two headers still fail on the retained callback's missing name (0/2, Clang22.1.8,
one worker). A declaration-only probe fails earlier at the separate
[create-data owner conversion](interface-heritage-widening.md#g2-gl-bitmap-text-renderer-create-data-callback).
No renderer implementation or declaration repair is carried; see the
[retained aggregate callback boundary](retained-declaration-calls-omitted-callee.md#g2-gl-bitmap-text-retains-a-refused-callback-in-an-aggregate).

## G2 raster shape native and generic renderer callbacks

The current `gl_shape_data.hpp` override preserves its actual `GlShapeRendererData` owner through
`ErasedRef`; its create callback returns `optional<ErasedRef>` from `(Ref<GlRenderState>, Node2D)`.
The emitted `Scene2DRenderer.create_data` instead requires `optional<Ref<RendererData>>` from
`(Ref<RenderState>, Node2D)`. One scratch raster header with only its missing draw declaration
fails compiling this exact callback conversion before reaching a link step (Clang22.1.8).

The same typed-owner/checked-dispatch requirement above applies to backend state and renderer data.
A callback must keep the GL state, its exact renderer-data allocation and the complete Node2D domain;
casting function pointers, copying base data or reinterpreting every generic state as GL proves none
of these. Destroy/submit carrier differences are visible by type inspection but were not reached by
the first-error probe. The existing native shape-data partial carry supplies mesh lifecycle only,
and throws for raster resource teardown; full raster acquisition/upload/replay/lifecycle remains debt.

See the [valid but insufficient declaration witness](retained-declaration-calls-omitted-callee.md#g2-raster-shape-declaration-witness-is-valid-but-callback-carriers-fail)
and [separate body presence refusal](nullable-optional-presence.md#g2-raster-shape-null-presence-refusal).
No callback adapter, fake draw or rendering implementation was carried.

## G2 Scale9 sprite native Sprite callbacks are not generic renderer callbacks

At Flight `a627849` / compiler `806d257`, `gl_scale9_sprite.hpp` assigns the current native Sprite
create/dirty helpers to a generated `Scene2DRenderer`. Native create accepts
`Ref<GlRenderState>, Node2D` and returns `optional<ErasedRef>`; the slot instead takes generic
`Ref<RenderState>, Node2D` and returns `optional<Ref<RendererData>>`. One scratch actual unused header
with only the missing draw declaration fails this conversion at `.create_data` (candidate170).
The original production header remains0/1 at missing draw168; no link boundary was reached.

By type inspection, the dirty slot additionally takes `NodeAny`/optional nominal RendererData while
the native helper takes `Node2D`/optional ErasedRef. The refused GL draw signature also differs from
the generic state's submit signature. Preserve the original backend state, renderer scratch allocation
and registered node owner through checked dispatch; casting callbacks or fabricating base data does
not reconcile these carriers.

There is also a concrete-owner boundary: both native Sprite helpers call `native_sprite`, which
uses checked `shared_exact_object<Sprite>()`. The Scale9 proxy owns `Scale9Sprite`, with independent
flattened data/grid/runtime, not that exact native Sprite owner. Its generated texture cell is
`optional<Ref<Texture>>`, while native Sprite data holds `optional<Texture>` over the four-alternative
texture family. These inspected storage differences are not another tested compile result, and
converting the source to a Sprite copy or choosing one texture arm would narrow the declared domain.
Keep the complete Scale9 owner, original texture/material references and revisions at its typed API.

The [nullable material widening](compiler-interface-heritage-owner-widening.md#g2-gl-scale9-sprite-material-owner)
first refuses the body; [retained aggregate evidence](retained-declaration-calls-omitted-callee.md#g2-gl-scale9-sprite-retains-a-refused-aggregate-callback)
records why no declaration-only supply is carried. No adapter, owner copy, fake nine-slice packets
or native Scale9 drawing behavior was added or claimed.

## G2 WGPU Sprite retains GL-specific native Sprite helpers

At Flight `a627849` / compiler `806d257`, `wgpu_sprite.hpp` assigns the native Sprite create/dirty
helpers to a generated `Scene2DRenderer`. Native create accepts `(Ref<GlRenderState>, Node2D)` and
returns `optional<ErasedRef>`; the slot accepts `(Ref<RenderState>, Node2D)` and returns
`optional<Ref<RendererData>>`. An actual unused header with only the exact missing WGPU draw
declaration fails this conversion first at `.create_data` (candidate136). Stock original1 stays
0/1 at missing draw134, Clang22.1.8 jobs1; no link boundary is reached.

By type inspection, dirty additionally differs at NodeAny versus Node2D and optional nominal
RendererData versus ErasedRef. The exact draw requires WgpuRenderState, whereas generic submit
accepts RenderState. The current `NativeGlScene2DRendererBinding` is explicitly GL-typed and cannot
stand in for the original WGPU state/runtime. Provide an owner-preserving backend callback contract
and checked dispatch retaining the same proxy/source/data owners; do not cast function pointers,
copy generic or GL-shaped states, fabricate nominal RendererData or narrow to one material/shader.

The native helper recovers the exact Sprite and keeps its original optional texture family and
version in SpriteIdentityRendererData. Dirty compares then updates that identity/version, while a
missing renderer-data entry returns false. The source stamp is per-state/proxy, so pipelines must
keep independent stamps. Preserve this owner/lifecycle behavior through any callback adaptation.
These inspected dirty/submit obligations are not further measured compile or behavior results.

The [nullable material-owner refusal](compiler-interface-heritage-owner-widening.md#g2-wgpu-sprite-material-owner)
and [retained aggregate qualification](retained-declaration-calls-omitted-callee.md#g2-wgpu-sprite-retains-its-refused-draw-callback)
remain separate. No callback adapter, shared Sprite/GL change, draw body or GPU proof is supplied.

## G2 Interaction text hit callbacks need the original text and 2D owners

At Flight `a627849` / compiler `806d257`, `interaction/registerTextHitTest.ts:26`
refuses `resolveTextCharIndex` at `source as TextLabel` with
`cpp-reference-assertion-without-heritage`. Its callback input is the full `NodeAny`,
emitted as `shared_ptr<Node<Any>>`, while `TextLabel` is an independent flattened
reference owner with its original `TextLabelData` and typed `TextLabelRuntime` slot.
The two retained registrations install this same callback for `TextLabelKind` and
`RichTextKind`; source `RichText` extends `TextLabel` with its own data/runtime refinement.
Neither a kind string nor selection of the precise-provider registry supplies that
owner relation. Do not restrict the public callback to a concrete TextLabel or exclude RichText.

The actual source first tests the original graph-local bounds and returns -1 outside.
Only a bounds hit reaches `getTextLayout` on that same node; null layout returns0.
A present layout then triggers the world-matrix read, inverse transform into the original
shared scratch point and `computeRichTextCharIndexAtPoint` over that same layout.
Preserve this order, both early returns, graph/transform/bounds cells, scratch writes,
exceptions and precise character-index result. The layout getter can validate or rebuild
the original runtime cache, invoke the optional text guard and registered measure provider,
and update its content revision/text stamps. A fixed0/-1 result or copied text/runtime/layout
would bypass those effects. The character query retains nearest-line and glyph-advance
selection, including empty-layout0 and the line-end result below the chosen line.

Current native helpers already retain an exact TextLabel when it enters a `Node2D`
structural view, and native text layout reads that TextLabel's own typed runtime. That is
not recovery from the precise callback's existing nominal `NodeAny` carrier, nor does
its closed Shape/Sprite/TextLabel helper establish a RichText owner contract. Shared text,
GL text, Node and bounds code are left untouched. A real implementation needs an
owner-preserving checked dispatch/capability boundary through the graph and callback;
no base pointer cast, replacement graph node, side table or domain narrowing is supplied.

The earlier [graph-local-bounds request](assertion-to-undeclared-row-member.md#g2-interaction-graph-bounds-need-the-original-nodes-2d-capability)
also applies to the first operation, whose generic body remains refused. These are source/type
obligations, not additional measured diagnostics after a supplied text resolver. The bounded
[retained callback declaration](retained-declaration-calls-omitted-callee.md#g2-interaction-text-hit-test-retains-its-refused-callback)
addresses name visibility only and leaves the resolver body unavailable.
