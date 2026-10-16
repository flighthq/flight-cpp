# Compiler pin 806d257: what moved, what it cost, what is still owed

The runtime's pinned compiler moved from `2b687ea` to `806d257` in two commits: the mechanical
identifier rename (`7b725c7e`, pin unchanged) and the pin move itself (`fba64a77`: lock, rehydrate,
three Scene2D source patches, one full `sdk:generate`). This records what that cost, measured.

## Patches

| | |
| --- | --- |
| compiler patches verified against 806d257 | 68 -> 64 |
| applied cleanly | 58 |
| re-anchored | 5 |
| retired, the fix having landed upstream | 5 (one re-instated, see below) |
| flight patches | 178 -> 180 |

Re-anchored: `compiler-concrete-owner-union-row-assertions` (test anchor only),
`compiler-guarded-path-narrowing` (`guardedPathKinds` in the context literal),
`compiler-catch-await-owned-carrier`, `compiler-exported-type-owner-cache` (ported onto the new
`localForwarded` step, truncation propagated), `compiler-placeholder-graph-qualification` (806d257 added a
twelfth context parameter to `emitIrModuleCppWithContext`, so the refusal-placeholder graph now carries
`moduleImporterIndex`).

Retired because the old pin lacked the change and the new pin carries it: `compiler-scoped-emission-semantic-context`,
`compiler-nested-import-forward-closure`, `compiler-sdk-lane-local-reexports`,
`compiler-array-from-runtime-member`, and `compiler-dependent-erased-object-recovery-template`.

## The one retirement that was wrong

`compiler-dependent-erased-object-recovery-template` was retired on a code comparison that showed the new
pin emitting a disambiguated `object_if` from a receiver-dependence test. That comparison was right about
the shape and wrong about the coverage: 806d257 tests whether the RECEIVER is type-dependent, while the
patch's condition was the anonymous struct type parameters in scope, and the second is not a subset of the
first. @flighthq/materials lost eight headers to `use 'template' keyword to treat 'object_if' as a
dependent template name`. The patch is re-instated, re-anchored onto 806d257's `const recovery = ...` as
the union of both conditions, and materials measures 21 of 24 headers compiling, against 12 before the
re-anchor and 20 at the M2 baseline.

## Emission repairs

21 repairs matched no generated header after the move and were deleted, each verified against the tree:
the respellings' `from` text is gone and the corrected form is emitted, the `using` insertions' symbols are
now qualified at their call sites, and the three forward declarations are now emitted by the emitter at
the top of the header they belong to. `sdk:check` fails while a repair matches nothing, so this was the
whole of that debt.

## Overrides

88 of 186 declared overrides shadow a generated file that changed. The obvious re-derivation -- the
three-way merge `git merge-file <override> <7b725c7e:generated/...> <new generated>` -- was run for all of
them and MEASURED before it was believed: 43 merged without conflict and 45 conflicted, and the merged
corpus then compiled 163 of 186, against 174 of 186 for the prior emission left alone. It fixed nothing
and broke eleven, because the generated code changed structurally and a text merge splices an old body
into new scaffolding.

So every override keeps its prior emission verbatim, `derivedFrom` is refreshed to the new generated hash
with that retention recorded in the entry's `reason`, and the corpus was verified by compiling it:
174 of 186 shadowed headers compile. The twelve that do not are listed below; nine of them failed at the
M2 baseline too, and four of the remaining are `-j8` compiler crashes that pass at `-j1`.

One header, `flight/picking/runtime_support.hpp`, was a hand-written support header under
`overrides/include` that no manifest entry named -- invisible to the drift check and to tier accounting.
It has no generated counterpart to be derived from, so it was folded into the two overrides that included
it and removed. `npm run overrides:check` is green: 186 declared, none drifted, none unmanifested.

## Packages that compile fewer headers than the M2 baseline

| package | baseline | now | what it is |
| --- | --- | --- | --- |
| materials | 20 | 21 | recovered by the re-instated patch; above the baseline, so not a regression |
| image_codec | 5 | 1 | `std::variant<ArrayBuffer, Uint8Array>` cast |
| text | 9 | 4 | `get_text_layout` |
| intl | 5 | 3 | `get_collator`, `get_plural_rules` |
| media | 4 | 2 | `register_bus_in_reverse_map` |
| effects_wgpu | 6 | 4 | `optional<Ref<WgpuEffectPipeline>>` conversion |
| shape | 14 | 13 | `scale9_shape.hpp`; `shape.hpp` also lost an include of `flight/types/shape_command.hpp` |
| tray | 1 | 0 | `destroy_tray_runtime` |
| tokens | (gained elsewhere) | | `flight_document_scene_tokens.hpp`, optional-variant conversion |
| scene2d_resources | 7 | 6 | `load_scene_2d_image_resources.hpp` |
| scene3d_gl | 23 | 16 | NOT a regression. Every "lost" header was a `-j8` compiler crash; retried serially, all pass. The six scene3d_gl headers that still fail serially -- `_internal_index`, `anisotropy_pbr_gl_extension`, `gl_blinn_phong_mesh_material_renderer`, `gl_extended_pbr_mesh_material_renderer`, `gl_shaded_prelude`, `gl_specular_glossiness_pbr_mesh_material_renderer` -- failed at the M2 baseline too. |

`effects_wgpu` and `scene3d_wgpu` were also the two packages still emitted at the older source revision
`7e2fc7d`; the move regenerated them at `a627849`, so their headers changed for a source reason and not
for a compiler one.

## The header sweep

`FLIGHT_CPP_COMPILE_JOBS=8 npm run sdk:compile:sdl`:

| | baseline (2b687ea) | after the move |
| --- | --- | --- |
| headers attempted | 3152 | 3152 |
| headers compiling | **2292** | **2291** |
| headers failing | 860 | 861 |
| failing with `compiler exited with status 134` | 9 | 20 |

The sweep is noisy at `-j8`: a `134` is the compiler dying under eight concurrent heavy translation units,
not a rejected header. Retrying all twenty serially, fourteen passed and the other six -- every one in
scene3d_gl -- had already failed at the baseline. The per-package table below is what survives that, after
mapping the baseline's pre-rename header names onto the names the rename commit gave them and setting the
crashes aside.

## The carry table

Every header below passed at the M2 baseline and fails now for the reason given, and each ships its prior
emission as a `partial` override (see [the request](compiler-requests/806d257-prior-emission-carry.md)):

| header | diagnostic |
| --- | --- |
| `effects_wgpu/wgpu_effect_blit_shader`, `_box_blur`, `_tint_shader` | `optional<Ref<WgpuEffectPipeline>>` conversion |
| `image_codec/{_internal_index,contract,decode_image,detect_image_mime_type}` | `variant<ArrayBuffer, Uint8Array>` cast |
| `intl/{collator,plural}` | `get_collator` / `get_plural_rules` undeclared |
| `media/{audio_mixer,enable_audio_mixer_guards}` | `register_bus_in_reverse_map` undeclared |
| `scene2d_resources/load_scene_2d_image_resources` | `variant<ArrayBuffer, Uint8Array>` cast |
| `shape/scale9_shape` | `structural_ref_cast` |
| `text/{_internal_index,contract,enable_text_label_guards,native_text,text_label_layout}` | `get_text_layout`; `Traits` in native_text |
| `tokens/flight_document_scene_tokens` | optional-variant conversion |
| `tray/tray` | `destroy_tray_runtime` undeclared |

## Examples

`npm run examples:check` passes: the two generated native examples match repaired 806d257 output. The six
SDL example candidates could not be rebuilt here: this workspace carries SDL3 headers only
(`.tools/sdl3/SDL3-3.4.10/include`) and no `libSDL3`, so `Flight::HostSdl` does not link. The committed
`artifacts/e2e/scene3d.png` and `artifacts/e2e/camera2d.png` remain from the A5 baseline.

## Obsolete patches

Five compiler patches retired because the change they carried landed upstream:
`compiler-scoped-emission-semantic-context`, `compiler-nested-import-forward-closure`,
`compiler-sdk-lane-local-reexports`, `compiler-array-from-runtime-member`, and
`compiler-dependent-erased-object-recovery-template` — the last re-instated above, so four remain retired.
Their files are gone from `source-patches/`, and `docs/compiler-requests/README.md` keeps their rows,
marked retired, as the reference to the request document each one answered.

## Tooling

Two defects in the rename tool, both from the move it exists to perform:

- The harvest read only the pinned TypeScript. `Material3DValue` is a name the emitter INVENTS -- an open
  base contract materialized as a union -- and no Flight token spells it, so `material3_dvalue.hpp` was
  left unrenamed and the next regeneration renamed a file two overrides included. The harvest now also
  reads the module names the committed inventory's own headers state.
- The converter self-check read the OLD converter from the checkout working tree. After the move that tree
  holds the NEW one, so the check threw before planning and the tool could not be re-run over a branch
  that already carried the rename -- the property it is documented to have. Both converters are now read
  from their own revisions with `git show <pin>:...`.

Both have controls in `scripts/renameIdentifiers.test.mjs`.
