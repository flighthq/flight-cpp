# Clip native parity addendum

This is an additive audit of `@flighthq/clip` at Flight revision
`a62784923f3be814463286c9fb28edfe6ab15789`. Its baseline is the authoritative integrated commit
`0b2fdfe1`, especially the 306-line `tests/generated_clip_test.cpp`. That file, its registration,
its parity documentation, and the integrated one-line generated-header adjustment were not copied,
replaced, narrowed, or edited here.

## Authoritative local inventory

The live baseline has one package-focused target, `flight_cpp.generated_clip`, labeled `pkg:clip`.
Its 306-line source covers pooling and reset, construction ownership, clone/copy identity,
conservative region predicates, exact point containment, rectangular and aliased intersection,
contour-retaining union, transformed bounds and representation, mutation/version ordering, malformed
contour bounds, the three low-level guard seams, and public guard enable/disable.

The only new test is `tests/generated_clip_edge_test.cpp`, registered separately as
`flight_cpp.generated_clip_edge` and labeled `pkg:clip`. Keeping it separate makes the additive
boundary reviewable and prevents a stale checkout from replacing the integrated baseline. Its 18
grouped checks cover only gaps measured against that exact live file.

Repository-wide exact package/include and registration scans found no other package-focused Clip
test. `generated_scene2d_display_object_test.cpp` stores a `types::ClipRegion`,
`structural_row_test.cpp` uses a local `TestClipRegion`, and `generated_interaction_test.cpp`
constructs an optional ClipRegion row field; none calls `flight::clip`. Uses of a variable named
`clip` in Path Boolean and media tests are unrelated. There is no `overrides/include/flight/clip`
directory. The existing `clip-explicit-contours-storage` source patch and its generated output are
baseline inputs, not changes made by this audit.

## Mechanical upstream census

The pinned source contains two test files and 81 literal `it(...)` declarations: 76 in
`clipRegion.test.ts` and 5 in `enableClipGuards.test.ts`. Separate scans found zero `it.each`, zero
`test.each`, zero `describe.each`, and zero loop- or `forEach`-generated test declarations. Both
upstream modules emit, compile, and are called by native tests, so the unreachable-module count is
**zero**.

The table maps every upstream describe group. “Live” means the integrated 306-line test already
calls that behavior. “Additive” means the supplemental target supplies the previously absent direct
axis. “Constructor-driven” means the upstream case only asserts that an initializer exists; native
constructor calls compile and execute that initializer rather than duplicating a JavaScript
`typeof` assertion. This is behavioral-axis coverage, not a claim that all 81 Vitest bodies were
ported statement for statement.

| upstream describe group | cases | native evidence after this audit |
|---|---:|---|
| `acquireClipRegion` | 4 | live |
| `clipRegionContainsClipRegion` | 1 | live |
| `clipRegionContainsPoint` | 4 | live; additive separates even-odd from non-zero winding |
| `clipRegionContainsRectangle` | 4 | additive direct rectangle predicate |
| `clipRegionIntersectsClipRegion` | 1 | live |
| `clipRegionIntersectsRectangle` | 4 | additive direct rectangle predicate |
| `cloneClipRegion` | 2 | live |
| `copyClipRegion` | 2 | live |
| `createClipRegionFromCircle` | 1 | additive direct, including default tolerance |
| `createClipRegionFromContours` | 4 | live ownership/malformed input; additive empty representation |
| `createClipRegionFromEllipse` | 1 | additive direct |
| `createClipRegionFromPath` | 1 | additive direct |
| `createClipRegionFromRectangle` | 1 | live |
| `createClipRegionFromRoundedRectangle` | 2 | additive positive-radius and zero-radius branches |
| `equalsClipRegion` | 5 | live across identity, equal, and unequal geometry |
| `explainClipRegion` | 1 | live released state; additive active conservative/rectangular split |
| `explainClipRegionContours` | 1 | additive odd, short-even, and valid results |
| `getClipRegionBounds` | 1 | live |
| `initializeClipRegionFromContours` | 1 | constructor-driven through live contour construction |
| `initializeClipRegionFromPath` | 1 | constructor-driven through additive path construction |
| `initializeClipRegionFromRectangle` | 1 | constructor-driven through live rectangle construction |
| `intersectClipRegions` | 5 | live rect/disjoint/alias; additive mixed and richer-contour order |
| `invalidateClipRegion` | 1 | live, including unsigned wrap |
| `isClipRegionEmpty` | 3 | live malformed case; additive empty-contour, empty-rect, and non-empty split |
| `isClipRegionRectangular` | 2 | additive direct |
| `normalizeClipRegion` | 6 | additive rect promotion, non-rect preservation, and alias safety |
| `releaseClipRegion` | 1 | live |
| `setClipRegionContoursGuard` | 1 | live odd input; additive short-even versus valid input |
| `setClipRegionReleaseGuard` | 2 | live |
| `setClipRegionToContours` | 1 | live |
| `setClipRegionToRectangle` | 1 | live |
| `setClipRegionUseGuard` | 1 | live |
| `transformClipRegion` | 5 | live axis-aligned, rotated, contour, and alias paths |
| `unionClipRegions` | 4 | live richer-contour ownership; additive rectangular bounds and both aliases |
| `disableClipGuards` | 1 | live |
| `enableClipGuards` | 4 | live installation/removal plus malformed, release, and use seams |
| **total** | **81** | **2 direct modules, 0 unreachable modules** |

There is no Clip subtraction export, implementation, import, or upstream test group at this pin.
Intersection and union are the complete composition surface; subtraction is therefore classified as
absent rather than an uncovered native path.

## Measured additive gaps

Comparison against the exact live file found these distinct gaps, all now in the supplemental target:

- circle, ellipse, path, and rounded-rectangle construction, including the omitted `0.25` tolerance;
- rectangle containment/intersection calls and a fixture where containment differs from overlap;
- two same-order contours for which `evenOdd` and `nonZero` deliberately disagree;
- rectangular union bounds and both output-alias orders;
- mixed intersection plus selection, winding, order, and deep copy of the richer contour operand;
- normalization to scissor form and alias-safe preservation of an irregular contour;
- the semantic distinction between an empty contour array and an empty rectangular clip; and
- odd, short-even, and valid diagnostics, including the optional guard's valid-input silence.

The live suite already closed the requested identity/copy, transform-representation, and release/use
guard axes. Those assertions were retained as the baseline instead of being duplicated.

## Frozen mutation evidence

The authoritative live source was frozen at SHA-256
`211c9d1f58c8140d2eded914785eb515832bef6a6ef246e88a1d0430ca990aa3`; the additive source was frozen
at `eb888360c25c32b9f7307532c6d62c98e317140e34c1bec798a3004832e13c26`. Every mutant was applied
alone to `generated/include/flight/clip/clip_region.hpp`, confirmed as a one-file diff, compiled,
run, and restored before the next mutant. Both hashes were unchanged after the run.

Eight mutants identify genuine additive holes: the live suite passed, while the supplemental target
failed by the named assertion.

| axis | isolated mutant | live result | additive failure |
|---|---|---|---|
| optional/default | circle default tolerance `0.25` to `4.0` | pass | documented default tolerance |
| containment | rectangle containment called overlap instead of enclosure | pass | enclosed versus overlapping rectangle |
| winding/order | even-odd returned non-zero winding truth | pass | duplicated same-order contour disagreement |
| intersection | richer-contour comparator `>=` changed to `<=` | pass | richer operand, winding, and order |
| union/bounds | union merged `a` with `a` instead of `a` with `b` | pass | full rectangular union extent |
| normalization | one-contour eligibility changed from `1` to `2` | pass | quad-to-scissor promotion |
| empty/degenerate | rectangular predicate inverted contour presence | pass | empty contour versus rectangular form |
| diagnostics/guard | too-few threshold changed from 6 coordinates to 4 | pass | short-even explanation and guard |

Three control mutants were already killed by the live suite, confirming that no additive assertion
was warranted for those axes:

| live-owned axis | isolated mutant | live failure |
|---|---|---|
| identity/copy | clone retained the source contour array | clone ownership and post-mutation equality |
| transform | every rectangle transform was forced axis-aligned | rotated rectangle contour promotion |
| guard | double-release membership detection was suppressed | exactly-once release report |

The live test's own frozen evidence also records a killed rectangular-intersection ordering mutant.
Together the live and additive targets exercise construction, intersection and union, containment and
bounds, transforms, empty and degenerate clips, winding/order, identity/copy behavior,
optional/default behavior, and guard paths without changing generated or override code.

## Verification

- `@flighthq/clip` baseline package compile: 4/4 headers passed independently;
- package tier: assisted and shippable, 4/4 emitted and compiled, zero overrides;
- standalone clean runs: both the exact live source and the additive source passed; and
- CMake focused target: `flight_cpp_generated_clip_edge_tests` built at `-j2` and passed.

