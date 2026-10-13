# Scene3D resources native inventory at a627849

The pinned `@flighthq/scene3d-resources` package has 29 source modules. Before this slice its
committed row was still at Flight `7e2fc7d` with 28 modules (11 emitted and 17 refused). The scoped
regeneration moves only this package to the repository's current Flight pin and records 12 emitted
modules and 17 refused modules. It writes the package's 29 generated headers plus the eight shared
generated build and inventory ledgers; it does not regenerate a frozen 3D neighbor.

“Emitted” is not synonymous with independently compilable or callable here. Before migration, a
fresh package-scoped compile attempted all 28 stale headers and passed 3. At the current pin, the
same gate attempts all 29 headers: 14 compile and 15 fail, with none unattempted. Several passing
headers contain only a refusal placeholder for their public function. In particular, the eight
material texture-registration headers and `load_scene3_dresources.hpp` compile but expose no
generated registration or load/wait implementation. `pkg:tier` therefore classifies the package as
partial and not shippable.

The package has no source patches or overrides. Its current-pin generation uses the declared
`scene3d-resources-unqualified-types-names` emission repair. This slice adds no source patch,
emission repair, override, compiler request, or hand-written production implementation; refused
behavior stays refused.

The upstream inventory contains 28 test files, 122 `it(...)` cases, and 258 `expect(...)` calls.
The file-by-file inventory remains in `docs/parity/3d.md`; this document records which portable
contracts are actually callable and evidenced at the present native boundary.

## Focused P-3D2 native evidence

`tests/generated_scene3d_resources_test.cpp` is registered as the exact
`flight_cpp.generated_scene3d_resources` CTest with only the `scene3d-resources` package label. The
test directly includes five independently compiling package headers and exercises only their
generated callable behavior with real generated resource, resolver, texture, and signal owners.

| Evidence family | Native assertion |
| --- | --- |
| Image URI resolution | Relative paths join a base with exactly one separator; root paths, HTTP/data/custom-scheme URIs remain absolute; and a missing base preserves the relative URI. |
| Document base paths | Query and fragment suffixes are discarded, nested and absolute directories are retained, a bare filename has no base, and a root-level document preserves its empty root base. |
| Resource sidecar identity | Embedded and external reference textures are visited in authored order, absent lists are ignored, exact shared owners are deduplicated, prior output is cleared, inputs retain identity, and an empty scene clears the result. |
| Resource signal lifecycle | Signals start absent, enabling is idempotent and stores the exact group without allocating an incidental entity runtime, resolved and failed channels are distinct, dispatch reaches only the connected channel, clear removes its data without replacing the group, and independently created groups own distinct signals. |
| Absent failure-guard disable | Disabling a guard that is not installed is an identity-preserving no-op, remains disabled, and does not allocate resolver signals as a side effect. |

Five independent mutation modes suppress one evidence-family oracle each:

- `--mutate-uri-resolution`
- `--mutate-base-path`
- `--mutate-resource-identity`
- `--mutate-signal-lifecycle`
- `--mutate-guard-disable`

Normal execution passes. Every mutation execution fails its named assertion, so a registered target
or a process that merely reaches `main` cannot masquerade as parity evidence.

## Compile boundary and residual

The package-scoped compiler report is the mechanical boundary: 29/29 headers attempted, 14
passing, 15 failing, and none unattempted. The focused target builds and passes, and its exact CTest
label runs one test.

The passing-header count must not be read as material-registration parity. All eight anisotropy,
clearcoat, iridescence, shaded, sheen, specular, transmission/volume, and wrapped-diffuse material
texture registration functions remain refusal placeholders. Material registry construction,
standard/unlit/extended-PBR listers, lookup, and registration remain behind the failing
`scene_material_texture_registry.hpp` dependency closure.

The remaining boundary also includes web image-fetch construction and its asynchronous closures;
document byte/text network loading and resource-base-path mutation; AWD2, glTF/GLB, MD2, MD5, OBJ,
and 3DS URL loaders; resolver construction, initialization, disposal, streaming, resolution,
recovery, reveal, and coverage explanation; resource-reference lookup; failure-guard installation;
and `loadScene3DResources`/resolver waiting. These gaps remain visible as refused or
dependency-incomplete generated behavior rather than being filled with stubs, fakes, invented APIs,
or hand-written substitutes.
