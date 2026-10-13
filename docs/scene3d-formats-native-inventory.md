# Scene3D formats native inventory at a627849

The pinned `@flighthq/scene3d-formats` package has 111 source modules. Before this slice its
committed row was still at Flight `7e2fc7d` with 40 modules (20 emitted and 20 refused). The scoped
regeneration in this slice moves only this package to the repository's current Flight pin and records
73 emitted modules and 38 refused modules. The migration changes the package's 111 generated
headers plus the eight shared generated build and inventory ledgers; it does not regenerate a frozen
3D neighbor.

“Emitted” is not synonymous with independently compilable here. The package-scoped header gate
attempts all 111 headers: 25 compile and 86 fail. The failures include both refused placeholders and
emitted modules whose dependency closure is incomplete. `pkg:tier` therefore classifies the package
as partial and not shippable, with zero unattempted headers.

The regeneration applies three pre-existing, current-pin source patches:

- `scene3d-formats-optional-null-storage` for the optional MTL material storage path;
- `scene3d-formats-resource-ref-owner-lookup` for exact resource-reference owner lookup; and
- `scene3d-formats-gltf-camera-diagnostic-details` for exact glTF camera diagnostic details.

It also applies the package's declared unqualified-name emission repairs. There is no complete
override supplying any of the 38 refused modules, and this slice adds no source patch, emission
repair, override, compiler request, or hand-written production implementation.

The upstream inventory contains 101 files, 1296 `it(...)` cases, and 3664 `expect(...)` calls. The
complete file-by-file inventory remains in `docs/parity/3d.md`; this document records which portable
contracts are callable and evidenced at the present native boundary.

## Focused P-3D2 native evidence

`tests/generated_scene3d_formats_test.cpp` is registered as the exact
`flight_cpp.generated_scene3d_formats` CTest with only the `scene3d-formats` package label. Because
the package contract is refused, the test directly includes only independently compiling package
headers. It exercises these material contracts:

| Evidence family | Native assertion |
| --- | --- |
| AWD2 header | A valid header is decoded little-endian from a nonzero typed-array byte offset without replacing source ownership. Bad magic, unsupported major version, and truncation return absence. |
| MD2 feature detection | Mesh, material, and animation features are emitted in canonical order from exact header counts. A valid empty feature set remains distinct from invalid magic, version, negative counts, and truncation. |
| OBJ material references | Comments and `usemtl` are ignored; whitespace and multi-path `mtllib` directives are parsed; duplicate paths are removed in first-seen order. |
| 3DS chunk census | An independently written nested fixture counts trimesh, camera, light/spot, material and three texture-map kinds, plus keyframe nodes. Known and unknown names are stable, input ownership is retained, invalid roots are rejected, and malformed child lengths stop safely without inventing counts. |
| glTF registries | Core and extension handlers append distinct kinds and replace an equal kind in place, preserving order and exact handler owners. Draco registration exposes the installed decoder and unregisters cleanly. |
| MTL helpers | New materials preserve canonical color/scalar defaults and absent optional texture/PBR members. Exact RGB triples parse; short and nonnumeric colors return absence and aggregate diagnostics by kind/discriminator. |
| MD5 animation helpers | Quoted joint names, parent indices, flags, component starts, baseframe position/orientation order, flag counts, and hierarchy totals are exact. Missing quotes, insufficient fields, and nonnumeric hierarchy/baseframe values are rejected into distinct diagnostic tallies. |

Seven independent mutation modes suppress one family oracle each:

- `--mutate-awd2-header`
- `--mutate-md2-features`
- `--mutate-obj-references`
- `--mutate-3ds-census`
- `--mutate-gltf-registries`
- `--mutate-mtl-helpers`
- `--mutate-md5-helpers`

Normal execution passes. Every mutation execution fails its named assertion, so a registered target
or a process that merely reaches `main` cannot masquerade as parity evidence.

## Verification boundary and residual

The package-scoped compiler report is the boundary: 111/111 headers attempted, 25 passing, 86
failing, and none unattempted. The focused test target builds and passes, its exact CTest label runs
one test, and the package-scope audit checks the direct generated includes against that label.

This is intentionally not a claim of whole-package parity. Full AWD2, Collada, glTF, MD2, MD5, OBJ,
STL, and 3DS document import remains behind refused entry points or dependency-incomplete emitted
headers. In particular, scene construction, mesh/material ownership across those importers,
coordinate and winding conversion in `shared`, full MTL parsing, full MD5 animation construction,
serialization/round-trips, and importer-level diagnostic flush/order are outside the callable
boundary. The package is import-oriented and exposes no callable serialization surface to test at
this pin. Those gaps stay visible as compiler/dependency debt rather than being filled with test
stubs, fakes, or hand-written replacement behavior.
