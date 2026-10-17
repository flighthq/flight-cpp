# DOM style writes require a shared element and style boundary

G1 cluster `missing-member-c36ea2d4` contains three headers whose first reported error is
`no member named 'opacity' in 'flight::host_sdl::DomStyle'` at
`flight/scene2d_dom/dom_style.hpp:33`. At compiler `806d257` / Flight `a627849`, this is an
incomplete host binding boundary, rather than a missing numeric opacity operator.

## Source and declared applicability

`packages/scene2d-dom/src/domStyle.ts` writes the existing HTMLElement's style:

```ts
setDomTransform(element, node.transform2D, state.roundPixels);
element.style.opacity = node.alpha < 1 ? String(node.alpha) : '';
element.style.imageRendering = state.allowSmoothing ? '' : 'pixelated';
if (state.domCssFilterResolver !== null) element.style.filter = state.domCssFilterResolver(node) ?? '';
state.applyBlendMode?.(element, node.blendMode);
```

`prepareDomElement` also writes `position`, `left`, `top`, `transformOrigin` and `pointerEvents`.
The TypeScript DOM declaration types `CSSStyleDeclaration.opacity` as `string`. The source's
empty-string branch must remain distinct from a numeric default; it must preserve the existing
String conversion and the conditional filter/blend callbacks.

The package's pinned `package.json` explicitly declares `flight.environment: "web"`, and the
committed SDK manifest records `environment: "web"`. `deferred-packages.json` has an empty
`applicableEnvironments` list for the native SDL profile, so this package is already derived as
not applicable to that profile's shippable denominator. This is declared applicability, not an
inference from the package name. Its headers remain in the compile inventory and remain failing;
no applicability setting, deferral or denominator is changed by this request.

## Actual host carrier

`bindings/sdl-app.json` elects `HTMLElement` as shared ownership with target
`flight::host_sdl::HtmlElement`. The actual target is an alias to `DomElement`, whose `style` is
an inline `DomStyle` value. `DomStyle` contains plain `String` members for a limited set of
shell metadata, including `transform`, but no `opacity`, `image_rendering`, `filter`,
`transform_origin` or `pointer_events` fields. String itself stores a value-owned UTF-16 string.

The emitter calls the existing carrier by value:

```cpp
inline void set_dom_transform(flight::host_sdl::HtmlElement element, /* ... */) {
  element.style.transform = /* matrix string */;
}
```

`apply_dom_style` and `prepare_dom_element` also take that carrier by value. The plain style
members are copied with the element, so writing a style member in these helpers changes their
local copy rather than the caller's style. The shared event-listener collection does not make
those style strings shared.

This matches the documented host scope: `docs/host-sdl.md` describes lightweight document
attachment and a DOM value facade for the SDL GL application shell. The host source explicitly
does not materialize an HTML tree: `append_child` returns its argument, selector queries return
an empty snapshot, and canvas conversion copies the facade's style into the canvas handle.
This is not a retained DOM style/rendering provider for the web scene renderer.

## Required boundary and disposition

A usable web binding must retain the same element/style owner across the emitted value handles,
make writes observable through aliases and callbacks, and provide the renderer's declared style
and attachment surface. A compiler/host binding that passes actual element references is another
possible shape, provided the ownership and lifetime contract is explicit. This request does not
choose or implement that broader host capability.

Adding `String opacity` alone would let one assignment type-check while still writing only a
copied shell value. Adding numeric opacity, an always-opaque/default read, copied replacement
elements, a side table detached from the element, or a no-op DOM renderer would substitute
behavior. No property, binding election, source patch, generated body or renderer is added here.
The earlier transform field remains part of the existing shell; its presence does not prove that
these scene-style helpers preserve element mutation.

**G1 status retained: 0/3 pass, 3 fail; 0 unchanged header recompiles.** The owned headers are
`flight/scene2d_dom/dom_style.hpp`, `flight/scene2d_dom/dom_html_view.hpp` and
`flight/scene2d_dom/dom_text_label.hpp`. Their status rows now link this binding request and the
recorded first-error location. No C++ input changed, so no new native semantic result or next
compiler diagnostic is claimed. Text layout and the real Scene2D GL renderer are outside this scope.

## G2 DOM material blend writes use the same missing shared style boundary

At Flight `a627849` / compiler `806d257`, cluster `missing-member-1620083c` contains only
`scene2d_dom/dom_materials.hpp`. Its retained first failure is
`no member named 'mix_blend_mode' in 'flight::host_sdl::DomStyle'` at line 27. The actual source is:

```ts
export function applyDomBlendMode(element: HTMLElement, value: BlendMode | null): void {
  element.style.mixBlendMode = (value !== null ? DOM_BLEND_MODE[value] : null) ?? '';
}
```

The generated helper takes `flight::host_sdl::HtmlElement` by value and assigns
`element.style.mix_blend_mode`. `enableDomBlendModeSupport` installs that same function into the
original `DomRenderState.applyBlendMode`; the emitted callback also carries the element by value.
The state itself is the existing reference-backed `DomRenderState`, not a replacement material owner.
Adding one plain String field would still write the helper's copied style rather than the actual
rendered element. The host has no existing CSS compositing/style provider to bind this operation to;
its document facade and GL canvas conversion do not supply that capability.

The pinned source `BlendMode` and `AdvancedBlendMode` types are open string families, including vendor
keys, not closed enums. The blend table has 17 explicit keys: six fixed-function names and eleven
additional advanced names. `Add` maps to CSS `screen` and is classified **approximate**; `Normal` maps
to the present empty string; the other mapped CSS modes keep their source spelling and **exact**
classification. A null input, a missing vendor-key lookup or a null table entry reaches the source
empty-string fallback. Preserve absence separately from a present empty string before that fallback;
do not throw on an absent map entry, invent a vendor-mode realization, restrict the input to built-ins,
or upgrade the approximate mapping to exact. The fidelity helper indexes its separate map directly;
no unregistered-key fidelity fallback is supplied by the source.

A real provider must make this CSS string write observable through every alias and the installed
callback for the identical element/style owner. Preserve the source lookup and assignment evaluation
order and setter exceptions. A copied value facade, disconnected side table, fake material/compositor,
no-op style assignment or hardcoded blend result cannot implement that boundary.

`scene2d-dom/package.json` and the generated package record both explicitly declare environment `web`.
The native SDL profile's existing empty `applicableEnvironments` list therefore already derives this
package as not applicable to its shippable denominator. This audit changes no applicability, deferral,
package totals or compile denominator. The header stays in the inventory and its original **0/1 pass,
1 fail** diagnostic remains. No C++/binding/source/override/generated body changed; no generation,
unchanged native compile, style behavior executable or rendering test was run. The emitted nullish
lookup shape was inspected but no later compiler diagnostic or native behavior result is claimed.
