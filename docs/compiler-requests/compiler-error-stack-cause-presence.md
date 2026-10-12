# Lower runtime-declared Error members and their presence tests

Once a runtime declares an Error member, its reads and its `!== undefined` tests need a lowering that
answers on the member's own storage. Upstream has none: a presence test on `error.stack` reaches the
generic "no absence channel" refusal, and the read itself is refused as unrepresented.

## Minimal TypeScript

```ts
export function describe(value: unknown): Record<string, unknown> {
  if (!(value instanceof Error)) return {};
  const result: Record<string, unknown> = {};
  if (value.stack !== undefined) result.stack = value.stack;
  if (value.cause !== undefined) result.cause = value.cause;
  return result;
}
```

## Expected C++ (flight-cpp profile)

```cpp
if (value.error_if()->stack().has_value()) { /* ... */ }
if (([&]() { const auto& presence_operand = value.error_if()->cause().value_or(flight::Any());
             return !presence_operand.is_undefined(); }())) { /* ... */ }
```

`stack` is `std::optional<String>` and never null. `cause` is the erased value, which is undefined when
there was none.

## Affected modules

`packages/log/src/log.ts` (`serializeLogError`).

## Carrying workaround

[`compiler-error-stack-cause-presence.patch`](../../source-patches/compiler-error-stack-cause-presence.patch).
It follows, and narrows only by these two names, the refusal requested in
[compiler-unrepresented-error-member](compiler-unrepresented-error-member.md).
