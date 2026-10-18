# A numeric enum interpolated into a template literal has no string conversion

A TypeScript numeric `enum` value inside a template literal is stringified as its number: `${RenderRegistryTable.MaterialRenderer}` is `"2"`. The emitter lowers the interpolation to `flight::to_string(registry)` with the `enum class` operand. The runtime has no `to_string` for an arbitrary `enum class`, and should not invent one, because its spelling is the source's number and not the enumerator's name.

Measured at flight-compiler `806d257` with the current patch stack, GCC 15.2:

```text
flight/render/render_registry_guards.hpp:127:136: error: no matching function for call to 'to_string(flight::types::RenderRegistryTable&)'
```

The source is `renderRegistryGuards.ts`, the `logOnce` key `` `render:registry-miss:${stateId}:${registry}:${kind}` ``, where `registry: RenderRegistryTable` is a numeric enum.

## Requested lowering

A numeric-enum operand in a template literal, or in `String(x)`, lowers to the number's own string conversion: `flight::to_string(static_cast<double>(registry))`. That gives the same text JavaScript produces for the enum's numeric value. A string enum keeps its string value. No enumerator-name table is introduced.

## Affected

`render_registry_guards.hpp` and the 8 headers that reach it. It became the first failure after `compiler-membership-test-presence` lowered the earlier `in` sites; see the [membership request](membership-test-lowered-literally.md).
