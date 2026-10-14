
## Re-audit (success-banner defect)

This suite's pass summary used to be printed **479 lines before the end**, so all 65 assertions below it --
the entire second half, including every case added by the filesystem continuation arc -- could fail while the
run still emitted `generated filesystem: all checks passed`. Any verification that grepped for that string,
which is what this project's own verification does, passed either way. **Those assertions had therefore never
actually been verified.** The banner is now the last statement.

### What re-verifying them found

A 22-mutation battery, each scoped to a single function body of the supplying override with the anchor
validated to hit exactly one site before its result was read. Twelve of the first fifteen caught; the three
outcomes that were not clean produced the findings:

* **THE SIGNAL-PRESENT FORWARDING BRANCH OF EVERY SIGNAL-TAKING OPERATION WAS UNEXERCISED.** Each body is a
  ternary -- `signal.has_value() ? f(path, data, signal) : f(path, data, nullopt)` -- and the suite took the
  `nullopt` arm almost everywhere. Swapping `appendTextFile`'s path and data on that branch, and dropping
  `readDirectory`'s signal on it, both SURVIVED the whole suite. A new block drives that branch for
  `appendTextFile`, `readTextFile`, `readDirectory`, `writeTextFile`, `writeFileAtomic` and
  `readBinaryFileRange`, comparing the signal by identity (`AbortSignal::operator==` compares the shared state,
  so a forwarded substitute is a different answer).
* **AND THE FIRST FIX FOR IT WAS ITSELF INCOMPLETE**: that block asserted the signal but not the other
  arguments, so the path/data swap still survived it. Measured, not assumed -- the mutation was re-run against
  the new block and survived again. Path and data are now recorded alongside the signal on that branch.
  `writeTextFile`'s branch had been caught only incidentally, through the dialog-handle case; it now has a
  direct assertion too.

Two mutation anchors were mis-targeted on the first attempt (the `stat_file` and `read_directory` default
expressions are aliased as `Result`, so my guessed text matched nothing). The harness reported `MIS-TARGETED`
rather than silently replacing nothing; both were re-run with corrected anchors and caught.

### Battery

| # | mutation | result |
| --- | --- | --- |
| F1 | base name returns the FIRST segment | caught |
| F2 | the extension drops its leading dot | caught |
| F3 | `fileExists` asks the directory operation | caught |
| F4 | `directoryExists` asks the file operation | caught |
| F5 | the access mode is replaced by a constant | caught |
| F6 | a hostless symlink reports success | caught |
| F7 | `removeDirectory` drops the recursive intent | caught |
| F8 | `copyFile` swaps its two paths | caught |
| F10 | an omitted `makeDirectory` reports success | caught |
| F13 | `readFileSymlink` invents a target | caught |
| F14 | a native-path handle reads through its display name | caught |
| F15 | the chunk is written without being copied first | caught |
| S4 | an omitted `statFile` invents a record | caught (after anchor correction) |
| S5 | an omitted `readDirectory` invents an entry | caught (after anchor correction) |
| S1 | append's signal branch swaps path and data | **caught only after the branch, then the arguments, were both recorded** |
| S2 | writeTextFile's signal branch swaps path and data | caught (now directly, not only via the dialog case) |
| S3 | `readDirectory`'s signal branch drops the signal | **caught only after the new block** |
| S6 | `readTextFile`'s signal branch drops the signal | caught by the new block |
| S7 | `writeFileAtomic`'s signal branch drops the signal | caught by the new block |
