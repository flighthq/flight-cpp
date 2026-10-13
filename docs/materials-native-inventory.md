# Materials native inventory at Flight `a627849`

`@flighthq/materials` contains 24 source modules and 22 upstream test files (185 individual
Vitest cases) at the current pin. Materials-only generation emits 23 modules. The one refusal is
`materialPresets.ts` at line 21 (`structural-row construction requires explicit named properties`);
the provenance-checked `overrides/include/flight/materials/material_presets.hpp` supplies that API.
This inventory does not claim a case-for-case translation of all 185 JavaScript tests. It records
which upstream behavioral families have a native oracle and where the remaining JavaScript-only
detail lives.

The cohesive native oracle is `tests/generated_materials_test.cpp`. It includes every public
materials family used below, runs without renderer initialization, and asserts values rather than
header presence. `tests/structural_row_test.cpp` separately pins the sparse optional-options and
represented-union behavior required by the generated factories and material clone path.

| Upstream test file | Native evidence | Remaining boundary |
| --- | --- | --- |
| `material.test.ts` | Exact create/clone/copy owner identity, scalar/name copy, equality, alias safety, and typed `getMaterialOfKind` match/mismatch/null | Dynamic addition of an undeclared JavaScript property has no nominal C++ owner slot |
| `material3d.test.ts` | Exact kind/null-name, all shared option fields, defaults, and masked/opaque/blended predicates | None for the public native surface |
| `classicMaterials.test.ts` | Blinn-Phong, Lambert, and Phong kinds and exact numeric/color defaults; shared surface forwarding | Texture-handle override cases are represented by the shared optional option path rather than repeated per family |
| `pbrMaterials.test.ts` | Standard/spec-gloss defaults, partial override propagation, dielectric conversion, exact roughness, map-safe output, and guard routing lifecycle | The full-metallic conversion vector and every forwarded optional texture are not duplicated |
| `extendedPbrMaterial.test.ts` | Exact standard-block identity, extension-array identity and order, defaults, and partial-owner propagation | None |
| `materialPresets.test.ts` | Every packed preset base color; gold override ordering; glass shared/nested options, exact standard owner, concrete transmission owner, and extension order | Supplied by the declared override because the generated module is the package's single refusal |
| `customShaderMaterial.test.ts` | Shader-key linkage plus exact uniform-record identity and scalar/vector payloads | Texture record and empty-sentinel cases are not duplicated |
| `standardMaterial.test.ts` | Native 2D kind and null authored-name sentinel | Authored-name override is already exercised through base material clone/name propagation |
| `unlitMaterials.test.ts` | All eight constructors with exact packed colors, scalar defaults, and null-map defaults | Per-constructor override cases are represented by the same sparse options path exercised elsewhere |
| `colorScaleBias.test.ts` | Exact four-channel concatenation, packed RGBA channel order, identity reset, and identity predicate | Clone/copy, array export, inversion, RGB-only comparison, and alias permutations remain upstream-only |
| `materialValidation.test.ts` | In-place/returned-owner clamp identity; exact clamp domains; finite weight/IOR/UV success and NaN/Infinity failure | The clearcoat/thickness convenience predicates are covered indirectly by extension validators |
| `pbrExtension.test.ts` | Canonical UV channels 0/1 accepted and 2 rejected | None |
| `anisotropyPbrExtension.test.ts` | Exact entity owner, undefined runtime sentinel, UV override, defaults, valid and invalid strength | Infinite rotation and invalid UV are not repeated |
| `clearcoatPbrExtension.test.ts` | Exact clearcoat/roughness/normal-scale/UV defaults and valid owner | Each invalid field is not independently mutated |
| `iridescencePbrExtension.test.ts` | Exact strength, IOR, thickness range, UV defaults, and valid owner | Invalid thickness ordering is not independently mutated |
| `sheenPbrExtension.test.ts` | Exact packed color, roughness, UV defaults, and valid owner | Invalid UV is not independently mutated |
| `specularPbrExtension.test.ts` | Exact strength, packed color, UV defaults, and valid owner | Invalid UV is not independently mutated |
| `transmissionVolumePbrExtension.test.ts` | Exact infinite attenuation, color, IOR, thickness/transmission, UV defaults, plus invalid IOR failure | Invalid attenuation and UV are not independently mutated |
| `wrappedDiffusePbrExtension.test.ts` | Exact strength/color/thickness/UV defaults and valid owner | Invalid thickness and UV are not independently mutated |
| `phongToPbr.test.ts` | Exact shininess-to-roughness, metallic helper, and `log2(pi)` exposure mapping | Whole-object Phong conversion and caller override precedence remain upstream-only |
| `explainMaterialConversion.test.ts` | Explanation owner is routed intact through the conversion guard | Exact dropped-map lists/reason strings remain upstream-only |
| `enableMaterialConversionGuards.test.ts` | Install/report/uninstall lifecycle and silent post-uninstall path | Log-sink message text, `logOnce` separation, and process-wide deduplication belong to the log integration test surface |

## Mutation evidence

The native executable exposes eight deliberate corruption modes. Each must exit nonzero and name the
assertion it invalidates: `--mutate-identity`, `--mutate-order`, `--mutate-shader`, `--mutate-color`,
`--mutate-conversion`, `--mutate-clamp`, `--mutate-validation`, and `--mutate-lifecycle`. Together they
prove the identity, ordering, shader linkage, numeric transform, conversion, clamp, failure-domain,
and lifecycle assertions are capable of detecting a wrong implementation.

## Scope and dependency boundary

This lane regenerates only `@flighthq/materials`. It does not edit or regenerate the frozen
registry/render/render-GL/effects atomic group or the expanded scene2d-GL, scene2d-WGPU, and
guard-required scene3d-WGPU group. No frozen renderer dependency blocks the native materials oracle:
it is intentionally renderer-free. The package inventory still records mixed binding profiles for
`@flighthq/entity` and `@flighthq/log`; package compilation is the compatibility check at this boundary,
not permission to regenerate those dependencies here.
