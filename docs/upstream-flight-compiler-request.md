# flight-compiler package graph review

This review covers Flight `1274ec5` and flight-compiler `3f86fcb`. The compiler revision is pinned in
[`dependencies.lock.json`](../dependencies.lock.json). The compiler reciprocally pins flight-cpp `70656d4`, the
revision immediately before the runtime additions described here.

## Downstream adoption

The runtime now implements the portable names introduced by flight-compiler's
[`agents/flight-cpp-adoption.md`](https://github.com/flighthq/flight-compiler/blob/3f86fcb04df8425c5757eccc16560614cc9b5589/agents/flight-cpp-adoption.md):

- `ArrayBuffer` owns stable, zero-initialized shared storage. Every typed array can view that storage and exposes its
  `buffer`, `byte_offset`, and `byte_length`; aliases and differently positioned views observe the same writes.
- `DataView` implements the currently mapped `get_float64` operation with view bounds and explicit byte order. The
  remaining standard numeric get/set operations are present too, ready for the compiler's ambient member table to
  expose them.
- `TextDecoder` decodes UTF-8, strips an initial BOM, and applies replacement-character recovery to malformed input.
  `String::from_code_point` handles BMP and astral scalars and rejects invalid values.
- `parse_int`, `to_number`, `Object`/`object_keys`, interned `Symbol::for_key`, and `Url::protocol` provide the exact
  spellings in the compiler binding table.
- `RegExp`, captures, global match state, and string match/replacement have an initial ECMAScript-compatible lane.
  It remains a planned capability while Unicode patterns and the full replacement contract are unfinished.
- The compiler-named `Intl` types have a deterministic locale-neutral baseline. Locale-aware behavior and option
  handling remain planned and will need an explicit native dependency or host policy.

CMake and Bazel expose every new public header. The CMake development build compiles the runtime, native tween, SDL
host path, and each public header independently. The runtime tests cover shared binary storage, endian and bounds
behavior, malformed UTF-8, numeric conversion, symbols, URLs, regular expressions, and the locale-neutral baseline.

`Json::parse` and `Json::stringify` remain deliberately absent. A JSON value model must retain null, booleans,
numbers, strings, arrays, and objects and must interoperate with compiler-emitted structural types. A placeholder
opaque value would make headers compile while losing the source behavior.

## Current upstream blocker

The full SDK graph cannot be regenerated successfully at `3f86fcb`. `npm run sdk:generate` spent 57 minutes at one
full CPU core with about 2.3 GiB resident memory and produced no candidate file before it was stopped. The same graph
completed in 82 seconds at `14a9ff4`.

The pinned compiler's own C++ backend test file also has six regressions at this revision: 569 tests pass, one is
skipped, and six fail. Four failures report a missing synthetic `__type[type]` runtime binding; one fails structural
type substitution for a generic class; and one no longer emits the expected intersection-type refusal. These are
compiler-owned failures and correlate with the graph regression. The focused runtime binding tests pass.

Because the compiler never returned a candidate tree, [`generated/manifest.json`](../generated/manifest.json) still
truthfully records the last successful generation at `14a9ff4`. It must not be relabeled as `3f86fcb` without output
from that compiler. After the upstream regression is fixed, run:

```sh
npm run rehydrate
npm ci --prefix .dependencies/flight-compiler
npm run sdk:generate
npm run sdk:check
```

Then compile the dependency-closed headers, update the emitted/refused counts in this review, and advance the
compiler's reciprocal flight-cpp pin so its compile gate sees the new runtime surface.

## Next compiler/runtime edge

flight-cpp already provides snake-case operations for `DataView.getInt8`, `getUint8`, `getInt16`, `getUint16`,
`getInt32`, `getUint32`, `getFloat32`, and every corresponding setter. It also exposes typed-array `byte_length`.
Adding these members to the compiler ambient and member tables should unlock binary parsers without another runtime
round trip.

The remaining compiler-owned work listed in the adoption register should stay upstream: package-graph type evidence
for isolated probes, for-of destructuring with imported element types, unbounded iterable spread, imported anonymous
object evidence, and executable top-level module initialization. flight-cpp should not compensate with permissive
runtime types or generated-source rewriting.

The host boundary is unchanged. SDL owns lifecycle and native GL, Vulkan, and wgpu surface acquisition. Generated
Flight renderer packages own rendering. Browser, media, Node, and tooling globals require explicit host manifests;
they do not belong in the portable runtime.
