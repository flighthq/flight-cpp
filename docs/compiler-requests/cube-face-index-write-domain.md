# Cube face writes require the full numeric array-property domain

At Flight `a62784923f3be814463286c9fb28edfe6ab15789`, the exact public setter is:

```ts
export function setCubeTextureFace(cube: CubeTexture, faceIndex: number, source: TextureSource | null): void {
  const sources = getCubeSources(cube);
  if (sources[faceIndex] === source) return;
  (sources as unknown as (TextureSource | null)[])[faceIndex] = source;
  cube.version = (cube.version + 1) >>> 0;
}
```

There is no integer or range guard. The type of `faceIndex` admits every JavaScript number.
The pinned-source oracle executes this body after erasing TypeScript syntax, preserving the
original array identity and version-update order.

| Index on six existing null slots | JavaScript effect |
| --- | --- |
| 0 | Replace one dense slot |
| 6 | Append; length becomes 7 |
| 8 | Length becomes 9; indices 6 and 7 are holes |
| -1, 0.5, NaN, Infinity | Create the corresponding named own array property; length remains 6 |

All these writes succeed. A changed property increments version with unsigned 32-bit wrapping;
a repeated identity-equal write returns before the increment. Reading a hole returns undefined,
which differs from null, and array iteration skips holes for operations such as `every`.
Named properties are observable through ordinary property access and own-key enumeration while
remaining outside indexed array iteration.

`flight::Array` is deliberately a dense shared vector. Its `element` operation throws outside the
current integer bounds; `get` retains index-miss absence separately from a nullable element.
`resize` cannot represent holes: extending a nullable array creates dense null elements. It also
has no storage for named numeric properties. Choosing a vector element, clamping/rejecting indices,
or filling a gap with null would alter the pinned setter's absence, iteration or exception behavior.

The [indexed-write append request](array-index-write-must-grow.md) covers the contiguous index-at-length
case. It does not provide the sparse or named-property cases above. A future full setter must preserve
all those represented domains and the identical face owner, array identity and version ordering.

## Current scope decision and supplied surface

Manager decision `manager-978c7b5c`, relayed in `foreman-b22591a1`, keeps the dense array profile and
does not authorize a sparse/named-index Array redesign. The cube override therefore remains partial
and leaves `setCubeTextureFace` visibly refused and absent. It does not publish a bounded setter
under the unrestricted source API name or add a target-only throwing fallback.

The independent clone/copy/equals follow-up supplies those three operations using current readonly
row cells and existing entity, sampler and vector primitives. Clone creates independent sampling-view
owners and face-list storage while sharing face owners. Copy preserves destination sampler and UV
owners, creates fresh face-list storage even for self-copy, captures version before mutations and
preserves UV copy order under cross-aliasing. Equality checks nullable operands before reading fields,
uses row identity before value comparisons, and then follows the pinned short-circuit field order.
A distinct proxy view does not inherit its underlying native object's identity shortcut.

`cubeTextureApisOracle.mjs` reads all exercised TypeScript helper declarations directly from the pinned
Git objects and erases types with the parser already installed for the pinned compiler input. It adds no
runtime dependency. It checks these ownership/alias/exception cases and records the setter domain above.
The native API regression checks the same supported semantics, including the emitted nullable-readonly
function signature. The original all-surface Texture target passes after these three operations are
supplied; it does not exercise the absent setter and therefore cannot certify a complete cube package.
