# Package-scoped pin moves must reject consumers of removed headers

The generated inventory currently mixes `@flighthq/types` at
`a62784923f3be814463286c9fb28edfe6ab15789` with Scene3D packages at
`7e2fc7df5378313ceaf21ba8d6ef39cc18a6f4ad`. The types move removed
`flight/types/material_value.hpp`, but seven old-revision generated Scene3D-GL headers and the
Scene3D mesh override still include it. A focused Scene3D build therefore stops before either the
appearance or scene3d-node baseline can compile:

```text
overrides/include/flight/scene3d/mesh.hpp:15:10: fatal error:
  'flight/types/material_value.hpp' file not found
```

The untouched reproduction is:

```sh
PYTHONPATH="$PWD/../.tooling/cmake-python" \
  ../.tooling/cmake-python/bin/cmake --build build -j2 \
  --target flight_cpp_generated_scene3d_tests
```

## Source ownership and generation boundary

This is not a refusal to lower `MaterialValue`. At the current pin,
`packages/types/src/MaterialValue.ts` does not exist and `packages/types/src/Mesh.ts` deliberately
stores `(Material3D | null)[]`; the current-pin types generation is consequently correct to omit the
header. `generated/best-effort.json` likewise has no `MaterialValue` module row.

The repository's `material-concrete-value-path` source patch owned the former declaration. Its
`packages/types/src/MaterialValue.ts` addition defined an open `Material | <built-in materials>`
union, but the patch is marked `pendingRebase` against a627849 and is filtered out before source
patch application. Historical generated output at commit `c57c7a04` contains a valid
`material_value.hpp` for that declaration, proving the compiler emitter can represent the alias.

The failure boundary is instead the package-scoped generation transition. Commit `787a9d1f` moved
the types inventory to a627849 and deleted the provider header while retaining old-revision
consumers. `assertNoOrphanedDependents` and `danglingIncludes` in `scripts/packageScope.mjs` already
encode the relevant checks, but `verifyMixedInventory` only reports rows at another revision as
`pending`; it does not validate the include closure of those retained rows or the configured
override roots.

## Requested generation contract

Before a package-scoped pin move is accepted, validate direct `flight/...` includes across the
resulting generated tree and its configured runtime/override include roots. If the candidate removes
a provider header that a retained package or override still includes, fail the move and name the
provider package, consumer path, and missing include. The diagnostic should require a coordinated
move of the dependent package or an updated source/override, rather than allowing an internally
unbuildable mixed inventory.

For this reproduction, the missing include is `flight/types/material_value.hpp`; the provider is
`@flighthq/types`; and the direct consumers are the Scene3D mesh override plus seven generated
`@flighthq/scene3d-gl` headers.

No compiler lowering change, synthesized compatibility header, or current-pin `MaterialValue`
declaration is requested. The current source's `Material3D` carrier remains authoritative; this
request is only for generator orchestration to reject stale consumers when their provider moves.

## Carrying workaround

None. The repository intentionally does not add a stub generated header or broaden the material
carrier. The blocked Scene3D rows remain blocked until their packages move coherently to the current
pin or the source-level material path is deliberately rebased.
