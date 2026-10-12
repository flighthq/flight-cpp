# A readonly structural sequence stored as an owning array must keep its identity

A parameter typed as a readonly array of readonly rows is stored where an owning array is expected. In
TypeScript the stored value is the same array object. The emitter refuses: each element is a `RowOf`
view of an exact nominal referent, the view keeps the outer owner only behind opaque identity, and
recovering an owning `flight::Array<Ref<T>>` would copy the elements into a different array object.

## Minimal TypeScript

```ts
interface Entry { depth: number }
interface Source { frames: (readonly Entry[])[] }

export function createSource(frames: readonly (readonly Readonly<Entry>[])[]): Source {
  return { frames: frames as (readonly Entry[])[] };
}
```

`createSource(x).frames === x` holds in TypeScript.

## Required C++ shape

An identity-preserving projected-sequence carrier: the stored member keeps the caller's array owner and
exposes element views over it, rather than a copied owning array.

## Affected modules

- `createSwfTimelineSource` in `@flighthq/swf` (`swfDocument.ts`): refused, not supplied.
- `cffCharstring.ts` in `@flighthq/font-formats`: refused, supplied by override
  `font-formats-cff-charstring-structural-subroutines`.
