# External optional result widening

A manifest-declared nullable external call result is already a complete `optional<T>` source carrier.
When stored in `T | null | undefined`, the emitter treats that entire carrier as one present value and
cannot reconstruct the destination's distinct sentinels. The same family previously regressed a plain
`getExtension(...) !== null` test at compiler revision `b05f4df`.

## Minimal TypeScript

```ts
interface Locations { color?: WebGLUniformLocation | null }
export function resolve(
  gl: WebGL2RenderingContext,
  program: WebGLProgram,
  locations: Locations,
): WebGLUniformLocation | null {
  if (locations.color !== undefined) return locations.color;
  const color = gl.getUniformLocation(program, 'u_color');
  locations.color = color;
  return color;
}
```

The binding declares `getUniformLocation` as
`std::optional<WebGlUniformLocation>` with `callResultAbsence: "null"`.

## Expected C++

```cpp
auto color = gl.get_uniform_location(program, flight::String("u_color"));
locations->color = color
    ? Locations::Color{std::in_place_type<WebGlUniformLocation>, color.value()}
    : Locations::Color{std::in_place_type<flight::Null>, flight::null};
if (!std::holds_alternative<flight::Undefined>(locations->color)) {
  return /* null-or-location projection */;
}
```

The call's `null` must never become destination `undefined`.

## Affected modules

Wireframe, PBR, toon, classic, matcap, shaded, debug, unlit, and shadow GL programs, plus the SDL/GL
extension-presence probe.

## Carrying workaround

[`compiler-external-optional-result-widening.patch`](../../source-patches/compiler-external-optional-result-widening.patch)
defers exact manifest carriers to represented-union conversion and selects null/undefined alternatives
directly. It expires when upstream handles optional external results as source unions.

## Not covered: an inferred mutable local

The applied patch handles an immutable result stored into the dual-sentinel cell. At `a627849`,
`glMeshProgram.ts` resolves its lazy caches through `let loc = program.locX; if (loc === undefined) { loc =
gl.getUniformLocation(…) }`, a local inferred from the three-state cell that then receives the two-state
result. It refuses with `external binding storage std::optional<flight::host_sdl::WebGlUniformLocation> requires
one unresolved optional value domain`. See
[the lazy uniform-location request](scene3d-gl-lazy-uniform-location-boundaries.md).
