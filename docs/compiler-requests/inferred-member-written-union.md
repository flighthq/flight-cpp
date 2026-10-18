# A member's written union must survive an inferred receiver

`@flighthq/loader`'s `loadBytes` guards `response.body instanceof ArrayBuffer` and then constructs
`new Uint8Array(response.body)`. `NetResponseBody` is `string | unknown | ArrayBuffer | Blob | null`,
and TypeScript collapses `string | unknown` to `unknown`, so:

- for a parameter receiver the emitter resolves the member declaration and projects the proved
  alternative (`union_kind_or_throw<flight::ArrayBuffer>(response->body)`);
- for a receiver inferred from `await sendNetRequest(...)` the receiver arrives as a
  checker-collapsed object shape in which `body` is `unknown`, and the written union is lost. The
  guard's narrowing evidence is empty and the constructor receives the raw
  `std::optional<std::variant<...>>`, which does not convert (`no matching conversion ... at
  load.hpp:53`).

## Lowering

When a member's declared type is a union type node but lowering the node as a whole collapses to
`unknown`, the semantic lowering lowers each member of that union on its own and keeps the written
alternatives. The recovered evidence is used both for the member's binding type and for the
guard-narrowing fallback, so a guard compares against the alternatives the carrier actually stores.
Nothing is added, dropped, widened or converted; `unknown` stays the erased alternative the carrier
holds as `Any`.

## Disposition

Carried by `compiler-inferred-member-written-union`, with a focused backend test covering the
parameter and awaited receivers (both project and never emit the unprojected form) and a
title-level regression comparison across the semantic and backend suites (82 failures with and
without the patch on the same stack, identical titles). It expires when inferred receiver shapes
retain the member's written union, or upstream lowers the member type node without re-collapsing
`T | unknown`.
