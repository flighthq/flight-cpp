# An indexed dense-array element keeps the presence established by lockstep append

An array element read is optional because an arbitrary numeric index can be out of bounds. That rule
is correct in isolation, but it loses a local invariant in `@flighthq/path-boolean`: a helper creates an
index only after appending an element at that index, and every caller obtains its index from that helper.
The resulting member call is refused even though the element is present on every source path.

## Reduction

```ts
class Graph {
  private readonly keys = new Map<string, number>();
  private readonly outgoing: number[][] = [];

  add(key: string, edge: number): void {
    const index = this.vertex(key);
    this.outgoing[index].push(edge);
  }

  private vertex(key: string): number {
    const found = this.keys.get(key);
    if (found !== undefined) return found;
    const index = this.outgoing.length;
    this.keys.set(key, index);
    this.outgoing.push([]);
    return index;
  }
}
```

At flight-compiler `2b687eaf6b631e0fa4d7d6e18dad93b845f99633`, the `push` call refuses:

```text
member push on optional C++ storage requires proven present payload
```

This is narrower than the general optional indexed-receiver case. The producer returns either a
previously recorded index or the exact pre-append length, and the new branch appends before returning.
The `Map` entry and the dense array therefore grow together. No caller can manufacture an index.

## Expected lowering

The backend needs a way to retain the helper's returned-index/postcondition relationship and project the
present element before the member call. With that proof, the relevant C++ operation is equivalent to:

```cpp
outgoing.element(index).push(edge);
```

The proof must not make arbitrary array indexing total. An index not derived from the append-or-existing
helper still needs a guard or an optional carrier, and the existing-index branch must remain tied to the
same map/array invariant rather than being accepted solely because its type is `number`.

## Corpus impact and workaround

At Flight source revision `a62784923f3be814463286c9fb28edfe6ab15789`, the refusal is
`packages/path-boolean/src/martinezKernel.ts:578`. It withholds the module-private `DirectedGraph` and
causes `contract.ts` and `index.ts` to be refused as dependents. The class actually grows `xs`, `ys`, and
`outgoing` in lockstep; `addEdge` obtains both endpoint indices only through `vertex`, then appends the
new edge to `outgoing[from]`.

The carried override `path-boolean-martinez-directed-graph` supplies the source-equivalent graph and
expires when this invariant can be represented without weakening out-of-bounds behavior. No source patch
is carried: adding a redundant defensive branch would obscure a valid construction invariant solely for
the C++ emitter.

This is related to [optional indexed member flow](optional-index-member-present-flow.md), but that page
covers presence introduced by a guard or optional chain. Here the evidence is relational: it is created
by a helper that appends a dense element and returns its index.
