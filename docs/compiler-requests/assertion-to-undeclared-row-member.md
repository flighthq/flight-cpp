# A row assertion to a narrower data type must check the member it reads

`(node.data as SwfAuthoredBoundsData).authoredBounds` asserts that a node's data row carries a member the
declared data type does not. The emitter refuses, because the asserted row reads `authoredBounds`, which
the source row type `Node2DData` does not declare, and nothing proves the runtime value has it.

## Minimal TypeScript

```ts
interface NodeData { kind: string }
interface AuthoredBoundsData extends NodeData { authoredBounds: Rectangle }

export function boundsOf(data: NodeData): Rectangle {
  return (data as AuthoredBoundsData).authoredBounds;
}
```

## Required C++ shape

A checked downcast of the retained owner to the asserted nominal type (the value was created as
`AuthoredBoundsData`), failing explicitly when it is not one, rather than a row read of an undeclared member:

```cpp
// illustrative: a checked owner cast the runtime would need to provide
auto owner = checked_owner_cast<AuthoredBoundsData>(data);  // throws TypeError when it is not one
return owner->authored_bounds;
```

Related: [assertion lowered to an unchecked static_cast](assertion-lowered-to-unchecked-static-cast.md).

## Affected modules

`computeSwfLocalBoundsRectangle` in `@flighthq/swf` (`swfNode.ts`): refused, not supplied.
