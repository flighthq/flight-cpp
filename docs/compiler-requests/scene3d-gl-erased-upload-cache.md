# The Scene3D GL upload cache must state its erasure at the storage boundary

The pending `scene3d-gl-erased-upload-cache` patch is the source half of
[a checked typed view of an erased WeakMap](compiler-erased-ref-weak-map-view.md). The compiler half is applied
and emits `flight::checked_weak_map_view` for an **immutable local** typed view whose key and value are proven
Flight reference carriers. At Flight `a627849` the source still recovers the typed map the other way round,
through a mutable local asserted out of the erased context slot and asserted back into it:

```ts
// packages/scene3d-gl/src/glScene3DRuntime.ts:154 at a627849
let uploadCache = stateRuntime.context.sceneMeshUploadCache as
  | WeakMap<MeshGeometry, GlMeshUpload>
  | null
  | undefined;
if (uploadCache == null) {
  uploadCache = new WeakMap();
  stateRuntime.context.sceneMeshUploadCache = uploadCache as unknown as WeakMap<object, object>;
}
```

`GlScene3DRuntime.uploadCache` is declared `WeakMap<MeshGeometry, GlMeshUpload>`, while the context slot it is
read from (`GlContextRuntime.sceneMeshUploadCache`) is `WeakMap<object, object> | null`. That shape never
reaches the immutable-local path, so the module still refuses with the terminal message the compiler patch was
written to remove:

```
glScene3DRuntime.ts (emitted line 152): flight-cpp WeakMap<object, object> typed views require the runtime
overload flight::checked_weak_map_view<Key, Value>(flight::WeakMap<flight::Ref<void>, flight::ErasedRef>&)
```

Every module that imports the runtime inherits this refusal: `glMeshUpload`, `glLitProgram`,
`glScene3DTime`, the environment-cube chain, the PBR extension registry and the five `enableGlScene3D*Guards`
modules.

## Exact current-pin replacement

The rewrite is unchanged in substance. It has drifted only in context: `.ts` import specifiers, and the
`GlMeshUpload` type import that the active `scene3d-gl-erased-program-cache` patch (later in the stack)
uses as context. That import stays, so the stack composes; Flight's tsconfig does not set `noUnused*`.

| current source | exact replacement |
| --- | --- |
| `packages/types/src/GlScene3DRuntime.ts` | `uploadCache: WeakMap<object, object>`; drop the now-unused `MeshGeometry` import |
| `packages/scene3d-gl/src/glScene3DRuntime.ts` | read the slot in its own `WeakMap<object, object> \| null` type, create `new WeakMap<object, object>()` and store it with no assertion; drop the `MeshGeometry` import |
| `packages/scene3d-gl/src/glMeshUpload.ts:142` | `const cache = getGlScene3DRuntime(state).uploadCache as WeakMap<MeshGeometry, GlMeshUpload>;`, the one immutable typed view at the consumer |

Re-derived against the stack at its manifest position (after `pbr-extension-concrete-value-arms`, which renames
the neighbouring import), measured in a scratch worktree and not committed:

```diff
--- a/packages/scene3d-gl/src/glMeshUpload.ts
+++ b/packages/scene3d-gl/src/glMeshUpload.ts
@@ -139,7 +139,7 @@ export function ensureGlMeshUpload(
   const gl = state.gl;
-  const cache = getGlScene3DRuntime(state).uploadCache;
+  const cache = getGlScene3DRuntime(state).uploadCache as WeakMap<MeshGeometry, GlMeshUpload>;
   let upload = cache.get(geometry as MeshGeometry);
--- a/packages/scene3d-gl/src/glScene3DRuntime.ts
+++ b/packages/scene3d-gl/src/glScene3DRuntime.ts
@@ -9,7 +9,6 @@ import type {
   GlSkinPaletteTexture,
-  MeshGeometry,
 } from '@flighthq/types/contract';
@@ -151,13 +150,10 @@ export function getGlScene3DRuntime(state: GlRenderState): GlScene3DRuntime {
-    let uploadCache = stateRuntime.context.sceneMeshUploadCache as
-      | WeakMap<MeshGeometry, GlMeshUpload>
-      | null
-      | undefined;
+    let uploadCache = stateRuntime.context.sceneMeshUploadCache;
     if (uploadCache == null) {
-      uploadCache = new WeakMap();
-      stateRuntime.context.sceneMeshUploadCache = uploadCache as unknown as WeakMap<object, object>;
+      uploadCache = new WeakMap<object, object>();
+      stateRuntime.context.sceneMeshUploadCache = uploadCache;
     }
--- a/packages/types/src/GlScene3DRuntime.ts
+++ b/packages/types/src/GlScene3DRuntime.ts
@@ -6,7 +6,6 @@
-import type { MeshGeometry } from './MeshGeometry.ts';
@@ -187,7 +186,7 @@ export interface GlScene3DRuntime {
-  uploadCache: WeakMap<MeshGeometry, GlMeshUpload>;
+  uploadCache: WeakMap<object, object>;
```

## Equivalence

Type assertions and generic arguments erase in JavaScript, so the emitted JavaScript is unchanged:
- **Same map:** one `WeakMap` is still created lazily, stored in the same context-tier slot, and retained in
  the same scene runtime. A derived state still adopts the primary's map instead of creating its own.
- **Same accesses:** `ensureGlMeshUpload` queries and updates that map with the same geometry key and upload
  value objects, in the same order.
- **What it doesn't change:** no key, value, lifetime, call order or identity changes. No copy or side table is
  introduced.

On the native side, the consumer's view shares storage with the erased map. Each `get` validates the recorded
value owner before returning it, so a mismatched owner throws rather than being reinterpreted. That is the
checked recovery the compiler half already pins in its own regressions.

## Measured at Flight `a627849` / compiler `2b687ea` with the live stack

These were scoped runs:

```sh
node scripts/sdkGeneration.mjs --package=@flighthq/scene3d-gl --emit-path-prefix=packages/scene3d-gl/
```

Each was run in a scratch worktree whose `source-patches/` carried the re-derived patch with `baseCommit` set to the pin. The pinned
checkout was left clean after every run.

- **The named refusal is cleared.** `checked_weak_map_view` no longer appears for `glScene3DRuntime.ts`.
- **The module then stops at the next, unrelated refusal**, emitted line 19 (pin line 21):
  `const cleanups = (scene.resourceCleanups ??= []);`. A `??=` result is present, but the compiler does
  not treat it as proven present. That is a separate compiler request:
  [the result of `??=` is present](nullish-assignment-result-presence.md).
- **A scratch probe only (not proposed here)** rewrote that one line into the explicit-guard form already used
  by the active `canvas-texture-resolver-present-registry` patch. With it, `glScene3DRuntime.ts`,
  `glMeshUpload.ts`, `glScene3DTime.ts`, `glLitProgram.ts` and the environment-cube modules all emit, and
  scene3d-gl refusal rows fall from 60 to 47. So this patch plus a `??=` fix together unblock the runtime root.

## Stack and package coupling

- **Types regeneration:** the rewrite edits `packages/types`, so re-landing it regenerates `@flighthq/types`
  as well as scene3d-gl.
- **Types headers compile:** a run that also emitted `packages/types` produced four re-emitted headers, and
  all four compile against the runtime and override trees: `types/gl_scene3_druntime.hpp`,
  `types/pbr_extension.hpp`, `types/extended_pbr_material.hpp` and `types/gl_pbr_extension_registration.hpp`.
  `upload_cache` is emitted as `flight::WeakMap<flight::Ref<void>, flight::ErasedRef>`.
- **Struct table:** re-rendered from the merged tree, the structural member table gains exactly two keys
  (`hasTransmissionSceneColor`, `isTextureReady`) and drops none.
- **Composition:** the patch composes with every later active patch only while the `GlMeshUpload` import line
  above survives, because `scene3d-gl-erased-program-cache` uses it as context.

## Focused tests

1. **Upstream behavior suites (pinned semantics this rewrite must preserve):**
   - `glMeshUpload.test.ts`: "caches by geometry and reuses the upload when the version is unchanged" (`:112`),
     which asserts `getGlScene3DRuntime(state).uploadCache.get(geometry)` is the first upload; and
     "re-uploads when the geometry version is bumped, reusing the GL objects" (`:124`).
   - `glScene3DRuntime.test.ts`: "surfaces its upload cache without replacing persistent material dispatch
     policy" (`:260`), which asserts `stateRuntime.context.sceneMeshUploadCache` is `scene.uploadCache`.
   - `renderGlScene3D.test.ts`: "uploads one shared geometry once across a primary and derived state on the
     same context" (`:59`).
2. **Focused generation.** Generate scene3d-gl with the patch. Assert the `checked_weak_map_view` refusal is
   absent and that `glMeshUpload.hpp` acquires the view through `flight::checked_weak_map_view`.
3. **Native compile.** This is not yet possible at the live base. With the scratch types headers and the merged
   member table, every scene3d-gl header that reaches `flight/render/render_target.hpp` fails there:
   `no matching function for call to 'get_node_local_matrix'`. The committed `render_target.hpp` fails the same
   way on its own, so this is a pre-existing render-row break, not this patch. Native GL behavior cannot run
   under the offscreen driver (`tests/host_sdl_test.cpp` notes a GL context cannot be made there), so the
   upstream suites above remain the behavioral evidence.

## Status

The patch is still pending on the GL lane. This request changes neither source patches nor generated inventory.
It expires with the compiler half: when checked erased views are recovered through mutable locals and
nullable slots automatically, or when Flight adopts the boundary and local-view annotations upstream.
