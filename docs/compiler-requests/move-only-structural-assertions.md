# Move-only structural assertions should not require derived cells

The asserted-row proof compares the target's entire declared shape even when the asserted value is
only moved, compared by identity, passed, or returned. It therefore refuses for derived-only cells
that no operation ever reads.

## Minimal TypeScript

```ts
interface Node { id: number }
interface NodeOf<T> extends Node { runtime: T }

export function retain<T>(value: Node, out: Array<NodeOf<T>>): NodeOf<T> {
  const view = value as NodeOf<T>;
  out.push(view);
  return view;
}
```

No property of `view` is read.

## Expected C++

The assertion may retain the same owner as a structural view used only for transport:

```cpp
auto view = flight::structural_ref_cast</* NodeOf<T> row */>(value);
out.push(view);
return view;
```

If a later expression reads `runtime`, the existing derived-cell proof must still apply and may refuse.

## Affected modules

`packages/node/src/hierarchy.ts` and `nodeOrderList.ts`; the stricter read case in
`boundsRectangle.ts` is deliberately excluded because it does access derived-only members.

## Carrying workaround

There is no general compiler patch in this repository. Source patches rewrite some concrete assertion
sites to exact named carriers, while the remaining `cpp-structural-assertion-owner-unproven` sites stay
refused. The requested upstream fix is use-sensitive gating: preserve the proof for member reads, but
allow identity-preserving transport with no derived-cell access.
