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
