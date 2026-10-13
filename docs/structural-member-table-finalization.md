# Structural member table finalization

## Decision

Do not widen `structuralMemberTable` from the names the emitted SDK uses to every declared row
member. The measured widening is too expensive, and flattening source keys through one sanitized C++
spelling loses distinctions the structural proof must preserve. This decision does not change the table
or its renderer.

## Evidence

At commit `6369dc02`, the shared renderer reproduced the committed table byte-for-byte. Feeding it every
source-declared row name found across the committed generated and override headers changed the table as
follows:

| | Current | Naively widened | Change |
| --- | ---: | ---: | ---: |
| Row keys | 2,176 | 5,672 | +3,496 (+160.66%) |
| Lines | 8,786 | 22,770 | +13,984 (+159.16%) |
| Bytes | 1,202,168 | 3,164,836 | +1,962,668 (+163.26%) |

Five alternating uncached compilations of `tests/structural_row_test.cpp` used the repository's Zig
0.17.0 / Clang 22.1.8 compiler and identical flags. Median wall time rose from 18.011 seconds to 33.873
seconds (+88.06%). Median peak RSS rose from 910,456 KiB to 1,253,904 KiB (+343,448 KiB, or 335.40
MiB / 37.72%).

The inventory also found 85 sets of distinct source names that `safeCppMemberName` collapses to one C++
member. It found one source key with two committed C++ spellings: `texture2DByUnit` becomes
`texture2_dby_unit` in
`generated/include/flight/render_gl/gl_render_state_bracket.hpp`, while the shadowing override declares
`texture2_d_by_unit` in `overrides/include/flight/render_gl/gl_render_state_bracket.hpp`. The current
renderer deterministically selects the former, so a flat widened table cannot represent both declarations.

The complete mappings, collision list, per-run measurements, commands, and scratch hashes are in the
delivered **GEN structuralMemberTable widening inventory** report and its attached `inventory.json`. This
note records the resulting design decision rather than copying that evidence ledger into the repository.

## Future design

A future redesign must keep source-key identity separate from the sanitized C++ member spelling. Its
generated evidence must retain enough ownership or declaration context to prove which source key a member
represents instead of treating equal sanitized spellings as equal keys.

Generation must reject ambiguous evidence explicitly. In particular, it must refuse both a source key
that resolves to multiple C++ spellings and distinct source keys that collapse to one spelling in a domain
where the widening proof could confuse them. No permissive fallback, alias, or first-match rule may turn a
collision into a successful structural conversion.

## Standing rule

Keep the table use-driven: collect names from retained `RowKey` uses, plus the small explicit construction
seeds already maintained by the renderer. Do not add the full declared-member vocabulary until the
identity-preserving design exists, rejects the known collision families, and has compile-cost and semantic
coverage of its own.
