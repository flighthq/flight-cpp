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
| 2 | `missing-member-b1a6d5cf` | 34 | 2D | compiler | scene2d-canvas emits `using ...get_canvas_render_state_runtime;` before the function it names is defined *(request filed)* |
| 3 | `in-operator-not-lowered-b1eae4af` | 31 | 2D, GL, PLAT, WGPU | compiler | the TypeScript `in` operator is emitted verbatim as C++ `in` at 4 root headers *(existing request extended)* |
| 4 | `undeclared-identifier-993e82f1` | 29 | 2D, 3D, PLAT, TEXT, WGPU | compiler | the template parameter name `Traits` leaks into emitted code as a free identifier *(request filed; builder4/G2)* |
| 5 | `call-has-no-matching-3882b3e9` | 27 | 2D, 3D, TEXT | binding | same braced-descriptor shape against the device binding's constrained variadic create_texture *(builder4/E0a)* |
| 6 | `other-52e846a6` | 18 | ANIM | compiler | a value_type is combined with a three-alternative variant where the declared slot expects one alternative |
| 7 | `call-has-no-matching-47410fe2` | 16 | 2D, 3D | runtime (fixed) | Number is now a callable conversion object; G2 recheck: 2 pass, 14 fail at later compiler diagnostics |
| 8 | `other-40da43f7` | 16 | 2D, 3D, ANIM | compiler | an optional<Record<...>> is initialised from a shape that is not its declared storage |
| 9 | `static-cast-assertion-9674a66c` | 15 | 2D | compiler | a TypeScript `as` on `JSON.parse` is emitted as a `static_cast` from `JsonValue` to an unrelated `Ref<T>` *(request filed)* |
| 10 | `call-has-no-matching-076d207f` | 14 | WGPU | runtime (fixed) | typed `create_pipeline_layout` descriptor overload; G2 recheck: 0 pass, 14 fail at `create_bind_group_layout` |
| 11 | `other-e9936ce6` | 13 | 3D | compiler | a pair literal the declared element type spells as `std::tuple<A, B>` is emitted as `flight::Array{...}`, so class-template ded... *(request filed)* |
| 12 | `call-has-no-matching-d6950545` | 12 | 2D, CORE | compiler | the emitted call to `get_node_child_count` matches no declared overload |
| 13 | `no-viable-conversion-9c3803c5` | 9 | 2D, WGPU | compiler | a value is passed where its declared type cannot receive it |
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
| 27 | `no-viable-conversion-f02000a9` | 6 | 2D, GL | compiler | a value is passed where its declared type cannot receive it |
| 28 | `undeclared-identifier-6e587559` | 6 | ANIM, TEXT | runtime | `equals_markup_format` declared by repair; G2 recheck: 0 pass, 6 fail at the next refused helper `parse_markup_attributes` |
| 29 | `undeclared-identifier-a74215ca` | 6 | 2D | compiler | a generic call's object-literal argument is built as `ShapeBoundsCommand<K>`, naming the callee's type parameter *(request filed)* |
| 30 | `undeclared-identifier-f5e3440f` | 6 | 3D, CORE | [compiler request](compiler-requests/retained-declaration-calls-omitted-callee.md) | createResourceLoader calls initializeResourceLoader after the compiler refuses the initializer |
| 31 | `call-has-no-matching-5a045ef4` | 5 | ANIM | compiler | the emitted call to `read_swf_shape_edge_record` matches no declared overload |
| 32 | `call-has-no-matching-b897f03f` | 5 | 3D | compiler | the emitted call to `get_node_signals` matches no declared overload |
| 33 | `call-has-no-matching-dd66c064` | 10 | ANIM | compiler | an emitted SwfReader class instance cannot be passed as its structural SwfTagReader interface to the parse callback *(existing request)* |
| 34 | `compiler-crash-aba5e9ec` | 5 | 3D | none (sweep resource limit; passes serially) | the -j8 sweep ran out of room (retried serially) |
| 35 | `invalid-operands-6d9a155d` | 5 | 2D | runtime | an operator is applied to operand types it does not accept |
| 36 | `missing-member-5e9e8a2c` | 5 | PLAT | runtime | the emitted code reads a member the target does not declare (`hit_test_graph_local_bounds`) |
| 37 | `no-viable-conversion-eaf1a6e0` | 5 | 2D | compiler | a value is passed where its declared type cannot receive it |
| 38 | `no-viable-overload-658901de` | 5 | 2D, GL | override/runtime (fixed) | concrete glass extension storage and Canvas paint variants repaired; G2: 4 pass, 1 compiler failure |
| 39 | `call-has-no-matching-4ce2cc01` | 4 | ANIM | compiler | the emitted call to a function matches no declared overload |
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
| 50 | `other-d6be4352` | 4 | ANIM | runtime | uncategorised: the first diagnostic does not name a single cause |
| 51 | `static-assertion-00a0e75d` | 4 | 3D, WGPU | compiler | a runtime static assertion rejects the emitted shape |
| 52 | `undeclared-identifier-0fc9e20f` | 4 | 2D | declaration repair (applied) | omitted artboard-import-context declaration supplied; G2: 0 pass, 4 fail at the next omitted callee; function remains unlinkable |
| 53 | `undeclared-identifier-69b85faa` | 4 | PLAT | declaration repair (applied) | omitted untrack callee declaration supplied; G2: 0 pass, 4 fail at the next omitted callee; function remains unlinkable |
| 58 | `undeclared-identifier-7d64d9c1` | 4 | GL | compiler | the emitter refuses the `glCustomShaderEffectRunner` binding and its resolver but still emits the registration call that passes... *(request filed)* |
| 55 | `undeclared-identifier-862d162a` | 4 | 3D | runtime | the emitted code names `build_gltf_animations`, which nothing declares |
| 56 | `undeclared-identifier-f2b46b01` | 4 | 3D | runtime | the emitted code names `list_standard_pbr_material_textures`, which nothing declares |
| 57 | `call-has-no-matching-3b2f72dd` | 3 | PLAT | compiler | the emitted call to `is_optional_number` matches no declared overload |
| 58 | `call-has-no-matching-56e47749` | 3 | GL | compiler | the emitted call to `compute_node_root_local_bounds_rectangle` matches no declared overload |
| 59 | `invalid-operands-98cd7094` | 3 | 2D | runtime | an operator is applied to operand types it does not accept |
| 60 | `missing-member-95ead583` | 3 | 3D | [compiler request](compiler-requests/cube-texture-width-and-completeness.md#g2-gl-environment-face-revisions-use-the-same-selected-owner-read) | nullable cube-face source retains seven real owners, but version is read on the variant itself |
| 61 | `missing-member-b22d5c52` | 3 | 2D | runtime | the emitted code reads a member the target does not declare (`resolve_canvas_texture_window_source`) |
| 62 | `missing-member-c36ea2d4` | 3 | 2D | binding | the web renderer needs shared CSS style state; SDL exposes copied shell values with no opacity field *(request filed)* |
| 63 | `no-viable-conversion-4ca7626f` | 3 | ANIM | compiler | a value is passed where its declared type cannot receive it |
| 64 | `no-viable-conversion-b3064463` | 3 | GL | compiler | a value is passed where its declared type cannot receive it |
| 65 | `no-viable-conversion-b3f3c7c6` | 3 | 3D | compiler | a value is passed where its declared type cannot receive it |
| 66 | `no-viable-conversion-b5b698fe` | 3 | 2D | compiler | a value is passed where its declared type cannot receive it |
| 67 | `other-3d2efc1d` | 3 | 2D | runtime | uncategorised: the first diagnostic does not name a single cause |
| 68 | `other-41f4a462` | 3 | ANIM | runtime | uncategorised: the first diagnostic does not name a single cause |
| 69 | `other-6a1604d5` | 3 | PLAT | runtime | uncategorised: the first diagnostic does not name a single cause |
| 70 | `other-b860ec87` | 3 | 2D, GL | emission repair (applied) | omitted nominal-owner recovery supplied at the `return parent` branch; G2: 0 pass, 3 fail at the readonly-vs-nominal comparison |
| 71 | `other-f9c5bc10` | 3 | 2D | compiler | an `as` assertion over a `flight::JsonValue` is lowered with the erased-value `object_if` pattern, which a tagged JSON value ca... *(request filed)* |
| 72 | `redefinition-c11d96e9` | 3 | TEXT | runtime (fixed) | the bitmap_font_record override's `#pragma once` was commented out; G2 recheck: 0 pass, 3 fail at `bitmap_font_fnt.hpp:187` |
| 73 | `undeclared-identifier-145c75d7` | 3 | TEXT | runtime | the emitted code names `initialize_glyph_source_from_glyph_atlas`, which nothing declares |
| 74 | `undeclared-identifier-2cdb69a6` | 3 | PLAT | runtime | the emitted code names `channel_elements`, which nothing declares |
| 75 | `undeclared-identifier-564c29cc` | 3 | TEXT | runtime | the emitted code names `format_date_value`, which nothing declares |
| 76 | `undeclared-identifier-6a187568` | 3 | 3D | runtime | the emitted code names `count_ascii_stl_facets`, which nothing declares |
| 77 | `undeclared-identifier-dc087108` | 3 | PLAT | runtime | the emitted code names `load_video_resource_from_url`, which nothing declares |
| 78 | `call-has-no-matching-84410867` | 2 | 3D | compiler | the emitted call to `buffer_data` matches no declared overload |
| 79 | `call-has-no-matching-d42144f0` | 2 | ANIM | compiler | the emitted call to `push` matches no declared overload |
| 80 | `missing-member-da12a523` | 2 | 3D | runtime | the emitted code reads a member the target does not declare (`parse_obj_material_library`) |
| 81 | `missing-member-dd9b4baa` | 2 | 3D, GL | runtime | the emitted code reads a member the target does not declare (`renderbuffer_binding`) *(G2: binding added; follow-on `no-matching-conversion-bc278253` fixed: 1 pass, 1 to `missing-member-95ead583`)* |
| 82 | `no-viable-conversion-303c42f1` | 2 | ANIM | compiler | a value is passed where its declared type cannot receive it |
| 83 | `other-67dc02c6` | 2 | 2D | runtime | uncategorised: the first diagnostic does not name a single cause |
| 84 | `other-6f513db2` | 2 | 2D, 3D | runtime | uncategorised: the first diagnostic does not name a single cause |
| 85 | `other-86d0b50b` | 2 | 2D, 3D | runtime | uncategorised: the first diagnostic does not name a single cause |
| 86 | `redefinition-caa49360` | 2 | 3D | runtime | a declaration is emitted twice |
| 87 | `undeclared-identifier-1048386b` | 2 | 3D | runtime | the emitted code names `md5_material_handler`, which nothing declares |
| 88 | `undeclared-identifier-20fa691d` | 2 | 2D | runtime | the emitted code names `draw_gl_raster_shape`, which nothing declares |
| 89 | `undeclared-identifier-225478b5` | 2 | 2D | runtime | the emitted code names `submit_gl_bitmap_text`, which nothing declares |
| 90 | `undeclared-identifier-3e98002c` | 2 | 3D | runtime | the emitted code names `initialize_awd2_parse_state`, which nothing declares |
| 91 | `undeclared-identifier-644a9b33` | 2 | 2D | runtime | the emitted code names `clip_programs`, which nothing declares |
| 92 | `undeclared-identifier-7ce7bd18` | 2 | 3D | runtime | the emitted code names `md2_skin_handler`, which nothing declares |
| 93 | `undeclared-identifier-86212434` | 2 | GL | runtime | the emitted code names `gl_ssao_effect_runner`, which nothing declares |
| 94 | `undeclared-identifier-a02f3fb2` | 2 | 3D | runtime | the emitted code names `build_gltf_skins`, which nothing declares |
| 95 | `undeclared-identifier-e39f9d67` | 2 | 3D | runtime | the emitted code names `resolve_gl_modifier_snippet_source`, which nothing declares |
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
| 110 | `invalid-operands-dc1c09c4` | 1 | PLAT | runtime | an operator is applied to operand types it does not accept |
| 111 | `missing-member-1620083c` | 1 | 2D | runtime | the emitted code reads a member the target does not declare (`mix_blend_mode`) |
| 112 | `missing-member-1a2e58cb` | 1 | 2D | runtime | the emitted code reads a member the target does not declare (`register_dom_texture_resolver`) |
| 113 | `missing-member-1ce5ed2e` | 1 | 3D | runtime | the emitted code reads a member the target does not declare (`physics_3d_cone_twist_joint_solver`) |
| 114 | `missing-member-42783657` | 1 | 2D | runtime | the emitted code reads a member the target does not declare (`font`) |
| 115 | `missing-member-5ac43ed0` | 1 | 3D | runtime | the emitted code reads a member the target does not declare (`element`) |
| 116 | `missing-member-703ef98e` | 1 | 2D | runtime | the emitted code reads a member the target does not declare (`lottie_additive_mask_handler`) |
| 117 | `missing-member-8aa8170f` | 1 | ANIM | runtime | the emitted code reads a member the target does not declare (`swf_video_handler`) |
| 118 | `missing-member-ad3558ee` | 1 | ANIM | runtime | the emitted code reads a member the target does not declare (`physics_2d_distance_joint_solver`) |
| 119 | `missing-member-b5408b35` | 1 | 3D | runtime | the emitted code reads a member the target does not declare (`get_node_world_matrix4`) |
| 120 | `missing-member-d7bf452c` | 1 | 2D | runtime | the emitted code reads a member the target does not declare (`begin`) |
| 121 | `missing-member-dae8c839` | 4 | TEXT | runtime | the emitted code reads a member the target does not declare (`width`) |
| 122 | `missing-member-dcf85c36` | 1 | 3D | runtime | the emitted code reads a member the target does not declare (`collect_md5_anim_features`) |
| 123 | `missing-member-ee398c94` | 1 | 2D | runtime | the emitted code reads a member the target does not declare (`register_canvas_texture_resolver`) |
| 124 | `missing-member-fd2be1bc` | 1 | TEXT | runtime | the emitted code reads a member the target does not declare (`object_if`) |
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
| 140 | `no-viable-conversion-cef6403d` | 1 | 3D | compiler | a value is passed where its declared type cannot receive it |
| 141 | `no-viable-conversion-dc893fc7` | 1 | GL | compiler | a value is passed where its declared type cannot receive it |
| 142 | `no-viable-conversion-eabaf455` | 1 | 3D | compiler | a value is passed where its declared type cannot receive it |
| 143 | `no-viable-conversion-f7267dc5` | 1 | ANIM | compiler | a value is passed where its declared type cannot receive it |
| 144 | `other-1e3a0d33` | 1 | ANIM | runtime | uncategorised: the first diagnostic does not name a single cause |
| 145 | `other-4703cbbe` | 1 | 3D | runtime | uncategorised: the first diagnostic does not name a single cause |
| 146 | `other-5dfe27ec` | 1 | ANIM | runtime | uncategorised: the first diagnostic does not name a single cause |
| 147 | `other-acb4b229` | 1 | 2D | runtime | uncategorised: the first diagnostic does not name a single cause |
| 148 | `static-assertion-18059635` | 1 | 3D | compiler | a runtime static assertion rejects the emitted shape |
| 149 | `static-assertion-261f0db6` | 1 | 3D | compiler | a runtime static assertion rejects the emitted shape |
| 150 | `undeclared-identifier-0168969a` | 1 | 2D | runtime | the emitted code names `dom_scene_2d_clip_hooks`, which nothing declares |
| 151 | `undeclared-identifier-07640d32` | 1 | GL | runtime | the emitted code names `gl_outline_effect_runner`, which nothing declares |
| 152 | `undeclared-identifier-076c02b1` | 1 | PLAT | runtime | the emitted code names `resolve_tilemap_cell_index`, which nothing declares |
| 153 | `undeclared-identifier-09cc9e86` | 1 | GL | runtime | the emitted code names `gl_film_grain_effect_runner`, which nothing declares |
| 154 | `undeclared-identifier-0b8d3b44` | 1 | GL | runtime | the emitted code names `box_blur_shaders`, which nothing declares |
| 155 | `undeclared-identifier-0c07642f` | 1 | GL | runtime | the emitted code names `gl_glitch_effect_runner`, which nothing declares |
| 156 | `undeclared-identifier-11d6fcbe` | 1 | GL | runtime | the emitted code names `gl_chromatic_aberration_effect_runner`, which nothing declares |
| 157 | `undeclared-identifier-1462818e` | 1 | 2D | runtime | the emitted code names `ensure_gl_velocity_program`, which nothing declares |
| 158 | `undeclared-identifier-17cb8386` | 1 | GL | runtime | the emitted code names `gl_bokeh_depth_of_field_effect_runner`, which nothing declares |
| 159 | `undeclared-identifier-1892fccb` | 1 | PLAT | runtime | the emitted code names `resolve_text_char_index`, which nothing declares |
| 160 | `undeclared-identifier-1a056bbc` | 1 | TEXT | runtime | the emitted code names `get_number_format`, which nothing declares |
| 161 | `undeclared-identifier-1d8e072a` | 1 | 2D | runtime | the emitted code names `draw_gl_scale9_sprite`, which nothing declares |
| 162 | `undeclared-identifier-2110ec37` | 1 | WGPU | runtime | the emitted code names `draw_wgpu_sprite`, which nothing declares |
| 163 | `undeclared-identifier-2363216f` | 1 | GL | runtime | the emitted code names `gl_dither_effect_runner`, which nothing declares |
| 164 | `undeclared-identifier-2437e103` | 1 | 3D | runtime | the emitted code names `parse_triangle_geometry_block`, which nothing declares |
| 165 | `undeclared-identifier-251d1ba2` | 1 | 3D | runtime | the emitted code names `collect_pending_texture_owners`, which nothing declares |
| 166 | `undeclared-identifier-2d9cd75f` | 1 | PLAT | runtime | the emitted code names `hit_test_shape_fill`, which nothing declares |
| 167 | `undeclared-identifier-3017e8aa` | 1 | GL | runtime | the emitted code names `acquire_gl_render_texture`, which nothing declares |
| 168 | `undeclared-identifier-332a5586` | 1 | 2D | runtime | the emitted code names `draw_dom_scale9_sprite`, which nothing declares |
| 169 | `undeclared-identifier-34adf2ab` | 1 | ANIM | runtime | the emitted code names `parse_spine_slots`, which nothing declares |
| 170 | `undeclared-identifier-3582eeb5` | 1 | GL | runtime | the emitted code names `gl_screen_space_fog_effect_runner`, which nothing declares |
| 171 | `undeclared-identifier-3cb2bb54` | 1 | GL | runtime | the emitted code names `invert_tint_shaders`, which nothing declares |
| 172 | `undeclared-identifier-3e7f03c4` | 1 | GL | runtime | the emitted code names `gl_sharpen_effect_runner`, which nothing declares |
| 173 | `undeclared-identifier-44058981` | 1 | GL | runtime | the emitted code names `gl_blend_effect_runner`, which nothing declares |
| 174 | `undeclared-identifier-49b86813` | 1 | ANIM | runtime | the emitted code names `solve_skeleton_2d_ik_constraint`, which nothing declares |
| 175 | `undeclared-identifier-4fc6ab69` | 1 | 2D | runtime | the emitted code names `create_svg_image_node`, which nothing declares |
| 176 | `undeclared-identifier-514a5b7b` | 1 | 2D | runtime | the emitted code names `submit_gl_tilemap`, which nothing declares |
| 177 | `undeclared-identifier-51763a3c` | 1 | GL | runtime | the emitted code names `gl_tone_map_effect_runner`, which nothing declares |
| 178 | `undeclared-identifier-529e49d7` | 1 | GL | runtime | the emitted code names `gl_directional_blur_effect_runner`, which nothing declares |
| 179 | `undeclared-identifier-58fb713b` | 1 | 2D | runtime | the emitted code names `draw_dom_scale9_shape`, which nothing declares |
| 180 | `undeclared-identifier-5be7be47` | 1 | GL | runtime | the emitted code names `gl_scanlines_effect_runner`, which nothing declares |
| 181 | `undeclared-identifier-6647740e` | 1 | 2D | runtime | the emitted code names `create_rive_relative_transforms`, which nothing declares |
| 182 | `undeclared-identifier-66ea97f8` | 1 | WGPU | runtime | the emitted code names `initialize_wgpu_screen_render_target`, which nothing declares |
| 183 | `undeclared-identifier-67f2c521` | 1 | GL | runtime | the emitted code names `gl_god_rays_effect_runner`, which nothing declares |
| 184 | `undeclared-identifier-6ad30a3d` | 1 | GL | runtime | the emitted code names `gl_crt_effect_runner`, which nothing declares |
| 185 | `undeclared-identifier-6dd20ce4` | 1 | 2D | runtime | the emitted code names `submit_gl_quad_batch`, which nothing declares |
| 186 | `undeclared-identifier-757c2c8a` | 1 | 2D | runtime | the emitted code names `get_gl_scale9_shape_data`, which nothing declares |
| 187 | `undeclared-identifier-785ed669` | 1 | PLAT | runtime | the emitted code names `resolve_quad_batch_instance_index`, which nothing declares |
| 188 | `undeclared-identifier-7905640c` | 1 | 2D | runtime | the emitted code names `canvas_clip_hooks`, which nothing declares |
| 189 | `undeclared-identifier-7d85a310` | 1 | GL | runtime | the emitted code names `gl_lens_flare_effect_runner`, which nothing declares |
| 190 | `undeclared-identifier-7e3a780f` | 1 | PLAT | runtime | the emitted code names `hit_test_sprite_alpha`, which nothing declares |
| 191 | `undeclared-identifier-7fd1550e` | 1 | GL | runtime | the emitted code names `gl_camera_motion_blur_effect_runner`, which nothing declares |
| 192 | `undeclared-identifier-7ff1c319` | 1 | GL | runtime | the emitted code names `gl_vignette_effect_runner`, which nothing declares |
| 193 | `undeclared-identifier-8653adf0` | 1 | 2D | runtime | the emitted code names `ensure_scene_2d_runtime`, which nothing declares |
| 194 | `undeclared-identifier-898d7996` | 1 | GL | runtime | the emitted code names `gl_displacement_effect_runner`, which nothing declares |
| 195 | `undeclared-identifier-8ae633dc` | 1 | GL | runtime | the emitted code names `gl_smaa_effect_runner`, which nothing declares |
| 196 | `undeclared-identifier-8b3950d6` | 1 | GL | runtime | the emitted code names `gl_motion_blur_effect_runner`, which nothing declares |
| 197 | `undeclared-identifier-8e604c65` | 1 | GL | runtime | the emitted code names `gl_sketch_effect_runner`, which nothing declares |
| 198 | `undeclared-identifier-9562ef07` | 1 | 2D | runtime | the emitted code names `draw_canvas_tilemap`, which nothing declares |
| 199 | `undeclared-identifier-9aaa5b7f` | 1 | WGPU | runtime | the emitted code names `acquire_wgpu_screen_render_target_capture_texture`, which nothing declares |
| 200 | `undeclared-identifier-a3f09fb8` | 1 | 2D | runtime | the emitted code names `draw_dom_shape`, which nothing declares |
| 201 | `undeclared-identifier-a4a22a69` | 1 | GL | runtime | the emitted code names `gl_lens_distortion_effect_runner`, which nothing declares |
| 202 | `undeclared-identifier-a89b9a39` | 1 | GL | runtime | the emitted code names `gl_bitmap_displacement_effect_runner`, which nothing declares |
| 203 | `undeclared-identifier-aa084556` | 1 | PLAT | runtime | the emitted code names `is_track_list`, which nothing declares |
| 204 | `undeclared-identifier-ae594b8a` | 1 | 2D | runtime | the emitted code names `append_lottie_shape_items`, which nothing declares |
| 205 | `undeclared-identifier-af7a1054` | 1 | GL | runtime | the emitted code names `gl_median_effect_runner`, which nothing declares |
| 206 | `undeclared-identifier-b15d9d15` | 1 | ANIM | runtime | the emitted code names `parse_dragon_bones_bones`, which nothing declares |
| 207 | `undeclared-identifier-befaa2d6` | 1 | 2D | runtime | the emitted code names `compute_scissor_rect`, which nothing declares |
| 208 | `undeclared-identifier-c162d0c0` | 1 | ANIM | runtime | the emitted code names `solve_skeleton_2d_transform_constraint`, which nothing declares |
| 209 | `undeclared-identifier-c29489a5` | 1 | GL | runtime | the emitted code names `gl_bloom_effect_runner`, which nothing declares |
| 210 | `undeclared-identifier-c3c55c65` | 1 | GL | runtime | the emitted code names `gl_composite_effect_runner`, which nothing declares |
| 211 | `undeclared-identifier-c45cc1bb` | 1 | GL | runtime | the emitted code names `gl_fxaa_effect_runner`, which nothing declares |
| 212 | `undeclared-identifier-c7551b38` | 1 | GL | runtime | the emitted code names `gl_pixelate_effect_runner`, which nothing declares |
| 213 | `undeclared-identifier-cc7c5006` | 1 | ANIM | runtime | the emitted code names `solve_skeleton_2d_path_constraint`, which nothing declares |
| 214 | `undeclared-identifier-cf2fed2b` | 1 | GL | runtime | the emitted code names `gl_posterize_effect_runner`, which nothing declares |
| 215 | `undeclared-identifier-d14c2215` | 1 | GL | runtime | the emitted code names `gl_convolution_effect_runner`, which nothing declares |
| 216 | `undeclared-identifier-d687475c` | 1 | GL | runtime | the emitted code names `gl_halftone_effect_runner`, which nothing declares |
| 217 | `undeclared-identifier-d7a1d1b7` | 1 | GL | runtime | the emitted code names `gl_tilt_shift_effect_runner`, which nothing declares |
| 218 | `undeclared-identifier-e3d6c4e4` | 1 | GL | runtime | the emitted code names `apply_gl_gaussian_blur_pass`, which nothing declares |
| 219 | `undeclared-identifier-e4a7b51e` | 1 | GL | runtime | the emitted code names `gl_lens_dirt_effect_runner`, which nothing declares |
| 220 | `undeclared-identifier-e4eb111d` | 1 | GL | runtime | the emitted code names `gl_kuwahara_effect_runner`, which nothing declares |
| 221 | `undeclared-identifier-e7bf11f9` | 1 | WGPU | runtime | the emitted code names `draw_wgpu_scale9_sprite`, which nothing declares |
| 222 | `undeclared-identifier-ebf3999b` | 1 | GL | runtime | the emitted code names `gl_radial_blur_effect_runner`, which nothing declares |
| 223 | `undeclared-identifier-f020c1f7` | 1 | GL | runtime | the emitted code names `gl_white_balance_effect_runner`, which nothing declares |
| 224 | `undeclared-identifier-f6affb68` | 1 | 3D | runtime | the emitted code names `resolve_transmission_volume`, which nothing declares |
| 225 | `undeclared-identifier-f91f1e72` | 1 | 2D | runtime | the emitted code names `get_gl_text_label_data`, which nothing declares *(builder4/E0a)* |
| 226 | `undeclared-identifier-fcbdae42` | 1 | 2D | runtime | the emitted code names `draw_canvas_text_label`, which nothing declares |
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

### 2. `missing-member-b1a6d5cf` — 34 header(s)

- cause: scene2d-canvas emits `using ...get_canvas_render_state_runtime;` before the function it names is defined
- subject: `get_canvas_render_state_runtime`
- lanes: 2D
- fix route: compiler
- why this route: scene2d-canvas emits `using ...get_canvas_render_state_runtime;` before the function it names is defined
- first failure at: `flight/scene2d_canvas/canvas_render_state.hpp:69`
- example: `no member named 'get_canvas_render_state_runtime' in namespace 'scene2d_canvas'`
- compiler request: `docs/compiler-requests/local-reexport-using-before-declaration.md`

### 3. `in-operator-not-lowered-b1eae4af` — 31 header(s)

- cause: the TypeScript `in` operator is emitted verbatim as C++ `in`
- lanes: 2D, GL, PLAT, WGPU
- fix route: compiler; the `render_registry_guards` receiver also needs a structural carrier
- first failure at: `flight/interaction/interaction_manager.hpp:510`
- example: `expected ')'`
- existing request extended: [membership test lowered literally](compiler-requests/membership-test-lowered-literally.md);
  related: [Interaction runtime membership](compiler-requests/interaction-runtime-property-membership.md)

Only four headers contain the failing expression. By include walk, the other 27 reach
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

Eight more headers contain the expression but fail first on another cluster's diagnostic. They are listed
in the request as latent sites and are not reclassified here. No source patch, repair, override or runtime
change was made. An own-keys predicate, value read or
nominal downcast would narrow the semantics, and no erased TypeScript form is simpler than `in`.

**G1 status retained: 0/31 pass, 31 fail.** No C++ recompile was run because no code changed.

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

### 5. `call-has-no-matching-3882b3e9` — 27 header(s) *(builder4/E0a)*

- cause: same braced-descriptor shape against the device binding's constrained variadic create_texture
- subject: `create_texture`
- lanes: 2D, 3D, TEXT
- fix route: binding
- why this route: same braced-descriptor shape against the device binding's constrained variadic create_texture
- first failure at: `flight/scene3d_formats/shared.hpp:100`
- example: `no matching function for call to 'create_texture'`
- owned by: builder4 (E0a: TextLabel / layout / Scene2D GL text)

### 6. `other-52e846a6` — 18 header(s)

- cause: a value_type is combined with a three-alternative variant where the declared slot expects one alternative
- lanes: ANIM
- fix route: compiler
- why this route: a value_type is combined with a three-alternative variant where the declared slot expects one alternative
- first failure at: `flight/skeleton2d/skeleton2d_animation_target.hpp:217`
- example: `incompatible operand types ('value_type' and 'variant<Ref<Attachment2D>, Null, Undefined>' )`

- G2 check: **0/18 compile**, 18 failures and zero unattempted at `-j1`; every row retains the same root diagnostic.
- Root: a nullable array element is not lifted into the slot's distinct reference/null/undefined carrier.
- Compiler request: [nullable array element into an optional-nullable slot](compiler-requests/nullable-array-element-into-dual-sentinel-slot.md).
- The cheap branch-assignment source rewrite still emits an optional directly into the variant. No source patch, runtime change, repair or override is carried; the cluster remains open.

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

### 12. `call-has-no-matching-d6950545` — 12 header(s)

- cause: the emitted call to `get_node_child_count` matches no declared overload
- subject: `get_node_child_count`
- lanes: 2D, CORE
- fix route: compiler
- first failure at: `flight/command/command_binding.hpp:155`
- example: `no matching function for call to 'get_node_child_count'`

### 13. `no-viable-conversion-9c3803c5` — 9 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 2D, WGPU
- fix route: compiler
- first failure at: `flight/scene2d_canvas/canvas_node_2d.hpp:62`
- example: `no viable conversion from 'optional<Ref<RendererData>> (Ref<RenderState>, NodeAny)' ') to 'function<optional<Ref<RendererData>> (Ref<RenderState>, Node2D)>' >')`

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

- cause: a value is passed where its declared type cannot receive it
- lanes: 2D, GL
- fix route: compiler
- first failure at: `flight/render/render_proxy_adapter.hpp:54`
- example: `no viable conversion from 'Ref<RenderProxy2D>' to 'Ref<RenderProxy>'`

### 28. `undeclared-identifier-6e587559` — 6 header(s)

- cause: `text_markup.hpp` keeps calls to helpers its own module refused. `equalsMarkupFormat` is refused because `aValue.length` on an optional `TextFormat` member has no present-value proof, but the retained `push_markup_range` still calls it.
- subject: `parse_markup_attributes` (was `equals_markup_format`)
- lanes: ANIM, TEXT
- fix route: runtime (repair)
- why this route: G2, measured with GCC 15.2 at `-j1`. All three `declare-refused-function` witnesses hold in `text_markup.hpp` (the NOT GENERATED marker, the retained call, no definition). So the repair `text-markup-declare-refused-equals-markup-format` declares it with the emitter's own carrier (`StructuralRef<RowReadonly<RowOf<Ref<TextFormat>>>>` twice, returning `bool`). There is no body: calling it fails to link.
- G2 recheck: **0/6 compile, 6 fail** at the next retained call into a refused helper in the same header, `parse_markup_attributes` (`text_markup.hpp:407`). The refused helpers still called are `parseMarkupAttributes`, `isSafeMarkupHref`, `normalizeMarkupTagResult`, `resolveMarkupFormatRuns` and `parseTextMarkupInternal`. The last one takes a callback whose emitted carrier the retained calls do not fix, so declarations alone do not finish this header.
- first failure at: `flight/text_markup/text_markup.hpp:407`
- example: `'parse_markup_attributes' was not declared in this scope`

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

### 31. `call-has-no-matching-5a045ef4` — 5 header(s)

- cause: the emitted call to `read_swf_shape_edge_record` matches no declared overload
- subject: `read_swf_shape_edge_record`
- lanes: ANIM
- fix route: compiler
- first failure at: `flight/swf/swf_shape.hpp:241`
- example: `no matching function for call to 'read_swf_shape_edge_record'`

### 32. `call-has-no-matching-b897f03f` — 5 header(s)

- cause: the emitted call to `get_node_signals` matches no declared overload
- subject: `get_node_signals`
- lanes: 3D
- fix route: compiler
- first failure at: `flight/scene3d/billboard.hpp:88`
- example: `no matching function for call to 'get_node_signals'`

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

- cause: an operator is applied to operand types it does not accept
- lanes: 2D
- fix route: runtime
- first failure at: `flight/shape/morph_shape_paint.hpp:77`
- example: `invalid operands to binary expression ('value_type' and 'String')`

### 36. `missing-member-5e9e8a2c` — 5 header(s)

- cause: the emitted code reads a member the target does not declare (`hit_test_graph_local_bounds`)
- subject: `hit_test_graph_local_bounds`
- lanes: PLAT
- fix route: runtime
- first failure at: `flight/interaction/display_hit_tests.hpp:28`
- example: `no member named 'hit_test_graph_local_bounds' in namespace 'interaction'`

### 37. `no-viable-conversion-eaf1a6e0` — 5 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 2D
- fix route: compiler
- first failure at: `flight/scene2d_canvas/canvas_bitmap_text.hpp:116`
- example: `no viable conversion from 'optional<Ref<RendererData>> (Ref<RenderState>, NodeAny)' ') to 'function<optional<Ref<RendererData>> (Ref<RenderState>, Ref<Sprite>)>' >')`

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

### 39. `call-has-no-matching-4ce2cc01` — 4 header(s)

- cause: the emitted call to a function matches no declared overload
- lanes: ANIM
- fix route: compiler
- first failure at: `flight/skeleton2d_formats/spine_json_animations_handler.hpp:66`
- example: `no matching function for call to object of type 'function<void (shared_ptr<SpineJsonTimelineContext>, String, Record<String, Any>)>'`

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

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/particles_formats/pixi_parse.hpp:55`
- example: `no matching conversion for static_cast from 'JsonValue' to 'PixiRaw'`

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

- cause: the emitted code names `build_gltf_animations`, which nothing declares
- subject: `build_gltf_animations`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_formats/gltf_animations.hpp:209`
- example: `use of undeclared identifier 'build_gltf_animations'`

### 56. `undeclared-identifier-f2b46b01` — 4 header(s)

- cause: the emitted code names `list_standard_pbr_material_textures`, which nothing declares
- subject: `list_standard_pbr_material_textures`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_resources/scene_material_texture_registry.hpp:162`
- example: `use of undeclared identifier 'list_standard_pbr_material_textures'`

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

### 59. `invalid-operands-98cd7094` — 3 header(s)

- cause: an operator is applied to operand types it does not accept
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_gl/enable_gl_color_adjustment_guards.hpp:35`
- example: `invalid operands to binary expression ('variant<function<void (Ref<GlRenderState>, variant<Array<double>, Ref<ColorScaleBias>, Ref<TintMaterialData>>)>, Null, Undefined>' >, Null, Undefined>') and 'void ()')`

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

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: 2D
- fix route: runtime
- first failure at: `flight/textureatlas_formats/texture_atlas_detect.hpp:67`
- example: `type 'Any' does not provide a subscript operator`

### 68. `other-41f4a462` — 3 header(s)

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/skeleton2d_formats/dragon_bones_parse_helpers.hpp:61`
- example: `incompatible operand types ('NamedProperties' and 'Record<String, Any>')`

### 69. `other-6a1604d5` — 3 header(s)

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: PLAT
- fix route: runtime
- first failure at: `flight/tray/enable_tray_guards.hpp:25`
- example: `no type named 'set_tray_animation_guard' in namespace 'tray'`

### 70. `other-b860ec87` — 3 header(s), expression repaired

- original cause: `resolveInheritedColorScaleBias` returned a readonly structural row directly as its
  declared nominal `ColorScaleBias` owner, and the emitter dropped the recovery cast on that branch
- lanes: 2D, GL
- fix route: emission repair (text only), with the compiler request still open
- compiler request: [readonly-row-return-owner-recovery.md](compiler-requests/readonly-row-return-owner-recovery.md)

`render-inherited-color-scale-bias-parent-owner-recovery` adds exactly the
`flight::structural_ref_cast<Ref<ColorScaleBias>>(...)` the emitter already emits on the sibling
`return local` branch. The cast recovers the exact shared owner (and throws `bad_cast` when the view is
not that object), so one object, one control block and one lifetime are kept; no owner is copied, no
writable row is formed and no optional is reinterpreted.

Written by **normal package generation** at the exact pins (`flight` a627849 / `flight-compiler`
806d257): the pre-write check named exactly `enable_color_adjustments.hpp` and `manifest.json`, and the
write produced only those two.

G2: **0/3 compile, 3 fail, 0 unattempted** at `-j1` with Clang 22.1.8. All three now fail one family
further, at the strict comparison between the nominal optional and the readonly row.

| follow-on cluster | owned headers | first failure | route / cause |
| --- | --- | --- | --- |
| `invalid-operands-6c12cdba` | 3 | `flight/render/enable_color_adjustments.hpp:94` | compiler: `previous != parent.value()` compares `optional<Ref<ColorScaleBias>>` with a readonly `StructuralRef`; [existing request](compiler-requests/readonly-structural-nominal-identity-comparison.md). The same header keeps a readonly-to-writable projection at `:95` and omitted matrix helpers at `:147`-`:149` and `:198`. |

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

### 73. `undeclared-identifier-145c75d7` — 3 header(s)

- cause: the emitted code names `initialize_glyph_source_from_glyph_atlas`, which nothing declares
- subject: `initialize_glyph_source_from_glyph_atlas`
- lanes: TEXT
- fix route: runtime
- first failure at: `flight/glyphatlas/glyph_source.hpp:78`
- example: `use of undeclared identifier 'initialize_glyph_source_from_glyph_atlas'`

### 74. `undeclared-identifier-2cdb69a6` — 3 header(s)

- cause: the emitted code names `channel_elements`, which nothing declares
- subject: `channel_elements`
- lanes: PLAT
- fix route: runtime
- first failure at: `flight/media/video_channel.hpp:91`
- example: `use of undeclared identifier 'channel_elements'`

### 75. `undeclared-identifier-564c29cc` — 3 header(s)

- cause: the emitted code names `format_date_value`, which nothing declares
- subject: `format_date_value`
- lanes: TEXT
- fix route: runtime
- first failure at: `flight/intl/datetime.hpp:45`
- example: `use of undeclared identifier 'format_date_value'`

### 76. `undeclared-identifier-6a187568` — 3 header(s)

- cause: the emitted code names `count_ascii_stl_facets`, which nothing declares
- subject: `count_ascii_stl_facets`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_formats/stl_features.hpp:154`
- example: `use of undeclared identifier 'count_ascii_stl_facets'`

### 77. `undeclared-identifier-dc087108` — 3 header(s)

- cause: the emitted code names `load_video_resource_from_url`, which nothing declares
- subject: `load_video_resource_from_url`
- lanes: PLAT
- fix route: runtime
- first failure at: `flight/video/video_resource_from.hpp:66`
- example: `use of undeclared identifier 'load_video_resource_from_url'`

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

### 80. `missing-member-da12a523` — 2 header(s)

- cause: the emitted code reads a member the target does not declare (`parse_obj_material_library`)
- subject: `parse_obj_material_library`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_formats/obj_material_model.hpp:58`
- example: `no member named 'parse_obj_material_library' in namespace 'scene3d_formats'`

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

### 83. `other-67dc02c6` — 2 header(s)

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_canvas/canvas_element.hpp:34`
- example: `no matching conversion for static_cast from 'NativeSurfaceHandle' to 'host_sdl::GlCanvas'`

### 84. `other-6f513db2` — 2 header(s)

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: 2D, 3D
- fix route: runtime
- first failure at: `flight/scene2d_gl/gl_particle_emitter_2d.hpp:58`
- example: `non-constant-expression cannot be narrowed from type 'int' to 'double' in initializer list [-Wc++11-narrowing]`

### 85. `other-86d0b50b` — 2 header(s)

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: 2D, 3D
- fix route: runtime
- first failure at: `flight/scene2d_gl/explain_gl_scene_2d_coverage.hpp:58`
- example: `initialization of non-aggregate type 'SceneCoverageEntry' with a designated initializer list`

### 86. `redefinition-caa49360` — 2 header(s)

- cause: a declaration is emitted twice
- subject: `ensure_node_local_matrix4`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/picking/scene_hit_attributes.hpp:21`
- example: `redefinition of 'ensure_node_local_matrix4'`

### 87. `undeclared-identifier-1048386b` — 2 header(s)

- cause: the emitted code names `md5_material_handler`, which nothing declares
- subject: `md5_material_handler`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_formats/md5_material_handler.hpp:69`
- example: `use of undeclared identifier 'md5_material_handler'`

### 88. `undeclared-identifier-20fa691d` — 2 header(s)

- cause: the emitted code names `draw_gl_raster_shape`, which nothing declares
- subject: `draw_gl_raster_shape`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_gl/gl_raster_shape_renderer.hpp:173`
- example: `use of undeclared identifier 'draw_gl_raster_shape'`

### 89. `undeclared-identifier-225478b5` — 2 header(s)

- cause: the emitted code names `submit_gl_bitmap_text`, which nothing declares
- subject: `submit_gl_bitmap_text`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_gl/gl_bitmap_text.hpp:163`
- example: `use of undeclared identifier 'submit_gl_bitmap_text'`

### 90. `undeclared-identifier-3e98002c` — 2 header(s)

- cause: the emitted code names `initialize_awd2_parse_state`, which nothing declares
- subject: `initialize_awd2_parse_state`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_formats/awd2_block_dispatch.hpp:86`
- example: `use of undeclared identifier 'initialize_awd2_parse_state'`

### 91. `undeclared-identifier-644a9b33` — 2 header(s)

- cause: the emitted code names `clip_programs`, which nothing declares
- subject: `clip_programs`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_gl/gl_clip_contours.hpp:72`
- example: `use of undeclared identifier 'clip_programs'`

### 92. `undeclared-identifier-7ce7bd18` — 2 header(s)

- cause: the emitted code names `md2_skin_handler`, which nothing declares
- subject: `md2_skin_handler`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_formats/md2_skin_handler.hpp:123`
- example: `use of undeclared identifier 'md2_skin_handler'`

### 93. `undeclared-identifier-86212434` — 2 header(s)

- cause: the emitted code names `gl_ssao_effect_runner`, which nothing declares
- subject: `gl_ssao_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_ssao_effect.hpp:88`
- example: `use of undeclared identifier 'gl_ssao_effect_runner'`

### 94. `undeclared-identifier-a02f3fb2` — 2 header(s)

- cause: the emitted code names `build_gltf_skins`, which nothing declares
- subject: `build_gltf_skins`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_formats/gltf_skins.hpp:116`
- example: `use of undeclared identifier 'build_gltf_skins'`

### 95. `undeclared-identifier-e39f9d67` — 2 header(s)

- cause: the emitted code names `resolve_gl_modifier_snippet_source`, which nothing declares
- subject: `resolve_gl_modifier_snippet_source`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_gl/gl_shaded_prelude.hpp:168`
- example: `use of undeclared identifier 'resolve_gl_modifier_snippet_source'`

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

### 110. `invalid-operands-dc1c09c4` — 1 header(s)

- cause: an operator is applied to operand types it does not accept
- lanes: PLAT
- fix route: runtime
- first failure at: `flight/layout/flex_layout.hpp:171`
- example: `invalid operands to binary expression ('variant<double, String>' and 'String')`

### 111. `missing-member-1620083c` — 1 header(s)

- cause: the emitted code reads a member the target does not declare (`mix_blend_mode`)
- subject: `mix_blend_mode`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_dom/dom_materials.hpp:27`
- example: `no member named 'mix_blend_mode' in 'host_sdl::DomStyle'`

### 112. `missing-member-1a2e58cb` — 1 header(s)

- cause: the emitted code reads a member the target does not declare (`register_dom_texture_resolver`)
- subject: `register_dom_texture_resolver`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_dom/dom_image_texture_resolver.hpp:67`
- example: `no member named 'register_dom_texture_resolver' in namespace 'scene2d_dom'`

### 113. `missing-member-1ce5ed2e` — 1 header(s)

- cause: the emitted code reads a member the target does not declare (`physics_3d_cone_twist_joint_solver`)
- subject: `physics_3d_cone_twist_joint_solver`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/physics3d/register_built_in_physics_3d_joint_solvers.hpp:22`
- example: `no member named 'physics_3d_cone_twist_joint_solver' in namespace 'physics3d'`

### 114. `missing-member-42783657` — 1 header(s)

- cause: the emitted code reads a member the target does not declare (`font`)
- subject: `font`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_dom/dom_native_text.hpp:69`
- example: `no member named 'font' in 'host_sdl::DomStyle'`

### 115. `missing-member-5ac43ed0` — 1 header(s)

- cause: the emitted code reads a member the target does not declare (`element`)
- subject: `element`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d/instanced_mesh.hpp:233`
- example: `no member named 'element' in 'optional<TypedArray<unsigned int>>'; did you mean to use '->' instead of '.'?`

### 116. `missing-member-703ef98e` — 1 header(s)

- cause: the emitted code reads a member the target does not declare (`lottie_additive_mask_handler`)
- subject: `lottie_additive_mask_handler`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_formats/lottie_mask_handlers.hpp:18`
- example: `no member named 'lottie_additive_mask_handler' in namespace 'scene2d_formats'`

### 117. `missing-member-8aa8170f` — 1 header(s)

- cause: the emitted code reads a member the target does not declare (`swf_video_handler`)
- subject: `swf_video_handler`
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/swf/swf_video_tag_family.hpp:15`
- example: `no member named 'swf_video_handler' in namespace 'swf'`

### 118. `missing-member-ad3558ee` — 1 header(s)

- cause: the emitted code reads a member the target does not declare (`physics_2d_distance_joint_solver`)
- subject: `physics_2d_distance_joint_solver`
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/physics2d/register_built_in_physics_2d_joint_solvers.hpp:21`
- example: `no member named 'physics_2d_distance_joint_solver' in namespace 'physics2d'`

### 119. `missing-member-b5408b35` — 1 header(s)

- cause: the emitted code reads a member the target does not declare (`get_node_world_matrix4`)
- subject: `get_node_world_matrix4`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_gl/gl_shadow_map.hpp:228`
- example: `no member named 'get_node_world_matrix4' in namespace 'node'`

### 120. `missing-member-d7bf452c` — 1 header(s)

- cause: the emitted code reads a member the target does not declare (`begin`)
- subject: `begin`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/object.hpp:45`
- example: `no member named 'begin' in 'StructuralRef<RowReadonly<RowOf<Record<String, FlightDocumentValue>>>>'`

### 121. `missing-member-dae8c839` — 4 header(s)

- cause: the emitted code reads a member the target does not declare (`width`)
- subject: `width`
- lanes: TEXT
- fix route: runtime
- first failure at: `flight/bitmapfont_formats/bitmap_font_fnt.hpp:187`
- example: `no member named 'width' in 'variant<shared_ptr<VoxelGrid>, shared_ptr<TextureSource>, shared_ptr<ExternalTexture>, shared_ptr<Bitmap>, shared_ptr<CompressedImageResource>, shared_ptr<RenderTarget>, shared_ptr<ImageResource>>'`

### 122. `missing-member-dcf85c36` — 1 header(s)

- cause: the emitted code reads a member the target does not declare (`collect_md5_anim_features`)
- subject: `collect_md5_anim_features`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_formats/md5_anim_requirements.hpp:20`
- example: `no member named 'collect_md5_anim_features' in namespace 'scene3d_formats'`

### 123. `missing-member-ee398c94` — 1 header(s)

- cause: the emitted code reads a member the target does not declare (`register_canvas_texture_resolver`)
- subject: `register_canvas_texture_resolver`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_canvas/canvas_image_texture_resolver.hpp:50`
- example: `no member named 'register_canvas_texture_resolver' in namespace 'scene2d_canvas'`

### 124. `missing-member-fd2be1bc` — 1 header(s)

- cause: the emitted code reads a member the target does not declare (`object_if`)
- subject: `object_if`
- lanes: TEXT
- fix route: runtime
- first failure at: `flight/bitmapfont_formats/bitmap_font_format_kind.hpp:41`
- example: `no member named 'object_if' in 'JsonValue'`

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

### 140. `no-viable-conversion-cef6403d` — 1 header(s)

- cause: a value is passed where its declared type cannot receive it
- lanes: 3D
- fix route: compiler
- first failure at: `flight/picking/scene_hit_attributes.hpp:131`
- example: `no viable conversion from returned value of type 'optional<variant<shared_ptr<SpecularGlossinessPbrMaterial>, shared_ptr<VertexColorMaterial>, shared_ptr<UnlitMaterial>, shared_ptr<StandardPbrMaterial>, shared_ptr<CustomShaderMaterial>, shared_ptr<BlinnPhongMaterial>, shared_ptr<LambertMaterial>, sh`

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

### 144. `other-1e3a0d33` — 1 header(s)

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/skeleton2d_formats/spine_binary_bones_handler.hpp:48`
- example: `incompatible operand types ('value_type' and 'Ref<reflection_rotation_scale_translation_9dff6858eecb0eb4>' )`

### 145. `other-4703cbbe` — 1 header(s)

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_formats/gltf_features.hpp:59`
- example: `no matching conversion for static_cast from 'JsonValue' to 'Ref<GltfDocument>'`

### 146. `other-5dfe27ec` — 1 header(s)

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/skeleton2d_formats/dragon_bones_skins_handler.hpp:373`
- example: `invalid range expression of type 'Any'; no viable 'begin' function available`

### 147. `other-acb4b229` — 1 header(s)

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: 2D
- fix route: runtime
- first failure at: `flight/spritesheet_formats/texture_packer_parse.hpp:128`
- example: `no matching conversion for static_cast from 'JsonValue' to 'TexturePackerDocument'`

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

### 150. `undeclared-identifier-0168969a` — 1 header(s)

- cause: the emitted code names `dom_scene_2d_clip_hooks`, which nothing declares
- subject: `dom_scene_2d_clip_hooks`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_dom/dom_clip.hpp:81`
- example: `use of undeclared identifier 'dom_scene_2d_clip_hooks'`

### 151. `undeclared-identifier-07640d32` — 1 header(s)

- cause: the emitted code names `gl_outline_effect_runner`, which nothing declares
- subject: `gl_outline_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_outline_effect.hpp:96`
- example: `use of undeclared identifier 'gl_outline_effect_runner'`

### 152. `undeclared-identifier-076c02b1` — 1 header(s)

- cause: the emitted code names `resolve_tilemap_cell_index`, which nothing declares
- subject: `resolve_tilemap_cell_index`
- lanes: PLAT
- fix route: runtime
- first failure at: `flight/interaction/register_tilemap_hit_test.hpp:59`
- example: `use of undeclared identifier 'resolve_tilemap_cell_index'`

### 153. `undeclared-identifier-09cc9e86` — 1 header(s)

- cause: the emitted code names `gl_film_grain_effect_runner`, which nothing declares
- subject: `gl_film_grain_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_film_grain_effect.hpp:89`
- example: `use of undeclared identifier 'gl_film_grain_effect_runner'`

### 154. `undeclared-identifier-0b8d3b44` — 1 header(s)

- cause: the emitted code names `box_blur_shaders`, which nothing declares
- subject: `box_blur_shaders`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_effect_box_blur.hpp:115`
- example: `use of undeclared identifier 'box_blur_shaders'`

### 155. `undeclared-identifier-0c07642f` — 1 header(s)

- cause: the emitted code names `gl_glitch_effect_runner`, which nothing declares
- subject: `gl_glitch_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_glitch_effect.hpp:92`
- example: `use of undeclared identifier 'gl_glitch_effect_runner'`

### 156. `undeclared-identifier-11d6fcbe` — 1 header(s)

- cause: the emitted code names `gl_chromatic_aberration_effect_runner`, which nothing declares
- subject: `gl_chromatic_aberration_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_chromatic_aberration_effect.hpp:88`
- example: `use of undeclared identifier 'gl_chromatic_aberration_effect_runner'`

### 157. `undeclared-identifier-1462818e` — 1 header(s)

- cause: the emitted code names `ensure_gl_velocity_program`, which nothing declares
- subject: `ensure_gl_velocity_program`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_gl/gl_velocity.hpp:155`
- example: `use of undeclared identifier 'ensure_gl_velocity_program'`

### 158. `undeclared-identifier-17cb8386` — 1 header(s)

- cause: the emitted code names `gl_bokeh_depth_of_field_effect_runner`, which nothing declares
- subject: `gl_bokeh_depth_of_field_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_bokeh_depth_of_field_effect.hpp:100`
- example: `use of undeclared identifier 'gl_bokeh_depth_of_field_effect_runner'`

### 159. `undeclared-identifier-1892fccb` — 1 header(s)

- cause: the emitted code names `resolve_text_char_index`, which nothing declares
- subject: `resolve_text_char_index`
- lanes: PLAT
- fix route: runtime
- first failure at: `flight/interaction/register_text_hit_test.hpp:61`
- example: `use of undeclared identifier 'resolve_text_char_index'`

### 160. `undeclared-identifier-1a056bbc` — 1 header(s)

- cause: the emitted code names `get_number_format`, which nothing declares
- subject: `get_number_format`
- lanes: TEXT
- fix route: runtime
- first failure at: `flight/intl/number.hpp:42`
- example: `use of undeclared identifier 'get_number_format'`

### 161. `undeclared-identifier-1d8e072a` — 1 header(s)

- cause: the emitted code names `draw_gl_scale9_sprite`, which nothing declares
- subject: `draw_gl_scale9_sprite`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_gl/gl_scale9_sprite.hpp:168`
- example: `use of undeclared identifier 'draw_gl_scale9_sprite'`

### 162. `undeclared-identifier-2110ec37` — 1 header(s)

- cause: the emitted code names `draw_wgpu_sprite`, which nothing declares
- subject: `draw_wgpu_sprite`
- lanes: WGPU
- fix route: runtime
- first failure at: `flight/scene2d_wgpu/wgpu_sprite.hpp:134`
- example: `use of undeclared identifier 'draw_wgpu_sprite'`

### 163. `undeclared-identifier-2363216f` — 1 header(s)

- cause: the emitted code names `gl_dither_effect_runner`, which nothing declares
- subject: `gl_dither_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_dither_effect.hpp:86`
- example: `use of undeclared identifier 'gl_dither_effect_runner'`

### 164. `undeclared-identifier-2437e103` — 1 header(s)

- cause: the emitted code names `parse_triangle_geometry_block`, which nothing declares
- subject: `parse_triangle_geometry_block`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_formats/awd2_geometry_handler.hpp:325`
- example: `use of undeclared identifier 'parse_triangle_geometry_block'`

### 165. `undeclared-identifier-251d1ba2` — 1 header(s)

- cause: the emitted code names `collect_pending_texture_owners`, which nothing declares
- subject: `collect_pending_texture_owners`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_resources/reveal_scene_3d_resources_on_resolve.hpp:167`
- example: `use of undeclared identifier 'collect_pending_texture_owners'`

### 166. `undeclared-identifier-2d9cd75f` — 1 header(s)

- cause: the emitted code names `hit_test_shape_fill`, which nothing declares
- subject: `hit_test_shape_fill`
- lanes: PLAT
- fix route: runtime
- first failure at: `flight/interaction/register_shape_hit_test.hpp:66`
- example: `use of undeclared identifier 'hit_test_shape_fill'`

### 167. `undeclared-identifier-3017e8aa` — 1 header(s)

- cause: the emitted code names `acquire_gl_render_texture`, which nothing declares
- subject: `acquire_gl_render_texture`
- lanes: GL
- fix route: runtime
- first failure at: `flight/render_gl/gl_render_texture_pool.hpp:136`
- example: `use of undeclared identifier 'acquire_gl_render_texture'`

### 168. `undeclared-identifier-332a5586` — 1 header(s)

- cause: the emitted code names `draw_dom_scale9_sprite`, which nothing declares
- subject: `draw_dom_scale9_sprite`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_dom/dom_scale9_sprite.hpp:149`
- example: `use of undeclared identifier 'draw_dom_scale9_sprite'`

### 169. `undeclared-identifier-34adf2ab` — 1 header(s)

- cause: the emitted code names `parse_spine_slots`, which nothing declares
- subject: `parse_spine_slots`
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/skeleton2d_formats/spine_json_slots_handler.hpp:81`
- example: `use of undeclared identifier 'parse_spine_slots'`

### 170. `undeclared-identifier-3582eeb5` — 1 header(s)

- cause: the emitted code names `gl_screen_space_fog_effect_runner`, which nothing declares
- subject: `gl_screen_space_fog_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_screen_space_fog_effect.hpp:98`
- example: `use of undeclared identifier 'gl_screen_space_fog_effect_runner'`

### 171. `undeclared-identifier-3cb2bb54` — 1 header(s)

- cause: the emitted code names `invert_tint_shaders`, which nothing declares
- subject: `invert_tint_shaders`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_effect_tint_shader.hpp:75`
- example: `use of undeclared identifier 'invert_tint_shaders'`

### 172. `undeclared-identifier-3e7f03c4` — 1 header(s)

- cause: the emitted code names `gl_sharpen_effect_runner`, which nothing declares
- subject: `gl_sharpen_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_sharpen_effect.hpp:86`
- example: `use of undeclared identifier 'gl_sharpen_effect_runner'`

### 173. `undeclared-identifier-44058981` — 1 header(s)

- cause: the emitted code names `gl_blend_effect_runner`, which nothing declares
- subject: `gl_blend_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_blend_effect.hpp:119`
- example: `use of undeclared identifier 'gl_blend_effect_runner'`

### 174. `undeclared-identifier-49b86813` — 1 header(s)

- cause: the emitted code names `solve_skeleton_2d_ik_constraint`, which nothing declares
- subject: `solve_skeleton_2d_ik_constraint`
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/skeleton2d/ik_constraint_2d.hpp:205`
- example: `use of undeclared identifier 'solve_skeleton_2d_ik_constraint'`

### 175. `undeclared-identifier-4fc6ab69` — 1 header(s)

- cause: the emitted code names `create_svg_image_node`, which nothing declares
- subject: `create_svg_image_node`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_formats/svg_image_element.hpp:128`
- example: `use of undeclared identifier 'create_svg_image_node'`

### 176. `undeclared-identifier-514a5b7b` — 1 header(s)

- cause: the emitted code names `submit_gl_tilemap`, which nothing declares
- subject: `submit_gl_tilemap`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_gl/gl_tilemap.hpp:165`
- example: `use of undeclared identifier 'submit_gl_tilemap'`

### 177. `undeclared-identifier-51763a3c` — 1 header(s)

- cause: the emitted code names `gl_tone_map_effect_runner`, which nothing declares
- subject: `gl_tone_map_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_tone_map_effect.hpp:96`
- example: `use of undeclared identifier 'gl_tone_map_effect_runner'`

### 178. `undeclared-identifier-529e49d7` — 1 header(s)

- cause: the emitted code names `gl_directional_blur_effect_runner`, which nothing declares
- subject: `gl_directional_blur_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_directional_blur_effect.hpp:91`
- example: `use of undeclared identifier 'gl_directional_blur_effect_runner'`

### 179. `undeclared-identifier-58fb713b` — 1 header(s)

- cause: the emitted code names `draw_dom_scale9_shape`, which nothing declares
- subject: `draw_dom_scale9_shape`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_dom/dom_scale9_shape.hpp:151`
- example: `use of undeclared identifier 'draw_dom_scale9_shape'`

### 180. `undeclared-identifier-5be7be47` — 1 header(s)

- cause: the emitted code names `gl_scanlines_effect_runner`, which nothing declares
- subject: `gl_scanlines_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_scanlines_effect.hpp:87`
- example: `use of undeclared identifier 'gl_scanlines_effect_runner'`

### 181. `undeclared-identifier-6647740e` — 1 header(s)

- cause: the emitted code names `create_rive_relative_transforms`, which nothing declares
- subject: `create_rive_relative_transforms`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_formats/rive_clipping.hpp:207`
- example: `use of undeclared identifier 'create_rive_relative_transforms'`

### 182. `undeclared-identifier-66ea97f8` — 1 header(s)

- cause: the emitted code names `initialize_wgpu_screen_render_target`, which nothing declares
- subject: `initialize_wgpu_screen_render_target`
- lanes: WGPU
- fix route: runtime
- first failure at: `flight/render_wgpu/wgpu_screen_render_target.hpp:151`
- example: `use of undeclared identifier 'initialize_wgpu_screen_render_target'`

### 183. `undeclared-identifier-67f2c521` — 1 header(s)

- cause: the emitted code names `gl_god_rays_effect_runner`, which nothing declares
- subject: `gl_god_rays_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_god_rays_effect.hpp:106`
- example: `use of undeclared identifier 'gl_god_rays_effect_runner'`

### 184. `undeclared-identifier-6ad30a3d` — 1 header(s)

- cause: the emitted code names `gl_crt_effect_runner`, which nothing declares
- subject: `gl_crt_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_crt_effect.hpp:92`
- example: `use of undeclared identifier 'gl_crt_effect_runner'`

### 185. `undeclared-identifier-6dd20ce4` — 1 header(s)

- cause: the emitted code names `submit_gl_quad_batch`, which nothing declares
- subject: `submit_gl_quad_batch`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_gl/gl_quad_batch.hpp:191`
- example: `use of undeclared identifier 'submit_gl_quad_batch'`

### 186. `undeclared-identifier-757c2c8a` — 1 header(s)

- cause: the emitted code names `get_gl_scale9_shape_data`, which nothing declares
- subject: `get_gl_scale9_shape_data`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_gl/gl_scale9_shape.hpp:114`
- example: `use of undeclared identifier 'get_gl_scale9_shape_data'`

### 187. `undeclared-identifier-785ed669` — 1 header(s)

- cause: the emitted code names `resolve_quad_batch_instance_index`, which nothing declares
- subject: `resolve_quad_batch_instance_index`
- lanes: PLAT
- fix route: runtime
- first failure at: `flight/interaction/register_quad_batch_hit_test.hpp:94`
- example: `use of undeclared identifier 'resolve_quad_batch_instance_index'`

### 188. `undeclared-identifier-7905640c` — 1 header(s)

- cause: the emitted code names `canvas_clip_hooks`, which nothing declares
- subject: `canvas_clip_hooks`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_canvas/canvas_clip.hpp:89`
- example: `use of undeclared identifier 'canvas_clip_hooks'`

### 189. `undeclared-identifier-7d85a310` — 1 header(s)

- cause: the emitted code names `gl_lens_flare_effect_runner`, which nothing declares
- subject: `gl_lens_flare_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_lens_flare_effect.hpp:91`
- example: `use of undeclared identifier 'gl_lens_flare_effect_runner'`

### 190. `undeclared-identifier-7e3a780f` — 1 header(s)

- cause: the emitted code names `hit_test_sprite_alpha`, which nothing declares
- subject: `hit_test_sprite_alpha`
- lanes: PLAT
- fix route: runtime
- first failure at: `flight/interaction/register_sprite_hit_test.hpp:127`
- example: `use of undeclared identifier 'hit_test_sprite_alpha'`

### 191. `undeclared-identifier-7fd1550e` — 1 header(s)

- cause: the emitted code names `gl_camera_motion_blur_effect_runner`, which nothing declares
- subject: `gl_camera_motion_blur_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_camera_motion_blur_effect.hpp:88`
- example: `use of undeclared identifier 'gl_camera_motion_blur_effect_runner'`

### 192. `undeclared-identifier-7ff1c319` — 1 header(s)

- cause: the emitted code names `gl_vignette_effect_runner`, which nothing declares
- subject: `gl_vignette_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_vignette_effect.hpp:95`
- example: `use of undeclared identifier 'gl_vignette_effect_runner'`

### 193. `undeclared-identifier-8653adf0` — 1 header(s)

- cause: the emitted code names `ensure_scene_2d_runtime`, which nothing declares
- subject: `ensure_scene_2d_runtime`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d/scene2d.hpp:247`
- example: `use of undeclared identifier 'ensure_scene_2d_runtime'`

### 194. `undeclared-identifier-898d7996` — 1 header(s)

- cause: the emitted code names `gl_displacement_effect_runner`, which nothing declares
- subject: `gl_displacement_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_displacement_effect.hpp:91`
- example: `use of undeclared identifier 'gl_displacement_effect_runner'`

### 195. `undeclared-identifier-8ae633dc` — 1 header(s)

- cause: the emitted code names `gl_smaa_effect_runner`, which nothing declares
- subject: `gl_smaa_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_smaa_effect.hpp:86`
- example: `use of undeclared identifier 'gl_smaa_effect_runner'`

### 196. `undeclared-identifier-8b3950d6` — 1 header(s)

- cause: the emitted code names `gl_motion_blur_effect_runner`, which nothing declares
- subject: `gl_motion_blur_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_motion_blur_effect.hpp:92`
- example: `use of undeclared identifier 'gl_motion_blur_effect_runner'`

### 197. `undeclared-identifier-8e604c65` — 1 header(s)

- cause: the emitted code names `gl_sketch_effect_runner`, which nothing declares
- subject: `gl_sketch_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_sketch_effect.hpp:86`
- example: `use of undeclared identifier 'gl_sketch_effect_runner'`

### 198. `undeclared-identifier-9562ef07` — 1 header(s)

- cause: the emitted code names `draw_canvas_tilemap`, which nothing declares
- subject: `draw_canvas_tilemap`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_canvas/canvas_tilemap.hpp:120`
- example: `use of undeclared identifier 'draw_canvas_tilemap'`

### 199. `undeclared-identifier-9aaa5b7f` — 1 header(s)

- cause: the emitted code names `acquire_wgpu_screen_render_target_capture_texture`, which nothing declares
- subject: `acquire_wgpu_screen_render_target_capture_texture`
- lanes: WGPU
- fix route: runtime
- first failure at: `flight/render_wgpu/wgpu_screen_capture.hpp:122`
- example: `use of undeclared identifier 'acquire_wgpu_screen_render_target_capture_texture'`

### 200. `undeclared-identifier-a3f09fb8` — 1 header(s)

- cause: the emitted code names `draw_dom_shape`, which nothing declares
- subject: `draw_dom_shape`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_dom/dom_shape.hpp:132`
- example: `use of undeclared identifier 'draw_dom_shape'`

### 201. `undeclared-identifier-a4a22a69` — 1 header(s)

- cause: the emitted code names `gl_lens_distortion_effect_runner`, which nothing declares
- subject: `gl_lens_distortion_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_lens_distortion_effect.hpp:87`
- example: `use of undeclared identifier 'gl_lens_distortion_effect_runner'`

### 202. `undeclared-identifier-a89b9a39` — 1 header(s)

- cause: the emitted code names `gl_bitmap_displacement_effect_runner`, which nothing declares
- subject: `gl_bitmap_displacement_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_bitmap_displacement_effect.hpp:153`
- example: `use of undeclared identifier 'gl_bitmap_displacement_effect_runner'`

### 203. `undeclared-identifier-aa084556` — 1 header(s)

- cause: the emitted code names `is_track_list`, which nothing declares
- subject: `is_track_list`
- lanes: PLAT
- fix route: runtime
- first failure at: `flight/layout/grid_layout.hpp:151`
- example: `use of undeclared identifier 'is_track_list'`

### 204. `undeclared-identifier-ae594b8a` — 1 header(s)

- cause: the emitted code names `append_lottie_shape_items`, which nothing declares
- subject: `append_lottie_shape_items`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_formats/lottie_shape_layer.hpp:153`
- example: `use of undeclared identifier 'append_lottie_shape_items'`

### 205. `undeclared-identifier-af7a1054` — 1 header(s)

- cause: the emitted code names `gl_median_effect_runner`, which nothing declares
- subject: `gl_median_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_median_effect.hpp:90`
- example: `use of undeclared identifier 'gl_median_effect_runner'`

### 206. `undeclared-identifier-b15d9d15` — 1 header(s)

- cause: the emitted code names `parse_dragon_bones_bones`, which nothing declares
- subject: `parse_dragon_bones_bones`
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/skeleton2d_formats/dragon_bones_bones_handler.hpp:141`
- example: `use of undeclared identifier 'parse_dragon_bones_bones'`

### 207. `undeclared-identifier-befaa2d6` — 1 header(s)

- cause: the emitted code names `compute_scissor_rect`, which nothing declares
- subject: `compute_scissor_rect`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_gl/gl_clip_rectangle.hpp:108`
- example: `use of undeclared identifier 'compute_scissor_rect'`

### 208. `undeclared-identifier-c162d0c0` — 1 header(s)

- cause: the emitted code names `solve_skeleton_2d_transform_constraint`, which nothing declares
- subject: `solve_skeleton_2d_transform_constraint`
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/skeleton2d/transform_constraint_2d.hpp:175`
- example: `use of undeclared identifier 'solve_skeleton_2d_transform_constraint'`

### 209. `undeclared-identifier-c29489a5` — 1 header(s)

- cause: the emitted code names `gl_bloom_effect_runner`, which nothing declares
- subject: `gl_bloom_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_bloom_effect.hpp:106`
- example: `use of undeclared identifier 'gl_bloom_effect_runner'`

### 210. `undeclared-identifier-c3c55c65` — 1 header(s)

- cause: the emitted code names `gl_composite_effect_runner`, which nothing declares
- subject: `gl_composite_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_composite_effect.hpp:92`
- example: `use of undeclared identifier 'gl_composite_effect_runner'`

### 211. `undeclared-identifier-c45cc1bb` — 1 header(s)

- cause: the emitted code names `gl_fxaa_effect_runner`, which nothing declares
- subject: `gl_fxaa_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_fxaa_effect.hpp:86`
- example: `use of undeclared identifier 'gl_fxaa_effect_runner'`

### 212. `undeclared-identifier-c7551b38` — 1 header(s)

- cause: the emitted code names `gl_pixelate_effect_runner`, which nothing declares
- subject: `gl_pixelate_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_pixelate_effect.hpp:86`
- example: `use of undeclared identifier 'gl_pixelate_effect_runner'`

### 213. `undeclared-identifier-cc7c5006` — 1 header(s)

- cause: the emitted code names `solve_skeleton_2d_path_constraint`, which nothing declares
- subject: `solve_skeleton_2d_path_constraint`
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/skeleton2d/path_constraint_2d.hpp:290`
- example: `use of undeclared identifier 'solve_skeleton_2d_path_constraint'`

### 214. `undeclared-identifier-cf2fed2b` — 1 header(s)

- cause: the emitted code names `gl_posterize_effect_runner`, which nothing declares
- subject: `gl_posterize_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_posterize_effect.hpp:85`
- example: `use of undeclared identifier 'gl_posterize_effect_runner'`

### 215. `undeclared-identifier-d14c2215` — 1 header(s)

- cause: the emitted code names `gl_convolution_effect_runner`, which nothing declares
- subject: `gl_convolution_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_convolution_effect.hpp:131`
- example: `use of undeclared identifier 'gl_convolution_effect_runner'`

### 216. `undeclared-identifier-d687475c` — 1 header(s)

- cause: the emitted code names `gl_halftone_effect_runner`, which nothing declares
- subject: `gl_halftone_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_halftone_effect.hpp:88`
- example: `use of undeclared identifier 'gl_halftone_effect_runner'`

### 217. `undeclared-identifier-d7a1d1b7` — 1 header(s)

- cause: the emitted code names `gl_tilt_shift_effect_runner`, which nothing declares
- subject: `gl_tilt_shift_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_tilt_shift_effect.hpp:90`
- example: `use of undeclared identifier 'gl_tilt_shift_effect_runner'`

### 218. `undeclared-identifier-e3d6c4e4` — 1 header(s)

- cause: the emitted code names `apply_gl_gaussian_blur_pass`, which nothing declares
- subject: `apply_gl_gaussian_blur_pass`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_blur_effect.hpp:93`
- example: `use of undeclared identifier 'apply_gl_gaussian_blur_pass'`

### 219. `undeclared-identifier-e4a7b51e` — 1 header(s)

- cause: the emitted code names `gl_lens_dirt_effect_runner`, which nothing declares
- subject: `gl_lens_dirt_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_lens_dirt_effect.hpp:112`
- example: `use of undeclared identifier 'gl_lens_dirt_effect_runner'`

### 220. `undeclared-identifier-e4eb111d` — 1 header(s)

- cause: the emitted code names `gl_kuwahara_effect_runner`, which nothing declares
- subject: `gl_kuwahara_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_kuwahara_effect.hpp:86`
- example: `use of undeclared identifier 'gl_kuwahara_effect_runner'`

### 221. `undeclared-identifier-e7bf11f9` — 1 header(s)

- cause: the emitted code names `draw_wgpu_scale9_sprite`, which nothing declares
- subject: `draw_wgpu_scale9_sprite`
- lanes: WGPU
- fix route: runtime
- first failure at: `flight/scene2d_wgpu/wgpu_scale9_sprite.hpp:164`
- example: `use of undeclared identifier 'draw_wgpu_scale9_sprite'`

### 222. `undeclared-identifier-ebf3999b` — 1 header(s)

- cause: the emitted code names `gl_radial_blur_effect_runner`, which nothing declares
- subject: `gl_radial_blur_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_radial_blur_effect.hpp:96`
- example: `use of undeclared identifier 'gl_radial_blur_effect_runner'`

### 223. `undeclared-identifier-f020c1f7` — 1 header(s)

- cause: the emitted code names `gl_white_balance_effect_runner`, which nothing declares
- subject: `gl_white_balance_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_white_balance_effect.hpp:87`
- example: `use of undeclared identifier 'gl_white_balance_effect_runner'`

### 224. `undeclared-identifier-f6affb68` — 1 header(s)

- cause: the emitted code names `resolve_transmission_volume`, which nothing declares
- subject: `resolve_transmission_volume`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_formats/gltf_transmission_volume.hpp:166`
- example: `use of undeclared identifier 'resolve_transmission_volume'`

### 225. `undeclared-identifier-f91f1e72` — 1 header(s) *(builder4/E0a)*

- cause: the emitted code names `get_gl_text_label_data`, which nothing declares
- subject: `get_gl_text_label_data`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_gl/gl_text_label.hpp:100`
- example: `use of undeclared identifier 'get_gl_text_label_data'`
- owned by: builder4 (E0a: TextLabel / layout / Scene2D GL text)

### 226. `undeclared-identifier-fcbdae42` — 1 header(s)

- cause: the emitted code names `draw_canvas_text_label`, which nothing declares
- subject: `draw_canvas_text_label`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_canvas/canvas_text_label.hpp:140`
- example: `use of undeclared identifier 'draw_canvas_text_label'`

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
