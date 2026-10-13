# A plain indexed array element must remain present for its member call

TypeScript gives a plain `Array<T>` index expression the element type `T` when
`noUncheckedIndexedAccess` is not enabled. The flight-cpp backend instead keeps the lookup's internal
optional storage as the receiver of a following member call and refuses to unwrap it, even when the
source has populated the array and uses the same bounded index for the read.

## Minimal TypeScript

```ts
export function group(count: number): number[][] {
  const groups: number[][] = [];
  for (let index = 0; index < count; index += 1) groups.push([]);
  for (let index = 0; index < count; index += 1) groups[index].push(index);
  return groups;
}
```

At flight-compiler `2b687ea`, the corresponding `@flighthq/texture-formats` expression refuses as
`cpp-member-projection-without-present-storage`:

```text
member push on optional C++ storage requires proven present payload
```

The source expression is not an optional chain and its static receiver type is `number[]`. If the
runtime index is out of range, JavaScript fails at the member access; silently returning absence would
also be wrong.

## Required C++ shape

Use the runtime array's checked element projection before the member call:

```cpp
groups.element(index).push(index);
```

That preserves the source's present element type and its failure behavior for an invalid index. The
compiler must not require an additional source guard whose only purpose would be to recover the type
TypeScript already assigned to the index expression.

## Affected module and carried workaround

`packages/texture-formats/src/parseAtf.ts` initializes one inner level array per format slot, then walks
the same slot bound and calls `perSlotLevels[slot].push(...)`. The refusal withholds
`parseAtfInternal`; three aggregate headers inherit the refusal. The complete
`texture-formats-parse-atf-nested-levels` override uses `Array::element(slot)` and otherwise transcribes
the source parser, restoring all 11 package headers.

This is distinct from [optional index/member flow](optional-index-member-present-flow.md), whose source
uses `values[index]?.member()`, and from [nullable array element tests](nullable-array-element-null-test.md),
whose element type itself carries `null`. Here neither the source element nor the access is optional.

