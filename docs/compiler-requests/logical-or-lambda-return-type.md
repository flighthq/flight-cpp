# A lambda whose fallback is absence is declared to return the present type

A `||` whose right operand is `undefined` is lowered into a lambda declared to return the present type,
with a `return std::nullopt` inside it. The lambda then cannot compile, and the module's callers take its
result as a non-optional.

Measured at `2b687ea`, flight `a627849`, in the regenerated `@flighthq/audio` tree.

## Minimal TypeScript

`packages/audio/src/audioResourceFrom.ts:53`:

```ts
const arrayBuffer = await blob.arrayBuffer();
return loadAudioResourceFromBytes(audioDecode, new Uint8Array(arrayBuffer), blob.type || undefined, signal, decoders);
```

The parameter it fills is `mimeType: string` — but the *argument* is a logical-or whose fallback is
absence, so the value reaching the call is `string | undefined`.

## Observed

`generated/include/flight/audio/audio_resource_from.hpp:94`:

```cpp
co_return co_await load_audio_resource_from_bytes(audio_decode, flight::Uint8Array(array_buffer),
    ([&]() -> flight::String {
       auto logical_or_value = blob.type;
       if (flight::to_boolean(logical_or_value)) return logical_or_value;
       return std::nullopt;
     }()), signal, decoders);
```

```
error: no viable conversion from returned value of type 'const nullopt_t' to function return type 'flight::String'
```

The lambda's declared return type is the *present* type of the value it may also return absence for; the
two returns cannot both satisfy it.

## Expected behavior

The lambda returns the optional carrier — `std::optional<flight::String>` — and the call site passes what
the source passed: a value that may be absent. Where the destination parameter is declared non-optional
(`mimeType: string`), echoing the source's `|| undefined` as a non-optional would be inventing a value
the source did not have; the absence has to survive to the boundary the emitter chooses, the way the
other nullish coalesces in this same file already do (`flight::audio::infer_audio_mime_type(url)` fills an
`std::optional<flight::String>` parameter correctly one function down).

## Affected modules

`@flighthq/audio` — `audioResourceFrom.ts` and the three headers behind it. The package's own override
has hand-fixed this site since the pin before last (`audio-from-nullopt-return-type-and-missing-infer`:
"Fixed lambda return type to `std::optional<flight::String>`"), which is evidence the defect outlives any
one module and that a supply is the only thing standing between it and the tree.

## Carrying workaround

The override above, which is now drifted against the new emission. Until it is re-derived the module emits
this site unfixed.
