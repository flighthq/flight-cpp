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

## Carrying workaround

None. The audio override that shadows this module comments the enclosing function out rather than
lowering the check, which is why it cannot be re-derived against the new emission without a decision:
see its manifest entry's declared reason.

## Not to be confused with the `in`-operator defect

[`log-native-compile-defects.md`](log-native-compile-defects.md) entry 6 records TypeScript `in` emitted
verbatim. That is a different operator with a different lowering; this is `instanceof`.
