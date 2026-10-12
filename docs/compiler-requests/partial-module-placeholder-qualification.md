# A partially refused module's retained declarations must keep their names in scope

When a module is refused in part, the emitter writes a PARTIAL header that retains the declarations it
could lower. It drops the `using` declarations and includes those retained declarations depend on, so
they name `SwfTagReader`, `Texture2D` or `create_sampler` unqualified, with nothing bringing them into
scope. The placeholder therefore fails to compile for a reason unrelated to the refusal.

## Expected

A retained declaration is emitted with the same qualification and includes it would have in a fully
emitted module. The refusal then costs only the refused declarations.

## Affected modules

`swfVideoHandler.ts` in `@flighthq/swf`. The module is refused for a callable union arm (see
[callable-arm-in-contextual-union](callable-arm-in-contextual-union.md)). Its retained
`readSwfVideoDefinition` and `acquireSwfVideoTexture` are emitted unqualified. With the declarations
added by hand (measured), the header still fails, at the `create_texture` call. That is a texture-lane
override signature mismatch, so a declaration-only repair would not make this header compile, and none is
carried.

## Carrying workaround

[`compiler-placeholder-graph-qualification.patch`](../../source-patches/compiler-placeholder-graph-qualification.patch)
salvages the placeholder inside the emission session, with the same graph-wide context as every other module.
Retained declarations then resolve imports and include their headers. A refused declaration's partially
recorded includes and anonymous structs are rolled back with it. An import is included only when a retained
declaration references it, so nothing the refused code alone used leaks in. Fixture tests pin qualification,
both negative cases, and parity with a fully emitted module. The request closes when flight-compiler salvages
placeholders inside the emission session.
