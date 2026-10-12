# Distinct union domains must not collapse onto one carrier

`options.charstrings ?? [new Uint8Array([14])]` unions two array types whose elements are different
domains. The emitter refuses because both map to `flight::Array<flight::Uint8Array>` without a
discriminator or checked projection, so the value's origin would be lost.

## Minimal TypeScript

```ts
interface Options { charstrings?: readonly Uint8Array[] }
export function charstrings(options: Readonly<Options>): readonly Uint8Array[] {
  return options.charstrings ?? [new Uint8Array([14])];
}
```

## Required C++ shape

When both arms have the same element carrier and nothing downstream discriminates them, the union is the
carrier itself; a `??` with same-carrier arms needs no variant:

```cpp
return options->charstrings.has_value() ? options->charstrings.value()
                                        : flight::Array<flight::Uint8Array>{flight::Uint8Array{14}};
```

## Affected modules

`openTypeTestHelper.ts` in `@flighthq/font-formats` (a test helper): refused, not supplied.
