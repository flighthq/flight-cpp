# flight-cpp API Review

Date: 2026-10-12
Revised: 2026-10-12

The flight-cpp SDK is designed as a C/C++ library, written in TypeScript, and
lowered to C++. The generated output has a procedural, C-style shape: free
functions, out-parameters, type names encoded in function names, factory
functions. This shape is correct and intentional. It supports aggressive tree
shaking for web targets and provides a natural native API surface.

## Design constraints

Two constraints govern the API design:

1. **C-style procedural API.** The SDK targets the C idiom: free functions,
   out-parameters, type-encoded names, explicit lifecycle. This is not a defect
   to fix — it is the design. OOP (methods on types, constructors, operator
   overloads) is out of scope.

2. **C ABI as the primary API surface.** The public API must be expressible as
   `extern "C"` with C-compatible types. This is a hard requirement, not a
   future consideration. The C ABI is the universal FFI boundary — it is what
   Python (ctypes/cffi), Rust (bindgen), Go (cgo), C# (P/Invoke), Java
   (JNI/Panama), Zig (@cImport), Lua (LuaJIT FFI), Swift, and every other
   language's FFI binds to. C++ types (templates, namespaces, exceptions,
   shared_ptr) cannot cross this boundary.

   The implementation remains C++. The runtime uses C++20 features (coroutines,
   concepts, templates) internally. But the generated SDK surface — the
   functions users call — must have signatures that can be expressed in a C
   header with opaque handles.

   This is the same architecture as Dawn/webgpu.h (C header, C++ implementation),
   PhysX (C++ engine, C API), and Skia (C++ internals, skia-c bindings). The
   implementation language is an internal detail; the API contract is C.

### What this validates

The current generated code gets several things right:

- **Free functions with type-encoded names.** This is the only shape that
  survives `extern "C"`. You can't export methods, overloads, or templates
  across a C ABI.
- **Out-parameters.** C FFI can't reliably return structs across all
  platforms/languages. Out-parameters via pointer are universally safe. (Small
  scalars like `double` are fine to return directly, which the API already does.)
- **Factory functions (`create_*`).** The standard C lifecycle pattern.
- **Type names in function names.** Without method dispatch, the function name
  must carry the type context. `flight_add_vector2(out, a, b)` is correct.

### What this invalidates

Several current patterns cannot cross a C ABI and must change:

- **Nested C++ namespaces.** `extern "C"` is flat. `flight::camera_controls::`
  becomes `flight_` at the boundary. The namespaces add hierarchy the C header
  can't express.
- **StructuralRef in function signatures.** Template metaprogramming cannot cross
  `extern "C"`. Every function that takes `StructuralRef<RowReadonly<...>>` is
  unbindable from other languages.
- **`row_get<RowKey<"x">>(ref)` for member access.** This is C++ template
  machinery accessing a plain struct member. The C surface needs `ref->x`.
- **`std::variant<..., flight::Null, flight::Undefined>`.** Not representable
  in C. Nullable parameters become a pointer that can be NULL.
- **`std::optional<T>` in public signatures.** Not C-compatible. Becomes either
  a nullable pointer or a separate `bool has_field` + value convention.
- **`Ref<T>` (shared_ptr/value conditional alias).** The C surface uses opaque
  handles with retain/release. `Ref<T>` is an internal implementation detail.
- **Function overloading.** C has no overloading. Each variant needs a distinct
  name (`flight_create_vector2`, `flight_create_vector2_from_polar`).
- **Exceptions.** Cannot cross `extern "C"`. Errors become return codes or an
  error-query pattern (`flight_get_last_error()`).

---

## 1. Snake-case lowering of numeric suffixes

**Status:** Broken — must fix.

The code generator lowers CamelCase identifiers to snake_case by inserting
underscores at every uppercase-to-lowercase transition. This produces incorrect
results for numeric suffixes:

| TypeScript | Current | Correct |
|------------|---------|---------|
| `Camera2D` | `camera2_d` | `camera_2d` or `camera2d` |
| `Node3D` | `node3_d` | `node_3d` or `node3d` |
| `Scene3DGlPipeline` | `scene3_dgl_pipeline` | `scene_3d_gl_pipeline` |

In a C API where function names carry the full type context, readability is
everything. `flight_zoom_camera2_dat_screen_point` is not readable.

**Fix:** Treat `[digit][A-Z]` sequences (`2D`, `3D`) as atomic tokens in the
lowering algorithm.

**Effort:** Low.

---

## 2. Flat naming — single `flight_` prefix

**Status:** Required by C ABI constraint.

`extern "C"` has no namespaces. The C header must use a flat prefix. The current
nested namespace structure:

```cpp
flight::camera::create_camera2_d(...)
flight::camera_controls::update_camera2_dfollow(...)
flight::geometry::add_vector2(...)
```

becomes, at the C boundary:

```c
FlightCamera2DRef flight_create_camera_2d(double w, double h);
void flight_update_camera_2d_follow(FlightCamera2DRef cam, double x, double y,
                                     double dt, FlightCamera2DFollowOptionsRef opts);
void flight_add_vector2(FlightVector2Ref out, const FlightVector2Ref a,
                         const FlightVector2Ref b);
```

This is consistent with how C libraries work: SDL uses `SDL_`, sokol uses `sg_`,
stb uses `stbi_`. One prefix, globally unique names, no hierarchy to memorize.

The C++ layer can optionally keep a single `namespace flight` that wraps the
same functions for C++ consumers who prefer `flight::create_camera_2d()` over
`flight_create_camera_2d()`. But the C header is the source of truth for naming.

**Recommendation:** Design the flat C names first. Derive the C++ namespace
grouping from the C names, not the other way around.

**Effort:** Medium — generator change plus API break.

---

## 3. StructuralRef / readonly / row_get — hard blocker

**Status:** Cannot ship a C API without resolving this.

StructuralRef is template metaprogramming. It cannot appear in `extern "C"`
signatures. Every function currently typed as:

```cpp
void add_vector2(
    flight::types::Vector2Like out,
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::Vector2Like>>> a,
    flight::StructuralRef<flight::RowReadonly<flight::RowOf<flight::types::Vector2Like>>> b);
```

must become:

```c
void flight_add_vector2(FlightVector2Ref out, const FlightVector2Ref a,
                         const FlightVector2Ref b);
```

Similarly, member access via `row_get<RowKey<"x">>(ref)` must become direct
struct member access in the implementation. The struct has `double x` as a
member. Access it with `ref->x`.

**What needs to change:**

1. **Readonly params:** `StructuralRef<RowReadonly<...>>` → `const T*` at the
   C boundary. The read-only guarantee is `const`. This is C89.
2. **Member access:** `row_get<RowKey<"x">>(ref)` → `ref->x`. Resolve property
   access at generation time, not at C++ compile time.
3. **Nullable params:** `std::variant<..., Null, Undefined>` → pointer that can
   be NULL.

**Partial and Pick:** If the TypeScript source uses `Partial<T>` or `Pick<T, K>`
in a function signature, the generator must resolve those to concrete struct
types at generation time. A `Partial<Camera2DOptions>` becomes a
`FlightCamera2DOptions*` where all fields are nullable/zeroed, or a dedicated
partial struct with explicit `has_` flags. This is the hard case — the generator
can no longer defer structural subtyping to C++ templates. It must resolve it
statically.

**Effort:** High — the single largest change required, but it is a hard blocker
for the C ABI.

---

## 4. Handle lifecycle — retain/release/destroy

**Status:** Required by C ABI constraint.

The current API uses `Ref<T>` (which resolves to `shared_ptr<T>`) for object
lifetime. At the C boundary, this becomes opaque handles with explicit lifecycle
management. The convention uses two distinct mechanisms, each named honestly for
what it does:

### retain / release — reference counting

`flight_retain` and `flight_release` manage the reference count. This is the
universal lifecycle for any handle obtained from `flight_create_*`. "Release"
is honest: you are releasing your reference, not destroying the object — it may
live on if other references exist.

```c
FlightVector2Ref v = flight_create_vector2(1.0, 2.0);
/* ... use v ... */
flight_release(v);  /* decrement refcount; object freed when it hits zero */
```

For shared ownership:

```c
FlightVector2Ref v2 = flight_retain(v);  /* increment refcount */
/* ... both v and v2 are valid ... */
flight_release(v);
flight_release(v2);  /* object freed here, last reference gone */
```

`flight_retain` / `flight_release` are generic — a single pair for all handle
types. All handles share the same ref-counting mechanism internally. This
matches CoreFoundation's `CFRetain` / `CFRelease` pattern.

Foreign languages attach `flight_release` to their GC destructor mechanism:
Python's `__del__`, Rust's `Drop`, Go's `runtime.SetFinalizer`, C#'s
`SafeHandle`, etc. This lets the host language's GC drive the release naturally.

### destroy — eager teardown of external resources

`flight_destroy_*` is reserved for types that hold external resources (GL
contexts, audio devices, GPU buffers) that need eager teardown — resources
that should not wait for the refcount to hit zero. This parallels the existing
`destroy` convention in the TypeScript SDK, where `destroy` reaches into native
resources. `dispose` retains its existing distinct meaning in the TS SDK.

```c
FlightGlContextRef ctx = flight_create_gl_context(/* ... */);
/* ... use ctx ... */
flight_destroy_gl_context(ctx);  /* tears down GL resources NOW */
flight_release(ctx);              /* release the handle */
```

Not every type gets a `destroy_*` — only those with external resources. The
generator knows which types these are from the TS source (types that already
have `destroy` methods).

### C++ layer

In C++, `Ref<T>` (shared_ptr) handles release automatically via RAII — when the
last reference goes out of scope, the destructor runs. For types with external
resources, the shared_ptr custom deleter calls the destroy logic automatically.
No explicit retain/release/destroy calls needed in C++ code.

The layering:

```
TS:    create_*()          → GC handles cleanup; destroy() for external resources
C++:   flight::create_*()  → Ref<T> (shared_ptr), RAII handles release
C:     flight_create_*()   → opaque handle, user calls flight_release()
```

**Effort:** Medium — straightforward once the handle abstraction exists. The
existing `c_runtime.cpp` already has atomic refcounting for the C ABI — this
extends that pattern to all SDK types.

---

## 5. Opaque handles and accessor functions

**Status:** Decided — opaque handles for now.

All SDK types are opaque at the C boundary. Consumers cannot access struct
fields directly. Field access goes through accessor functions:

```c
double x = flight_get_vector2_x(v);
flight_set_vector2_x(v, 3.0);
```

This locks nothing about the struct layout. Fields can be reordered, added, or
removed without breaking ABI. Direct struct access (exposing the layout in the
C header) could be revisited later for performance-critical types like Vector2
where the layout is unlikely to change, but is out of scope for now.

**Effort:** Low — the generator already emits accessor-style functions. The
change is to stop exposing struct definitions in the public C header.

---

## 6. Header organization

**Status:** Decided.

### C headers

```c
#include <flight/flight.h>      /* universal — includes everything */
#include <flight/geometry.h>    /* geometry package */
#include <flight/camera.h>      /* camera package */
#include <flight/scene2d.h>     /* scene2d package */
```

C package headers are monolithic — all declarations for the package in one file.
Each is self-contained (includes its own dependencies). The universal
`flight/flight.h` includes all package headers.

### C++ headers

```cpp
#include <flight/flight.hpp>    /* universal — includes everything */
#include <flight/geometry.hpp>  /* includes per-file headers internally */
```

C++ package headers include per-file sub-headers (`flight/geometry/vector2.hpp`,
`flight/geometry/matrix.hpp`, etc.) for code organization. Users include the
package header; the internal split is an implementation detail.

**Effort:** Low.

---

## 7. Container conventions

**Status:** Decided for now — opaque handles with accessor functions.

`Array<T>`, `Map<K,V>`, `Set<T>` are C++ templates and cannot cross the C
boundary. At the C surface, containers are opaque handles with accessor
functions:

```c
FlightArrayRef arr = flight_get_some_array(obj);
int32_t len = flight_array_length(arr);
FlightVector2Ref item = flight_array_get_vector2(arr, 0);
flight_release(arr);
```

This is the only approach that works mechanically for the current ported output
without rethinking the container model from the TS side. A more considered
approach (e.g. C-style pointer + length for simple cases, iterator patterns for
complex ones) may be worth revisiting once the basic C API is working.

**Effort:** Medium — requires generating type-specific array accessor functions
or a type-erased accessor pattern.

---

## 8. Enum / discriminated union representation

**Status:** Open question — word order needs decision.

TypeScript string unions (`"AmbientLight" | "DirectionalLight"`) are currently
`inline const flight::String` constants. The C API needs C enums. The naming
convention should preserve the original word order from the TypeScript source:

```c
typedef enum {
    FLIGHT_AMBIENT_LIGHT,       /* from AmbientLight — preserves word order */
    FLIGHT_DIRECTIONAL_LIGHT,   /* from DirectionalLight */
    FLIGHT_POINT_LIGHT,         /* from PointLight */
} FlightLightKind;
```

Flipping word order for alphabetical grouping (`FLIGHT_LIGHT_AMBIENT`,
`FLIGHT_LIGHT_DIRECTIONAL`) would match some C conventions but diverges from the
TS source names and creates a mapping users must learn. Preserving word order
keeps the C enum predictable from the TS documentation.

**Open question:** Confirm that preserving TS word order is the right default,
or whether there are cases where grouping by prefix is worth the divergence.

**Effort:** Low — mechanical conversion in the generator.

---

## 9. String handling

**Status:** Open question.

`flight::String` is UTF-16 internally (matching JavaScript semantics). The C API
must accept and return C-compatible string types. Options:

1. **UTF-8 only (`const char*`).** The modern C convention. Most FFI consumers
   expect UTF-8. Requires conversion at the boundary.
2. **UTF-16 only (`const uint16_t*` or platform `wchar_t`).** Matches internal
   representation. No conversion cost. But UTF-16 is uncommon in C APIs outside
   Windows.
3. **Both.** Provide both UTF-8 and UTF-16 variants for string-accepting
   functions. More API surface but avoids forcing a choice on consumers.

It is tempting to support multiple string types, but this multiplies the API
surface for every function that touches strings.

**Open question:** Decide whether to commit to one string encoding at the C
boundary or support multiple. UTF-8 is the pragmatic default for maximum
compatibility.

---

## 10. Error handling

**Status:** Required by C ABI constraint.

C++ exceptions cannot cross `extern "C"`. The current runtime has 169 `throw`
sites. The C boundary needs an error convention. Options:

1. **Return codes.** Each function returns `int` (0 = success, nonzero = error
   code). Out-parameter for the actual result. Most C-like but changes every
   function signature.

2. **Error query.** Functions that can fail return a sentinel (NULL handle, NaN,
   -1). Call `flight_get_last_error()` for details. Matches `errno` / OpenGL's
   `glGetError` pattern. Less invasive.

3. **Error callback.** Register a global error handler with
   `flight_set_error_handler(callback, userdata)`. The runtime calls it instead
   of throwing. Least invasive but loses per-call error handling.

**Recommendation:** Option 2 for most functions (return NULL on failure, query
for details). Option 1 for functions where failure is expected and must be
handled per-call. Avoid option 3 as the sole mechanism.

**Effort:** Medium — requires wrapping every `extern "C"` entry point in a
try/catch that translates exceptions to the error convention.

---

## 11. Async / Task\<T\>

**Status:** Design question — needed if async SDK functions are exposed to C.

`Task<T>` is a C++20 coroutine type. It cannot cross `extern "C"`. If any SDK
functions are async, the C API needs a callback convention:

```c
typedef void (*FlightCallback)(void* result, void* userdata);
void flight_load_texture_async(const char* path, FlightCallback on_complete,
                                void* userdata);
```

Or a future/poll pattern:

```c
FlightFutureRef flight_load_texture_async(const char* path);
bool flight_future_is_ready(FlightFutureRef future);
FlightTextureRef flight_future_get(FlightFutureRef future);
flight_release(future);
```

**Recommendation:** Callback + userdata is the most universally bindable pattern.
Every FFI system can pass function pointers and a context value.

**Effort:** Medium — mechanical wrapping, but need to decide on the convention.

---

## 12. JS-isms in generated code

**Status:** Should fix — these leak through the C boundary or signal
untranslated source.

| Pattern | Current | C equivalent |
|---------|---------|-------------|
| Null/Undefined variants | `std::variant<..., flight::Null, flight::Undefined>` | NULL pointer |
| Power function | `flight::number_power(x, 2.0)` | `x * x` |
| Parenthesized assignments | `(out->x = (ax + bx));` | `out->x = ax + bx;` |
| Entity allocation ceremony | `allocate_entity` / `initialize` / `finish_entity` visible in generated code | Internal detail; C surface sees only `flight_create_vector2(x, y)` returning a handle |
| `flight::Null`, `flight::Undefined` types | Exist as distinct types | Should not exist in C surface; NULL pointer covers both |

**Effort:** Low — generator cleanup.

---

## 13. ABI versioning

**Status:** Future work — needs a mechanism eventually.

If the C API is consumed by other languages via FFI, the ABI is a contract.
Adding, removing, or reordering function parameters breaks bindings. A
versioning mechanism should be introduced before the API stabilizes. Options
include:

- A version constant in the header (`FLIGHT_API_VERSION`)
- Versioned function names for breaking changes (`flight_create_camera_2d_v2`)
- A function table / dispatch struct pattern (like Vulkan's dispatch tables)

Not blocking for initial development, but should be in place before the API is
declared stable.

---

## Summary

| # | Issue | Status | Effort | Priority |
|---|-------|--------|--------|----------|
| 3 | StructuralRef / row_get in signatures | **Hard blocker** — can't cross C ABI | High | Highest |
| 4 | Handle lifecycle (retain/release/destroy) | **Required** — C ABI needs explicit lifecycle | Medium | High |
| 10 | Error handling across C boundary | **Required** — exceptions can't cross `extern "C"` | Medium | High |
| 1 | `camera2_d` naming | Broken — must fix | Low | High |
| 2 | Flat `flight_` prefix | Required by C ABI — no namespaces in `extern "C"` | Medium | High |
| 5 | Opaque handles / accessor functions | Decided — opaque for now | Low | High |
| 6 | Per-package C/C++ headers | Decided — `flight/geometry.h`, `flight/flight.h` | Low | Medium |
| 7 | Container conventions | Decided — opaque handles with accessors for now | Medium | Medium |
| 8 | Enum / discriminated union naming | Open — word order needs confirmation | Low | Medium |
| 9 | String handling | Open — encoding at C boundary undecided | Medium | Medium |
| 11 | Async convention | Design question — needed if async is exposed | Medium | Medium |
| 12 | JS-isms | Should fix | Low | Medium |
| 13 | ABI versioning | Future work — before API stabilizes | Low | Low |

### Decisions made

- **Procedural shape:** Validated. Free functions, out-parameters, type-encoded
  names, factory functions — all correct and required by the C ABI constraint.
- **C ABI is the primary surface.** `extern "C"` with opaque handles. C++
  implementation stays internal. This enables CFFI binding from any language.
- **Opaque handles.** All types are opaque at the C boundary. Field access goes
  through getter/setter functions. Direct struct access may be revisited later
  for stable, performance-critical types.
- **`flight_` prefix.** All functions use a flat `flight_` prefix. No nested
  namespaces. C++ consumers optionally get `namespace flight` wrapping the same
  functions.
- **`retain` / `release` for refcounting.** Honest naming — `release` says you
  are releasing your reference, not destroying the object.
- **`destroy_*` for eager teardown.** Reserved for types with external resources
  (GL, audio, GPU). Parallels the existing TS `destroy` convention. Not every
  type gets a `destroy_*`.
- **Per-package headers.** `flight/geometry.h`, `flight/camera.h`,
  `flight/scene2d.h`, etc. — each self-contained. C headers are monolithic per
  package. C++ package headers include per-file sub-headers internally.
  Universal header: `flight/flight.h`.
- **Containers.** Opaque handles with accessor functions for now. May revisit
  once the basic C API is working.

### Open questions

- **Enum word order:** Preserve TS source order (`FLIGHT_AMBIENT_LIGHT`) or
  reorder for C grouping (`FLIGHT_LIGHT_AMBIENT`)? Preserving source order is
  the current recommendation but needs confirmation.
- **String encoding:** UTF-8 only, UTF-16 only, or both at the C boundary?
- **Async convention:** Callback + userdata, or future/poll pattern?
- **Error convention:** Return-code vs. sentinel + error-query vs. callback.

### The hard blocker

§3: StructuralRef cannot cross `extern "C"`. The generator must resolve
structural typing at generation time and emit direct struct member access and
`const` pointers. Everything downstream (flat naming, handle lifecycle, error
handling) is straightforward once the generated code emits C-compatible
signatures.
