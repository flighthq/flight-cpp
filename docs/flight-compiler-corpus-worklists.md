# Latest Flight C++ check worklists

This is the deduplicated register derived from the compiler's complete Default and Web check reports. It is separate
from SDK generation: `flight-compile check` reports the first blocker for every selected module without applying
flight-cpp's downstream binding manifests, while SDK generation composes those manifests explicitly.

## Reproducible inputs

The reports use Flight `develop` at `cefb2f9914270340afd4cc39e407f1202bcf8fc5` and a fresh build of
flight-compiler `main` at `839d91eb9f9f3b66681443bb53b861dd2a7f2774`. Both source trees were clean. The compiler was
built from a deleted `packages/tool-compiler/dist` with `npm ci` and `npm run build`; each command below ran with `.`
as the Flight checkout root:

```sh
flight-compile check . --target cpp --format json \
  --report out/flight-compiler-cpp-default.json
flight-compile check . --target cpp --environment web --format json \
  --report out/flight-compiler-cpp-web.json
```

| Environment | Packages | Direct findings | Dependency cascades | Emitted modules | Gating | Target-runtime | Compiler-defect | Compiler-restriction |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Default | 146 | 1,452 | 794 | 409 | 238 | 511 | 703 | 238 |
| Web | 155 | 1,899 | 910 | 412 | 435 | 745 | 719 | 435 |

The two raw reports live under gitignored `out/`; they are regeneration inputs, not maintained repository artifacts.
Their revisions, commands, and the downstream comparison revision are in
[`flight-compiler-cpp-provenance.json`](../artifacts/flight-compiler-cpp-provenance.json). The generated
[`flight-compiler-cpp-worklist.json`](../artifacts/flight-compiler-cpp-worklist.json) contains every stable
rule/code group and every required `sourceName[space]`, with direct counts, affected cascades, binding profiles, and
sample reproductions. Regenerate that index with `npm run compiler-reports:summary`; when either ignored report is
absent, the command identifies the missing inputs and skips without changing the committed summary.

An affected cascade names at least one direct finding in a group. Cascade counts overlap and are not additive; a
cascade may name blockers from several groups. A direct module that loses its first blocker advances to emission or
to its next blocker, so "advances" is more precise than claiming that every such module immediately compiles.

## Worklist: implement or declare this in flight-cpp

### P0 — make the maintained binding profiles visible to `check`

All 511 Default runtime findings and all 745 Web runtime findings share one stable rule:
`unsupported-ir/cpp-runtime-external-symbol-binding-incomplete`. They are not 1,256 independent runtime TODOs.

Against flight-cpp `0b135b890378e9299a4d94ec900fcd13dc522384`, the existing manifests completely declare every
required symbol in **497 of 511** Default findings and **692 of 745** Web findings. Those findings touch 289 and 365
dependency cascades respectively. Only 21 Default and 22 Web cascades depend exclusively on those already-declared
runtime findings; the others also name compiler blockers.

The missing integration contract is versioned profile ingestion. A C++ check must either accept repeated
`flight-cpp-external-bindings/1` manifests or resolve a versioned target descriptor that names them. The report must
record the flight-cpp revision and manifest identities/digests; the current reports say the target is `unversioned`,
which prevents a result from proving which downstream ABI it checked. Profile composition must remain explicit:
portable runtime/headless, SDL/GL, SDL/WGPU, and full SDL are different claims.

The impact below is the number of direct runtime findings and cascades that mention at least one API already declared
by each manifest. Rows overlap because one module can require APIs from several profiles.

| Existing profile | Default direct / cascades | Web direct / cascades | Contract owner |
| --- | ---: | ---: | --- |
| `runtime-carriers` | 342 / 242 | 423 / 307 | flight-cpp core runtime |
| `sdl-gl` | 104 / 35 | 201 / 86 | SDL/OpenGL provider |
| `web-types` | 54 / 35 | 159 / 73 | provider-neutral Web value types |
| `sdl-wgpu` | 54 / 17 | 97 / 29 | SDL/WGPU provider boundary |
| `sdl-app` | 9 / 2 | 64 / 40 | SDL application host |
| `sdl-image` | 18 / 19 | 48 / 36 | SDL decoded-image provider |
| `headless` | 8 / 33 | 13 / 46 | portable host services |

Acceptance:

1. Run `check` with a named, versioned flight-cpp profile composition.
2. Assert that every raw runtime requirement whose `declaredByProfiles` list is non-empty is resolved by the chosen
   composition, or is excluded with a named environment/profile reason.
3. Preserve a report target revision and the exact binding identities/digests.
4. Re-run Default and Web, then the downstream SDK generation/header compilation matrix. Do not infer runtime
   implementation work from the raw 511/745 totals after profile application.

### P1 — keep the remaining provider boundaries out of core flight-cpp

After the union of the seven maintained manifests, only 14 Default and 53 Web direct runtime findings retain an
undeclared API. Their ownership is already sharply localized:

| Remaining source area | Default direct / cascades | Web direct / cascades | Required contract |
| --- | ---: | ---: | --- |
| `packages/intl` | 6 / 0 | 6 / 0 | `LocalesArgument` and the collator, date-time, list, number, plural, and relative-time option dictionaries, backed by a selected internationalization provider |
| `packages/textsegment` | 2 / 0 | 2 / 0 | `Intl.Segmenter`, `Intl.Segments`, `Intl.SegmentData`, `SegmenterOptions`, and `LocalesArgument`, backed by Unicode segmentation data |
| `packages/host-web` | 0 / 0 | 35 / 30 | browser DOM, networking, storage, media/audio, MIDI, permissions, sensors, clipboard, WebSocket, and window providers; these are Web host bindings, not core runtime semantics |
| `packages/render-wgpu` | 4 / 0 | 4 / 0 | browser image constructor values, `ImageBitmapRenderingContext`, `WebGLRenderingContext`, plus test-only `Navigator`/property-descriptor evidence |
| `packages/scene2d-dom` | 0 / 0 | 4 / 1 | browser DOM node/text/SVG and image-constructor bindings |
| `packages/image` | 1 / 0 | 1 / 0 | host `location` used for URL resolution |
| `packages/effects-gl` | 1 / 0 | 1 / 0 | dynamic `Function` construction in a test helper; this is not an honest native runtime API |

There is therefore no evidence in these reports for another core flight-cpp implementation tranche. The remaining
portable-looking group is internationalization, which needs an explicit provider decision rather than ASCII or
locale-dependent behavior disguised as `Intl`. The complete exact API list is the set of entries with an empty
`declaredByProfiles` array in the generated worklist JSON.

### P1 — host-adapter choices, if those environments are selected

Do not satisfy the Web findings by adding semantic stubs to core flight-cpp. A selected adapter must provide the
actual behavior:

- Web network: Fetch request/response/header/body types and values, readable-stream result arms, and abort/timer
  integration.
- Web media: Web Audio graph nodes, media streams/session, video sources, MIDI, and permission state.
- DOM/window: node and text identity, events/listener removal identity, resize observation, storage, fullscreen,
  styles, screen/navigation state, SVG nodes, and image/canvas constructors.
- SDL/native: only bind a browser-shaped contract where the SDL adapter implements the same observable behavior;
  otherwise keep it an explicit refusal.

Each provider needs a compiler-emitted acceptance fixture that calls the bound surface and a native behavioral test.
Opaque type aliases alone do not meet the contract.

## Worklist: compiler/profile corrections

1. **Scope emitted paths by package.** One `duplicate-emitted-path` defect accounts for 703 Default and 719 Web
   direct findings. It touches 769/884 cascades, and 447/461 cascades depend exclusively on this defect family.
   `contract.hpp`, `_internal_index.hpp`, and ordinary same-named modules from different packages are being checked
   in one global path namespace. Package output roots/include prefixes must distinguish them while retaining real
   collision detection within one package. This is compiler-owned and must not be worked around by renaming Flight's
   public modules.
2. **Version the target profile in check reports.** The upstream revision is correctly embedded, but compiler and
   target revisions are `unversioned`. A report intended to drive downstream work must name both.
3. **Do not classify `as const` as a runtime type.** `packages/host-web/src/webNet.ts` reports `const[type]` from
   `_netTimeoutReason = { flightNetTimeout: true } as const`. `const` is TypeScript syntax, not an ambient API. Fix
   external-symbol collection and add this module as the regression.
4. **Keep test-only dynamic evaluation out of the runtime queue.** `glShaderTestHelper.ts` uses `new Function` to
   evaluate GLSL-like text under JavaScript tests. Either keep that helper out of production package roots or report
   it as source portability. A `Function[value]` flight-cpp binding would falsely claim native JavaScript evaluation.
5. **Treat structural library types by use.** `Navigator[type]`, `PropertyDescriptor[type]`, and
   `PropertyDescriptorMap[type]` in `wgpuTestHelper.ts` are type-level evidence. Prove/lower their `Pick` and property
   operations where possible before demanding nominal host objects.

## Worklist: change Flight now to accelerate the first pipeline

These are temporary source simplifications, ranked by reach and reversibility. Counts use the Web report, the maximal
graph; "cascades" means affected cascades and is non-additive.

| Rank | Candidate | Direct / cascades | Recommendation and acceptance |
| ---: | --- | ---: | --- |
| 1 | Replace cross-package `export *` files in `packages/sdk` with generated explicit named reexports | 155 / 0 | Do now. Preserve the exact public type/value export inventory with a before/after API test. This is mechanical, target-neutral, and reversible. |
| 2 | Spell the three dual-sentinel optional-property sites as explicit presence branches | 3 / 36 | Do now if the rewrite preserves the distinction among absent, `undefined`, and `null`. Reproduce in `node.ts`, `swfControlHandler.ts`, and `swfPlaceObjectHandler.ts`, then compare TypeScript behavior. |
| 3 | Move captured local callbacks after the bindings they capture in `webDialog.ts` and `webMenu.ts` | 2 / 5 | Do now. Six reported occurrences are ordinary hoisting patterns; retain teardown/listener identity and add the existing Web tests as acceptance. |
| 4 | Replace length-only dense-array construction with an explicit push/fill loop at five internal sites | 5 / 4 | Reasonable now where holes are never observed. Prove length, element presence, and iteration behavior before rewriting; sparse-array semantics are not interchangeable. |
| 5 | Narrow homogeneous internal `WeakMap<object, unknown>` caches to their actual nominal key/value types | 24 / 9 | Do selectively. Apply only where one cache is semantically homogeneous and private. Do not rewrite heterogeneous caches or change weak identity/lifetime to satisfy C++. |
| 6 | Union/type-assertion clusters | 88 / 51 | Investigate per pattern, not as a bulk rewrite: contextual union value (27/20), type assertion identity (26/19), contextual union equivalence (22/9), and union member access (13/3) have distinct direct findings but overlapping cascades. Prefer small constructors or explicit guards only when they improve the TypeScript design. |

Do not rewrite around `duplicate-emitted-path`, generated-symbol key reachability (19 direct), or already-declared
ambient helpers such as `Array.from`. Those are compiler/profile defects, and changing Flight would either rename its
API or duplicate runtime behavior that flight-cpp already supplies.

## Closeout sequence

1. Land the explicit SDK reexports and the small proven source rewrites independently, rerunning both reports after
   each class so their actual effect is measured.
2. Fix package-scoped output paths in flight-compiler and correct the `const[type]` classification.
3. Add versioned external-binding composition to `check`, then apply flight-cpp's portable and SDL profiles in
   separate reports.
4. Reclassify the residual findings by provider. Implement a provider only after its host/runtime choice is explicit.
5. Pin the resulting Flight, flight-compiler, and flight-cpp revisions; regenerate the SDK goldens and run the full
   header/oracle/build matrix.
