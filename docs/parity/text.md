# TEXT lane parity inventory

_Upstream readings taken at Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler `2b687eaf6b631e0fa4d7d6e18dad93b845f99633`, this checkout's pin (`dependencies.lock.json`)._

This is the Q2 parity inventory for the TEXT lane: for each package, the upstream Vitest tests, the
behaviors they assert, what local CTest already covers, and which behaviors are web-only. It exists
so Phase 1 lane work can be planned against a known list of upstream guarantees rather than
rediscovered package by package.

## Provenance and method

**Two revisions are in play, and they are not the same one.**

- The **upstream columns** (test files, asserted behavior groups, `it`/`expect` counts) are read from
  the pinned Flight checkout that `npm run rehydrate` materializes into the gitignored
  `.dependencies/`, at the **Q0 pin** in the header above. Upstream tests are the Vitest files at
  `.dependencies/flight/packages/<package>/src/*.test.ts`; the *Asserted behavior groups* column is
  the set of `describe(...)` titles in each file, which name the behaviors that file asserts.
- The **inventory columns** (source / emitted / refused modules) are read from the committed
  `generated/manifest.json` and `generated/refusals.json`. That tree was generated at the *previous*
  pin (`7e2fc7df`); the Q0 re-pin moved the lock but did not regenerate the tree, and regenerating it
  is out of scope for this document. Treat those three columns as the current committed inventory,
  not as a reading of the new pin.

The local-coverage columns come from the repository's own sources: which headers each `tests/*.cpp`
and `examples/**/*.cpp` includes, and which of those are registered as CTest cases in
`tests/CMakeLists.txt`, `tests/upstream_test_harness.hpp` and `examples/upstream/CMakeLists.txt`.

**No build was run and no whole-SDK operation was performed to produce this document** — it is read
from the pinned sources and the committed generated tree only.

## Lane definition

This lane covers these 13 packages:

`@flighthq/text`, `@flighthq/text-markup`, `@flighthq/textbidi`, `@flighthq/textinput`, `@flighthq/textlayout`, `@flighthq/textsegment`, `@flighthq/textshaper`, `@flighthq/font`, `@flighthq/font-formats`, `@flighthq/glyphatlas`, `@flighthq/bitmapfont`, `@flighthq/bitmapfont-formats`, `@flighthq/bitmaptext`.

The TEXT lane is disjoint from the GL+2D lane at this granularity, and every package in it declares
no `flight.environment`, so all of it is environment-agnostic. `@flighthq/textshaper-canvas` — the
web Canvas text-shaper backend, the one web-only package this lane used to contain — **no longer
exists upstream** at the Q0 pin (the package was removed); the committed generated tree still
carries its headers because it predates the re-pin, so it will disappear when the tree is
regenerated.

## Summary

| Package | src | emitted | refused | upstream files | upstream `it` | local CTest | web-only |
| --- | ---: | ---: | ---: | ---: | ---: | :-- | :-- |
| `text` | 9 | 4 | 5 | 7 | 213 | none |  |
| `text-markup` | 8 | 6 | 2 | 6 | 68 | none |  |
| `textbidi` | 8 | 7 | 1 | 7 | 57 | none |  |
| `textinput` | 6 | 5 | 1 | 4 | 150 | none |  |
| `textlayout` | 14 | 12 | 2 | 12 | 156 | none |  |
| `textsegment` | 7 | 5 | 2 | 6 | 40 | none |  |
| `textshaper` | 10 | 9 | 1 | 8 | 98 | yes |  |
| `font` | 9 | 8 | 1 | 7 | 63 | yes |  |
| `font-formats` | 17 | 15 | 2 | 15 | 219 | yes |  |
| `glyphatlas` | 10 | 7 | 3 | 8 | 63 | none |  |
| `bitmapfont` | 8 | 7 | 1 | 6 | 50 | none |  |
| `bitmapfont-formats` | 8 | 6 | 2 | 8 | 90 | none |  |
| `bitmaptext` | 6 | 3 | 3 | 4 | 81 | none |  |
| **total** | | | | **98** | **1348** | | |

## Per-package inventory

### @flighthq/text

Inventory: 9 source modules — 4 emitted, 5 refused.

Upstream tests: 7 files, 213 `it` cases, 387 `expect` assertions.

| Upstream test file | it | Asserted behavior groups | Web-only |
| --- | ---: | --- | :--: |
| `src/enableTextLabelGuards.test.ts` | 4 | disableTextLabelGuards · enableTextLabelGuards |  |
| `src/explainTextLabelContent.test.ts` | 4 | explainTextLabelContent |  |
| `src/nativeText.test.ts` | 29 | computeNativeTextLocalBoundsRectangle · createNativeText · createNativeTextData · createNativeTextRuntime · getNativeTextMeasuredHeight · getNativeTextMeasuredWidth · getNativeTextRuntime · getNativeTextString · getNativeTextStyle · initializeNativeTextData · patchNativeTextStyle · setNativeTextAutoSize · setNativeTextHeight · setNativeTextString · setNativeTextStyle · setNativeTextVerticalAlign · setNativeTextWidth |  |
| `src/richText.test.ts` | 130 | appendRichTextString · buildRichTextLayoutParams · clearRichTextFormatRanges · computeRichTextLocalBoundsRectangle · createRichText · createRichTextData · createRichTextRuntime · createTextFieldSignals · dispatchRichTextLinkAtPoint · dispatchRichTextWheel · enableTextFieldSignals · getRichTextBottomScrollV · getRichTextCharIndexAtPoint · getRichTextDefaultTextFormat · getRichTextFormatRangeAt · getRichTextFormatRangeByIndex · getRichTextFormatRangeCount · getRichTextFormatRangesIn · getRichTextLength · getRichTextLineCount · getRichTextLineMetrics · getRichTextMaxScrollH · getRichTextMaxScrollV · getRichTextPasswordCharacter · getRichTextRuntime · getRichTextString · getRichTextTextHeight · getRichTextTextWidth · getTextFieldSignals · initializeTextFieldSignals · insertRichTextString · removeRichTextFormatRangesIn · replaceRichTextString · setRichTextBackground · setRichTextBackgroundColor · setRichTextBorder · setRichTextBorderColor · setRichTextCondenseWhite · setRichTextContent · setRichTextDefaultTextFormat · setRichTextFormatRange · setRichTextHeight · setRichTextMaxChars · setRichTextMouseWheelEnabled · setRichTextMultiline · setRichTextScrollH · setRichTextScrollV · setRichTextSelectable · setRichTextString · setRichTextString no-op writes · setRichTextTextColor · setRichTextVerticalAlign · setRichTextWidth · setRichTextWordWrap |  |
| `src/textFormatFont.test.ts` | 5 | computeTextFormatFontString |  |
| `src/textLabel.test.ts` | 31 | appendTextLabelString · computeTextLabelLocalBoundsRectangle · createTextLabel · createTextLabelData · createTextLabelRuntime · getTextLabelFormat · getTextLabelRuntime · getTextLabelString · initializeTextLabelData · setTextLabelAutoSize · setTextLabelFormat · setTextLabelHeight · setTextLabelString · setTextLabelVerticalAlign · setTextLabelWidth |  |
| `src/textLabelLayout.test.ts` | 10 | ensureTextLayout · getTextLayout · getTextLayoutMetrics · setTextLabelGuard |  |

Local CTest coverage: **none** — no local test or example includes this package’s headers.

Web-only exclusions:
  - None identified for this package.

### @flighthq/text-markup

Inventory: 8 source modules — 6 emitted, 2 refused.

Upstream tests: 6 files, 68 `it` cases, 123 `expect` assertions.

| Upstream test file | it | Asserted behavior groups | Web-only |
| --- | ---: | --- | :--: |
| `src/enableTextMarkupGuards.test.ts` | 4 | disableTextMarkupGuards · enableTextMarkupGuards |  |
| `src/markupClassStyles.test.ts` | 5 | registerMarkupClassStyles |  |
| `src/markupNamedColors.test.ts` | 4 | registerMarkupNamedColors |  |
| `src/markupTagRegistry.test.ts` | 13 | createMarkupTagRegistry · registerMarkupTag · registerStandardMarkupTags · resolveMarkupHexColor |  |
| `src/textMarkup.test.ts` | 40 | explainTextMarkup · formatTextMarkup · parseTextMarkup · textMarkupRoundTrip |  |
| `src/textMarkupGuards.test.ts` | 2 | reportTextMarkupIssue · setTextMarkupGuard |  |

Local CTest coverage: **none** — no local test or example includes this package’s headers.

Web-only exclusions:
  - None identified for this package.

### @flighthq/textbidi

Inventory: 8 source modules — 7 emitted, 1 refused.

Upstream tests: 7 files, 57 `it` cases, 79 `expect` assertions.

| Upstream test file | it | Asserted behavior groups | Web-only |
| --- | ---: | --- | :--: |
| `src/bidiClassKernel.test.ts` | 6 | compactBidiClassKernel · createCompactBidiClassKernel · explainBidiClassKernel · initializeCompactBidiClassKernel |  |
| `src/bidiConformance.test.ts` | 3 | bidi conformance fixtures |  |
| `src/enableTextBidiGuards.test.ts` | 4 | disableTextBidiGuards · enableTextBidiGuards |  |
| `src/getBidiRuns.test.ts` | 9 | getBidiRuns |  |
| `src/reorderBidiLine.test.ts` | 6 | reorderBidiLine |  |
| `src/resolveBidiLevels.test.ts` | 27 | resolveBidiLevels |  |
| `src/textBidiGuards.test.ts` | 2 | reportTextBidiCompactTableMiss · setTextBidiGuard |  |

Local CTest coverage: **none** — no local test or example includes this package’s headers.

Web-only exclusions:
  - None identified for this package.

### @flighthq/textinput

Inventory: 6 source modules — 5 emitted, 1 refused.

Upstream tests: 4 files, 150 `it` cases, 230 `expect` assertions.

| Upstream test file | it | Asserted behavior groups | Web-only |
| --- | ---: | --- | :--: |
| `src/selectableRichTextManager.test.ts` | 18 | blurSelectableRichText · createSelectableRichTextManager · dispatchSelectableRichTextKeyDown · dispatchSelectableRichTextPointerDown · dispatchSelectableRichTextPointerMove · dispatchSelectableRichTextWheel · focusSelectableRichText · getSelectableRichTextSelectionText · initializeSelectableRichTextManager |  |
| `src/textInput.test.ts` | 7 | disableTextInput · enableTextInput · getTextInputState · hasTextInput |  |
| `src/textInputEditing.test.ts` | 105 | appendTextInput · applyTextInputRestriction · canRedoTextInput · canUndoTextInput · clearTextInputHistory · deleteTextInputBackward · deleteTextInputForward · deleteTextInputWordBackward · deleteTextInputWordForward · getTextInputCaretIndex · getTextInputCaretRectangle · getTextInputCharacterIndexAtPoint · getTextInputDisplayText · getTextInputSelectionBeginIndex · getTextInputSelectionEndIndex · getTextInputSelectionRectangles · getTextInputSelectionText · handleTextInputKeyboard · insertTextInput · moveTextInputCaret · moveTextInputCaretByWord · moveTextInputCaretDown · moveTextInputCaretToLineEnd · moveTextInputCaretToLineStart · moveTextInputCaretUp · redoTextInput · replaceSelectedTextInput · replaceTextInput · scrollTextInputCaretIntoView · selectAllTextInput · selectLineAtTextInputIndex · selectWordAtTextInputIndex · setTextInputSelection · undoTextInput |  |
| `src/textInputManager.test.ts` | 20 | blurTextInput · connectInputToTextInput · createTextInputManager · dispatchTextInput · dispatchTextInputKeyDown · dispatchTextInputPointerDown · dispatchTextInputPointerMove · dispatchTextInputWheel · focusTextInput · initializeTextInputManager |  |

Local CTest coverage: **none** — no local test or example includes this package’s headers.

Web-only exclusions:
  - None identified for this package.

### @flighthq/textlayout

Inventory: 14 source modules — 12 emitted, 2 refused.

Upstream tests: 12 files, 156 `it` cases, 234 `expect` assertions.

| Upstream test file | it | Asserted behavior groups | Web-only |
| --- | ---: | --- | :--: |
| `src/richTextContent.test.ts` | 12 | clearRichTextContent · computeRichTextContent · createRichTextContent · getRichTextContent · initializeRichTextContent |  |
| `src/richTextMetrics.test.ts` | 12 | computeRichTextBottomScrollV · computeRichTextLineCount · computeRichTextMaxScrollH · computeRichTextMaxScrollV · computeRichTextTextHeight · computeRichTextTextWidth · getRichTextScrollYOffset |  |
| `src/richTextQuery.test.ts` | 23 | computeRichTextCharIndexAtPoint · computeRichTextLineMetrics · getRichTextCharBoundaries · getRichTextFirstCharInParagraph · getRichTextLineIndexAtPoint · getRichTextLineIndexOfChar · getRichTextLineLength · getRichTextLineOffset · getRichTextLineText · getRichTextLinkAtPoint · getRichTextParagraphLength · getRichTextSelectionRectangles |  |
| `src/textBounds.test.ts` | 12 | computeTextBoundsHeight · computeTextBoundsOffsetX · computeTextBoundsRectangle · computeTextBoundsWidth · TEXT_BOUNDS_GUTTER |  |
| `src/textFormat.test.ts` | 11 | getTextFormatAscent · getTextFormatDescent · getTextFormatHeight · getTextFormatLeading · mergeTextFormat |  |
| `src/textFormatRange.test.ts` | 2 | createTextFormatRange · initializeTextFormatRange |  |
| `src/textLayout.test.ts` | 57 | computeTextLayout · center alignment · empty input · explicit line breaks (multiline) · line metrics · multiple format ranges · right alignment · single line · vertical alignment · word wrap · computeTextLayout — bullet list items · computeTextLayout — codepoint iteration · computeTextLayout — conformance: bullet listMarker:none suppresses glyph · computeTextLayout — conformance: center alignment golden values · computeTextLayout — conformance: justify multi-paragraph · computeTextLayout — conformance: right alignment golden values · computeTextLayout — conformance: truncation + word-wrap combined · computeTextLayout — justify alignment · computeTextLayout — justify single-format · computeTextLayout — kerning flag · computeTextLayout — maxLines truncation · computeTextLayout — start/end alignment · createTextLayoutResult · initializeTextLayoutResult · isTextLayoutTruncated · TEXT_LAYOUT_GUTTER |  |
| `src/textLayoutGroup.test.ts` | 4 | createTextLayoutGroup · initializeTextLayoutGroup |  |
| `src/textLayoutMeasure.test.ts` | 4 | getTextLayoutMeasureProvider · setTextLayoutMeasureProvider |  |
| `src/textLayoutRuntime.test.ts` | 3 | clearTextLayoutResult · getTextLayoutResult |  |
| `src/textLineBreaks.test.ts` | 13 | getTextLineBreakIndex · getTextLineBreaks |  |
| `src/textMetrics.test.ts` | 3 | createTextMetrics · getTextMetrics · initializeTextMetrics |  |

Local CTest coverage: **none** — no local test or example includes this package’s headers.

Web-only exclusions:
  - None identified for this package.

### @flighthq/textsegment

Inventory: 7 source modules — 5 emitted, 2 refused.

Upstream tests: 6 files, 40 `it` cases, 53 `expect` assertions.

| Upstream test file | it | Asserted behavior groups | Web-only |
| --- | ---: | --- | :--: |
| `src/enableTextSegmentGuards.test.ts` | 3 | disableTextSegmentGuards · enableTextSegmentGuards |  |
| `src/textSegment.test.ts` | 6 | segmentGraphemes · segmentSentences · segmentWords |  |
| `src/textSegmentBoundary.test.ts` | 21 | getNextGraphemeBoundary · getNextSentenceBoundary · getNextWordBoundary · getPreviousGraphemeBoundary · getPreviousSentenceBoundary · getPreviousWordBoundary · getWordRangeAt |  |
| `src/textSegmentConformance.test.ts` | 1 | text segment conformance fixtures |  |
| `src/textSegmentGuards.test.ts` | 2 | reportTextSegmenterUnavailable · setTextSegmentGuard |  |
| `src/textSegmenterBackend.test.ts` | 7 | explainTextSegmenterBackend · webTextSegmenterBackend |  |

Local CTest coverage: **none** — no local test or example includes this package’s headers.

Web-only exclusions:
  - None identified for this package.

### @flighthq/textshaper

Inventory: 10 source modules — 9 emitted, 1 refused.

Upstream tests: 8 files, 98 `it` cases, 173 `expect` assertions.

| Upstream test file | it | Asserted behavior groups | Web-only |
| --- | ---: | --- | :--: |
| `src/enableTextShaperGuards.test.ts` | 3 | disableTextShaperGuards · enableTextShaperGuards |  |
| `src/textShaper.test.ts` | 5 | explainTextShaperOperation · hasTextShaperOperation · measureText |  |
| `src/textShaperCache.test.ts` | 15 | clearTextShaperCache · createTextShaperCache · disposeTextShaperCache · initializeTextShaperCache · shapeTextRunCached |  |
| `src/textShaperCluster.test.ts` | 12 | getCaretPositionsForRun · getClusterForIndex · getIndexRangeForCluster |  |
| `src/textShaperItemize.test.ts` | 11 | itemizeText · shapeTextRuns |  |
| `src/textShaperPool.test.ts` | 9 | acquireShapedRun · releaseShapedRun · setShapedRunReleaseGuard |  |
| `src/textShaperRun.test.ts` | 36 | clearShapedRun · createShapedRun · getCodePointForGlyph · getFontMetrics · getFontMetricsInto · getFontUnitScale · getGlyphExtents · getGlyphExtentsBatch · getGlyphExtentsInto · getGlyphIndexForCodePoint · getGlyphName · initializeShapedRun · shapeTextRun · shapeTextRunInto |  |
| `src/textShaperSignals.test.ts` | 7 | disposeTextShaperSignals · enableTextShaperSignals · getTextShaperSignals · initializeTextShaperSignals |  |

Local CTest coverage: `tests/generated_textshaper_cache_test.cpp` (flight_cpp.generated_textshaper_cache)

Web-only exclusions:
  - None identified for this package.

### @flighthq/font

Inventory: 9 source modules — 8 emitted, 1 refused.

Upstream tests: 7 files, 63 `it` cases, 102 `expect` assertions.

| Upstream test file | it | Asserted behavior groups | Web-only |
| --- | ---: | --- | :--: |
| `src/_fontFaceLoad.test.ts` | 5 | _loadFontFaceFromBytes · _loadFontFaceFromUrl · _loadFontFaceFromUrls · _loadFontFacesFromName |  |
| `src/fontFormat.test.ts` | 20 | detectFontFormat · inferFontFormatFromUrl |  |
| `src/fontResource.test.ts` | 4 | createFontResource · initializeFontResource |  |
| `src/fontResourceFrom.test.ts` | 19 | loadFontResourceFromBytes · loadFontResourceFromName · loadFontResourceFromUrl · loadFontResourceFromUrls |  |
| `src/fontShorthand.test.ts` | 5 | getFontShorthand |  |
| `src/fontStatus.test.ts` | 5 | isFontLoaded · whenFontsReady |  |
| `src/glyphOutlineSource.test.ts` | 5 | allocateGlyphRasterizerBackendFromGlyphOutlineSource · initializeGlyphRasterizerBackendFromGlyphOutlineSource |  |

Local CTest coverage: `examples/upstream/sdl_host.cpp` — linked into every `flight_cpp_upstream_*` example; glyph outlines rasterized at runtime (exercised, not asserted)

Web-only exclusions:
  - None identified for this package.

### @flighthq/font-formats

Inventory: 17 source modules — 15 emitted, 2 refused.

Upstream tests: 15 files, 219 `it` cases, 391 `expect` assertions.

| Upstream test file | it | Asserted behavior groups | Web-only |
| --- | ---: | --- | :--: |
| `src/cffCharstring.test.ts` | 17 | cffSubroutineBias · runCffCharstring |  |
| `src/cffDict.test.ts` | 10 | readCffDict |  |
| `src/cffFdSelect.test.ts` | 7 | readCffFdSelect |  |
| `src/cffIndex.test.ts` | 10 | readCffIndex |  |
| `src/cffTable.test.ts` | 8 | readCffTable · readCffTable — CID-keyed |  |
| `src/openTypeCmap.test.ts` | 11 | findOpenTypeUnicodeSubtable · rankOpenTypeUnicodeEncoding · readOpenTypeCodepointMap |  |
| `src/openTypeGlyf.test.ts` | 12 | readOpenTypeGlyphOutline · readOpenTypeGlyphRanges |  |
| `src/openTypeGlyfWinding.test.ts` | 3 | openTypeGlyfWinding |  |
| `src/openTypeGlyphOutlineSource.test.ts` | 27 | createGlyphOutlineSourceFromOpenTypeFont · explainOpenTypeFont · initializeGlyphOutlineSourceFromOpenTypeFont |  |
| `src/openTypeMetrics.test.ts` | 12 | readOpenTypeAdvances · readOpenTypeGlyphCount · readOpenTypeLocaFormat · readOpenTypeMetrics |  |
| `src/sfntAssembly.test.ts` | 28 | assembleSfntFont · computeSfntTableChecksum · encodeSfntCompositeGlyph · encodeSfntLoca · encodeSfntSimpleGlyph · packSfntTag |  |
| `src/sfntTableDirectory.test.ts` | 9 | readSfntTableDirectory · readSfntTag |  |
| `src/woff2Font.test.ts` | 15 | readWoff2Font · readWoff2TableDirectory · WOFF2_COMPRESSION |  |
| `src/woff2GlyfTransform.test.ts` | 35 | createWoff2TransformReverser · decodeWoff2Triplet · getWoff2BboxBitmapByteLength · hasWoff2GlyphBbox · isWoff2PointOnCurve · measureWoff2CompositeGlyph · readWoff2GlyfStreams · readWoff2Short · reverseWoff2GlyfTransform |  |
| `src/woffFont.test.ts` | 15 | readWoffChecksumMismatches · readWoffFont |  |

Local CTest coverage: `examples/upstream/sdl_host.cpp` — reads sfnt directory, loca, metrics, advances, cmap and glyf outlines for the example font (exercised, not asserted)

Web-only exclusions:
  - None identified for this package.

### @flighthq/glyphatlas

Inventory: 10 source modules — 7 emitted, 3 refused.

Upstream tests: 8 files, 63 `it` cases, 141 `expect` assertions.

| Upstream test file | it | Asserted behavior groups | Web-only |
| --- | ---: | --- | :--: |
| `src/enableGlyphAtlasGuards.test.ts` | 6 | disableGlyphAtlasGuards · enableGlyphAtlasGuards |  |
| `src/explainGlyphAtlasEntry.test.ts` | 5 | explainGlyphAtlasEntry |  |
| `src/glyphAtlas.test.ts` | 11 | createGlyphAtlas · createGlyphAtlas font style and weight · deriveGlyphMetricsFromFontSize · disposeGlyphAtlas · getGlyphAtlasBitmap · getGlyphAtlasLayoutVersion · initializeGlyphAtlas |  |
| `src/glyphAtlasDirty.test.ts` | 4 | clearGlyphAtlasDirty · getGlyphAtlasDirtyRegion |  |
| `src/glyphAtlasEntry.test.ts` | 23 | getGlyphAtlasEntry · repack layout version · retained-byte and area budgets · setGlyphAtlasEntryGuard |  |
| `src/glyphAtlasMetrics.test.ts` | 5 | getGlyphAtlasKerning · getGlyphAtlasMetrics · getGlyphAtlasMetrics from a measuring backend |  |
| `src/glyphRasterizerBackend.test.ts` | 5 | allocateStubGlyphRasterizerBackend · initializeStubGlyphRasterizerBackend |  |
| `src/glyphSource.test.ts` | 4 | createGlyphSourceFromGlyphAtlas · initializeGlyphSourceFromGlyphAtlas |  |

Local CTest coverage: **none** — no local test or example includes this package’s headers.

Web-only exclusions:
  - None identified for this package.

### @flighthq/bitmapfont

Inventory: 8 source modules — 7 emitted, 1 refused.

Upstream tests: 6 files, 50 `it` cases, 92 `expect` assertions.

| Upstream test file | it | Asserted behavior groups | Web-only |
| --- | ---: | --- | :--: |
| `src/bitmapFont.test.ts` | 26 | createBitmapFont · getBitmapFontGlyph · getBitmapFontKerning · getBitmapFontKerning supplementary planes · getBitmapFontMetrics · getBitmapFontPage · getBitmapFontPages · hasBitmapFontGlyph · initializeBitmapFont · packBitmapFontKerningKey · setBitmapFontGuard · unpackBitmapFontKerningKey |  |
| `src/bitmapFontFromGlyphAtlas.test.ts` | 5 | createBitmapFontFromGlyphAtlas |  |
| `src/bitmapFontGlyphSource.test.ts` | 5 | createGlyphSourceFromBitmapFont · initializeGlyphSourceFromBitmapFont |  |
| `src/enableBitmapFontGuards.test.ts` | 4 | disableBitmapFontGuards · enableBitmapFontGuards |  |
| `src/explainBitmapFontGlyph.test.ts` | 5 | explainBitmapFontGlyph |  |
| `src/summarizeBitmapFont.test.ts` | 5 | summarizeBitmapFont |  |

Local CTest coverage: **none** — no local test or example includes this package’s headers.

Web-only exclusions:
  - None identified for this package.

### @flighthq/bitmapfont-formats

Inventory: 8 source modules — 6 emitted, 2 refused.

Upstream tests: 8 files, 90 `it` cases, 210 `expect` assertions.

| Upstream test file | it | Asserted behavior groups | Web-only |
| --- | ---: | --- | :--: |
| `src/bitmapFontBinary.test.ts` | 11 | parseBitmapFontBinary |  |
| `src/bitmapFontDetect.test.ts` | 24 | applyBitmapFontImportOptions · bitmapFontAllFormats · bmFontBinaryFormat · bmFontJsonFormat · bmFontTextFormat · bmFontXmlFormat · detectBitmapFontFormat · getBitmapFontFormat · getBitmapFontFormatKinds · parseBitmapFont · registerBitmapFontFormat · unregisterBitmapFontFormat |  |
| `src/bitmapFontFnt.test.ts` | 13 | formatBitmapFontFnt · parseBitmapFontFnt · parseBitmapFontFntDroppedRecords |  |
| `src/bitmapFontFormatKind.test.ts` | 7 | readBitmapFontFormatKind |  |
| `src/bitmapFontJson.test.ts` | 7 | parseBitmapFontJson |  |
| `src/bitmapFontRecord.test.ts` | 9 | buildBitmapFontFromRecord · reportDroppedBitmapFontRecords |  |
| `src/bitmapFontXml.test.ts` | 5 | parseBitmapFontXml |  |
| `src/explainBitmapFontParse.test.ts` | 14 | explainBitmapFontParse |  |

Local CTest coverage: **none** — no local test or example includes this package’s headers.

Web-only exclusions:
  - None identified for this package.

### @flighthq/bitmaptext

Inventory: 6 source modules — 3 emitted, 3 refused.

Upstream tests: 4 files, 81 `it` cases, 187 `expect` assertions.

| Upstream test file | it | Asserted behavior groups | Web-only |
| --- | ---: | --- | :--: |
| `src/bitmapText.test.ts` | 32 | computeBitmapTextLocalBoundsRectangle · createBitmapText · createBitmapTextData · createBitmapTextRuntime · getBitmapTextBounds · getBitmapTextLineCount · getBitmapTextPages · initializeBitmapTextData · isBitmapTextGlyphLayoutStale · isBitmapTextTruncated · reserveBitmapText · setBitmapTextAlign · setBitmapTextEllipsis · setBitmapTextGlyphSource · setBitmapTextLetterSpacing · setBitmapTextLineHeight · setBitmapTextMaxLines · setBitmapTextText · setBitmapTextWrapWidth |  |
| `src/enableBitmapTextGuards.test.ts` | 6 | disableBitmapTextGuards · enableBitmapTextGuards |  |
| `src/explainBitmapTextMissingGlyphs.test.ts` | 5 | explainBitmapTextMissingGlyphs |  |
| `src/updateBitmapText.test.ts` | 38 | refreshBitmapTextGlyphLayout · setBitmapTextLayoutGuard · setBitmapTextMissingGlyphGuard · updateBitmapText |  |

Local CTest coverage: **none** — no local test or example includes this package’s headers.

Web-only exclusions:
  - None identified for this package.

## Packages adjacent to this lane but out of scope

Owned by other lanes and deliberately not inventoried here: `textureatlas`, `textureatlas-formats`
(atlas resource plumbing, GL+2D), `layout` (UI layout, core), `image` / `image-codec` (core),
`render`, `scene2d*`, `gui`. `font-formats` and `font` are inventoried here even though the GL/2D
examples consume them through the SDL host.
