# @flighthq/textbidi -- upstream fixture coverage

`@flighthq/textbidi` is `assisted` at a627849: 7 of 8 modules emitted, **8 of 8 emitted headers
compile**, 0 failing, 0 unattempted. The native suite is `tests/generated_textbidi_parity_test.cpp`,
84 checks.

`resolveBidiLevels` is supplied by the hand-written override
`overrides/include/flight/textbidi/resolve_bidi_levels.hpp`, which replaces the whole module. These
cases therefore check OUR resolver against the upstream intent, not the emitter's output -- which is
why the mutations below matter more here than in a package the emitter produced.

## Inventory of local TextBidi test files

Checked before any gap claim, because a second file would change the accounting. **One** local test
file references textbidi: `tests/generated_textbidi_parity_test.cpp` (registered in
`tests/CMakeLists.txt` as `flight_cpp.generated_textbidi_parity`, label `pkg:textbidi`). No other
test, helper header, or fixture file in this repository touches it.

## Upstream case inventory

Seven fixtures, **57 cases**, counted two ways that agree exactly: a brace-depth scanner that also
expands `it.each` rows (there are none in this package) and a plain per-file count of `it(`/`test(`
at statement position.

| fixture | cases | accounting |
| --- | --- | --- |
| `resolveBidiLevels.test.ts` | 27 | reachable -- covered |
| `getBidiRuns.test.ts` | 9 | reachable -- covered |
| `reorderBidiLine.test.ts` | 6 | reachable -- covered |
| `bidiClassKernel.test.ts` | 6 | reachable -- covered |
| `bidiConformance.test.ts` | 3 | reachable -- covered |
| `enableTextBidiGuards.test.ts` | 4 | reachable -- covered (restored with the log override) |
| `textBidiGuards.test.ts` | 2 | reachable -- covered |

No case in this package is blocked on a profile boundary. `enableTextBidiGuards.test.ts`'s four
cases assert on LOG ENTRIES and were previously unportable while the log override stubbed
`merge_span_fields`; the log CORE override now supplies it, so they are ported and pass.

## What the re-audit found

The suite was already strong and had been mutation-audited before. Two holes survived that audit,
both in the resolver, and both share a shape worth naming: **the fixture set did not contain an
input that reaches the differing branch**, so the mutation left every existing assertion passing.

- **The paragraph-level scan did not have to skip isolate content.** P2/P3 derive the paragraph
  level from the first strong character *outside* any isolate. Every isolate fixture here has a
  strong character outside the isolate *before the isolate starts*, so skipping and not skipping
  agreed on all of them. With the only strong text inside an isolate -- `LRI + HEBREW + PDI` -- they
  diverge: skipping leaves no strong character and the base is 0 (`0 3 3 3 3 0`), while looking
  inside derives 1 (`1 3 3 3 3 1`).
- **`nextOdd` did not have to force an odd level.** Every embedding fixture starts from an EVEN
  paragraph level, where `level + 1` and `bitwise_or(level + 1, 1)` coincide. An embedding that
  starts from an odd level separates them: `HEBREW + RLE + "abc" + PDF + HEBREW` gives `4` for the
  L content when the RLE is `nextOdd(1) = 3` (I1/I2 raise L content in an odd run once more) and `2`
  when it is `1 + 1`.

Both are asserted now, each constructed from the UAX#9 rule it pins rather than quoted from
upstream, and each marked as constructed in the test.

## Mutations

**24 mutations**, every one scoped to a single function body with a one-site anchor and confirmed to
hit exactly one site before its result was read. `MIS-TARGETED` would have been reported rather than
skipped; none were, after the applier was written to tolerate the wrapped signatures in this tree.

| # | mutation | target | before | after |
| --- | --- | --- | --- | --- |
| D1 | a leading L no longer fixes the paragraph to level 0 | override | caught | caught |
| D2 | a leading R/AL no longer fixes it to level 1 | override | caught | caught |
| D3 | the paragraph scan no longer skips isolate content | override | **SURVIVED** | **caught** |
| D4 | the neutral direction is inverted | override | caught | caught |
| E1 | `nextEven` no longer forces an even level | override | caught | caught |
| E2 | `nextOdd` no longer forces an odd level | override | **SURVIVED** | **caught** |
| E2b | `nextOdd`'s return value is ignored entirely | override | caught | caught |
| L1 | the trailing L1 reset is dropped | override | caught | caught |
| B1 | the closing-bracket lookup returns the opening one | override | caught | caught |
| R1 | the reversal floor starts at zero, not past-the-end | `reorder_bidi_line` | caught | caught |
| R2 | the reversal floor tracks even levels instead of odd | `reorder_bidi_line` | caught | caught |
| G1 | the run direction parity is inverted | `get_bidi_runs` | caught | caught |
| G2 | runs split on equal level instead of differing level | `get_bidi_runs` | caught | caught |
| GD1 | installing a guard no longer replaces the callback | `text_bidi_guards` | caught | caught |
| GD2 | the disengaged-guard short circuit is removed | `text_bidi_guards` | caught (crash) | -- |
| EB1 | disabling no longer clears the guard | `enable_text_bidi_guards` | caught | caught |
| EB2 | the warning stops naming the kernel fixing call | `enable_text_bidi_guards` | caught | caught |
| EB3 | the warn-once key varies per code point | `enable_text_bidi_guards` | caught | caught |
| K1 | the uncovered fallback class is not L | `bidi_class_kernel` | caught | caught |
| K2 | the compact miss is never reported | `bidi_class_kernel` | caught | caught |
| X1 | the compact kernel label is not "compact" | `bidi_class_kernel` | caught | caught |
| X2 | the compact coverage label is swapped | `bidi_class_kernel` | caught | caught |
| X3 | a custom kernel claims the compact fallback class | `bidi_class_kernel` | caught | caught |
| X4 | the table-valid claim stops following the table check | `bidi_class_kernel` | caught | caught |

**Every fix was re-run against the mutant that motivated it** and is caught by a clean `FAIL:` line.

EB3 is worth naming because it pins a **relationship** rather than a value: the warn-once behaviour
is the log key's identity, so varying the key per code point turns one entry into two. That is the
only assertion in the suite that can see the dedup at all.

### Crash detections, kept separate

GD2 is caught only by **crash** (exit 134 on a call through a disengaged optional), not by a check.
It is a real detector but a weak one -- it shows the code is wrong, not that the suite understands
why -- so it is listed apart. No assertion was added for it: a crash is a sufficient kill, and an
assertion re-testing the same reachability would be padding.

### Equivalences

**None.** Both survivors turned out to be real holes once an input reached the differing branch, so
this audit found no mutation that is unobservable in principle. That is the opposite of the previous
textsegment audit, where one survived as a genuine equivalence -- worth stating because "survived"
is not by itself evidence of equivalence, and here it was not.

### A probe that lied, and how it was caught

The first check of E2 printed the levels for `HEBREW + RLO + "ab" + PDF + HEBREW` under both the
original and the mutated `nextOdd`, and they were **identical**. Read alone, that says "equivalent".
It is not: that fixture's downstream I1/I2 normalisation masks the parity difference. The
discriminating input is an embedding whose content differs in direction from the embedding, which
found `4` against `2`. The lesson is the one this project keeps relearning -- a single probe showing
no difference is not proof of equivalence, and `nextOdd`'s return being *used* (proved by mutating it
to a constant, which IS caught) has to be established separately from its *odd-forcing* being
observable.

### The wrapped-signature trap, handled

`explain_bidi_class_kernel`, `resolve_bidi_levels`, `get_compact_bidi_class` and `apply_explicit_levels`
all have signatures that wrap across lines in this tree. The applier anchors on a regex that spans
the wrap and locates the body from the preceding line start, so a wrapped signature reports correctly
rather than silently failing to match. After every reverted probe the target is **rebuilt before the
baseline is read**: a reverted source with a stale binary reports the old probe's result as the
baseline, which is how a passing tree gets reported as failing.

## State

Package unchanged: `assisted` at a627849, 8/8 headers compiling, 0 failing, 0 unattempted. The
`resolve_bidi_levels` override hashes clean against its `derivedFrom`. This arc closes coverage
holes; it does not move the tier.
