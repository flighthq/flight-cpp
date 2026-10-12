# A narrowing that excludes absence must prove the payload present

After `x.field !== undefined` (or an equivalent guard), the source reads the field as present. The
emitter refuses because the C++ storage carries two absence channels (`null` and `undefined`) or an
optional union, and the narrowing it sees still admits absence, so reading the payload would unpack
storage that may be empty.

## Minimal TypeScript

```ts
interface Options { deflate: Inflate | null }

export function merge(...parts: readonly Readonly<Partial<Options>>[]): Options {
  let deflate: Options['deflate'] = null;
  for (const part of parts) if (part.deflate !== undefined) deflate = part.deflate;
  return { deflate };
}
```

## Required C++ shape

The guard must narrow the outer optional (absent) while keeping the inner `null` arm. That needs two
absence channels (see [nullable-optional-presence](nullable-optional-presence.md)) and a projection that
records presence once the guard holds.

## Affected modules

- `mergeSwfParseOptions.ts` in `@flighthq/swf`: refused, not supplied.
- `fontFormat.ts` in `@flighthq/font` (`inferFontFormatFromUrl`, a member split after `Array.pop`):
  refused, supplied by override `font-infer-format-from-url-optional-pop`.
