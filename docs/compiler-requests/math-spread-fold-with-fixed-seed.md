# A Math collection spread with fixed operands needs a seeded fold

The flight-cpp backend already lowers a single collection spread into `Math.min` or `Math.max` as a
range fold. Adding a fixed operand before that same spread falls through to the generic spread refusal,
even though the fixed operand is exactly the fold's initial value.

## Minimal TypeScript

```ts
export function stride(offsets: readonly number[]): number {
  return Math.max(1, ...offsets);
}
```

At flight-compiler `2b687ea` this refuses:

```text
spreading an unbounded collection requires a fold or a variadic target
```

The wording is shared with the generic rest-call request, but the lowering is not. The backend already
recognizes `Math.max(...offsets)` and emits `flight::maximum(offsets)`; it loses that specialization only
because the call also has a fixed argument.

## Pinned boundary

| source shape | result |
| --- | --- |
| `Math.max(...offsets)` | emits `flight::maximum(offsets)` |
| `Math.max(1, ...offsets)` | refused |
| `[1, ...offsets, 2]` | emits a dense array append loop |
| `out.push(...offsets)` | emits an append loop |
| `[...values]` for a `ReadonlySet<string>` | emits an iteration loop |

The passing collection cases rule out a general array, Set, or spread-expression gap. The existing
[unbounded rest-call request](unbounded-spread-into-rest-call.md) remains the request for arbitrary
callees; this request extends the backend's already-specialized Math fold.

## Expected lowering

Evaluate fixed operands and spread operands once, in source order, then fold the collection from the
fixed seed through the runtime operation:

```cpp
return ([&]() {
  auto seed = 1.0;
  auto values = offsets;
  auto result = seed;
  for (const auto& value : values) result = flight::maximum(result, value);
  return result;
}());
```

Using `flight::maximum` or `flight::minimum` for every step retains the runtime's JavaScript `NaN` and
signed-zero behavior. An empty spread returns the fixed operand. A general implementation must not nest
ordinary C++ call arguments in a way that loses JavaScript's left-to-right operand evaluation.

## P-3D2 reach

Two production sites have this exact fixed-seed shape:

- `packages/scene3d-formats/src/colladaGeometryDecoder.ts` computes an input stride with
  `Math.max(1, ...inputs.map(...))`; and
- `packages/scene3d-formats/src/colladaControllerDecoder.ts` does the same for weight inputs.

Four nearby P-3D2 sites use only one spread in `Math.min` or `Math.max` and match the passing control:
the two UV bounds in `meshGeometryBuilders.ts`, and diagnostic minima in `awd2SkeletonHandler.ts` and
`md2Parse.ts`. The committed first-refusal ledger does not currently name either COLLADA decoder, so
the two fixed-seed sites are measured source reach, not a claim that they are today's first blocker.

No source rewrite or downstream fold is carried.
