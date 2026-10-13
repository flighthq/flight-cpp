# A named function with fewer parameters must be adapted where a wider callable is expected

TypeScript lets a function with fewer parameters stand wherever a function with more is expected: the extra
arguments are simply ignored. The emitter already honors that for an arrow literal, which it emits as a
lambda padded with `ignored_callback_argument` parameters. A reference to a **named** function gets no such
adapter: `construct ?? noop` produces a conditional between `std::function<void(String, double)>` and the
plain function `void()`, which does not compile. The emitted function is not refused.

Measured at flight-compiler `2b687ea` with all 44 of this repository's compiler source patches applied, one
module per case, each emitted header then compiled with clang 22.

## Minimal TypeScript (measured)

```ts
export interface Source { construct: (target: string, frame: number) => void }
function noop(): void {}
export function make(construct?: (target: string, frame: number) => void): Source {
  return { construct: construct ?? noop };
}
```

```text
error: incompatible operand types ('std::function<void (flight::String, double)>' and 'void ()')
```

## Pinned boundary

| case | C++ |
| --- | --- |
| named `function noop(): void {}` as the `??` fallback for a two-parameter slot | **fails** |
| the same fallback declared `noop(_target: string, _frame: number)` | compiles |
| an arrow literal `() => {}` stored in the two-parameter slot | compiles (emitted with `ignored_callback_argument` parameters) |

## Required C++ shape

Wrap the named function in the same padding the arrow literal already gets, at the point its type widens:

```cpp
std::function<void(flight::String, double)>([](flight::String, double) { noop(); })
```

This applies to any function reference crossing into a callable type with more parameters, not only `??`.

## Affected Flight modules

`@flighthq/timeline` `timeline.ts` `initializeTimelineSource`: `out.constructFrame = obj.constructFrame ??
noopConstructFrame`, where `noopConstructFrame(): void` takes no parameters and the slot is
`(target: Node2D, frame: number) => void`.

## Carrying workaround

[`timeline-noop-construct-frame-arity`](../../source-patches/timeline-noop-construct-frame-arity.patch)
declares the two ignored parameters on the module-private no-op, the measured compiling control.
