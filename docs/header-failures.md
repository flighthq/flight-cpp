# Header failures

Every public header the SDK ships, compiled on its own, grouped by **cause**. A cluster is one
defect: every header in it is fixed by fixing that defect, whether they sit in one package or ten.
Measured with `FLIGHT_CPP_COMPILE_JOBS=8 npm run sdk:compile:sdl` at compiler
`806d257` / flight `a627849`.

| | |
| --- | --- |
| headers compiled | 3152 |
| compiling | **2339** |
| failing | **813** |
| failure clusters | 227 |
| carry clusters (passing only through a prior emission) | 9 |
| `-j8` crashes retried serially and compiled | 12 |

A `compiler exited with status 134` at `-j8` is the sweep running out of memory, not a rejected header:
every crash was recompiled one at a time before anything was classified, and the ones that compiled are
recorded as passing. A runtime header that landed after the sweep base (`flight/host_sdl/gl_app.hpp`,
H1) is appended from its author's standalone build and public-header test rather than swept again.

Ranked below by headers affected. The fix route is a first-pass judgement from the emitted code and the
host headers; a G2 worker confirms it before choosing a mechanism. Clusters marked as owned by builder4
are reserved for its G2 follow-up to E0a's TextLabel / text-layout / Scene2D GL text work.

## Clusters, ranked by headers affected

| # | cluster | headers | lanes | fix route | cause |
| --- | --- | --- | --- | --- | --- |
| 1 | `call-has-no-matching-9c08d665` | 117 | WGPU | compiler | bound WebGPU descriptor object literals are emitted as untyped source-order braced initializers with anonymous nested owners *(request filed)* |
| 2 | `missing-member-b1a6d5cf` | 39 | 2D | compiler | scene2d-canvas emits `using ...get_canvas_render_state_runtime;` before the function it names is defined *(request filed)* |
| 3 | `in-operator-not-lowered-b1eae4af` | 32 | 2D, 3D, GL, PLAT, WGPU | compiler | the TypeScript `in` operator is emitted verbatim; G2 particle3D joins this existing cause *(existing request extended)* |
| 4 | `undeclared-identifier-993e82f1` | 29 | 2D, 3D, PLAT, TEXT, WGPU | compiler | the template parameter name `Traits` leaks into emitted code as a free identifier *(request filed; builder4/G2)* |
| 5 | `call-has-no-matching-3882b3e9` | 28 | 2D, 3D, TEXT | compiler | bare object literals passed to SDK create_texture do not construct the TypeScript-selected owned options carrier *(request filed; builder4/G2)* |
| 6 | `other-52e846a6` | 18 | ANIM | compiler *(existing request)* | a nullable attachment array element is not lifted into the reference/null/undefined slot carrier |
| 7 | `call-has-no-matching-47410fe2` | 16 | 2D, 3D | runtime (fixed) | Number is now a callable conversion object; G2 recheck: 2 pass, 14 fail at later compiler diagnostics |
| 8 | `other-40da43f7` | 16 | 2D, 3D, ANIM | compiler | an optional<Record<...>> is initialised from a shape that is not its declared storage |
| 9 | `static-cast-assertion-9674a66c` | 15 | 2D | compiler | a TypeScript `as` on `JSON.parse` is emitted as a `static_cast` from `JsonValue` to an unrelated `Ref<T>` *(request filed)* |
| 10 | `call-has-no-matching-076d207f` | 14 | WGPU | runtime (fixed) | typed `create_pipeline_layout` descriptor overload; G2 recheck: 0 pass, 14 fail at `create_bind_group_layout` |
| 11 | `other-e9936ce6` | 13 | 3D | compiler | a pair literal the declared element type spells as `std::tuple<A, B>` is emitted as `flight::Array{...}`, so class-template ded... *(request filed)* |
| 12 | `call-has-no-matching-d6950545` | 12 | 2D, CORE | compiler *(existing requests)* | generic `get_node_child_count<Traits>` called without the inferred trait on a non-deducible `RowMerge` parameter (command: `NodeAny`; SVG/Lottie: `Node2D` via heritage) |
| 13 | `no-viable-conversion-9c3803c5` | 9 | 2D, WGPU | compiler *(existing request)* | generic `noop_renderer_data(…, NodeAny)` stored in `Scene2DRenderer.create_data`, whose `Node2D` does not widen to `NodeAny` |
| 14 | `no-viable-conversion-dfea2d92` | 9 | 3D | compiler | a value is passed where its declared type cannot receive it |
| 15 | `variant-cast-ccfedad4` | 9 | 3D | compiler | a retained readonly program row is reconstructed through a writable row before a readonly draw helper *(existing request extended)* |
| 16 | `call-has-no-matching-b569e92e` | 8 | 3D, ANIM | compiler | the emitted call to `init_joint_base` matches no declared overload |
| 17 | `undeclared-identifier-294e632e` | 8 | ANIM | declaration repair (applied) | omitted callee declaration supplied; G2: 1 pass, 7 fail at later diagnostics; function remains unlinkable |
| 18 | `undeclared-identifier-3e3cccce` | 8 | GL | runtime | the emitted code names `blit_offset_shaders`, which nothing declares *(G2: 1 pass, 7 to `undeclared-identifier-e7884ac7`)* |
| 19 | `invalid-operands-4b19651b` | 7 | 2D, ANIM | compiler | a guarded `unknown` (typeof record member, or `Array.isArray` length) is still read as `flight::Any` before a numeric comparison *(request filed)* |
| 20 | `missing-member-fd48c0bb` | 7 | 3D | [compiler request](compiler-requests/narrowed-imported-union-into-declared-parameter.md) | the emitted code reads a member the target does not declare (`collide_physics_3d_collider_shapes`) |
| 21 | `other-18dcce71` | 7 | ANIM | [compiler request](compiler-requests/erased-array-element-reading.md) | Array.isArray narrowing leaves an erased array length read as Any in a numeric assignment |
| 22 | `undeclared-identifier-7dc86326` | 7 | WGPU | declaration repair (applied) | refused scene accessor declared; G2: 2 pass, 5 later compiler failures; accessor remains unlinkable |
| 23 | `undeclared-identifier-b90ed4db` | 7 | ANIM | [compiler request](compiler-requests/erased-array-element-reading.md) | a retained callback alias references the DragonBones animations handler after its body is refused |
| 24 | `invalid-operands-ec4dbe74` | 6 | CORE, GL | runtime | an operator is applied to operand types it does not accept |
| 25 | `missing-member-237022b0` | 6 | 2D, TEXT | compiler | `create_document_refusal` lives in the wholly refused `sceneDocumentRefusal.ts`, refused for a misclassified imported Record alias *(request filed)* |
| 26 | `missing-member-e27d26f9` | 6 | 3D | runtime | the emitted code reads a member the target does not declare (`attach_gltf_pbr_extension`) |
| 27 | `no-viable-conversion-f02000a9` | 6 | 2D, GL | [compiler request](compiler-requests/interface-heritage-widening.md) | `RenderProxy2D` cannot widen to the `RenderProxy` parameter because the emitted owners lost their declared heritage |
| 28 | `undeclared-identifier-6e587559` | 6 | ANIM, TEXT | compiler | G2: 0/6; equality declaration retained, five-helper candidate fails unused compile at Record deletion/union projection |
| 29 | `undeclared-identifier-a74215ca` | 6 | 2D | compiler | a generic call's object-literal argument is built as `ShapeBoundsCommand<K>`, naming the callee's type parameter *(request filed)* |
| 30 | `undeclared-identifier-f5e3440f` | 6 | 3D, CORE | [compiler request](compiler-requests/retained-declaration-calls-omitted-callee.md) | createResourceLoader calls initializeResourceLoader after the compiler refuses the initializer |
| 31 | `call-has-no-matching-5a045ef4` | 5 | ANIM | compiler *(existing request)* | `SwfShapeState` passed to the anonymous writable `pen: { x; y }` parameter without an identity-preserving view |
| 32 | `call-has-no-matching-b897f03f` | 5 | 3D | [compiler request](compiler-requests/heritage-argument-generic-deduction.md) | the `getNodeSignals` traits argument falls back to the default `Ref<NodeTraits>` instead of the inferred `Node3DTraits` |
| 33 | `call-has-no-matching-dd66c064` | 10 | ANIM | compiler | an emitted SwfReader class instance cannot be passed as its structural SwfTagReader interface to the parse callback *(existing request)* |
| 34 | `compiler-crash-aba5e9ec` | 5 | 3D | none (sweep resource limit; passes serially) | the -j8 sweep ran out of room (retried serially) |
| 35 | `invalid-operands-6d9a155d` | 5 | 2D | [compiler request](compiler-requests/token-discriminant-comparison.md#g2-current-pin-qualification-all-three-paint-guards-use-this-carrier) | three morph-paint guards compare the whole optional token variant with a String |
| 36 | `missing-member-5e9e8a2c` | 5 | PLAT | compiler | retained cross-module calls name the refused generic graph-bounds helper *(existing requests extended)* |
| 37 | `no-viable-conversion-eaf1a6e0` | 5 | 2D | compiler *(existing request)* | generic `noop_renderer_data(…, NodeAny)` stored in `SpriteRenderer.create_data`, whose `Sprite` does not widen to `NodeAny` |
| 38 | `no-viable-overload-658901de` | 5 | 2D, GL | override/runtime (fixed) | concrete glass extension storage and Canvas paint variants repaired; G2: 4 pass, 1 compiler failure |
| 39 | `call-has-no-matching-4ce2cc01` | 4 | ANIM | compiler *(existing request)* | asserted record lowered to a `NamedProperties` view but the Spine timeline handler parameter to an owning `Record<String, Any>` |
| 40 | `call-has-no-matching-82244b93` | 4 | 2D | compiler | the emitted call to `get_or_create_render_proxy_2d` matches no declared overload |
| 41 | `call-has-no-matching-9248dccf` | 4 | ANIM | compiler | the emitted call to `get_node_world_matrix4` matches no declared overload |
| 42 | `call-has-no-matching-9ebeb95c` | 4 | ANIM, GL | compiler | the emitted call to `get_transform_2d_runtime` matches no declared overload |
| 43 | `call-has-no-matching-c3aea71c` | 4 | 2D, GL | compiler | the emitted call to `structural_ref_cast` matches no declared overload |
| 44 | `no-viable-conversion-5903ce63` | 4 | 3D, GL | compiler | a value is passed where its declared type cannot receive it |
| 45 | `no-viable-conversion-e1a24bd1` | 4 | 2D | compiler | a value is passed where its declared type cannot receive it |
| 46 | `other-2aef05a3` | 6 | ANIM | [compiler request](compiler-requests/interface-heritage-widening.md) | shadow-effect conditional branches cannot widen their independent concrete owners into the declared Effect array element |
| 47 | `other-43523429` | 4 | ANIM | [compiler request](compiler-requests/heritage-argument-generic-deduction.md) | heritage-based Node2D type arguments are not substituted at addNodeChild, leaving a free Traits identifier |
| 48 | `other-4e22e4de` | 4 | 2D | compiler | JSON.parse assertion casts JsonValue to the Aseprite document-owner variant *(existing request extended)* |
| 49 | `other-5611a858` | 4 | ANIM | declaration repair (applied) | ColorScaleBiasAdjustment heritage restored; G2: 1 pass, 3 compiler failures |
| 50 | `other-d6be4352` | 4 | ANIM | [compiler request](compiler-requests/json-parse-assertion-static-cast.md#g2-pixi-particle-parser-the-destination-is-a-dynamic-record) | parsed JSON is asserted to PixiRaw with an unrelated C++ record cast; nested shape/absence lowering also remains open |
| 51 | `static-assertion-00a0e75d` | 4 | 3D, WGPU | compiler | a runtime static assertion rejects the emitted shape |
| 52 | `undeclared-identifier-0fc9e20f` | 4 | 2D | declaration repair (applied) | omitted artboard-import-context declaration supplied; G2: 0 pass, 4 fail at the next omitted callee; function remains unlinkable |
| 53 | `undeclared-identifier-69b85faa` | 4 | PLAT | declaration repair (applied) | omitted untrack callee declaration supplied; G2: 0 pass, 4 fail at the next omitted callee; function remains unlinkable |
| 58 | `undeclared-identifier-7d64d9c1` | 4 | GL | compiler | the emitter refuses the `glCustomShaderEffectRunner` binding and its resolver but still emits the registration call that passes... *(request filed)* |
| 55 | `undeclared-identifier-862d162a` | 4 | 3D | [compiler request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-gltf-animations-a-declaration-fails-the-unused-header-link-prerequisite) | the refused animation builder is still called by the global handler; a declaration fails the unused-header link prerequisite |
| 56 | `undeclared-identifier-f2b46b01` | 4 | 3D | declaration repairs (applied) | refused standard/unlit callback declarations supplied; G2: 1 pass, 3 deeper compiler failures; callbacks remain unlinkable |
| 57 | `call-has-no-matching-3b2f72dd` | 3 | PLAT | compiler | the emitted call to `is_optional_number` matches no declared overload |
| 58 | `call-has-no-matching-56e47749` | 3 | GL | compiler | a `Node2D` passed to a `Spatial2DNode<Traits>` parameter cannot deduce `Traits` *(existing request linked)* |
| 59 | `invalid-operands-98cd7094` | 3 | 2D | [compiler request](compiler-requests/local-handler-callable-identity.md) | a nullable registry callback is compared with the module function by identity, but the emitted `std::function` adapter erased it |
| 60 | `missing-member-95ead583` | 3 | 3D | [compiler request](compiler-requests/cube-texture-width-and-completeness.md#g2-gl-environment-face-revisions-use-the-same-selected-owner-read) | nullable cube-face source retains seven real owners, but version is read on the variant itself |
| 61 | `missing-member-b22d5c52` | 3 | 2D | declaration repair (applied) | refused callee `resolve_canvas_texture_window_source` declared across modules; G2: 0 pass; follow-on `missing-member-f497840c` fixed by correcting a source patch; all 3 now in `missing-member-b1a6d5cf`; function remains unlinkable |
| 62 | `missing-member-c36ea2d4` | 3 | 2D | binding | the web renderer needs shared CSS style state; SDL exposes copied shell values with no opacity field *(request filed)* |
| 63 | `no-viable-conversion-4ca7626f` | 3 | ANIM | compiler | a value is passed where its declared type cannot receive it |
| 64 | `no-viable-conversion-b3064463` | 3 | GL | compiler | `GlRenderState` passed where `RenderState` is declared; the emitter flattens the heritage *(existing request linked)* |
| 65 | `no-viable-conversion-b3f3c7c6` | 3 | 3D | compiler | a value is passed where its declared type cannot receive it |
| 66 | `no-viable-conversion-b5b698fe` | 3 | 2D | compiler | a value is passed where its declared type cannot receive it |
| 67 | `other-3d2efc1d` | 3 | 2D | [compiler request](compiler-requests/erased-array-element-reading.md) | the emitter indexes a still-erased `flight::Any` after `Array.isArray` instead of using the runtime's exact-array recovery |
| 72 | `other-41f4a462` | 3 | ANIM | compiler | a named-property view (which aliases the source object) and an empty Record are the two arms of one conditional, so the conditi... *(request filed)* |
| 69 | `other-6a1604d5` | 3 | PLAT | override completeness (fixed) | the tray placeholder override shadowed `set_tray_animation_guard`, which the generated guard header calls; G2 recheck: 3/3 pass |
| 70 | `other-b860ec87` | 3 | 2D, GL | compiler | the emitted code returns a readonly structural row as its declared nominal owner and drops the recovery; fix attempted and withdrawn |
| 71 | `other-f9c5bc10` | 3 | 2D | compiler | an `as` assertion over a `flight::JsonValue` is lowered with the erased-value `object_if` pattern, which a tagged JSON value ca... *(request filed)* |
| 72 | `redefinition-c11d96e9` | 3 | TEXT | runtime (fixed) | the bitmap_font_record override's `#pragma once` was commented out; G2 recheck: 0 pass, 3 fail at `bitmap_font_fnt.hpp:187` |
| 73 | `undeclared-identifier-145c75d7` | 3 | TEXT | declaration repair (applied) | refused glyph-atlas initializer declared; G2: 3 pass; factory remains unlinkable |
| 74 | `undeclared-identifier-2cdb69a6` | 3 | PLAT | runtime + override (fixed) | the emitted code names `channel_elements`, which nothing declares; G2: 3 pass |
| 75 | `undeclared-identifier-564c29cc` | 3 | TEXT | runtime + repair (fixed) | the emitted code names `format_date_value`, which nothing declares; G2: 1 pass, 2 to `undeclared-identifier-1a056bbc` |
| 76 | `undeclared-identifier-6a187568` | 3 | 3D | declaration repair (applied) | refused ASCII STL counter declared; G2: 3 pass; counter remains unlinkable |
| 77 | `undeclared-identifier-dc087108` | 3 | PLAT | declaration repair (applied) | refused URL loader declared; G2: 3 pass; URL loading remains unlinkable |
| 78 | `call-has-no-matching-84410867` | 2 | 3D | compiler | the emitted call to `buffer_data` matches no declared overload |
| 79 | `call-has-no-matching-d42144f0` | 2 | ANIM | compiler | the emitted call to `push` matches no declared overload |
| 80 | `missing-member-da12a523` | 2 | 3D | runtime + repair (fixed) | the emitted code reads a member the target does not declare (`parse_obj_material_library`); G2: 0 pass, 2 follow-ons (`static-assertion-ecc68c2b`, `undeclared-identifier-d663f62e`) |
| 81 | `missing-member-dd9b4baa` | 2 | 3D, GL | runtime | the emitted code reads a member the target does not declare (`renderbuffer_binding`) *(G2: binding added; follow-on `no-matching-conversion-bc278253` fixed: 1 pass, 1 to `missing-member-95ead583`)* |
| 82 | `no-viable-conversion-303c42f1` | 2 | ANIM | compiler | a value is passed where its declared type cannot receive it |
| 83 | `other-67dc02c6` | 2 | 2D | compiler | `getSurfaceHandle(...) as HTMLCanvasElement` lowered to an unchecked `static_cast` from the erased handle *(existing request extended)* |
| 84 | `other-6f513db2` | 2 | 2D, 3D | binding (fixed) | attribute locations now use the source number carrier; G2: 0 pass, 2 later compiler failures |
| 85 | `other-86d0b50b` | 5 | 2D, 3D | [compiler request](compiler-requests/object-literal-into-outcome-variant.md) | SceneCoverageEntry literals initialize the outer variant instead of a selected concrete reference arm |
| 86 | `redefinition-caa49360` | 2 | 3D | override de-duplication (fixed) | two picking overrides each defined the same three node matrix helpers; G2: 0/2 pass, both move to the material conversion below |
| 87 | `undeclared-identifier-1048386b` | 2 | 3D | compiler | refused MD5 material handler retained by global family; exact declaration fails unused-header positive link *(existing requests, G2: 0/2 retained)* |
| 88 | `undeclared-identifier-20fa691d` | 2 | 2D | compiler | refused raster draw referenced by retained renderer; exact declaration fails native/generic callback carrier conversion *(existing requests)* |
| 89 | `undeclared-identifier-225478b5` | 2 | 2D | compiler | refused bitmap-text submit callback lacks a declaration; exact declaration still exposes incompatible renderer callback carriers *(existing requests)* |
| 90 | `undeclared-identifier-3e98002c` | 2 | 3D | declaration repair (applied) | omitted AWD2 parse-state initializer declared; G2: 1 pass, 1 moves to the create_texture carrier family; initializer remains unlinkable |
| 91 | `undeclared-identifier-644a9b33` | 2 | 2D | runtime (partial override) | the emitted code names `clip_programs`, which nothing declares *(G2: 1 pass, 1 to `undeclared-identifier-befaa2d6`)* |
| 92 | `undeclared-identifier-7ce7bd18` | 2 | 3D | compiler | refused MD2 skin handler retained by global family; exact declaration fails unused-header positive link *(existing requests, G2: 0/2 retained)* |
| 93 | `undeclared-identifier-86212434` | 2 | GL | compiler *(existing request)* | the emitter refuses the `glSsaoEffectRunner` binding (and its callee `applySsaoEffectToGl`) but still emits the registration call that passes the runner |
| 94 | `other-8b0f8597` | 2 | 3D | [compiler request](compiler-requests/host-initialize-optional-fallback-on-a-required-read.md) | skin builder emits after a source patch; SequenceView index nullish fallback still fails (G2: 0/2 pass) |
| 95 | `undeclared-identifier-e39f9d67` | 2 | 3D | compiler | missing resolver cleared; original2 now 1 pass / 1 readonly program-owner failure; Map guard remains declaration-only *(requests extended)* |
| 96 | `variant-cast-37507e07` | 2 | 3D | compiler | a functional-style cast is applied to a variant the target type cannot take |
| 97 | `call-has-no-matching-00dc04ad` | 1 | 2D | compiler | the emitted call to `get_texture_uv_matrix` matches no declared overload |
| 98 | `call-has-no-matching-35224a3f` | 1 | 3D | compiler | the emitted call to `gl_uniform_type_name` matches no declared overload |
| 99 | `call-has-no-matching-4c12e411` | 15 | WGPU | compiler | `create_bind_group_layout` entries are emitted as anonymous nested owners inside an untyped braced descriptor *(request filed)* |
| 100 | `call-has-no-matching-76a6a61a` | 1 | 2D | compiler | the emitted call to `get_if` matches no declared overload |
| 101 | `call-has-no-matching-7f4ae181` | 1 | 2D | compiler | the emitted call to `add_scene_2d_used_kind` matches no declared overload |
| 102 | `call-has-no-matching-8a1f3a33` | 1 | GL | compiler | the emitted call to `row_get` matches no declared overload |
| 103 | `call-has-no-matching-8d9de718` | 1 | PLAT | compiler | the emitted call to `collect_spatial_candidates` matches no declared overload |
| 104 | `call-has-no-matching-b3c6bcee` | 1 | GL | compiler | the emitted call to `log_once` matches no declared overload |
| 105 | `call-has-no-matching-c287f892` | 1 | ANIM | compiler | the emitted call to `named_properties` matches no declared overload |
| 106 | `call-has-no-matching-c8046ec6` | 1 | 3D | compiler | the emitted call to `uniform2fv` matches no declared overload |
| 107 | `call-has-no-matching-e7b28969` | 1 | WGPU | compiler | the emitted call to `write_texture` matches no declared overload |
| 108 | `call-has-no-matching-fbd6de0d` | 1 | 3D | compiler | the emitted call to `invoke` matches no declared overload |
| 109 | `call-has-no-matching-fee2f93f` | 1 | 3D | compiler | the emitted call to a function matches no declared overload |
| 110 | `invalid-operands-dc1c09c4` | 1 | PLAT | source guards + declaration repair (applied) | flex basis comparison and unsafe numeric projection cleared; G2: 1 pass; refused resolver registration remains unlinkable |
| 111 | `missing-member-1620083c` | 1 | 2D | binding | SDL DOM value/style copies cannot carry the web renderer mix-blend write; open blend domain/provider required; G2: 0/1 retained |
| 112 | `undeclared-identifier-af6f50aa` | 1 | 2D | compiler | the DOM image registration retains its refused optional image callback after the persistent table registration now emits |
| 113 | `missing-member-1ce5ed2e` | 1 | 3D | declaration repair (applied) | six refused built-in joint-solver bindings declared; G2: 1/1 pass, registration remains unlinkable |
| 114 | `missing-member-42783657` | 1 | 2D | binding | SDL DOM value/style copies cannot carry the native text font/textAlign/lineHeight writes; shared style provider required; G2: 0/1 retained |
| 115 | `missing-member-5ac43ed0` | 1 | 3D | source fix; compiler bounds debt | optional color write emits checked TypedArray assignment; G2: 0/1 now writable Aabb output-owner failure |
| 116 | `missing-member-703ef98e` | 1 | 2D | compiler | G2: 0/1; refused additive handler retained in registration/global family; declaration fails unused positive link |
| 117 | `missing-member-8aa8170f` | 1 | ANIM | compiler *(existing request)* | the refused `swfVideoHandler` binding is still named by `swfVideoTagFamily`; its placement callable needs a signature adapter |
| 118 | `missing-member-ad3558ee` | 1 | ANIM | runtime + repair (fixed) | the emitted code reads a member the target does not declare (`physics_2d_distance_joint_solver`); G2: 1 pass |
| 119 | `missing-member-b5408b35` | 1 | 3D | override (fixed) | the `gl_shadow_map` override regained the generated `node_transform3d.hpp` include; G2 recheck: 1 pass |
| 120 | `missing-member-d7bf452c` | 1 | 2D | [compiler/runtime contract](compiler-requests/readonly-record-entry-enumeration.md) | readonly Record entry enumeration and value-to-view construction disagree; G2: 0/1 retained |
| 121 | `missing-member-dae8c839` | 4 | TEXT | [compiler request](compiler-requests/reference-backed-outcome-variant-narrowing.md) | FNT formatter reads dimensions on an image-source variant instead of its active owner |
| 122 | `missing-member-dcf85c36` | 1 | 3D | repair (fixed) | refused `collectMd5AnimFeatures` declared (declaration only) for its retained caller; G2 recheck: 1 pass |
| 123 | `missing-member-ee398c94` | 1 | 2D | compiler *(existing request)* | G2: the line-50 diagnostic was a cascade; the first GCC failure is cluster 2's re-export before declaration; row moved |
| 124 | `missing-member-fd2be1bc` | 1 | TEXT | compiler *(existing request)* | an `as { chars?, common? }` assertion over a parsed `JsonValue` is lowered with the erased-`Any` `object_if` recovery |
| 125 | `no-viable-conversion-03bf8175` | 1 | 2D | compiler | a value is passed where its declared type cannot receive it |
| 126 | `no-viable-conversion-0faa55ca` | 1 | GL | compiler | a value is passed where its declared type cannot receive it |
| 127 | `no-viable-conversion-2909a65f` | 1 | 2D | compiler | a value is passed where its declared type cannot receive it |
| 128 | `no-viable-conversion-31b754c5` | 1 | ANIM | compiler | a value is passed where its declared type cannot receive it |
| 129 | `no-viable-conversion-331a999f` | 1 | GL | compiler | a value is passed where its declared type cannot receive it |
| 130 | `no-viable-conversion-4bbb9be5` | 1 | 2D | compiler | a value is passed where its declared type cannot receive it *(builder4/E0a)* |
| 131 | `no-viable-conversion-562151da` | 1 | WGPU | compiler | a value is passed where its declared type cannot receive it |
| 132 | `no-viable-conversion-68fff449` | 1 | 3D | compiler | a value is passed where its declared type cannot receive it |
| 133 | `no-viable-conversion-7e6bb150` | 1 | WGPU | compiler | a value is passed where its declared type cannot receive it |
| 134 | `no-viable-conversion-802e8004` | 1 | GL | compiler | a value is passed where its declared type cannot receive it |
| 135 | `no-viable-conversion-9695423c` | 1 | 2D | compiler | a value is passed where its declared type cannot receive it |
| 136 | `no-viable-conversion-a516fbdb` | 1 | 3D | compiler | a value is passed where its declared type cannot receive it |
| 137 | `no-viable-conversion-b47bc6cd` | 1 | 2D | compiler | a value is passed where its declared type cannot receive it |
| 138 | `no-viable-conversion-bf84cbac` | 1 | 3D | compiler | a value is passed where its declared type cannot receive it |
| 139 | `no-viable-conversion-c997578d` | 1 | 2D | compiler | a value is passed where its declared type cannot receive it |
| 140 | `no-viable-conversion-cef6403d` | 3 | 3D | compiler | a value is passed where its declared type cannot receive it |
| 141 | `no-viable-conversion-dc893fc7` | 1 | GL | compiler | a value is passed where its declared type cannot receive it |
| 142 | `no-viable-conversion-eabaf455` | 1 | 3D | compiler | a value is passed where its declared type cannot receive it |
| 143 | `no-viable-conversion-f7267dc5` | 1 | ANIM | compiler | a value is passed where its declared type cannot receive it |
| 144 | `other-1e3a0d33` | 1 | ANIM | source fix; compiler follow-ons | G2: 0/1; typed preset owners fix conditional, now optional Record detail; nominal Bone2D store still owed |
| 145 | `other-4703cbbe` | 1 | 3D | compiler | G2: 0/1; parsed JSON is cast to a nominal glTF document; existing assertion request extended |
| 146 | `other-5dfe27ec` | 1 | ANIM | compiler | G2: 0/1; erased display-array iteration; collector/derived attachment/full skin algorithms remain refused |
| 147 | `other-acb4b229` | 1 | 2D | compiler | G2: 0/1; JSON assertion casts to nominal array/hash document union; converter independently refused |
| 148 | `static-assertion-18059635` | 1 | 3D | compiler | a runtime static assertion rejects the emitted shape |
| 149 | `static-assertion-261f0db6` | 1 | 3D | compiler | a runtime static assertion rejects the emitted shape |
| 150 | `undeclared-identifier-0168969a` | 1 | 2D | compiler *(existing + new request filed)* | refused `domScene2DClipHooks` (RenderState asserted to DomRenderState without heritage) still stored; next callee `setDomClipHooks` is in the wholly refused domClipRectangle module |
| 151 | `undeclared-identifier-07640d32` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glOutlineEffectRunner` binding (and its callee `applyOutlineEffectToGl`) but still emits the registration call that passes the runner |
| 152 | `undeclared-identifier-076c02b1` | 1 | PLAT | declaration repair (applied) | omitted tilemap cell resolver declared; G2: 1/1 pass, registration remains unlinkable |
| 153 | `undeclared-identifier-09cc9e86` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glFilmGrainEffectRunner` binding (and its callee `applyFilmGrainEffectToGl`) but still emits the registration call that passes the runner |
| 154 | `undeclared-identifier-0b8d3b44` | 1 | GL | compiler | G2: 0/1; bound-context WeakMap refused; shader spread/owner and omitted blur-pass union debt remain |
| 155 | `undeclared-identifier-0c07642f` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glGlitchEffectRunner` binding (and its callee `applyGlitchEffectToGl`) but still emits the registration call that passes the runner |
| 156 | `undeclared-identifier-11d6fcbe` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glChromaticAberrationEffectRunner` binding (and its callee `applyChromaticAberrationEffectToGl`) but still emits the registration call that passes the runner |
| 157 | `undeclared-identifier-1462818e` | 1 | 2D | compiler | refused velocity program owner; exact declaration fails unused-header compilation at VIEWPORT method/constant collision *(requests filed)* |
| 158 | `undeclared-identifier-17cb8386` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glBokehDepthOfFieldEffectRunner` binding (and its callee `applyBokehDepthOfFieldEffectToGl`) but still emits the registration call that passes the runner |
| 159 | `undeclared-identifier-1892fccb` | 1 | PLAT | declaration repair (applied) | exact refused text callback declared; G2: 1 pass; registration and character resolution remain unlinkable |
| 160 | `undeclared-identifier-1a056bbc` | 3 | TEXT | compiler | refused number formatter retained by five wrappers; G2: declaration candidate fails optional literal construction, full option spreads dropped; 0/3 remain failed |
| 161 | `undeclared-identifier-1d8e072a` | 1 | 2D | compiler | Scale9 draw refuses nullable material owner widening; declaration probe fails native/generic callback conversion *(existing requests extended)* |
| 162 | `undeclared-identifier-2110ec37` | 1 | WGPU | compiler | WGPU Sprite draw refuses nullable material owner widening; exact declaration compile fails GL-specific Sprite callback carrier *(existing requests extended)* |
| 163 | `undeclared-identifier-2363216f` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glDitherEffectRunner` binding (and its callee `applyDitherEffectToGl`) but still emits the registration call that passes the runner |
| 164 | `undeclared-identifier-2437e103` | 1 | 3D | compiler | the emitted code names `parse_triangle_geometry_block`, which nothing declares; declaration candidate rejected at the unused-header positive prerequisite *(requests filed)* |
| 165 | `undeclared-identifier-251d1ba2` | 1 | 3D | compiler | G2: 0/1; exact collector prototype fails unused compile at optional ease row and tween variant order; rejected |
| 166 | `undeclared-identifier-2d9cd75f` | 1 | PLAT | declaration repair (applied) | exact refused shape-fill callback declared; G2: 1 pass; three-kind registration and fill hits remain unlinkable |
| 167 | `undeclared-identifier-3017e8aa` | 0 | GL | declaration repair (applied) | owned header 1/1; acquire body absent; void bracket instantiation still fails |
| 168 | `undeclared-identifier-332a5586` | 1 | 2D | compiler *(existing request)* | refused `drawDomScale9Sprite` retained as `.submit` in the renderer aggregate; declaration probe fails at `.create_data` |
| 169 | `undeclared-identifier-34adf2ab` | 1 | ANIM | compiler | G2: 0/1; refused parser/result retained; exact declaration candidate fails unused compile in result-owner dependency |
| 170 | `undeclared-identifier-3582eeb5` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glScreenSpaceFogEffectRunner` binding (and its callee `applyScreenSpaceFogEffectToGl`) but still emits the registration call that passes the runner |
| 171 | `undeclared-identifier-3cb2bb54` | 1 | GL | runtime | the emitted code names `invert_tint_shaders`, which nothing declares |
| 172 | `undeclared-identifier-3e7f03c4` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glSharpenEffectRunner` binding (and its callee `applySharpenEffectToGl`) but still emits the registration call that passes the runner |
| 173 | `undeclared-identifier-44058981` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glBlendEffectRunner` binding (and its callee `applyBlendEffectToGl`) but still emits the registration call that passes the runner |
| 174 | `undeclared-identifier-49b86813` | 1 | ANIM | declaration repair (applied) | exact refused IK solver declared; G2: 1 pass; registration and full constraint solving remain unlinkable |
| 175 | `undeclared-identifier-4fc6ab69` | 0 | 2D | declaration repair (applied) | G2: 1/1 header; unused positive passes, handler links fail exactly on refused image helper |
| 176 | `undeclared-identifier-514a5b7b` | 1 | 2D | compiler | Tilemap submit refuses nullable material owner widening; exact declaration compile fails NodeAny/Sprite callback carrier *(existing requests extended)* |
| 177 | `undeclared-identifier-51763a3c` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glToneMapEffectRunner` binding (and its callee `applyToneMapEffectToGl`) but still emits the registration call that passes the runner |
| 178 | `undeclared-identifier-529e49d7` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glDirectionalBlurEffectRunner` binding (and its callee `applyDirectionalBlurEffectToGl`) but still emits the registration call that passes the runner |
| 179 | `undeclared-identifier-58fb713b` | 1 | 2D | compiler *(existing request)* | refused `drawDomScale9Shape` retained as `.submit` in the renderer aggregate; declaration probe fails at `.create_data` |
| 180 | `undeclared-identifier-5be7be47` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glScanlinesEffectRunner` binding (and its callee `applyScanlinesEffectToGl`) but still emits the registration call that passes the runner |
| 181 | `undeclared-identifier-6647740e` | 1 | 2D | compiler | G2: 0/1; exact transform prototype fails unused compile at target presence and Record detail; candidate rejected |
| 182 | `undeclared-identifier-66ea97f8` | 1 | WGPU | compiler | screen initializer refuses format union; valid declaration witness still fails native GPUTexture width access *(existing and new requests)* |
| 183 | `undeclared-identifier-67f2c521` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glGodRaysEffectRunner` binding (and its callee `applyGodRaysEffectToGl`) but still emits the registration call that passes the runner |
| 184 | `undeclared-identifier-6ad30a3d` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glCrtEffectRunner` binding (and its callee `applyCrtEffectToGl`) but still emits the registration call that passes the runner |
| 185 | `undeclared-identifier-6dd20ce4` | 1 | 2D | compiler | QuadBatch submit refuses nullable material owner widening; exact declaration compile fails NodeAny/Sprite callback carrier *(existing requests extended)* |
| 186 | `undeclared-identifier-757c2c8a` | 1 | 2D | compiler | private Scale9 shape owner recovery refused; exact declaration compile fails native texture-cache local carrier *(requests filed)* |
| 187 | `undeclared-identifier-785ed669` | 1 | PLAT | declaration repair (applied) | omitted quad-batch instance resolver declared; G2: 1/1 pass, registration remains unlinkable |
| 188 | `undeclared-identifier-7905640c` | 1 | 2D | compiler *(existing request)* | refused `canvasClipHooks` (RenderState asserted to CanvasRenderState without heritage) still stored; all clip callees emitted |
| 189 | `undeclared-identifier-7d85a310` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glLensFlareEffectRunner` binding (and its callee `applyLensFlareEffectToGl`) but still emits the registration call that passes the runner |
| 190 | `undeclared-identifier-7e3a780f` | 1 | PLAT | declaration repair (applied) | exact Sprite alpha helper declared; G2: 1 pass; registration and pixel hit testing remain unlinkable |
| 191 | `undeclared-identifier-7fd1550e` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glCameraMotionBlurEffectRunner` binding (and its callee `applyCameraMotionBlurEffectToGl`) but still emits the registration call that passes the runner |
| 192 | `undeclared-identifier-7ff1c319` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glVignetteEffectRunner` binding (and its callee `applyVignetteEffectToGl`) but still emits the registration call that passes the runner |
| 193 | `undeclared-identifier-8653adf0` | 0 | 2D | declaration repair (applied) | G2: 1/1 header; unused positive passes, both retained runtime/signal callers link only to the absent helper |
| 194 | `undeclared-identifier-898d7996` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glDisplacementEffectRunner` binding (and its callee `applyDisplacementEffectToGl`) but still emits the registration call that passes the runner |
| 195 | `undeclared-identifier-8ae633dc` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glSmaaEffectRunner` binding (and its callee `applySmaaEffectToGl`) but still emits the registration call that passes the runner |
| 196 | `undeclared-identifier-8b3950d6` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glMotionBlurEffectRunner` binding (and its callee `applyMotionBlurEffectToGl`) but still emits the registration call that passes the runner |
| 197 | `undeclared-identifier-8e604c65` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glSketchEffectRunner` binding (and its callee `applySketchEffectToGl`) but still emits the registration call that passes the runner |
| 198 | `undeclared-identifier-9562ef07` | 1 | 2D | compiler *(existing request)* | refused `drawCanvasTilemap` retained as `.submit` in the SpriteRenderer aggregate; declaration probe fails at `.create_data` |
| 199 | `undeclared-identifier-9aaa5b7f` | 1 | WGPU | compiler | capture hooks refuse optional guard evidence; exact three-declaration probe fails native mapped-range access *(existing and new requests)* |
| 200 | `undeclared-identifier-a3f09fb8` | 1 | 2D | compiler *(existing request)* | refused `drawDomShape` retained as `.submit` in the renderer aggregate; declaration probe fails at `.create_data` |
| 201 | `undeclared-identifier-a4a22a69` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glLensDistortionEffectRunner` binding (and its callee `applyLensDistortionEffectToGl`) but still emits the registration call that passes the runner |
| 202 | `undeclared-identifier-a89b9a39` | 1 | GL | compiler *(existing request)* | refused `glBitmapDisplacementEffectRunner` and resolver `isGlBitmapDisplacementEffectResolvable` both still registered (custom-shader shape) |
| 203 | `undeclared-identifier-aa084556` | 1 | PLAT | runtime + declaration repairs (applied) | refused track-list guard and grid resolver declared; strict erased integer predicate supplied; G2: 1 pass; validator/registration remain unlinkable |
| 204 | `undeclared-identifier-ae594b8a` | 0 | 2D | declaration repair (applied) | G2: 1/1 header; unused positive passes, handler links fail exactly on refused Lottie group walker |
| 205 | `undeclared-identifier-af7a1054` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glMedianEffectRunner` binding (and its callee `applyMedianEffectToGl`) but still emits the registration call that passes the runner |
| 206 | `undeclared-identifier-b15d9d15` | 1 | ANIM | compiler | G2: 0/1; refused bone parser and complete result omitted; no exact declaration candidate |
| 207 | `undeclared-identifier-befaa2d6` | 2 | 2D | source patch + repairs (applied) | the emitted code names `compute_scissor_rect`, which nothing declares *(G2: 1 pass, 1 to `no-viable-conversion-e25fffde`)* |
| 208 | `undeclared-identifier-c162d0c0` | 1 | ANIM | declaration repair (applied) | exact refused transform solver declared; G2: 1 pass; registration and full constraint solving remain unlinkable |
| 209 | `undeclared-identifier-c29489a5` | 1 | GL | compiler *(existing request)* | refused `glBloomEffectRunner` still registered; callee widens pooled `Ref<GlTextureRenderTarget>` intermediates to `GlRenderTarget` |
| 210 | `undeclared-identifier-c3c55c65` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glCompositeEffectRunner` binding (and its callee `applyCompositeEffectToGl`) but still emits the registration call that passes the runner |
| 211 | `undeclared-identifier-c45cc1bb` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glFxaaEffectRunner` binding (and its callee `applyFxaaEffectToGl`) but still emits the registration call that passes the runner |
| 212 | `undeclared-identifier-c7551b38` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glPixelateEffectRunner` binding (and its callee `applyPixelateEffectToGl`) but still emits the registration call that passes the runner |
| 213 | `undeclared-identifier-cc7c5006` | 1 | ANIM | declaration repair (applied) | exact refused path solver declared; G2: 1 pass; registration and full constraint solving remain unlinkable |
| 214 | `undeclared-identifier-cf2fed2b` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glPosterizeEffectRunner` binding (and its callee `applyPosterizeEffectToGl`) but still emits the registration call that passes the runner |
| 215 | `undeclared-identifier-d14c2215` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glConvolutionEffectRunner` binding (and its callee `applyConvolutionEffectToGl`) but still emits the registration call that passes the runner |
| 216 | `undeclared-identifier-d687475c` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glHalftoneEffectRunner` binding (and its callee `applyHalftoneEffectToGl`) but still emits the registration call that passes the runner |
| 217 | `undeclared-identifier-d7a1d1b7` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glTiltShiftEffectRunner` binding (and its callee `applyTiltShiftEffectToGl`) but still emits the registration call that passes the runner |
| 218 | `undeclared-identifier-e3d6c4e4` | 1 | GL | runtime | the emitted code names `apply_gl_gaussian_blur_pass`, which nothing declares |
| 219 | `undeclared-identifier-e4a7b51e` | 1 | GL | compiler *(existing request)* | refused `glLensDirtEffectRunner` still registered; callee widens pooled `Ref<GlTextureRenderTarget>` intermediates to `GlRenderTarget` |
| 220 | `undeclared-identifier-e4eb111d` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glKuwaharaEffectRunner` binding (and its callee `applyKuwaharaEffectToGl`) but still emits the registration call that passes the runner |
| 221 | `undeclared-identifier-e7bf11f9` | 1 | WGPU | compiler | WGPU Scale9 draw refuses nullable material owner widening; declaration compile fails native/generic callback carrier *(existing requests extended)* |
| 222 | `undeclared-identifier-ebf3999b` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glRadialBlurEffectRunner` binding (and its callee `applyRadialBlurEffectToGl`) but still emits the registration call that passes the runner |
| 223 | `undeclared-identifier-f020c1f7` | 1 | GL | compiler *(existing request)* | the emitter refuses the `glWhiteBalanceEffectRunner` binding (and its callee `applyWhiteBalanceEffectToGl`) but still emits the registration call that passes the runner |
| 224 | `undeclared-identifier-f6affb68` | 1 | 3D | [compiler request](compiler-requests/retained-declaration-calls-omitted-callee.md) | global transmission/volume/IOR callbacks retain a refused owner resolver; declaration fails unused-header link |
| 225 | `undeclared-identifier-f91f1e72` | 1 | 2D | compiler (existing partial native carry) | G2 current effective header1/1 pass; generated Canvas getter/draw owner gaps remain *(builder4)* |
| 226 | `undeclared-identifier-fcbdae42` | 1 | 2D | compiler *(existing request)* | refused `drawCanvasTextLabel` retained as `.submit` in the renderer aggregate; declaration probe fails at `.create_data` |
| 227 | `variant-cast-7dd5385f` | 1 | GL | compiler | a functional-style cast is applied to a variant the target type cannot take |

## Carry clusters

These headers compile today only because they ship the emission the previous pinned compiler produced
(`docs/compiler-806d257.md`). They pass the sweep, so they are not in the ranking, but the pinned
compiler still owes each of these lowerings and they are G2 work of their own.

| cluster | headers | lanes | owed |
| --- | --- | --- | --- |
| `carry-effects-wgpu` | 3 | WGPU | passing only through the prior-emission override; the pinned compiler still owes this lowering |
| `carry-image-codec` | 4 | GL | passing only through the prior-emission override; the pinned compiler still owes this lowering |
| `carry-intl` | 2 | TEXT | passing only through the prior-emission override; the pinned compiler still owes this lowering |
| `carry-media` | 2 | PLAT | passing only through the prior-emission override; the pinned compiler still owes this lowering |
| `carry-scene2d-resources` | 1 | 2D | passing only through the prior-emission override; the pinned compiler still owes this lowering |
| `carry-shape` | 1 | 2D | passing only through the prior-emission override; the pinned compiler still owes this lowering |
| `carry-text` | 5 | TEXT | passing only through the prior-emission override; the pinned compiler still owes this lowering |
| `carry-tokens` | 1 | TEXT | passing only through the prior-emission override; the pinned compiler still owes this lowering |
| `carry-tray` | 1 | PLAT | passing only through the prior-emission override; the pinned compiler still owes this lowering |

## The clusters in detail

Fix order is `AGENTS.md`'s: extend the runtime, then declare the external binding, then a source patch
or emission repair, and only then an override. A cluster whose route is `compiler` needs one
`docs/compiler-requests/<slug>.md` rather than a workaround, and no header is made to compile by
deleting or stubbing behaviour.

### 1. `call-has-no-matching-9c08d665` — 117 header(s)

- cause: bound WebGPU descriptor object literals are emitted as untyped source-order braced initializers with anonymous nested owners, which no host overload can faithfully accept
- subject: `begin_render_pass`
- lanes: WGPU
- fix route: compiler
- why this route: G2 reclassified from binding. A typed descriptor overload cannot take `Array<Ref<anon>>` attachments whose `view` and `clear_value` shapes differ per site
- first failure at: `flight/render_wgpu/wgpu_render_pass.hpp:146`
- example: `no matching member function for call to 'begin_render_pass'`
- compiler request: `docs/compiler-requests/external-descriptor-literal-construction.md`

### 2. `missing-member-b1a6d5cf` — 39 header(s) (34 at G1; 4 joined in G2 from cluster 61; 1 from cluster 123)

- cause: scene2d-canvas emits `using ...get_canvas_render_state_runtime;` before the function it names is defined
- subject: `get_canvas_render_state_runtime`
- lanes: 2D
- fix route: compiler
- why this route: scene2d-canvas emits `using ...get_canvas_render_state_runtime;` before the function it names is defined
- first failure at: `flight/scene2d_canvas/canvas_render_state.hpp:69`
- example: `no member named 'get_canvas_render_state_runtime' in namespace 'scene2d_canvas'`
- compiler request: `docs/compiler-requests/local-reexport-using-before-declaration.md`

### 3. `in-operator-not-lowered-b1eae4af` — 32 current header(s), 31 in G1

- cause: the TypeScript `in` operator is emitted verbatim as C++ `in`
- lanes: 2D, 3D, GL, PLAT, WGPU
- fix route: compiler; the `render_registry_guards` receiver also needs a structural carrier
- first failure at: `flight/interaction/interaction_manager.hpp:510`
- example: `expected ')'`
- existing request extended: [membership test lowered literally](compiler-requests/membership-test-lowered-literally.md);
  related: [Interaction runtime membership](compiler-requests/interaction-runtime-property-membership.md)

In G1, four headers contain the failing expression. By include walk, the other 27 reach
`interaction_manager` (21), `render_registry_guards` (8), `explain_scene_3d_pipeline_coverage` (1), or
`wgpu_render_state` (`render_wgpu/wgpu_shader.hpp`). The last header's own first diagnostic belongs to another cluster.

| root header | receiver as emitted | keys | faithful lowering |
|---|---|---|---|
| `interaction/interaction_manager.hpp:510` | readonly structural row | `worldMatrix` | `row_has<RowKey<"worldMatrix">>` |
| `render/explain_scene_3d_pipeline_coverage.hpp:44` | readonly structural row over `NodeAny` | `materials` | `row_has<RowKey<"materials">>` |
| `render/render_registry_guards.hpp:48-81` | nominal `Ref<RenderState>` | `device`, `element`, `gl` | none until the carrier changes |
| `render_wgpu/wgpu_render_state.hpp:259` | variant of runtime and device-state row | `EntityRuntimeKey` | test the one arm declaring it as required |

Every probed string key is required on every type that declares it, so present-but-undefined cannot occur
for them. For a key outside the static schema, `row_has` asks the shared owner whether the cell exists,
without reading its value or invoking a getter. That makes it exact at the two structural sites.

The Symbol key is different. `Entity` declares `[EntityRuntimeKey]: EntityRuntime | undefined`, and entities
are routinely built with it undefined. `row_has(Symbol)` reports whether the slot holds a value, so it would
answer false where JavaScript answers true. Only the variant-arm test is faithful there.

The nominal site cannot be fixed by lowering `in` alone. The native backend states derive only from
`ReferenceEnabled`, not from `RenderState`, so a `Ref<RenderState>` never holds the state being probed.
Any compiling spelling would answer false at all seven sites and always return the fallback message.

In G1, eight more headers contain the expression but fail first on another cluster's diagnostic. They are listed
in the request as latent sites and are not reclassified here. No source patch, repair, override or runtime
change was made. An own-keys predicate, value read or
nominal downcast would narrow the semantics, and no erased TypeScript form is simpler than `in`.

**G1 status retained: 0/31 pass, 31 fail.** No C++ recompile was run because no code changed.

G2 particle follow-on: after the attribute-location binding fix, the owned
`scene3d_gl/gl_particle_emitter_3d.hpp` first fails at its previously latent literal `in` expression
at line 113. It joins this cause as the 32nd current failure and adds the 3D lane. No unowned row was
recompiled or changed. See the [measured particle site](compiler-requests/membership-test-lowered-literally.md#g2-particle-3d-reaches-the-previously-latent-membership-site).

### 4. `undeclared-identifier-993e82f1` — 29 header(s) *(builder4/G2)*

- cause: the template parameter name `Traits` leaks into emitted code as a free identifier
- subject: `Traits`
- lanes: 2D, 3D, PLAT, TEXT, WGPU
- fix route: compiler
- why this route: the template parameter name `Traits` leaks into emitted code as a free identifier
- first failure at: `flight/scene3d/scene_document.hpp:106`
- example: `use of undeclared identifier 'Traits'`
- owned by: builder4 (G2: reserved Traits / text-binding follow-up)

G2 scoped result after E0a, Clang 22.1.8 at `-j1`: **0/29 compile; all 29 still fail on `Traits`**.
The original 29 rows were selected explicitly; the first three completed before a checkpoint stop,
and the remaining 26 resumed with `-Wfatal-errors`. No header was inferred passing or left unattempted.

| actual first error site | rows |
| --- | --- |
| `flight/scene3d/scene_document.hpp:106` | 17 |
| `flight/textinput/text_input_editing.hpp:172` | 9 |
| `flight/physics3d/node_sync.hpp:31` | 1 |
| `flight/scene2d_formats/lottie_solid_layer.hpp:50` | 1 |
| `flight/scene3d/scene_node_transform.hpp:82` | 1 |

These non-generic callers still convert an argument to a target containing the callee's unsubstituted
`Traits` parameter. The native TextLabel work cleared none of these first causes. They link the existing
[generic call argument substitution request](compiler-requests/generic-call-argument-uses-unsubstituted-parameter-type.md);
no runtime alias or replacement owner was added. The five source-site files are unchanged across the
clean delivered-base update after the check. G1 sweep totals and unowned rows remain unchanged.

### 5. `call-has-no-matching-3882b3e9` — 28 header(s) *(builder4/G2)*

- cause: bare object literals passed to SDK create_texture do not construct the TypeScript-selected owned options carrier
- subject: `create_texture`
- lanes: 2D, 3D, TEXT
- fix route: compiler
- why this route: the SDK overload needs the exact options owner selected by TypeScript
- first failure at: `flight/scene3d_formats/shared.hpp:100`
- example: `no matching function for call to 'create_texture'`
- owned by: builder4 (G2: reserved Traits / text-binding follow-up)

G2 scoped result after E0a, Clang 22.1.8 at `-j1` with `-Wfatal-errors`: **0/27 compile; all 27
still fail on `create_texture`**. Every original selected header was attempted; no crashes or
unattempted rows remain.

| actual first error site | rows |
| --- | --- |
| `flight/scene3d_formats/shared.hpp:100` | 17 |
| `flight/scene2d_formats/rive_asset_binding.hpp:106` | 6 |
| `flight/bitmaptext/update_bitmap_text.hpp:121` | 4 |

The earlier device-binding attribution was wrong: these are calls to SDK `flight::texture::create_texture`.
TypeScript selects the 2D options overload by its literal discriminant or the optional 2D default.
The emitted bare literal constructs none of the owned options carriers. This is the existing
[contextual options-owner construction request](compiler-requests/contextual-object-literal-owner-materialization.md).
Its required lowering retains the selected overload, original source/resource references, absence and
null states, and source evaluation order. Native TextLabel changes cleared none of these first causes.

All 27 selected header bodies, the three error-site files, and the options/factory files are unchanged
from check start `1fe98c66` through completion on delivered `c343d73a`. G1 sweep/carry products and
unowned rows/clusters remain unchanged; no SDK behavior or new options overload was added.

### 6. `other-52e846a6` — 18 header(s)

- cause: a nullable attachment array element (reference or null) is not lifted into the slot carrier that also admits undefined, so the conditional pairs `std::optional<Ref<Attachment2D>>` with `variant<Ref<Attachment2D>, Null, Undefined>`
- lanes: ANIM
- fix route: compiler
- why this route: the conditional and assignment operate on standard-library carriers. A runtime helper, binding or declaration repair cannot add the conversion, and the cheap branch rewrite still needs it. An override would copy a partially refused module. Null and undefined must stay distinct.
- first failure at: `flight/skeleton2d/skeleton2d_animation_target.hpp:217`
- example: `incompatible operand types ('value_type' and 'variant<Ref<Attachment2D>, Null, Undefined>' )`

- G2 check: **0/18 compile**, 18 failures and zero unattempted at `-j1`; every row retains the same root diagnostic.
- Root: a nullable array element is not lifted into the slot's distinct reference/null/undefined carrier.
- Compiler request: [nullable array element into an optional-nullable slot](compiler-requests/nullable-array-element-into-dual-sentinel-slot.md).
- The cheap branch-assignment source rewrite still emits an optional directly into the variant. No source patch, runtime change, repair or override is carried; the cluster remains open.
- G2 link pass: the cluster entry now carries this request as its primary `compilerRequest` (the 18 rows already did), and each row records the measured site `skeleton2d_animation_target.hpp:217`. The measurement above was Clang 22.1.8 at `-j1`. No C++ changed, so nothing was recompiled.
- compiler request: `docs/compiler-requests/nullable-array-element-into-dual-sentinel-slot.md`

### 7. `call-has-no-matching-47410fe2` — G2 recheck of 16 headers

The actual receiver is `Array<String>`, and both Array and SequenceView already provide `map`.
The original failure came from passing the overloaded `flight::to_number` name as a callback.
The runtime now uses a callable Number conversion object, following `to_boolean`, with the
same String, C-string, string-view, double and bool conversions. Array/SequenceView callback
arity, numeric indices and retained source owners are unchanged; mapping still returns a fresh Array.

G2: **2/16 compile, 14 fail**, all attempted at `-j1` with Clang 22.1.8, compiler `806d257` /
Flight `a627849`. The runtime build and `flight_cpp.runtime` test pass (1/1), including Number
callbacks through Array, retained SequenceView and Function, order, fresh result identity,
unchanged source values, NaN, infinity and signed zero.

Passing: `flight/scene2d_formats/svg_xml.hpp` and `flight/scene2d_formats/svg_geometry_path.hpp`.
The other rows remain failures and have been reclassified by their newly exposed diagnostics:

| follow-on cluster | headers | first failure | cause |
| --- | --- | --- | --- |
| `call-has-no-matching-4f8413e4` | 9 | `flight/scene3d_formats/collada_xml.hpp:44` | a numeric builtin used as a callback is emitted as the overloaded std::isfinite name |
| `undeclared-identifier-fb3e0e45` | 3 | `flight/scene2d_formats/svg_color.hpp:134` | parseSvgColor is refused for unproven optional member storage, but retained resolveSvgColor still calls it |
| `no-viable-conversion-a21c880d` | 1 | `flight/scene2d_formats/svg_style.hpp:53` | a numeric optional-chain length is erased to optional<Any> before assignment to double |
| `no-viable-conversion-deeedfb9` | 1 | `flight/scene2d_formats/svg_transform.hpp:52` | a readonly Matrix row is converted through a writable row while forming a readonly MatrixLike argument |

The nine COLLADA failures are covered by the one new
[callable numeric predicate request](compiler-requests/builtin-numeric-predicate-callback.md).
The five SVG failures above remain open for separate triage; no parser body, erased-value
conversion or writable projection was invented to make them pass. No generated header,
source patch or override changed in this work. Builder6 owns the independent SequenceView
filter implementation.

The opening totals and this group's original rank describe G1. Only these 16 header rows and
their replacement failure clusters changed here; `previousCluster` preserves the G1 group.

### 8. `other-40da43f7` — 16 header(s)

- cause: an optional<Record<...>> is initialised from a shape that is not its declared storage
- lanes: 2D, 3D, ANIM
- fix route: compiler
- why this route: an optional<Record<...>> is initialised from a shape that is not its declared storage
- first failure at: `flight/scene3d_formats/three_ds_parse.hpp:208`
- example: `no matching constructor for initialization of 'optional<Record<String, variant<bool, double, String>>>'`
- compiler request: [optional-record-literal-not-materialized.md](compiler-requests/optional-record-literal-not-materialized.md) (filed; sites still failing, status unchanged)

### 9. `static-cast-assertion-9674a66c` — 15 header(s)

- cause: a TypeScript `as` on `JSON.parse` is emitted as a `static_cast` from `JsonValue` to an unrelated `Ref<T>`
- lanes: 2D
- fix route: compiler
- why this route: an assertion between unrelated types is not a cast; the emitter must materialize the declared owner or refuse
- first failure at: `flight/scene2d_formats/lottie_document.hpp:324`
- example: `no matching conversion for static_cast from 'JsonValue' to 'flight::Ref<flight::types::LottieDocument>'`
- compiler request: `docs/compiler-requests/json-parse-assertion-static-cast.md`

### 10. `call-has-no-matching-076d207f` — G2 recheck of 14 headers

Every `create_pipeline_layout` site passes one field, `.bind_group_layouts`, as an
`Array<WgpuBindGroupLayout>`: no anonymous owner and no field order to get wrong. So the host now
declares `WgpuPipelineLayoutDescriptor` (`bind_group_layouts`, `label`) and a typed overload beside the
variadic one, as `create_texture` already has. Its body is still `unavailable`; nothing is implemented.

G2: **0/14 compile, 14 fail**, all attempted at `-j1` with g++ 15.2. The sweep's Clang 22 is not
installed in this workspace, so the diagnostic text below is g++'s. `flight/host_sdl/wgpu.hpp` still
compiles on its own, and the passing `flight/effects_wgpu/wgpu_effect_pass.hpp`, which has two
`create_pipeline_layout` sites, still passes, as do the three carry headers in `carry-effects-wgpu`.
All 14 now fail at the next call in `wgpu_shader.hpp`:

| follow-on cluster | headers | first failure | cause |
| --- | --- | --- | --- |
| `call-has-no-matching-4c12e411` | 14 | `flight/render_wgpu/wgpu_shader.hpp:98` | `create_bind_group_layout` entries are anonymous nested owners for the bound `GPUBindGroupLayoutEntry` |

That shape is the [descriptor literal request](compiler-requests/external-descriptor-literal-construction.md),
so no second request is filed.

### 11. `other-e9936ce6` — 13 header(s)

- cause: a pair literal the declared element type spells as `std::tuple<A, B>` is emitted as `flight::Array{...}`, so class-template deduction fails
- lanes: 3D
- fix route: compiler
- why this route: the inner literal must be the declared tuple; a deduction guide would compile it with the wrong element type
- first failure at: `flight/scene3d_formats/collada_feature_requirements.hpp:24`
- example: `no viable constructor or deduction guide for deduction of template arguments of 'Array'`
- compiler request: `docs/compiler-requests/array-pair-literal-deduction.md`

### 12. `call-has-no-matching-d6950545` — G2 recheck of 12 header(s)

- cause: `get_node_child_count<Traits>` (generated and overridden `node/hierarchy.hpp`) takes `StructuralRef<RowReadonly<RowMerge<RowOf<Ref<Node<Traits>>>, RowOf<Traits>>>>`, in which `Traits` is not deducible. The emitted calls pass a different row without the type argument TypeScript inferred.
- subject: `get_node_child_count`
- lanes: 2D, CORE
- fix route: compiler
- why this route: only the checker knows the inferred trait. A permissive overload, a cast or a narrowed row would guess it.
  - The command rows (6) pass `Readonly<NodeAny>`. That is Face A of [node-call-trait-absent-from-scope](compiler-requests/node-call-trait-absent-from-scope.md).
  - The scene2d-formats rows (6) pass `Node2D`, whose `Node2DTraits` comes through `extends`. That is [heritage-argument-generic-deduction](compiler-requests/heritage-argument-generic-deduction.md).
- first failure at: `flight/command/command_binding.hpp:155`
- example: `no matching function for call to 'get_node_child_count'`
- Scoped native recheck (GCC 15.2, jobs 1): **3 attempted / 0 pass / 3 fail**, at `command_binding.hpp:155`, `svg_bounds.hpp:67` and `lottie_test_fixtures.hpp:125`. Each row carries its measured location. The other 9 headers reach `command_binding.hpp` or `svg_bounds.hpp` through their includes. They were not recompiled, keep their recorded failure, and are given no location. No overload, cast, copied owner or narrowed domain is supplied.
- compiler request: `docs/compiler-requests/node-call-trait-absent-from-scope.md` (command), `docs/compiler-requests/heritage-argument-generic-deduction.md` (scene2d-formats)

### 13. `no-viable-conversion-9c3803c5` — G2 recheck of 9 header(s)

- cause: the generic `flight::render::noop_renderer_data(Ref<RenderState>, NodeAny)` is stored in `Scene2DRenderer.create_data`, whose `std::function` passes a flattened `Node2D` that does not widen to `NodeAny`. TypeScript accepts it because a `Node2D` is a `Node`.
- subject: `noop_renderer_data`
- lanes: 2D, WGPU
- fix route: compiler
- why this route: the carriers need the same-owner `Node2D` to `NodeAny` widening of [interface-heritage-widening](compiler-requests/interface-heritage-widening.md). This is the `Node2D` form of its existing `Sprite` create-data sections. An adapter that rebuilds the node, a function-pointer cast or a narrowed domain would not preserve the owner.
- first failure at: `flight/scene2d_canvas/canvas_node_2d.hpp:62`
- example: `no viable conversion from 'optional<Ref<RendererData>> (Ref<RenderState>, NodeAny)' ') to 'function<optional<Ref<RendererData>> (Ref<RenderState>, Node2D)>' >')`
- Scoped native recheck (GCC 15.2, jobs 1): **3 attempted / 0 pass / 3 fail** at the three roots, `canvas_node_2d.hpp:62`, `dom_node_2d.hpp:53` and `wgpu_node_2d.hpp:120`, which carry measured locations. The six other Canvas rows reach `canvas_node_2d.hpp` through their includes. They were not recompiled, keep their recorded failure, and are given no location. Nothing is carried.
- compiler request: `docs/compiler-requests/interface-heritage-widening.md`

### 14. `no-viable-conversion-dfea2d92` — 9 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 3D
- fix route: compiler
- first failure at: `flight/scene3d_gl/gl_pbr_extension_registry.hpp:64`
- example: `no viable conversion from '(lambda at /home/joshua/.quimby/workspaces/4611d72d-6367-41cb-8cf3-aade4e05de18/.quimby/agents/fbd4b072-2bb9-4e1f-a50b-5b91857e85fc/repo/overrides/include/flight/scene3d_gl/gl_pbr_extension_registry.hpp:64:24)' to 'function<bool (optional<Texture>)>' >')`

### 15. `variant-cast-ccfedad4` — 9 header(s), still failing

- cause: the native readonly active-program row is reconstructed through a writable row before a readonly draw-helper call
- lanes: 3D
- fix route: compiler; native storage and consumer emission must agree on the structural carrier
- first failure at: `flight/scene3d_gl/gl_depth_mesh_material_renderer.hpp:66`
- example: `no matching conversion for functional-style cast from 'StructuralRef<RowReadonly<RowOf<shared_ptr<GlMeshProgram>>>>' to 'StructuralRef<RowWritable<RowOf<Ref<GlMeshProgram>>>>'`
- existing request extended: [guarded readonly optional helper call](compiler-requests/guarded-readonly-optional-helper-call.md)

The effective `types/gl_scene_3d_runtime.hpp` override retains `active_mesh_program` as an optional
readonly structural view of the concrete program allocation. The pinned source and generated type
header instead name `GlMeshProgram | null` and an optional nominal reference. The nine generated
renderer callers read that slot once, guard absence, and try to form a writable row from its actual
readonly payload before passing it to the already-readonly `draw_gl_mesh_subset` parameter.
This is a carrier disagreement at the native handoff, not a variant cast.

The runtime's readonly-to-writable rejection is correct. The helper should receive the existing readonly
program owner directly once storage and emission agree. No runtime conversion, source rewrite,
generated body, override or cache behavior changed. The related request already existed and was
extended with the actual storage provenance and all nine sites rather than duplicated.

**G1 status retained: 0/9 pass, 9 fail.** No C++ recompile was run because code did not change, per
G2's scoped-only-when-changed rule. Only these owned rows gained request links and cast locations.

### 16. `call-has-no-matching-b569e92e` — 8 header(s)

- cause: the emitted call to `init_joint_base` matches no declared overload
- subject: `init_joint_base`
- lanes: 3D, ANIM
- fix route: compiler
- first failure at: `flight/physics2d/joint_factories.hpp:65`
- example: `no matching function for call to 'init_joint_base'`
- compiler request: [entity-construction-view-passed-to-owner-parameter.md](compiler-requests/entity-construction-view-passed-to-owner-parameter.md) (filed; sites still failing, status unchanged)

### 17. `undeclared-identifier-294e632e` — 8 G1 header(s), declaration repaired

- original cause: retained code calls `apply_swf_authored_bounds` after its function body was refused
- lanes: ANIM
- fix route: declaration-only emission repair, with implementation debt still open
- compiler request: [retained-declaration-calls-omitted-callee.md](compiler-requests/retained-declaration-calls-omitted-callee.md)

The exact refusal marker, retained call and absence of any definition justify
`swf-declare-refused-authored-bounds`. It declares the emitted `Node2D` and optional readonly
`SwfTagRectangle` carriers before the caller. No function body or replacement node-data owner is
supplied. The underlying authored-bounds reader and writer still need the owner-recovery contract
in [assertion-to-undeclared-row-member.md](compiler-requests/assertion-to-undeclared-row-member.md).

Initial declaration G2: **1/8 compiles, 7 fail, 0 unattempted**, all attempted at `-j1` with Clang 22.1.8,
compiler `806d257` / Flight `a627849`. `flight/swf/swf_node.hpp` passes. Including that actual
repaired header without calling the function also compiles, links and runs. Calling
`apply_swf_authored_bounds(Node2D{}, std::nullopt)` compiles to an object, then fails linking on
that exact undefined symbol. Authored bounds remains unavailable at runtime.

| follow-on cluster | owned headers | first failure | route / cause |
| --- | --- | --- | --- |
| `call-has-no-matching-dd66c064` | 5 | `flight/swf/swf_timeline_parse.hpp:122` | compiler: SwfReader class instance has no owner-preserving SwfTagReader interface carrier; [existing request](compiler-requests/class-instance-as-implemented-interface.md) |
| `no-viable-conversion-3cfd059c` | 2 | `flight/swf/swf_define_shape_handler.hpp:106` | binding: native copy_shape_commands rejected the readonly source; resolved by the follow-up below |

The five callback failures are `swf_content_capabilities.hpp`, `swf_document.hpp`, `swf_header.hpp`,
`swf_requirements.hpp` and `swf_tag_census.hpp`, all under `flight/swf/`. The source supplies all
five parse callback arguments; this is a reader class/interface conversion gap, not an arity error.
They join the existing five-row callback cluster without recompiling or changing those other rows.

The two initial binding failures were `flight/swf/swf_define_shape_handler.hpp` and
`flight/swf/swf_shape_tag_family.hpp`. Their retained caller passes a readonly structural Shape
view. The native override instead required a nominal `Ref<Shape>` source, although
`packages/shape/src/shape.ts:32` declares `Readonly<Shape>`.

The assigned two-row binding follow-up now accepts the canonical readonly structural Shape view
and reads its original data row cell. The legacy nominal-reference overload forwards to that same
implementation. The shape override remains partial with all other declared debt intact, and its
manifest still matches the unchanged current generated-source hash.

**Binding follow-up: shape semantic/owner CTest 1/1 pass; override integrity pass;
scoped headers 0/2 pass, 2 fail, 0 unattempted at `-j1`** with Clang 22.1.8. The semantic test
checks replaced source data cells and later token mutations through the retained readonly owner,
destination array identity/order, shared-array/self-copy behavior, empty legacy calls and exact
content/bounds invalidation. No copied Shape or writable capability upgrade was introduced.

Both headers advance past `copy_shape_commands` to the compiler's mismatched inferred placement
array at `flight/swf/swf_define_shape_handler.hpp:115`. The spread lambda constructs
`Array<Ref<SwfTagPlacement>>`, but its destination local is declared `Array<Any>`.
The two rows remain failures in `no-viable-conversion-2411db39`, linked to
[map-values-spread-local-element-carrier.md](compiler-requests/map-values-spread-local-element-carrier.md).
The old two-row binding cause is resolved and removed from active JSON clusters. This follow-up
recompiled only those two headers; the other five callback failures remain unchanged.

The opening sweep totals and this group's rank remain the historical G1 snapshot. Only these
eight header rows, the resolved failure cluster and their replacement cause metadata changed;
`previousCluster` retains the G1 group. A passing header does not make the refused function callable.

### 18. `undeclared-identifier-3e3cccce` — G2 recheck of 8 headers

- cause: the emitted code names `blit_offset_shaders`, which nothing declares
- subject: `blit_offset_shaders`
- lanes: GL
- fix route: runtime (partial override), with the binding request for the compiler
- first failure at: `flight/effects_gl/gl_effect_blit_shader.hpp:87`
- example: `use of undeclared identifier 'blit_offset_shaders'`
- compiler request filed: [a bound Flight interface never reaches its weak-key policy](compiler-requests/bound-interface-weak-map-key-policy.md)

`glEffectBlitShader.ts` declares three module-scope `WeakMap<GlContext, …>` caches. The emitter refuses
them (`cpp-weak-map-key-representation-unproven`) but emits the getters that use them. `GlContext` is a
Flight interface, and the external weak-key policy is read only for ambient key names, although the
binding, `using GlContext = flight::host_sdl::WebGl2Context` and `WebGl2ContextWeakPolicy` all exist.
The partial override `effects-gl-blit-shader-context-caches` supplies the three maps as the source's own:
- one per program lifetime;
- keyed by context identity, so copies of a context are one key;
- holding the context weakly, with no value keeping its key alive.

It makes two further corrections:
- It restores the dropped `{ ...base, locOffset }` spread, which had left the program fields value-initialized.
- It gives `BlitOffsetShaderLocations` one carrier instead of two unrelated ones.

The three `apply*` passes stay NOT GENERATED for their separate contextual-union refusal.

`flight_cpp.generated_effects_gl_blit_cache` (registered with the `Flight::HostSdlGl` tests) checks:
- copies of a context hit one entry;
- distinct contexts and the three maps are independent;
- a getter hit returns the identical cached object without compiling;
- the context is not kept alive;
- a dead context's program is released by the next lookup.

The miss branch needs a live GL context and is compiled but not executed. This machine has no SDL/GL, so
the check was run with a scratch link shim that aborts on any GL call. It passed. Two mutations were
detected: sharing the erase and blit maps failed 2 checks, and a never-storing cache reached the GL miss
path.

**G2: 1/8 compile, 7 fail** (`-j1`, Clang 22.1.8, compiler `806d257` / Flight `a627849`). Passing:
`flight/effects_gl/gl_effect_blit_shader.hpp`. The seven filters (bevel, drop shadow, gradient bevel,
gradient glow, inner glow, inner shadow, outer glow) move to `undeclared-identifier-e7884ac7`. They first
fail in the sibling `gl_effect_box_blur.hpp` on `box_blur_shaders`. That is the same refused `GlContext`
cache, which also recurs as `tint_shaders`/`invert_tint_shaders` and `bevel_composite_shaders`, along with
the same split intersection carrier. They also call this module's still-refused
`apply_gl_effect_blit_pass`/`apply_gl_effect_erase_pass`, so further sibling overrides alone would not
make them compile.
That cluster therefore names both filed causes: `docs/compiler-requests/contextual-union-value-arm-member-shape.md`
for the two apply passes, and `docs/compiler-requests/bound-interface-weak-map-key-policy.md` for the
`GlContext` weak-key cache. No cache-only override was taken while the apply passes are unsupplied.

### 19. `invalid-operands-4b19651b` — 7 header(s)

- cause: a type guard narrows an `unknown`, but the emitted read after it is still the erased `flight::Any`, and `>` is applied to that `Any`
- lanes: 2D, ANIM
- fix route: compiler
- why this route: G2, measured with GCC 15.2 at `-j1`. There are two causes and no runtime fix: a relational operator on `flight::Any` would have to invent JavaScript's abstract comparison, which is a permissive fake.
  - **Lottie, 4 headers** (`lottie_counts`, `lottie_requirements`, `contract`, `_internal_index`). `typeof record.fr === 'number' && record.fr > 0` tests correctly, but reads `record.get("fr").value()` rather than `.as_number()`. Narrowed locals already use `as_number()`.
  - **Skeleton2d, 3 headers** (`skeleton2d_json_analyzer`, `spine_json_requirements`, `spine_json_section_counts`). `Array.isArray(value)` then `value.length` is emitted as `named_properties(value).get("length") > 0.0`.
- first failure at: `flight/scene2d_formats/lottie_counts.hpp:26`
- example: `invalid operands to binary expression ('Any' and 'double')`
- compiler request: `docs/compiler-requests/typeof-narrowed-record-member-read.md` (Lottie rows); `docs/compiler-requests/erased-array-element-reading.md` (skeleton2d rows, existing)

### 20. `missing-member-fd48c0bb` — 7 header(s)

- cause: the emitted code reads a member the target does not declare (`collide_physics_3d_collider_shapes`)
- subject: `collide_physics_3d_collider_shapes`
- lanes: 3D
- fix route: compiler — [narrowed imported union request](compiler-requests/narrowed-imported-union-into-declared-parameter.md)
- first failure at: `flight/physics3d/contact_intake.hpp:284`
- example: `no member named 'collide_physics_3d_collider_shapes' in namespace 'physics3d'`

- G2 disposition: the missing name is a refused TypeScript callee, not a missing runtime primitive. `collider_collision.hpp` marks `collidePhysics3DColliderShapes` NOT GENERATED at source line 19 (`cpp-contextual-union-inequivalent`); `contact_intake.hpp:284`, `continuous.hpp:375`, and `solver.hpp:510` retain calls. The narrowed seven built-in shape arms must enter `CollisionShape3D` without replacing their owners.
- The existing declaration repair requires refusal and call in the same header; these witnesses span sibling headers. No repair, source patch, runtime binding, override, or body is carried. The request records both the missing declaration and the refused conversion, including why the earlier whole-union-local workaround cannot be applied before this source's concave-shape guards.
- Numbers: **0/7 pass, 7 fail**, preserved from G1; no C++ input changed and no unchanged compile rerun was performed. All seven owned rows remain failing and link the request. No package behavior or link success is claimed.

### 21. `other-18dcce71` — 7 header(s)

- cause: Array.isArray narrowing leaves an erased array length read as Any in a numeric assignment
- lanes: ANIM
- fix route: compiler — [erased-array reading request](compiler-requests/erased-array-element-reading.md)
- first failure at: `flight/skeleton2d_formats/spine_parse_helpers.hpp:90`
- example: `assigning to 'double' from incompatible type 'Any'`
- G2 classification: **one shared cause**, not seven independent failures. Each owned row has the same first diagnostic in `spine_parse_helpers.hpp:90`: the `Array.isArray(raw)` branch of `skipCrumbSpineTimelineGroup` emits `count = flight::named_properties(raw).get("length")`, assigning `Any` to `double`.
- The numeric extraction is only half the gap: `named_properties(const Any&)` resolves object row owners, while an erased array is held as `AnyArray`, so this view returns an empty owner and its length read is undefined. An implicit numeric conversion, an added cast, or replacing array length with enumerable key count cannot faithfully fix it. Exact typed-array recovery already exists, but an array guard alone does not prove its element carrier; the request records the current site beside the earlier measured failed source spellings.
- Numbers: **0/7 pass, 7 fail**, retained from G1. All seven rows stay failed and link the same request; cluster ID is retained for continuity while its cause and route are corrected. No C++ input, source patch, binding, declaration repair, or override changed; no unchanged compile rerun or behavior improvement is claimed.

### 22. `undeclared-identifier-7dc86326` — 7 G1 header(s), declaration repaired

- original cause: `getWgpuSkinningAdapter` calls `getWgpuScene3DRuntime` after its body was refused
- lanes: WGPU
- fix route: declaration-only emission repair; remaining causes are compiler defects
- compiler requests: [retained omitted callee](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-site-scene3d-wgpu-scene-runtime), [runtime owner recovery](compiler-requests/asserted-erased-runtime-owner.md#g2-site-flighthqscene3d-wgpu-scene-runtime)

The actual refusal is `cpp-structural-assertion-owner-unproven` at the source assertion from
`RenderStateRuntime` to `WgpuRenderStateRuntime`. The module already emits a concrete
`WeakMap<Ref<WgpuRenderState>, Ref<WgpuScene3DRuntime>>`; this is not an unrepresented WeakMap
value or evidence of a missing GPU provider. Construction and retention still erase the wider runtime
owner, so a faithful body needs more than changing this accessor's return type.

`scene3d-wgpu-declare-refused-scene-runtime` uses the existing exact refusal marker, same-header direct
call and absent definition witnesses. It declares only the actual `Ref<WgpuRenderState>` parameter and
`Ref<WgpuScene3DRuntime>` result. The real header compiles, links and runs without a call; an object
calling the retained `get_wgpu_skinning_adapter` compiles, then fails to link exactly on
`get_wgpu_scene_3d_runtime`. The original PARTIAL marker and refusal remain. No GPU implementation,
cache replacement, owner cast/copy or new design is supplied.

G2 at compiler `806d257` / Flight `a627849`, Clang 22.1.8, **one worker: 7 attempted, 2 pass,
5 fail, 0 unattempted**. The runtime header and
`enable_wgpu_scene_3d_forward_light_selection_guards.hpp` compile; this does not make the refused
accessor callable or complete the package. Existing repair witness/body-rejection tests: **3/3 pass**.

| follow-on cluster | owned headers | first failure | compiler request |
| --- | --- | --- | --- |
| `undeclared-identifier-cebba2ff` | 3 | `run_wgpu_custom_shader_guards` undeclared at custom-shader guards header:116 | [retained omitted declaration](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-site-scene3d-wgpu-scene-runtime); a stored function value supplies no direct-call witness for another repair |
| `call-has-no-matching-9c08d665` | 1 | `begin_render_pass` descriptor mismatch at `render_wgpu/wgpu_render_pass.hpp:146` | [existing descriptor construction request](compiler-requests/external-descriptor-literal-construction.md); only this newly exposed owned row joins the aggregate |
| `missing-member-ff8549f6` | 1 | `image.value().kind` reads directly on a variant at `wgpu_environment_cube.hpp:50` | [active-arm member projection](compiler-requests/reference-backed-outcome-variant-narrowing.md#g2-cube-image-value-variant) |

Only the seven owned rows were rechecked and updated. The existing descriptor aggregate grows by that
one owned row; other headers, clusters and historical sweep totals are unchanged. No later failure is
promoted to a pass and no broader native sweep was run.

Generation follow-up at the same exact pins: `node scripts/pkgGenerate.mjs @flighthq/scene3d-wgpu
--check --progress` completed once: 1,527 of 1,669 modules emitted and 142 refused in the 37-package
dependency closure. The package
check **failed on one path**, `include/flight/scene3d_wgpu/scene_3d_wgpu_pipeline.hpp`; it reported no
drift for the repaired runtime header or the manifest entry. Thus the new declaration artifacts
reproduce, while the package as a whole is not generation-current. The check left the inventory
unchanged and restored both pinned source checkouts cleanly. The separate pipeline drift is reported
to Foreman; no unrelated regeneration or repeat of the earlier native/link checks is claimed.

The subsequent bounded pipeline reconciliation used one authorized exact-pin emission of
`scene3DWgpuPipeline.ts`, retained in scratch through normal patch application and cleanup. Its
actual diff changes only the private anonymous owner/guard suffix `dcdbf6a9260ce6ad` to
`19350a15f76885a9`; replacing that name makes the files byte-identical. Member types, callback
signatures, values, order, allocation and function bodies are unchanged. No other inventory file
references that owner, and the existing manifest repair record is identical, so only the emitted
pipeline header was refreshed. The incomplete one-module tree and its structural table were not
merged into the inventory.

Changed-header check, one worker: **1 attempted, 0 pass, 1 fail, 0 unattempted**. The first diagnostic
remains `expected ')'`, now recorded precisely at `flight/render/render_registry_guards.hpp:48`:
the existing [literal membership-test request](compiler-requests/membership-test-lowered-literally.md)
already describes that nominal `Ref<RenderState>` receiver and its unlowered `in` expression. Only
this owned row gains the measured location and request link; its failure and 31-header family remain
unchanged. The refreshed header matches the retained single-module output exactly. No full-package
generation check was repeated, no whole-package pass is claimed, and no GPU implementation or new
owner design was added.

### 23. `undeclared-identifier-b90ed4db` — 7 header(s)

- cause: a retained callback alias references the DragonBones animations handler after its body is refused
- subject: `dragon_bones_animations_section_handler`
- lanes: ANIM
- fix route: compiler — [erased-array reading request](compiler-requests/erased-array-element-reading.md)
- first failure at: `flight/skeleton2d_formats/dragon_bones_animations_handler.hpp:128`
- example: `use of undeclared identifier 'dragon_bones_animations_section_handler'`
- G2 disposition: one shared omitted-handler family. `dragon_bones_animations_handler.hpp:41` marks `dragonBonesAnimationsSectionHandler` NOT GENERATED (`cpp-presence-test-without-absence-storage`); line 128 retains a `std::function<void(Ref<DragonBonesSectionContext>)>` alias initialized from that missing function. No declaration or later definition exists, so moving declarations cannot restore the body.
- This is a function-value reference, not a retained direct call. An in-memory probe through the existing `declare-refused-function` handler confirms zero matches and byte-identical output; its required direct-call witness is absent. No repair or body was added, so no negative-link check is applicable or claimed. The request already names the erased-array/null-test refusal in this source and now records the retained alias, registry references, exact context carrier, and preservation of handler identity and real animation behavior.
- Numbers: **0/7 pass, 7 fail**, retained from G1. All seven owned rows remain failing and request-linked. No C++ input changed, no native rerun, and no runtime/behavior/link improvement is claimed.

### 24. `invalid-operands-ec4dbe74` — 6 header(s)

- cause: an operator is applied to operand types it does not accept
- lanes: CORE, GL
- fix route: runtime
- first failure at: `flight/render/enable_color_adjustment_guards.hpp:62`
- example: `invalid operands to binary expression ('variant<function<void (Ref<RenderState>, NodeAny)>, Null, Undefined>' >, Null, Undefined>') and 'void ()')`

### 25. `missing-member-237022b0` — 6 header(s)

- cause: `flight_document_text.hpp` calls `create_document_refusal`, which belongs to `sceneDocumentRefusal.ts`. That module is refused whole (`cpp-imported-type-unsupported:unsupportedAmbientReference`) because it imports `FlightDocumentResourceLookup`, which is `Readonly<Record<string, unknown>>`. `flight/types` already emits that alias as `flight::Record<flight::String, flight::Any>`, so it is not ambient.
- subject: `create_document_refusal`
- lanes: 2D, TEXT
- fix route: compiler
- why this route: G2, measured with GCC 15.2 at `-j1`. The callee's NOT GENERATED surface is a whole-module placeholder in another header, so the same-header `declare-refused-function` witnesses do not hold, and no declaration or body is supplied. It is the existing imported-Record-alias family.
- first failure at: `flight/scene_document/flight_document_text.hpp:226`
- example: `no member named 'create_document_refusal' in namespace 'scene_document'`
- compiler request: `docs/compiler-requests/imported-record-alias-loses-runtime-representation.md`

### 26. `missing-member-e27d26f9` — 6 header(s)

- cause: the emitted code reads a member the target does not declare (`attach_gltf_pbr_extension`)
- subject: `attach_gltf_pbr_extension`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_formats/gltf_anisotropy.hpp:121`
- example: `no member named 'attach_gltf_pbr_extension' in namespace 'scene3d_formats'`

### 27. `no-viable-conversion-f02000a9` — 6 header(s)

- cause: `RenderProxy2D extends RenderProxy` in the source, but the emitted owners are independent, so
  `Ref<RenderProxy2D>` does not widen to the `Ref<RenderProxy>` parameter
- lanes: 2D, GL
- fix route: compiler — [interface heritage widening](compiler-requests/interface-heritage-widening.md)
- first failure at: `flight/render/render_proxy_adapter.hpp:54`
- example: `no viable conversion from 'Ref<RenderProxy2D>' to 'Ref<RenderProxy>'`

`applyRenderProxyAdapter(state, source, data: RenderProxy2D)` calls `updateRenderProxyRenderer(state,
data)`, whose parameter is `Ref<RenderProxy>`. The source wide-narrows implicitly; the emitted structs
repeat the base fields and inherit only `ReferenceEnabled`, so the same object cannot enter the base
typed slot. The other five headers — `render/render_cache.hpp`,
`scene2d_canvas/canvas_cache.hpp`, `scene2d_dom/dom_cache.hpp`, `scene2d_dom/scene_2d_dom_pipeline.hpp`,
`scene2d_gl/gl_cache.hpp` — fail through their render dependencies on the same call.

No workaround is carried: a cast, a copied base owner or a wrapper adapter would split the existing
proxy's identity or narrow the domain. The request's shape is the declared heritage
(`struct RenderProxy2D : RenderProxy`), which lets the same object enter the base slot unchanged.

This row now links the existing request; no source or generated change was made, so no rerun is claimed.

### 28. `undeclared-identifier-6e587559` — G2 recheck of 6 header(s)

- cause: text_markup retains calls to its refused parser/format helpers after the prior equality declaration
- subject: parse_markup_attributes
- lanes: ANIM, TEXT
- fix route: [existing retained-callee request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-text-markup-helper-chain-has-concrete-carriers-but-fails-before-link)
- actual first failure at: `flight/text_markup/text_markup.hpp:407`

Current Clang22.1.8 jobs1 completes **6 attempted / 0 pass / 6 fail / 0 unattempted**,388s.
All six owned headers still fail at missing parse_markup_attributes. The existing equals_markup_format
repair and body refusal remain unchanged. Current TextMarkupGuard and existing result/run owners now
fix the candidate carriers, superseding the older callback uncertainty; no new owner or API is invented.

Five scratch prototypes match existing exact marker/direct-call/no-definition witnesses, one header,
fixed point and removal restoring original bytes. Actual unused whole-header compile exits1 on
retained Record deletion/union projection. Positive unused link/run and qualified called-function
negative links are not reached; candidates rejected. No production repair, changed normal generation,
C++/runtime/source/binding/override or manifest change is carried. The six rows stay failed with
compiler request links; no other cluster or historical/G1 totals are reclassified.

The request records mandatory/full regex captures versus optional values, String Unicode iteration,
nominal effect/format ownership and Array.from mapper binding; retained href/color presence and merged
format spreads are inspected only. Preserve lenient/malformed markup, entity decoding, full custom
registries/results, href filtering/diagnostics, stack composition, distinct per-interval formats,
fieldwise overlap ordering, source equality and serializer fixed points. Source tests are requirements,
not native parser/text-layout/SWF/drawing proof. No fake parser/plain-text empty success, copied text
format owner or narrowed tag/attribute/format domain. Only this worklist entry, owned root/six compact
rows and one existing request appendix change; foreign bytes/prose/repairs/manifests remain unchanged.

### 29. `undeclared-identifier-a74215ca` — 6 header(s)

- cause: `registerShapeBoundsCommand<K extends ShapeCommandKey>` is emitted correctly as a template, but its non-generic caller `register_default_shape_bounds_commands` builds each object literal as `ShapeBoundsCommand<K>`, with the callee's type parameter instead of the inferred argument
- subject: `K`
- lanes: 2D
- fix route: compiler
- why this route: G2, measured with GCC 15.2 at `-j1`. The type argument is the checker's inference; no runtime constant or repair can supply it. It is the same defect as the `Traits` cluster `undeclared-identifier-993e82f1` (builder4), which is not changed here
- first failure at: `flight/shape/register_default_shape_bounds_commands.hpp:17`
- example: `use of undeclared identifier 'K'`
- compiler request: `docs/compiler-requests/generic-call-argument-uses-unsubstituted-parameter-type.md`

### 30. `undeclared-identifier-f5e3440f` — 6 header(s)

- cause: createResourceLoader calls initializeResourceLoader after the compiler refuses the initializer
- subject: `initialize_resource_loader`
- lanes: 3D, CORE
- fix route: compiler — [retained callee request](compiler-requests/retained-declaration-calls-omitted-callee.md)
- first failure at: `flight/loader/resource_loader.hpp:465`
- example: `use of undeclared identifier 'initialize_resource_loader'`
- G2 inspection: the exact source-line-315 NOT GENERATED marker is present (`cpp-contextual-union-inequivalent` over `const opts = options ?? {}`), the factory calls the missing symbol at line 465, and no definition exists in generated/runtime/override code. The caller supplies `EntityConstruction<Ref<ResourceLoaderInternal>>` and optional readonly `ResourceLoaderOptions` structural storage; these determine the declaration's signature, with no invented body.
- A temporary candidate applied that declaration through the existing `declare-refused-function` helper. It matched this header and reached a fixed point. The root-only native check at one worker was **0/1 pass, 1 fail, 0 unattempted**: the first missing-name diagnostic cleared, but candidate line 468 then failed returning `EntityConstruction<Ref<ResourceLoaderInternal>>` as `Ref<ResourceLoader>`.
- The required compile prerequisite for a negative-link check therefore failed. The candidate was rejected; no declaration repair, generated header, runtime, source patch, binding, or override is carried. The measured follow-on is recorded in the existing [flattened-interface owner request](compiler-requests/flattened-interface-refinement-identity.md). A native cast, copied public loader, dummy loader, or error fallback would not preserve the original owner and semantics. Merely finishing the construction still leaves unrelated `ResourceLoaderInternal` and `ResourceLoader` owner structs.
- Numbers for the delivered tree: **0/6 pass, 6 fail**, retained from G1; the stock missing-name diagnostics remain unchanged and all six rows now link the existing retained-callee request. Only the changed temporary root candidate was compiled; no full-cluster, unchanged-stock, or global rerun was performed. The rejected candidate's new diagnostic remains a failure, not a header promotion. Loader initialization and behavior remain unsupplied.

### 31. `call-has-no-matching-5a045ef4` — G2 recheck of 5 header(s)

- cause: `readSwfShapeEdgeRecord(reader, state)` passes the private `SwfShapeState` where the callee declares the anonymous `pen: { x: number; y: number }`. The emitted `Ref<SwfShapeState>` does not convert to `Ref<x_y_8365950bd60f783f>`.
- subject: `read_swf_shape_edge_record`
- lanes: ANIM
- fix route: compiler
- why this route: the callee writes `pen.x` and `pen.y`, and the caller's next record reads `state.x` and `state.y`. So the argument needs a **writable** identity-preserving view, per [structural-subtype-reference-argument-owner](compiler-requests/structural-subtype-reference-argument-owner.md). A copied pen or a write-back shim would lose or reorder those updates.
- first failure at: `flight/swf/swf_shape.hpp:241`
- example: `no matching function for call to 'read_swf_shape_edge_record'`
- Scoped native recheck (GCC 15.2, jobs 1): **1 attempted / 0 pass / 1 fail** at `swf_shape.hpp:241`, which carries the measured location. `swf_text`, `swf_font_handler`, `swf_static_text_handler` and `swf_font_tag_family` reach it through their includes. They were not recompiled, keep their recorded failure, and are given no location. Nothing is carried.
- compiler request: `docs/compiler-requests/structural-subtype-reference-argument-owner.md`

### 32. `call-has-no-matching-b897f03f` — 5 header(s)

- cause: the generic `getNodeSignals<Node<Traits>>` call is emitted with the default
  `Ref<NodeTraits>` argument because the heritage walk that would infer `Node3DTraits` from
  `Billboard`'s `Node3D` alias does not happen
- lanes: 3D
- fix route: compiler — [heritage argument generic deduction](compiler-requests/heritage-argument-generic-deduction.md)
- first failure at: `flight/scene3d/billboard.hpp:88`
- example: `no matching function for call to 'get_node_signals'`

`Billboard extends Node3D`, and `Node3D = Node<Node3DTraits> & Node3DTraits`, so TypeScript infers
`Traits = Node3DTraits`. The emitter emits
`get_node_signals<flight::Ref<flight::types::NodeTraits>>(source)`, whose parameter is
`Ref<Node<Ref<NodeTraits>>>`, and `source` is a nominal `Ref<Billboard>`, so no overload matches. The
other four headers (`_internal_index`, `billboard_camera`, `contract`, `enable_scene_3d_guards`) fail
through the same call.

No workaround is carried: a cast to `Node<Ref<NodeTraits>>` would not recover the concrete traits owner,
and a copied owner would change identity. The request's heritage walk substitutes `Node3DTraits`.

This row now links the existing request; no source or generated change was made, so no rerun is claimed.

### 33. `call-has-no-matching-dd66c064` — 10 current header(s), 5 in G1

- cause: an emitted SwfReader class instance cannot be passed as its structural SwfTagReader interface to the parse callback
- lanes: ANIM
- fix route: compiler
- first failure at: `flight/swf/swf_timeline_parse.hpp:122`
- example: `no matching function for call to object of type 'const function<bool (shared_ptr<SwfTagReader>, double, shared_ptr<SwfTagParseState>, shared_ptr<SwfTagTimelineState>, optional<Array<shared_ptr<ImportDiagnostic>>>)>'`
- compiler request: [class-instance-as-implemented-interface.md](compiler-requests/class-instance-as-implemented-interface.md)

Five owned SWF rows joined this existing cluster after the declaration repair in section 17.
The original five G1 rows remain unchanged and were not recompiled. The same mutable reader
instance must supply interface reads and writes; copying it into an interface owner would change behavior.

### 34. `compiler-crash-aba5e9ec` — 5 header(s)

- cause: the -j8 sweep ran out of room (retried serially)
- lanes: 3D
- fix route: none (sweep resource limit; passes serially)
- example: `compiler exited with status 134`

### 35. `invalid-operands-6d9a155d` — 5 header(s)

- cause: three morph-paint guards compare the whole optional token variant with a String
- lanes: 2D
- fix route: compiler — [existing token comparison request](compiler-requests/token-discriminant-comparison.md#g2-current-pin-qualification-all-three-paint-guards-use-this-carrier)
- first failure at: `flight/shape/morph_shape_paint.hpp:77`
- example: `invalid operands to binary expression ('value_type' and 'String')`

The actual source tests beginFill, lineStyle and the remaining binding.commandKey with strict
inequality. All three emitted guards need a presence check and selection of the represented String
alternative. The complete token domain still includes bool, number, numeric arrays, Matrix and all
four Texture owners. The runtime's existing three-state presence equality does not match this
heterogeneous optional variant; no broader coercion/operator, narrowed token union or body substitute
is carried. Sampling must retain the original command array and mutate its existing Matrix/gradient
buffers, while preserving the endpoint snapshots explicitly made during construction.

Delivered qualification remains **0/5 pass, 5 fail**, retained from G1 with the exact original
diagnostics. The five owned rows now link the existing request; unowned rows/causes and historical
totals remain unchanged. No production C++ input changed, so no generation, unchanged five-header
compile, global sweep or upstream test port was run and no paint execution or new native pass is claimed.

### 36. `missing-member-5e9e8a2c` — 5 header(s)

- cause: retained display/sprite handlers call `hit_test_graph_local_bounds` after the compiler refuses its generic Node-to-Node2D assertion
- subject: `hit_test_graph_local_bounds`
- lanes: PLAT
- fix route: compiler — [retained omitted callee](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-interaction-graph-local-bounds-calls-cross-the-refused-module), with [node capability recovery](compiler-requests/assertion-to-undeclared-row-member.md#g2-interaction-graph-bounds-need-the-original-nodes-2d-capability)
- first failure at: `flight/interaction/display_hit_tests.hpp:28`
- example: `no member named 'hit_test_graph_local_bounds' in namespace 'interaction'`
- G2 disposition: one omitted-callee family. `hit_tests.hpp:88` marks `hitTestGraphLocalBounds` NOT GENERATED at `cpp-structural-assertion-owner-unproven`; its source inverse-transforms the point through the same node's world matrix, then tests its actual local bounds. Real node/runtime/matrix/bounds identities, lazy provider effects and call order must survive. A false hit result, copied graph or closed Node2D-only API cannot replace that operation.
- Declaration witnesses do not coincide: the defining header has the exact marker but **zero live calls**; `display_hit_tests.hpp:28/32/36` and `sprite_hit_tests.hpp:16` have four retained `NodeAny` calls but no marker. The existing declaration-repair probe matches zero files and changes no bytes. No repair/body was added, so no negative-link result is claimed.
- Numbers: **0/5 pass, 5 fail**, retained from G1. All five owned rows preserve their diagnostics and failure status and gain the existing request links; the work-item ID remains with corrected cause/compiler route. No C++ input changed and no native gate was rerun. No Scene2D/TextLabel/Node file, new compiler request or promotion was added. Builder4 owns its active shared-node/text work; this disposition stays within Interaction and its evidence.

### 37. `no-viable-conversion-eaf1a6e0` — G2 recheck of 5 header(s)

- cause: the generic `flight::render::noop_renderer_data(Ref<RenderState>, NodeAny)` is stored in `SpriteRenderer.create_data`, whose `std::function` passes a flattened `Ref<Sprite>` that does not widen to `NodeAny`. TypeScript accepts it because a `Sprite` is a `Node`.
- subject: `noop_renderer_data`
- lanes: 2D
- fix route: compiler
- why this route: the carriers need the same-owner `Sprite` to `NodeAny` widening of [interface-heritage-widening](compiler-requests/interface-heritage-widening.md). This is the Canvas form of its GL bitmap text and QuadBatch create-data sections. An adapter that rebuilds the node, a function-pointer cast or a narrowed domain would not preserve the owner.
- first failure at: `flight/scene2d_canvas/canvas_bitmap_text.hpp:116`
- example: `no viable conversion from 'optional<Ref<RendererData>> (Ref<RenderState>, NodeAny)' ') to 'function<optional<Ref<RendererData>> (Ref<RenderState>, Ref<Sprite>)>' >')`
- Scoped native recheck (GCC 15.2, jobs 1): **2 attempted / 0 pass / 2 fail** at the roots, `canvas_bitmap_text.hpp:116` and `canvas_quad_batch.hpp:159`, which carry measured locations. `scene_2d_canvas_pipeline`, `contract` and `_internal_index` reach them through their includes. They were not recompiled, keep their recorded failure, and are given no location. Nothing is carried.
- compiler request: `docs/compiler-requests/interface-heritage-widening.md`

### 38. `no-viable-overload-658901de` — 5 G1 header(s), 4 now compile

This G1 group combined assignment failures at distinct storage boundaries. The material preset
override assigned `Array<Ref<PbrExtension>>` to the current `Array<PbrExtensionValue>` field.
The active source patch already supplies the concrete transmission alternative. Glass now places its
original `Ref<TransmissionVolumePbrExtension>` directly in that alternative, removing the obsolete
prefix reinterpretation without a copied owner or a permissive runtime Array conversion.

The Canvas source-mode compositor assigns an emitted
`variant<CanvasGradient, CanvasPattern, String>` to `CanvasStyle`. Each alternative was already a
valid native style, but the union was not accepted. The runtime now constructs and assigns variants
containing only those three exact paint types, in any order or subset, through the existing per-arm
operations. Gradient and pattern handles retain their live owners, assignment returns the same
style, and an absent optional still leaves the existing style unchanged. Numeric and Any arms
remain rejected.

**G2: 4/5 compile, 1 fails, 0 unattempted**, all attempted at `-j1` with Clang 22.1.8,
compiler `806d257` / Flight `a627849`. Passing:

- `flight/effects_canvas/canvas_source_mode_compositing.hpp`
- `flight/materials/_internal_index.hpp`
- `flight/materials/contract.hpp`
- `flight/materials/material_presets.hpp`

The materials semantic build/CTest passes (1/1), including preset defaults, nested overrides,
ordered extension storage, concrete descriptor fields, shared control-block identity, alias mutation
and fresh owners for independent glass results. The runtime build/CTest also passes (1/1), including
all paint alternatives, live gradient/pattern mutations, reordered/subset variants, optional absence
and rejection of erased/numeric arms. Override integrity passes, and the preset generated-source
hash remains current. No unchanged materials test or whole-SDK sweep was rerun after the Canvas edit.

`flight/render/render_queue.hpp` remains failed at `:131`: a child from
`NodeOf<Ref<NodeTraits>>` is assigned to the nominal `NodeAny` scratch stack. The source current
node comes from `NodeAny[]`, but its emitted runtime accessor selects the default NodeTraits
owner. This is now active cluster `no-viable-overload-287a3cf5`, covered by the extended
[node-any-generic-traits-leak.md](compiler-requests/node-any-generic-traits-leak.md) request.
No replacement node, unrestricted structural-to-nominal assignment, source patch or queue body
is supplied; lookup identity, traversal order, pruning and scratch reuse must be preserved.

The preset override is explicitly partial: its pre-existing optional nominal glass-options entry
still needs the canonical readonly structural entry adaptation. A passing header does not restore
the ten refused standard-preset emissions; see
[named-interface-partial-shape.md](compiler-requests/named-interface-partial-shape.md).
Only these five product rows and their replacement cause changed. The opening G1 sweep totals and
rank remain historical, and `previousCluster` retains this original group.

### 39. `call-has-no-matching-4ce2cc01` — G2 recheck of 4 header(s)

- cause: `spineJsonAnimationsHandler` asserts each animation entry `as Record<string, unknown>`. The emitter lowers that to the aliasing `NamedProperties` view, but lowers the `SpineJsonTimelineHandler` parameter `Readonly<Record<string, unknown>>` to an owning `Record<String, Any>`, so `handler.value()(timeline_context, name, anim)` matches nothing.
- subject: `SpineJsonTimelineHandler`
- lanes: ANIM
- fix route: compiler
- why this route: this is the call-argument form of [named-view-record-ternary](compiler-requests/named-view-record-ternary.md). The parameter must take the same view. Materializing a `Record` would copy every property and break the alias the handlers read through.
- first failure at: `flight/skeleton2d_formats/spine_json_animations_handler.hpp:66`
- example: `no matching function for call to object of type 'function<void (shared_ptr<SpineJsonTimelineContext>, String, Record<String, Any>)>'`
- Scoped native recheck (GCC 15.2, jobs 1): **1 attempted / 0 pass / 1 fail** at `spine_json_animations_handler.hpp:66`, which carries the measured location. `spine_json_section_handlers`, `spine_json_handlers` and `spine_json_full` reach it through their includes. They were not recompiled, keep their recorded failure, and are given no location. Nothing is carried.
- compiler request: `docs/compiler-requests/named-view-record-ternary.md`

### 40. `call-has-no-matching-82244b93` — 4 header(s)

- cause: the emitted call to `get_or_create_render_proxy_2d` matches no declared overload
- subject: `get_or_create_render_proxy_2d`
- lanes: 2D
- fix route: compiler
- first failure at: `flight/scene2d_dom/dom_cssfilter_binding.hpp:39`
- example: `no matching function for call to 'get_or_create_render_proxy_2d'`

### 41. `call-has-no-matching-9248dccf` — 4 header(s)

- cause: the emitted call to `get_node_world_matrix4` matches no declared overload
- subject: `get_node_world_matrix4`
- lanes: ANIM
- fix route: compiler
- first failure at: `flight/particleemitter/update_particle_emitter_3d.hpp:112`
- example: `no matching function for call to 'get_node_world_matrix4'`

### 42. `call-has-no-matching-9ebeb95c` — 4 header(s)

- cause: the emitted call to `get_transform_2d_runtime` matches no declared overload
- subject: `get_transform_2d_runtime`
- lanes: ANIM, GL
- fix route: compiler
- first failure at: `flight/node/node_transform2d.hpp:251`
- example: `no matching function for call to 'get_transform_2d_runtime'`

### 43. `call-has-no-matching-c3aea71c` — 4 header(s)

- cause: the emitted call to `structural_ref_cast` matches no declared overload
- subject: `structural_ref_cast`
- lanes: 2D, GL
- fix route: compiler
- first failure at: `flight/scene2d/scale9_sprite.hpp:162`
- example: `no matching function for call to 'structural_ref_cast'`

### 44. `no-viable-conversion-5903ce63` — 4 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 3D, GL
- fix route: compiler
- first failure at: `flight/shading/register_built_in_modifiers.hpp:63`
- example: `no viable conversion from 'SignatureFunction' >') to 'optional<function<String (ReadonlyModifierValue)>>' >>')`

### 45. `no-viable-conversion-e1a24bd1` — 4 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 2D
- fix route: compiler
- first failure at: `flight/scene2d_formats/rive_document.hpp:133`
- example: `no viable conversion from 'optional<Record<[...], double>>' to 'optional<Record<[...], variant<bool, double, String>>>'`

### 46. `other-2aef05a3` — 6 current header(s), 4 in G1

- cause: shadow-effect conditional branches cannot widen their independent concrete owners into the declared Effect array element
- lanes: ANIM
- fix route: compiler — [interface-heritage owner request](compiler-requests/interface-heritage-widening.md)
- first failure at: `flight/swf/swf_filter.hpp:257`
- example: `incompatible operand types ('Ref<InnerShadowEffect>' and 'Ref<DropShadowEffect>')`
- G2 classification: one owner-widening family. `outEffects` is declared `Effect[]` and emits `Array<Ref<Effect>>`; `Effect` is an open base contract, not a closed union of known effects. The canonical concrete and base effect structs independently derive from `ReferenceEnabled`, with no effective type override providing the declared TypeScript heritage.
- A clearly equivalent first-shadow branch source patch was tested through normal focused generation of `swfFilter.ts`, preserving the selected original factory, options, single append, and exact returned owner. The emitted direct `push(create_inner_shadow_effect(...))` at candidate line 288 still requires a concrete-to-base owner conversion. An unused runtime helper cannot change that C++ pointer relationship; a cast, copied base descriptor, or closed effect union would change identity or the public contract.
- The temporary patch and manifest entry were withdrawn, the manifest restored byte-for-byte, and both pinned checkouts cleaned by normal generation. No source patch, generated header, runtime, repair, binding, or override is carried. Builder5's Shape/read-only work is in separate files and was not touched.
- Delivered numbers: **0/4 pass, 4 fail**, retained from G1, with the stock conditional diagnostics unchanged and every owned row linked to the existing request. No unchanged-stock or global rerun is claimed.
- Changed candidate result: **0/4 pass, 4 fail, 0 unattempted**, Clang 22.1.8 at one worker. All four first errors are `no matching member function for call to 'push'` at the direct inner-shadow push, candidate `swf_filter.hpp:288`. The source branch patch was rejected; this changed diagnostic remains a failure and is not written over the shipped tree's stock diagnostics.
- The separate adjustment-heritage repair in work item 49 exposes this same stock conditional diagnostic in `swf_place_object3_handler.hpp` and `swf_placement_tag_family.hpp`. These two newly measured failures join this cause, making six current rows. The original four rows and their G1 qualification remain unchanged; they were not recompiled in the four-header adjustment check.

### 47. `other-43523429` — 4 header(s)

- cause: heritage-based Node2D type arguments are not substituted at `addNodeChild`, leaving a free `Traits` identifier
- lanes: ANIM
- fix route: compiler — [heritage type-argument deduction request](compiler-requests/heritage-argument-generic-deduction.md)
- first failure at: `flight/movieclip/spritesheet_timeline_source.hpp:123`
- example: `use of undeclared identifier 'Traits'`
- G2 native fix: the original expected-zero/have-one diagnostic at line 122 was `create_sprite(std::nullopt)` emitted for the source's `createSprite()`, not a callback dropping legitimate arguments. The existing native Sprite completion now accepts exactly `std::nullopt_t` and forwards to its no-options constructor. Present options and arbitrary argument types are rejected; the override remains partial and its manifest records the supported absence-only boundary.
- Verification: the project native owner target built and `flight_cpp.generated_scene2d_gl_owners` passed **1/1**, checking defaults/texture absence, fresh data/runtime identities, retained graph cells, and compile-time rejection of supplied option rows and integers. Override integrity passed **226/226**.
- Changed-input scoped native result: **0/4 pass, 4 fail, 0 unattempted**, one worker. All four first errors now reach the same `Traits` leak at line 123; the prior arity error is removed, and the new compiler diagnostic remains a failure. Every owned row has the actual new diagnostic/location and existing request link.
- These four Movieclip rows retain their work-item ID for continuity. The separately reserved 29-header `Traits`/TextLabel/E0a cluster and all unowned rows remain unchanged. No compiler source patch, package-wide check, global header sweep, or upstream test port was performed; Movieclip playback is not claimed working.

### 48. `other-4e22e4de` — 4 header(s)

- cause: `JSON.parse(json) as AsepriteDocument` is emitted as a `static_cast` between unrelated carriers
- subject: `AsepriteDocument`
- lanes: 2D
- fix route: compiler — [existing JSON assertion request](compiler-requests/json-parse-assertion-static-cast.md#g2-aseprite-document-union)
- first failure at: `flight/spritesheet_formats/aseprite_parse.hpp:144`
- example: `no matching conversion for static_cast from 'JsonValue' to 'AsepriteDocument'`
- G2 disposition: **one shared JSON-assertion cause**, matching the existing Lottie request, with a two-reference document variant here. `Json::parse` already returns a real JSON tree; it cannot be cast into `variant<Ref<AsepriteArrayDocument>, Ref<AsepriteHashDocument>>`. Both public Aseprite parse entry points contain that cast.
- The document-returning API retains the original array/hash document and layer metadata, while spritesheet mapping also consumes durations and tags. The existing native atlas parser returns geometry/page metadata and omits those values, so substituting it would lose the parser's required result. Materializing the full document or refusing the assertion belongs to the compiler; a permissive conversion, empty document, fake parser or added schema-failure fallback cannot preserve the source contract.
- `documentToData` is independently omitted at an atlas-schema contextual-union conversion, so no claim is made that removing this cast alone makes the parser work. A declaration repair cannot convert the JSON tree into the document owners.
- Numbers: **0/4 pass, 4 fail**, retained from G1. All four owned rows keep their exact diagnostic and gain the actual location and existing request link; the original work-item ID is retained with the corrected cause/route. No C++ input changed, no unchanged native rerun and no promotion. Other clusters, the Lottie rows and historical sweep totals are unchanged.

### 49. `other-5611a858` — 4 header(s)

- cause: ColorScaleBiasAdjustment was emitted independently of its declared ColorMatrixAdjustment base, preventing the real concrete factory owner from entering the open Adjustment array
- lanes: ANIM
- fix route: declaration repair applied; remaining failures use existing compiler requests
- original first failure at: `flight/swf/swf_appearance.hpp:123`
- original example: `no matching constructor for initialization of 'Array<Ref<Adjustment>>'`

The existing ColorMatrixAdjustment and TintAdjustment repairs already retain declared heritage. The
new `adjustments-color-scale-bias-interface-heritage` repair completes that same chain, replacing only
the duplicated base declarations with `ColorScaleBiasAdjustment : ColorMatrixAdjustment`. No body,
copied owner, pointer cast or closed Adjustment union is supplied. The real factory's single allocation
now widens through the existing matrix base to Adjustment, retaining its inherited cells and lifetime.
See the [existing heritage request](compiler-requests/interface-heritage-widening.md#g2-swf-color-transform-arrays-complete-the-existing-declared-heritage-chain).

The existing native adjustments target built and `flight_cpp.generated_adjustments` passed **1/1**,
covering the actual factory, array order with a vendor kind, shared control block, same inherited field
cells, matrix writes and final owner release. The exact repair match, fixed point and manifest entry
were also checked.

Changed-input scoped native result: **1/4 pass, 3 fail, 0 unattempted**, Clang 22.1.8 at one worker.
`swf_appearance.hpp` passes. The remaining rows retain the actual first diagnostics:

| follow-on cluster | owned headers | first failure | route / cause |
| --- | --- | --- | --- |
| `other-2aef05a3` | 2 | `flight/swf/swf_filter.hpp:257` | compiler: incompatible InnerShadowEffect/DropShadowEffect owners; [existing heritage request](compiler-requests/interface-heritage-widening.md) |
| `undeclared-identifier-1e1a12b0` | 1 | `flight/swf/swf_place_object_handler.hpp:106` | compiler: retained `read_legacy_place_object` call after its body was refused; [existing omitted-callee request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-swf-legacy-placement-follow-on-after-adjustment-heritage) |

The two filter failures are `swf_place_object3_handler.hpp` and `swf_placement_tag_family.hpp`;
`swf_place_object_handler.hpp` reports the undeclared legacy helper. These changed diagnostics remain
failures. Only these four header rows and their affected causes were updated; the G1 sweep totals and
all unowned rows remain unchanged. This does not make legacy placement or filter parsing available,
and it does not resolve general reverse-direction interface owner recovery. No global sweep or
upstream test port was performed.

### 50. `other-d6be4352` — 4 header(s)

- cause: a JSON.parse assertion to PixiRaw is emitted as a static_cast from JsonValue to Record<String, Any>
- lanes: ANIM
- fix route: compiler — [existing JSON assertion request](compiler-requests/json-parse-assertion-static-cast.md#g2-pixi-particle-parser-the-destination-is-a-dynamic-record)
- first failure at: `flight/particles_formats/pixi_parse.hpp:55`
- example: `no matching conversion for static_cast from 'JsonValue' to 'PixiRaw'`

`parsePixiJson` already validates a non-null, non-array object. PixiRaw has no mandatory config fields;
`{}` must reach the finite defaults, while supplied nested bounds/pairs/colors/spawn shapes affect
the actual config and ordered import diagnostics. The same parsed object feeds both consumers.
The [existing parsed-object shape request](compiler-requests/parsed-json-object-shape-reads.md#g2-current-pin-pixi-inventory-qualification)
already records the nested anonymous-struct recovery and missing-key `get().value()` barriers. Making
the outer cast compile alone would leave those behaviors broken; no permissive conversion, defaults-only
parser or full parser fork is supplied.

Delivered qualification remains **0/4 pass, 4 fail**, retained from G1 with the exact diagnostics
unchanged and both existing requests linked in the product. Only the four owned rows and this cause
were updated; all other rows and historical totals remain unchanged. No C++ input changed, so no
native recompile or global sweep was run and no Pixi parity or upstream test port is claimed.

### 51. `static-assertion-00a0e75d` — 4 header(s)

- cause: a runtime static assertion rejects the emitted shape
- lanes: 3D, WGPU
- fix route: compiler
- first failure at: `flight/structural_ref.hpp:1421`
- example: `static assertion failed due to requirement '!is_void_v<void>': structural row schema does not contain the requested property`

### 52. `undeclared-identifier-0fc9e20f` — 4 header(s), declaration repaired

- original cause: retained code calls `initialize_rive_artboard_import_context` after its body was
  refused at the projected-sequence storage boundary
- lanes: 2D
- fix route: declaration-only emission repair, with projected-sequence debt still open
- compiler request: [readonly-sequence-owning-array.md](compiler-requests/readonly-sequence-owning-array.md)

The exact refusal marker, the single retained call and the absence of any definition justify
`scene2d-formats-declare-refused-rive-artboard-import-context`. It declares the seven carriers the
retained call constructs, including the `SequenceView` the caller already holds. No construction body,
field initialisation, owning array or recovered sequence storage is supplied; calls remain unresolved
links.

Written by **normal package generation** at the exact pins (`flight` a627849 / `flight-compiler`
806d257, rehydrated without `--update`): the pre-write check reported exactly
`rive_import_registry.hpp` and `manifest.json` as the differing paths, and the write produced only
those two.

G2: **0/4 compile, 4 fail, 0 unattempted** at `-j1` with Clang 22.1.8. The artboard callee resolves in
`rive_import_registry.hpp` and the three headers that include it; all four now fail one callee further
on `initialize_rive_document_import_context`.

| follow-on cluster | owned headers | first failure | route / cause |
| --- | --- | --- | --- |
| `undeclared-identifier-6bcf3171` | 4 | `flight/scene2d_formats/rive_import_registry.hpp:169` | compiler: the document import context is refused at the same projected-sequence storage boundary; its retained delegating caller is beside it in the same file; [existing request](compiler-requests/readonly-sequence-owning-array.md) |

### 53. `undeclared-identifier-69b85faa` — 4 header(s), declaration repaired

- original cause: retained code calls `untrack_midi_subscription` after its function body was refused
  at the optional `ownerSubscriptions?.delete` call
- lanes: PLAT
- fix route: declaration-only emission repair, with implementation debt still open
- compiler request: [optional-property-call-result-evidence.md](compiler-requests/optional-property-call-result-evidence.md)

The exact refusal marker, the three retained calls and the absence of any definition justify
`midi-declare-refused-untrack-subscription`. It declares the emitted
`flight::Ref<flight::types::Entity>` and `flight::Ref<MidiSubscriptionRuntime>` carriers before the
first caller. No body, no `Set` mutation and no null write are supplied.

G2: **0/4 compile, 4 fail, 0 unattempted**, all attempted at `-j1` with Clang 22.1.8, compiler
`806d257` / Flight `a627849`. With the declaration, `midi_subscription.hpp:164` resolves in
`_internal_index.hpp`, `contract.hpp`, `midi_access.hpp` and `midi_subscription.hpp`; all four now fail
one callee further, on `detach_midi_subscription`. Each of those headers also carries the retained
callers, so the follow-on is the same omission class rather than a new defect.

| follow-on cluster | owned headers | first failure | route / cause |
| --- | --- | --- | --- |
| `undeclared-identifier-f2f09e07` | 4 | `flight/midi/midi_subscription.hpp:198` | compiler: `detachMidiSubscription` refused at an unrepresented `Task<MidiSubscriptionDetachOutcome>` domain; its three delegators pass concrete subscription refs where `Entity` is declared and the emitted structs inherit only `ReferenceEnabled`; [existing request](compiler-requests/task-of-union-domain-unrepresented.md) |

### 58. `undeclared-identifier-7d64d9c1` — 4 header(s)

- cause: the emitter refuses the `glCustomShaderEffectRunner` binding and its resolver but still emits the registration call that passes both
- subject: `gl_custom_shader_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: a runtime definition would fake the effect runner; the refusal names an owner-preserving carrier as the compiler-side answer
- first failure at: `flight/effects_gl/gl_custom_shader_effect.hpp:183`
- example: `use of undeclared identifier 'gl_custom_shader_effect_runner'`
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 55. `undeclared-identifier-862d162a` — 4 header(s)

- cause: buildGltfAnimations is refused, but the globally initialized handler retains an undeclared call
- subject: `build_gltf_animations`
- lanes: 3D
- fix route: compiler — [existing retained-callee request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-gltf-animations-a-declaration-fails-the-unused-header-link-prerequisite)
- first failure at: `flight/scene3d_formats/gltf_animations.hpp:209`
- example: `use of undeclared identifier 'build_gltf_animations'`

All three declaration witnesses hold: exact builder refusal marker, retained direct call and absent
definition. A temporary one-declaration overlay compiled the actual unused header to an object, but
its required positive link failed on exactly the builder symbol: the global handler initializer
retains the apply lambda even when main never uses it. The candidate was rejected; no repair entry,
generated C++, manifest change or substitute animation body is carried. This failed positive-link
prerequisite is not a successful negative-link check.

The builder's refusal is `cpp-member-projection-without-present-storage` for an unidentified `length`
receiver. The separately refused weights helper is covered by the
[existing contextual-union evidence request](compiler-requests/compiler-contextual-imported-union-owner.md).
Real animation channels retain track owners and the accessor-backed times/values; a faithful fix must
preserve their order, diagnostics and morph-node fan-out rather than return an empty list or copy owners.

Delivered qualification remains **0/4 pass, 4 fail**, retained from G1 with the exact diagnostics
unchanged. Only the four owned rows and this cause were updated; unowned rows and historical totals
remain unchanged. With no production C++ change, no unchanged four-header recompile or global sweep
was run and no animation execution or upstream test coverage is claimed.

### 56. `undeclared-identifier-f2b46b01` — 4 header(s)

- cause: the standard and unlit material texture listers are refused, but retained registrations pass their undeclared callbacks
- subject: `list_standard_pbr_material_textures`
- lanes: 3D
- fix route: declaration repairs applied; remaining failures use an existing compiler request
- original first failure at: `flight/scene3d_resources/scene_material_texture_registry.hpp:162`
- original example: `use of undeclared identifier 'list_standard_pbr_material_textures'`

Two guarded declare-refused-function repairs supply only the exact Scene3DMaterialTextureLister
signatures for the standard PBR and unlit callbacks. The optional exact registration-line witness
handles a function passed as a callback without matching bare symbol mentions. Both source markers,
retained references and absent definitions agree. Normal focused generation at the locks writes
exactly two declarations; three owner-recovery refusals and the partial module counters remain.
See the [retained-callback request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-scene3d-material-texture-listers-are-retained-callback-values).

Repair guards: **26 tests pass, zero fail, one generic C++ fixture skipped** because the default
compiler is absent from PATH. Actual-header native checks separately pass: unused include compiles,
links and runs; both retained registrations and exact callback carriers compile to an object; linking
that object fails on exactly the two refused lister symbols. No list implementation, owner downcast,
copied texture reference or closed extension family is supplied. The underlying
[Material owner-recovery request](compiler-requests/assertion-to-undeclared-row-member.md#g2-scene3d-material-texture-listers-retain-only-the-base-material-row)
stays open for standard, unlit and extended material callbacks.

Changed-input exact four-header check: **1/4 pass, 3 fail, 0 unattempted**, Clang 22.1.8 at one worker.
`scene_material_texture_registry.hpp` passes. `_internal_index.hpp`, `contract.hpp` and
`explain_scene_3d_resource_coverage.hpp` now fail at the latter header's line 57: a designated initializer
targets SceneCoverageEntry's outer variant. These three rows join existing `other-86d0b50b`, with
actual diagnostics/locations and the [existing outcome-construction request](compiler-requests/object-literal-into-outcome-variant.md#g2-scene3d-resource-coverage-returns-a-literal-into-the-outer-outcome-variant).
All three changed diagnostics remain failures. Only the four owned rows and affected causes changed;
unowned rows and historical sweep totals remain unchanged. No global or unchanged rerun or upstream
test port was performed. Header inclusion does not make the refused registrations executable.

### 57. `call-has-no-matching-3b2f72dd` — 3 header(s)

- cause: the emitted call to `is_optional_number` matches no declared overload
- subject: `is_optional_number`
- lanes: PLAT
- fix route: compiler
- first failure at: `flight/layout/anchor_layout.hpp:76`
- example: `no matching function for call to 'is_optional_number'`

### 58. `call-has-no-matching-56e47749` — 3 header(s)

- cause: the emitted call to `compute_node_root_local_bounds_rectangle` matches no declared overload
- subject: `compute_node_root_local_bounds_rectangle`
- lanes: GL
- fix route: compiler
- first failure at: `flight/effects/effect_capture_geometry.hpp:68`
- example: `no matching function for call to 'compute_node_root_local_bounds_rectangle'`
- existing request linked: [generic structural parameter given a narrower row](compiler-requests/generic-structural-parameter-given-a-narrower-row.md) (re-measured at `806d257`)

`effectCaptureGeometry.ts` passes its `Node2D` to `computeNodeRootLocalBoundsRectangle<Traits>(out, root:
Spatial2DNode<Traits>)`. TypeScript infers `Traits = Node2DTraits` and projects the same owner. In the
emission, `Node2D` is a `RowOf` row and `Spatial2DNode<Traits>` is a `RowMerge` row, so clang reports
`could not match 'flight::RowMerge' against 'flight::RowOf'` and `Traits` is never deduced. The existing
request already measured this exact site; it has been re-measured here. There is no faithful local fix: a
cast, a copied owner or a substitute bounds computation would each break the identity-preserving
projection. `contract.hpp` and `_internal_index.hpp` fail by including this header.

**G1 status retained: 0/3 pass, 3 fail.** No code changed, so no recompile was run.

### 59. `invalid-operands-98cd7094` — 3 header(s)

- cause: `enableGlColorAdjustmentGuards` tests idempotence with
  `runtime.registries.colorAdjustmentFeatureGuard !== warnGlColorAdjustmentNotEnabled`; the slot emits as
  `std::variant<std::function<void (Ref<GlRenderState>, variant<Array<double>, Ref<ColorScaleBias>,
  Ref<TintMaterialData>>)>, Null, Undefined>` and the test as `!=` against the `void ()` function
- lanes: 2D
- fix route: compiler — [local handler callable identity](compiler-requests/local-handler-callable-identity.md)
- first failure at: `flight/scene2d_gl/enable_gl_color_adjustment_guards.hpp:35`
- example: `invalid operands to binary expression ('variant<function<void (Ref<GlRenderState>, variant<Array<double>, Ref<ColorScaleBias>, Ref<TintMaterialData>>)>, Null, Undefined>' >, Null, Undefined>') and 'void ()')`

This is the render cluster's identity family, not the readonly-owner recovery one: the module function has
one JavaScript identity, the assignment adapts its zero parameters to the guard's two, and the comparison
must be `callable_identity_equal` over the present alternative, false for both `Null` and `Undefined`. A
runtime operator over the stored value cannot recover the identity the adapter discarded, and comparing
carriers would invent an equality JavaScript does not have. The `assets-acquire-guard-callable-identity`
respell is only a storage-type change and does not supply the comparison. `contract.hpp` and
`_internal_index.hpp` fail through the same include.

Rechecked at `-j1` with Clang 22.1.8: **0/3 pass, 3 fail**, all at the same diagnostic. The route and
request link are recorded; no fix is carried.

### 60. `missing-member-95ead583` — 3 header(s)

- cause: the GL cube-face revision loop unwraps the nullable face but reads version on its seven-owner TextureSourceValue variant instead of visiting the active owner
- subject: `version`
- lanes: 3D
- fix route: compiler — [existing cube-texture member-projection request](compiler-requests/cube-texture-width-and-completeness.md#g2-gl-environment-face-revisions-use-the-same-selected-owner-read)
- first failure at: `flight/scene3d_gl/gl_environment_cube.hpp:56`
- example: `no member named 'version' in 'variant<shared_ptr<VoxelGrid>, shared_ptr<TextureSource>, shared_ptr<ExternalTexture>, shared_ptr<Bitmap>, shared_ptr<CompressedImageResource>, shared_ptr<RenderTarget>, shared_ptr<ImageResource>>'`

All seven actual source arms already declare `double version`; the open TextureSource arm remains
part of the contract. The pinned source uses `source?.version ?? -1`. Its existing source patch expands
that into an ordered loop with a null check, which the C++ retains, but `source.value()` remains the
complete variant. The same source owner's field must be read through a visit. Adding a runtime field
or a host overload cannot make this emitted variant member access valid. Narrowing every face to
Bitmap/ImageResource or copying a common base would change the source domain or identity.

A bounded experiment changed only the existing loop's ternary into explicit null/else branches,
preserving one append per face, the null fallback, order and owners. Normal focused generation at the
exact locks emits **1/1 module with zero refusals**, but the else branch still reads
`source.value().version` directly at candidate line 60. The candidate is measured through exactly
these three owned headers, with one C++ worker: **0/3 pass, 3 fail, 0 unattempted**. All three first
errors are the same missing `version` member at candidate line 60; generation acceptance is not a
header pass.

The source edit was rejected and the original patch restored byte-for-byte. No generated header,
runtime, binding, repair, override or source change is carried. The three shipped rows retain their
**0/3 pass, 3 fail** G1 qualification and original line-56 diagnostics, with the exact source/owner
analysis and existing request link added. Only those rows and this cause changed; unowned rows and
historical sweep totals remain unchanged. No unchanged-stock or global rerun, duplicate request or
upstream test port was performed.

### 61. `missing-member-b22d5c52` — 3 header(s)

- cause: the emitted code reads a member the target does not declare (`resolve_canvas_texture_window_source`)
- subject: `resolve_canvas_texture_window_source`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_canvas/canvas_fill_pattern.hpp:77`
- example: `no member named 'resolve_canvas_texture_window_source' in namespace 'scene2d_canvas'`
- fix route (G2): declaration-only emission repair, with the function's behavior still owed

`resolveCanvasTextureWindowSource` is refused with `cpp-contextual-union-value-type-unrepresented`:
`HostImageSource` is not a represented runtime domain. Its three callers survive in other scene2d-canvas
modules, each of which includes its header: `createBitmapPattern`, the Scale9Sprite draw, and the bitmap
fill command. The function cannot be supplied cheaply. It materializes uv windows onto per-render-state
Canvas surfaces with a version-keyed cache, so anything less than the real thing would be a fake Canvas,
provider or empty image.

`declare-refused-function` gains an optional `calledFrom` list for this split case, with the same three
witnesses held across the tree:
- `appliesTo` names the refused module's exact header, which must carry the exact NOT GENERATED marker and
  define nothing;
- every `calledFrom` header must be in the emitted set, call the symbol outside comments, and define
  nothing.

The single declaration goes immediately before the refused function's marker. `scripts/emissionRepairs.test.mjs`
covers the insertion, idempotence, a missing, non-calling or defining caller, and the exact-header load
rule (28 tests pass).

`scene2d-canvas-declare-refused-texture-window-source` declares
`std::optional<flight::types::HostImageSource> resolve_canvas_texture_window_source(flight::Ref<flight::types::CanvasTextureResolvers> resolvers, flight::types::Texture texture);`.
The parameters are the carriers the retained calls pass, matching `createBitmapPattern`'s own emitted
`CanvasTextureResolvers`/`Readonly<Texture>` parameters. The result is the carrier the bitmap fill command
stores it into (`CanvasShapeDrawState.bitmap_src`). It was applied with `applyEmissionRepairs` to the
committed emission, adding the declaration to `canvas_texture_window_source.hpp` and its manifest record, so
no scoped-generation anonymous-struct drift came with it.

That header still compiles. A translation unit that calls the function compiles and then fails to link
on exactly `flight::scene2d_canvas::resolve_canvas_texture_window_source`, so texture-window resolution
stays visibly unusable rather than substituted.

**G2: 0/3 compile, 3 fail** (`-j1`, Clang 22.1.8). All three now get past the window source and fail
first at `canvas_fill_pattern.hpp:148` on `acquire_canvas_texture_resolver_surface`. They move to
`missing-member-f497840c`. That callee is in `canvasTextureResolver.ts`, which is **wholly** refused
(`cpp-runtime-external-symbol-binding-incomplete`: `HostImageSource` has no type or value binding), and two
sibling resolver modules are refused as dependencies of it. The missing `HostImageSource` external binding
is therefore the shared root of this family. It is a binding-route fix, and broader than this cluster.

#### Follow-on `missing-member-f497840c`: not a binding gap, a misplaced patch line

Bounded investigation showed the `HostImageSource` refusal is not a missing binding.
- `HostImageSource` is Flight's alias of `CanvasImageSource`. The SDL image binding already maps
  `CanvasImageSource` to `flight::host_sdl::ImageSource`, and the types tree emits
  `using HostImageSource = flight::host_sdl::ImageSource;`.
- `canvasTextureResolver.ts` is the only module in the inventory that asks for `HostImageSource` as an
  external symbol.
- The cause is `source-patches/canvas-texture-resolver-host-image-result.patch`. Its first hunk was written
  against the file after `canvas-texture-resolver-exact-2d-source.patch` deletes the texture import, and it
  placed `  HostImageSource,` one line early, before `import type {`. Applied in manifest order, the
  pinned checkout therefore compiled a stray `HostImageSource,` expression statement. That statement is
  exactly the refusal's `missing: [value], HostImageSource[type], HostImageSource[value]`. Applying the
  patches in a scratch tree reproduced the line, and it was also visible in the live patched checkout.

G2 corrects the hunk so `HostImageSource` sits inside the type import, between `HostCanvasCapability` and
`RenderState`. A type import is erased, so the JavaScript is unchanged, and the patch's stated intent is
unchanged too. Every scene2d-canvas and types patch still applies cleanly in manifest order.

Scoped `pkg:generate @flighthq/scene2d-canvas` then emits `canvasTextureResolver.ts` in full: 33/51
modules instead of 32, 18 refusals instead of 19, and no NOT GENERATED marker in
`canvas_texture_resolver.hpp`. Its `resolve_canvas_texture` emits as
`std::optional<flight::types::HostImageSource>(flight::Ref<CanvasTextureResolvers>, flight::types::Texture)`,
the same carrier the window-source declaration above already uses.
- `resolveCanvasTextureWindowSource` stays refused, now for an honest reason
  (`cpp-union-runtime-domains-erased`: six DOM image domains share one `ImageSource` carrier). The
  `calledFrom` declaration still applies, which proves the repair under a normal package-scoped
  generation as well as through the applier.
- `flight/sdk/structural_members.hpp` gains the `registryMiss` cells the newly emitted module reads.
- `scene_2d_canvas_pipeline.hpp` takes the scoped run's closure-dependent anonymous-struct ordering. That
  struct is referenced only inside its own header, and no override shadows scene2d-canvas.

**G2: 0/3 compile, 3 fail.** All three get past `acquire_canvas_texture_resolver_surface`, which now exists.
They fail first in the newly included `canvas_render_state.hpp:69` on `get_canvas_render_state_runtime`,
the existing `missing-member-b1a6d5cf` (re-export before declaration, which already has a compiler request).
They move there, and that cluster's count goes from 34 to 38.

The fourth row is `canvas_texture_resolver.hpp` itself. It was recorded as passing only because its
best-effort placeholder carried no code. It now emits for real, includes `canvas_render_state.hpp` and fails
there, so it moves from pass to fail (`previousStatus: pass`). An include walk finds no passing header
that reaches it, so no other row regresses.

### 62. `missing-member-c36ea2d4` — 3 header(s)

- cause: the bound SDL DOM shell lacks a shared CSS style owner for the web renderer's opacity and related writes
- subject: `opacity`
- lanes: 2D
- fix route: binding
- first failure at: `flight/scene2d_dom/dom_style.hpp:33`
- example: `no member named 'opacity' in 'host_sdl::DomStyle'`
- request: [dom-style-shared-element-boundary.md](compiler-requests/dom-style-shared-element-boundary.md)

The pinned source writes a String opacity or the empty string, then smoothing/filter/blend styles
on the existing HTMLElement. The bound `HtmlElement` is actually a value `DomElement` with an
inline `DomStyle` of ordinary String fields. Even the existing transform helper receives that
carrier by value, so its style writes affect a copied shell value. Adding one numeric/default
property, or all missing fields without a shared owner, would hide the boundary.

`scene2d-dom/package.json` and the generated manifest explicitly declare environment `web`;
the native SDL profile has no applicable web environment. This existing applicability rule is
unchanged and does not remove these headers from the compile inventory. The SDL application
shell is documented as a DOM value facade, not a retained HTML tree/style renderer.

**G1 status retained: 0/3 pass, 3 fail.** No C++ input changed and no unchanged header or native
test was recompiled. Only the three owned rows gained location/request links, and the cluster
route/cause now records the actual host binding boundary. No DOM renderer, copied element,
permissive assignment or always-default style behavior was supplied. Text layout/GL work is untouched.

### 63. `no-viable-conversion-4ca7626f` — 3 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: ANIM
- fix route: compiler
- first failure at: `flight/swf/swf_frame_action.hpp:88`
- example: `no viable conversion from 'StructuralRef<RowWritable<RowOf<Ref<MovieClip>>>>' to 'Ref<MovieClip>'`

### 64. `no-viable-conversion-b3064463` — 3 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: GL
- fix route: compiler
- first failure at: `flight/render_gl/enable_gl_render_state_guards.hpp:79`
- example: `no viable conversion from 'Ref<GlRenderState>' to 'Ref<RenderState>'`
- existing request linked: [interface heritage widening](compiler-requests/interface-heritage-widening.md) (site added)

`enableGlRenderStateGuards.ts` calls `getRenderStateRuntime(state)` with its `GlRenderState`. That is a
derived-to-base widening, and the emitter cannot make it because it flattens
`GlRenderState : RenderState`. The guard it installs into the base-typed `renderRootGuard` slot asserts
`state as GlRenderState`, which is refused as `cpp-reference-assertion-without-heritage`. There is no
faithful local fix:
- substituting `get_gl_render_state_runtime` would pick an owner the source does not name;
- a cast, a copied base owner or an always-false guard would misstate the guard.

`_internal_index.hpp` and `contract.hpp` fail by including this header.

**G1 status retained: 0/3 pass, 3 fail.** No code changed, so no recompile was run.

### 65. `no-viable-conversion-b3f3c7c6` — 3 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 3D
- fix route: compiler
- first failure at: `flight/scene3d_formats/md5_parse.hpp:151`
- example: `no viable conversion from 'StructuralRef<RowReadonly<RowOf<shared_ptr<MeshGeometry>>>>' to 'Ref<MeshGeometry>'`

### 66. `no-viable-conversion-b5b698fe` — 3 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 2D
- fix route: compiler
- first failure at: `flight/shape_formats/shape_json.hpp:54`
- example: `no viable conversion from 'Any' to 'Array<Any>'`

### 67. `other-3d2efc1d` — 3 header(s)

- cause: after `Array.isArray(frames)` the emitter narrows only the guard and writes
  `frames[static_cast<size_t>(0.0)]` on the still-erased `flight::Any`
- lanes: 2D
- fix route: compiler — [erased array element reading](compiler-requests/erased-array-element-reading.md)
- first failure at: `flight/textureatlas_formats/texture_atlas_detect.hpp:67`
- example: `type 'Any' does not provide a subscript operator`

The runtime already carries both halves of the faithful lowering: `flight::is_array` (which the
emitter does call) and `Any::external_if<flight::Array<flight::Any>>()` for the checked exact-array
recovery, which no emitted code uses. A subscript operator on `Any` would be permissive, and copying
the array would replace the source's own array view, so no runtime or emission-repair fix is carried;
`contract.hpp` and `_internal_index.hpp` fail through the same include.

Rechecked at `-j1` with Clang 22.1.8: **0/3 pass, 3 fail**, all at the same diagnostic. The route and
request link are recorded; nothing else changed and no native rerun is claimed.

### 72. `other-41f4a462` — 3 header(s)

- cause: a named-property view (which aliases the source object) and an empty Record are the two arms of one conditional, so the conditional has no common type
- lanes: ANIM
- fix route: compiler
- why this route: NamedProperties already aliases the owner and has an empty default state; joining on it preserves identity, while a Record conversion would copy
- first failure at: `flight/skeleton2d_formats/dragon_bones_parse_helpers.hpp:61`
- example: `incompatible operand types ('NamedProperties' and 'flight::Record<flight::String, flight::Any>')`
- compiler request: `docs/compiler-requests/named-view-record-ternary.md`
- related compiler request: `docs/compiler-requests/membership-test-lowered-literally.md`

### 69. `other-6a1604d5` — 3 header(s), fixed by completing the override

- cause: the `tray-prior-emission-tray` override is a placeholder that declares nothing but two
  forward declarations, so it hid `set_tray_animation_guard` from the generated
  `enable_tray_guards.hpp`, which calls it twice
- lanes: PLAT
- fix route: override completeness, not a compiler or binding defect
- first failure was at: `flight/tray/enable_tray_guards.hpp:25`

The 806d257 `tray.hpp` emission *does* declare `animation_guard` and
`set_tray_animation_guard`, but that header does not compile on its own (retained callers name
`attach_tray_event`, `destroy_tray_runtime` and `invoke_update`), so the package ships the previous
pinned compiler's placeholder. The placeholder omitted the slot and setter that a sibling generated
header needs. Both are now supplied in the override exactly as the current emission writes them: one
optional module-level slot and a plain assignment. No native tray provider is invented and the callback
domain is unchanged.

G2: **3/3 pass, plus `flight/tray/tray.hpp` passes**, measured at `-j1` with Clang 22.1.8. The rest of
the module's surface is still only declared in the placeholder's comments and remains owed until the
pinned compiler emits `tray.ts` so that its header compiles independently.

### 70. `other-b860ec87` — 3 header(s)

- cause: `resolveInheritedColorScaleBias` returns a readonly structural row as its declared nominal
  `ColorScaleBias` owner, and the emitter emits no owner recovery on that branch
- lanes: 2D, GL
- fix route: compiler — [readonly-row-return-owner-recovery.md](compiler-requests/readonly-row-return-owner-recovery.md)
- first failure at: `flight/render/enable_color_adjustments.hpp:92`
- example: `no matching constructor for initialization of 'optional<Ref<ColorScaleBias>>'`

A bounded fix was attempted on 2026-10-17 and withdrawn. The sibling `return local` branch emits
`structural_ref_cast<Ref<ColorScaleBias>>`, and adding the same cast at `return parent` made all three
headers move to the comparison family at `:94` (measured at `-j1`). It was withdrawn because an
emission repair may not add an operation, a header override would not compile (the header still fails
on the comparison at `:94` and the omitted helpers at `:147`-`:149` and `:198`, so it could not claim
`supplies`), and the recovery boundary is not the source's: measured on this schema, owner-backed
views (including fresh `make_structural_ref` rows) round-trip, while a bare row owner with no native
object throws `bad_cast`, so the parameter type does not itself promise an owner. The attempted repair
entry and its manifest record were removed and the generated line re-emitted by normal package
generation; **no fix is carried**.

### 71. `other-f9c5bc10` — 3 header(s)

- cause: an `as` assertion over a `flight::JsonValue` is lowered with the erased-value `object_if` pattern, which a tagged JSON value cannot answer
- lanes: 2D
- fix route: compiler
- why this route: a JsonValue holds a tagged JSON tree, not an exact C++ type; the assertion must read the tree through the declared owner or be refused
- first failure at: `flight/tilemap_formats/tilemap_detect.hpp:112`
- example: `no member named 'object_if' in 'flight::JsonValue'`
- compiler request: `docs/compiler-requests/json-value-erased-assertion.md`

### 72. `redefinition-c11d96e9` — G2 recheck of 3 headers

Nothing was emitted twice. The override `overrides/include/flight/bitmapfont_formats/bitmap_font_record.hpp`
had its `#pragma once` joined onto the end of a comment line (`// express.#pragma once`), so it had no
include guard. `bitmap_font_detect.hpp` reaches it twice, through `bitmap_font_fnt.hpp` and
`bitmap_font_binary.hpp`, and the second copy redefined `build_bitmap_font_from_record` and its default
argument. The guard now has its own line. No declaration, body, default argument or signature changed,
and `npm run overrides:check` still matches all 226 overrides to their generated sources: the override
is unchanged against `eb4d4fdf...` and is still `complete`. This was the only override with a commented-out guard.

G2: **0/3 compile, 3 fail**, all attempted at `-j1` with GCC 15.2. `bitmap_font_record.hpp` compiles on
its own. All three now fail at the next diagnostic:

| follow-on cluster | headers | first failure | cause |
| --- | --- | --- | --- |
| `missing-member-dae8c839` | 3 | `flight/bitmapfont_formats/bitmap_font_fnt.hpp:187` | `.width` read on an image-source `std::variant` |

### 73. `undeclared-identifier-145c75d7` — 3 G1 header(s), declaration repaired

The existing guarded `declare-refused-function` repair now declares
`initialize_glyph_source_from_glyph_atlas` with exactly the retained factory's emitted carriers:
`EntityConstruction<Ref<GlyphSource>>` and `StructuralRef<RowReadonly<RowOf<Ref<GlyphAtlas>>>>`.
The exact refusal marker, same-header direct call and absent definition agree; `retainedReference`
and `calledFrom` are unnecessary. Exact-pin single-module generation reproduced only this declaration
and its repair manifest record, with both pinned checkouts restored clean.

The initializer body remains refused at `Bitmap -> TextureSource | null` owner widening, covered by
[the existing heritage-owner request](compiler-requests/compiler-interface-heritage-owner-widening.md#g2-glyph-atlas-declaration-boundary).
The [retained-callee request](compiler-requests/retained-declaration-calls-omitted-callee.md) covers the
missing declaration. The atlas's real bitmap, rasterize-on-miss entries, kerning, shared metrics and
layout revision remain source requirements. No copied texture owner, fake glyph, narrowed source,
initializer body or replacement implementation was supplied; this partial module is still unusable
through the factory or initializer.

Actual checks with Clang 22.1.8: **3 attempted / 3 pass / 0 fail / 0 unattempted**, one worker,
covering only `glyphatlas/_internal_index.hpp`, `glyphatlas/contract.hpp` and `glyphatlas/glyph_source.hpp`.
An unused program including all three headers compiled, linked and ran. Calling the actual retained
factory compiled to an object, then failed to link only on the declared refused initializer.
Six existing repair witness/load/body checks passed. The three product rows now pass with
`previousCluster` and the existing request links; the old name-error cluster is removed. Historical
G1 totals, foreign rows/clusters and unrelated report sections remain unchanged. No global sweep.

### 74. `undeclared-identifier-2cdb69a6` — 3 G1 header(s), 3 now compile

- cause: the emitted code names `channel_elements`, which nothing declares
- subject: `channel_elements`
- lanes: PLAT
- fix route: runtime + override (fixed)
- first failure at: `flight/media/video_channel.hpp:91`
- example: `use of undeclared identifier 'channel_elements'`

`videoChannel.ts` declares `const channelElements = new WeakMap<VideoChannel, HostImageSource>()`
at line 167; the emitter refuses that declaration as `cpp-weak-map-value-representation-unproven`
and still lowers the getter, deleter and setter that use it, so the header named an undeclared map.
The sibling `videoChannelRuntimes` map is emitted in the same header, so the representation is not
in doubt here. `overrides/include/flight/media/video_channel.hpp` is the generated header plus that
one declaration, `flight::WeakMap<flight::Ref<flight::types::VideoChannel>,
flight::types::HostImageSource>`: the channel reference keys by shared-owner identity, the value is
the exact `HostImageSource` the source stores, and an entry stops resolving when its channel is
collected as `WeakMap` requires. It is the source's own storage, not a cache added by the override.

That declaration exposed the next diagnostic in all three headers: emitted code calls
`Task::catch_error` with a zero-argument closure (`catch(() => …)`) while the runtime required a
handler invocable with `const Rejection&`. JavaScript supplies the reason, but a handler that ignores
it may declare no parameter, so `Task::catch_error` now resolves the handler arity through
`detail::invoke_rejection_handler` and calls the zero-argument form with no argument at all, rather
than passing the reason to something that discards it. The exact reason still reaches a handler that
declares it, and the runtime test covers both forms for a value task and for a void task.

### 75. `undeclared-identifier-564c29cc` — 3 G1 header(s), 1 now compiles

- cause: the emitted code names `format_date_value`, which nothing declares
- subject: `format_date_value`
- lanes: TEXT
- fix route: runtime + repair (fixed)
- first failure at: `flight/intl/datetime.hpp:45`
- example: `use of undeclared identifier 'format_date_value'`

`formatDateValue` is refused at `cpp-contextual-union-value-type-unrepresented` because the
options record flowing into the retained `getCacheKey` call is not a represented domain for the
`Readonly<object> | undefined` carrier; that shape is already filed in
`docs/compiler-requests/compiler-intl-optional-and-union-carrier-evidence.md`. The emitter still
lowers `formatDate`, `formatTime` and `formatDateTime`, which call it, so the header named an
undeclared symbol. `intl-datetime-declare-refused-format-date-value` inserts the emitted signature
and nothing else: `flight::String format_date_value(std::variant<flight::Date, double>,
flight::types::LocaleInput, flight::IntlDateTimeFormatOptions)`, taken from the carriers those three
retained calls construct. A translation unit that includes the header but calls none of them links,
and a call of `format_date`, `format_time` or `format_date_time` fails to link on
`format_date_value`, so the formatter stays visibly unusable: no body, no locale behavior, no cache
lookup and no substituted date string.

That fixes `datetime.hpp`. `_internal_index.hpp` and `contract.hpp` include the whole package, so
their first failure moves to `get_number_format` and they join `undeclared-identifier-1a056bbc`.

### 76. `undeclared-identifier-6a187568` — 3 G1 header(s), declaration repaired

- Applied repair: `scene3d-formats-declare-refused-ascii-stl-count`, using the existing guarded declaration-only mechanism. The exact `countAsciiStlFacets` refusal marker, live same-header `collect_stl_features` call and absent definition agree; no cross-header `calledFrom` witness is needed.
- Exact carrier: `std::optional<double> count_ascii_stl_facets(flight::String text);`. The retained caller constructs the String via `decode_utf8` and stores the result as optional<double>. Normal exact-pin single-module generation added only this declaration and its repair manifest record; both pinned checkouts were restored clean.
- Remaining body causes: `countAsciiStlFacets` and `readAsciiStlBodyTokens` are refused on indexed string member presence. The plain token reads are covered by [indexed array member calls](compiler-requests/indexed-array-member-call-present-element.md); six optional indexed member calls are covered by [optional index/member flow](compiler-requests/optional-index-member-present-flow.md). The [retained-callee request](compiler-requests/retained-declaration-calls-omitted-callee.md) records the declaration boundary.
- Semantic limit: the source validates the complete ASCII facet grammar and finite coordinates after exact binary bounds and prefix checks. No parser, fallback count, grammar change or behavior is supplied. Both refusal blocks remain, so this partial module is still not a complete ASCII STL implementation.
- Actual-header proof: unused header compiled, linked and ran; invoking the retained collector compiled to an object, then failed to link on exactly `flight::scene3d_formats::count_ascii_stl_facets(flight::String)`. Five existing repair witness/load/body tests passed.
- Scoped native result after the C++ change: **3 attempted / 3 pass / 0 fail / 0 unattempted**, jobs **1**, covering only `_internal_stl_test_helper.hpp`, `stl_features.hpp`, and `stl_requirements.hpp` in `flight/scene3d_formats/`. All three product rows now pass with `previousCluster` and the request links. No global sweep was run; foreign rows/clusters and opening G1 totals remain unchanged.

### 77. `undeclared-identifier-dc087108` — 3 G1 header(s), declaration repaired

- Applied repair: `video-declare-refused-url-loader` uses the existing declaration-only mechanism. The exact `loadVideoResourceFromUrl` refusal marker, two live same-header calls from the blob and URL-list loaders, and absent definition agree. No `calledFrom` or new mechanism is needed.
- The signature preserves the retained callers' emitted carriers: readonly `HostVideoCapability` structural row, `flight::String`, optional readonly `VideoResourceLoadOptions` row, optional `AbortSignal`, and `Task<Ref<VideoResource>>` result. Source optional-argument defaults are retained. Normal exact-pin single-module generation reproduced only the owned header's declaration addition and matching repair manifest record; both inputs were restored clean.
- The URL body remains refused at `cpp-union-runtime-domains-erased`. [Retained-callee emission](compiler-requests/retained-declaration-calls-omitted-callee.md) and the existing [image-source discriminator contract](compiler-requests/image-source-union-domain-discriminator.md) remain open. HostVideo's load callback already returns the real HostImageSource task and the SDL carrier already retains provider kind; no decoder/provider/host-domain implementation is supplied by this declaration.
- Semantic limit: preserve absent-callback rejection, original URL/options/signal, the returned host element and owning resource, task/error/cancellation behavior, blob object-URL creation/revocation, and URL-list selection. The source's empty-resource branch belongs only to no selected URL; it is not a substitute for an unavailable loader.
- Actual-header proof: unused-header compile/link/run and exact signature/default-argument checks passed. Separate real blob-loader and URL-list-loader programs compiled to objects; each then failed to link with exactly `flight::video::load_video_resource_from_url` undefined. Existing repair witness/rejection/body checks **3/3 passed**.
- Scoped native result: **3 attempted / 3 pass / 0 fail / 0 unattempted**, jobs **1**, for `video_resource_from.hpp`, `contract.hpp` and `_internal_index.hpp` in `flight/video/`. Own rows now pass with `previousCluster` and both request links, but URL loading remains unusable when called and the module stays partial. Foreign rows/clusters and historical sweep totals are unchanged; no global sweep.

### 78. `call-has-no-matching-84410867` — 2 header(s)

- cause: the emitted call to `buffer_data` matches no declared overload
- subject: `buffer_data`
- lanes: 3D
- fix route: compiler
- first failure at: `flight/scene3d_gl/gl_wireframe_upload.hpp:136`
- example: `no matching member function for call to 'buffer_data'`

### 79. `call-has-no-matching-d42144f0` — 2 header(s)

- cause: the emitted call to `push` matches no declared overload
- subject: `push`
- lanes: ANIM
- fix route: compiler
- first failure at: `flight/swf/swf_sound_handler.hpp:120`
- example: `no matching member function for call to 'push'`

### 80. `missing-member-da12a523` — 2 G1 header(s), 0 pass, 2 follow-ons

- cause: the emitted code reads a member the target does not declare (`parse_obj_material_library`)
- subject: `parse_obj_material_library`
- lanes: 3D
- fix route: runtime + repair (fixed)
- first failure at: `flight/scene3d_formats/obj_material_model.hpp:58`
- example: `no member named 'parse_obj_material_library' in namespace 'scene3d_formats'`

`parseObjMaterialLibrary` is refused in `mtl_parse.ts` because `lines[i].trim()` projects a member
on the internal optional storage of a plain array index inside a loop bounded by `lines.length`
(the family in `docs/compiler-requests/indexed-array-member-call-present-element.md`). The module's
other declarations are emitted, and `collectObjMaterialModels` in `obj_material_model.hpp` calls the
refused function, so both headers named a missing member. The existing declaration mechanism is
enough and the callers are in another header, so
`scene3d-formats-declare-refused-parse-obj-material-library` names `obj_material_model.hpp` in
`calledFrom` and inserts only the emitter's own signature:

```cpp
flight::Ref<flight::types::ObjMaterialLibrary> parse_obj_material_library(
    flight::String source,
    std::optional<flight::Array<flight::Ref<flight::types::ImportDiagnostic>>> diagnostics = std::nullopt);
```

It supplies no MTL grammar, material record, diagnostics tally or parser result. An unused include of
`mtl_parse.hpp` links, and a call of `parse_obj_material_library` fails to link on exactly that
symbol. The retained collector in `obj_material_model.hpp` cannot compile yet, but for the
structural-reference follow-on below, not for this declaration — so it has not reached a link either
way. The declaration itself is recorded in
`docs/compiler-requests/retained-declaration-calls-omitted-callee.md`.

#### Follow-on `static-assertion-ecc68c2b`, and `undeclared-identifier-d663f62e`

Neither header compiles yet; the repair moved them past the missing member.

- `obj_material_model.hpp` now fails at `flight/structural_ref.hpp:448` with
  `dependent_false<flight::types::ObjMaterial>`: `has_obj_pbr_directives` casts the anonymous
  material-shape row to `RowReadonly<RowOf<Ref<ObjMaterial>>>`, and the runtime rejects the source
  object type. That is a structural-reference conversion gap, not a missing declaration.
- `obj_standard_pbr_material_handler.hpp` now names `obj_material_to_standard_pbr`, another
  retained caller whose callee (`objMaterialToStandardPbr`, source line 30) is refused in the same
  header. It is a separate cause and is listed as its own cluster rather than folded into this one.

### 81. `missing-member-dd9b4baa` — 2 header(s)

- cause: the emitted code reads a member the target does not declare (`renderbuffer_binding`)
- subject: `renderbuffer_binding`
- lanes: 3D, GL
- fix route: runtime
- first failure at: `flight/render_gl/gl_cube_render_target.hpp:96`
- example: `no member named 'renderbuffer_binding' in 'host_sdl::WebGl2Context'`

`createGlCubeRenderTarget` saves the bindings it will restore:

```ts
gl.getParameter(gl.RENDERBUFFER_BINDING) as WebGLRenderbuffer | null
gl.getParameter(gl.TEXTURE_BINDING_CUBE_MAP) as WebGLTexture | null
```

The host `WebGl2Context` declared neither query. The emitted cast already names the right nullable
handles, so the gap is the host binding, not the emitter. G2 adds both queries to
`include/flight/host_sdl/webgl.hpp` and `src/host_sdl/webgl.cpp`:
- `renderbuffer_binding` (`GL_RENDERBUFFER_BINDING`, 0x8CA7) and `texture_binding_cube_map`
  (`GL_TEXTURE_BINDING_CUBE_MAP`, 0x8514);
- a `std::optional<WebGlRenderbuffer>` alternative and conversion on `GlParameterValue`;
- `get_parameter` branches that resolve the bound name to the handle state this context issued for it,
  exactly as `FRAMEBUFFER_BINDING` and `TEXTURE_BINDING_2D` already do.

So the query returns the same object the source bound (`0` is `null`), and a name the context never
issued raises the existing `native GL returned an object outside this WebGL context` error rather than a
manufactured handle. `tests/host_sdl_test.cpp` pins both enum values and the nullable-handle conversion
at compile time.

No live GL context exists on this machine and the SDL host library is not built here, so the query has
not been executed. `src/host_sdl/webgl.cpp` (with SDL's built-in GL definitions) and the host test
were syntax-checked only.

**G2: 0/2 compile, 2 fail** (`-j1`, Clang 22.1.8, compiler `806d257` / Flight `a627849`). Both headers
pass `renderbuffer_binding` and now fail first in the same function, `read_gl_cube_face_box`
(`gl_cube_render_target.hpp:140-142`). Both rows move to `no-matching-conversion-bc278253`, which covers
two further host binding shapes:
- `gl.getParameter(parameter) as ArrayLike<number> | null` is cast to
  `std::optional<SequenceView<double>>`, while `GlParameterValue` converts only to
  `std::variant<SequenceView<double>, Null, Undefined>`;
- `gl.drawingBufferWidth`/`Height` are read as properties of what the host declares as member functions.

`gl_environment_capture.hpp` has later, unrelated scene3d-gl failures behind those (`TextureCube`,
`begin_gl_cube_render_face`, `add_gl_scene_3d_resource_cleanup`).

#### Follow-on `no-matching-conversion-bc278253`, the same two headers

Both shapes are host binding gaps and are answered without changing the existing call API.

- **Nullable numeric sequence.** `GlParameterValue` gains `explicit operator
  std::optional<SequenceView<double>>()`. It is the nullable-carrier twin of the existing
  `variant<SequenceView<double>, Null, Undefined>` operator. The only callers pass `SCISSOR_BOX` or
  `VIEWPORT`, which WebGL never answers with `null`, so the result is an engaged view over the same `Array`
  owner. A parameter that is not a numeric sequence is refused with the same `logic_error`, never read as
  absent and never through `Any`.
- **Property read of a host accessor.** The existing `render-gl-drawing-buffer-width/height-property-binding`
  repairs call `drawing_buffer_width()`/`drawing_buffer_height()` only where the read ends a statement.
  The new `render-gl-cube-face-box-drawing-buffer-property-binding` repair answers the one braced
  `Array<double>` site in `readGlCubeFaceBox`. It calls the accessor and applies the `int` to Number
  conversion the JavaScript read performs, which is exact for any pixel dimension. The host methods are
  unchanged. Like the others, it expires when external binding metadata distinguishes native property
  accessors during emission.

The generated header and manifest carry exactly that repair's application. A scoped
`pkg:generate @flighthq/render-gl` on this floor also re-hashed closure-dependent anonymous structs in
four render-gl headers, two of them shadowed by peer-owned overrides. Since AGENTS.md notes that these
names depend on the closure that first met the shape, that drift was not committed. Only the repaired
line and its manifest record were kept.

`tests/host_sdl_test.cpp` pins the carrier at compile time. `tests/host_sdl_gl_app_test.cpp` asserts it in
a live offscreen context: `viewport(1, 2, 30, 40)` reads back as an engaged four-element view, and a
non-sequence parameter is refused. That test is gated on `Flight::HostSdlGl` and has not been run here.
builder4's live cache test separately covers the renderbuffer and cube-map binding queries.

**G2: 1/2 compile, 1 fail** (`-j1`, Clang 22.1.8). Passing: `flight/render_gl/gl_cube_render_target.hpp`.
`flight/scene3d_gl/gl_environment_capture.hpp` moves to the existing `missing-member-95ead583`. In
compiler order, its first failure is now the included `gl_environment_cube.hpp:56` variant `version` read
that already holds its three environment siblings. Behind that, its own body reads
`gl.get_parameter(gl.active_texture)`: the WebGL constant `ACTIVE_TEXTURE` snake-cases onto the host's
`active_texture()` member function. The host spells that constant `active_texture_constant`, but only
hand-written overrides use the `*_constant` names. That collision is recorded here and is not filed as a
cluster of its own, since no header fails on it first.

### 82. `no-viable-conversion-303c42f1` — 2 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: ANIM
- fix route: compiler
- first failure at: `flight/skeleton2d_formats/dragon_bones_section_counts.hpp:33`
- example: `no viable conversion from 'Any' to 'double'`

### 83. `other-67dc02c6` — 2 header(s), still failing

- cause: `getSurfaceHandle(surface) as HTMLCanvasElement` is emitted as `static_cast<GlCanvas>` of the erased
  `NativeSurfaceHandle` (`flight::Any`)
- lanes: 2D
- fix route: compiler (checked assertion lowering), and an SDL Canvas host provider for the handle's domain
- first failure at: `flight/scene2d_canvas/canvas_element.hpp:34`
- example: `no matching conversion for static_cast from 'NativeSurfaceHandle' to 'host_sdl::GlCanvas'`
- existing request extended: [assertion lowered to unchecked static_cast](compiler-requests/assertion-lowered-to-unchecked-static-cast.md)

`GlCanvas` is the right native `HTMLCanvasElement`: it has `style`, `width`, `height` and
`get_context("2d")` returning `CanvasRenderingContext2D`. `Any`'s checked read of it is
`external_if<GlCanvas>()`. But `HostCanvasCapability.create` returns `NativeSurfaceHandle` (`unknown`), and
no SDL host provider implements that capability yet. So nothing natively defines what a Canvas surface
handle holds. Neither a cast, nor narrowing the `unknown` domain to `GlCanvas`, nor an invented
provider or surface would be faithful.

The request is extended with all nine source sites. These two headers fail on the first two. **G1
status retained: 0/2 pass, 2 fail.** No code changed, so no recompile was run.

### 84. `other-6f513db2` — 2 header(s)

- cause: an SDL attribute lookup returning int is list-initialized into the generated double location field
- lanes: 2D, 3D
- fix route: runtime binding corrected; both remaining failures are compiler debt
- original first failure at: `flight/scene2d_gl/gl_particle_emitter_2d.hpp:58`
- original example: `non-constant-expression cannot be narrowed from type 'int' to 'double' in initializer list [-Wc++11-narrowing]`

The pinned DOM declaration returns GLint, a TypeScript number. The SDL binding now returns double
and explicitly converts the unchanged real GL query result. Every signed 32-bit GLint, including
the missing-location -1 sentinel, is exactly representable; the program owner, encoded name, lookup
and error behavior remain unchanged. No generated header, source patch, emission repair or warning
suppression is carried.

The changed two-header check at one worker is **0 pass, 2 fail, 0 unattempted**. Both original narrowing
errors clear. The 2D header now fails on the refused draw callee at line 268, recorded as
`undeclared-identifier-789527d7` under the [existing retained-callee request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-gl-particle-renderer-after-the-attribute-location-number-carrier-fix).
The 3D header reaches literal membership at line 113 and joins the
[existing membership cause](compiler-requests/membership-test-lowered-literally.md#g2-particle-3d-reaches-the-previously-latent-membership-site).
Both stay failed; the old narrowing cause has no remaining owned rows. Historical totals and
unowned rows remain unchanged.

The native binding implementation and extended host_sdl_gl_app test compile to objects locally.
Builder4 built the changed target at one job and ran `flight_cpp.host_sdl_gl_app` once on real SDL
offscreen software GL: **1/1 pass**. All three C++ source blobs exactly match this change. The test
links an actual shader with attribute location 5, checks that location and missing-name -1 through
a double aggregate, pins the return type and checks teardown without a GL error. Existing application
checks ran in that same invocation. Local live configuration lacked the SDL3 library package;
Builder4 used its existing SDL3 3.4.2 setup. This verifies the binding, not either particle draw body.

### 85. `other-86d0b50b` — 5 current header(s), 2 in G1

- cause: a SceneCoverageEntry object literal initializes the outer outcome variant instead of its selected concrete reference arm
- lanes: 2D, 3D
- fix route: compiler — [existing outcome-construction request](compiler-requests/object-literal-into-outcome-variant.md#g2-scene3d-resource-coverage-returns-a-literal-into-the-outer-outcome-variant)

The three new Scene3D resource rows are measured after the callback declarations in work item 56,
at `explain_scene_3d_resource_coverage.hpp:57`. The two original GL rows retain their G1 diagnostics
and were not recompiled or edited. The complete outcome must select the actual fallback/catalog arm
and retain all source spread fields; no permissive runtime constructor or incomplete outcome is supplied.
- first failure at: `flight/scene2d_gl/explain_gl_scene_2d_coverage.hpp:58`
- example: `initialization of non-aggregate type 'SceneCoverageEntry' with a designated initializer list`

### 86. `redefinition-caa49360` — 2 header(s), fixed by de-duplicating the overrides

- cause: two hand-written picking overrides defined the same `flight::picking::detail` helpers
  (`ensure_node_local_matrix4`, `ensure_node_world_matrix4`, `node_world_matrix4`), so a translation unit
  that includes both redefined them
- lanes: 3D
- fix route: override de-duplication, not a compiler or runtime defect
- first failure was at: `flight/picking/scene_hit_attributes.hpp:21`

`pick_scene_3d.hpp` and `scene_hit_attributes.hpp` carried byte-identical copies of the three helpers —
same parameter types, no default arguments, same bodies. Neither module defines these in Flight source;
both import the helper from `@flighthq/node/contract`, and each override had re-derived its own plain
`Node3DView` form. `scene_hit_attributes.hpp` now includes `pick_scene_3d.hpp` and keeps no copies, so
there is one definition with the same body, name, namespace and ABI.

Measured at `-j1`: the redefinition is gone from all four headers, `pick_scene_3d.hpp` still passes, and
the two failing headers plus `scene_hit_attributes.hpp` now stop first on the material conversion at
`scene_hit_attributes.hpp:60` (cluster `no-viable-conversion-cef6403d`). No behavior was deleted or
widened; the only change is that one copy of an identical helper is gone.

### 87. `undeclared-identifier-1048386b` — 2 header(s)

- cause: the retained global material family references its refused handler binding
- subject: `md5_material_handler`
- lanes: 3D
- fix route: compiler — [existing retained-binding request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-md5-material-family-retains-a-refused-binding)
- related: [declared material heritage and owner identity](compiler-requests/interface-heritage-widening.md#g2-md5-material-handler-needs-the-original-concrete-material-owner)
- first failure at: `flight/scene3d_formats/md5_material_handler.hpp:69`
- example: `use of undeclared identifier 'md5_material_handler'`

**G2: 0/2 compile, 2 fail, 0 unattempted** (Clang 22.1.8, one worker, compiler `806d257` /
Flight `a627849`). The material header and its section-registry consumer both retain this diagnostic.
The whole source handler binding is refused at a `BlinnPhongMaterial` to `Material` assertion:
the declared source heritage is absent between the flattened native owners. The retained material
family globally initializes its one element from that omitted binding; the full-support registry
includes this header and spreads the skeleton family followed by the material family.

An exact extern-only readonly `Md5SectionHandler` declaration, derived from the sibling handler's
emitted carrier and applied with the existing declaration engine in scratch, compiles the actual
unused-header program to an object. Its **required positive link fails**, with exactly
`flight::scene3d_formats::md5_material_handler` undefined from the global family initializer.
Main does not read the family or invoke a handler. This is a failed prerequisite, not a successful
explicit-call negative-link check; the declaration is rejected. No repair, generated C++, manifest,
source patch, substitute material/body or native behavior claim is carried. Both rows stay failed,
with unchanged global/G1 totals and no header gain.

### 88. `undeclared-identifier-20fa691d` — 2 header(s), compiler route

Scoped Clang22.1.8, one worker: **2 attempted / 0 pass / 2 fail / 0 unattempted**. Both
`scene2d_gl/gl_raster_shape_renderer.hpp` and `scene2d_gl/gl_shape.hpp` fail at the former's
line173, `use of undeclared identifier 'draw_gl_raster_shape'`.

The [retained-callee request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-raster-shape-declaration-witness-is-valid-but-callback-carriers-fail)
now records the exact cross-header `calledFrom` witness in `gl_shape.hpp`. The existing repair
mechanism matches, but a scratch unused header with only the exact draw declaration fails at
`.create_data`: native `optional<ErasedRef>(Ref<GlRenderState>, Node2D)` cannot initialize generic
`optional<Ref<RendererData>>(Ref<RenderState>, Node2D)` callback storage. No unused-header positive
link or missing-body negative-link boundary was reached; no declaration repair is carried.

The draw body remains refused at a [null-presence property test](compiler-requests/nullable-optional-presence.md#g2-raster-shape-null-presence-refusal).
The ledger identifies only `property`, not the exact null test, so no source cell is guessed.
[Typed backend/data owner recovery](compiler-requests/readonly-callback-derived-owner-recovery.md#g2-raster-shape-native-and-generic-renderer-callbacks)
records the callback boundary. The existing native shape-data override remains partial: raster
surfaces, image upload and raster-resource teardown are unavailable. Full command replay, pixelRatio
invalidation, cache resource deletion, image versioning, bounds/alpha/material/color batching and
real texture/quad submission are still required; no fake draw, copied owner or narrowed scene is supplied.

Only owned2 rows/cluster/report slice and existing requests changed. Historical/foreign products and
all measured C++ inputs remain unchanged; no generation, broad sweep or old live baseline rerun.

### 89. `undeclared-identifier-225478b5` — 2 header(s), compiler route

Scoped Clang 22.1.8, one worker: **2 attempted / 0 pass / 2 fail / 0 unattempted**. Both
`scene2d_gl/gl_bitmap_text.hpp` and `scene2d_gl/scene_2d_gl_pipeline.hpp` still fail at
`gl_bitmap_text.hpp:163`, `use of undeclared identifier 'submit_gl_bitmap_text'`.

The retained `gl_bitmap_text_renderer` aggregate refers to the refused `submitGlBitmapText`.
Its body asserts readonly `Node2DRuntime` to writable `BitmapTextRuntime`, without the declared
owner/cells for `pages`, `glyphLayoutVersion`, `lineCount` and `truncated`. The current native
`NodeRuntime<Ref<Node2DTraits>>` accessor is not the flattened `BitmapTextRuntime` owner either.
[Typed callback/runtime owner recovery](compiler-requests/readonly-callback-derived-owner-recovery.md#g2-gl-bitmap-text-needs-its-own-runtime-owner)
covers that boundary; removing only the writable assertion does not establish the missing cells.

One scratch copy added only the exact source callback declaration, `void submit_gl_bitmap_text(
Ref<GlRenderState>, Ref<RenderProxy2D>);`. The unused-header program still failed to compile at
`.create_data`: actual `noop_renderer_data(RenderState, NodeAny)` cannot initialize
`SpriteRenderer.create_data(RenderState, Sprite)`. This is the existing
[heritage/carrier request](compiler-requests/interface-heritage-widening.md#g2-gl-bitmap-text-renderer-create-data-callback).
The later submit `GlRenderState` versus generic `RenderState` domain was inspected, not reached by
that first-error probe. The [retained-callee request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-gl-bitmap-text-retains-a-refused-callback-in-an-aggregate)
records why no declaration-only repair or positive/negative link claim was carried.

The required renderer uses real page ids/transforms, atlas textures, UV rotation, alpha, material
and color adjustments through the GL batch writer, retaining batch-flush ordering and actual draw
counts. No fake draw, copied owner, narrowed input, callback adapter or source/runtime change was
supplied. Only the two owned rows/cluster, this report slice and existing requests changed; G1 totals
and foreign products are retained. No generation or global/native baseline rerun was needed.

### 90. `undeclared-identifier-3e98002c` — 2 header(s), declaration repaired

- original cause: retained `createAwd2ParseState` calls `initializeAwd2ParseState` after its body was
  refused at the `Object.assign` boundary
- lanes: 3D
- fix route: declaration-only emission repair, with the initializer debt still open
- compiler request: [object assign onto structural record](compiler-requests/object-assign-onto-structural-record.md)

The exact refusal marker, the same-header retained call and the absence of any definition justify
`scene3d-formats-declare-refused-awd2-parse-state`. It declares the five carriers that call constructs:
the `EntityConstruction` of the parse-state reference, the `Scene3DDocument` reference, `Uint8Array`,
`DataView` and the optional diagnostics array. No parse state, map, array, owner or field assignment is
supplied, and the refused initializer itself stays refused.

Written by **normal package generation** at the exact pins (`flight` a627849 / `flight-compiler`
806d257), and the applied text matches the repair engine's output byte for byte.

Checks at `-j1` with Clang 22.1.8:

- **Unused header positive:** including `awd2_block_dispatch.hpp` with an empty `main` compiles, links
  and runs.
- **Exact negative link:** a translation unit that calls `initialize_awd2_parse_state` with those
  carriers compiles to an object and then fails to link on exactly
  `flight::scene3d_formats::initialize_awd2_parse_state(StructuralRef<RowWritable<RowOf<shared_ptr<Awd2ParseState>>>>, shared_ptr<Scene3DDocument>, TypedArray<unsigned char>, DataView, optional<Array<shared_ptr<ImportDiagnostic>>>>)`.
- **Scoped headers:** `awd2_block_dispatch.hpp` passes; `scene3d_formats/_internal_index.hpp` now stops
  on the existing `create_texture` carrier family at `shared.hpp:100`.

Pre-existing inventory drift was preserved rather than swept: the generation check also reports
`generated/README.md`, `best-effort.json` and `initialization.json` differing for unrelated glTF-skin
module entries and package counts that this repair does not touch; those files were left at their
committed content.

### 91. `undeclared-identifier-644a9b33` — 2 header(s)

- cause: the emitted code names `clip_programs`, which nothing declares
- subject: `clip_programs`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_gl/gl_clip_contours.hpp:72`
- example: `use of undeclared identifier 'clip_programs'`
- G2 fix: partial override `scene2d-gl-clip-contours-context-cache`; the binding gap is filed as
  [a bound Flight interface never reaches its weak-key policy](compiler-requests/bound-interface-weak-map-key-policy.md)

`glClipContours.ts:122` declares `const clipPrograms = new WeakMap<GlContext, ClipProgram>()`. The emitter
refuses it (`cpp-weak-map-key-representation-unproven`) for the same reason as effects-gl's blit caches:
`GlContext` is a Flight interface, so the `WebGl2ContextWeakPolicy` the binding declares is never read.
The emitted `ensure_clip_program` and `clip_program_for` still use it. This module refuses nothing else.

The override copies the emission and declares the source's own map as a module-scope
`flight::WeakMap<GlContext, Ref<ClipProgram>, WebGl2ContextWeakPolicy>`: one map for the program's
lifetime, keyed by context identity, holding the context weakly. The cached program and buffer handles
hold their surface only weakly, so no value keeps its key alive. No cache, program or shader is invented.

With the map in place, three host-binding spellings in the same bodies surfaced. The override corrects
each one in place, without changing behaviour:
- `drawingBufferWidth`/`Height` are read through the host accessors, as the render-gl drawing-buffer
  repairs already do;
- the source's three-argument `uniformMatrix3fv` with a plain array is called without the two absent
  optional offsets the emitter appended;
- `gl.CULL_FACE` becomes the host's `cull_face_constant`, because `cull_face` is the method.

The live GL behaviour is compiled but not executed here, since this machine has no SDL/GL.

**G2: 1/2 compile, 1 fail** (`-j1`, Clang 22.1.8). Passing: `flight/scene2d_gl/gl_clip_contours.hpp`.
`flight/scene2d_gl/gl_clip.hpp` moves to `undeclared-identifier-befaa2d6`: its first failure is the
included `gl_clip_rectangle.hpp:108` (`compute_scissor_rect`). Behind that are its own `std::function`
registration conversions at `gl_clip.hpp:38-49`.

### 92. `undeclared-identifier-7ce7bd18` — 2 header(s)

- cause: the retained global skin family references its refused handler binding
- subject: `md2_skin_handler`
- lanes: 3D
- fix route: compiler — [existing retained-binding request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-md2-skin-family-retains-its-refused-binding)
- related: [concrete material heritage and MD2 semantics](compiler-requests/interface-heritage-widening.md#g2-md2-skin-handler-reaches-the-same-material-owner-boundary)
- first failure at: `flight/scene3d_formats/md2_skin_handler.hpp:123`
- example: `use of undeclared identifier 'md2_skin_handler'`

**G2: 0/2 compile, 2 fail, 0 unattempted** (Clang 22.1.8, one worker, compiler `806d257` /
Flight `a627849`). Both the section registry and skin handler retain this diagnostic. The actual
source21 binding independently refuses `BlinnPhongMaterial` to `Material` without emitted heritage;
the retained global skin family reads the omitted value. The source's fixed64 byte path bounds,
truncation/aggregate-empty diagnostics, alternate material order/identity and first-only mesh binding
remain obligations, distinct from the MD5 handler's per-mesh shader behavior.

A scratch declaration through the existing engine uses the actual emitted readonly
`Md2SectionHandler` carrier from its animation sibling. The original path reader/global initializer
stay intact. The actual unused-header program compiles to an object, but its **required positive link
fails** with exactly `flight::scene3d_formats::md2_skin_handler` undefined from global initialization.
The declaration is rejected; this is not a successful explicit-use negative-link check. No repair,
generated C++, manifest, source patch, substitute skin/material/body or native behavior claim is
carried. Both rows stay failed; global/G1 totals and all unowned rows remain unchanged.

### 93. `undeclared-identifier-86212434` — G2 recheck of 2 header(s)

- cause: the emitter refuses the `glSsaoEffectRunner` binding (and its callee `applySsaoEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_ssao_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: a runtime definition would fake the effect runner. The runner refusal (`cpp-structural-assertion-writable-capability-unproven`) names an owner-preserving carrier as the compiler-side answer. The callee refusal (`cpp-contextual-union-value-type-unrepresented`) is the `Readonly<GlTextureRenderTarget>` to `Readonly<GlRenderTarget> | null` widening of [interface-heritage-widening](compiler-requests/interface-heritage-widening.md).
- first failure at: `flight/effects_gl/gl_ssao_effect.hpp:88`
- example: `use of undeclared identifier 'gl_ssao_effect_runner'`
- Scoped native recheck (GCC 15.2, jobs 1): **2 attempted / 0 pass / 2 fail**. Both headers fail first at line 88. `gl_contact_shadows_effect.hpp:41` also calls the refused `apply_ssao_effect_to_gl`. That declaration would satisfy all three witnesses, but it would move neither row, so it is not carried. No runner, effect body, cache or narrowed shader domain is supplied.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 94. `other-8b0f8597` — 2 header(s), formerly `undeclared-identifier-a02f3fb2`

- Source patch: `scene3d-formats-gltf-skin-matrix-loop` replaces the exact length-16 `Float32Array.from` mapper with one fresh typed array per joint and ascending component writes. It names the fresh inverse-bind array's existing Matrix4Like element type and constructs the fresh diagnostic Record inside its original argument position. No skin/mesh/source owner is copied or replaced, no empty success/domain narrowing or declaration-only substitute is supplied.
- Normal exact-pin generation of only `gltfSkins.ts` and `registerGltfSkinHandlers.ts`: **2 emitted / 0 refusals**, through 245 patches. The full existing-tree structural table reproduces before the change and gains only eight lines for the newly used readAccessor/reportAccessorFault keys. Only the two owned refusal/module records and their aggregate emitted/refused counts change; all foreign metadata and consumer edges are preserved. This records C++ emission, not native compilation success.
- Source equivalence: **9 pinned original/patched handler cases passed**, covering Float32 conversion/NaN/signed zero/absent elements, independent matrices, joint mapping, original document/mesh owners, accessor absence/fault/count mismatch, read/report/data exceptions, absent skins, and a diagnostic getter that mutates the count before detail evaluation.
- New native first failure: `flight/scene3d_formats/gltf_skins.hpp:118`, `member reference base type 'double' is not a structure or union`. The emitted `matrices->data[index].value_or(0)` calls optional operations on the double from SequenceView::operator[]. The existing [required-read/nullish-storage request](compiler-requests/host-initialize-optional-fallback-on-a-required-read.md#g2-gltf-skin-component-distinguish-an-index-miss-from-a-present-double) records the checked `get(index).value_or(0)` contract. The [typed-array binding request](compiler-requests/typed-array-static-from-binding.md) stays open for the general operation.
- Scoped final native result: **2 attempted / 0 pass / 2 fail / 0 unattempted**, jobs **1**, root and registration header only, both at the same new diagnostic. Both rows remain failed with previousCluster and request links. The real handler behavior executable failed to build and was not run; no positive unused-header link, expected negative-link result, callable skin import or native behavior pass is claimed. No declaration repair or fake body was added. Historical G1 sweep/carry totals and foreign worklist rows/clusters remain unchanged; no global sweep.

Follow-up aggregate audit against the saved exact two-module emission corrects an earlier counting error:
`registerGltfSkinHandlers` was already counted as emitted while marked `dependency-incomplete`; only
`gltfSkins` changes the package emitted/refused counts. The supported aggregate delta is therefore **+1
emitted / -1 refused**, not +2/-2. `scene3d-formats` is corrected from 75/36 to **74 emitted / 37 refused**,
and the global summary/README decrease emitted by one and increase refused by one. Both owned best-effort
rows already agree with the saved emitted records; their out-of-closure consumer edge remains preserved.

The same saved output supplies the two missing owned initialization entries/module records. Existing
`evaluationGroups` derivation adds exactly their two noncyclic singleton groups; every foreign entry,
module and evaluation group remains unchanged and in the same order. No header/body/refusal ledger,
source patch or native status changes in this audit, and no generation or native check is rerun. The
previous native **0/2 fail** result remains unchanged.

### 95. `undeclared-identifier-e39f9d67` — 2 G1 header(s), resolver restored

The initial scoped run measured **2 attempted / 0 pass / 2 fail / 0 unattempted**, Clang22.1.8,
one worker, both at the older active override's missing resolver on `gl_shaded_prelude.hpp:168`.
The current concrete-value source patch is already rebased/applied. Normal exact-pin single-module
generation retains all nine modifier arms, three definition arms and the real checked resolver.
The prelude override is rederived from that emission, keeping byte-identical GLSL and only the
previously accepted native nested-optional color-feature projection.

The existing `declare-refused-function` mechanism adds the exact private Map-guard declaration under
its refusal/direct-call/absent-body witnesses. An unused actual prelude compiled, linked and ran;
a real Map-alternative resolver call compiled to an object, then failed to link only on
`is_gl_modifier_snippet_table(GlModifierSnippetSource)`. The override remains partial; no guard,
shader snippet or rendering body is supplied. The [Map constructor-evidence request](compiler-requests/readonly-map-instanceof-member-evidence.md)
records the remaining refusal; the [concrete-owner request](compiler-requests/modifier-concrete-value-path.md)
records the restored full domain.

The final scoped run measured **2 attempted / 1 pass / 1 fail / 0 unattempted**, jobs **1**,
Clang22.1.8, 306.5 seconds. `gl_shaded_prelude.hpp` passes. The material renderer now reaches:

| follow-on cluster | headers | first failure | cause |
| --- | --- | --- | --- |
| `readonly-view-round-trip-4476a9e9` | 1 | `flight/scene3d_gl/gl_shaded_mesh_material_renderer.hpp:169` | a retained readonly program row is reconstructed through a writable row before the readonly draw helper |

This is the existing [guarded readonly helper request](compiler-requests/guarded-readonly-optional-helper-call.md#g2-shaded-material-follow-on),
with the same native storage provenance as the nine earlier renderers. No mutable upgrade, nominal
owner recovery or program/cache copy is valid. Only these original two rows and their replacement
cluster changed; `previousCluster` retains their G1 group. Opening sweep/carry totals and all foreign
rows/clusters remain unchanged. Header and link boundaries do not establish shader assembly or
shaded rendering while the guard has no body; no global/native baseline was rerun.

### 96. `variant-cast-37507e07` — 2 header(s)

- cause: a functional-style cast is applied to a variant the target type cannot take
- lanes: 3D
- fix route: compiler
- first failure at: `flight/scene3d_formats/gltf_punctual_lights.hpp:216`
- example: `no matching conversion for functional-style cast from 'optional<Ref<GltfPunctualLight>>' to 'StructuralRef<RowWritable<RowOf<Ref<GltfPunctualLight>>>>'`

### 97. `call-has-no-matching-00dc04ad` — 1 header(s)

- cause: the emitted call to `get_texture_uv_matrix` matches no declared overload
- subject: `get_texture_uv_matrix`
- lanes: 2D
- fix route: compiler
- first failure at: `flight/scene2d_canvas/canvas_texture_view.hpp:68`
- example: `no matching function for call to 'get_texture_uv_matrix'`

### 98. `call-has-no-matching-35224a3f` — 1 header(s)

- cause: the emitted call to `gl_uniform_type_name` matches no declared overload
- subject: `gl_uniform_type_name`
- lanes: 3D
- fix route: compiler
- first failure at: `flight/scene3d_gl/enable_gl_scene_3d_custom_shader_guards.hpp:85`
- example: `no matching function for call to 'gl_uniform_type_name'`

### 99. `call-has-no-matching-4c12e411` — 15 header(s)

- cause: the `create_bind_group_layout` entries literal is emitted as anonymous nested owners for the bound `GPUBindGroupLayoutEntry`, inside an untyped braced descriptor
- subject: `create_bind_group_layout`
- lanes: WGPU
- fix route: compiler
- why this route: the same emission as the descriptor literal request. It includes the 14 headers from `call-has-no-matching-076d207f` that now first fail at `flight/render_wgpu/wgpu_shader.hpp:98`
- first failure at: `flight/render_wgpu/wgpu_fullscreen_pass.hpp:148`
- example: `no matching member function for call to 'create_bind_group_layout'`
- compiler request: `docs/compiler-requests/external-descriptor-literal-construction.md`

### 100. `call-has-no-matching-76a6a61a` — 1 header(s)

- cause: the emitted call to `get_if` matches no declared overload
- subject: `get_if`
- lanes: 2D
- fix route: compiler
- first failure at: `flight/scene2d_resources/resolve_scene_2d_resources.hpp:49`
- example: `no matching function for call to 'get_if'`

### 101. `call-has-no-matching-7f4ae181` — 1 header(s)

- cause: the emitted call to `add_scene_2d_used_kind` matches no declared overload
- subject: `add_scene_2d_used_kind`
- lanes: 2D
- fix route: compiler
- first failure at: `flight/scene2d/scene_kind_usage.hpp:95`
- example: `no matching function for call to 'add_scene_2d_used_kind'`

### 102. `call-has-no-matching-8a1f3a33` — 1 header(s)

- cause: the emitted call to `row_get` matches no declared overload
- subject: `row_get`
- lanes: GL
- fix route: compiler
- first failure at: `flight/render_gl/gl_external_texture.hpp:51`
- example: `no matching function for call to 'row_get'`

### 103. `call-has-no-matching-8d9de718` — 1 header(s)

- cause: the emitted call to `collect_spatial_candidates` matches no declared overload
- subject: `collect_spatial_candidates`
- lanes: PLAT
- fix route: compiler
- first failure at: `flight/interaction/interaction_spatial_index.hpp:74`
- example: `no matching function for call to 'collect_spatial_candidates'`

### 104. `call-has-no-matching-b3c6bcee` — 1 header(s)

- cause: the emitted call to `log_once` matches no declared overload
- subject: `log_once`
- lanes: GL
- fix route: compiler
- first failure at: `flight/render_gl/enable_gl_render_texture_guards.hpp:27`
- example: `no matching function for call to 'log_once'`

### 105. `call-has-no-matching-c287f892` — 1 header(s)

- cause: the emitted call to `named_properties` matches no declared overload
- subject: `named_properties`
- lanes: ANIM
- fix route: compiler
- first failure at: `flight/particles_formats/unity_parse.hpp:77`
- example: `no matching function for call to 'named_properties'`

### 106. `call-has-no-matching-c8046ec6` — 1 header(s)

- cause: the emitted call to `uniform2fv` matches no declared overload
- subject: `uniform2fv`
- lanes: 3D
- fix route: compiler
- first failure at: `flight/scene3d_gl/gl_custom_shader_mesh_material_renderer.hpp:127`
- example: `no matching member function for call to 'uniform2fv'`

### 107. `call-has-no-matching-e7b28969` — 1 header(s)

- cause: the emitted call to `write_texture` matches no declared overload
- subject: `write_texture`
- lanes: WGPU
- fix route: compiler
- first failure at: `flight/render_wgpu/wgpu_texture_upload.hpp:22`
- example: `no matching member function for call to 'write_texture'`

### 108. `call-has-no-matching-fbd6de0d` — 1 header(s)

- cause: the emitted call to `invoke` matches no declared overload
- subject: `invoke`
- lanes: 3D
- fix route: compiler
- first failure at: `flight/array.hpp:43`
- example: `no matching function for call to 'invoke'`

### 109. `call-has-no-matching-fee2f93f` — 1 header(s)

- cause: the emitted call to a function matches no declared overload
- lanes: 3D
- fix route: compiler
- first failure at: `flight/scene3d_gl/_internal_gl_scene_3d_test_helper.hpp:373`
- example: `no matching function for call to object of type 'function<void (double, String, Array<Any>)>'`

### 110. `invalid-operands-dc1c09c4` — 1 header(s), header boundary fixed

- original cause: the optional number/string `basis` member is compared directly with `String("auto")`, then projected as a number even for an invalid runtime string
- lanes: PLAT
- fix route: [equivalent source guards](compiler-requests/mixed-value-union-literal-comparison.md) plus declaration-only repair
- original first failure at: `flight/layout/flex_layout.hpp:171`
- original example: `invalid operands to binary expression ('variant<double, String>' and 'String')`

`layout-flex-basis-unknown-comparison` moves the strict literal and finite nonnegative numeric tests into
two boolean helpers over the field's existing union. Each checks its local active type; all three original
property reads remain in their short-circuit positions. Original/patched full-source validation agrees in
**30 cases**, including changing/throwing getters and invalid values. Normal exact-pin single-module
emission reproduces the explicit variant-index guards. No global variant equality, Any conversion,
coercion, snapshot, copied owner, narrowed values or layout implementation is added.

The separate [nullable style owner refusals](compiler-requests/assertion-to-nullable-variant-alternative.md#g2-flex-header-boundary-nullable-style-owners-remain-refused)
leave `getFlexBase`, `getFlexShrinkScale` and the resolver binding omitted. The existing exact-header
`insert-forward-declaration` repair `layout-declare-refused-flex-resolver` declares only the actual
`LayoutResolver` binding for retained explicit registration. Normal generation reproduces the declaration
and repair record; the module/refusal ledger and package emission counts remain partial and unchanged.

- **Original header gate:** complete serial C++20 check, **1 attempted / 1 pass / 0 fail / 0 unattempted**.
- **Unused header positive:** actual header compiles, links and runs.
- **Explicit registration negative:** actual retained registration compiles to an object, then fails to link on exactly `flight::layout::flex_layout_resolver`.
- **Native guard domain:** the actual emitted anonymous style owner passes absence/auto/nonnegative, invalid string/number, NaN/infinity, negative zero, alignment and grow/shrink checks with owner/value preservation. Arbitrary style-owner recovery and full layout behavior are not proved.

The one row is pass with `previousCluster`; the original active cluster is removed. No other header is
measured or promoted, and foreign/G1 totals remain unchanged. The refused resolver stays unavailable;
no empty callback, fake rectangle or successful layout result is supplied.

### 111. `missing-member-1620083c` — 1 header(s)

- cause: the SDL DOM shell has no shared element/style compositor boundary for the web renderer's blend write
- subject: `mix_blend_mode`
- lanes: 2D
- fix route: binding — [existing shared DOM style request](compiler-requests/dom-style-shared-element-boundary.md#g2-dom-material-blend-writes-use-the-same-missing-shared-style-boundary)
- first failure at: `flight/scene2d_dom/dom_materials.hpp:27`
- retained example: `no member named 'mix_blend_mode' in 'host_sdl::DomStyle'`

The source writes `HTMLElement.style.mixBlendMode` on the original element and installs that helper in
the existing reference-backed `DomRenderState.applyBlendMode`. The emitted element and callback arguments
are value `HtmlElement` carriers, aliases of `DomElement` with inline plain-String `DomStyle`; even a new
field would mutate a copy. The existing SDL application shell has no retained HTML/CSS rendering provider
for this operation. No field-only runtime addition, fake material/compositor, copied owner, disconnected
side table, no-op assignment or new backend is supplied.

The source blend families are open strings. Keep all 17 mapped keys, `Add -> screen` **approximate**,
other mapped modes' **exact** fidelity, normal's present empty string and null/unregistered-key empty-CSS
fallback. Do not narrow vendor modes or turn an absent lookup into a throw or invented fidelity result.
The source lookup/assignment order, existing element/style identity, callback and setter effects remain
requirements for a real binding; no later compiler diagnostic is inferred from the inspected lookup.

The pinned package and manifest explicitly declare environment `web`, outside the native SDL profile's
existing empty applicable-environment set. This already derives not-applicable status; no policy, deferral,
count or header inventory changes. **Original 0/1 pass, 1 fail retained**, with the same diagnostic and
cluster. No C++/binding/source/override/generation input changed, so no unchanged header/native/rendering
check or generation is rerun. Only the existing request and owned cluster/row/table/section111 change;
foreign/G1 data and the sibling opacity cluster remain intact.

### 112. `undeclared-identifier-af6f50aa` — 1 header(s)

- previous cluster: `missing-member-1a2e58cb`
- cause: the DOM image registration retains its refused optional image callback after the persistent table registration now emits
- subject: `resolve_dom_image_texture`
- lanes: 2D
- fix route: compiler — [retained optional callback](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-dom-image-registration-retains-a-refused-optional-callback)
- related request: [exact host alias and full source-member domain](compiler-requests/compiler-imported-alias-outer-union-abi.md#g2-dom-image-callback-exact-alias-recovery-must-preserve-source-member-reads)
- current failure at: `flight/scene2d_dom/dom_image_texture_resolver.hpp:67`
- current example: `use of undeclared identifier 'resolve_dom_image_texture'`

The carried `scene2d-dom-texture-table-carrier` source patch adds only the existing local table's
`ReadonlyMap<TextureSourceKind, DomTextureResolver>` annotation; the complete module emits byte-identical
JavaScript. Normal exact-pin two-module generation emits the real persistent registration body using
its existing runtime owner and Map clone. Native execution passes open vendor registration, replacement,
present/absent removal, snapshot isolation, state isolation, no eager callbacks and original callback,
state and texture owners. The generated registration header exactly matches those tested bytes.

The separately tried image-result alias annotation is **rejected and uncarried**. Its normal body and
both headers compiled; native image/null results and all seven provider-kind identities passed, but a
represented Bitmap texture source threw `bad_variant_access` where the actual pinned source returns
null for absent `.source`. No narrowing of the declared Texture/source owners is accepted. The original
image header remains byte-identical, still marks the callback refused, and its nested optional initializer
is outside the existing callback declaration witness. No witness extension or declaration bypass is added.

Final committed-input scoped jobs1 check: **original 0/1 pass, 1 fail** at the next missing callback;
**changed dependency 1/1 pass** (`dom_texture_resolver.hpp`), total1/2 with zero unattempted. The old
registration-member error is cleared; image resolution and generic `resolveDomTexture` remain unavailable.
The two best-effort statuses, package/global counts and initialization records remain unchanged; only the
owned table fingerprint and refusal record move. The explicit `web` environment and existing native SDL
applicability/deferral policy remain unchanged. No decoder, DOM image backend, copied owner, narrowed
input, fake callback or override is supplied. Foreign worklist/G1 rows and totals are preserved.

### 113. `missing-member-1ce5ed2e` — 1 header(s), declaration repaired

- original cause: `registerBuiltInPhysics3DJointSolvers` re-emits all seven registrations, but six of
  the built-in solver bindings are refused, so the header names members that do not exist
- lanes: 3D
- fix route: declaration-only emission repair, with the solver bodies still owed
- compiler request: [flattened interface refinement identity](compiler-requests/flattened-interface-refinement-identity.md)

`joints.ts` refuses the fixed, distance, cone-twist, hinge, generic-6dof and slider solvers at
`cpp-reference-assertion-without-heritage`: each method asserts a `Physics3DJoint` to its concrete
joint type. The exact `NOT GENERATED: variable (binding)` marker for each, the register call in the
sibling header and the absent definition agree, so six repairs insert an `extern` declaration each.
No solver object, method, joint assertion or registration result is supplied.

Written by **normal package generation** at the exact pins and matched byte for byte by the repair
engine. Checks at `-j1` with Clang 22.1.8: the header **passes**; an unused include compiles, links and
runs; a translation unit that calls `register_built_in_physics_3d_joint_solvers` compiles and fails to
link on all six symbols at once, so registration stays visibly unavailable rather than substituted.
The generated `joints.hpp` and `manifest.json` were the only paths written; no ledger drift.

### 114. `missing-member-42783657` — G2 recheck of 1 header(s)

- cause: the bound SDL DOM shell lacks a shared CSS style owner for the native text `font`, `textAlign` and `lineHeight` writes
- subjects: `font`, `text_align`, `line_height`
- lanes: 2D
- fix route: binding
- why this route: `apply_native_text_style` takes `host_sdl::HtmlElement` by value, and `HtmlElement` aliases the value `DomElement`. Adding the three `String` fields to `DomStyle` would compile, but the writes would land on a copy, which is the copied facade that [dom-style-shared-element-boundary](compiler-requests/dom-style-shared-element-boundary.md) rules out. The refused `drawDomNativeText` is still registered at lines 136 and 139, which is reached only after this boundary. `scene2d-dom` declares environment `web`, so applicability is unchanged.
- first failure at: `flight/scene2d_dom/dom_native_text.hpp:69`
- example: `no member named 'font' in 'host_sdl::DomStyle'`
- Scoped native recheck (GCC 15.2, jobs 1): **1 attempted / 0 pass / 1 fail**, same diagnostic. No DOM/text backend, style field, metrics, glyphs or narrowed input is supplied.
- compiler request: `docs/compiler-requests/dom-style-shared-element-boundary.md`

### 115. `missing-member-5ac43ed0` — 1 header(s)

- original cause: indexed write calls `element` on optional Uint32Array storage
- lanes: 3D
- fix route: [erased source assertion](compiler-requests/initialized-optional-typed-array-write.md), then [existing writable output-owner request](compiler-requests/writable-structural-out-of-a-wider-owner.md#g2-instanced-mesh-bounds-after-the-color-write-fix)
- original first failure at: `flight/scene3d/instanced_mesh.hpp:233`
- original example: `no member named 'element' in 'optional<TypedArray<unsigned int>>'`

`scene3d-instanced-color-present-write` adds only `!` to the final indexed color write. Whole original
and patched module JavaScript is byte-identical. Normal exact-pin single-module generation reproduces
payload unwrapping and `set_index`, preserving typed-array conversion and invalid-property behavior;
the only other emitted diff renumbers a later temporary. No runtime API, owner/material copy,
permissive Any or restricted numeric domain is added.

The original one-header check completes at one worker: **0/1 pass, 1 fail, 0 unattempted**. The color
error clears, but `compute_instanced_mesh_local_bounds_aabb` now fails at line353 passing its scratch
Aabb to `transform_aabb_by_matrix4`'s nominal writable AabbLike slot. Line363 has the same issue at
`union_aabb`. The row moves to `no-viable-conversion-84892a1c` with its previous cluster retained;
the original cause has no remaining owned rows. Historical/G1 totals and unowned rows stay unchanged.

The actual real-factory/color-setter native test failed to build at these two bounds calls, so no
native test ran. Its scratch source/logs remain available; no unbuildable test/registration is carried.
The source-identity and generation checks pass, with no header gain or native color/instance/material
behavior pass claimed. The bounds owner contract remains open; no body, cast or copy adapter hides it.

### 116. `missing-member-703ef98e` — G2 recheck of 1 header(s)

- cause: the additive mask function refuses at a generic animatable-property conversion; its registration and global family retain the absent callback
- subject: `lottie_additive_mask_handler`
- lanes: 2D
- fix route: [retained callback/family request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-lottie-mask-family-requires-the-refused-function-during-initialization), with [generic call specialization](compiler-requests/generic-call-argument-uses-unsubstituted-parameter-type.md#g2-lottie-mask-handler-refuses-before-emitting-the-call)
- actual first failure at: `flight/scene2d_formats/lottie_mask_handlers.hpp:18`
- example: `no member named 'lottie_additive_mask_handler' in namespace 'flight::scene2d_formats'`

The original one-header check completes with Clang22.1.8 at one worker: **0/1 pass, 1 fail,
0 unattempted**. Exact pinned source and aliases retain the same Ref<LottieMaskContext>, readonly mask
views, original Node2D target and import context. The source guard/initial-outline/animated update
semantics remain owed; no fake mask, empty family, copied owner or narrowed domain is supplied.

A scratch candidate through the existing declaration loader/engine inserts only the actual function
signature at its exact refused marker, matches one header and reaches a fixed point. The real family
header and its inline global initializer stay intact. An include-only program with an empty main
compiles to an object, but its required unused-header positive link fails on exactly this missing
handler, referenced by the family's line21 global initializer. This is not an explicit-use negative-link
pass. No executable or native mask behavior runs.

The candidate is rejected. No C++, repair, source patch, generated inventory or manifest is changed;
no normal generation or global/package/native baseline rerun is needed. Only the owned product root/row
and this worklist entry are reconciled, and the existing requests gain this exact source/link evidence.
The row remains failed, no header gain is claimed, and all foreign rows/prose and G1 totals are preserved.

### 117. `missing-member-8aa8170f` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `swfVideoHandler` binding (`cpp-contextual-union-value-type-unrepresented`), and `swfVideoTagFamily` still names it. The binding's `createPlacementNode` omits the slot's trailing `diagnostics` parameter and returns `Sprite | null` where the slot returns `Node2D | null`. TypeScript accepts both differences.
- subject: `swf_video_handler`
- lanes: ANIM
- fix route: compiler
- why this route: a runtime handler would fake video placement, and the declaration repair declares only functions, not a `Ref<SwfTagHandler>` variable. The callable needs a signature adapter that keeps the `Sprite` identity, and only the emitter can produce one.
- first failure at: `flight/swf/swf_video_tag_family.hpp:15`
- example: `no member named 'swf_video_handler' in namespace 'swf'`
- Scoped native recheck (GCC 15.2, jobs 1): **1 attempted / 0 pass / 1 fail**, same diagnostic. `swf_video_handler.hpp` itself compiles. No handler, decoder, empty success or narrowed domain is supplied.
- compiler request: `docs/compiler-requests/callable-arm-in-contextual-union.md`

### 118. `missing-member-ad3558ee` — 1 G1 header(s), 1 now compiles

- cause: the emitted code reads a member the target does not declare (`physics_2d_distance_joint_solver`)
- subject: `physics_2d_distance_joint_solver`
- lanes: ANIM
- fix route: runtime + repair (fixed)
- first failure at: `flight/physics2d/register_built_in_physics_2d_joint_solvers.hpp:21`
- example: `no member named 'physics_2d_distance_joint_solver' in namespace 'physics2d'`

All nine `physics2D*JointSolver` bindings in `joints.ts` are refused at
`cpp-reference-assertion-without-heritage`: each body narrows `Ref<Physics2DJoint>` to its concrete
joint, and there is no C++ heritage or checked owner recovery to cast along (the measured boundary in
`docs/compiler-requests/interface-heritage-widening.md`). The emitter still lowers
`registerBuiltInPhysics2DJointSolvers`, which passes every binding to
`register_physics_2d_joint_solver(world, kind, Ref<Physics2DJointSolver>)`, so the header named a
member nothing declared. `physics2d-declare-refused-joint-solvers` is the existing
`insert-forward-declaration` mechanism guarded by the exact refused-binding marker: it declares the
nine `extern Ref<Physics2DJointSolver>` slots in the refused module's own header, which is the carrier
the registration call constructs. It supplies no warm-start, solve, accumulated-impulse clearing or
reaction write, no impulse math, and no joint narrowing.

The cluster's advantage over the refused-binding families that failed their positive prerequisite is
structural: these bindings are referenced only inside the inline registration function, not by a
global initializer. An unused include of the registration header links (exit 0), and a call of
`register_built_in_physics_2d_joint_solvers` compiles to an object and fails to link on exactly the
nine solver symbols (exit 1), so registration stays visibly unusable rather than substituted.

### 119. `missing-member-b5408b35` — G2 recheck of 1 header(s)

- cause: the override `overrides/include/flight/scene3d_gl/gl_shadow_map.hpp` calls `flight::node::get_node_world_matrix4` at line 228. That function is declared in `flight/node/node_transform3d.hpp`, which the generated header includes and the override did not. The function exists and nothing about it is missing.
- subject: `get_node_world_matrix4`
- lanes: 3D
- fix route: override (fixed)
- change: the override now has the generated header's own `#include <flight/node/node_transform3d.hpp>`, and nothing else. The shadow behavior, material domain and the refused `renderGlScene3DShadowMap` are unchanged. `derivedFrom` still matches the generated file, and `overrides:check` reports all 228 overrides fresh.
- Scoped native recheck (GCC 15.2, jobs 1): **1 attempted / 1 pass / 0 fail**. A TU that only includes the header links against `libflight_cpp_c.a`. No rendering behavior is claimed as run.

### 120. `missing-member-d7bf452c` — 1 header(s)

- cause: `Object.entries` selects an iterable helper for a readonly structural Record view
- subject: `begin`
- lanes: 2D
- fix route: compiler/runtime contract — [readonly Record construction and enumeration](compiler-requests/readonly-record-entry-enumeration.md)
- first failure at: `flight/object.hpp:45`
- example: `no member named 'begin' in 'StructuralRef<RowReadonly<RowOf<Record<String, FlightDocumentValue>>>>'`

**G2: 0/1 compile, 1 fail, 0 unattempted** (Clang 22.1.8, one worker, compiler `806d257` /
Flight `a627849`). The owned SDK header is
`scene_document/scene_document_interactive_state_bindings.hpp`; `object.hpp` is the included runtime
location. Its source `cloneFields` snapshots `Object.entries` of an existing readonly field Record
before recursively cloning each value into its intentionally fresh output.

A runtime-only scratch delegation to the checked native Record's existing `enumerable_entries()`
clears `begin`, but the same header still fails at line152 converting a Record value into
`StructuralRef<RowWritable<RowOf<Record<...>>>>`. That changed-input check also completes **0/1**.
The candidate is rejected: an enumeration helper alone cannot provide the missing owner/readonly
construction contract or assume every structural input has an exact native Record owner. Preserve
source dictionary identity/lifetime, typed recursive values, own string-key order and the entry
snapshot. No permissive Any, copied dictionary/view, empty result or narrowed domain is supplied.
No production C++, generation or native behavior change is carried. The original row/diagnostic stays
failed, with foreign rows/clusters and historical G1 totals unchanged.

### 121. `missing-member-dae8c839` — 4 header(s)

- Cause: `format_bitmap_font_fnt` reads `.width` and `.height` directly on `primary_image.value()`, an image-source variant, after proving only its optional presence. This is in the formatter; the parser has no winning missing-width diagnostic.
- Route: compiler, [existing variant member-projection request](compiler-requests/reference-backed-outcome-variant-narrowing.md). All seven stored source-owner alternatives already declare both dimensions. `texture.hpp`'s emitted `get_texture_width` demonstrates the appropriate active-owner visitor; a runtime field or binding cannot add a direct member to `std::variant`.
- Preserve the first page/source selection and absence, read width then height from that same owner, and keep the formatter's zero fallback. Do not narrow the open TextureSource kind family to Bitmap or ImageResource, copy a base owner, coerce through Any, fabricate sizes, or replace the captured source with repeated lookups and the texture helper's different minus-one fallback.
- First recorded failure: `flight/bitmapfont_formats/bitmap_font_fnt.hpp:187`, missing `width` on the seven-owner variant. Four owned rows retain their exact diagnostics and existing `previousCluster` include-guard-repair provenance, and now link the request. The work-item ID remains for continuity.
- Recorded result retained: **0/4 pass, 4 fail**. No C++/source patch/binding/override/repair registry changed, so no unchanged native rerun or normal generation was performed. Parser/atlas resolution, metrics, glyph and kerning ordering/identity, and shared scratch behavior were not changed or claimed working. Foreign rows/clusters and historical sweep totals remain unchanged; no global sweep.

### 122. `missing-member-dcf85c36` — G2 recheck of 1 header(s)

- cause: `collectMd5AnimFeatures` is refused (`cpp-member-projection-without-present-storage` on `lines[i].trim()`), but its caller `parseMd5AnimRequirements` is kept in `md5_anim_requirements.hpp`.
- subject: `collect_md5_anim_features`
- lanes: 3D
- fix route: repair (fixed)
- change: declaration-only emission repair `scene3d-formats-declare-refused-collect-md5-anim-features` (`calledFrom` the requirements header). All three witnesses hold: the exact NOT GENERATED marker, the retained call, and no definition. The declaration uses the emitter's own carriers, `flight::Set<flight::String>(flight::String)`, the same ones the emitted sibling `collect_collada_features` uses.
- Normal exact-pin `pkg:generate @flighthq/scene3d-formats` (245 patches): the repaired header and the manifest repair record reproduce. `--check` differs only in four foreign paths (`README.md`, `best-effort.json`, `initialization.json` and the `manifest.json` module counts). These are the glTF-skin records from section 94, and the committed tree's versions are kept unchanged.
- Scoped native recheck (GCC 15.2, jobs 1): **1 attempted / 1 pass / 0 fail**. A TU that only includes the header links. A TU that calls `parse_md5_anim_requirements` fails to link, and the only undefined reference is `collect_md5_anim_features`. No feature set, requirements or narrowed domain is supplied.
- compiler request: `docs/compiler-requests/indexed-array-member-call-present-element.md`

### 123. `missing-member-ee398c94` — G2 recheck of 1 header(s)

- `register_canvas_texture_resolver` is not missing. `canvas_texture_resolver.hpp:104` defines it with the parameters line 50 passes (resolvers, source kind and optional `CanvasTextureResolver`), and the header includes it. The swept diagnostic was a cascade from an earlier failure in that include chain.
- Scoped native recheck (GCC 15.2, jobs 1, all errors shown): **1 attempted / 0 pass / 1 fail**. There is no error at line 50. The first failure is `flight/scene2d_canvas/canvas_render_state.hpp:69`: `'get_canvas_render_state_runtime' has not been declared`. That is cluster 2's re-export before declaration, reached through `canvas_texture_resolver.hpp`, `canvas_render_surface.hpp` and `canvas_render_state.hpp`. The two later errors, the `CanvasRenderState` to `RenderState` conversion at `:253` and `canvas_render_surface.hpp:35`, are in the same included chain.
- No Canvas backend, image provider, decode or resolver is supplied. The image resolver's own code is unchanged.

| follow-on cluster | owned headers | first failure | route / cause |
| --- | --- | --- | --- |
| `missing-member-b1a6d5cf` | 1 | `flight/scene2d_canvas/canvas_render_state.hpp:69` | compiler: re-export `using` before declaration; [existing request](compiler-requests/local-reexport-using-before-declaration.md) |

### 124. `missing-member-fd2be1bc` — G2 recheck of 1 header(s)

- cause: `isBmFontJsonObject` reads `raw as { chars?: unknown; common?: unknown }` from a parsed `JsonValue`. The emitter lowers this with the erased-`Any` `object_if` recovery, which `JsonValue` does not offer.
- subject: `object_if`
- lanes: TEXT
- fix route: compiler
- why this route: a `JsonValue::object_if` would build the anonymous struct from the JSON tree, and that is permissive materialization. A `Record<string, unknown>` source rewrite reaches the missing conversion of `parsed-json-object-shape-reads`. The faithful lowering reads `chars` and `common` from the JSON object while keeping absence distinct, and only the emitter can choose it.
- first failure at: `flight/bitmapfont_formats/bitmap_font_format_kind.hpp:41`
- example: `no member named 'object_if' in 'JsonValue'`
- Scoped native recheck (GCC 15.2, jobs 1): **1 attempted / 0 pass / 1 fail**, same diagnostic. No detector, parser, `Any` conversion or narrowed domain is supplied.
- compiler request: `docs/compiler-requests/json-value-erased-assertion.md`

### 125. `no-viable-conversion-03bf8175` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 2D
- fix route: compiler
- first failure at: `flight/scene2d/html_view.hpp:104`
- example: `no viable conversion from returned value of type 'Value' to function return type 'variant<host_sdl::HtmlElement, Null, Undefined>'`

### 126. `no-viable-conversion-0faa55ca` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: GL
- fix route: compiler
- first failure at: `flight/render/explain_scene_2d_pipeline_coverage.hpp:60`
- example: `no viable conversion from 'Map<[...], Ref<NodeRenderer>>' to 'Map<[...], Any>'`

### 127. `no-viable-conversion-2909a65f` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 2D
- fix route: compiler
- first failure at: `flight/scene2d_dom/merge_dom_render_options.hpp:65`
- example: `no viable conversion from returned value of type 'Array<tuple<String, Ref<CanvasShapeCommand<ShapeCommandKey>>>>' to function return type 'Map<String, Ref<CanvasShapeCommand<ShapeCommandKey>>>'`

### 128. `no-viable-conversion-31b754c5` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: ANIM
- fix route: compiler
- first failure at: `flight/particles_formats/spine_parse.hpp:102`
- example: `no viable conversion from returned value of type 'Any' to function return type 'Record<String, Any>'`

### 129. `no-viable-conversion-331a999f` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: GL
- fix route: compiler
- first failure at: `flight/render_gl/gl_readback.hpp:37`
- example: `no viable conversion from 'variant<Float32Array, Uint8Array>' to 'ArrayBufferView'`

### 130. `no-viable-conversion-4bbb9be5` — 1 header(s) *(builder4/E0a)*

- cause: a value is passed where its declared type cannot receive it
- lanes: 2D
- fix route: compiler
- first failure at: `flight/scene2d_gl/gl_rich_text.hpp:221`
- example: `no viable conversion from 'WeakMap<TextureSourceValue, [...]>' to 'WeakMap<Ref<TextureSource>, [...]>'`
- owned by: builder4 (E0a: TextLabel / layout / Scene2D GL text)

### 131. `no-viable-conversion-562151da` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: WGPU
- fix route: compiler
- first failure at: `flight/scene3d_wgpu/wgpu_environment_ibl_bake.hpp:87`
- example: `no viable conversion from 'WgpuObject<WgpuTextureViewTag>' to 'Any'`

### 132. `no-viable-conversion-68fff449` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 3D
- fix route: compiler
- first failure at: `flight/scene3d/scene_animation.hpp:64`
- example: `no viable conversion from 'Ref<Node3D>' to 'Ref<SceneAnimationMorphProbe>'`

### 133. `no-viable-conversion-7e6bb150` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: WGPU
- fix route: compiler
- first failure at: `flight/effects_wgpu/wgpu_color_matrix_pass.hpp:41`
- example: `no viable conversion from 'Ref<WgpuEffectPipeline>' to 'Ref<entity_runtime_key_pipeline_blend_mode_compile_for_format_variants_5a993344e208d790>'`

### 134. `no-viable-conversion-802e8004` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: GL
- fix route: compiler
- first failure at: `flight/render_gl/gl_render_state.hpp:329`
- example: `no viable conversion from 'const Ref<GlRenderState>' to 'Ref<RenderState>'`

### 135. `no-viable-conversion-9695423c` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 2D
- fix route: compiler
- first failure at: `flight/scene2d_formats/rive_object_graph.hpp:39`
- example: `no viable conversion from 'Record<String, double>' to 'optional<Record<String, variant<bool, double, String>>>'`

### 136. `no-viable-conversion-a516fbdb` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 3D
- fix route: compiler
- first failure at: `flight/scene3d/prepare_scene_3d_morph.hpp:32`
- example: `no viable conversion from 'StructuralRef<RowReadonly<RowOf<NodeAny>>>' to 'Ref<geometry_enabled_869b32e775d5849b>'`

### 137. `no-viable-conversion-b47bc6cd` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 2D
- fix route: compiler
- first failure at: `flight/scene_document/scene_document_layout_bindings.hpp:138`
- example: `no viable conversion from 'Any' to 'const ErasedRef'`

### 138. `no-viable-conversion-bf84cbac` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 3D
- fix route: compiler
- first failure at: `flight/scene3d_gl/present_gl_scene_3d.hpp:46`
- example: `no viable conversion from 'StructuralRef<RowReadonly<RowOf<shared_ptr<GlTextureRenderTarget>>>>' to 'Ref<GlTextureRenderTarget>'`

### 139. `no-viable-conversion-c997578d` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 2D
- fix route: compiler
- first failure at: `flight/scene2d_canvas/canvas_bitmap_texture_resolver.hpp:65`
- example: `no viable conversion from 'StructuralRef<RowReadonly<RowOf<shared_ptr<Bitmap>>>>' to 'const shared_ptr<Bitmap>'`

### 140. `no-viable-conversion-cef6403d` — 3 header(s)

- cause: the picking override's `get_scene_3d_hit_material` returns the stored
  `optional<variant<Ref<SpecularGlossinessPbrMaterial>, …>>` where the declared result is
  `optional<Ref<Material3D>>`; the concrete material owners have no identity-preserving heritage to the
  declared interface
- lanes: 3D
- fix route: compiler
- first failure at: `flight/picking/scene_hit_attributes.hpp:60`
- example: `no viable conversion from returned value of type 'optional<variant<shared_ptr<SpecularGlossinessPbrMaterial>, shared_ptr<VertexColorMaterial>, shared_ptr<UnlitMaterial>, shared_ptr<StandardPbrMaterial>, shared_ptr<CustomShaderMaterial>, shared_ptr<BlinnPhongMaterial>, shared_ptr<LambertMaterial>, sh...`

`picking/_internal_index.hpp` and `picking/contract.hpp` join this cluster after the duplicate-helper
redefinition below was removed; `picking/scene_hit_attributes.hpp` was already here and its line moved
from `:131` to `:60` when the override stopped repeating the node matrix helpers. This is the gap the
override's `remaining` names: the material result is narrowed to the concrete `Material3D` type until
generated owners carry identity-preserving interface heritage.

### 141. `no-viable-conversion-dc893fc7` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: GL
- fix route: compiler
- first failure at: `flight/render_gl/gl_compressed_texture.hpp:407`
- example: `no viable conversion from 'const nullopt_t' to 'size_t'`

### 142. `no-viable-conversion-eabaf455` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 3D
- fix route: compiler
- first failure at: `flight/scene3d/scene_material.hpp:94`
- example: `no viable conversion from 'optional<variant<shared_ptr<SpecularGlossinessPbrMaterial>, shared_ptr<VertexColorMaterial>, shared_ptr<UnlitMaterial>, shared_ptr<StandardPbrMaterial>, shared_ptr<CustomShaderMaterial>, shared_ptr<BlinnPhongMaterial>, shared_ptr<LambertMaterial>, shared_ptr<DepthMaterial>`

### 143. `no-viable-conversion-f7267dc5` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: ANIM
- fix route: compiler
- first failure at: `flight/skeleton2d_formats/spine_json_bones_handler.hpp:36`
- example: `no viable conversion from 'Ref<reflection_rotation_scale_translation_9dff6858eecb0eb4>' to 'Ref<TransformInherit2D>'`

### 144. `other-1e3a0d33` — G2 recheck of 1 header(s)

- original cause: readonly preset tuple emits erased Any cells, then mixes an element with a shared preset Ref
- lanes: ANIM
- fix route: [erased source element-union annotation](compiler-requests/readonly-preset-tuple-element-owner.md), then [optional Record detail](compiler-requests/optional-record-literal-not-materialized.md) and [preset/Bone2D owner contract](compiler-requests/satisfies-record-member-carrier.md#g2-spine-binary-table-selection-and-bone-storage-are-distinct-boundaries)
- original first failure at: `flight/skeleton2d_formats/spine_binary_bones_handler.hpp:48`
- original example: `incompatible operand types ('value_type' (aka 'flight::Any') and preset Ref)`

The explicit readonly element-union annotation emits byte-identical JavaScript. Normal exact-pin
emission includes the unchanged canonical preset dependency so the existing alias engine retains its
shared owner. Only the exact owned header/provenance is carried; every emitted function/parser body
is unchanged. The initial incomplete-closure trial fails at a local anonymous-owner Array constructor
and is not carried. No shared preset table, runtime API, bone/parser/body, view copy or input domain
is replaced.

Original1 and final changed1 each complete at jobs1: **0/1 pass, 1 fail, 0 unattempted**.
The Any conditional clears; final first failure is optional Record diagnostic construction at line63.
The row stays failed, moves to existing `other-40da43f7` with previousCluster retained, and that
cluster's owned membership rises by one. The separate nominal preset-to-Bone2D store remains inspected
debt. Historical/G1 totals and all unowned rows/roots/prose are preserved apart from that exact count.

Source proof and normal output/alias checks pass. No native bone/parser execution, link check,
complete functionality or header gain is claimed. Binary consumption, root parent omission,
invalid-parent Drop/overrun, sticky truncation and original context/diagnostics/preset identities
remain the source contract, recorded in the linked requests. No global/package baseline is rerun.

### 145. `other-4703cbbe` — G2 recheck of 1 header(s)

- cause: JSON.parse yields JsonValue, but the emitted assertion casts it to an unrelated nominal GltfDocument reference
- lanes: 3D
- fix route: [existing JSON assertion request](compiler-requests/json-parse-assertion-static-cast.md#g2-gltf-feature-collection-keeps-the-json-and-document-input-domains), with [nested JSON/absence reads](compiler-requests/parsed-json-object-shape-reads.md)
- actual first failure at: `flight/scene3d_formats/gltf_features.hpp:59`
- example: `no matching conversion for static_cast from 'JsonValue' to 'Ref<GltfDocument>'`

Original1 jobs1 Clang22.1.8 completes in7.8s: **1 attempted / 0 pass / 1 fail / 0 unattempted**.
All functions are emitted; a declaration repair cannot fix the cast. Runtime JSON already has a real
kind-tagged tree, not the nominal GltfDocument/asset/schema owners. The emitted folded root guard and
version-presence check also fail to represent source JSON type checks; no native execution claimed.

Preserve both document/string arms, parse-only catch, dynamic root/version/optional-length and
extension iteration semantics, fresh Set with source insertion/deduplication order and GLB's exact
view/header/min-length/chunk/TextDecoder behavior. This collector neither validates complete resource
owners nor loads buffers/images/scenes. No fake features/parser, copied owner, empty-success result,
narrowed arrays/schema/extension domain or resource loading is supplied.

No C++, runtime, binding, source patch, repair, override, manifest, inventory or changed normal
generation is carried. Only this failed root/row and worklist entry plus one existing request appendix
change. Foreign raw/semantic rows/roots/prose, package/G1 counts and prior glTF ledger/edge corrections
remain unchanged. No native feature/parser/header/functionality gain is claimed.

### 146. `other-5dfe27ec` — G2 recheck of 1 header(s)

- cause: an Array.isArray guard is followed by a range loop on raw Any instead of an original-owner array-element view
- lanes: ANIM
- fix route: [existing erased-array request](compiler-requests/erased-array-element-reading.md#g2-dragonbones-skins-emitted-display-iteration-and-refused-collection), with [derived attachment heritage](compiler-requests/interface-heritage-widening.md#g2-dragonbones-skins-need-the-original-derived-attachment)
- actual first failure at: `flight/skeleton2d_formats/dragon_bones_skins_handler.hpp:373`
- example: `invalid range expression of type 'flight::Any'; no viable 'begin' function available`

The original one-header check completes at one worker: **0/1 pass, 1 fail, 0 unattempted**.
Current AnyArray keeps exact typed array handles and identity, but supplies no generic element reader.
An Array<Any>-only iterator/recovery, materialized replacement array, filtered display list or narrowed
input is not the full unknown-array contract. Eight helpers/collectors remain explicitly omitted,
including erased array assertions, contextual Any construction, typed-array static from, unqualified
identifier presence and the RegionAttachment2D-to-Attachment2D owner boundary. No particular unnamed
presence-test variable is guessed.

The existing requests now record full default/alternate skin order, positional null display retention,
shared actual attachment owners, default table updates before unresolved-slot filtering, original
context/diagnostic arrays, image/unweighted/weighted mesh fields, raw positional bone remapping,
slot/bone-pose inverse transforms, truncated/singular/unresolved/overflow recovery and typed conversions.
No fake skins, empty success, copied owner/view or restricted mesh/array domain is carried.

No C++, source, binding, iterator, declaration/repair, override, inventory or manifest changes; no normal
generation is needed. No native skin/parser/weighting behavior, positive-unused/negative-link check or
header gain is claimed. Only this original failed row/root and worklist entry change; all foreign
rows/roots/prose and historical/G1 totals are preserved.

### 147. `other-acb4b229` — G2 recheck of 1 header(s)

- cause: `JSON.parse(json) as TexturePackerDocument` emits a C++ cast from JsonValue to a variant of unrelated array/hash document owners
- lanes: 2D
- fix route: [JSON assertion request](compiler-requests/json-parse-assertion-static-cast.md#g2-texturepacker-retains-its-original-document-as-well-as-spritesheet-metadata), with [parallel schema union conversion](compiler-requests/parallel-schema-union-structural-conversion.md#g2-texturepacker-document-converter-is-still-omitted)
- actual first failure at: `flight/spritesheet_formats/texture_packer_parse.hpp:128`
- example: `no matching conversion for static_cast from 'JsonValue' to 'TexturePackerDocument'`

The original one-header check completes at one worker: **0/1 pass, 1 fail, 0 unattempted**.
Both public parse bodies repeat the invalid cast; the same header's `documentToData` is independently
refused at the distinct spritesheet-to-atlas schema union. The real JsonValue tree and checked kind
accessors cannot reinterpret it into generated nominal Ref owners. No runtime conversion or declaration
can invent that owner contract, and a copied view or narrowed domain is rejected.

Source semantics retain array/hash document shape, frame order/names, rotated upright dimensions,
trim/source size/pivot, animations from inclusive frame tags, original image/size/scale and diagnostic
sink/catch boundaries. The document-returning parser exposes the original metadata. Existing native
atlas geometry does not retain that document/tags and uses different scale normalization, so it is not
a substitute parser. Syntax failure still owes its Reject diagnostic and real empty-array sentinel;
valid JSON must not be universally routed there.

Only existing requests and this owned root/row/worklist entry change. No C++, source, binding, repair,
override, manifest or generated inventory changes; no normal generation or global/package/native
baseline rerun is needed. No parser behavior, round-trip, link check or header gain is claimed.
Foreign rows/roots/prose and historical/G1 totals are preserved.

### 148. `static-assertion-18059635` — 1 header(s)

- cause: a runtime static assertion rejects the emitted shape
- lanes: 3D
- fix route: compiler
- first failure at: `flight/structural_ref.hpp:1683`
- example: `static assertion failed due to requirement '!is_void_v<void>': a merged structural row must be projected before native reference recovery`

### 149. `static-assertion-261f0db6` — 1 header(s)

- cause: a runtime static assertion rejects the emitted shape
- lanes: 3D
- fix route: compiler
- first failure at: `flight/structural_ref.hpp:1584`
- example: `static assertion failed: structural row construction has an incompatible value type`

### 150. `undeclared-identifier-0168969a` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `domScene2DClipHooks` binding (`cpp-reference-assertion-without-heritage`, because each hook asserts the shared `RenderState` to `DomRenderState`), but `enable_dom_clip_support` still stores it at line 81. The next callee, `set_dom_clip_hooks` at line 82, belongs to `domClipRectangle.ts`, which is refused as a whole because its private `DomClipStyle extends CSSStyleDeclaration` stops the interface-inheritance pass. Its header is a best-effort placeholder.
- subject: `dom_scene_2d_clip_hooks`
- lanes: 2D
- fix route: compiler
- why this route: a hook table or clip provider would fake clipping. The declaration repair declares only functions (the hooks are a variable), and the placeholder has no NOT GENERATED marker to declare against. The DOM owner needs real heritage or a checked dispatch contract. The clip-path write is also blocked behind the [shared DOM style boundary](compiler-requests/dom-style-shared-element-boundary.md). `scene2d-dom` declares environment `web`. The sibling `canvas_clip.hpp:89` (`undeclared-identifier-7905640c`) has the same shape, and its row is not changed here.
- first failure at: `flight/scene2d_dom/dom_clip.hpp:81`
- example: `use of undeclared identifier 'dom_scene_2d_clip_hooks'`
- Scoped native recheck (GCC 15.2, jobs 1): **1 attempted / 0 pass / 1 fail**, same diagnostic. Nothing is carried.
- compiler request: `docs/compiler-requests/canvas-render-state-owner-asserted-without-heritage.md`
- compiler request: `docs/compiler-requests/local-interface-extending-lib-dom-interface.md` *(new)*

### 151. `undeclared-identifier-07640d32` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glOutlineEffectRunner` binding (and its callee `applyOutlineEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_outline_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: a runtime definition would fake the effect runner. The runner refusal (`cpp-structural-assertion-writable-capability-unproven`) names an owner-preserving carrier as the compiler-side answer. The callee refusal (`cpp-contextual-union-value-type-unrepresented`) is the `Readonly<GlTextureRenderTarget>` to `Readonly<GlRenderTarget> | null` widening of [interface-heritage-widening](compiler-requests/interface-heritage-widening.md). This is the same family as the SSAO runner (cluster 93).
- first failure at: `flight/effects_gl/gl_outline_effect.hpp:96`
- example: `use of undeclared identifier 'gl_outline_effect_runner'`
- Scoped native recheck (GCC 15.2, jobs 1, all errors shown): **1 attempted / 0 pass / 1 fail** at line 96. No other header names either refused symbol. No runner, effect body, shader, cache or narrowed domain is supplied.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 152. `undeclared-identifier-076c02b1` — 1 header(s), declaration repaired

- original cause: `registerTilemapHitTest` passes the refused `resolveTilemapCellIndex` to
  `registerHitTestPrecise`, so the header names a function nothing declares
- lanes: PLAT
- fix route: declaration-only emission repair, with the resolver body still owed
- compiler request: [flattened interface refinement identity](compiler-requests/flattened-interface-refinement-identity.md)

The resolver is refused at `cpp-reference-assertion-without-heritage`: it asserts the `NodeAny` source
to the concrete `Tilemap` and to `Node2D`, whose emitted owners are independent, and it reads the
tilemap graph and bounds. The exact marker, the retained registration and the absence of a definition
agree. Because the resolver is passed as a value rather than called, the `declare-refused-function`
kind does not match it (that kind requires `symbol(`), so the carried repair is
`insert-forward-declaration` keyed on the exact marker, declaring
`double resolve_tilemap_cell_index(flight::types::NodeAny, double, double)` -- the signature the
`HitTestPreciseFunction` carrier expects. No hit result, false-hit sentinel, copied graph, narrowed node
or runtime provider is supplied.

Written by **normal package generation** at the exact pins and matched byte for byte by the repair
engine. Checks at `-j1`: the header **passes**; an unused include compiles, links and runs; a caller of
`register_tilemap_hit_test` compiles and fails to link on exactly `resolve_tilemap_cell_index`, so the
tilemap hit test stays visibly unavailable rather than substituted.

### 153. `undeclared-identifier-09cc9e86` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glFilmGrainEffectRunner` binding (and its callee `applyFilmGrainEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_film_grain_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: a runtime definition would fake the effect runner. The runner refusal (`cpp-structural-assertion-writable-capability-unproven`: `Readonly<Effect>` declares none of `intensity`, `seed` and `size`) names an owner-preserving carrier as the compiler-side answer. The callee refusal is the `GlTextureRenderTarget` widening of [interface-heritage-widening](compiler-requests/interface-heritage-widening.md). This is the same family as the SSAO and outline runners.
- first failure at: `flight/effects_gl/gl_film_grain_effect.hpp:89`
- example: `use of undeclared identifier 'gl_film_grain_effect_runner'`
- Verified by inspection: line 89 is the `register_gl_film_grain_effect` call that passes the refused binding. Nothing in the tree defines either symbol, and no other header names either one. This docs-only disposition changes no compiled input, so the recorded **0/1** failure stands without an unchanged rerun. No runner, effect body, shader or narrowed parameters are supplied.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 154. `undeclared-identifier-0b8d3b44` — G2 recheck of original 1 header

At Flight `a627849` / compiler `806d257`, actual Clang22.1.8 jobs1 completes
**1 attempted / 0 pass / 1 fail / 0 unattempted**,57.1s. The first error remains missing
box_blur_shaders at effects_gl/gl_effect_box_blur.hpp115. Source46 declares one module-scope
WeakMap<GlContext,BoxBlurShaderLocations>; its cpp-weak-map-key-representation-unproven refusal is
already the [bound-interface policy request](compiler-requests/bound-interface-weak-map-key-policy.md#g2-dedicated-gl-box-blur-cache-and-shader-owner).
GlContext maps to the existing WebGl2Context and weak policy, but the compiler consults the external
policy only for ambient key references. This is a refused variable, not an omitted function body.

The retained getter has two further inspected defects: it constructs a derived anonymous shader
record while returning the unrelated flattened named BoxBlurShaderLocations, and drops the source
...base spread (program, attributes and texture uniforms). Those are not measured later native
errors or working-cache evidence. The sibling blit override does not supply this module's cache or
shader owner. No cache-only declaration/override or copied/default program is carried.

Source applyBoxBlurPass110 and applyBlurBlit98 separately refuse the existing
[render-target owner/nullable-row boundary](compiler-requests/interface-heritage-widening.md#g2-gl-box-blur-pass-destination-and-source-recipe):
a plain Ref<GlTextureRenderTarget> must reach drawGlFullscreenPass's Readonly<GlRenderTarget>|null
without replacing the target. The two exact retained-callee declarations cannot supply the missing
cache or correct the getter; [declaration qualification](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-gl-box-blur-refuses-a-cache-before-its-omitted-pass-calls)
is not reached. No unused-positive or exact-negative-link result is claimed.

Existing requests now retain context/cache identity, complete shader fields and source compilation/
uniform/cache order, sigma-derived X/Y passes and original temp/dest ping-pong, radius-zero skip and
tail blit, optional edge-color distinction, exact shader sampling and replacement blend behavior.
No fake blur/shader/cache, storage/operation repair, narrowed numeric/target domain or native GL
execution is supplied. No production source/repair/generation/runtime/binding/override change is
carried. Only this cluster/row/report154 changes; foreign compact products/prose and all historical
sweep/carry totals remain intact. Other headers mentioning this module are not rechecked or promoted.

### 155. `undeclared-identifier-0c07642f` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glGlitchEffectRunner` binding (and its callee `applyGlitchEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_glitch_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: a runtime definition would fake the effect runner. The runner refusal (`cpp-structural-assertion-writable-capability-unproven`: `Readonly<Effect>` declares none of `blockSize`, `colorShift`, `intensity` and `seed`) names an owner-preserving carrier as the compiler-side answer. The callee refusal is the `GlTextureRenderTarget` widening of [interface-heritage-widening](compiler-requests/interface-heritage-widening.md). This is the same family as the SSAO, outline and film grain runners.
- first failure at: `flight/effects_gl/gl_glitch_effect.hpp:92`
- example: `use of undeclared identifier 'gl_glitch_effect_runner'`
- Verified by inspection: line 92 is the `register_gl_glitch_effect` call that passes the refused binding. Nothing in the tree defines either symbol, and no other header names either one. This docs-only disposition changes no compiled input, so the recorded **0/1** failure stands without an unchanged rerun. No runner, effect body, shader, cache or narrowed domain is supplied.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 156. `undeclared-identifier-11d6fcbe` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glChromaticAberrationEffectRunner` binding (and its callee `applyChromaticAberrationEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_chromatic_aberration_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: a runtime definition would fake the effect runner. The runner refusal (`cpp-structural-assertion-writable-capability-unproven`: `Readonly<Effect>` declares neither `intensity` nor `radial`) names an owner-preserving carrier as the compiler-side answer. The callee refusal is the `GlTextureRenderTarget` widening of [interface-heritage-widening](compiler-requests/interface-heritage-widening.md). This is the same family as the SSAO, outline, film grain and glitch runners.
- first failure at: `flight/effects_gl/gl_chromatic_aberration_effect.hpp:88`
- example: `use of undeclared identifier 'gl_chromatic_aberration_effect_runner'`
- Verified by inspection: line 88 is the registration call that passes the refused binding. Nothing in the tree defines either symbol, and no other header names either one. This docs-only disposition changes no compiled input, so the recorded **0/1** failure stands without an unchanged rerun. No runner, effect body, shader, cache or narrowed domain is supplied.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 157. `undeclared-identifier-1462818e` — 1 header, compiler-owned

The original header measured **1 attempted / 0 pass / 1 fail / 0 unattempted**, Clang22.1.8,
jobs **1**, 87.8 seconds, still at `ensure_gl_velocity_program` on line155. The body is refused as
`cpp-contextual-union-inequivalent`: the cache lookup names `Ref<GlVelocityProgram>`, while local
rebind inference names an anonymous owner with its five program/buffer/location fields. The
[existing Map-value carrier request](compiler-requests/contextual-union-map-value-carrier.md#g2-gl-velocity-program-lookup-and-rebind)
records the exact pair. The context WeakMap is independently refused under the
[existing bound-key policy request](compiler-requests/bound-interface-weak-map-key-policy.md).

The existing declaration mechanism matches the exact refusal/live calls/absent body with
`Ref<GlVelocityProgram> ensure_gl_velocity_program(Ref<GlRenderState>)`; loader, one-insertion and
idempotence checks pass in scratch. The actual unused-header candidate **fails compilation**, before
link: `reference to non-static member function must be called` at scratch line410 (stock408),
`gl.get_parameter(gl.viewport)`. The host constant is `viewport_constant`; `viewport` is the method.
The [precise external-property binding request](compiler-requests/external-property-method-name-collision.md)
records that measured boundary and the separately inspected typed-query carrier seam.
The declaration is rejected; no repair, source patch, generated/override/binding/runtime change or
normal generation is carried. There is no positive unused-header link/run or qualified negative-link
result. See the [retained-callee evidence](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-gl-velocity-program-declaration-fails-the-unused-header-compile).

Full velocity behavior remains owed. Traversal is independently refused by a null-presence test on
an unnamed identifier; no exact writer/child variable is inferred from that message. Three writer
bindings refuse erased-to-structural recovery, requiring the same full node/runtime owners under the
[typed callback contract](compiler-requests/readonly-callback-derived-owner-recovery.md): ordinary node
world bounds, particle-specific vectors/transforms/worldSpace and atlas regions, and QuadBatch
per-instance vectors/vector2-or-affine transforms plus only its source-defined coarse fallback.
Preserve invalid/zero skips, rotated regions, signed/subpixel RG velocity in rgba16f, pixelRatio/y-down
mapping, covered A=1 versus clear A=0, kind/child order, and framebuffer/blend/viewport/clear-color
restoration. No fake draw, fixed velocity, copied owner or narrowed kind/traits domain is supplied.

Only this row/cluster, report157 and the named request evidence change. Historical sweep/carry totals
and all foreign worklist rows/clusters stay unchanged. No velocity-data suite, actual GL velocity pass,
old native proof or global sweep was rerun; no renderer behavior gain is claimed.

### 158. `undeclared-identifier-17cb8386` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glBokehDepthOfFieldEffectRunner` binding (and its callee `applyBokehDepthOfFieldEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_bokeh_depth_of_field_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: a runtime definition would fake the effect runner. The runner refusal (`cpp-structural-assertion-writable-capability-unproven`: `Readonly<Effect>` declares none of `focusDistance`, `focusRange` and `maxBlur`) names an owner-preserving carrier as the compiler-side answer. The callee refusal is the `GlTextureRenderTarget` widening of [interface-heritage-widening](compiler-requests/interface-heritage-widening.md). This is the same family as the SSAO, outline, film grain, glitch and chromatic aberration runners.
- first failure at: `flight/effects_gl/gl_bokeh_depth_of_field_effect.hpp:100`
- example: `use of undeclared identifier 'gl_bokeh_depth_of_field_effect_runner'`
- Verified by inspection: line 100 is the registration call that passes the refused binding. Nothing in the tree defines either symbol, and no other header names either one. This docs-only disposition changes no compiled input, so the recorded **0/1** failure stands without an unchanged rerun. No runner, effect body, shader or narrowed parameters are supplied.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 159. `undeclared-identifier-1892fccb` — 1 header, resolved by declaration-only repair

- original subject: `resolve_text_char_index`
- lanes: PLAT
- original failure at: `flight/interaction/register_text_hit_test.hpp:61`
- fix route: existing `declare-refused-function` repair, `interaction-declare-refused-text-char-index`
- retained-callback request: [exact declaration and link boundary](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-interaction-text-hit-test-retains-its-refused-callback)
- body request: [original text/2D owner recovery](compiler-requests/readonly-callback-derived-owner-recovery.md#g2-interaction-text-hit-callbacks-need-the-original-text-and-2d-owners)

The exact refusal marker, two live function-argument uses and absent definition agree. The existing
repair inserts only `double resolve_text_char_index(flight::types::NodeAny source, double x, double y);`,
the precise callback slot's actual carrier. It matches exactly one header and is idempotent. Both
TextLabel and RichText registrations keep their full NodeAny domain; no cross-header witness, engine
extension, callback adapter, shared TextLabel/GL edit or runtime/source/override change is needed.

Normal exact-pin one-module generation reproduces only the declaration (+2 header lines) and its
own manifest repair record:1394 lowered modules,0 complete/1 retained partial-refusal,249 patches,
3 repaired headers; both pins restored clean. The carried header exactly equals the qualified candidate.
The actual unused header compiles, links and runs with static callback/signature checks. Calling the real
two-kind registration compiles to an object, then fails to link on **only** the exact missing callback.
Neither the registration nor the hit-test body runs; no -1/0/character result or native text hit behavior
is supplied. No eager global registration is mistaken for an explicit-use negative link.

The original owned-header jobs1 check completes **1 attempted / 1 pass / 0 fail / 0 unattempted**.
The sole row is pass with `previousCluster`, and the active cluster is removed; no other header is
measured or promoted. The original callback refusal, source/refusal fingerprints, partial marker,
package/global counts and initialization records remain unchanged. Bounds miss -1, no-layout0,
original layout-cache/measure/guard effects, world-to-local transform order and exact character-index
semantics stay obligations of the original owners, not a fake hit or copied graph. Historical/G1 totals,
foreign compact rows/request sections and applicability policy are preserved.

### 160. `undeclared-identifier-1a056bbc` — 3 header(s)

- cause: `getNumberFormat` is refused at typed optional options → erased-object union conversion, but five wrappers still call its absent symbol
- subject: `get_number_format`
- lanes: TEXT
- fix route: compiler — [retained refused callee](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-intl-number-declaration-fails-the-unused-object-prerequisite)
- first production failure at: `flight/intl/number.hpp:42`
- retained production example: `use of undeclared identifier 'get_number_format'`

The original number header and the two rows inherited from `undeclared-identifier-564c29cc`
(`_internal_index.hpp` and `contract.hpp`) remain **0/3 pass, 3 fail**. The previous date provenance
and all three original diagnostics are retained; the unchanged production headers were not rerun.

The existing declaration mechanism matches the exact marker, all five live calls and absent definition
in scratch, using `IntlNumberFormat(LocaleInput, optional<IntlNumberFormatOptions>)`. Serial actual-header
unused-object checks complete **3 attempted, 0 pass, 3 fail, 0 unattempted**: the compact wrapper's bare
notation literal cannot initialize the optional parameter. This is a failed compile prerequisite, not a
positive unused-header link or successful exact negative-link test. The declaration candidate is rejected;
no repair, C++, manifest, runtime/cache body or source patch is carried, so no generation rerun is needed.

Existing requests separately cover the [helper's optional union evidence](compiler-requests/compiler-intl-optional-and-union-carrier-evidence.md#g2-number-formatter-still-loses-the-typed-optional-options-boundary),
[optional literal construction](compiler-requests/compiler-union-and-literal-argument-construction.md#g2-number-declaration-candidate-stops-before-link-qualification)
and [all four dropped caller option spreads](compiler-requests/object-spread-of-a-parameter-dropped.md#g2-intl-number-wrappers-drop-all-caller-option-spreads).
Caller options override the spelled defaults; preserving their full domain, own-property order and getter
effects rules out a default-only overload or copying just the four currently bound runtime fields.
No locale formatting or callable number API progress is claimed.

### 161. `undeclared-identifier-1d8e072a` — 1 header, compiler-owned

The scoped stock header remains **1 attempted / 0 pass / 1 fail / 0 unattempted**, Clang22.1.8,
jobs **1**, 67.3 seconds, at missing `draw_gl_scale9_sprite:168`. The draw body refuses
`cpp-contextual-union-interface-heritage-carrier-unrepresented`: nullable `Material2D` cannot widen
to `Material` while retaining its original allocation. These are independent flattened owners,
despite their matching base fields. This is the existing
[owner-widening request](compiler-requests/compiler-interface-heritage-owner-widening.md#g2-gl-scale9-sprite-material-owner),
not a missing runtime draw to substitute or a texture-dimension refusal.

The retained `.submit = draw_gl_scale9_sprite` is an aggregate field, not a direct call or function
argument accepted by the existing declaration repair. The scratch loader/matcher confirms no direct
match and rejection of that exact aggregate as `retainedReference`; no mechanism is extended.
A separate scratch header adding only the exact source declaration
`void draw_gl_scale9_sprite(Ref<GlRenderState>, Ref<RenderProxy2D>);` still fails the actual unused
include's compilation at `.create_data` (scratch170): native
`optional<ErasedRef>(GlRenderState, Node2D)` cannot construct the generic callback returning
`optional<Ref<RendererData>>` from `(RenderState, Node2D)`. The declaration is rejected before link;
no positive unused-header link/run or qualified negative link was reached. See
[retained aggregate evidence](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-gl-scale9-sprite-retains-a-refused-aggregate-callback).

The [typed backend/data-owner request](compiler-requests/readonly-callback-derived-owner-recovery.md#g2-scale9-sprite-native-sprite-callbacks-are-not-generic-renderer-callbacks)
also records inspected later dirty/submit domains. Native Sprite helpers recover exactly `Sprite`,
whereas the source proxy owns `Scale9Sprite` with its own data/grid/runtime; no adapter may copy it
into a base sprite or claim that exact-owner recovery succeeds. Its nullable texture remains the
full texture family, with only the source's actual 2D/source/positive-size guards; native Sprite and
generated Scale9 texture storage differ and were not silently reconciled.

Faithful drawing must retain the original material/texture/sampler and working-color/alpha realization,
UV matrix and render-target V reflection, grid/mapper/scale behavior, nine row-major affine slices,
post-prepare batch indices, 13-float writes, per-slice material/color/alpha and final count increment.
No fake draw, zero packet, copied material/sprite/runtime or narrowed domain is carried. Only the
owned row/cluster, this report slice and three existing request sections change; historical G1
sweep/carry totals and all foreign products remain unchanged. No normal generation is needed for a
rejected declaration with unchanged production code, and no scale9 behavior or old/global proof was run.

### 162. `undeclared-identifier-2110ec37` — 1 header, compiler-owned

The scoped stock header remains **1 attempted / 0 pass / 1 fail / 0 unattempted**, Clang22.1.8,
jobs **1**, 66.3 seconds, at missing `draw_wgpu_sprite:134`. The source body refuses
`cpp-contextual-union-interface-heritage-carrier-unrepresented` at nullable `Material2D` to `Material`
owner widening during quad-material resolution. Preserve the same material owner/null, original WGPU
state/runtime, proxy, Sprite/data and texture. See the existing
[material owner request](compiler-requests/compiler-interface-heritage-owner-widening.md#g2-wgpu-sprite-material-owner).

The retained `.submit = draw_wgpu_sprite` is an aggregate field, not a direct call or supported
function-argument `retainedReference`. The existing matcher finds no call; the loader rejects the
exact aggregate witness. A separate scratch actual unused include, with only the exact declaration
`void draw_wgpu_sprite(Ref<WgpuRenderState>, Ref<RenderProxy2D>);`, fails compilation at `.create_data`
(candidate136): native `create_sprite_renderer_data(GlRenderState, Node2D)` returns
`optional<ErasedRef>`, while the generated `Scene2DRenderer` slot takes generic RenderState and
returns `optional<Ref<RendererData>>`. See the existing
[typed callback/backend contract](compiler-requests/readonly-callback-derived-owner-recovery.md#g2-wgpu-sprite-retains-gl-specific-native-sprite-helpers).
Dirty/submit state/node/data differences are inspected later, not additional measured first errors.
No positive unused-header link/run or qualified called-function negative-link is reached; the
declaration candidate is rejected, with no repair, normal generation, callback cast or witness extension.
See [retained aggregate qualification](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-wgpu-sprite-retains-its-refused-draw-callback).

Keep the render-pass and null/non-2d/source texture guards. The custom-shader branch must flush the
quad writer, apply the resolved blend hook, realize the same texture in working color/premultiplied
mode, bind the actual shader to the original state/proxy, then return. The default quad branch alone
checks positive view size; preserve registered material lookup/miss reporting and registered standard
fallback, with null meaning no draw. Do not invent a built-in default material or omit custom shaders.
Keep actual texture entry/sampler, resource acquisition, affine transform, all six UV coefficients,
alpha, original materialData and matrix-before-scale-bias fallback. Prepare may flush: use its returned
instanceIndex for packet base, material and color writes, then increment the real writer count once.
Native Sprite dirty data must retain the original texture identity/version and per-state stamp; a
copied generic/GL state or fabricated RendererData cannot provide the WGPU runtime/callback contract.

Only this row/cluster, report162 and appended sections in three existing requests change. Historical
sweep/carry totals and all foreign products remain unchanged. No production code/body, fake shader,
GPU draw proof, narrowed material domain or unchanged old/global check is carried or claimed.

### 163. `undeclared-identifier-2363216f` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glDitherEffectRunner` binding (and its callee `applyDitherEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_dither_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of levels, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_dither_effect.hpp:86`
- example: `use of undeclared identifier 'gl_dither_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 164. `undeclared-identifier-2437e103` — 1 header(s)

- cause: the emitted code names `parse_triangle_geometry_block`, which nothing declares
- subject: `parse_triangle_geometry_block`
- lanes: 3D
- fix route: compiler
- first failure at: `flight/scene3d_formats/awd2_geometry_handler.hpp:325`
- example: `use of undeclared identifier 'parse_triangle_geometry_block'`
- compiler requests: [typed-array static `from`](compiler-requests/typed-array-static-from-binding.md),
  [typed array into a SequenceView union](compiler-requests/typed-array-into-sequence-view-union.md)

`parseTriangleGeometryBlock` is refused, but the module still emits `awd2_triangle_geometry_handler`,
whose parse lambda calls it. Two compiler causes were measured in order. The first refusal is
`Uint32Array.from` with no C++ binding; a faithful `new Uint32Array(indices)` rewrite (the equivalence
already carried for `skeleton2d-formats`) was tried as a source patch and moved the refusal to the
next cause instead of emitting. The second is `cpp-contextual-union-inequivalent`: the typed-array
alternative has no exact carrier for the destination's `SequenceView<double>` contextual-union
alternative, even though the runtime's `SequenceView(TypedArray)` constructor performs exactly that
conversion outside a union. Both are compiler boundaries; the source patch was not carried because it
does not unblock the module.

The declaration-only candidate was then measured and rejected. It compiles the header, and a direct
call fails to link on exactly `parse_triangle_geometry_block`, but the required unused-header positive
link fails: the retained `inline` `awd2_triangle_geometry_handler` initializer binds the parse lambda,
so every including translation unit references the absent symbol — the same failed prerequisite
recorded for the MD5 and MD2 families in
[retained callers](compiler-requests/retained-declaration-calls-omitted-callee.md). No repair, override
or substitute parser is carried; the row stays failed.

### 165. `undeclared-identifier-251d1ba2` — G2 recheck of 1 header(s)

- cause: retained reveal recipe calls omitted collect_pending_texture_owners
- lanes: 3D
- fix route: [existing retained-callee request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-scene3d-resource-reveal-collector-declaration-fails-before-link)
- actual first failure at: `flight/scene3d_resources/reveal_scene_3d_resources_on_resolve.hpp:167`

Original1 Clang22.1.8 jobs1 completes in44.7s: **1 attempted / 0 pass / 1 fail / 0 unattempted**.
The exact refusal marker/direct call and existing readonly scene/resolver plus shared owner Map/Array
carriers qualify the scratch prototype mechanism; fixed point/removal restore original bytes. Actual
unused whole-header compile exits1 at the retained tween call (candidate183/original181): whole optional
ease owner passed to a row constructor, then reversed manager/target variant. Positive unused link/run
and qualified called-reveal negative link are not reached. Candidate rejected; no production repair,
C++/manifest/runtime/source/binding/override or changed normal generation is carried.

The body needs [readonly-row erasure with retained native owner](compiler-requests/structural-row-erasure-to-unknown.md#g2-resource-reveal-keeps-scene-resource-and-callback-identity).
The candidate measures [optional ease-owner projection](compiler-requests/structural-subtype-reference-argument-owner.md#g2-scene3d-reveal-optional-ease-owner-projection)
and [canonical union order](compiler-requests/canonical-union-carrier-order.md#g2-scene3d-reveal-tween-manager-argument).
Selected-overload/NumericProps literal obligations are inspected only, not isolated diagnostics.

Preserve root/descendant/material/lister order, every Texture/resource/original-node owner, duplicate
and shared texture tracking, collection-before-alpha-before-signals, all-pending settlement, caller
manager/ease/duration, same-slot resolved/failed connection and disposal order. The first-texture
comment is stale; body/tests wait until pending.size is0. Source tests are inspected requirements;
no native fade/provider/resource/scene behavior is run. No fake visibility/empty collector, copied
owner or narrowed material/texture/graph domain. Only owned failed root/row/worklist plus four existing
request appendices change; foreign products/prose/pins/generation metadata and C++ remain unchanged.

### 166. `undeclared-identifier-2d9cd75f` — 1 header, resolved by declaration-only repair

- original subject: `hit_test_shape_fill`
- lanes: PLAT
- original failure at: `flight/interaction/register_shape_hit_test.hpp:66`
- fix route: existing `declare-refused-function` repair, `interaction-declare-refused-shape-fill-hit`
- retained-callback request: [exact declaration and link boundary](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-interaction-shape-hit-test-retains-its-refused-callback)
- body request: [original shape-family owners and fill semantics](compiler-requests/readonly-callback-derived-owner-recovery.md#g2-interaction-shape-fill-hits-need-the-original-shape-family-owners)

The exact refusal marker, three live function-argument uses and absent definition agree. The existing
repair inserts only `double hit_test_shape_fill(flight::types::NodeAny source, double x, double y);`,
the actual precise callback carrier. It matches exactly one header and is idempotent. The retained
MorphShape/Shape/Scale9Shape registrations keep the same NodeAny domain; no type adapter, engine
extension, source/runtime/override body, shared graph/shape/bounds or tilemap-hit edit is carried.

Normal exact-pin one-module generation reproduces only the declaration (+2 header lines) and its
own manifest repair record. The carried header exactly equals the real qualified candidate. The actual
unused header compiles, links and runs with static slot/signature checks. Calling the real three-kind
registration compiles to an object, then fails to link on **only** the exact missing callback. Neither
registration nor the fill hit-test body runs; no native shape hit/miss or winding result is supplied.
There is no eager global registration being misreported as an explicit-use negative link.

The original owned-header jobs1 check completes **1 attempted / 1 pass / 0 fail / 0 unattempted**.
The sole row is pass with `previousCluster`, and the active cluster is removed; no other header is
measured or promoted. The callback's NodeAny-to-Shape refusal, partial marker, source/refusal
fingerprints, package/global counts, initialization and applicability policy remain unchanged.
The body must retain original shape/data/command/path owners, all three registered shape kinds,
null-versus-empty region behavior, winding and transform/scratch/read effects. Unsupported non-solid
regions return -1 before transforming; a non-null list transforms before testing regions in order,
returning0 only on a path hit. No false/constant hit, copied graph or narrowed command domain substitutes
for those operations. Foreign compact rows, request sections and historical/G1 totals are preserved.

### 167. `undeclared-identifier-3017e8aa` — G2 recheck of original 1 header

At Flight `a627849` / compiler `806d257`, stock Clang22.1.8 jobs1 completes
**1 attempted / 0 pass / 1 fail / 0 unattempted** in102.9s at missing acquire_gl_render_texture136.
The existing exact NOT GENERATED/direct-call repair now supplies only its source-faithful prototype.
Normal exact-pin generation produces the same header bytes as the qualified candidate and adds only
its own applied-repair manifest entry. Actual unused-header compile/link/run passes. Instantiating the
real retained with_gl_render_textures<double> compiles to an object; linking fails on **only** the
exact missing acquire body. Acquisition, callbacks, GL caches and pool operations are not executed.

The final original1 scoped header check completes **1 attempted / 1 pass / 0 fail / 0 unattempted**
in124.4s, jobs1. The owned row passes with previousCluster; only this active cluster is removed.
This is header/declaration availability. A separate actual with_gl_render_textures<void> instantiation
fails at the retained std::optional<T> finally-return slot (header134): optional<void> is ill-formed.
The generic source domain is retained, and the honest overall attestation records this compiler gap.

The existing [contextual union owner request](compiler-requests/contextual-union-value-arm-member-shape.md#g2-gl-render-texture-pool-keeps-the-original-render-texture-owner)
records the refused acquisition, exact GL-context ownership, original pooled RenderTarget and cache
identity, descriptor presence/copy/version semantics, LIFO leases and release/destroy error order.
The [retained-callee evidence](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-gl-render-texture-pool-acquisition-declaration-qualified)
and [void-finally request](compiler-requests/void-return-inside-try-finally.md#g2-generic-gl-render-texture-bracket-with-void-result)
separate the measured boundaries. No body, replacement target, provider, fake result, copied owner,
new storage/cache/operation or narrower descriptor/result domain is supplied. Foreign compact rows,
clusters/report prose and all historical sweep/carry totals are preserved.

### 168. `undeclared-identifier-332a5586` — G2 recheck of 1 header(s)

- cause: the emitter refuses `drawDomScale9Sprite` (`cpp-union-runtime-domains-erased`: six image domains map to one `host_sdl::ImageSource` carrier), but keeps `.submit = draw_dom_scale9_sprite` in the inline `dom_scale9_sprite_renderer` aggregate.
- subject: `draw_dom_scale9_sprite`
- lanes: 2D
- fix route: compiler
- why this route: a scratch copy with only the exact declaration `void draw_dom_scale9_sprite(Ref<DomRenderState>, Ref<RenderProxy2D>)` still fails at `.create_data`. There, `create_dom_scale9_sprite_data` returns `Ref<DomScale9SpriteData>` from `NodeAny`, and the slot needs `optional<Ref<RendererData>>` from `Node2D`. Neither link check is reachable, so no repair is carried. This is the same shape as the GL Scale9 sprite. `scene2d-dom` declares environment `web`.
- first failure at: `flight/scene2d_dom/dom_scale9_sprite.hpp:149`
- example: `use of undeclared identifier 'draw_dom_scale9_sprite'`
- Scoped native recheck (GCC 15.2, jobs 1, all errors shown): **1 attempted / 0 pass / 1 fail**, the header's own diagnostic. No DOM backend, draw, copied owner or narrowed image domain is supplied.
- compiler request: `docs/compiler-requests/retained-declaration-calls-omitted-callee.md`

### 169. `undeclared-identifier-34adf2ab` — G2 recheck of 1 header(s)

- cause: retained section handler calls the omitted parseSpineSlots and reads its omitted complete anonymous result
- subject: `parse_spine_slots`
- lanes: ANIM
- fix route: [retained callee/result contract](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-spine-json-slots-require-a-complete-result-owner-as-well-as-the-refused-callee), with [erased input/slot semantics](compiler-requests/erased-array-element-reading.md#g2-spine-json-slots-keep-aligned-outputs-and-original-bone-reads)
- actual first failure at: `flight/skeleton2d_formats/spine_json_slots_handler.hpp:81`
- example: `use of undeclared identifier 'parse_spine_slots'`

Original1 completes at jobs1: **0/1 pass, 1 fail, 0 unattempted**. The refused helper needs full unknown
array/entry recovery, original bone reads/first match, aligned name/slot order, null initial attachment,
source color conversion and appending into the same context arrays. No fake slot/parser, copied owner,
new validation, universal empty success or narrowed JSON domain is supplied.

The exact two-array Ref result exists in the binary-slot header with matching source members. Existing
declaration loader/engine with that complete-owner include matches the owned header once/idempotently;
no result definition or body is copied. Actual unused JSON-header object compile fails at the included
binary result-owner module's optional Record diagnostic71. Candidate rejected before positive link/run
or exact-negative link, neither claimed. The body and reader-alias linkage remain unavailable obligations.

No repair, source, C++, override, manifest or inventory change and no normal generation is needed.
Only this owned failed root/row and worklist entry change, plus existing request appendices. No native
slot/parser/functionality/header gain is claimed; foreign rows/roots/prose and historical/G1 totals
remain unchanged.

### 170. `undeclared-identifier-3582eeb5` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glScreenSpaceFogEffectRunner` binding (and its callee `applyScreenSpaceFogEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_screen_space_fog_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of color, density, far and near, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_screen_space_fog_effect.hpp:98`
- example: `use of undeclared identifier 'gl_screen_space_fog_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 171. `undeclared-identifier-3cb2bb54` — 1 header(s)

- cause: the emitted code names `invert_tint_shaders`, which nothing declares
- subject: `invert_tint_shaders`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_effect_tint_shader.hpp:75`
- example: `use of undeclared identifier 'invert_tint_shaders'`

### 172. `undeclared-identifier-3e7f03c4` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glSharpenEffectRunner` binding (and its callee `applySharpenEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_sharpen_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of amount, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_sharpen_effect.hpp:86`
- example: `use of undeclared identifier 'gl_sharpen_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 173. `undeclared-identifier-44058981` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glBlendEffectRunner` binding (and its callee `applyBlendEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_blend_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of backdropKey, mode and opacity, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_blend_effect.hpp:119`
- example: `use of undeclared identifier 'gl_blend_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 174. `undeclared-identifier-49b86813` — 1 header, resolved by declaration-only repair

- original subject: `solve_skeleton_2d_ik_constraint`
- lanes: ANIM
- original failure at: `flight/skeleton2d/ik_constraint_2d.hpp:205`
- fix route: existing `declare-refused-function` repair, `skeleton2d-declare-refused-ik-constraint-solver`
- retained callback: [exact declaration and link boundary](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-skeleton2d-ik-registration-retains-its-refused-solver)
- body request: [original constraint and pose owner recovery](compiler-requests/assertion-to-undeclared-row-member.md#g2-ik-constraint-dispatch-must-retain-the-original-constraint-and-pose-owners)

The exact refusal marker, live direct function-argument registration and absent definition agree.
The existing retainedReference repair declares only void(Ref<Skeleton2D>, readonly Skeleton2DConstraint
row), exactly the emitted Skeleton2DConstraintSolver slot. It matches one header and is idempotent.
The original mutable skeleton Ref and full readonly base constraint ABI remain; no IK-only callback,
closed kind union, adapter, cross-header list, repair-engine extension or replacement owner is added.

Normal exact-pin one-module generation plus the existing derived structural-alias rule reproduces
only the declaration (+2 header lines) and own repair record. The raw narrow closure omits Screen,
so the existing rule, applied only to this header and its committed canonical Screen owner, restores
the already-carried x_y alias/include. The owner and alias provenance stay unchanged; the result
exactly equals the tested real candidate. The actual unused
header compiles, links and runs with static solver-slot/signature checks. Calling the actual opt-in
registration compiles to an object, then fails to link on **only** the exact omitted IK solver signature.
There is no eager global registration being mistaken for this explicit-use negative link. Neither
registration nor the full constraint body runs, and no native IK/pose result is supplied.

The original owned-header jobs1 check completes **1 attempted / 1 pass / 0 fail / 0 unattempted**.
The sole row is pass with `previousCluster`; the active cluster is removed, with no other header
measured or promoted. The helper bodies, partial marker, original row-owner refusal, source/refusal
fingerprints, package/global counts, initialization and applicability policy remain unchanged.
Existing requests retain the open solver registry and original constraint, bone and Float32 owners;
parent-space inversion/range/degeneracy checks, one/two-bone math, both cosine clamps, signed angle
wrap, original mix/bend/stretch/compress and parent-before-child matrix refresh; the caller's ordered
pass and later descendant propagation remain. No fake constraint, empty solver/result, copied pose
owner, narrowed numeric/family domain or source/runtime/override body is carried. Foreign raw
registry/worklist/request data and historical/G1 totals are preserved.

### 175. `undeclared-identifier-4fc6ab69` — declaration repaired (1 header rechecked)

Original1 jobs1, Clang22.1.8: **0/1 pass**, missing create_svg_image_node128. Source24 refuses
the existing Sprite owner at the nullable Node2D contextual member-shape boundary. The exact
source/retained readonly XML/style/context carriers and optional<Node2D> result match the emitted
Node2D alias and sibling SVG helper parameters; no nominal/base/result owner is invented.

Existing `declare-refused-function` repair `scene2d-formats-declare-refused-svg-image-node`
inserts one exact prototype after checking marker/live call/no definition. One owned-header match
and fixed point pass; removing the prototype recovers original bytes. Actual unused owned-header
compile/link/run passes. Using the real retained handler compiles, then links fail exactly on the
absent helper/signature; no native image behavior or called-handler execution is claimed.

Normal one-module generation reproduces the complete tested header and applied-repair record;
final owned1 jobs1 is **1 attempted / 1 pass / 0 fail / 0 unattempted**. The failed root is removed,
the row passes with previousCluster, and this worklist entry is updated. Partial/refused image body,
module/package totals, foreign rows/roots/prose and historical/G1 counts remain unchanged.

The [retained-callee request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-svg-image-handler-retains-its-refused-image-helper)
and [existing body-owner request](compiler-requests/contextual-union-value-arm-member-shape.md#g2-svg-image-preserves-the-original-sprite-resource-and-geometry)
record full resolver/Drop diagnostics, original ImageResource/texture/Sprite identity, numeric input,
aspect ratio and author*geometry transforms, appearance and intrinsic objectBoundingBox before clip
dispatch. No fake image/decode/SVG parser, copied owner or narrowed source domain is supplied.

### 176. `undeclared-identifier-514a5b7b` — 1 header, compiler-owned

The scoped stock header remains **1 attempted / 0 pass / 1 fail / 0 unattempted**, Clang22.1.8,
jobs **1**, 34.9 seconds, at missing `submit_gl_tilemap:165`. The source body refuses
`cpp-contextual-union-interface-heritage-carrier-unrepresented` at nullable `Material2D` to `Material`
owner widening during quad-material resolution. Preserve the proxy's original material and selected
renderer; a base-prefix copy or default material loses that identity. See the existing
[material owner request](compiler-requests/compiler-interface-heritage-owner-widening.md#g2-gl-tilemap-material-owner).

The retained `.submit = submit_gl_tilemap` is an aggregate field, not a direct call or supported
function-argument `retainedReference`. The existing matcher finds no call and the loader rejects the
exact aggregate witness. A separate scratch actual unused include, with only the exact declaration
`void submit_gl_tilemap(Ref<GlRenderState>, Ref<RenderProxy2D>);`, fails compilation at `.create_data`
(candidate167): `noop_renderer_data(RenderState, NodeAny)` cannot initialize `SpriteRenderer`'s
callback taking `(RenderState, Sprite)`. See the existing
[callback owner request](compiler-requests/interface-heritage-widening.md#g2-gl-tilemap-create-data-callback).
No positive unused-header link/run or qualified called-function negative-link is reached. The candidate
is rejected; no repair, normal generation, callback cast or new witness is carried.
[Retained aggregate evidence](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-gl-tilemap-retains-its-refused-submit-callback)
also records the later inspected generic RenderState versus GLstate submit domain.

The proxy must retain its full Tilemap/data, atlas/texture, signed `Int16Array` tiles, per-cell
materialData and real GL writer runtime. Preserve atlas/texture/source and zero-row/column guards,
material selection, working color, sampler, blend, straight alpha and proxy alpha. Visit cells in
row-major order using `row * columns + col`; skip negative/out-of-range ids and nonpositive regions.
Compose the parent affine transform with `(col * tileWidth, row * tileHeight)`. Geometry always uses
`tileWidth`/`tileHeight`; atlas dimensions and both 90-degree rotations affect UVs, not those sizes.
After prepare reserves `columns * rows` and may flush, write compact valid packets/material/color at
its returned start plus `drawCount`; per-cell color still reads the original input cell index. Keep
ColorScaleBias/Tint/readonly-number-array override, then node matrix, then node scale-bias fallback,
the instance stride and final writer-count increment by valid draws. No fake packets or narrowed tiles.

Only this row/cluster, report176 and appended sections in three existing requests change. Historical
sweep/carry totals and foreign products remain unchanged. No production source/runtime/override/body,
native packet/drawing proof or unchanged old/global check is carried or claimed.

### 177. `undeclared-identifier-51763a3c` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glToneMapEffectRunner` binding (and its callee `applyToneMapEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_tone_map_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of exposure, operator and white, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_tone_map_effect.hpp:96`
- example: `use of undeclared identifier 'gl_tone_map_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 178. `undeclared-identifier-529e49d7` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glDirectionalBlurEffectRunner` binding (and its callee `applyDirectionalBlurEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_directional_blur_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of angle, length and samples, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_directional_blur_effect.hpp:91`
- example: `use of undeclared identifier 'gl_directional_blur_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 179. `undeclared-identifier-58fb713b` — G2 recheck of 1 header(s)

- cause: the emitter refuses `drawDomScale9Shape` (`cpp-contextual-union-missing-expression-type:optionalSingle`; the refusal does not name the expression), but keeps `.submit = draw_dom_scale9_shape` in the inline `dom_scale9_shape_renderer` aggregate.
- subject: `draw_dom_scale9_shape`
- lanes: 2D
- fix route: compiler
- why this route: a scratch copy with only the exact declaration `void draw_dom_scale9_shape(Ref<DomRenderState>, Ref<RenderProxy2D>)` still fails at `.create_data`. There, `create_dom_scale9_shape_data` returns `Ref<DomScale9ShapeData>` from `NodeAny`, and the slot needs `optional<Ref<RendererData>>` from `Node2D`. Neither link check is reachable, so no repair is carried. This is the same shape as the DOM Scale9 sprite. `scene2d-dom` declares environment `web`.
- first failure at: `flight/scene2d_dom/dom_scale9_shape.hpp:151`
- example: `use of undeclared identifier 'draw_dom_scale9_shape'`
- Scoped native recheck (GCC 15.2, jobs 1, all errors shown): **1 attempted / 0 pass / 1 fail**, the header's own diagnostic. No DOM backend, raster, copied owner or narrowed shape is supplied.
- compiler request: `docs/compiler-requests/retained-declaration-calls-omitted-callee.md`

### 180. `undeclared-identifier-5be7be47` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glScanlinesEffectRunner` binding (and its callee `applyScanlinesEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_scanlines_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of count and intensity, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_scanlines_effect.hpp:87`
- example: `use of undeclared identifier 'gl_scanlines_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 181. `undeclared-identifier-6647740e` — G2 recheck of 1 header(s)

- cause: retained clipping pass calls the omitted create_rive_relative_transforms
- lanes: 2D
- fix route: [existing retained-callee request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-rive-clipping-declaration-does-not-pass-the-unused-header)
- actual first failure at: `flight/scene2d_formats/rive_clipping.hpp:207`

Original1 Clang22.1.8 jobs1 completes in59.5s: **1 attempted / 0 pass / 1 fail / 0 unattempted**.
The exact refused marker, direct call and readonly artboard/Matrix-array carriers qualify an existing
mechanism scratch prototype, with fixed point and removal restoring original bytes. Actual unused
whole-header compile exits1 at target.value().has_value() on a DisplayObject payload (original224),
then bare optional Record detail (original231). Positive unused link/run and qualified negative link
are not reached; candidate rejected. No production repair/C++ or changed normal generation is carried.

The helper's sparse scratch needs [graph initialization proof](compiler-requests/dense-array-length-initialization-proof.md#g2-rive-relative-transforms-resolve-parent-chains-before-reading).
The following [nullable indexed binding](compiler-requests/nullable-array-element-null-test.md#g2-rive-clipping-conditional-binding-has-one-optional-layer)
and [optional Record](compiler-requests/optional-record-literal-not-materialized.md#g2-rive-clipping-diagnostic-detail)
errors are measured in the candidate only. Registration independently refuses the represented
[contextual callable signature](compiler-requests/callable-arm-in-contextual-union.md).

Full-graph parent ordering/errors and Matrix identity, inverse-clipped/source transfer with singular
fallback, numeric property defaults, all path commands/contours, per-clip fill normalization, original
node Map keys, missing-source diagnostics and deferred clip assignments remain required. Existing
Matrix/Path/ClipRegion/kernel owners are inspected; source tests are requirements, not native geometry
results. No fake clip/path/parser, copied owner, no-op, narrowed graph/path or successful clipping claim.
Only owned failed root/row, worklist entry and existing request appendices change. Foreign products,
prose, generation metadata, pins and runtime/source/C++ bytes remain unchanged; functionality gain0.

### 182. `undeclared-identifier-66ea97f8` — 1 header, compiler-owned

The scoped stock header remains **1 attempted / 0 pass / 1 fail / 0 unattempted**, Clang22.1.8,
jobs **1**, 10.9 seconds, at missing `initialize_wgpu_screen_render_target:151`. The initializer
refuses `cpp-union-runtime-domains-erased`: `GPUTextureFormat | primitive` both map to `flight::String`
at the format fallback. The [existing same-carrier union request](compiler-requests/distinct-union-domains-one-carrier.md#g2-wgpu-screen-target-format-fallback)
records this bound scalar-alias case; no closed format subset or hardcoded option is substituted.

The existing declaration repair qualifies the exact refusal marker, retained factory call and absent
definition in one header, reaches a fixed point, and removal restores original bytes. Its signature
uses `EntityConstruction<Ref<WgpuScreenRenderTarget>>`, readonly structural host/surface rows, native
WgpuDevice and optional readonly options with the original default. A scratch actual unused header
with only that declaration still fails compilation at candidate159 (stock157): the depth-stencil
resize helper reads `texture.width`, absent from native `WgpuObject<WgpuTextureTag>`. `height` is also
absent by inspection, not a second measured first error. See the new
[native texture size-property request](compiler-requests/webgpu-texture-size-properties.md) and
[retained declaration qualification](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-wgpu-screen-target-initializer-declaration-cannot-pass-the-unused-header).
No positive unused-header link/run or real-factory negative object/link is reached. The candidate is
rejected; no declaration repair, normal generation, copied target or fake GPU provider is carried.

Preserve the original construction owner, host/device/drawable/options, attach result's exact context
and live surface owner, default format/alpha/color-space and exact attach-null error. Initial storage
uses clamped surface dimensions and real depth-stencil texture/view; capture/antialias hooks and
resources retain their initial null/false/zero values, attachment count1 and sampleCount1. Each pass
resyncs live extent (2x per axis for antialias), then checks old texture size and destroys/replaces it
on mismatch. Capture acquisition precedes swap-chain fallback; presentation views last one frame,
antialias view selection follows, and screen targets remain distinct from sampleable texture targets.
Destroy must preserve source resource/null-write order and context unconfigure, without inventing
device/host-acquisition release. Bind and destroy remain independently refused at optional-call
expression/callable-result evidence; native GPU creation/view methods are explicitly unavailable.

Only this row/cluster, report182, two existing request appendices and the new request/index change.
Historical sweep/carry totals and all foreign products remain unchanged. No attachment, resize,
capture, presentation, GPU lifecycle behavior or unchanged old/global check is claimed.

### 183. `undeclared-identifier-67f2c521` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glGodRaysEffectRunner` binding (and its callee `applyGodRaysEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_god_rays_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of centerX, centerY, decay, density, exposure, samples and weight, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_god_rays_effect.hpp:106`
- example: `use of undeclared identifier 'gl_god_rays_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 184. `undeclared-identifier-6ad30a3d` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glCrtEffectRunner` binding (and its callee `applyCrtEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_crt_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of aberration, curvature, scanlineIntensity and vignette, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_crt_effect.hpp:92`
- example: `use of undeclared identifier 'gl_crt_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 185. `undeclared-identifier-6dd20ce4` — 1 header, compiler-owned

The scoped stock header remains **1 attempted / 0 pass / 1 fail / 0 unattempted**, Clang22.1.8,
jobs **1**, 67 seconds, at missing `submit_gl_quad_batch:191`. Submit refuses
`cpp-contextual-union-interface-heritage-carrier-unrepresented` at nullable `Material2D` to `Material`
owner widening during quad-material resolution. The original material cannot be retagged, copied to
a base prefix or replaced by a default. See the existing [material owner request](compiler-requests/compiler-interface-heritage-owner-widening.md#g2-gl-quadbatch-material-owner).

The retained `.submit = submit_gl_quad_batch` is an aggregate field, not a direct call or supported
function-argument `retainedReference`. The existing matcher finds no call and the loader rejects that
exact aggregate witness. A separate scratch include with only the exact source declaration
`void submit_gl_quad_batch(Ref<GlRenderState>, Ref<RenderProxy2D>);` fails compilation at `.create_data`
(candidate193): `noop_renderer_data(RenderState, NodeAny)` cannot initialize the
`SpriteRenderer` callback taking `(RenderState, Sprite)`. This is the existing
[callback owner conversion](compiler-requests/interface-heritage-widening.md#g2-gl-quadbatch-create-data-callback),
not a positive link or called-function negative-link result. The declaration is rejected; no repair,
normal generation, callback cast or new witness is carried. [Retained aggregate evidence](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-gl-quadbatch-retains-its-refused-submit-callback)
also records the later inspected generic RenderState versus GLstate submit domain.

The source proxy must retain its complete QuadBatch and original atlas, ids/transforms/materialData
arrays and GL writer runtime. A Sprite copy or narrowed transform/color/atlas subset cannot replace
that owner. Preserve actual atlas/texture/source/count guards, texture/sampler/working-color/alpha,
vector2 and affine parent composition, both 90-degree region rotations, UV and width/height handling,
id-range/nonpositive-region skips, and compact valid-instance packet order. `prepareGlQuadBatchWrite`
may flush; material/color writes must use its returned start plus `drawCount`, while source per-quad
color lookup still uses input index `i`. Keep per-quad color/Tint/number-array override, then node
color-matrix, then node scale-bias fallback, original materialData references/alpha, stride and final
count increment by actual valid draws, not reserved instanceCount. No fake packets or draw behavior.

Only this row/cluster, report185 and appended sections in three existing requests change. Historical
sweep/carry totals and all foreign products remain unchanged. No positive unused-header link/run,
qualified negative link, new source/runtime/override/body or native packet/drawing proof is claimed;
no unchanged old/global checks were rerun.

### 186. `undeclared-identifier-757c2c8a` — 1 header, compiler-owned

The scoped stock header remains **1 attempted / 0 pass / 1 fail / 0 unattempted**, Clang22.1.8,
jobs **1**, 53.5 seconds, at missing `get_gl_scale9_shape_data:114`. The private getter refuses
`cpp-reference-assertion-without-heritage`: generic `RendererData` does not retain a checked
`GlScale9ShapeData` owner. The [existing refinement request](compiler-requests/flattened-interface-refinement-identity.md#g2-gl-scale9-shape-private-raster-data)
records the original allocation, eight extra raster/cache cells and same-owner lifecycle requirement.
The already-carried Scale9Shape factory/runtime module is separately partial; no replacement runtime
or reuse of mesh-only shape data closes the private raster owner boundary.

The existing exact declaration mechanism qualifies
`Ref<GlScale9ShapeData> get_gl_scale9_shape_data(Ref<RendererData>);` once and idempotently under the
marker/direct-call/absent-body witnesses. The actual unused-header candidate then **fails compilation**
at scratch121 (stock119): native `WeakMap<TextureSourceValue, Entry>` cannot initialize emitted
`WeakMap<Ref<TextureSource>, Entry>`. The [new cache-local carrier request](compiler-requests/native-texture-source-cache-local-carrier.md)
records this native-slot versus generated-consumer representation seam, not a missing permissive
Map constructor. The declaration is rejected before link; no positive unused-header link/run,
qualified negative-link or normal generation is claimed. See [retained getter/draw evidence](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-gl-scale9-shape-getter-declaration-stops-at-the-cache-carrier).

The draw body independently refuses a null-presence test on an unnamed property; its ledger does not
prove which field, so none is guessed. The retained mask wrapper and global renderer still reference
the refused draw. Backend callback and full shape/runtime owners need the existing
[typed owner contract](compiler-requests/readonly-callback-derived-owner-recovery.md), not a copied
Shape, RendererData, texture source or surface. The current native shape-data lane supplies mesh
lifecycle and explicitly refuses raster teardown; it is not a Scale9 raster implementation.

Preserve registered rasterizer dispatch/miss reporting, mapper-null delegation, original commands/grid
and bounds/content revision, pixelRatio/size/scale invalidation, remapped-command replay, pixel-space
transform/reset, same-host surface acquisition/reuse/release, Image-keyed GPU eviction before release,
resize failure/absence, actual image invalidation, stripped-scale matrix and real textured quad/mask.
No fake raster/draw, empty success, copied owner or narrowed shape/key domain is carried. Only this
row/cluster, report186 and named request evidence change; all foreign products and historical
sweep/carry totals remain unchanged. No old native/rendering proof or global sweep was rerun.

### 187. `undeclared-identifier-785ed669` — 1 header(s), declaration repaired

- original cause: `registerQuadBatchHitTest` passes the refused `resolveQuadBatchInstanceIndex` to
  `registerHitTestPrecise`, so the header names a function nothing declares
- lanes: PLAT
- fix route: declaration-only emission repair, with the resolver body still owed
- compiler request: [flattened interface refinement identity](compiler-requests/flattened-interface-refinement-identity.md)

The resolver is refused at `cpp-reference-assertion-without-heritage`: it asserts the `NodeAny` source
to the concrete `QuadBatch` and to `Node2D`, whose emitted owners are independent, and it reads the
atlas, ids, transforms and per-instance bounds. The resolver is passed as a value, so the carried repair
is `insert-forward-declaration` keyed on the exact marker, declaring
`double resolve_quad_batch_instance_index(flight::types::NodeAny, double, double)` -- the
`HitTestPreciseFunction` carrier's shape. No hit index, false-hit sentinel, copied quad-batch graph,
narrowed node/domain or runtime provider is supplied.

Written by **normal package generation** at the exact pins and matched byte for byte by the repair
engine. Checks at `-j1`: the header **passes**; an unused include compiles, links and runs; a caller of
`register_quad_batch_hit_test` compiles and fails to link on exactly `resolve_quad_batch_instance_index`,
so the quad-batch hit test stays visibly unavailable rather than substituted.

### 188. `undeclared-identifier-7905640c` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `canvasClipHooks` binding (`cpp-reference-assertion-without-heritage`, because each hook starts with `state as CanvasRenderState`), but `enable_canvas_clip` still stores it at line 89.
- subject: `canvas_clip_hooks`
- lanes: 2D
- fix route: compiler
- why this route: `pop_canvas_clip_rectangle`, `push_canvas_clip_rectangle` and `push_canvas_clip_contours` are all emitted in `canvas_clip_rectangle.hpp` with `Ref<CanvasRenderState>`. The only gap is recovering the Canvas owner from the shared `RenderState`, which needs heritage or a checked dispatch contract. A hook table would fake clipping, and the declaration repair declares only functions. This is the same shape as the DOM clip hooks (cluster 150).
- first failure at: `flight/scene2d_canvas/canvas_clip.hpp:89`
- example: `use of undeclared identifier 'canvas_clip_hooks'`
- Scoped native recheck (GCC 15.2, jobs 1, all errors shown): **1 attempted / 0 pass / 1 fail** at the header's own line 89, not a cascade from an included header. Nothing is carried.
- compiler request: `docs/compiler-requests/canvas-render-state-owner-asserted-without-heritage.md`

### 189. `undeclared-identifier-7d85a310` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glLensFlareEffectRunner` binding (and its callee `applyLensFlareEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_lens_flare_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of ghosts, halo, intensity and threshold, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_lens_flare_effect.hpp:91`
- example: `use of undeclared identifier 'gl_lens_flare_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 190. `undeclared-identifier-7e3a780f` — 1 header, resolved by declaration-only repair

- original subject: `hit_test_sprite_alpha`
- lanes: PLAT
- original failure at: `flight/interaction/register_sprite_hit_test.hpp:127`
- fix route: existing `declare-refused-function` repair, `interaction-declare-refused-sprite-alpha-hit`
- retained caller: [exact direct-call declaration and link boundary](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-interaction-sprite-alpha-hit-retains-its-refused-helper)
- body request: [original node and pixel-source owners](compiler-requests/readonly-callback-derived-owner-recovery.md#g2-interaction-sprite-alpha-hits-need-the-original-node-and-pixel-source-owners)

The exact refusal marker, live direct same-header lambda call and absent definition agree. The existing
repair declares the actual readonly HostBitmapReadbackCapability row, NodeAny and present double x/y/
threshold parameters with a double result. It matches one header and is idempotent; neither a callback
reference witness nor cross-header caller list/engine extension is needed. The wrapper's default1,
optional threshold, readonly-host capture and complete NodeAny/Texture/source/numeric domains stay.

Normal exact-pin one-module generation reproduces only the declaration (+2 header lines) and its own
manifest repair record. The carried header exactly equals the tested real candidate. The actual unused
header compiles, links and runs with static signature/slot/default/explicit-zero checks. Actual default-
and explicit-threshold registration calls compile to an object, then fail to link on **only** the exact
missing helper. No eager registration is misreported as an explicit-use negative link, and no host
provider, registration, readback, bitmap-cache or hit operation is executed or supplied.

The original owned-header jobs1 check completes **1 attempted / 1 pass / 0 fail / 0 unattempted**.
The sole row is pass with `previousCluster`, and the active cluster is removed; no other header is
measured or promoted. Both body omissions (`hitTestSpriteAlpha` and `bitmapForImage`), partial marker,
source/refusal fingerprints, package/global counts, initialization and applicability policy remain.
The bitmap helper has no retained live caller and receives no declaration. Existing owner/refinement
requests retain graph-local bounds, original Sprite/data/texture/source/Bitmap owners and weak image
lifetime, successful-only capture caching/retries, original required provider results/errors and exact
UV-offset/image-size/local-point floors, bitmap bounds and Alpha-threshold comparison. No fake hit,
copied graph/bitmap, new invalidation policy, narrowed domain, source/runtime/override supply or shared
Sprite/Node/bounds/peer QuadBatch edit is carried. Foreign raw registry/worklist/request data and
historical/G1 totals are preserved.

### 191. `undeclared-identifier-7fd1550e` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glCameraMotionBlurEffectRunner` binding (and its callee `applyCameraMotionBlurEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_camera_motion_blur_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of intensity and samples, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_camera_motion_blur_effect.hpp:88`
- example: `use of undeclared identifier 'gl_camera_motion_blur_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 192. `undeclared-identifier-7ff1c319` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glVignetteEffectRunner` binding (and its callee `applyVignetteEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_vignette_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of color, intensity, radius and softness, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_vignette_effect.hpp:95`
- example: `use of undeclared identifier 'gl_vignette_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 193. `undeclared-identifier-8653adf0` — declaration repaired (1 header rechecked)

Original1 Clang22.1.8 jobs1: **0/1 pass**,31.7s, missing ensure_scene_2d_runtime247.
The helper refuses recovery from inherited EntityRuntime to Scene2DRuntime with its scene2dSignals
cell; exact Scene2D and Scene2DRuntime references already exist. Existing declare-refused-function
repair scene2d-declare-refused-ensure-runtime adds only that source/retained-caller prototype.
Marker/live-call/no-definition, one owned match, fixed point and removal restoring original bytes PASS.

Actual unused whole-header/static signature compile/link/run0; a program invoking both real retained
enable_scene_2d_signals and get_scene_2d_runtime compiles0, then links1 **only** on the absent helper,
with both references named by the linker. It is not run. This qualifies declaration/header availability;
no helper, runtime, signal-enabling or scene/provider body execution is claimed.

Normal single-module exact-pin generation reproduces the whole tested header and own applied-repair
record. Final owned1 jobs1: **1 attempted / 1 pass / 0 fail / 0 unattempted**. Old root removed;
row pass with previousCluster. Partial marker and all five refused bodies plus package/refusal totals
stay unchanged. Foreign raw/semantic roots/rows, worklist prose, history and G1 totals are preserved.

The [retained-callee request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-scene2d-lazy-runtime-accessor-remains-an-absent-body)
and [full scene/runtime/root contract](compiler-requests/asserted-erased-runtime-owner.md#g2-scene2d-lazy-runtime-and-root-ownership)
record Scene2D versus tree-root identity, nullish options/defaults, lazy runtime/binding/signal reuse,
nonallocating signals read, conditional resize write/callback order and current-root membership.
The root's readonly/base runtime needs separate legitimate mutation and full Node2DRuntime cells;
no shared owner/ABI/provider implementation or narrowing is carried. Source tests are requirements,
not native scene/root/runtime/signal/provider/rendering results. No fake scene, copied owner or empty
success is supplied. Scope is bounded to the owned header, repair/record and compact products/requests.

### 194. `undeclared-identifier-898d7996` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glDisplacementEffectRunner` binding (and its callee `applyDisplacementEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_displacement_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of frequency, intensity and seed, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_displacement_effect.hpp:91`
- example: `use of undeclared identifier 'gl_displacement_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 195. `undeclared-identifier-8ae633dc` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glSmaaEffectRunner` binding (and its callee `applySmaaEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_smaa_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of threshold, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_smaa_effect.hpp:86`
- example: `use of undeclared identifier 'gl_smaa_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 196. `undeclared-identifier-8b3950d6` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glMotionBlurEffectRunner` binding (and its callee `applyMotionBlurEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_motion_blur_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of intensity and samples, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_motion_blur_effect.hpp:92`
- example: `use of undeclared identifier 'gl_motion_blur_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 197. `undeclared-identifier-8e604c65` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glSketchEffectRunner` binding (and its callee `applySketchEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_sketch_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of strength, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_sketch_effect.hpp:86`
- example: `use of undeclared identifier 'gl_sketch_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 198. `undeclared-identifier-9562ef07` — G2 recheck of 1 header(s)

- cause: the emitter refuses `drawCanvasTilemap` (`cpp-contextual-union-interface-heritage-carrier-unrepresented`: a nullable `Material2D` widened to `Material`), but keeps `.submit = draw_canvas_tilemap` in the inline `canvas_tilemap_renderer` `SpriteRenderer` aggregate.
- subject: `draw_canvas_tilemap`
- lanes: 2D
- fix route: compiler
- why this route: a scratch copy with only the exact declaration `void draw_canvas_tilemap(Ref<CanvasRenderState>, Ref<RenderProxy2D>)` still fails at `canvas_tilemap.hpp:121` `.create_data`, where `noop_renderer_data` takes `NodeAny` and the slot expects `Sprite`. `submit` also takes `CanvasRenderState` where the slot takes `RenderState` (type inspection only). Neither the positive unused-header link nor the negative link is reachable, and `retainedReference` does not accept an aggregate member, so no repair is carried. This is the same shape as GL bitmap text.
- first failure at: `flight/scene2d_canvas/canvas_tilemap.hpp:120`
- example: `use of undeclared identifier 'draw_canvas_tilemap'`
- Scoped native recheck (GCC 15.2, jobs 1, all errors shown): **1 attempted / 0 pass / 1 fail**, the header's own diagnostic. No tilemap draw, no-op, Canvas backend, copied owner or narrowed tiles is supplied.
- compiler request: `docs/compiler-requests/retained-declaration-calls-omitted-callee.md`

### 199. `undeclared-identifier-9aaa5b7f` — 1 header, compiler-owned

Stock original1 remains **1 attempted / 0 pass / 1 fail / 0 unattempted**, Clang22.1.8, jobs **1**,
12 seconds, missing `acquire_wgpu_screen_render_target_capture_texture:122`. Acquire and encode refuse
`cpp-optional-member-access-unproven` at height/width despite the same local texture's source null
guards. See [external optional evidence](compiler-requests/guarded-external-optional-member-view.md#g2-wgpu-capture-guards-the-exact-retained-texture).
The map helper separately refuses `cpp-shared-mutable-capture-missing-initial-storage` on its timer;
see [captured initial storage](compiler-requests/captured-referent-mutation.md#g2-wgpu-capture-timer-initial-storage).

The two hook values in enable are assignments, not direct calls. Existing marker-guarded
`insert-forward-declaration` qualifies their exact GPUTexture/target and void/target/encoder callback
types; existing `declare-refused-function` qualifies the directly awaited `Task<void>(WgpuBuffer,double)`
map helper. All three match one header, reach a fixed point and removal restores original bytes.
The actual unused-header scratch with only those prototypes still fails compilation at candidate175
(stock169): native `WgpuObject<WgpuBufferTag>` lacks `get_mapped_range`. See the new
[mapped readback contract](compiler-requests/webgpu-buffer-mapped-readback.md) and
[declaration evidence](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-wgpu-capture-three-exact-declarations-still-fail-native-readback).
No positive unused-header link/run or qualified negative object/link is reached; no repair, generation,
fake provider/capture/readback, copied owner or narrowed domain is carried.

Preserve original target/device/texture/retained-buffer identity. Capture uses the logical floor/clamped
extent after undoing antialias2x, texture reuse/resize and original format/usage flags. Copy is encoded
in the frame's own encoder; pending/mapped buffers skip writes, unknown map state falls through.
Rows align to256 bytes; buffer dimensions/stride update only on reallocation. Readback snapshots the
same buffer/dimensions/stride before awaiting, strips padding, swizzles only the two BGRA formats,
preserves stored premultiplied RGB/alpha, and unmaps rather than destroys on normal completion.
Keep the absent-buffer error, default10000ms and <=0 indefinite wait, one map operation, adopted late
rejection, exact timeout error and timer cleanup in finally. Timeout does not cancel/unmap a late map;
it is a hard end for capture against that buffer. The source has no readback-wide cleanup finally, so
no stronger failure-cleanup claim is made. Native map/view/copy/provider behavior remains unavailable.

Only this row/cluster, report199, three existing request appendices and new request/index change.
Historical sweep/carry totals and foreign products/prose remain unchanged. No GPU capture, pixel,
timeout behavior, source/runtime body or unchanged old/global check is claimed.

### 200. `undeclared-identifier-a3f09fb8` — G2 recheck of 1 header(s)

- cause: the emitter refuses `drawDomShape` (`cpp-contextual-union-missing-expression-type:optionalSingle`; the refusal does not name the expression), but keeps `.submit = draw_dom_shape` in the inline `dom_shape_renderer` aggregate, which `dom_morph_shape_renderer` aliases.
- subject: `draw_dom_shape`
- lanes: 2D
- fix route: compiler
- why this route: a scratch copy with only the exact declaration `void draw_dom_shape(Ref<DomRenderState>, Ref<RenderProxy2D>)` still fails at `.create_data`. There, `create_dom_shape_data` returns `Ref<DomShapeData>` from `NodeAny`, and the slot needs `optional<Ref<RendererData>>` from `Node2D`. Neither link check is reachable, so no repair is carried. This is the same shape as the DOM Scale9 shape. `scene2d-dom` declares environment `web`.
- first failure at: `flight/scene2d_dom/dom_shape.hpp:132`
- example: `use of undeclared identifier 'draw_dom_shape'`
- Scoped native recheck (GCC 15.2, jobs 1, all errors shown): **1 attempted / 0 pass / 1 fail**, the header's own diagnostic. No DOM backend, raster, copied owner or narrowed shape is supplied.
- compiler request: `docs/compiler-requests/retained-declaration-calls-omitted-callee.md`

### 201. `undeclared-identifier-a4a22a69` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glLensDistortionEffectRunner` binding (and its callee `applyLensDistortionEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_lens_distortion_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of amount and scale, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_lens_distortion_effect.hpp:87`
- example: `use of undeclared identifier 'gl_lens_distortion_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 202. `undeclared-identifier-a89b9a39` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glBitmapDisplacementEffectRunner` binding (`cpp-structural-assertion-writable-capability-unproven`) and the `isGlBitmapDisplacementEffectResolvable` resolver (`cpp-structural-assertion-owner-unproven`, asserting `Readonly<Effect>` to read `map`), but `register_gl_bitmap_displacement_effect` still passes both. The callee `applyBitmapDisplacementEffectToGl` is refused on the readonly structural `GlTextureRenderTarget` widening.
- subject: `gl_bitmap_displacement_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: this is the two-symbol custom-shader shape the request was filed for. The resolver alone would meet the `retainedReference` witness, but the runner on the same line is a variable the repair cannot declare, so the row would not move. A runner or resolver body would fake the effect or its map resolution.
- first failure at: `flight/effects_gl/gl_bitmap_displacement_effect.hpp:153`
- example: `use of undeclared identifier 'gl_bitmap_displacement_effect_runner'`
- Verified by inspection: line 153 is the registration call. No symbol is defined or named elsewhere. This docs-only disposition changes no compiled input, so the recorded **0/1** failure stands without an unchanged rerun. No texture provider, shader or narrowed domain is supplied.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 203. `undeclared-identifier-aa084556` — 1 header(s), header boundary fixed

- original cause: the retained grid container validator calls `is_track_list` after its unknown-array predicate is refused
- subject: `is_track_list`; the separately refused `grid_layout_resolver` binding also remains referenced
- lanes: PLAT
- fix route: noncoercive runtime integer predicate plus [two existing declaration repairs](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-grid-track-guard-and-resolver-retain-two-distinct-omissions)
- original first failure at: `flight/layout/grid_layout.hpp:151`
- original example: `use of undeclared identifier 'is_track_list'`

`layout-declare-refused-track-list` declares `bool is_track_list(flight::Any)` from the exact function
marker, two retained calls and absent definition. `layout-declare-refused-grid-resolver` separately
declares the actual external `LayoutResolver` binding, guarded by its own line-17 refusal marker.
Normal exact-pin single-module generation reproduces only these declarations and their repair records;
no source, validator, resolver, owner, field or layout algorithm is rewritten or supplied.

An initial scratch unused-header candidate fails before any link because `Number.isInteger` is called
with `Any`, while the runtime accepts only double. `is_integer(const Any&)` now selects only the number
kind before delegating the existing finite/integral test; every represented nonnumber is rejected without
coercion, callable invocation or implicit numeric conversion. The existing runtime tests cover numeric
edges, all represented nonnumber categories and stored-value effects.

- **Original header gate:** complete serial check, **1 attempted / 1 pass / 0 fail / 0 unattempted**.
- **Runtime:** existing CMake runtime target builds at jobs1 and CTest passes **1/1**.
- **Unused header positive:** actual repaired header compiles, links and runs.
- **Track predicate negative:** actual retained container-validator call compiles to an object, then fails to link on exactly `flight::layout::is_track_list(flight::Any)`.
- **Registration negative:** actual retained registration compiles to an object, then fails to link on exactly `flight::layout::grid_layout_resolver`.

The [unknown track-list evidence](compiler-requests/erased-array-element-reading.md#g2-grid-track-list-validator-retains-the-same-erased-element-boundary),
[grid item/container recovery](compiler-requests/assertion-to-nullable-variant-alternative.md#g2-grid-resolver-keeps-its-own-item-and-container-contracts)
and [active track fraction owner](compiler-requests/reference-backed-outcome-variant-narrowing.md#g2-grid-track-fraction-still-requires-an-active-owner)
remain refused. Grid requires non-null containers with nonempty track arrays, paired explicit row/column,
positive spans and fixed/auto/fraction sizing; flex's validator/resolver is no substitute. Getter/read order,
source validation, intrinsic/gap/padding arithmetic and output writes remain intact in the refused source.

The one product row is pass with `previousCluster`; the old active cluster is removed. All six omissions,
module fingerprints/refusal ledger/package counts and foreign/G1 data remain unchanged. Neither refused
body was run; no native grid algorithm, successful validation/registration, arbitrary-owner recovery,
fake resolver or narrowed value domain is claimed.

### 204. `undeclared-identifier-ae594b8a` — declaration repaired (1 header rechecked)

Original1 jobs1 Clang22.1.8: **0/1 pass**,94.9s, missing append_lottie_shape_items153.
The source33 walker refuses its readonly LottieTransform assertion through the shape-item variant.
Its actual concrete transform and open Record arms remain required; selecting just one native
concrete arm would narrow the input. No body/owner/domain substitution is supplied.

Existing `declare-refused-function` repair `scene2d-formats-declare-refused-lottie-shape-items`
adds only the exact retained DisplayObject/Array<LottieShapeItem>/Ref<LottieImportContext>/nullable
String-name prototype. Marker/live-call/no-definition, one owned match and fixed point PASS;
removing the prototype restores original bytes. Actual unused owned-header compile/link/run0;
actual retained-handler object compile0 then link1 solely on absent append_lottie_shape_items.
The expected link failure qualifies the declaration; no called-handler execution or draw claim.

Normal one-module generation reproduces the complete tested header and new applied-repair record.
Final owned1 jobs1: **1 attempted / 1 pass / 0 fail / 0 unattempted**. Old failed root removed,
row pass with previousCluster; this worklist entry updated. Partial marker/body refusal and module/
package totals stay unchanged. Foreign raw roots/rows/prose and historical/G1 totals are preserved.

The [retained-callee request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-lottie-shape-layer-retains-its-refused-group-walker)
and [retained-owner/source contract](compiler-requests/readonly-interface-assertion-retained-owner.md#g2-lottie-shape-transform-view-and-complete-group-walk)
record first transform before hidden filtering, recursive group names/order, shared claims and live
arrays/closures, registered/custom kinds and diagnostics, postwalk pushed modifiers, painter order,
clear/unpainted/rerender behavior and conditional Shape/always-group attachment. Existing trim claims
and general render-stack limitations remain. No fake Lottie shape/parser/draw, copied owner, empty
result, narrowed input or native shape functionality is claimed. Validation is bounded to owned scope.

### 205. `undeclared-identifier-af7a1054` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glMedianEffectRunner` binding (and its callee `applyMedianEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_median_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of radius, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_median_effect.hpp:90`
- example: `use of undeclared identifier 'gl_median_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 206. `undeclared-identifier-b15d9d15` — G2 recheck of 1 header(s)

- cause: retained section handler calls the omitted parseDragonBonesBones and reads its omitted complete result
- subject: `parse_dragon_bones_bones`
- lanes: ANIM
- fix route: [retained callee/result contract](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-dragonbones-bones-omit-both-the-parser-and-its-complete-result-contract), with [full erased input/bone semantics](compiler-requests/erased-array-element-reading.md#g2-dragonbones-bones-preserve-topological-order-and-positional-remaps)
- actual first failure at: `flight/skeleton2d_formats/dragon_bones_bones_handler.hpp:141`
- example: `use of undeclared identifier 'parse_dragon_bones_bones'`

Original1 completes at jobs1, Clang22.1.8,29.8s: **1 attempted / 0 pass / 1 fail / 0 unattempted**.
The helper refuses erased-array recovery; runtime AnyArray already has exact Array<T> recovery,
but no generic erased element reads. No complete emitted anonymous bones/remap result exists to
qualify an exact declaration. The larger context cannot substitute for it. No candidate, unused
positive link/run or exact negative link was reached or is claimed.

Preserve input/raw-index positions, repeated pending-order parent resolution, duplicate-name Map
policy, unresolved parents/cycles retained as roots with one Drop diagnostic, exact transforms and
all four independent inheritance flags. Append original bone references and write the remap prefix
and name entries into the same context arrays/Map. Missing Record keys and indexed writes growing the
initially empty remap remain inspected obligations under existing requests; no native proof claimed.

No C++, source, repair, override, manifest, inventory or normal generation change is carried. Only
this failed root/row and worklist entry change, plus two existing request appendices. Foreign
rows/roots/prose and historical/G1 totals remain unchanged; no parser, bone or header gain is claimed.

### 207. `undeclared-identifier-befaa2d6` — 2 header(s) (1 at G1; `gl_clip.hpp` joined in G2 from cluster 91)

- cause: the emitted code names `compute_scissor_rect`, which nothing declares
- subject: `compute_scissor_rect`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_gl/gl_clip_rectangle.hpp:108`
- example: `use of undeclared identifier 'compute_scissor_rect'`
- G2 fix: source patch `scene2d-gl-clip-rectangle-viewport-number-domain`, plus two scoped drawing-buffer repairs

`computeScissorRect` is refused with `cpp-union-runtime-domains-erased` (`GLsizei | primitive -> double`)
on `viewport?.width ?? state.gl.drawingBufferWidth`, and the same for height: Web IDL's `GLsizei` and
`number` are one JavaScript domain that arrives through two declarations. The patch applies unary plus
after the nullish selection, exactly as the landed `scene3d-gl-viewport-number-domain` patch does. Both
operands are already numbers, so every value, NaN and signed zero is preserved, and so is the read order.
It applies cleanly to the pin.

Scoped generation then emits `compute_scissor_rect`. Its two drawing-buffer property reads needed the
existing statement-final accessor respell, whose `render_gl/` scope does not reach here, so two exact
`scene2d-gl-clip-rectangle-drawing-buffer-{width,height}-property-binding` repairs were added. There are
no scissor stand-ins: the generated function is the source's own computation.

The module is still listed as `dependency-refused`, because `glQuadBatchWriter.ts` (same number-domain
shape at its lines 340-341) is refused. Its header is shadowed by a peer-owned native override, so a
second patch would only drift that override without changing what compiles; it was measured and then
withdrawn.

**G2: 1/2 compile, 1 fail** (`-j1`, Clang 22.1.8). Passing: `flight/scene2d_gl/gl_clip_rectangle.hpp`.
`flight/scene2d_gl/gl_clip.hpp` moves to `no-viable-conversion-e25fffde`. Its `Scene2DClipHooks` slots are
declared over `RenderState` but implemented over `GlRenderState`. That needs emitted heritage plus checked
derived-owner recovery: the
[interface heritage widening](compiler-requests/interface-heritage-widening.md) request, extended with this
site. The clip renderer is not claimed complete.

### 208. `undeclared-identifier-c162d0c0` — 1 header, resolved by declaration-only repair

- original subject: `solve_skeleton_2d_transform_constraint`
- lanes: ANIM
- original failure at: `flight/skeleton2d/transform_constraint_2d.hpp:175`
- fix route: existing `declare-refused-function` repair, `skeleton2d-declare-refused-transform-constraint-solver`
- retained callback: [exact declaration and link boundary](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-skeleton2d-transform-registration-retains-its-refused-solver)
- body request: [original constraint and pose owner recovery](compiler-requests/assertion-to-undeclared-row-member.md#g2-transform-constraint-dispatch-must-retain-the-original-constraint-and-pose-owners)

The exact refusal marker, live direct function-argument registration and absent definition agree.
The existing retainedReference repair declares only void(Ref<Skeleton2D>, readonly Skeleton2DConstraint
row), exactly the emitted Skeleton2DConstraintSolver slot. It matches one header and is idempotent.
The mutable skeleton Ref and full readonly base constraint ABI remain. No transform-only callback,
closed kind union, adapter, repair-engine extension or replacement owner is added.

Normal exact-pin one-module generation plus the existing derived structural-alias rule reproduces
only the declaration (+2 header lines) and own repair record. The narrow closure omits Screen;
applying that existing rule only to this raw header and its committed canonical Screen owner
preserves the already-carried x_y alias/include. The canonical owner and alias provenance stay
unchanged; the result equals the tested candidate. The actual unused whole header compiles,
links and runs with static solver-slot/signature checks. Calling the actual opt-in registration
compiles to an object, then fails to link on **only** the exact omitted transform solver signature.
Neither registration nor the solver runs; there is no native transform/pose result.

The owned-header jobs1 check completes **1 attempted / 1 pass / 0 fail / 0 unattempted**.
The sole row is pass with previousCluster; the active cluster is removed without promoting or
measuring foreign headers. Helper bodies, partial marker, original row-owner refusal, source/refusal
fingerprints, package/global counts, initialization and applicability policy are unchanged.
Existing requests preserve the original constraint, Bone2D and Float32 owners; target snapshot,
all six channel mixes/offsets, signed angle wrap, world-scale ratios, combined world translation
before one parent inversion, source singular/range checks, original numeric/transform-mode domains
and per-bone refresh order. The open registry's ordered pass and caller's later descendant refresh
remain. No fake constraint, empty solver/result, copied pose owner, narrowed domain or
source/runtime/override body is carried. Foreign raw registry/worklist/request data and historical
G1 totals remain unchanged.

### 209. `undeclared-identifier-c29489a5` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glBloomEffectRunner` binding (`cpp-structural-assertion-writable-capability-unproven`: `Readonly<Effect>` declares none of `intensity`, `passes`, `radius` and `threshold`), but `register_gl_bloom_effect` still passes it. Its callee `applyBloomEffectToGl` is refused because each pooled intermediate (`acquireGlTextureRenderTarget`, a plain `Ref<GlTextureRenderTarget>`) is passed as `drawGlFullscreenPass`'s `Readonly<GlRenderTarget> | null` destination.
- subject: `gl_bloom_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: the runner is a variable, which the declaration repair cannot declare, and a definition would fake bloom. The callee needs real `Ref<Derived>` to `Ref<Base>` heritage, per [interface-heritage-widening](compiler-requests/interface-heritage-widening.md). This header is excluded from the runner-family table because of the plain-`Ref` callee, and is handled on its own here.
- first failure at: `flight/effects_gl/gl_bloom_effect.hpp:106`
- example: `use of undeclared identifier 'gl_bloom_effect_runner'`
- Scoped native recheck (GCC 15.2, jobs 1, all errors shown): **1 attempted / 0 pass / 1 fail** at line 106, the only error. Neither symbol is defined or named elsewhere. No bloom pass, shader, pool or narrowed domain is supplied.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 210. `undeclared-identifier-c3c55c65` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glCompositeEffectRunner` binding (and its callee `applyCompositeEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_composite_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of backdropKey and operator, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_composite_effect.hpp:92`
- example: `use of undeclared identifier 'gl_composite_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 211. `undeclared-identifier-c45cc1bb` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glFxaaEffectRunner` binding (and its callee `applyFxaaEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_fxaa_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of edgeThreshold and subpixel, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_fxaa_effect.hpp:86`
- example: `use of undeclared identifier 'gl_fxaa_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 212. `undeclared-identifier-c7551b38` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glPixelateEffectRunner` binding (and its callee `applyPixelateEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_pixelate_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of size, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_pixelate_effect.hpp:86`
- example: `use of undeclared identifier 'gl_pixelate_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 213. `undeclared-identifier-cc7c5006` — 1 header, resolved by declaration-only repair

- original subject: `solve_skeleton_2d_path_constraint`
- lanes: ANIM
- original failure at: `flight/skeleton2d/path_constraint_2d.hpp:290`
- fix route: existing `declare-refused-function` repair, `skeleton2d-declare-refused-path-constraint-solver`
- retained callback: [exact declaration and link boundary](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-skeleton2d-path-registration-retains-its-refused-solver)
- body request: [both original constraint and attachment owners](compiler-requests/assertion-to-undeclared-row-member.md#g2-path-constraints-need-both-original-constraint-and-attachment-owners)

The exact refusal marker, live direct function-argument registration and absent definition agree.
Existing retainedReference declares only void(Ref<Skeleton2D>, readonly Skeleton2DConstraint row),
exactly the emitted solver slot. Match1/idempotent/removal exact. Mutable skeleton and readonly
base constraint ABI remain. No path-only callback, closed kind family, adapter, new owner or engine
extension is added. resolveSkeleton2DPathAttachment is also refused, but only the omitted solver
comment references it, so no second declaration is carried.

Normal exact-pin one-module generation plus the existing derived structural-alias rule reproduces
only +2 owned header lines and its own repair record. The narrow closure omits Screen; applying
that existing rule only to the raw owned header and committed canonical Screen preserves the
already-carried x_y alias/include, exactly matching the real tested candidate. Canonical owner and
alias provenance are unchanged. The actual unused whole header compiles, links and runs, including
its retained scratch initializers. Actual opt-in registration compiles to an object, then fails
to link on **only** the exact omitted path solver signature. Neither registration nor solver runs.

The scoped owned-header jobs1 check completes **1 attempted / 1 pass / 0 fail / 0 unattempted**.
Only that row is pass with previousCluster and the old active cluster is removed. Both concrete
owner refusals, partial marker, source/refusal fingerprints, helper/scratch bodies, counts,
initialization metadata and applicability policy stay unchanged. Existing requests preserve
original constraint/attachment/path/bone/Float32 owners and open kinds, actual weighted/rigid
posed-path deformation, full position/spacing/rotation and numeric/TransformInherit domains,
per-sample clamping and invalid-index sampling, sample-before-mutation order, combined world
translation then parent inversion, tangent/next-sample rotation fallback and per-bone refresh.
The ordered constraint pass and caller's later descendant propagation remain. No fake constraint
or path solver, empty result, copied owner, narrowed domain, source/runtime/override body or native
path-following/deformation/pose gain is supplied. Foreign compact products/registry/prose and
historical G1 totals remain unchanged.

### 214. `undeclared-identifier-cf2fed2b` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glPosterizeEffectRunner` binding (and its callee `applyPosterizeEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_posterize_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of levels, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_posterize_effect.hpp:85`
- example: `use of undeclared identifier 'gl_posterize_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 215. `undeclared-identifier-d14c2215` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glConvolutionEffectRunner` binding (and its callee `applyConvolutionEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_convolution_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of bias, clamp, color, divisor, matrix, matrixX, matrixY and preserveAlpha, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_convolution_effect.hpp:131`
- example: `use of undeclared identifier 'gl_convolution_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 216. `undeclared-identifier-d687475c` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glHalftoneEffectRunner` binding (and its callee `applyHalftoneEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_halftone_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of angle and scale, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_halftone_effect.hpp:88`
- example: `use of undeclared identifier 'gl_halftone_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 217. `undeclared-identifier-d7a1d1b7` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glTiltShiftEffectRunner` binding (and its callee `applyTiltShiftEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_tilt_shift_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of blur, center and width, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_tilt_shift_effect.hpp:90`
- example: `use of undeclared identifier 'gl_tilt_shift_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 218. `undeclared-identifier-e3d6c4e4` — 1 header(s)

- cause: the emitted code names `apply_gl_gaussian_blur_pass`, which nothing declares
- subject: `apply_gl_gaussian_blur_pass`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_blur_effect.hpp:93`
- example: `use of undeclared identifier 'apply_gl_gaussian_blur_pass'`

### 219. `undeclared-identifier-e4a7b51e` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glLensDirtEffectRunner` binding (`cpp-structural-assertion-writable-capability-unproven`: `Readonly<Effect>` declares none of `intensity`, `seed` and `threshold`), but `register_gl_lens_dirt_effect` still passes it. Its callee `applyLensDirtEffectToGl` is refused because each pooled intermediate (`acquireGlTextureRenderTarget`, a plain `Ref<GlTextureRenderTarget>`) is passed as `drawGlFullscreenPass`'s `Readonly<GlRenderTarget> | null` destination.
- subject: `gl_lens_dirt_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: the runner is a variable, which the declaration repair cannot declare, and a definition would fake the effect. The callee needs real `Ref<Derived>` to `Ref<Base>` heritage, per [interface-heritage-widening](compiler-requests/interface-heritage-widening.md). This is the same shape as bloom (excluded from the runner-family table for the same reason).
- first failure at: `flight/effects_gl/gl_lens_dirt_effect.hpp:112`
- example: `use of undeclared identifier 'gl_lens_dirt_effect_runner'`
- Verified by inspection: line 112 is the registration call. Neither symbol is defined or named elsewhere. This docs-only disposition changes no compiled input, so the recorded **0/1** failure stands without an unchanged rerun. No pass, shader, pool or narrowed domain is supplied.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 220. `undeclared-identifier-e4eb111d` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glKuwaharaEffectRunner` binding (and its callee `applyKuwaharaEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_kuwahara_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of radius, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_kuwahara_effect.hpp:86`
- example: `use of undeclared identifier 'gl_kuwahara_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 221. `undeclared-identifier-e7bf11f9` — 1 header, compiler-owned

The scoped stock header remains **1 attempted / 0 pass / 1 fail / 0 unattempted**, Clang22.1.8,
jobs **1**, 58.8 seconds, at missing `draw_wgpu_scale9_sprite:164`. The source body refuses
`cpp-contextual-union-interface-heritage-carrier-unrepresented` at nullable `Material2D` to `Material`
owner widening during quad-material resolution. Preserve the proxy's material identity/null and actual
WGPU state/runtime. See the existing [material owner request](compiler-requests/compiler-interface-heritage-owner-widening.md#g2-wgpu-scale9sprite-material-owner).

The retained `.submit = draw_wgpu_scale9_sprite` is an aggregate field, not a direct call or supported
function-argument `retainedReference`. The existing matcher finds no call; the loader rejects the
exact aggregate witness. A separate scratch actual unused include, with only the exact declaration
`void draw_wgpu_scale9_sprite(Ref<WgpuRenderState>, Ref<RenderProxy2D>);`, fails compilation at
`.create_data` (candidate166): native `create_sprite_renderer_data(GlRenderState, Node2D)` returns
`optional<ErasedRef>`, whereas `Scene2DRenderer` takes generic RenderState and returns
`optional<Ref<RendererData>>`. The [typed callback request](compiler-requests/readonly-callback-derived-owner-recovery.md#g2-wgpu-scale9sprite-native-sprite-callback-and-owner-boundaries)
also records inspected dirty/submit domains and exact Sprite recovery versus independent Scale9Sprite
data/grid/runtime/texture owners. These later differences are not further measured first errors.
No positive unused-header link/run or qualified called-function negative-link is reached; no repair,
normal generation, callback cast, owner copy or witness extension is carried.
[Retained aggregate evidence](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-wgpu-scale9sprite-retains-its-refused-draw-callback).

Preserve render-pass/null/non-2d/source guards, original texture/view/sampler, material registration
lookup/miss/registered standard fallback or no draw, blend, working color and alpha. The mapper rejects
nonpositive scale/bounds and must retain the full grid math, including compressed borders when the
scaled center is negative. Keep source and mapped four-coordinate axes, all nine row-major slices
without new skip rules, inverse-scale affine basis, translated slice origins and all UV coefficients.
Material/color streams use the writer count read after prepare plus instance, with original
materialData and matrix-before-scale-bias fallback; the real writer count increases by nine.

Separate source inspection finds an offset-unit inconsistency: `wgpuScale9Sprite.ts:54,74` uses the
prepare result directly as `writeBase`, but `wgpuQuadBatchWriter.ts:356` returns an instance index and
its affine writer indexes float elements at `base` (13 floats per instance). With a compatible
nonempty batch this differs from the documented stride-scaled packet offset. This is unexecuted
source evidence, not another native error or a source fix; no correct nine-slice packet claim follows
from a future declaration alone.

Only this row/cluster, report221 and appended sections in three existing requests change. Historical
sweep/carry totals and all foreign products remain unchanged. No production code/body, fake draw,
narrowed mapper/material/texture domain, GPU proof or unchanged old/global check is carried or claimed.

### 222. `undeclared-identifier-ebf3999b` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glRadialBlurEffectRunner` binding (and its callee `applyRadialBlurEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_radial_blur_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of centerX, centerY, samples and strength, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_radial_blur_effect.hpp:96`
- example: `use of undeclared identifier 'gl_radial_blur_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 223. `undeclared-identifier-f020c1f7` — G2 recheck of 1 header(s)

- cause: the emitter refuses the `glWhiteBalanceEffectRunner` binding (and its callee `applyWhiteBalanceEffectToGl`) but still emits the registration call that passes the runner
- subject: `gl_white_balance_effect_runner`
- lanes: GL
- fix route: compiler
- why this route: one row of the [effects_gl runner family](compiler-requests/custom-shader-effect-runner-owner.md#g2-remaining-effects_gl-runner-family). `Readonly<Effect>` declares none of temperature and tint, and the callee refusal is the `GlTextureRenderTarget` widening.
- first failure at: `flight/effects_gl/gl_white_balance_effect.hpp:87`
- example: `use of undeclared identifier 'gl_white_balance_effect_runner'`
- Verified by inspection: docs-only, the recorded **0/1** stands, and nothing is carried.
- compiler request: `docs/compiler-requests/custom-shader-effect-runner-owner.md`

### 224. `undeclared-identifier-f6affb68` — 1 header(s)

- Cause: three globally initialized transmission/volume/IOR handler apply callbacks call `resolve_transmission_volume`, refused as `cpp-structural-assertion-owner-unproven` at PbrExtension-to-TransmissionVolumePbrExtension recovery. The actual base lacks the nine concrete attenuation/ior/thickness/transmission/map cells.
- Route: compiler, [existing retained-callee request](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-gltf-transmission-volume-a-declaration-also-fails-the-unused-header-link), with [existing narrower-row-owner request](compiler-requests/assertion-to-undeclared-row-member.md#g2-gltf-transmission-volume-and-ior-need-the-same-retained-descriptor). The current extension union already retains built-in and open base arms; kind equality alone does not recover the original concrete owner. Material find/attach helpers are separately refused at Standard/Extended material recovery.
- Exact declaration probe: optional Ref<TransmissionVolumePbrExtension> from the retained readonly GltfExtensionContext row and double index. Existing marker/live-call/no-definition guard matches this actual header once and is idempotent; no new mechanism is needed to match it. The real unused include compiles to an object, but its POSITIVE link fails on exactly the missing resolver, referenced from all three callbacks. This fails the repair prerequisite and is not a called-function negative-link pass; candidate rejected.
- Preserve one shared real descriptor across arbitrary handler order, original material/texture owners and legitimate standard-material promotion, absent-extension/material branches, factory/white/infinite-distance/zero-thickness/1.5-IOR defaults, and writes before texture-resolution callbacks or exceptions. No fake/empty volume, null substitute, copied base or narrowed open family is carried.
- Recorded result remains **0/1 pass, 1 fail** at `flight/scene3d_formats/gltf_transmission_volume.hpp:166`, original undeclared-call diagnostic unchanged. One owned row/cluster gains both request links. No production C++/source/repair/manifest change; no normal generation or unchanged native/global rerun. No positive unused-header link, qualified negative link or callable material-import behavior is claimed. Foreign worklist rows/clusters and historical G1 totals are unchanged.

### 225. `undeclared-identifier-f91f1e72` — 1 G1 header, passing through existing partial native carry

The historical missing `get_gl_text_label_data` at generated `gl_text_label.hpp:100` is already
shadowed by the landed `scene2d-gl-native-glyph-text-label` override. This G2 arc changes no C++,
source patch, repair or manifest. The current effective header measured **1 attempted / 1 pass /
0 fail / 0 unattempted**, Clang22.1.8, jobs **1**, 183.4 seconds. The one row now records pass,
its original cluster and the unresolved requests; the obsolete active failure cluster is removed.

The existing native path keeps the exact `TextLabel` from the render proxy, its typed
`TextLabelRuntime` and original data/layout/revision cells. Renderer scratch allocation is boxed as
`ErasedRef` by `NativeGlScene2DRendererBinding`, then recovered with checked
`as<NativeGlTextLabelData>()`; the original owner is retained. Its glyph-provider boundary, Bitmap,
material/texture realization and standard quad writer stay unchanged. No fixed glyph subset, copied
runtime/renderer data, unchecked base cast or fake drawing is introduced. These are inspected
properties of the prior approved carry, not fresh behavioral results.

The generated Canvas path remains partial for separate reasons. `getGlTextLabelData` refuses as
`cpp-reference-assertion-without-heritage`: flattened `RendererData` cannot recover the private
`GlTextLabelData` allocation. This is the existing
[identity-preserving refinement request](compiler-requests/flattened-interface-refinement-identity.md#g2-gl-textlabel-private-renderer-data).
`drawGlTextLabel` also refuses as `cpp-presence-test-without-absence-storage`; its message does not
identify the tested property, so no particular field or null-removal patch is inferred. See
[nullable presence](compiler-requests/nullable-optional-presence.md) and the
[retained getter/draw references](compiler-requests/retained-declaration-calls-omitted-callee.md#g2-gl-textlabel-generated-canvas-path-remains-unavailable).
The generated singleton's callback carriers also differ from the explicit native GL binding.

The native override remains **partial**: CanvasHost/ImageHost overloads and the browser singleton
renderer are unavailable, and font selection/shaping depend on the supplied glyph capability.
An unused-header link or called-function negative link was not needed or run: no declaration repair
is proposed, and the effective header does not call the refused getter. No generation, new glyph
semantics test, shader/draw execution, old E0a/clip-cache proof or global/native baseline was rerun.
Historical sweep/carry totals, all foreign rows/clusters and report slices remain unchanged.

### 226. `undeclared-identifier-fcbdae42` — G2 recheck of 1 header(s)

- cause: the emitter refuses `drawCanvasTextLabel` (`cpp-contextual-union-missing-expression-type:optionalSingle`; the refusal does not name the expression), but keeps `.submit = draw_canvas_text_label` in the inline `canvas_text_label_renderer` aggregate.
- subject: `draw_canvas_text_label`
- lanes: 2D
- fix route: compiler
- why this route: a scratch copy with only the exact declaration `void draw_canvas_text_label(Ref<CanvasRenderState>, Ref<RenderProxy2D>)` still fails at `.create_data`. There, `create_canvas_text_label_data` returns `Ref<RendererData>` but takes `CanvasRenderState` and `NodeAny` where the slot takes `RenderState` and `Node2D`. Neither link check is reachable, so no repair is carried. Only the Canvas TextLabel is covered here; the native GL TextLabel is untouched.
- first failure at: `flight/scene2d_canvas/canvas_text_label.hpp:140`
- example: `use of undeclared identifier 'draw_canvas_text_label'`
- Scoped native recheck (GCC 15.2, jobs 1, all errors shown): **1 attempted / 0 pass / 1 fail**, the header's own diagnostic. No Canvas backend, text, metrics, copied owner or narrowed input is supplied.
- compiler request: `docs/compiler-requests/retained-declaration-calls-omitted-callee.md`

### 227. `variant-cast-7dd5385f` — 1 header(s)

- cause: a functional-style cast is applied to a variant the target type cannot take
- lanes: GL
- fix route: compiler
- first failure at: `flight/effects_gl/gl_render_texture_effect.hpp:130`
- example: `no matching conversion for functional-style cast from 'const StructuralRef<RowReadonly<RowOf<shared_ptr<GlTextureRenderTarget>>>>' to 'StructuralRef<RowWritable<RowOf<Ref<GlTextureRenderTarget>>>>'`


## Scoped G2 PBR registry follow-up

The carried PBR extension registry override was repaired against 806d257. Of its nine
`no-viable-conversion-dfea2d92` headers, seven now compile. The two residuals are
`scene3d_gl/_internal_index.hpp` (readonly view to writable carrier) and
`scene3d_gl/enable_gl_pbr_extension_guards.hpp` (array of union to array of owner).
See [readonly view request](compiler-requests/readonly-view-to-writable.md) and
[array owner request](compiler-requests/array-of-union-into-array-of-owner.md).
This is a scoped update; the original G1 sweep totals above remain historical.
