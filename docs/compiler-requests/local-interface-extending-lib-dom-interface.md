# A module-local interface that extends a lib.dom interface refuses its whole module

At Flight `a627849` and flight-compiler `806d257`, the interface-inheritance lowering pass refuses all of
`packages/scene2d-dom/src/domClipRectangle.ts` because of one private type:

```ts
interface DomClipStyle extends CSSStyleDeclaration {
  webkitClipPath: string;
}

(element.style as DomClipStyle).webkitClipPath = clipPath;
```

```
cpp-lowering-pass-refused: Compiler lowering pass interface-inheritance failed ...: interface DomClipStyle
inherits a nonlocal interface that cannot be structurally resolved
```

Because the whole module is refused, `generated/include/flight/scene2d_dom/dom_clip_rectangle.hpp` is a
best-effort placeholder. None of the module's eleven declarations exist, including `setDomClipHooks`,
`pushDomClipRectangle` and `applyDomClipRectangles`. Every importer then fails on those names; see
`flight/scene2d_dom/dom_clip.hpp:82`.

## Expected lowering

Resolve the base through the binding profile that already carries `CSSStyleDeclaration` as the host
`DomStyle`, and lower the added member as a member of that style owner. Failing that, refuse only the
declarations that use `DomClipStyle`, not the whole module. `setDomClipHooks`, `pushDomClipRectangle`,
`createDomScene2DRectangle` and the rectangle intersection math never touch the style and would emit on
their own.

Even then, the `webkitClipPath` write is the same element and style ownership problem as
[dom-style-shared-element-boundary](dom-style-shared-element-boundary.md). The SDL `HtmlElement` is a value
copy, so the write needs a shared provider before a clip can take effect. Neither a runtime field nor a
repair can stand in for this: a field would drop the write, and a declaration cannot restore a whole refused
module.

## Affected

- `flight/scene2d_dom/dom_clip.hpp`, cluster `undeclared-identifier-0168969a`. Its first failure is the
  separately refused `domScene2DClipHooks` binding, see
  [canvas-render-state-owner-asserted-without-heritage](canvas-render-state-owner-asserted-without-heritage.md#g2-dom-clip-hooks-assert-the-dom-render-state-from-its-base).
- `scene2d-dom` declares environment `web`, so the native SDL denominator is unchanged.
