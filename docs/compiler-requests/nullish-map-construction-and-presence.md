# Nullish Map construction and single-read presence tests

The original `buildGlRenderRegistries` in Flight pin
`a62784923f3be814463286c9fb28edfe6ab15789` copies optional readonly Maps and uses
`options.passes == null ? null : [...options.passes]`. On the frozen 110-patch compiler floor,
Map constructor argument contextualization lost the optional Map carrier and emitted a C++ Map
constructor with an optional argument. It also missed the existing fresh-table clone rule. The
loose test of the dual-sentinel `passes` carrier emitted the property read twice, so a present
passes option was read three times instead of the authored two.

`compiler-nullish-map-construction-and-presence.patch` corrects these generic operations. A Map
argument remains at its declared storage type until construction consumes its nullish sentinels.
The constructor evaluates that argument once. Null or undefined produces a fresh empty Map; a
present Map produces a fresh shallow table using the existing clone operation. Existing entry and
callback owners, key identity, insertion order and lifetime survive. A source guard still performs
its own read and the constructor performs its separately authored read, including when a getter
changes the value between them. Initialized default parameters consume their caller-presence layer
before the Map's declared nullish carrier is inspected.

A loose dual-sentinel presence test binds its evaluated operand once before testing its null and
undefined alternatives. The conditional's selected branch still performs the separate read the
source wrote. Strict comparisons retain their existing lowering. No branch value is cached across
source reads, and exceptions escape at their original read or iteration position.

The correction changes no runtime APIs, type-owner election, structural relation, registry wrapper
or generated header body. Other collection constructors and iterable/range inputs retain their
existing lanes. A closed Map payload with nullish sentinels supplies the proof for the new lane;
an unrelated or mixed payload does not acquire that proof.

## Qualification

Compiler pin `806d257d2f4100de745cc97340e61c2f301b0f24`, ordered baseline110 at
`4a7dbfd4`; candidate111. The regression source strictly typechecks as TypeScript. Its compiler-emitted
native consumer exercises missing/default/null/undefined/present Map inputs, fresh tables and
arrays, live descriptor owners and lifetime, identity of reference keys, insertion order, source
getter counts, guarded second reads, changed getter values, same getter exceptions, both loose
predicate directions, and iterator failure before later option reads. Iterator controls use a
fixture external binding with the original iterable contract, rather than editing emitted code.

All 111 declared compiler patches apply from the raw pin. Reversing this patch restores the exact
110-patch backend and existing tests and removes its new fixture; reapplying reproduces all three
qualified files. The final patch has SHA-256
`a12042c5fadf164d0b25101c3f004cd46d99ba7051dbb345ef078ee0e979d3db`.
The production build passes, and the five focused files pass 15/15 with zero skipped. Baseline110
passes the thirteen existing focused cases and fails both new cases. A compiler mutant restoring only
the old presence rule still compiles with the Map fix, then fails the native getter-count assertion.

Four existing emitted-spelling assertions now require one captured member read and both sentinel
checks on that value. They fail on baseline110 and pass on the candidate. The broader backend
comparison selects 426 cases, including that fourth related assertion: the other 422 have identical
outcomes, 377 passes and 45 existing failures, including 18 timeouts. Another 1,150 cases are
unselected. Eight timing differences and one disk-affected case were rerun on both variants with
120-second limits; all nine pass on both. The candidate passes all thirteen selected rerun cases.
There are no new observed failures; the shared failures are not reported as successes.

An isolated source-only re-emission of the original GL module over the frozen 1,032 type IR modules
retains all 22 registry writes in source order and exactly the original option-read counts, twelve
fresh Map clone paths, and two reads of a present passes option. It is not normal SDK output. The
module still refuses teardown, and its isolated spread projection is not qualified against the shared
normal types, so this check removes no override and establishes no installed-header result.

The two source-patch automation tests pass. The additional package-scope automation aborted when its
fixture copies exhausted the 20 GB overlay. Its own failed 745 MB temporary tree was removed; no
package-scope pass is claimed.
Normal shared B generation and installed original GL/public registry qualification remain separate.
The allocation runtime is still withdrawn pending its independent full-suite gate and actual reland;
these independent compiler fixtures do not qualify that runtime change or the dependent GL API.


## Composition with the codec preparation

After rebasing onto the codec preparation at `92ecde29`, the compiler stack has 112 patches and the
Flight stack has 202. Every incoming declaration is retained, with only this compiler entry appended.
The registry patch hash remains unchanged. A fresh raw-pin ordered application and strict build pass;
the nineteen registry/related controls and sixteen codec source controls pass. Reversing and reapplying
this patch reproduces its three compiler files exactly while retaining the codec backend additions and
fixture. The broader baseline comparison above remains scoped to frozen110/111. This composition
check does not regenerate the SDK or qualify the installed GL surface.
