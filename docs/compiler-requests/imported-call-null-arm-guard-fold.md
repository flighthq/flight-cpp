# An imported call's nullable local folds its null test to `true`

A local whose initializer is `condition ? importedCall() : null` is emitted as a *nullable reference*
rather than the `std::optional` the same-module form produces. The `x !== null` guard that follows is
then folded to a constant `true`, so the guarded member read compiles unconditionally and dereferences
the absent reference.

Measured with the pinned compiler `806d257` plus the declared compiler patch stack, through the
compiler-backend fixture in the reproduction below.

## Minimal TypeScript

```ts
// widget.ts
interface Widget { value: number }
export function makePresent(): Widget { return { value: 1 }; }

// consumer.ts
import { makePresent } from './widget.ts';
export function pick(cond: boolean): number {
  const widget = cond ? makePresent() : null;
  return widget !== null ? widget.value : -1;
}
```

## Expected C++

The shape the same-module call already produces:

```cpp
std::optional<flight::Ref<Widget>> widget =
    (cond ? std::optional<flight::Ref<Widget>>{make_present()} : std::nullopt);
return (widget.has_value() ? widget.value()->value : -1.0);
```

## Observed C++

```cpp
auto widget = (cond ? flighthq_probe::make_present() : nullptr);
return (true ? widget->value : -1.0);
```

`widget` is a null-capable `Ref<Widget>` holding `nullptr` on the absent arm, and the null test is folded
to `true`, so the member read runs on `nullptr`.

## Scope: only the cross-module call form

The same fixture emits the correct optional form for every same-module shape:

| shape | emitted local | guard |
| --- | --- | --- |
| same-module call with a null arm | `std::optional<Ref<Widget>>` | `has_value()` |
| annotated `Widget \| null` | `std::optional` via `auto` | `has_value()` |
| loop-scoped same-module call | `std::optional<Ref<Widget>>` | `has_value()` |
| direct nullable call result | `auto` (optional) | `has_value()` |
| **imported call with a null arm** | nullable `Ref` | **`true`** |

The distinguishing factor is that the callee is declared in another module: its result type is resolved
through the module-resolution plan, where the union's null arm is represented as a null `Ref` instead of
an absent `std::optional`.

## Witness at a real site

The SWF dual-sentinel source correction first tried locally:

```ts
const colorTransform = body.pos < body.end ? readSwfColorTransform(body, 3) : null;
const colorAdjustments = colorTransform !== null ? colorTransform.colorAdjustments : null;
```

where `readSwfColorTransform` is imported from `swfAppearance.ts`. The emitted handler read

```cpp
auto color_transform = ((body->pos < body->end) ? read_swf_color_transform(body, 3.0) : nullptr);
std::optional<flight::Array<flight::Ref<flight::types::Adjustment>>> color_adjustments =
    (true ? std::optional{color_transform->color_adjustments} : std::nullopt);
```

A native probe over the absent-transform path (character id `0x5678`, depth `1`, identity matrix,
no transform bytes) aborted with
`member access within null pointer of type 'element_type' (aka 'flight::SwfColorTransform')` at that
line. The `DefineButton` path has the same shape (`version === 2 ? readSwfColorTransform(reader) : null`).

## Route and non-claims

This is a compiler defect, not a SWF one: any `const x = cond ? importedCall() : null` followed by
`x !== null` in emitted code is affected. The SWF correction that avoids it uses an explicit identity
carrier instead of a null test, which is semantics-preserving for those two transients but does not
close this cause for other call sites. No runtime change, source workaround or narrowing makes the
general form sound; the emitter must represent the imported call's union as its declared optional and
lower the test as presence. This request claims no header gain.

## Reproduction

`packages/compiler-backend-cpp/src/swfNullGuardProbe.test.ts` in the pinned compiler checkout emits six
shapes: five same-module forms and the imported form. The five same-module forms emit
`std::optional`/`has_value()`; the imported form emits `nullptr`/`true`. The cross-module shape is
lowered with `lowerTypeScriptSources` and the `flight-compiler-module-resolution/1` plan, then emitted
through `createEmissionSession`.
