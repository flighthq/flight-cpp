# A sized array filled by a pair-write loop is fully written before it escapes

`@flighthq/swf`'s `createSwfMaskClipRegion` transforms flattened contour points by allocating
`new Array<number>(contour.length)`, then writing flat x/y pairs:

```ts
const transformed = new Array<number>(contour.length);
for (let i = 0; i < contour.length; i += 2) {
  matrixTransformPointXY(_maskPoint, combined, contour[i], contour[i + 1]);
  transformed[i] = _maskPoint.x;
  transformed[i + 1] = _maskPoint.y;
}
contours.push(transformed);
```

A sized JavaScript array begins sparse, so the emitter refuses it unless it can prove every slot is
written (with the source's own evaluation order and errors) before the array can be observed. The
proof that covers this shape already exists in the sequential-append lane; three conservatisms kept the
real source out of it, and one missing coercion made the final odd pair diverge:

1. the loop bound `contour.length` was not a *stable* bound when `contour` was an unassigned
   parameter of array type (only number parameters were accepted);
2. any call in the loop body invalidated a `.length` bound, even when the call receives only element
   values (`contour[i], contour[i + 1]`) and cannot reach the array;
3. a closure, alias, or module global that can reach the bound array was not consulted, so a callee
   that captured the array could have mutated it while the proof assumed the length was fixed;
4. the partner read `contour[i + 1]` is not proven in bounds (an odd final pair reads past the end);
   JavaScript answers an absent index with undefined, and a numeric use of undefined is NaN.

## Emitted

The sized allocation is constructed (so the length check and any throw happen in source order), then
cleared, and every pair is appended at the current length with a check:

```cpp
flight::Array<double> transformed = ([&]() {
  auto sequential_append_array = flight::Array<double>(static_cast<double>(contour.size()));
  sequential_append_array.clear();
  return sequential_append_array;
}());
// each write:
if (index != static_cast<double>(receiver.size()))
  throw std::range_error("proven sequential Array write did not append at its current length");
receiver.push(value);
```

The numeric partner read at its source argument is the existing checked array read composed with the
presence-aware scalar cell:

```cpp
([&]() -> double {
  auto&& array = contour;
  const auto index = (i + 1.0);
  if (const auto value = array.get(index)) return flight::JsonScalarCell{flight::Any{*value}}.to_number();
  return flight::JsonScalarCell{}.to_number();     // absent -> undefined -> NaN
}())
```

## Equivalence

- Every slot is written exactly once, in source order, before the array escapes; the cleared
  allocation never serves a value, and no zero-filled element is fabricated.
- Appending at the current length preserves JavaScript's growth by one for a final odd pair; a
  non-sequential write throws rather than silently filling.
- The partner read keeps JavaScript's numeric semantics: an absent index is NaN, a stored NaN stays
  NaN, and a present value is unchanged. The array and index are evaluated once, in source order, at
  that numeric argument only; a read anywhere else keeps its existing lowering.
- The bound is accepted only while the array cannot be replaced, aliased, stored in a global, or
  mutated: a closure defined before the loop, an alias handed to a call, a module-global store, a
  callee that receives the array itself, or a body that mutates it all keep the loud refusal.

## Separately recorded: the general indexed-read lane

An indexed read outside this pair lane still lowers to `element(index)`, whose out-of-range read is a
`range_error`. That is the existing general lane's behavior, not something this change fixes; the
faithful undefined-to-NaN coercion is applied only at the pair lane's numeric partner argument, as
approved. No global read-semantics change is claimed.

## Evidence

- `cppDensePairWriteConstruction.test.ts`: 8 pass — real shape (sized+cleared, checked appends,
  `element(i)` for the proven slot, `JsonScalarCell` read for the partner); unassigned array parameter
  bound; a raw read outside the lane unchanged; mutation via a call receiving the array, a mutating
  body, a capturing closure, an alias passed to a call, and a module-global store all refusing.
- Compiler-emitted native fixture: even → `3,-1,7,-1` in order; zero-length → one empty array;
  odd-length contour → BOTH transformed slots NaN with the array grown by one; a stored NaN stays NaN;
  a throwing transform propagates after the writes it preceded; the source array is unmodified.
- JS oracle over the same module produces the same even/zero/odd values (odd slots NaN) and the same
  read order.

Managed patch: `source-patches/compiler-dense-pair-write-construction.patch`; expires when upstream
proves the same construction from the source.
