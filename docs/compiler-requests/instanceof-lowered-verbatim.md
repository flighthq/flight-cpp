# TypeScript `instanceof` is emitted verbatim as C++ text

A runtime type check on an emitted external type reaches the C++ as the source keyword, and the
translation unit stops at the parse.

Measured at `2b687ea`, flight `a627849`, in the regenerated `@flighthq/audio` tree.

## Minimal TypeScript

`packages/audio/src/audioResourceFrom.ts:104`:

```ts
const response = await sendNetRequest(hostNet, { method: 'GET', responseType: 'arraybuffer', url }, signal !== undefined ? { signal } : undefined);
if (!response.ok) throw new Error(`Failed to load audio: ${url} (${response.status} ${response.statusText})`);
if (!(response.body instanceof ArrayBuffer)) throw new Error(`Failed to load audio: ${url} (invalid body)`);
```

## Observed

`generated/include/flight/audio/audio_resource_from.hpp:102`:

```cpp
if (!(response->body instanceof flight::ArrayBuffer)) {
  throw flight::Error(flight::String("Failed to load audio: ") + flight::to_string(url) + flight::String(" (invalid body)"));
}
```

```
error: expected ')'
```

`instanceof` is not a C++ operator, so the header cannot be parsed past this line. The same module also
emits `flight::to_string(url)` in the same statement, which is a separate question the C++ toolchain
answers for itself.

## Expected behavior

The check is a presence-and-kind test on the *generated* type, and the runtime already has the
vocabulary for it — the emitted carrier is a `std::variant`/`std::optional` in the cases seen so far, so
the lowering is a `std::holds_alternative` or an explicit kind test on the variant the body was read
from. Which one is the emitter's decision; emitting the source keyword is not available.

## Affected modules

`@flighthq/audio` — `audioResourceFrom.ts`, and the three headers that include it
(`audio_resource_from.hpp` itself, `contract.hpp`, `_internal_index.hpp`). Discovered only after
`audio-indexed-member-presence-proof.patch` cleared the refusal earlier in the same module, so other
packages that were refused before reaching an `instanceof` may surface the same defect as their
refusals clear.

Also `@flighthq/loader` at `a627849`, `packages/loader/src/load.ts:29`:

```ts
return response.ok && response.body instanceof ArrayBuffer ? new Uint8Array(response.body) : null;
```

That emits `(response->body instanceof flight::ArrayBuffer)` in `load.hpp:51`, and the narrowed
`new Uint8Array(response.body)` passes the whole `NetResponseBody` optional-variant to `flight::Uint8Array`.
So the lowering also has to project the narrowed read, `std::get<flight::ArrayBuffer>(response->body.value())`.
`NetResponseBody` is `string | unknown | ArrayBuffer | Blob | null`, which TypeScript reduces to `unknown`,
yet it is emitted with an `Any` arm beside `ArrayBuffer`. The kind test therefore has to answer for an
`ArrayBuffer` held in the `Any` arm too. This fails `load.hpp`, `contract.hpp` and `_internal_index.hpp`.

## Carrying workaround

None. The audio override that shadows this module comments the enclosing function out rather than
lowering the check, which is why it cannot be re-derived against the new emission without a decision:
see its manifest entry's declared reason.

## Not to be confused with the `in`-operator defect

[`log-native-compile-defects.md`](log-native-compile-defects.md) entry 6 records TypeScript `in` emitted
verbatim. That is a different operator with a different lowering; this is `instanceof`.

## Carried by a compiler patch

`source-patches/compiler-runtime-instanceof-lowering.patch` lowers the guard onto `flight::union_kind_if`
and the projection it proves onto that function's throwing twin, so the source operator never reaches the
emitted C++ and the two answers cannot disagree. The question itself lives in the runtime
(`flight/optional_variant.hpp`), so the same walk over a carrier's alternatives answers a `std::optional`,
a `std::variant` and a bare erased value, and its arms are pinned in `tests/union_kind_test.cpp`. The
emitter's side is pinned in `cppRuntimeInstanceOf.test.ts`.

The lowering does not by itself make the two measured modules compile, and that residue is a second,
separate defect rather than a gap in this one: `NetResponseBody` is `string | unknown | ArrayBuffer | Blob
| null`, and for a read of that declared type the analysis produced neither the union evidence the guard
needs nor the narrowing the guarded read needs. `typeof o.body === 'string'` on the same property refused
for the same reason, so the defect was never about `instanceof`. It is fixed by
`source-patches/compiler-profile-type-name-evidence.patch` and
`source-patches/compiler-guarded-path-narrowing.patch`, and recorded in
[`property-union-evidence-lost.md`](property-union-evidence-lost.md).
