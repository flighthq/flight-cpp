# @flighthq/bitmaptext -- upstream fixture coverage, and why there is no suite

**No native behavioural test is registered for `@flighthq/bitmaptext`.** This document exists to
record that fact precisely and to account for every upstream case against the boundary that blocks
it -- **not** to imply partial coverage, progress, or readiness. Nothing here is covered; nothing here
is close to covered. The package cannot carry a behavioural suite until the boundaries below are
resolved, and writing one now would mean asserting stub behaviour, which `AGENTS.md` forbids.

## The inventory, and the searches behind it

- `tests/CMakeLists.txt` contains no `bitmap` in any form (case-insensitive, whole file).
- `ctest --test-dir build -N` lists 85 tests; none mentions bitmap.
- Of 86 test sources under `tests/`, none is named `bitmap*`, and **no test source references the
  `bitmaptext` namespace at all**. The single `bitmap` hit in `tests/` is `host_sdl_test.cpp` using
  `flight::host_sdl::ImageSourceKind::image_bitmap` -- an SDL image-source enum, unrelated to this
  package.
- `docs/` has no bitmaptext coverage document; the existing mentions are incidental (listed under
  Cross-references below).

A first pass at this inventory produced a **false negative** worth recording, because it would have
been reported as fact: grepping `add_executable(<name>` on one line misses the multi-line form that
the `textshaper` and `textsegment` targets use, so an entire registered target can appear absent. The
conclusion above comes from the authoritative sources -- `ctest -N`, and a whole-file search of
`tests/CMakeLists.txt` -- not from that regex.

## Package state

`@flighthq/bitmaptext` is **`partial`**: 6 modules, 3 emitted, 3 refused, **4 headers failing**, 0
unattempted. Re-measured live, `pkg:tier` reports it as `partial at 7e2fc7d -- 3/6 modules emitted,
6/6 headers compiled, 4 failing, 0 unattempted`; the counts agree with the repository's own parity
table in `docs/parity/text.md` (`bitmaptext | 6 | 3 | 3 | 4 | 81 | none`), including the 81-case
figure derived independently below. The earlier revision label in this document was the pin; the row
this package's tier reads from is the generated tree at `7e2fc7d`.

Four emitted headers fail to compile. They no longer inherit the textureatlas defect described under
Root 3 below: that root is now carried by the `textureatlas-region-nullable-partial-read` override, and
`texture_atlas_region.hpp:290` does not appear anywhere in this package's compile output when the
override is on the include path. The four fail on **bitmaptext's own** errors instead -- `no matching
function for call to 'create_texture'` at `update_bitmap_text.hpp:121`, and `use of undeclared
identifier 'build_bitmap_text_words'` at `:254` and `:303`:

| header | status |
| --- | --- |
| `bitmap_text.hpp` | compiles, via the `partial` override |
| `explain_bitmap_text_missing_glyphs.hpp` | compiles (function omitted -- see below) |
| `update_bitmap_text.hpp` | **COMPILE-BLOCKED** (`holds_alternative`) |
| `enable_bitmap_text_guards.hpp` | **COMPILE-BLOCKED** transitively: it includes `update_bitmap_text.hpp`, and the two guard setters it calls are defined only there |
| `_internal_index.hpp`, `contract.hpp` | **COMPILE-BLOCKED** (umbrella; inherit the above) |

There is no separate `bitmap_text_guards.hpp` in the emitted tree at all.

### The override

`bitmaptext-bitmap-text-missing-functions` (`overrides/include/flight/bitmaptext/bitmap_text.hpp`,
`status: partial`, hashing clean against its `derivedFrom`) supplies 14 declarations and carries 14
`NOT GENERATED` markers. Three things in it matter here:

- it **stubs** `compute_bitmap_text_local_bounds_rectangle` to a **zero rectangle**;
- it **stubs** `create_texture_atlas` to an **empty atlas**;
- it fixes `target->data.member` to `target->data->member` (the emitted member spelling was wrong).

Its own manifest `remaining` states the gap without softening it: *"Bitmap text still uses zero local
bounds and an empty texture atlas, and the copied partial module retains additional refused layout,
pagination, initialization, truncation, reservation, and option helpers."*

## Boundary legend

Every case below carries exactly one label. The labels are not degrees of coverage; they are the
distinct reasons a case cannot be asserted natively.

- **STUBBED** -- the function exists but the override supplies placeholder behaviour. A test could
  run, and would assert the stub. This is the most dangerous category, because a passing assertion
  here would certify debt as correct.
- **REFUSED** -- the emitter did not generate the function; the module carries a `NOT GENERATED`
  marker with the refusal reason.
- **COMPILE-BLOCKED** -- the header does not compile, so nothing it contains is reachable at all.
- **UNAVAILABLE-OWNER** -- refused by the readonly-row-to-writable-row structural assertion, because
  the source owner does not declare the cells the asserted row would mutate.
- **SUPPLIED-SUBJECT-UNAVAILABLE** -- the function is emitted and callable, but its subject is a value
  only a REFUSED constructor can produce, so no fixture can reach it.

## Case accounting -- 81 cases, 4 fixtures

Counted mechanically (per-file `it(`/`test(` at statement position) and cross-checked against
`docs/parity/text.md`. There are no `it.each` rows in this package.

### `bitmapText.test.ts` -- 32 cases

Module `bitmapText.ts`; emitted as `bitmap_text.hpp` through the override. **Every case here is
STUBBED, REFUSED, UNAVAILABLE-OWNER or SUPPLIED-SUBJECT-UNAVAILABLE.**

| describe (cases) | label | exact boundary |
| --- | --- | --- |
| `computeBitmapTextLocalBoundsRectangle` (3) | **STUBBED** | the override returns a zero rectangle; the cases assert a real laid-out extent, aliasing safety, and pre-layout zeros |
| `createBitmapText` (3) | **REFUSED** | not generated |
| `createBitmapTextData` (2) | **REFUSED** | not generated |
| `createBitmapTextRuntime` (1) | **UNAVAILABLE-OWNER** | the readonly `Node2DRuntime` row to writable `BitmapTextRuntime` assertion; the owner declares none of `glyphLayoutVersion`, `lineCount`, `pages`, `truncated` |
| `getBitmapTextBounds` (1) | **SUPPLIED-SUBJECT-UNAVAILABLE** | emitted, but its subject is a node only the REFUSED `createBitmapText` can build |
| `getBitmapTextLineCount` (4) | **REFUSED** | not generated |
| `getBitmapTextPages` (1) | **REFUSED** | not generated |
| `initializeBitmapTextData` (1) | **REFUSED** | not generated |
| `isBitmapTextGlyphLayoutStale` (3) | **REFUSED** | not generated |
| `isBitmapTextTruncated` (4) | **REFUSED** | not generated |
| `reserveBitmapText` (1) | **REFUSED** | not generated |
| the eight `setBitmapText*` setters (8) | **SUPPLIED-SUBJECT-UNAVAILABLE** | all emitted, but each mutates a node the REFUSED constructor must first produce |

The eight setters are worth stating plainly: they are the *only* substantial emitted surface in this
module, and they are still untestable, because there is no way to obtain the object they mutate.

### `updateBitmapText.test.ts` -- 38 cases

Module `updateBitmapText.ts`; emitted as `update_bitmap_text.hpp`. **Every case is COMPILE-BLOCKED.**
The header fails on `holds_alternative`, so none of its content is reachable, and two of its own
functions are additionally REFUSED: `buildBitmapTextWords` (source line 108) and `updateBitmapText`
(source line 62) both carry `NOT GENERATED` markers.

| describe (cases) | label |
| --- | --- |
| `refreshBitmapTextGlyphLayout` (4) | **COMPILE-BLOCKED** (and `updateBitmapText` REFUSED) |
| `setBitmapTextLayoutGuard` (3) | **COMPILE-BLOCKED** |
| `setBitmapTextMissingGlyphGuard` (2) | **COMPILE-BLOCKED** |
| `updateBitmapText` (29) | **COMPILE-BLOCKED** (and `updateBitmapText` REFUSED) |

### `enableBitmapTextGuards.test.ts` -- 6 cases

Module `enableBitmapTextGuards.ts`; emitted as `enable_bitmap_text_guards.hpp`. **Every case is
COMPILE-BLOCKED.** The header compiles only if its own module does, and it includes
`update_bitmap_text.hpp`, which does not; the guard setters it calls are defined only in that blocked
header.

| describe (cases) | label |
| --- | --- |
| `disableBitmapTextGuards` (2) | **COMPILE-BLOCKED** |
| `enableBitmapTextGuards` (4) | **COMPILE-BLOCKED** |

This is the one fixture whose blocker is *not* a capability gap of its own: it is downstream of the
`updateBitmapText` module. Nothing was changed to break that dependency cycle.

### `explainBitmapTextMissingGlyphs.test.ts` -- 5 cases

Module `explainBitmapTextMissingGlyphs.ts`; emitted as `explain_bitmap_text_missing_glyphs.hpp`. The
header **compiles** and is not in the failing set, but its single exported function
`explainBitmapTextMissingGlyphs` is **REFUSED** (source line 3), so **all five cases are REFUSED**.
This is the case that shows "compiles" and "usable" are different questions.

| describe (cases) | label |
| --- | --- |
| `explainBitmapTextMissingGlyphs` (5) | **REFUSED** |

## Totals

| label | cases |
| --- | --- |
| COMPILE-BLOCKED | 44 |
| REFUSED | 24 |
| SUPPLIED-SUBJECT-UNAVAILABLE | 9 |
| STUBBED | 3 |
| UNAVAILABLE-OWNER | 1 |
| **total** | **81** |

Covered: **0**.

## The two roots, and the open decision

The five labels reduce to three underlying causes, and two of them are already tracked:

1. **`string` `for...of` needs a code-point iteration runtime contract.**
   [`docs/compiler-requests/compiler-string-for-of-codepoint-iteration.md`](compiler-requests/compiler-string-for-of-codepoint-iteration.md)
   names `@flighthq/bitmaptext`'s `updateBitmapText.ts` and `explainBitmapTextMissingGlyphs.ts`
   explicitly as two refusals it blocks, and records that no workaround is carried *"because a
   hand-rewritten index loop is not equivalent for surrogate pairs"*. This is the root of the
   REFUSED label.

2. **A writable `BitmapTextRuntime` owner the source does not declare.** The assertion refused in
   `bitmapText.ts` asks for mutation the source withheld; the owner has no `glyphLayoutVersion`,
   `lineCount`, `pages` or `truncated` cells. This is the same runtime-wide identity-preserving owner
   recovery decision recorded as open in the manager's ledger, which also blocks the physics2d joints,
   `connectSignalTracked`, the loader refinement and `bitmapfont-formats`. See
   [`docs/compiler-requests/compiler-imported-readonly-view-owner-recovery.md`](compiler-requests/compiler-imported-readonly-view-owner-recovery.md)
   for the related shape. This is the root of the UNAVAILABLE-OWNER label.

3. **A nullable `Partial<T>` read lowered as a variant** -- the `holds_alternative` failure. This is
   **tracked**, by
   [`docs/compiler-requests/partial-row-nullable-read-carrier.md`](compiler-requests/partial-row-nullable-read-carrier.md).
   It is a defect in `@flighthq/textureatlas`, not in bitmaptext: `set_texture_atlas_region` reads a
   `T | null` property through a `RowPartial` (an `std::optional<T>`) and lowers it as though it were
   an `std::variant<T, Null, Undefined>`.

   **bitmaptext is NOT a consumer of it any more, and this document previously said it was.** That
   claim was true when written and has since been superseded: the
   `textureatlas-region-nullable-partial-read` override now carries that root, and a fresh
   package-scoped compile of this package reports `texture_atlas_region.hpp:290` **zero** times. The
   four failing headers are accounted for by bitmaptext's own `create_texture` and
   `build_bitmap_text_words` errors instead. The compiler-requests index entry added at the same time
   remains correct and is kept; the consumer claim is withdrawn here rather than left to mislead. This
   is still why COMPILE-BLOCKED is the largest bucket, but the cause is local, not inherited.

## Cross-references

- [`docs/parity/text.md`](parity/text.md) -- the package row (`bitmaptext | 6 | 3 | 3 | 4 | 81 | none`).
- [`docs/compiler-requests/compiler-string-for-of-codepoint-iteration.md`](compiler-requests/compiler-string-for-of-codepoint-iteration.md) -- root cause 1.
- [`docs/compiler-requests/README.md`](compiler-requests/README.md) -- the index entry naming the two blocked bitmaptext modules.
- [`docs/compiler-requests/compiler-imported-readonly-view-owner-recovery.md`](compiler-requests/compiler-imported-readonly-view-owner-recovery.md) -- root cause 2.
- [`docs/compiler-requests/partial-row-nullable-read-carrier.md`](compiler-requests/partial-row-nullable-read-carrier.md) -- root cause 3 (the `holds_alternative` failure); names `update_bitmap_text.hpp` among its consumers as of this investigation.
- [`docs/generation-state.md`](generation-state.md) -- the `BitmapText.ts` interface-inheritance failure (`Node2D` inherits incompatible property `EntityRuntimeKey`) and the `bitmaptext-unqualified-types-names` emission repair.
- `overrides/manifest.json` -- `bitmaptext-bitmap-text-missing-functions`, `status: partial`, including its `remaining` statement quoted above.

## What this document is not

It is not a plan, and it asserts no behaviour. No test, override or generated file was changed to
produce it, and no stub behaviour is asserted anywhere in it. It exists so the next reader learns the
package's real state from one place instead of re-deriving it: **there is no native bitmaptext suite,
none of the 81 upstream cases is covered, and the three causes above must move before one can be.**
