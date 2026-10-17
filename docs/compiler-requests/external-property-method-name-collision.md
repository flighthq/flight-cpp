# A bound external constant must keep its member target name

At Flight `a627849` / compiler `806d257`, `scene2d-gl/glVelocity.ts` saves the current viewport:

```ts
const prevViewport = gl.getParameter(gl.VIEWPORT) as Int32Array;
```

The emitted `render_gl_velocity` instead reads `gl.get_parameter(gl.viewport)`. The host deliberately
uses distinct members in `include/flight/host_sdl/webgl.hpp`:

```cpp
static constexpr std::uint32_t viewport_constant = 2978;
void viewport(int x, int y, int width, int height) const;
```

A declaration-only candidate for the independently refused `ensure_gl_velocity_program` clears the
first missing-name error and makes the actual unused-header compilation fail at this read:
`reference to non-static member function must be called`, scratch `gl_velocity.hpp:410` (stock408).
The source asks for a constant value, not the `viewport` function, and calling that function would
change the operation rather than provide the constant.

The bound `GlContext`/WebGL2RenderingContext property access must elect the host constant's target
name, so this read becomes `gl.get_parameter(gl.viewport_constant)`. Preserve `gl.viewport(...)` as
the viewport-setting method. The SDL GL profile already has per-member target-name declarations;
this property needs a coherent binding and emission choice through the bound Flight interface.
No binding, compiler rule or native body is changed by this request.

The following typed-query assertions are a separate inspected seam, not another measured diagnostic:
the emitted code casts `GlParameterValue` directly to `Int32Array` and `Float32Array`, while the
current host value exposes numeric Array/SequenceView conversions and no such typed-array conversions.
The query result must retain its declared value domain and values; fixing the property name alone
is not evidence that those casts compile or that saved state can be restored. No subsequent candidate
or runtime query adaptation was tested in this arc.

The actual production original1 check remains0/1 at missing `ensure_gl_velocity_program:155`.
The exact declaration candidate is rejected before positive unused-header link/run qualification;
no expected negative-link or GL rendering result is claimed. See the
[retained program caller](retained-declaration-calls-omitted-callee.md#g2-gl-velocity-program-declaration-fails-the-unused-header-compile)
and [cache value-owner request](contextual-union-map-value-carrier.md#g2-gl-velocity-program-lookup-and-rebind).
