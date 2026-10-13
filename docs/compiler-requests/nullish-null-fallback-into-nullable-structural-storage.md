# A `?? null` fallback written into nullable structural storage must produce that storage's absent state

`state.canvasHost = obj?.canvasHost ?? null` writes into a member declared
`Readonly<HostCanvasCapability> | null`. The emitter stores that member as
`std::optional<flight::StructuralRef<RowReadonly<RowOf<Ref<HostCanvasCapability>>>>>`.

It lowers the coalesce as a conditional whose null arm is `nullptr`:

```cpp
flight::row_set<flight::RowKey<"canvasHost">>(state,
    ((std::holds_alternative<flight::Null>(canvas_host) || std::holds_alternative<flight::Undefined>(canvas_host))
         ? nullptr
         : std::get<0>(canvas_host)));
```

`nullptr` and a `StructuralRef` have no common type, so the header does not compile:

```
error: non-pointer operand type 'variant_alternative_t<0UL, variant<flight::StructuralRef<...HostCanvasCapability...>, flight::Null, flight::Undefined>>' incompatible with nullptr
```

## Minimal TypeScript

```ts
interface Host { readonly id: number }
interface State { host: Readonly<Host> | null }
interface Options { host?: Readonly<Host> | null }

export function apply(state: State, options: Options): void {
  state.host = options.host ?? null;
}
```

## Required C++ shape

The conditional's value is the member's storage type. Its null arm is that storage's absent state:

```cpp
flight::row_set<flight::RowKey<"canvasHost">>(state,
    (is_null_or_undefined ? std::nullopt : std::optional{std::get<0>(canvas_host)}));
```

That is the state the runtime already writes for a bare `nullptr`. `row_set` in
`include/flight/structural_ref.hpp` treats `nullptr` written to a nullable structural value as the
disengaged optional (`writes_unrepresentable_null_as_absence`), because a structural reference has no
pointer-shaped null. The conditional only needs to reach that same state.

Pointer-valued members are a different case and are not part of this request. The same statement
for `displayObjectClipHooks: Scene2DClipHooks | null` (`std::optional<flight::Ref<...>>`) compiles,
because `nullptr` converts to `Ref`. It writes an engaged null pointer, which the runtime deliberately
keeps distinct from an absent property. No emitted header reads that member back, so this request makes
no claim about it.

## Affected modules

At flight `a627849`:
- `@flighthq/render` `renderState.ts`, `initializeRenderState`: `canvasHost` and `imageHost` (two sites).
  `flight/render/render_state.hpp` is included by almost every renderer header, so this one statement
  failed 227 header compilations across the registry migration group (render, render-gl,
  render-wgpu, effects-canvas, effects-gl, scene2d-canvas, scene2d-gl, scene2d-wgpu, scene3d-gl,
  shape-formats).
- `@flighthq/tilemap-formats` emits the same `? nullptr :` spelling in `tiled_json_parse.hpp` and
  `tiled_xml_parse.hpp`. Those sites are not examined here; they are another lane's.

## Workaround carried

The `render-state-nullable-structural-host-fallback` emission repair respells the two render sites
exactly, from `? nullptr : std::get<0>(canvas_host)` to `? std::nullopt : std::optional{std::get<0>(canvas_host)}`.
It does the same for `image_host`. It writes the absent state the runtime itself writes for `nullptr`,
so it changes no absence, identity or ordering. It expires when the emitter gives the conditional's
null arm the storage type's absent state.
