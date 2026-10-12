# An object literal passed through an optional call must build the parameter's reference owner

The emitter collapses a union of object shapes into one record held behind a reference: `Notice` becomes
`flight::Ref<operation_reason_socket_<hash>>`. When an object literal is passed to a nullable callable
through an optional call, `_guard?.({...})`, the argument is emitted as a bare designated initializer.
A `std::function<void(Ref<Record>)>` has no call that accepts one, so the header does not compile.

Measured at flight-compiler `2b687ea`, with every carried patch applied, against flight `a627849`.

## Minimal TypeScript

```ts
// types.ts
export interface Socket { url: string }
export type Notice =
  | { readonly operation: 'create'; readonly reason: 'no-connection'; readonly socket: Readonly<Socket> }
  | { readonly operation: 'close' | 'send'; readonly reason: 'disposed'; readonly socket: Readonly<Socket> };
export type Guard = (notice: Readonly<Notice>) => void;

// consumer.ts
import type { Guard, Notice, Socket } from './types';
let _guard: Guard | null = null;
export function close(socket: Socket): void {
  _guard?.({ operation: 'close', reason: 'disposed', socket });
}
```

## Emitted

```cpp
optional_chain_receiver.value()({.operation = flight::String("close"), .reason = flight::String("disposed"), .socket = /* row */});
```

## Expected C++

The literal builds a fresh owner of the parameter's record type, as the same literal does in a
non-optional call to a row-typed parameter:

```cpp
optional_chain_receiver.value()(flight::make_ref<Record>(Record{.operation = ..., .reason = ..., .socket = ...}));
```

## Affected

`@flighthq/socket` `socket.ts`: four `_guard?.({ operation, reason, socket })` sites in `closeSocket`,
`createSocket`, `enableSocketSignals` and `sendSocketMessage`. The same construct in `@flighthq/net`
compiles, because `NetGuardNotice` is a single interface that lowers to a structural row.

## Carrying workaround

The `socket-complete-lifecycle` override supplies `flight/socket/socket.hpp`. Its `emit_socket_guard`
builds the record through the public `SocketGuardNotice` alias.
