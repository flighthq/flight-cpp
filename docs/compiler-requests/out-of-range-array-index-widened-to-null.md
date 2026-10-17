# An out-of-range indexed array read must widen to `null`, not throw

`@flighthq/input`'s `getGamepadButtonName`/`getGamepadAxisName` read `_standardButtonNames[index] ?? null`.
For an index outside the table, JavaScript's indexed read is `undefined`, so the `??` yields `null` — and the
function's own doc comment says so: "or `null` if `mapping` is not `'standard'` or `index` is out of the standard
range". The emitted C++ reads through `flight::Array::element`, which is the **checked** accessor: it throws
`std::range_error`. A query that returns a value in one language terminates the process in the other.

## Minimal TypeScript

```ts
const names: readonly (string | undefined)[] = ['a', 'b'];
export function nameAt(index: number): string | null {
  return names[index] ?? null;
}
```

## Expected C++

The runtime already carries the operation that means `arr[i]` for an absent `i`: `Array::get(double)` returns
`std::optional<Element>` and normalizes an out-of-range, non-finite, fractional or negative index to
`std::nullopt`. The indexed read must lower to it whenever its result flows into a nullish fallback or another
optional:

```cpp
inline std::optional<flight::String> name_at(double index) {
  return names.get(index);
}
```

`Array::element` (throwing) stays correct where an index is already proven in range, which is why the runtime
keeps both accessors. This is a lowering choice, not a missing runtime capability: no new runtime operation is
needed to close it.

## Affected modules

`@flighthq/input` `inputManager.ts` — `getGamepadButtonName` and `getGamepadAxisName`.

The rule is general rather than input-specific: any emitted `arr[i] ?? fallback`, or any indexed read whose
result is observed as an optional, has the same defect. The two functions here are the measured instance.

## Carrying workaround

None. The module is emitted as written, and `@flighthq/input` is tier **assisted** for an unrelated reason (its
generated `input_manager.hpp` does not compile against the bound host timer type), so the override supplies the
same expression and the divergence is unchanged by it.

The divergence is pinned in `tests/generated_input_state_test.cpp` (`test_out_of_range_index_divergence`), which
asserts the throw as it stands today and must be inverted when this request lands.

## G2 SVG transform empty scale arguments retain the final one default

At Flight `a627849` / compiler `806d257`, source `svgTransform.ts` admits `scale()`:
its successful transform regexp match has an empty second capture, parseSvgNumberList returns
an empty numeric list, and the scale operation must default both axes to one. Source spells:

```ts
operation.a = values[0] ?? 1;
operation.d = values[1] ?? values[0] ?? 1;
```

The emitted second assignment at `svg_transform.hpp` instead performs:

```cpp
operation->d = ([&]() -> double {
  auto left = values.get(1.0);
  if (left.has_value()) return left.value();
  return values.element(0.0);
}());
```

The final source fallback is absent. The remaining index read is statically typed number,
but its actual empty-list value is undefined and must reach the final one; throwing element(0)
cannot stand for that read. Preserve the full y/x/one selection with absence-aware reads,
left-to-right short circuiting, explicit zero, negative values and the same Matrix allocation.
Do not forbid empty argument lists, substitute zero, require a second scale value or add a
permissive global checked-index fallback. This is the existing indexed-nullish lowering problem,
not the distinct optional<Any> SVG style length result.

This is an exact source/emitted-code inspection made during the
[readonly Matrix handoff](readonly-view-to-writable.md#g2-svg-transform-preserves-the-guarded-readonly-matrix)
qualification. It was not executed as a native parser case, added to a failure cluster or claimed
as the next native diagnostic. The bounded matrix conversion repair changes no parser bytes,
so this existing semantic debt remains explicit when that header passes its scoped compile gate.
All other transform functions, mandatory captures, permissive number-token domain, unknown-function
identity/null distinction, fresh owner and multiplication/viewport contracts remain required.
