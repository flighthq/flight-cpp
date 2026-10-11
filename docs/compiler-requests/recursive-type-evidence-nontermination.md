# Recursive type-evidence lowering must terminate

`lowerTypeScriptTypeNodeEvidence` recursively walks mutually recursive Flight types without a stable
visited/in-progress guard. On affected full graphs it repeatedly re-enters the same evidence and either
does not terminate or grows superlinearly beyond practical generation time.

## Minimal TypeScript

```ts
export interface Node<T> { runtime: NodeRuntime<T> }
export interface NodeRuntime<T> { owner: Node<T>; update(node: Node<T>): void }
export function retain<T>(node: Node<T>): Node<T> { return node; }
```

The real graph adds imported aliases and generic heritage, but the essential shape is the two-record
cycle.

## Expected C++

Lowering must complete once per evidence key and emit the ordinary recursive reference surface:

```cpp
template <typename T> struct Node;
template <typename T> struct NodeRuntime;
template <typename T> struct Node { flight::Ref<NodeRuntime<T>> runtime; };
template <typename T> struct NodeRuntime {
  flight::Ref<Node<T>> owner;
  std::function<void(flight::Ref<Node<T>>)> update;
};
```

## Affected modules

The failure was isolated to the `@flighthq/render-gl` closure in older affected revisions and was also
observed in full SDK generation. Mutually recursive node/runtime types are common throughout Flight.

## Carrying workaround

Generation is run in bounded package/slice scopes instead of one full graph. The carried
[`compiler-scoped-emission-semantic-context.patch`](../../source-patches/compiler-scoped-emission-semantic-context.patch)
makes those scoped runs semantically honest by retaining unselected dependencies in the backend session;
it does not fix the recursion itself. Upstream should memoize both completed and in-progress evidence
keys and return a stable recursive reference rather than re-entering the walk.
