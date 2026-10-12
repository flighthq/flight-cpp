# `registerAssetLoader` stores a readonly adapter as a writable one (Flight source)

This is a Flight source question rather than a compiler defect. The compiler's refusal is principled:

```ts
export function registerAssetLoader<T>(library: Readonly<AssetLibrary>, type: AssetType, adapter: Readonly<AssetLoaderAdapter<T>>): void {
  library.runtime.adapters.set(type, adapter as AssetLoaderAdapter);
}
```

```text
a structural assertion from the readonly row Readonly<AssetLoaderAdapter<T>> to the writable row
AssetLoaderAdapter claims mutation the source refused
```

The parameter promises not to mutate the adapter, and the assertion stores it where it can be mutated.
The adapter is only ever called after registration, never written. So the honest repair is upstream: type
`AssetLibraryRuntime.adapters` as `Map<AssetType, Readonly<AssetLoaderAdapter>>`. The alternative is to
declare the parameter writable if callers really hand over ownership. Which one is right depends on the
adapter's role, which only the Flight source can say. This repository should not decide it with a patch.

The generic erasure `AssetLoaderAdapter<T>` → `AssetLoaderAdapter` (`<unknown>`) is a second question in
the same assertion. It is the reference-assertion family recorded in
[generic-outcome-assertion-without-heritage](generic-outcome-assertion-without-heritage.md).

## Affected

`@flighthq/assets` `assetLibrary.ts` `registerAssetLoader` (line 235). At `a627849` this is the module's
third and last refusal, found by stubbing the first two.

## Carrying workaround

None. `registerAssetLoader` is absent from the `assets-reject-task-template-parameter` override.
