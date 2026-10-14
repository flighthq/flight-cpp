# `catch (error) { …; throw error; }` rethrows a sliced copy, and a second catch rethrows an undeclared name

The emitter lowers `catch (error)` to `catch (const std::exception& error)` and `throw error;` to `throw error;`.
In C++ that throws a copy of the *static* type, `std::exception`, so the caught value is sliced. A
`flight::TypeError` thrown by a callback comes back as a plain `std::exception` whose `what()` is
`"std::exception"`, and a caller's `catch (const flight::TypeError&)` no longer matches. The headers compile, so
nothing reports it. The source's contract is to rethrow the same value.

A second defect shows up in a function with two such handlers: the second catch declares `error`, but its rethrow
names `error_2`, which is undeclared, so that header does not compile.

Measured at flight-compiler `2b687ea` with this repository's 44 compiler patches applied. Both modules emit.

## Minimal TypeScript (measured)

```ts
export interface Box { value: number; }

export function guarded(box: Box, hook: (box: Box) => void): void {
  const saved = box.value;
  try {
    hook(box);
  } catch (error) {
    box.value = saved;
    throw error;
  }
}

// The same handler twice in one function: the second rethrow is emitted as `throw error_2;`.
export function guardedTwice(box: Box, first: (box: Box) => void, second: (box: Box) => void): void { /* … */ }
```

Probe, compiled against the emitted `guarded` with a hook that throws `flight::TypeError("hook")`:

```text
catch-rethrow: SLICED to std::exception what=std::exception value=1
try-finally:   TypeError kept, value=1
```

The control rewrites the handler as `let completed = false; try { hook(box); completed = true; } finally { if
(!completed) box.value = saved; }`. The emitter lowers `finally` with `catch (...)`, `std::current_exception()`
and `std::rethrow_exception`, which rethrows the original object.

## Expected behavior

A catch whose body ends by rethrowing its own binding should lower to `throw;`, or to
`std::rethrow_exception(std::current_exception())`, so the original object propagates unchanged, whatever its
type. It should also catch every thrown value, not only `std::exception`
([catch-handler-breadth-attempt-all](catch-handler-breadth-attempt-all.md)). The rethrow must name the binding its
own catch declared.

## Affected sites at a627849

Emitted `catch (const std::exception& error) { …; throw error; }`, which compiles and slices:
- `@flighthq/physics2d` `step.hpp`: the pre-solve and post-solve contact hooks (`step.ts` lines 502 and 673), and
  the impact-contact pre-solve hook (line 1184), whose rethrow is emitted as the undeclared `error_2`.
- `@flighthq/physics3d` `step.hpp` (line 197) and `continuous.hpp` (line 214).
- `@flighthq/video` `video_resource_from.hpp` (line 70).

`assets/asset_library.hpp`, `debug/debug.hpp` and `scene3d_resources/image_resource_fetch.hpp` also contain a
`throw error;` and are worth checking against this pattern. They are not changed here.

## Carrying workaround

For physics2d only, [`physics2d-contact-hook-restore-finally`](../../source-patches/manifest.json) writes each
hook guard as the control above. The other packages are left to their owners.
