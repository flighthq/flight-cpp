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
