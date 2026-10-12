# A null test on a narrowed optional-nullable member must read its storage

An optional member whose type already admits `null` is stored as two absences:

```ts
type Body = string | ArrayBuffer | ArrayBufferView | null;
interface Request { body?: Body }
```

```cpp
std::optional<std::optional<std::variant<flight::ArrayBuffer, flight::ArrayBufferView, flight::String>>> body;
```

The outer optional is `undefined` and the inner one is `null`. The strict and loose presence tests on
that storage are right. But when `request.body !== undefined` has narrowed the member first, the
following `request.body !== null` is emitted against the member's projected inner value instead of the
storage. Its test then asks the payload variant for `has_value()`:

```cpp
const auto& presence_operand = flight::row_get<flight::RowKey<"body">>(request).value();
return !presence_operand.has_value() || presence_operand.value().has_value();  // variant has no has_value
```

That does not compile. Even reached through a carrier where it would compile, the test is one level too
deep and would answer `true` for a null body.

The projection only happens when a payload arm is a profile-bound runtime type: `ArrayBufferView` in
flight-cpp's runtime profile. A payload of plain source types (`string | number[]`) already emits the
storage read.

## Minimal TypeScript

```ts
// body.ts
export type Body = string | ArrayBuffer | ArrayBufferView | null;
export interface Request { method: string; body?: Body }

// consumer.ts
import type { Request } from './body';
export function hasBody(request: Readonly<Request>): boolean {
  return request.body !== undefined && request.body !== null;
}
```

## Expected C++

Both tests read the storage. Short-circuit evaluation already supplies the narrowing:

```cpp
([&]() { const auto& presence_operand = flight::row_get<flight::RowKey<"body">>(request); return presence_operand.has_value(); }()) &&
([&]() { const auto& presence_operand = flight::row_get<flight::RowKey<"body">>(request); return !presence_operand.has_value() || presence_operand.value().has_value(); }())
```

This is the rule the dual-sentinel presence lane already applies to a narrowed property read.

## Affected

`@flighthq/net` `sendNetRequest` (`packages/net/src/net.ts`) guards a body on GET/HEAD with exactly this
test. It is the only compile error in the package's five headers at `a627849`, and it fails four of them
through `net.hpp`.

## Carrying workaround

[`compiler-nested-nullable-property-narrowed-null-test.patch`](../../source-patches/compiler-nested-nullable-property-narrowed-null-test.patch)
emits the un-narrowed storage read in the nested-nullable property branch of `emitCppPresenceTestCpp`,
as the dual-sentinel branch does.
