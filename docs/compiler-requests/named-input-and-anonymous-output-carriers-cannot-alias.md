# A named input carrier and a structurally identical anonymous output carrier cannot alias

When a parameter is declared as a **named** interface (`Readonly<Vector2Like>`) and the result is an
**inline anonymous** object type (`{ x: number; y: number }`), the emitter gives them two unrelated C++
types. TypeScript makes them interchangeable — the same object is legal as both — so a caller may pass
one object as the input and the output at once. In C++ that call does not compile, because the row over
the named type and the anonymous carrier have no conversion between them. Four `@flighthq/screen`
functions lose their aliasing half to this, and the upstream fixture exercises exactly that.

Measured at Flight `a62784923f3be814463286c9fb28edfe6ab15789`.

## Minimal TypeScript

From `packages/screen/src/screen.ts`. The input is a named interface; the result is inline and
anonymous — the two spellings are the same shape:

```ts
interface Vector2Like { x: number; y: number }

export function dipToScreenPoint(
  screen: Readonly<ScreenInfo>,
  point: Readonly<Vector2Like>,
  out: { x: number; y: number },          // <- anonymous, structurally identical to Vector2Like
): { x: number; y: number } {
  out.x = (point.x - screen.x) * screen.scaleFactor;
  out.y = (point.y - screen.y) * screen.scaleFactor;
  return out;
}

const point = { x: 112, y: 73 };
dipToScreenPoint(screen, point, point);   // out aliases the input -- legal, and the value converts
```

The same shape appears three more times. All four are affected:

| function | input carrier | output carrier |
| --- | --- | --- |
| `dipToScreenPoint` | `Readonly<Vector2Like>` | `{ x, y }` |
| `dipToScreenRect` | `Readonly<RectangleLike>` | `{ x, y, width, height }` |
| `screenToDipPoint` | `Readonly<Vector2Like>` | `{ x, y }` |
| `screenToDipRect` | `Readonly<RectangleLike>` | `{ x, y, width, height }` |

## Emitted C++

```cpp
inline flight::Ref<x_y_8365950bd60f783f> dip_to_screen_point(
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::Ref<flight::types::ScreenInfo>>>> screen,
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::Vector2Like>>> point,
    flight::Ref<x_y_8365950bd60f783f> out) { ... }
```

and, for the rectangle:

```cpp
inline flight::Ref<x_y_width_height_544099c876e1fbdc> dip_to_screen_rect(
    ...,
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::RectangleLike>>> rect,
    flight::Ref<x_y_width_height_544099c876e1fbdc> out) { ... }
```

`Vector2Like` is `flight::Ref<Vector2>`; `x_y_8365950bd60f783f` is a separate struct with exactly the same
two members. There is no widening proof between them.

## Observed: a hard compile error, not a silent difference

Passing one object as both arguments fails to compile:

```
include/flight/structural_ref.hpp:424:19: error: static assertion failed due to requirement
  'dependent_false<flight::types::x_y_8365950bd60f783f>':
  structural reference source has an incompatible object type
include/flight/structural_ref.hpp:1043:30: note: in instantiation of function template specialization
  'flight::detail::flatten_ref<flight::types::Vector2Like, flight::types::x_y_8365950bd60f783f>'
```

The reverse direction fails identically with `dependent_false<flight::types::Vector2Like>`. So neither
carrier converts to the other, and **no argument can alias both parameters**. The value-only calls work
and are unaffected: the arithmetic, the returned identity, and every field are correct — it is only the
aliasing form, which the upstream fixture exercises for all four functions, that cannot be written.

## Expected behaviour

Either spelling would restore it:

1. **One carrier for both.** Emit the anonymous result type once and use it for the input as well (or
   name the input `Vector2Like` and narrow `out` to it), so a single object satisfies both parameters —
   which is what TypeScript expresses.
2. **A widening between structurally identical carriers.** Admit the conversion when the two shapes have
   provably identical members, the same rule the derived duplicate-structural-struct alias already
   applies across packages: two anonymous shapes with byte-identical bodies become one type. Here one
   side is a *named* interface rather than an anonymous shape, which is why that repair does not reach
   it.

Whichever is chosen, the aliasing call must remain a **view**: `out.x` writes the object the input read,
with no copy and no second identity, exactly as the direct `out->x = ...` in the body already does.

## Strict negatives

The widening must NOT be a permissive "any two shapes with overlapping members" rule. In particular:

- **A superset must not narrow.** `{ x, y, z }` read as `{ x, y }` is not the same shape; admitting it
  would let an object with an unconsumed member alias an out that never writes it, and the caller would
  silently keep a stale `z`.
- **A point must not alias a rectangle.** `x_y_8365950bd60f783f` (`{x,y}`) and
  `x_y_width_height_544099c876e1fbdc` (`{x,y,width,height}`) must stay distinct: `dipToScreenPoint` and
  `dipToScreenRect` take different inputs, and interchanging them would let a point silently lose the
  two extent fields the rect path writes.
- **Unrelated shapes with a coincidental member must not convert.** `{ x, y }` and, say, `{ x, label }`
  share `x` and must remain unrelated.

So the admitted case is exactly *identical member sets with identical member types* — the same standard
the duplicate-structural-struct alias already holds itself to, extended to a pair where one side is a
named interface.

## Affected modules

`@flighthq/screen` — `dipToScreenPoint`, `dipToScreenRect`, `screenToDipPoint`, `screenToDipRect`. The
upstream `screen.test.ts` has an `is safe when out aliases the input` case for each of the four.

## Carrying workaround

None, and none is possible from the TypeScript side: the source already spells the two parameters in the
only way the API allows, and the gap is in the carriers the emitter chooses for them. The native parity
suite records the four aliasing cases as unreachable and asserts their value cases instead — see
`tests/generated_screen_parity_test.cpp`.
