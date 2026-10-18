# RichTextData initialization on the held B floor

The held B Types output makes `RichTextData` inherit `TextLabelData`. The partial rich-text override put seven inherited fields in its derived aggregate initializer, which C++ rejects. The repair keeps one `RichTextData` allocation, initializes its own aggregate fields, and writes `entity_runtime_key`, `auto_size`, `height`, `text`, `text_format`, `vertical_align` and `width` on that same object. This is the compiler's existing own-field plus inherited-write form, also visible in the installed original SVG text caller. No compiler change, data/base copy, Types generation or SDK run is included.

Foreman also authorized correcting the concrete default bugs exposed by the original-source oracle. The default colors now match `0xffffffff` and `0x000000ff`, and `text_format` and `default_text_format` have distinct fresh owners. Default and represented nullish construction remain supported. Supplied options explicitly raise `OutOfProfileError` rather than being silently ignored. The native return signatures and the override's `partial` status remain unchanged.

The source-derived three-way inputs are retained in the qualification archive: old generated `rich_text.hpp` at `2eb70fb5` (`0222a241…`), current generated reference (`4f2b3f67…`), and the previous partial override (`e4b9df24…`). Builder2's delivered B provenance is head `5f79108f`, held and not landed, with seed `5f0c98ee` and pre-B floor `33777898`. Its nine delivered patches were applied only to a private scratch snapshot, producing an immutable 480-header delta. The three relevant type hashes match the earlier immutable 443-file B receipt exactly. This proof does not promote B or write generated files in the repository.

Nine cases run complete original factory and SVG text modules; all 1,285 loaded sources match raw Flight pin `a62784923f3be814463286c9fb28edfe6ab15789`. They measure defaults, options, member-read order, changing height/width getters, the original thrown error, source-authored range mapping, nullish defaults, the public node factory with its retained runtime, and the actual SVG text RichText branch. No source body or imported function is replaced.

| Operation | Original source | Current partial override |
| --- | --- | --- |
| Default fields and separate format owners | Source defaults and two fresh formats | Corrected defaults and distinct fresh formats |
| Supplied data/node options | Read and honored | Explicit `OutOfProfileError` |
| Option getter order/errors and range mapping | Ordered reads, original exceptions, fresh mapped ranges | Unimplemented; entry refuses supplied options |
| RichText runtime recovery | Same retained runtime on repeated reads | Existing weak registry loses its separately allocated runtime; recovery throws |
| Whole SVG rich-text consumer | Source rich branch runs | Native execution remains unqualified |

The runtime recovery failure was measured in the first public native consumer after a successful compile: it aborted with `RichText runtime is unavailable`. That failure is preserved. The final component fixture catches that specific error as an explicit partial boundary; it does not turn runtime recovery into a supported operation or allocate a substitute owner. The source oracle separately verifies the original retained runtime. All eleven previously omitted declarations remain listed in the manifest.

The actual installed SVG text header also retains `createSvgTextFormat` as `NOT GENERATED` with `cpp-optional-member-access-unproven`. Its dedicated consumer calls the real `svg_text_element_handler` without a replacement definition. A failed native compile keeps its run unqualified; the original-source SVG case cannot establish a native SVG text pass. The bounded B-coherence/default result is separate from both remaining boundaries.

The [qualification receipt](../../artifacts/rich-text-data-b-coherence.json) records nine original-source cases, 26 passing native component/default checks and 15 passing override automation tests. The final actual SVG consumer compile fails with seven retained production diagnostics, zero errors in the rich-text override and zero fixture errors; its native run is not performed. Builder2's older whole-SVG report is preserved as provenance, not used to claim a current whole-consumer pass or an inventory change.
