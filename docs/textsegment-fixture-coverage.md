# @flighthq/textsegment -- upstream fixture coverage

`@flighthq/textsegment` is `assisted` at a627849: 5 of 7 modules emitted, **7 of 7 emitted headers
compile**, 0 failing, 0 unattempted. Two overrides carry the refused modules
(`text_segment_boundary.hpp`, `text_segmenter_backend.hpp`, both `complete`, both hashing clean
against `derivedFrom`). The native suite is `tests/generated_textsegment_parity_test.cpp`, 55 checks.

## Inventory

Five upstream fixtures, **39 cases**. Counted mechanically two ways that agree exactly: a
brace-depth scanner that also expands `it.each` rows (there are none in this package), and a plain
per-file count of `it(`/`test(` at statement position.

| fixture | cases | accounting |
| --- | --- | --- |
| `textSegmentBoundary.test.ts` | 21 | reachable -- covered |
| `textSegmenterBackend.test.ts` | 7 | 3 covered (1 of them as a stated DIVERGENCE); 4 bounded by the bundled provider |
| `textSegment.test.ts` | 6 | 2 covered; 4 bounded by the bundled provider |
| `enableTextSegmentGuards.test.ts` | 3 | 2 covered (restored with the log override); 1 unreachable here |
| `textSegmentGuards.test.ts` | 2 | reachable -- covered |

## The boundary, stated rather than worked around

This profile has **no `Intl.Segmenter`**. `has_intl_segmenter()` is false, the bundled provider
reports itself unavailable, and `segment()` returns no segments rather than fabricating a look-alike
result. Upstream's fixtures run on a host where it exists, so the seven cases that assert the bundled
provider's own clustering (the ZWJ family, the combining mark, sentence splitting, word-likeness,
gap-free offsets, the stable fallback, and `explain` reporting `available: true`) are **not
reproducible here** and are named rather than copied. Copying them would either fail or invite
someone to "fix" it with a substitute segmenter, which is the one thing the boundary forbids.

The clustering semantics themselves are NOT left uncovered: the boundary algorithms are pure
functions over the segments a backend returns, so they are driven here by a supplied backend that
returns exactly what a real segmenter would (`backend_with`), which is how the ZWJ cluster, the
clamping, and the whitespace-is-not-a-word rules are tested without Intl.

## What the re-audit found

The suite was already good, and the first pass over it read as complete. The measured gaps were
found by mutation, not by reading, and every one of them was a case the suite could not see:

- **Wrapper granularity was completely unpinned.** The six `get*Boundary` wrappers differ from one
  another ONLY in the granularity they pass (`getNextGraphemeBoundary` IS
  `getNextTextSegmentBoundary` with `"grapheme"`), and so do the three `segment*` wrappers. Every
  backend in the file took the granularity and **ignored** it, so swapping a wrapper to the wrong
  granularity left the entire suite passing. Two mutations proved it (`grapheme`->`sentence`,
  `word`->`grapheme`), and a third proved the same for `segment_graphemes`.
- **The index clamp was unpinned in both directions.** `getWordRangeAt` clamps the index to
  `[0, text.length]` before resolving (upstream `clampIndex`), so a negative index selects the FIRST
  word and an index past the end selects the LAST. Every index the suite passed was already in range,
  so removing either clamp branch changed nothing observable.
- **`intlSegmenterAvailable` was a second constant rather than the probe's answer.** Upstream defines
  it as `hasIntlSegmenter()`; the override hardcodes `false`, which agrees today but severs the tie,
  and `has_intl_segmenter()` is consequently never consulted. A mutation making the probe report a
  capability this profile lacks survived, because nothing connected the two.

The third of these is the interesting one: it is not a missing assertion about a value, it is a
missing assertion about a **relationship** between two values that happen to coincide.

## Mutations

Two batteries, **24 mutations**, every one scoped to a single function body with a one-site anchor
and confirmed to hit exactly one site before its result was read. `MIS-TARGETED` and `NO-OP` are
reported as such rather than silently skipped.

| # | mutation | target | before | after |
| --- | --- | --- | --- | --- |
| C1 | a negative index is not clamped to zero | boundary override | SURVIVED | caught |
| C2 | an index past the end is not clamped to length | boundary override | SURVIVED | caught |
| N1 | the forward step does not advance past the current boundary | boundary override | caught | caught |
| P1 | the backward walk stops one boundary late | boundary override | caught | caught |
| W1 | the end-of-text lookup no longer steps back one unit | boundary override | caught | caught |
| W2 | a non-word-like segment is accepted as a word | boundary override | caught | caught |
| W3 | empty text no longer short-circuits | boundary override | SURVIVED | still survives -- EQUIVALENT |
| W4 | the web-backend identity test is inverted | boundary override | caught | caught |
| G1 | `getNextGraphemeBoundary` asks for SENTENCE granularity | boundary override | SURVIVED | caught |
| G2 | `getPreviousWordBoundary` asks for GRAPHEME granularity | boundary override | SURVIVED | caught |
| T1 | the segmenter-less provider returns the start, not the clamp | boundary override | caught | caught |
| B1 | `available` is inverted for the bundled provider | backend override | MIS-TARGETED -> caught | caught |
| B2 | the backend labels are swapped | backend override | MIS-TARGETED -> caught | caught |
| B3 | the unavailable path stops reporting through the seam | backend override | caught | caught |
| B4 | the segmenter-less provider fabricates a segment | backend override | caught | caught |
| B5 | the Intl probe reports a capability this profile lacks | backend override | SURVIVED | caught |
| B6 | the bundled provider is left without a segment function | backend override | caught (crash) | -- |
| G3 | installing a guard no longer replaces the callback | text_segment_guards | caught | caught |
| G4 | the disengaged-guard short circuit is removed | text_segment_guards | caught (crash) | -- |
| E1 | disabling no longer clears the guard | enable_text_segment_guards | caught | caught |
| E2 | the warning stops naming the provider type | enable_text_segment_guards | caught | caught |
| E3 | enabling no longer installs the guard | enable_text_segment_guards | caught | caught |

**Every fix was re-run against the mutant that motivated it** and is caught by a clean `FAIL:` line,
not by a crash.

### Crash detections, kept separate

B6 and G4 are caught only by **crash** (exit 134 on a null call), not by a check. They are real
detectors but weak ones -- they show the code is wrong, not that the suite understands why -- so they
are listed apart from the check catches rather than folded in. Neither has an assertion behind it and
neither was "fixed": a crash is a sufficient kill, and adding an assertion that merely re-tests the
same reachability would be padding.

### The one equivalence

W3 (removing `if (text.empty()) return std::nullopt;`) **survives and cannot be made to fail**. With
empty text, `length` is 0, so `clamped == length` holds, `lookup` becomes `-1`, and no segment can
have a start `<= -1` -- the loop returns nullopt, exactly as the guard does. It is a true
equivalence, not an unasserted behaviour, and it is recorded as one rather than papered over with an
assertion that could never fail.

### A method note

B1 and B2 first reported MIS-TARGETED because `explain_text_segmenter_backend`'s signature **wraps
across two lines** in the override, so a matcher anchored on `^inline ... name(` missed it. That is
the same extraction trap a previous audit hit, and it is why the harness reports MIS-TARGETED loudly
instead of skipping silently: the first reading of those two was "not applicable" when the truth was
"not applied". Re-run with a matcher that tolerates the wrapped signature, both are caught.

## What was NOT added, and why

`textSegment.test.ts`'s "isolates callers that interleave different explicit hosts" is the one
upstream case with no assertion added and no note in the test. It asserts that two hosts do not
contaminate each other through shared state; the wrapper holds no state between calls (it forwards
straight to the host), so there is no defect for a mutation to express and no measured gap to close.
Per the standing rule -- add only measured gaps -- it is recorded here rather than padded into the
suite.

## The third guard case

`enableTextSegmentGuards.test.ts`'s "stays silent when the bundled Intl provider is available" is not
ported and is not a gap. It asserts the ABSENCE of a warning when `Intl.Segmenter` exists; on this
profile it never does, so the branch is unreachable and asserting silence would assert a capability
this profile does not have. The other two cases are ported and pass, now that the log CORE override
supplies a real `merge_span_fields` for `log_once` to build entries through.
