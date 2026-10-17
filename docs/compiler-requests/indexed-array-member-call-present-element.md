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

## G2 ASCII STL token reads

At Flight `a627849` / compiler `806d257`, `scene3d-formats/stlFeatures.ts` has two plain indexed
string reads in its refused functions:

```ts
// countAsciiStlFacets, inside while (index < tokens.length)
const keyword = tokens[index].toLowerCase();
// readAsciiStlBodyTokens, after rejecting an empty trimmed input
if (header.split(/\s+/)[0].toLowerCase() !== 'solid') return null;
```

Both refusal blocks report `cpp-member-projection-without-present-storage` on `toLowerCase`.
The tokens come from splitting the same local text; the first header token and the bounded loop
element are present. Their projection must preserve the original token and normal string case
conversion. The grammar also uses six indexed optional member calls, already covered by
[optional index/member flow](optional-index-member-present-flow.md); those must retain their
absent-token rejection branches, rather than use an unconditional checked projection.

`countAsciiStlFacets` accepts only complete facet blocks: a finite normal, `outer loop`, three finite
vertices, `endloop` and `endfacet`. It stops on `endsolid` and returns null when no facet was counted.
The header helper treats the remainder of the first line as the solid name. A parser that scans
ahead for `facet`, accepts partial blocks, substitutes zero or changes null into a count is not a fix.
The declaration repair described in
[retained callers](retained-declaration-calls-omitted-callee.md) supplies neither refused body.

