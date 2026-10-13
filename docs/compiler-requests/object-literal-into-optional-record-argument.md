# An object literal passed to an optional `Record` parameter must be constructed before the optional

`reportImportDiagnostic(sink, severity, kind, origin, detail?: Readonly<Record<string, boolean | number | string>>)`
takes an optional record. A call that writes the detail as an object literal is emitted as a brace
initializer in the argument position, and C++ cannot build both the `flight::Record` and the
`std::optional` around it from one brace list:

```text
error: no matching constructor for initialization of
'std::optional<flight::Record<flight::String, std::variant<bool, double, flight::String>>>'
```

The emitted function is not refused; the header fails at compile time.

## Minimal TypeScript

```ts
export function report(detail?: Readonly<Record<string, boolean | number | string>>): void {}
export function caller(count: number): void {
  report({ count, reason: 'short' });
}
```

## Required C++ shape

Materialize the contextual `Record` first, then construct the optional from it:

```cpp
report(std::optional<flight::Record<flight::String, std::variant<bool, double, flight::String>>>{
    flight::Record<flight::String, std::variant<bool, double, flight::String>>{/* entries */}});
```

## Affected Flight modules and carried workarounds

Every carrier binds the literal to a local declared with the parameter's own record type immediately before
the call and passes the local:
- `@flighthq/font-formats`: `font-formats-name-woff-diagnostic-details`;
- `@flighthq/particles-formats`: `particles-formats-diagnostic-details`;
- `@flighthq/scene3d-formats`: `scene3d-formats-gltf-camera-diagnostic-details`;
- `@flighthq/swf`: `swf-diagnostic-detail-records`;
- `@flighthq/skeleton2d-formats`: `skeleton2d-formats-diagnostic-detail-number-locals` for the three binary
  entry-point sites (`spineBinaryParse.ts`, `spineBinaryVersioned.ts`).

Still failing on it at `a627849`, uncarried: `@flighthq/skeleton2d-formats` `dragonBonesParse.ts`,
`spineBinaryDrawOrderTimelineHandler.ts`, `spineBinarySkinsHandler.ts`, `spineBinarySlotsHandler.ts`, and the
string-valued `{ version }` detail in `spineBinaryVersioned.ts`, which the number-locals patch does not touch (the
first diagnostic of each header; the package has 41 object-literal detail calls across 21 modules, so more sit
behind other blockers).
