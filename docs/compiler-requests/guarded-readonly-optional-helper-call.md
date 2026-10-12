# Guarded readonly optional rows must stay readonly across helper calls

After an `undefined` guard proves an optional readonly structural row present, passing that row to a
helper with the same readonly parameter type must preserve its const owner. The emitter currently
reconstructs the payload through a writable row first, which is an invalid const upgrade.

## Minimal TypeScript

```ts
interface Registry { readonly definitions: Map<string, number> }

declare function resolve(registry: Readonly<Registry>, key: string): number | null;

export function read(
  registry: Readonly<Registry> | undefined,
  key: string,
): number | null {
  if (registry === undefined) return null;
  return resolve(registry, key);
}
```

## Expected C++

The guarded payload already has the callee's readonly row type and should pass through unchanged:

```cpp
if (!registry.has_value()) return std::nullopt;
return resolve(registry.value(), key);
```

No `RowWritable` construction is valid or necessary. Rebinding a present optional payload does not
grant mutation rights to its referent.

## Affected modules

`getDefineSignature` in `@flighthq/shading` triggers the invalid conversion in
`flight/shading/get_modifier_define_key.hpp`; `contract.hpp` and `_internal_index.hpp` fail only by
including that header.

## Carrying workaround

[`shading-explicit-reference-storage.patch`](../../source-patches/shading-explicit-reference-storage.patch)
reads `registry.definitions.get(kind) ?? null` after the same guard. That is exactly the body of
`resolveModifier`, so it preserves one lookup, the returned definition's identity, and the null miss
sentinel while avoiding the broken helper boundary. Remove the direct lookup when the guarded payload
can pass to an identically readonly parameter without a writable round trip.
