# A non-null assertion must not turn an absent property into `optional::value()`

## Minimal reproduction

```ts
interface Runtime {
  binding: object | null;
}

interface Entity {
  runtime?: Runtime;
}

export function getRuntime(source: Readonly<Entity>): Readonly<Runtime> {
  return source.runtime!;
}
```

The non-null assertion is erased by JavaScript. Calling `getRuntime({})` therefore returns
`undefined`, even though the intentionally asserted TypeScript return type does not advertise that
runtime value.

## Current C++ shape

The generated function returns a non-optional structural reference and lowers the asserted read
through `std::optional::value()`:

```cpp
return structural_ref_cast<RuntimeView>(
    row_get<std::optional<Ref<Runtime>>>(source, runtime_key).value());
```

An absent property consequently throws `std::bad_optional_access`, adding a failure that the source
function does not have.

## Required C++ shape

The emitted boundary must retain the possibility that the stored property is absent. For example,
the result may be an optional structural reference with the assertion erased from the runtime read:

```cpp
std::optional<RuntimeView> get_runtime(EntityView source) {
  const auto runtime = row_get<std::optional<Ref<Runtime>>>(source, runtime_key);
  if (!runtime) return std::nullopt;
  return structural_ref_cast<RuntimeView>(*runtime);
}
```

The exact carrier may differ, but observing an absent asserted property must not throw solely because
the TypeScript assertion was present.

## Affected Flight module

- `@flighthq/entity/packages/entity/src/runtime.ts#getEntityRuntime` at Flight
  `a62784923f3be814463286c9fb28edfe6ab15789`

The upstream runtime test explicitly calls `getEntityRuntime` on a freshly allocated entity and
expects `undefined` before assigning a runtime.

## Repository workaround and expiry

Native callers use `hasEntityRuntime` before `getEntityRuntime`; the focused entity parity test
exercises present-value identity and records the absent-value behavior as this residual rather than
accepting the generated exception as parity. No generated header or Flight source patch is carried.

This request expires when scoped generation gives `get_entity_runtime` an absence-preserving result
and a native test can assert the fresh-entity call directly without an exception.
