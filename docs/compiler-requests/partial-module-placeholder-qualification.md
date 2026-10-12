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
