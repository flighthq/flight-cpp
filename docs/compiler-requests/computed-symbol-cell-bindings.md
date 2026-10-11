# Computed `Symbol` cells need generated bindings

The emitted SDK can declare a computed member keyed by a unique `Symbol`, but the runtime can bind only
the computed cells it knows statically. A generated widening table can see the member and fail closed;
`row_get` still cannot reach it and falls through to attachment storage.

## Minimal TypeScript

```ts
export const ResourceRuntime = Symbol('ResourceRuntime');
export interface Resource {
  [ResourceRuntime]?: { revision: number };
}
export function revision(value: Resource): number {
  return value[ResourceRuntime]?.revision ?? 0;
}
```

## Expected C++

The generated structural-member table must bind the exact symbol constant to the exact member:

```cpp
template <>
struct GeneratedSymbolBindings<flight::Ref<types::Resource>> {
  static void bind(RowOwner& owner, const flight::Ref<types::Resource>& value) {
    owner.bind_symbol(types::resource_runtime, &value->resource_runtime_key);
  }
};
```

The spelling is illustrative; the contract is identity-based binding to the retained object's cell.

## Affected modules

`Scene3DResourceResolverRuntime` reads and any future computed key created with a unique `Symbol(...)`
rather than `Symbol.for(...)`.

## Carrying workaround

There is no compiler patch. [`sdkGeneration.mjs`](../../scripts/sdkGeneration.mjs) records every
computed cell in the widening proof so incompatible rows fail closed, and reports cells the runtime
cannot read. That prevents unsound widening but does not repair the read; this request closes when the
compiler emits generated symbol bindings.
