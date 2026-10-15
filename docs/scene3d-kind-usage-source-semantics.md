# Scene3D KindUsage keeps the pinned cached property read

At Flight `a62784923f3be814463286c9fb28edfe6ab15789`, KindUsage reads a material's
`modifiers` property once and skips it only when the cached value is `undefined`.
The local `scene-nodeof-traversal-capabilities` patch had replaced that with a
property-presence predicate. A present `undefined` stack or an undefined-returning
getter then threw instead of being skipped. The compiler also emitted that
predicate's `in` token literally as invalid C++.

The correction removes the modifier rewrite and its helper from that existing
patch and retains the pinned `Modifier` import. It introduces no new assertion,
local declaration, nominal owner or runtime recovery. All other traversal-patch
modules, the NodeOf visitor refinement and the mesh geometry predicate remain as
before. The material and modifier families remain open.

`npm run scene3d-kind-usage:oracle -- --mutations` runs the real pinned APIs, then
the complete corrected Flight patch stack. Its eighteen cases cover eleven
upstream cases and seven focused observations: present undefined, arbitrary vendor
kinds, inherited stacks, single getter reads, undefined-returning getters, null
exceptions and reused output-array identity. The double-read, nullish-skip and
own-only mutations must all fail. These source checks make no native parity claim.

Faithful compiler refusal is accepted here. A presence-test lowering would keep
the semantic regression, and the earlier declaration experiment both added a
TypeScript weak-type error and emitted incompatible owner/optional carriers.
The independent capability-projection and optional-union compiler boundaries must
remain visible. No replacement body or supplying override is introduced.

Scoped generation applies 243 patches and emits 1235/1244 modules in the temporary
fourteen-package closure, with nine refusals. Only Scene3D is published into this
scratch inventory: **19/21 modules emitted, two refused**, previously 20/21.
The new refusal is `getScene3DKindUsage` at source line 29, classified
`cpp-type-assertion-unidentified` / `source-portability`: the emitter cannot project
the original readonly partial modifier capability from the seventeen-arm
Material3DValue union. The generated header marks the function NOT GENERATED and
retains its source and reason. SceneDocument remains separately refused and
unsupplied. No supplying override or replacement body hides either refusal.

The generated payload is one header (`scene_kind_usage.hpp`) and five shared
records: manifest, refusals, best-effort, initialization and README. Other package
provenance, refusals, initialization records and consumer edges are preserved;
initialization groups recompute from the merged graph. The README also reconciles
the pending material-unit provenance counts on this parent inventory. Dependency
headers, override hashes, structural-member vocabulary and build-header lists do
not change.

Run `node scripts/pkgGenerate.mjs @flighthq/scene3d --check` to verify generation,
then `node scripts/pkgCompile.mjs @flighthq/scene3d -j2` and
`node scripts/pkgTier.mjs @flighthq/scene3d` for independent native/header and tier
accounting. Source-oracle success does not imply native availability: this unit
restores semantics and makes the compiler limitation explicit. The package stays
partial and this correction does not claim runtime KindUsage parity.

The owned patch record uses the supported `and`-separated package label for Node,
Scene2D and Scene3D. Its previous comma-separated label was not recognized by
scoped generation, which preserved a stale digest. Normalizing this one record
and verifying the patch hash makes future scoped refreshes update its provenance.
No package-scope parser or unrelated patch record changes.
