# `ArrayBuffer.isView` must elect the existing native predicate

The runtime implements `flight::array_buffer_is_view`: it distinguishes `TypedArray` and
`DataView` carriers from their backing `ArrayBufferLike`. The compiler nevertheless refuses the
ambient static call before that runtime capability can be selected.

## Minimal TypeScript

```ts
export function isView(value: object): boolean {
  return ArrayBuffer.isView(value);
}
```

At flight-compiler `2b687ea` and Flight `a627849`, the refusal is:

```text
ambient value ArrayBuffer member isView has no C++ binding
```

while `include/flight/array_buffer_view.hpp` already supplies the target predicate and native tests
cover typed-array, data-view, and backing-buffer distinctions.

## Required behavior

Elect `ArrayBuffer.isView(value)` to `flight::array_buffer_is_view(value)` after the value's runtime
carrier is selected. Preserve false for ordinary arrays, objects, and the buffer itself. An erased
value needs checked dispatch over its held alternative; it must not guess from `typeof object`.

## Affected Flight module

`@flighthq/snapshot` `enableSnapshotGuards.ts` uses the call only after Map, Set, Date, and RegExp
checks, to name typed-array views as unsupported snapshot sources. The whole helper is refused even
though the native operation exists. A supplying override performs the corresponding explicit kind
check on its native plain-data carrier.
