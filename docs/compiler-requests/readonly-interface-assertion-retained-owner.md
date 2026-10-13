# A readonly interface assertion should project from the retained owner

A callback can receive a readonly base interface and assert it to a readonly interface whose fields
are selected by a discriminator. When the C++ value still retains its structural owner, the assertion
should produce a checked readonly partial projection of that owner. It must not require native class
heritage or manufacture writable authority.

## Minimal TypeScript

```ts
interface Modifier {
  readonly kind: string;
}

interface MaskedModifier extends Modifier {
  readonly mask?: unknown;
}

export function signature(modifier: Readonly<Modifier>): string {
  const masked = modifier as Readonly<MaskedModifier>;
  return masked.mask !== undefined ? 'm' : '';
}
```

## Expected C++

The assertion should retain the input owner and expose only a readonly partial view of the asserted
row:

```cpp
using MaskedView = flight::StructuralRef<flight::RowReadonly<
    flight::RowPartial<flight::RowOf<flight::Ref<MaskedModifier>>>>>;
const auto masked = flight::structural_ref_cast<MaskedView>(modifier);
return flight::row_get<flight::RowKey<"mask">>(masked).has_value() ? "m" : "";
```

The projection neither changes object identity nor claims that the base C++ owner has nominal
heritage. A missing cell reads as absent, matching the unchecked TypeScript assertion followed by an
optional-property read. A projection must still refuse a write or an incompatible represented cell.

## Affected modules

`registerBuiltInModifiers.ts` in `@flighthq/shading` uses this pattern for animated-normal,
emissive, fog, and vertex-displacement signatures. Its dissolve signature asserts an anonymous
optional-map shape through the same readonly callback boundary. The emitter refuses all five
definitions, so the partial header and its two aggregate consumers do not compile without the
override.

## Carrying workaround

The `shading-register-built-in-modifiers-structural-assertions` override keeps each callback's
original owner and uses readonly partial projections for the specific interfaces. Its anonymous-map
probe reads the named cell without imposing a value carrier, preserving the distinction between
missing/undefined and a present texture variant. Remove the override when the compiler can emit these
checked readonly projections directly.
