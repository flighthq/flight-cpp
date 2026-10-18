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

Final build, baseline parity and actual-source re-emission results will be recorded here before handoff.
Normal shared B generation and installed original GL/public registry qualification remain separate.
The allocation runtime is still withdrawn pending its independent full-suite gate and actual reland;
these independent compiler fixtures do not qualify that runtime change or the dependent GL API.
