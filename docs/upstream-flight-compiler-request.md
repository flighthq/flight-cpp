# Request to flight-compiler: C++ package graphs and native bindings

This request is ready to send upstream. It was refreshed against Flight `1274ec5` and flight-compiler `8a4b1a0`.
The committed [SDK manifest](../generated/manifest.json) and [refusal ledger](../generated/refusals.json) record the
last complete sweep at compiler `4b44279`; the same Flight checkout is the current reproduction corpus. The runtime
pin remains on `4b44279` because the current compiler emits an incompatible ABI assertion and a full current sweep
does not complete in a practical gate window.

## Requested work

Please add a public C++ package/workspace compilation request that compiles a module graph once, supports explicit
target identity and native external bindings, and emits a deterministic buildable package report. We need this to
compile the Flight SDK into one native package graph and connect it to a handwritten SDL host without teaching the
compiler about SDL.

The work can land incrementally in this order.

### 1. Align the emitted runtime ABI with the pinned runtime

The C++ backend at `8a4b1a0` emits:

```cpp
static_assert(flight::runtime_contract.cpp_abi == 2, "Flight C++ runtime ABI mismatch");
```

The compiler pins flight-cpp `c437f20`, whose `flight::abi_version` is 1. A header emitted by the compiler therefore
rejects the runtime revision the compiler declares as its dependency.

Acceptance criteria:

- Derive the required ABI from a versioned backend/runtime contract input, or emit ABI 1 while `c437f20` remains the
  compiler pin.
- Move the compiler pin and runtime contract together when ABI 2 is intentional.
- Add a gate that compiles one emitted C++ header against the compiler's pinned flight-cpp revision.

### 2. Compile packages and workspaces as one module graph

`compileCompilerCommandLineRequest` currently calls `compileTypeScriptModules` once for every source with a one-item
`sources` array. That discards sibling type/value identity and makes the new cross-module reference planner
unavailable to the directory command. For example, isolated `adjustment.ts` refuses imported
`EntityConstruction` with `indeterminateIdentity`.

The underlying graph API lowers all 22 modules in `@flighthq/adjustments` in about one second and reports the two
actual `FirstTypeNode` diagnostics. C++ emission still rebuilds reference representation planning for each module,
however, and multiple SDK modules consume minutes without completing. A serial full-SDK directory report made only
four headers before it was stopped after 38 minutes; running packages concurrently saturated eleven cores but reached
the same per-module stalls. The graph-context probe also found a current compiler invariant failure in
`assetLibrary.ts`: the `lowering-plan` pass reports a duplicate type-binding identity.

Acceptance criteria:

- Accept many sources carrying their real `packageName`, `sourceFile`, and package root in one request.
- Resolve sibling imports, reexports, interface/class heritage, type/value identity, and cross-package imports before
  C++ emission.
- Accept an explicit package dependency graph or workspace manifest rather than inferring npm layout in the backend.
- Keep report mode useful: return a deterministic per-module refusal with a stable code and source span while
  compiling unaffected modules where the graph permits it.
- Emit all intra-package and cross-package include dependencies, plus a machine-readable package dependency/output
  manifest.
- Reuse graph analysis across modules. A full report over this 2,851-module corpus needs to be practical as a normal
  regeneration/CI gate rather than performing a fresh compiler pipeline per file.

A suitable API could be a new request beside the discovery-oriented directory command. The existing isolated command
can remain available for breadth probes.

### 3. Make C++ namespace and include identity configurable

`convertPackageNameToCppNamespace` currently hardcodes the npm publisher into names such as `flighthq_render_wgpu`.
The publisher is `@flighthq`; the project and public code identity are `flight`.

Acceptance criteria:

- Accept an explicit target mapping per source package, with separate C++ namespace and installed include prefix.
- Apply the mapping consistently to declarations, imports, reexports, forward declarations, diagnostics, and emitted
  file manifests.
- Support this intended mapping without downstream text rewriting:

```json
{
  "@flighthq/types": {
    "namespace": "flight::types",
    "includePrefix": "flight/types"
  },
  "@flighthq/render-wgpu": {
    "namespace": "flight::render_wgpu",
    "includePrefix": "flight/render_wgpu"
  }
}
```

### 4. Accept target-native external symbol bindings

The SDK intentionally refers to platform APIs such as `WebGL2RenderingContext`, `HTMLCanvasElement`, `GPUDevice`,
`GPUCanvasContext`, WebGPU enums, and related values. Native C++ should inject those implementations. SDL only owns
the window/context/surface mechanics; Dawn, wgpu-native, or native GL supplies the graphics API.

Acceptance criteria:

- Accept a versioned C++ external-binding manifest for ambient types and values.
- Each binding can name required headers, the qualified C++ type/value, value versus type space, nullability,
  ownership/borrowing, and any constructor or static-member mapping needed by lowering.
- Diagnose every reachable ambient symbol without a binding; do not silently substitute an unrelated runtime type.
- Permit downstream packages to provide binding manifests without importing their implementation into the compiler.
- Prove the route with the Flight `GlContext`, `WgpuHostBackend`, `WgpuPresentationSurface`, and
  `WgpuRenderSurfaceProvider` dependency closure.

### 5. Emit executable module initialization

Examples and SDK packages contain top-level values and statements whose evaluation order is observable. A native
library needs one definition and deterministic dependency-ordered initialization; a collection of unrelated inline
headers cannot represent the whole module contract.

Acceptance criteria:

- Lower supported top-level function/value declarations and executable statements with ECMAScript module dependency
  and evaluation order.
- Emit an explicit implementation/translation-unit plan, or an equivalent one-definition-safe initialization API,
  so CMake can compile the result into a native library.
- Include initialization units and order in the output manifest.

### 6. Work down the refusal ledger after the graph boundary lands

Once imports carry real identities, prioritize the remaining constructs by count in `generated/refusals.json` rather
than adding host-specific compiler exceptions. The current recurring families include first-type-node resolution,
interface heritage, `unique symbol`, conditional and mapped types, index/construct signatures, computed property
names, and unsupported function statements. GL/WebGPU ambient failures then move to the external-binding lane above.

For each new lowering, an emitted C++ compile test and a TypeScript-versus-native behavioral oracle should accompany
the feature when it has runtime semantics. Type-only erasures still need structural tests proving that public identity
and constraints survive.

## Downstream division of responsibility

flight-compiler owns module graph semantics, lowering, names, external-binding contracts, output layout, and refusal
reports. flight-cpp owns the semantic runtime and CMake packaging. `Flight::HostSdl`, `Flight::HostSdlGl`,
`Flight::HostSdlVulkan`, and `Flight::HostSdlWgpu` own SDL lifecycle and native handle acquisition. The generated Flight
render packages continue to own rendering.
