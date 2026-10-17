# A `typeof` guard on a record member does not narrow the member's next read

At Flight `a62784923f3be814463286c9fb28edfe6ab15789` and flight-compiler
`806d257d2f4100de745cc97340e61c2f301b0f24`, `typeof record.fr === 'number'` on a
`Record<string, unknown>` member lowers to a correct runtime test, but the narrowed `record.fr` that
follows is still read as the erased `flight::Any`. The numeric comparison is then emitted on `Any`,
which has no relational operators, so the header does not compile.

## Source

```ts
function isLottieDocument(record: Readonly<Record<string, unknown>>): boolean {
  return (
    Array.isArray(record.layers) &&
    typeof record.fr === 'number' &&
    record.fr > 0 &&
    typeof record.ip === 'number' &&
    typeof record.op === 'number'
  );
}
```

(`packages/scene2d-formats/src/lottieCounts.ts`)

## Emitted mismatch

```cpp
(record.get(flight::String("fr")).value().type_of() == flight::String("number")) &&
    (record.get(flight::String("fr")).value() > 0.0)
```

```
flight/scene2d_formats/lottie_counts.hpp:26: error: no match for 'operator>' (operand types are 'flight::Any' and 'double')
```

The same guard on a **local** is already lowered faithfully. At this pin, 43 narrowed local reads are
emitted as `value.as_number()`, for example
`value.type_of() == flight::String("number")) && std::isfinite(value.as_number())`. So the gap is the
narrowing reference: TypeScript narrows the property-access path `record.fr`, and the emitter does not
carry that narrowing to the element read it lowers the access to.

## Expected lowering

After a `typeof` guard on a member path, read the narrowed member through the alternative the guard
proved, exactly as for a local:

```cpp
(record.get(flight::String("fr")).value().type_of() == flight::String("number")) &&
    (record.get(flight::String("fr")).value().as_number() > 0.0)
```

`as_number()` is checked, so it cannot misread a value the guard did not prove. A relational operator on
`flight::Any` would have to invent JavaScript's abstract comparison for every alternative, and it is not an
acceptable substitute.

## Affected headers

The four `@flighthq/scene2d-formats` headers of cluster `invalid-operands-4b19651b`: `lottie_counts.hpp`,
`lottie_requirements.hpp`, `contract.hpp` and `_internal_index.hpp`. The cluster's three
`@flighthq/skeleton2d-formats` headers share the diagnostic but not this cause. There, `Array.isArray(value)`
then `value.length` is emitted as `named_properties(value).get("length") > 0.0`, which is
[erased-array-element-reading](erased-array-element-reading.md).
