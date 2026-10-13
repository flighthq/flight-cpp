# `Array.isArray` must inspect an optional property's present payload

A property read through `Partial<T>` is represented as `std::optional<Value>`. When that read is
passed to `Array.isArray`, the emitter currently calls the runtime predicate on the optional carrier
itself. The generic non-array overload therefore returns `false` even when the property is present
and its payload is an array.

## Minimal TypeScript

```ts
interface Operation { kind: string }
interface MatrixOperation extends Operation { colorMatrix: readonly number[] }

export function matrixOf(operation: Operation): readonly number[] | null {
  const matrix = (operation as Partial<MatrixOperation>).colorMatrix;
  return Array.isArray(matrix) && matrix.length === 20 ? matrix : null;
}
```

## Observed C++

At flight `a627849` and flight-compiler `2b687ea`,
`packages/adjustments/src/colorMatrixAdjustment.ts` emits:

```cpp
std::optional<flight::Array<double>> matrix = flight::row_get<...>(...);
return ((flight::is_array(matrix) &&
         static_cast<double>(matrix.value().size()) == 20.0)
            ? std::optional<flight::Array<double>>{matrix.value()}
            : std::nullopt);
```

`flight::is_array(matrix)` selects the generic non-array overload and is always false. A live
`ColorMatrixAdjustment` consequently fails `getAdjustmentColorMatrix` and
`isColorMatrixAdjustment`; matrix-to-LUT wrapping and both color-adjustment resolution functions
also fail.

## Required shape

The call must test the present payload without evaluating it when absent. Either the emitter can
lower the optional explicitly,

```cpp
matrix.has_value() && flight::is_array(matrix.value())
```

or the runtime binding can define the equivalent optional-aware predicate. Both preserve the two
observable JavaScript results: an absent property is not an array, while a present array is.

## Carrying workaround

`flight::is_array(const std::optional<Value>&)` recurses only when the optional is engaged. Native
Adjustments evidence covers present matrix identity, absent non-matrix operations, matrix-to-transform
wrapping, affine resolution, channel-mixing classification, and the invalid-length failure boundary.
The overload is a runtime binding for the emitted carrier and can be removed when the compiler unwraps
the optional before calling the predicate.
