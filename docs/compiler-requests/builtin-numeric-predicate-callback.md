# A numeric builtin used as a callback needs a callable value

At compiler `806d257d2f4100de745cc97340e61c2f301b0f24` / Flight
`a62784923f3be814463286c9fb28edfe6ab15789`, `colladaNumbers` in
`packages/scene3d-formats/src/colladaXml.ts` passes `Number.isFinite` to `Array.filter`.
The emitted `flight/scene3d_formats/collada_xml.hpp:44` passes `std::isfinite` directly.
That C++ name is an overload set, so the runtime cannot deduce the predicate type.

## Reproduction

```ts
export function finite(values: number[]): number[] {
  return values.filter(Number.isFinite);
}
```

The failing expression already present in the generated COLLADA owner is:

```cpp
optional_chain_receiver.value().filter(std::isfinite)
```

`Array::filter` takes a deduced predicate, invokes it with an element and, if supported,
its numeric index, and preserves the selected elements in order. An overloaded function
name alone has no type from which C++ can deduce that predicate. Adding a dummy or
number-specific overload to the general collection API would hide this value-lowering seam.

## Required shape

Materialize a callable at the source callback's actual element carrier, for example:

```cpp
values.filter([](double value) { return std::isfinite(value); })
```

For this numeric array the builtin ignores both callback index and source-array arguments;
the adapter must retain that behavior. This must also work for a retained readonly sequence
without copying or replacing its source owner. A nonnumeric or union input needs the actual
`Number.isFinite` rule: nonnumbers return false, without numeric coercion. The related
[union predicate request](closed-key-reads-and-union-number-isfinite.md) covers that separate
argument-lowering defect.

## Runtime work and remaining debt

G1 cluster `call-has-no-matching-47410fe2` originally stopped earlier, at
`map(flight::to_number)`, in seven SVG and nine COLLADA headers. The runtime now represents
`to_number` as a callable object, following `to_boolean`, with the same existing conversion
operations. This fixes `map(Number)` for Array and SequenceView without changing their
callback or owner contracts. Direct calls keep the same conversions.

The scoped `-j1` recheck completed all 16 headers: two SVG headers compile, nine COLLADA headers
fail at `filter(std::isfinite)`, and five SVG headers expose other compiler diagnostics.
The runtime build and `flight_cpp.runtime` semantic test pass (1/1).

The numeric predicate still names an overloaded function in `std`, which a runtime extension
cannot replace. This request remains open. No generated behavior, source rewrite, or override
is carried for it; independent parser refusals and other diagnostics exposed by the scoped
recheck remain failures in the header-status product.

## Native Collada public-header carry (2026-10-17)

The emitted-header consumability follow-up carries a declared override of the original public
`flight/scene3d_formats/collada_xml.hpp` path. Its generated counterpart remains byte-identical.
Only two executable expressions differ: the finite callback above becomes a typed double lambda,
and colladaIdOf returns the original attribute lookup optional instead of eagerly calling value().
The latter independently matters: the pinned missing-id case requires null, while the emitted
unwrap throws. A present empty id remains present. See the
[Record missing-key request](record-missing-key-nullish-coalesce.md#native-collada-id-read).

A callback-only source wrapper would leave that runtime failure. The existing Record request records
failed typed-local and Partial Record alternatives; an own-key-only source guard would narrow
inherited-property reads. Declaration repairs cannot change these executable operations. This
bounded override therefore retains the entire emitted module and changes only those two lowerings;
no replacement parser/helper, owner allocation, source patch, pin, generated SDK file or generic
Array/numeric policy is added. Its derivation digest and expiry cover both lowering defects. The
record is complete for compilation and adds no supplies/tier claim or package promotion.

The consumer includes the original public module path and real generated XML parser. It compiles,
links and runs **16 cases: all 14 pinned colladaXml.test.ts cases plus two edge-case groups, all nine
helpers**, in **1/1 CTest pass, 0.02 s** (0.03 s total). Native fixtures cover plain/prefixed names,
exact suffix boundaries, first-colon local names, missing/empty ids and live attribute ownership,
ordered child/descendant reference identity, trimmed/missing/empty text, parent-first traversal and
callback exception propagation. Numeric cases retain Boolean filtering before Number conversion,
finite rejection, fractions/exponents/hex, signed zero, and the source distinction between blank
colladaNumbers (empty) and blank parseColladaFloats ([0]). No duplicate helper implementation is
used by the test.

The literal original TypeScript test file also ran **14/14** through TypeScript5.9.3 erasure and a
Node-assert adapter for its Vitest matchers, loading the full unmodified Collada and XML modules.
Four additional original-source numeric assertions match the native edge cases. This is an explicit
source test adapter, not a claim that the upstream Vitest runner or the full package suite ran.

Changed original1 standalone header gate: **1 attempted / 1 pass / 0 fail / 0 unattempted, 8.5 s,
Clang22.1.8 jobs1**. Override consistency initially passed **229/229**; after the independent Spine carry landed,
the updated manifest passed **230/230** with the Collada native inputs unchanged. The defining header row becomes
PASS and the active predicate cluster retains **eight historical failed includers**, not recompiled
here; no current follow-on or gain is invented for them. Earlier 16-header/9-Collada measurements
above remain history, and both upstream lowering debts remain open. No complete COLLADA document
parser/package or fresh sweep is claimed. Evidence: `/tmp/builder4-collada/` (native build/CTest,
header report, literal source runner/report and override consistency).
