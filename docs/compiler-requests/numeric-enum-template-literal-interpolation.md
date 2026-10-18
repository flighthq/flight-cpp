# A numeric enum interpolated into a template literal has no string conversion

A TypeScript numeric `enum` value inside a template literal is stringified as its number: `${RenderRegistryTable.MaterialRenderer}` is `"2"`. The emitter lowers the interpolation to `flight::to_string(registry)` with the `enum class` operand. The runtime has no `to_string` for an arbitrary `enum class`, and should not invent one, because its spelling is the source's number and not the enumerator's name.

Measured at flight-compiler `806d257` with the current patch stack, GCC 15.2:

```text
flight/render/render_registry_guards.hpp:127:136: error: no matching function for call to 'to_string(flight::types::RenderRegistryTable&)'
```

The source is `renderRegistryGuards.ts`, the `logOnce` key `` `render:registry-miss:${stateId}:${registry}:${kind}` ``, where `registry: RenderRegistryTable` is a numeric enum.

## Requested lowering

A numeric-enum operand in a template literal, or in `String(x)`, lowers to the number's own string conversion: `flight::to_string(static_cast<double>(registry))`. That gives the same text JavaScript produces for the enum's numeric value. A string enum keeps its string value. No enumerator-name table is introduced.

[`compiler-numeric-enum-string-conversion.patch`](../../source-patches/compiler-numeric-enum-string-conversion.patch) prepares this correction at the explicit source
operation. Declaration identity resolves local, imported and aliased enums; numeric alternatives in
optional and variant storage use the same conversion after their existing presence checks. The
numeric namespace wrapper also needs this projection: the old output silently selects the runtime's
boolean conversion and prints `true` for `2`. Empty enums retain the numeric carrier rather than
vacuously selecting a string wrapper. Existing enum-object/reverse-map support is unchanged.

The new focused tests pass with real emitted native executables: 39 source results match in both
local and barrel-imported modes. They cover duplicate values whose reverse-map name differs from
their numeric text, unnamed values, aliases and members, string enums, null/undefined and mixed
carriers, wrapper negative zero/NaN/infinity, Unicode, one producer evaluation and exact exception
owners. Both tests fail against the old ordered 92-patch compiler, including native compilation at
the actual enum `to_string` calls. An independent whole emitted namespace fixture produces the old
wrong `true` and the corrected `2`. Eighteen existing enum/template/sentinel tests pass. The full
static comparison keeps the same baseline failures; preparation does not claim a clean static sweep.

The fresh current stack of 93 prior patches plus this correction applies and builds, and the two
new tests pass again with all 78 native comparisons. Reversing the two-file correction privately
restores the exact current baseline backend and removes only its new test. Repository source-patch
automation passes 2/2. The 94-patch static check retains the same diagnostic sets as the earlier
baseline: 38 formatting paths, four lint diagnostics, 12 ordering errors and six foreign fixture type
errors; archive-only Git metadata failures and the existing membership API report drift also remain.

## Affected

`render_registry_guards.hpp` and the 8 headers that reach it. It became the first failure after `compiler-membership-test-presence` lowered the earlier `in` sites; see the [membership request](membership-test-lowered-literally.md).

These eight rows remain unqualified. The complete original public header also references the
refused `getRenderRegistriesMissMessage`, whose nominal `RenderState` cannot represent the backend
owners it probes. That independent membership/heritage dependency remains with its owner; no body,
stub or extracted production caller is supplied here. The other original exported numeric-enum
converter found by the scoped source audit belongs to `@flighthq/tool-capture`, whose declared Node
environment is outside this SDL profile. No scoped generation, header promotion or whole-cause
completion follows from the enum preparation alone.
