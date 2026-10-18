# A numeric optional-chain result must retain its numeric payload

At Flight `a627849` / compiler `806d257`, `scene2d-formats/svgStyle.ts` computes selector specificity:

```ts
const ids = selector.match(/#[\w-]+/g)?.length ?? 0;
const classes = selector.match(/\.[\w-]+/g)?.length ?? 0;
const hasType = /^[a-zA-Z_][\w-]*/.test(selector) ? 1 : 0;
return ids * 100 + classes * 10 + hasType;
```

The emitted `get_css_selector_specificity` retains the nullable match guard and computes a double
length, but declares the optional-chain lambda as `optional<Any>`:

```cpp
const double ids = ([&]() -> std::optional<flight::Any> {
  auto receiver = selector.match(/* original global id regexp */);
  if (!receiver.has_value()) return std::nullopt;
  return static_cast<double>(receiver.value().size());
}()).value_or(0.0);
```

`value_or` therefore returns Any where `ids` requires double. `classes` repeats the same carrier
mistake. This is loss of the chain's numeric result, not missing match/length support and not an
unproved receiver: the emitted branch already checks the actual match and unwraps it correctly.
The exact result is `optional<double>` (or an equivalent numeric conditional), keeping absence until
the existing coalesce chooses 0. Do not add an implicit Any-to-number conversion for arbitrary values,
change regex matching, count one match instead of all matches, erase the no-match branch or replace
selector specificity with a constant. Preserve both regex counts, id/class/type weights, original
selector, evaluation order, numeric result and exceptions. No schema owner or array copy is needed.

The G2 SVG-color declaration arc measured both `svg_geometry_element.hpp` and `svg_gradient.hpp`
at this unchanged include, `svg_style.hpp:53`, after clearing their old missing color-parser name.
Final scoped original 3: **3 attempted / 1 pass / 2 fail / 0 unattempted** at one Clang 22.1.8 worker; `svg_color.hpp` passes. The existing
`no-viable-conversion-a21c880d` cluster gains those two failed rows, alongside its historical
`svg_style.hpp` row. Only the two new consumers were measured here; the historical style row was
not rerun or edited. Its diagnostic is `no viable conversion from 'flight::Any' to 'const double'`.

This request files the exact newly exposed boundary; no C++ or source rewrite for style is carried.
The SVG-color parser remains declaration-only, so no color parsing/painting or CSS/cascade execution
is implied by its header pass. Other inspected transform/gradient/geometry refusals are not asserted
as newly measured next errors. A future numeric-carrier fix must reproduce normal emission and verify
its owned selectors/consumers without changing the source's accepted domain.

## G3 original-header helper availability

At the same pins, the original `flight/scene2d_formats/svg_style.hpp` is now consumable for
`resolveSvgWinding` and `parseStyleDeclarations`, with the private selector scorer also exercised.
The public TypeScript declarations, native names and include path are unchanged. No leaf body was
copied to a new header and no replacement CSS cascade was supplied.

Three carrying patches and one declaration address the complete inclusion boundary:

- `scene2d-svg-selector-numeric-match-length` binds each original global-regexp match once and
  explicitly guards the match and its length before assigning the numeric counter. Null and
  undefined still select zero; zero and NaN remain values. Both regexps, id/class/type weights,
  evaluation order, property-read effects and exceptions are retained without copying a match.
- `scene2d-svg-declarations-string-prototype-key` preserves a source no-op at this particular
  fresh `{}` allocation: assigning its already-computed primitive string to the inherited
  `__proto__` setter creates no own data property. The native `Record.set` would otherwise store
  that key. The guard follows the original value computation and leaves all real writes intact;
  it is neither a general Record rule nor a new inherited-prototype lookup capability.
- `compiler-nullable-map-get-nullish-projection` keeps the original definition-style source
  unchanged. `parentByElement` is a **Map** with `XmlElement | null` values, represented as
  `Map<Ref<XmlElement>, optional<Ref<XmlElement>>>`. Its lookup adds an outer optional. The
  authored `get(element) ?? null` deliberately merges missing and stored-null results. The
  compiler now uses that actual carrier through `.value_or(std::nullopt)` only for a proven
  standard Map/ReadonlyMap nullable value and an exactly matching nullable destination. It
  retains one receiver/key evaluation and the original present owner; lookalike methods and
  variant-backed projections keep their existing paths. General lookup conversion is not fixed.
- `scene2d-formats-svg-declare-refused-style` declares only the exact three readonly-row argument
  carriers and original `Ref<SvgStyle>` result required by the retained definition-style call.
  The `resolveSvgStyle` body remains refused on its record-index present-payload boundary.
  `collectCssRules` remains refused on capture storage. Both NOT GENERATED markers remain.

Final normal scoped generation lowered **1467 modules / 30 packages**, selected the one style
module, and retained its partial header with **0 fully emitted / 1 refused** selected modules,
**259 applied source patches** and two applicable repair records. The full normal header and
three owned patch digests plus the exact declaration record are carried; global/package inventory
counts and other modules' refusal products were not rewritten. Earlier guard-only parent and
optional-result prototypes did not qualify and are not carried.

The owned original header check is **1 attempted / 1 pass / 0 fail / 0 unattempted**, Clang
22.1.8, jobs 2, **17.7 seconds**. The registered CMake/Bazel consumer includes this original
header and runs its emitted bodies: **16 winding, 25 selector and 10 declaration inputs**,
including spellings/fallbacks, empty/malformed/duplicate/colon-containing values, exact important
suffix handling, open names, ignored prototype-string assignment, numeric-key order, fresh result
storage and live result aliases. CMake build/link passes; CTest
`flight_cpp.generated_svg_style` is **1/1 PASS**, 0.01 seconds (0.03 seconds total).

The actual original/patched source oracle also passes those scalar/own-data cases and **294**
instrumented nullish match/length, read-order and exception controls. Four source-only parent/cache
scenarios retain the original source contract; they do not qualify the native cascade. Ten direct
compiler controls cover object/number/string/boolean Map and ReadonlyMap values plus non-Map
counterexamples. Five actually emitted native probe functions pass absent/null/present identity,
false/empty string/signed zero/NaN, single receiver/key evaluation order and exception identity.
The mechanism suite passes **31**, skips **1**, fails **0**. The compiler TypeScript build passes;
its local Vitest and Oxlint binaries are absent, and its check launcher is absent from
`.script-build`, so those upstream gates are not claimed as passed. No network setup was run.

A forced reference to the exact refused resolver compiles to an object and fails to link **only**
on `resolve_svg_style` with its three original readonly rows. This declaration supplies no style
resolution, definition traversal or cache behavior. No native XML import, CSS collection/cascade,
painting, geometry or gradient execution is claimed. The two older geometry/gradient include
rows were not rerun and remain recorded failures; the old numeric cluster retains those two
members. Historical G1 and prior color-check counts remain historical.

## Original cascade follow-on at compiler 806d257

The optional-map owner milestone makes the original public gradient lookup usable, but
an actual full parser consumer still fails to link both declaration-only
`resolve_svg_style` and `parse_svg_color`. The approved follow-on supplies their actual
source bodies through shared compiler corrections, rather than replacing the helpers.

The cascade needs [actual Record property deletion](record-computed-delete.md) and
[primitive lookup absence storage](record-missing-key-nullish-coalesce.md). One erased
assertion on the fresh local declarations dictionary states the per-key presence fact
between its own-key snapshot and its first read; no callback or escape intervenes.
The original inherit loop, every style property, selector filtering/specificity/order,
inline declarations, fresh parent copy and definition-style cache remain authored code.

The color parser needs two erased assertions: its successful anchored regexp always
has a participating second group, and its private RGB component callback is called only
with indices 0, 1 and 2 after the dense component array has length at least three.
The entire original color body, including hex, named, RGB/HSL, alpha, invalid strings and
numeric behavior, remains. Complete module JavaScript is byte-identical for both source
edits, also after the previously declared source patches.

A diagnostic lowering of the actual 1,467-module source graph emits the real style
resolver and the complete color module. `collectCssRules` retains its distinct capture
storage refusal. The dictionary correction also applies to original nested attribute
and gradient fallbacks; every affected actual helper must be re-emitted together.
The first partial diagnostic mixture linked but threw on an old missing-key helper,
so it is not a runtime qualification. Full native source-case checks, the ordered
source/compiler stack and normal scoped generation are in progress. Declaration-only
repairs will be retired only alongside their actual emitted definitions. No complete
SVG import, CSS collection, five-caller or package claim is made.
