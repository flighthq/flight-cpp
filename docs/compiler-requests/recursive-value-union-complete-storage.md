# A recursive value union needs incomplete-type-safe native storage

`FlightDocumentValue` is a recursive JSON-like union: scalars, arrays of itself, and string-keyed records
of itself. The generated C++ defines the wrapper by inheriting from an `optional<variant<...>>` whose
array and record alternatives immediately instantiate `FlightDocumentValue` by value. libc++ must query
copy/destruction traits while the wrapper is still incomplete, so any header that reaches the definition
fails before its own declarations are checked.

Measured at Flight `a627849` / flight-compiler `2b687ea`. Six emitted
`@flighthq/spritesheet-formats` parser/aggregate headers stop at this dependency defect.

## Observed C++

```cpp
struct FlightDocumentValue : public std::optional<std::variant<
    flight::Array<FlightDocumentValue>,
    flight::Record<flight::String, FlightDocumentValue>,
    bool,
    double,
    flight::String>> {
  // ...
};
```

Clang's first error is:

```text
std::pair<flight::String, FlightDocumentValue>::second has incomplete type
```

Pre-including `flight_document_field_schema.hpp` does not repair ordering: the error occurs inside the
definition itself, while `FlightDocumentValue` is incomplete.

## Required representation

The recursive array/record arms need storage that is legal with an incomplete wrapper while preserving
JavaScript semantics: scalar values stay values, array and object arms retain reference identity, nested
mutation remains visible through aliases, and absence remains distinct from every present arm. This may
be an indirection-owning recursive box or another runtime representation designed for recursive unions;
it cannot be a by-value STL cycle that requires the complete wrapper during its own definition.

## Distinguish from recursive-union narrowing

`compiler-typeof-narrowing-on-recursive-union.md` concerns a `typeof` guard that folds incorrectly after
the value has a carrier. This request is earlier: the carrier definition itself is ill-formed, independent
of any narrowing operation.

## Carrying workaround

None. The defect belongs to the generated `@flighthq/types` surface, which is read-only in the
spritesheet-formats unit. Parser native evidence remains blocked until the shared recursive carrier is
representable; serializer and formatter evidence can compile independently.
