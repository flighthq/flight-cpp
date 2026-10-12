# An indexed access through `AbortSignalEventMap` refuses a whole module

`AbortSignal.addEventListener` is declared with an event-map indexed access — `addEventListener<K extends
keyof AbortSignalEventMap>(type: K, …)` — and the emitter will not compute `AbortSignalEventMap[K]`. The
refusal is at a **signature**, but it withholds every declaration in the module that uses that signal: one
module, forty-seven declarations, forty-three of whose bodies use carriers this SDK already provides.

Measured at flight-compiler `2b687ea` / flight `a627849` against emitted `@flighthq/filesystem`.

## Observed

```
cpp emission failed for @flighthq/filesystem/packages/filesystem/src/filesystem.ts: indexedAccess types
require C++ type computation lowering: object is AbortSignalEventMap, index is unknown
```

`packages/filesystem/src/filesystem.ts` is 433 lines and exports thirty-eight functions; the module's one
refused signature takes the refusal's whole blast radius with it:

```
generated/include/flight/filesystem/filesystem.hpp  -- BEST-EFFORT PLACEHOLDER -- replace this file.
```

The placeholder lists all forty-seven names for a replacement to supply. `@flighthq/filesystem` is
therefore `partial` on this one rule while every other part of the module is ordinary host-capability
forwarding.

## Reduction

Two things separate this refusal from a genuine lowering gap:

- the SDK already provides the carriers the bodies need, so the deferred work is the *signature* only:
  `AbortSignal.addEventListener` in C++ is a plain method taking an event name and a callback;
- the *index* the emitter reports as `unknown` is `keyof AbortSignalEventMap`, a closed key set the
  emitter can enumerate exactly (`'abort'` and its siblings, each mapping to a known listener type).

A module-wide replacement exists as `overrides/include/flight/filesystem/filesystem.hpp`, which supplies
all forty-seven declarations over the generated rows and callback signatures; the package's native test
(`tests/generated_filesystem_test.cpp`) covers the parts of it that do not need a host-side stream fixture.

## What would close this

Compute the indexed access over the map's closed keys — each key's member type is declared, so the union
of members is a finite union the emitter can lower — or bind the event map's member types as named
aliases the signature can reference. Either way the refusal should not be able to reach past the
signature into the bodies.

## Carrying workaround

`overrides/include/flight/filesystem/filesystem.hpp`, status `complete`, supplies the module. The override
is retained rather than re-derived because the emission has no bodies to derive from: the placeholder is
the whole emitted artifact.
