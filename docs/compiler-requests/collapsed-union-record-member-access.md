# Member reads on a collapsed union record must use one access form

Once a union of object shapes is collapsed into a reference-backed record (`Notice` is
`flight::Ref<Record>`), reads of a `Readonly<Notice>` parameter come out in three different access forms
within one function. Only one of them is valid for the carrier:

```cpp
inline flight::String describe(flighthq_types::Notice notice) {
  const flight::String url = flight::row_get<flight::RowKey<"url">>(notice.socket);  // `.` on a Ref
  return (notice->reason == flight::String("no-connection")                         // `->`: valid
      ? url
      : ... flight::row_get<flight::RowKey<"operation">>(notice) ...);              // row_get on a Ref, not a row
}
```

`notice.socket` is value syntax on a `std::shared_ptr`. `row_get<"operation">(notice)` treats the record
reference as a structural row, which it is not. Only the discriminant read (`notice->reason`) is right.

Measured at flight-compiler `2b687ea`, with every carried patch applied, against flight `a627849`.

## Minimal TypeScript

The `types.ts` from
[optional-call-object-literal-into-reference-owner](optional-call-object-literal-into-reference-owner.md), and:

```ts
import type { Notice } from './types';
export function describe(notice: Readonly<Notice>): string {
  const url = notice.socket.url;
  return notice.reason === 'no-connection' ? url : `${notice.operation}:${url}`;
}
```

## Expected C++

Every member read goes through the record reference, and the nested readonly row through its own access:

```cpp
const flight::String url = flight::row_get<flight::RowKey<"url">>(notice->socket);
... notice->reason ... notice->operation ...
```

## Affected

`@flighthq/socket` `enableSocketGuards.ts` `warnOnSocketMisuse`: 6 of the 10 errors in the generated
socket headers. The other 4 are the optional-call defect above.

## Carrying workaround

The `socket-guard-reporting` override supplies `flight/socket/enable_socket_guards.hpp` and reads the
notice through the reference.
