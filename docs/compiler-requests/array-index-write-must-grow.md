# An indexed array write at length must grow the array

JavaScript assignment to `array[array.length]` appends a dense element. The C++ emitter currently
lowers every indexed `Array<T>` write to `array.element(index) = value`; `element` is intentionally a
bounds-checked access and throws when `index === length`, so an ordinary dense append changes from a
successful write into `std::range_error`.

## Minimal TypeScript

```ts
interface Item { readonly id: number }

export function insertAtEnd(items: Readonly<Item>[], item: Readonly<Item>): void {
  items[items.length] = item;
}
```

## Expected C++

The write must preserve JavaScript's array-growth rule. A specialized append is sufficient when the
index is proven equal to the current length; a general indexed write needs a runtime operation that
resizes through `index + 1` before storing while retaining the same array identity.

```cpp
items.push(item);
```

or, for a dynamic index:

```cpp
items.set_index_growing(index, item);
```

This is distinct from an indexed read: a read beyond the live prefix remains absent, while a write
creates the intervening JavaScript slots. The dense Flight profile may reject a write that would
create holes, but it must not turn the dense `index === length` case into a bounds failure.

## Affected modules

`sceneForwardLights.ts` in `@flighthq/lighting` keeps exact-owner point and spot scratch arrays. Its
bounded insertion sort begins with an empty dense array, writes its first live element at index zero,
and later grows the live prefix one element at a time. The generated selector currently throws on
the first eligible light instead of returning the ordered forward-light selection.

## Carrying workaround

The `lighting-dense-forward-selection-scratch` source patch spells the two growth sites as guarded
`push` operations and retains indexed replacement once the slot already exists. It preserves the
same scratch-array identity, bounded live prefix, selected light references, scores, indices, and
ordering. Remove those guards when the compiler lowers dense indexed growth directly.
