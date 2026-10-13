# Resolving an imported type's declaring owner must not re-walk every star re-export

This is a performance defect, not a hang. Emission finishes and its output is correct, but one named-type
lookup costs a walk of the whole `@flighthq/types/contract` re-export graph. Emission performs that lookup
whenever it resolves an imported type alias, so a package whose types are imported through that barrel can spend tens
of minutes in one module.

## Measured

Measured with flight-compiler `2b687ea` and flight `a627849`, with all declared source patches applied,
using `--progress`:

- **The run:** `scripts/sdkGeneration.mjs` for the 9-package registry migration group (1889 modules in
  the closure).
  - Lowering finished normally.
  - Emission of one 78-line module, `@flighthq/scene2d-gl/packages/scene2d-gl/src/scene2DGlPipeline.ts`,
    did not finish for more than 10 minutes. The trace showed `module-emission started … (649/1889)` and no
    further event for that time. A 5-minute watch fired, and the module was still in progress when it was
    checked again; the process stayed at 100% CPU the whole time.
  - After the final progress event (`module-emission completed … (1889/1889)`), the process ran another
    85 minutes at 100% CPU before writing any output.
  - The whole run took about 3h40m. A whole-SDK generation is recorded at 27 minutes in
    [generation-state.md](../generation-state.md).
- **The samples:** Four samples were taken by pausing the live process through the Node inspector
  (`SIGUSR1`, then `Debugger.pause`). Every one was inside the same recursion:

  ```
  getCppExportedTypeDeclarationOwnersCpp
  (anon)                                   <- the `forwarded` flatMap over module.exports
  (anon)
  getCppExportedTypeDeclarationOwnersCpp
  importedOwner
  getCppNamedTypeDeclarationOwnerCpp
  resolveCppTypeAliasTarget
  emitType | getIrUnionTypeCpp | hasFlightReferenceRepresentationCpp | isCppImportedUnionAliasAbiCarrierTypeCpp
  ```

  The callers differ (type emission, union evidence, reference representation, external member
  binding), but every path reaches the owner lookup through `resolveCppTypeAliasTarget`.

## Cause

`getCppExportedTypeDeclarationOwnersCpp(module, exportedName, context, visited)` answers "which declaration
does this module export under this name". Each call does the following:

- scans all of `module.declarations` and `module.exports` for the name;
- for every `export *` and matching re-export, resolves the specifier and recurses into the target module;
- copies `visited` into a new `Set` at every level.

Nothing is cached, and every caller starts with `new Set()`. `@flighthq/types/contract.ts` is a barrel of
`export *` lines over about 995 modules, so one lookup of a types-owned name visits every one of them. It
then scans each one's declarations and exports, and resolves each specifier again. `scene2DGlPipeline.ts`
names `GlRenderRegistries`, `Kind`, `NodeRenderer` and many kind constants through that barrel. Emitting
it repeats the lookup for each type occurrence it evaluates.

## Minimal TypeScript shape

```ts
// packages/types/src/contract.ts -- a barrel of many star re-exports
export * from './A.ts';
export * from './B.ts';
// … several hundred more …

// packages/consumer/src/x.ts
import type { A, B } from '@flighthq/types/contract';
export const value: Readonly<Omit<A, 'k'>> = Object.freeze({ ...makeA(), b: makeB() });
```

The expression is not the cost. A single-module reduction of the same `Object.freeze({ ...spread })`
into `Readonly<Omit<…>>` emits in under 100 ms (probe run against the pinned compiler with all patches
applied). The probe had no barrel; the claim that the cost scales with the barrel rests on the inspector
samples and the code path above, not on a timed barrel probe.

## Required behavior

The result of an owner lookup depends only on the module graph, which does not change during an emission
session. It is a pure function of (module identity, exported name). Memoizing it per session is enough:

- either key a cache on `getCppModuleIdentityKey(module) + '\0' + exportedName` in the session's planner;
- or build one export table per module the first time it is asked, mapping each exported name to its
  owners after following re-exports, and answer lookups from the table.

Cycle handling stays as it is (a key already on the current path contributes nothing). A result computed
while a cycle cut it short is partial, so cache only results whose walk was never truncated, or compute the
table for a strongly connected group of modules at once.

## Affected work

- Every package generation whose closure includes the GL and canvas renderers. This currently means the
  registry migration group (registry, render, render-gl, render-wgpu, effects-canvas, effects-gl,
  scene2d-canvas, scene2d-gl, scene2d-wgpu, scene3d-gl, shape-formats). A group generation took 3h40m.
- The earlier "render-gl / scene2d-gl hang" entries in generation-state.md are consistent with this
  cost. The 2026-10-08 measurement already found emission advancing slowly rather than looping.

## Workaround carried

None. The output is correct, so there is nothing to repair. Runs are simply slow. A compiler patch would
need to be shown byte-identical on generated output before it could be carried. Until then, long runs use
`--progress`, and a stalled trace is checked with an inspector sample before it is called a hang.
