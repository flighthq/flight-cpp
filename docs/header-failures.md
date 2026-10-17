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
are inside E0a's TextLabel / text-layout / Scene2D GL text work and are not offered as separate G2 work.

## Clusters, ranked by headers affected

| # | cluster | headers | lanes | fix route | cause |
| --- | --- | --- | --- | --- | --- |
| 1 | `call-has-no-matching-9c08d665` | 116 | WGPU | compiler | bound WebGPU descriptor object literals are emitted as untyped source-order braced initializers with anonymous nested owners *(request filed)* |
| 2 | `missing-member-b1a6d5cf` | 34 | 2D | compiler | scene2d-canvas emits `using ...get_canvas_render_state_runtime;` before the function it names is defined *(request filed)* |
| 3 | `in-operator-not-lowered-b1eae4af` | 31 | 2D, GL, PLAT, WGPU | compiler | the TypeScript `in` operator is emitted verbatim as C++ `in` at 4 root headers *(existing request extended)* |
| 4 | `undeclared-identifier-993e82f1` | 29 | 2D, 3D, PLAT, TEXT, WGPU | compiler | the template parameter name `Traits` leaks into emitted code as a free identifier *(builder4/E0a)* |
| 5 | `call-has-no-matching-3882b3e9` | 27 | 2D, 3D, TEXT | binding | same braced-descriptor shape against the device binding's constrained variadic create_texture *(builder4/E0a)* |
| 6 | `other-52e846a6` | 18 | ANIM | compiler | a value_type is combined with a three-alternative variant where the declared slot expects one alternative |
| 7 | `call-has-no-matching-47410fe2` | 16 | 2D, 3D | runtime (fixed) | Number is now a callable conversion object; G2 recheck: 2 pass, 14 fail at later compiler diagnostics |
| 8 | `other-40da43f7` | 16 | 2D, 3D, ANIM | compiler | an optional<Record<...>> is initialised from a shape that is not its declared storage |
| 9 | `static-cast-assertion-9674a66c` | 15 | 2D | compiler | a TypeScript `as` on `JSON.parse` is emitted as a `static_cast` from `JsonValue` to an unrelated `Ref<T>` *(request filed)* |
| 10 | `call-has-no-matching-076d207f` | 14 | WGPU | compiler | the emitted call to `create_pipeline_layout` matches no declared overload |
| 11 | `other-e9936ce6` | 13 | 3D | compiler | a pair literal the declared element type spells as `std::tuple<A, B>` is emitted as `flight::Array{...}`, so class-template ded... *(request filed)* |
| 12 | `call-has-no-matching-d6950545` | 12 | 2D, CORE | compiler | the emitted call to `get_node_child_count` matches no declared overload |
| 13 | `no-viable-conversion-9c3803c5` | 9 | 2D, WGPU | compiler | a value is passed where its declared type cannot receive it |
| 14 | `no-viable-conversion-dfea2d92` | 9 | 3D | compiler | a value is passed where its declared type cannot receive it |
| 15 | `variant-cast-ccfedad4` | 9 | 3D | compiler | a retained readonly program row is reconstructed through a writable row before a readonly draw helper *(existing request extended)* |
| 16 | `call-has-no-matching-b569e92e` | 8 | 3D, ANIM | compiler | the emitted call to `init_joint_base` matches no declared overload |
| 17 | `undeclared-identifier-294e632e` | 8 | ANIM | runtime | the emitted code names `apply_swf_authored_bounds`, which nothing declares |
| 18 | `undeclared-identifier-3e3cccce` | 8 | GL | runtime | the emitted code names `blit_offset_shaders`, which nothing declares |
| 19 | `invalid-operands-4b19651b` | 7 | 2D, ANIM | runtime | an operator is applied to operand types it does not accept |
| 20 | `missing-member-fd48c0bb` | 7 | 3D | runtime | the emitted code reads a member the target does not declare (`collide_physics_3d_collider_shapes`) |
| 21 | `other-18dcce71` | 7 | ANIM | runtime | uncategorised: the first diagnostic does not name a single cause |
| 22 | `undeclared-identifier-7dc86326` | 7 | WGPU | runtime | the emitted code names `get_wgpu_scene_3d_runtime`, which nothing declares |
| 23 | `undeclared-identifier-b90ed4db` | 7 | ANIM | runtime | the emitted code names `dragon_bones_animations_section_handler`, which nothing declares |
| 24 | `invalid-operands-ec4dbe74` | 6 | CORE, GL | runtime | an operator is applied to operand types it does not accept |
| 25 | `missing-member-237022b0` | 6 | 2D, TEXT | runtime | the emitted code reads a member the target does not declare (`create_document_refusal`) |
| 26 | `missing-member-e27d26f9` | 6 | 3D | runtime | the emitted code reads a member the target does not declare (`attach_gltf_pbr_extension`) |
| 27 | `no-viable-conversion-f02000a9` | 6 | 2D, GL | compiler | a value is passed where its declared type cannot receive it |
| 28 | `undeclared-identifier-6e587559` | 6 | ANIM, TEXT | runtime | the emitted code names `equals_markup_format`, which nothing declares |
| 29 | `undeclared-identifier-a74215ca` | 6 | 2D | runtime | the emitted code names `K`, which nothing declares |
| 30 | `undeclared-identifier-f5e3440f` | 6 | 3D, CORE | runtime | the emitted code names `initialize_resource_loader`, which nothing declares |
| 31 | `call-has-no-matching-5a045ef4` | 5 | ANIM | compiler | the emitted call to `read_swf_shape_edge_record` matches no declared overload |
| 32 | `call-has-no-matching-b897f03f` | 5 | 3D | compiler | the emitted call to `get_node_signals` matches no declared overload |
| 33 | `call-has-no-matching-dd66c064` | 5 | ANIM | compiler | the emitted call to a function matches no declared overload |
| 34 | `compiler-crash-aba5e9ec` | 5 | 3D | none (sweep resource limit; passes serially) | the -j8 sweep ran out of room (retried serially) |
| 35 | `invalid-operands-6d9a155d` | 5 | 2D | runtime | an operator is applied to operand types it does not accept |
| 36 | `missing-member-5e9e8a2c` | 5 | PLAT | runtime | the emitted code reads a member the target does not declare (`hit_test_graph_local_bounds`) |
| 37 | `no-viable-conversion-eaf1a6e0` | 5 | 2D | compiler | a value is passed where its declared type cannot receive it |
| 38 | `no-viable-overload-658901de` | 5 | 2D, GL | runtime | an assignment or operator has no overload for the emitted operand types |
| 39 | `call-has-no-matching-4ce2cc01` | 4 | ANIM | compiler | the emitted call to a function matches no declared overload |
| 40 | `call-has-no-matching-82244b93` | 4 | 2D | compiler | the emitted call to `get_or_create_render_proxy_2d` matches no declared overload |
| 41 | `call-has-no-matching-9248dccf` | 4 | ANIM | compiler | the emitted call to `get_node_world_matrix4` matches no declared overload |
| 42 | `call-has-no-matching-9ebeb95c` | 4 | ANIM, GL | compiler | the emitted call to `get_transform_2d_runtime` matches no declared overload |
| 43 | `call-has-no-matching-c3aea71c` | 4 | 2D, GL | compiler | the emitted call to `structural_ref_cast` matches no declared overload |
| 44 | `no-viable-conversion-5903ce63` | 4 | 3D, GL | compiler | a value is passed where its declared type cannot receive it |
| 45 | `no-viable-conversion-e1a24bd1` | 4 | 2D | compiler | a value is passed where its declared type cannot receive it |
| 46 | `other-2aef05a3` | 4 | ANIM | runtime | uncategorised: the first diagnostic does not name a single cause |
| 47 | `other-43523429` | 4 | ANIM | runtime | uncategorised: the first diagnostic does not name a single cause |
| 48 | `other-4e22e4de` | 4 | 2D | runtime | uncategorised: the first diagnostic does not name a single cause |
| 49 | `other-5611a858` | 4 | ANIM | runtime | uncategorised: the first diagnostic does not name a single cause |
| 50 | `other-d6be4352` | 4 | ANIM | runtime | uncategorised: the first diagnostic does not name a single cause |
| 51 | `static-assertion-00a0e75d` | 4 | 3D, WGPU | compiler | a runtime static assertion rejects the emitted shape |
| 52 | `undeclared-identifier-0fc9e20f` | 4 | 2D | runtime | the emitted code names `initialize_rive_artboard_import_context`, which nothing declares |
| 53 | `undeclared-identifier-69b85faa` | 4 | PLAT | runtime | the emitted code names `untrack_midi_subscription`, which nothing declares |
| 54 | `undeclared-identifier-7d64d9c1` | 4 | GL | runtime | the emitted code names `gl_custom_shader_effect_runner`, which nothing declares |
| 55 | `undeclared-identifier-862d162a` | 4 | 3D | runtime | the emitted code names `build_gltf_animations`, which nothing declares |
| 56 | `undeclared-identifier-f2b46b01` | 4 | 3D | runtime | the emitted code names `list_standard_pbr_material_textures`, which nothing declares |
| 57 | `call-has-no-matching-3b2f72dd` | 3 | PLAT | compiler | the emitted call to `is_optional_number` matches no declared overload |
| 58 | `call-has-no-matching-56e47749` | 3 | GL | compiler | the emitted call to `compute_node_root_local_bounds_rectangle` matches no declared overload |
| 59 | `invalid-operands-98cd7094` | 3 | 2D | runtime | an operator is applied to operand types it does not accept |
| 60 | `missing-member-95ead583` | 3 | 3D | runtime | the emitted code reads a member the target does not declare (`version`) |
| 61 | `missing-member-b22d5c52` | 3 | 2D | runtime | the emitted code reads a member the target does not declare (`resolve_canvas_texture_window_source`) |
| 62 | `missing-member-c36ea2d4` | 3 | 2D | runtime | the emitted code reads a member the target does not declare (`opacity`) |
| 63 | `no-viable-conversion-4ca7626f` | 3 | ANIM | compiler | a value is passed where its declared type cannot receive it |
| 64 | `no-viable-conversion-b3064463` | 3 | GL | compiler | a value is passed where its declared type cannot receive it |
| 65 | `no-viable-conversion-b3f3c7c6` | 3 | 3D | compiler | a value is passed where its declared type cannot receive it |
| 66 | `no-viable-conversion-b5b698fe` | 3 | 2D | compiler | a value is passed where its declared type cannot receive it |
| 67 | `other-3d2efc1d` | 3 | 2D | runtime | uncategorised: the first diagnostic does not name a single cause |
| 68 | `other-41f4a462` | 3 | ANIM | runtime | uncategorised: the first diagnostic does not name a single cause |
| 69 | `other-6a1604d5` | 3 | PLAT | runtime | uncategorised: the first diagnostic does not name a single cause |
| 70 | `other-b860ec87` | 3 | 2D, GL | runtime | uncategorised: the first diagnostic does not name a single cause |
| 71 | `other-f9c5bc10` | 3 | 2D | runtime | uncategorised: the first diagnostic does not name a single cause |
| 72 | `redefinition-c11d96e9` | 3 | TEXT | runtime | a declaration is emitted twice |
| 73 | `undeclared-identifier-145c75d7` | 3 | TEXT | runtime | the emitted code names `initialize_glyph_source_from_glyph_atlas`, which nothing declares |
| 74 | `undeclared-identifier-2cdb69a6` | 3 | PLAT | runtime | the emitted code names `channel_elements`, which nothing declares |
| 75 | `undeclared-identifier-564c29cc` | 3 | TEXT | runtime | the emitted code names `format_date_value`, which nothing declares |
| 76 | `undeclared-identifier-6a187568` | 3 | 3D | runtime | the emitted code names `count_ascii_stl_facets`, which nothing declares |
| 77 | `undeclared-identifier-dc087108` | 3 | PLAT | runtime | the emitted code names `load_video_resource_from_url`, which nothing declares |
| 78 | `call-has-no-matching-84410867` | 2 | 3D | compiler | the emitted call to `buffer_data` matches no declared overload |
| 79 | `call-has-no-matching-d42144f0` | 2 | ANIM | compiler | the emitted call to `push` matches no declared overload |
| 80 | `missing-member-da12a523` | 2 | 3D | runtime | the emitted code reads a member the target does not declare (`parse_obj_material_library`) |
| 81 | `missing-member-dd9b4baa` | 2 | 3D, GL | runtime | the emitted code reads a member the target does not declare (`renderbuffer_binding`) |
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
| 99 | `call-has-no-matching-4c12e411` | 1 | WGPU | compiler | the emitted call to `create_bind_group_layout` matches no declared overload |
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
| 121 | `missing-member-dae8c839` | 1 | TEXT | runtime | the emitted code reads a member the target does not declare (`width`) |
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

### 1. `call-has-no-matching-9c08d665` — 116 header(s)

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

### 4. `undeclared-identifier-993e82f1` — 29 header(s) *(builder4/E0a)*

- cause: the template parameter name `Traits` leaks into emitted code as a free identifier
- subject: `Traits`
- lanes: 2D, 3D, PLAT, TEXT, WGPU
- fix route: compiler
- why this route: the template parameter name `Traits` leaks into emitted code as a free identifier
- first failure at: `flight/scene3d/scene_document.hpp:106`
- example: `use of undeclared identifier 'Traits'`
- owned by: builder4 (E0a: TextLabel / layout / Scene2D GL text)

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

### 10. `call-has-no-matching-076d207f` — 14 header(s)

- cause: the emitted call to `create_pipeline_layout` matches no declared overload
- subject: `create_pipeline_layout`
- lanes: WGPU
- fix route: compiler
- first failure at: `flight/render_wgpu/wgpu_shader.hpp:54`
- example: `no matching member function for call to 'create_pipeline_layout'`

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

### 17. `undeclared-identifier-294e632e` — 8 header(s)

- cause: the emitted code names `apply_swf_authored_bounds`, which nothing declares
- subject: `apply_swf_authored_bounds`
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/swf/swf_node.hpp:112`
- example: `use of undeclared identifier 'apply_swf_authored_bounds'`

### 18. `undeclared-identifier-3e3cccce` — 8 header(s)

- cause: the emitted code names `blit_offset_shaders`, which nothing declares
- subject: `blit_offset_shaders`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_effect_blit_shader.hpp:87`
- example: `use of undeclared identifier 'blit_offset_shaders'`

### 19. `invalid-operands-4b19651b` — 7 header(s)

- cause: an operator is applied to operand types it does not accept
- lanes: 2D, ANIM
- fix route: runtime
- first failure at: `flight/scene2d_formats/lottie_counts.hpp:26`
- example: `invalid operands to binary expression ('Any' and 'double')`

### 20. `missing-member-fd48c0bb` — 7 header(s)

- cause: the emitted code reads a member the target does not declare (`collide_physics_3d_collider_shapes`)
- subject: `collide_physics_3d_collider_shapes`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/physics3d/contact_intake.hpp:284`
- example: `no member named 'collide_physics_3d_collider_shapes' in namespace 'physics3d'`

### 21. `other-18dcce71` — 7 header(s)

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/skeleton2d_formats/spine_parse_helpers.hpp:90`
- example: `assigning to 'double' from incompatible type 'Any'`

### 22. `undeclared-identifier-7dc86326` — 7 header(s)

- cause: the emitted code names `get_wgpu_scene_3d_runtime`, which nothing declares
- subject: `get_wgpu_scene_3d_runtime`
- lanes: WGPU
- fix route: runtime
- first failure at: `flight/scene3d_wgpu/wgpu_scene_3d_runtime.hpp:194`
- example: `use of undeclared identifier 'get_wgpu_scene_3d_runtime'`

### 23. `undeclared-identifier-b90ed4db` — 7 header(s)

- cause: the emitted code names `dragon_bones_animations_section_handler`, which nothing declares
- subject: `dragon_bones_animations_section_handler`
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/skeleton2d_formats/dragon_bones_animations_handler.hpp:128`
- example: `use of undeclared identifier 'dragon_bones_animations_section_handler'`

### 24. `invalid-operands-ec4dbe74` — 6 header(s)

- cause: an operator is applied to operand types it does not accept
- lanes: CORE, GL
- fix route: runtime
- first failure at: `flight/render/enable_color_adjustment_guards.hpp:62`
- example: `invalid operands to binary expression ('variant<function<void (Ref<RenderState>, NodeAny)>, Null, Undefined>' >, Null, Undefined>') and 'void ()')`

### 25. `missing-member-237022b0` — 6 header(s)

- cause: the emitted code reads a member the target does not declare (`create_document_refusal`)
- subject: `create_document_refusal`
- lanes: 2D, TEXT
- fix route: runtime
- first failure at: `flight/scene_document/flight_document_text.hpp:226`
- example: `no member named 'create_document_refusal' in namespace 'scene_document'`

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

- cause: the emitted code names `equals_markup_format`, which nothing declares
- subject: `equals_markup_format`
- lanes: ANIM, TEXT
- fix route: runtime
- first failure at: `flight/text_markup/text_markup.hpp:177`
- example: `use of undeclared identifier 'equals_markup_format'`

### 29. `undeclared-identifier-a74215ca` — 6 header(s)

- cause: the emitted code names `K`, which nothing declares
- subject: `K`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/shape/register_default_shape_bounds_commands.hpp:17`
- example: `use of undeclared identifier 'K'`

### 30. `undeclared-identifier-f5e3440f` — 6 header(s)

- cause: the emitted code names `initialize_resource_loader`, which nothing declares
- subject: `initialize_resource_loader`
- lanes: 3D, CORE
- fix route: runtime
- first failure at: `flight/loader/resource_loader.hpp:465`
- example: `use of undeclared identifier 'initialize_resource_loader'`

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

### 33. `call-has-no-matching-dd66c064` — 5 header(s)

- cause: the emitted call to a function matches no declared overload
- lanes: ANIM
- fix route: compiler
- first failure at: `flight/swf/swf_timeline_parse.hpp:122`
- example: `no matching function for call to object of type 'const function<bool (shared_ptr<SwfTagReader>, double, shared_ptr<SwfTagParseState>, shared_ptr<SwfTagTimelineState>, optional<Array<shared_ptr<ImportDiagnostic>>>)>'`

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

### 38. `no-viable-overload-658901de` — 5 header(s)

- cause: an assignment or operator has no overload for the emitted operand types
- lanes: 2D, GL
- fix route: runtime
- first failure at: `flight/materials/material_presets.hpp:113`
- example: `no viable overloaded '='`

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

### 46. `other-2aef05a3` — 4 header(s)

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/swf/swf_filter.hpp:257`
- example: `incompatible operand types ('Ref<InnerShadowEffect>' and 'Ref<DropShadowEffect>')`

### 47. `other-43523429` — 4 header(s)

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/movieclip/spritesheet_timeline_source.hpp:122`
- example: `too many arguments to function call, expected 0, have 1`

### 48. `other-4e22e4de` — 4 header(s)

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: 2D
- fix route: runtime
- first failure at: `flight/spritesheet_formats/aseprite_parse.hpp:144`
- example: `no matching conversion for static_cast from 'JsonValue' to 'AsepriteDocument'`

### 49. `other-5611a858` — 4 header(s)

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: ANIM
- fix route: runtime
- first failure at: `flight/swf/swf_appearance.hpp:123`
- example: `no matching constructor for initialization of 'Array<Ref<Adjustment>>'`

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

### 52. `undeclared-identifier-0fc9e20f` — 4 header(s)

- cause: the emitted code names `initialize_rive_artboard_import_context`, which nothing declares
- subject: `initialize_rive_artboard_import_context`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_formats/rive_import_registry.hpp:130`
- example: `use of undeclared identifier 'initialize_rive_artboard_import_context'`

### 53. `undeclared-identifier-69b85faa` — 4 header(s)

- cause: the emitted code names `untrack_midi_subscription`, which nothing declares
- subject: `untrack_midi_subscription`
- lanes: PLAT
- fix route: runtime
- first failure at: `flight/midi/midi_subscription.hpp:164`
- example: `use of undeclared identifier 'untrack_midi_subscription'`

### 54. `undeclared-identifier-7d64d9c1` — 4 header(s)

- cause: the emitted code names `gl_custom_shader_effect_runner`, which nothing declares
- subject: `gl_custom_shader_effect_runner`
- lanes: GL
- fix route: runtime
- first failure at: `flight/effects_gl/gl_custom_shader_effect.hpp:183`
- example: `use of undeclared identifier 'gl_custom_shader_effect_runner'`

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

- cause: the emitted code reads a member the target does not declare (`version`)
- subject: `version`
- lanes: 3D
- fix route: runtime
- first failure at: `flight/scene3d_gl/gl_environment_cube.hpp:56`
- example: `no member named 'version' in 'variant<shared_ptr<VoxelGrid>, shared_ptr<TextureSource>, shared_ptr<ExternalTexture>, shared_ptr<Bitmap>, shared_ptr<CompressedImageResource>, shared_ptr<RenderTarget>, shared_ptr<ImageResource>>'`

### 61. `missing-member-b22d5c52` — 3 header(s)

- cause: the emitted code reads a member the target does not declare (`resolve_canvas_texture_window_source`)
- subject: `resolve_canvas_texture_window_source`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_canvas/canvas_fill_pattern.hpp:77`
- example: `no member named 'resolve_canvas_texture_window_source' in namespace 'scene2d_canvas'`

### 62. `missing-member-c36ea2d4` — 3 header(s)

- cause: the emitted code reads a member the target does not declare (`opacity`)
- subject: `opacity`
- lanes: 2D
- fix route: runtime
- first failure at: `flight/scene2d_dom/dom_style.hpp:33`
- example: `no member named 'opacity' in 'host_sdl::DomStyle'`

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

### 70. `other-b860ec87` — 3 header(s)

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: 2D, GL
- fix route: runtime
- first failure at: `flight/render/enable_color_adjustments.hpp:92`
- example: `no matching constructor for initialization of 'optional<Ref<ColorScaleBias>>'`

### 71. `other-f9c5bc10` — 3 header(s)

- cause: uncategorised: the first diagnostic does not name a single cause
- lanes: 2D
- fix route: runtime
- first failure at: `flight/tilemap_formats/tilemap_detect.hpp:49`
- example: `too many arguments to function call, expected single argument 'prefix', have 2 arguments`

### 72. `redefinition-c11d96e9` — 3 header(s)

- cause: a declaration is emitted twice
- lanes: TEXT
- fix route: runtime
- first failure at: `flight/bitmapfont_formats/bitmap_font_record.hpp:23`
- example: `redefinition of default argument`

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

### 99. `call-has-no-matching-4c12e411` — 1 header(s)

- cause: the emitted call to `create_bind_group_layout` matches no declared overload
- subject: `create_bind_group_layout`
- lanes: WGPU
- fix route: compiler
- first failure at: `flight/render_wgpu/wgpu_fullscreen_pass.hpp:148`
- example: `no matching member function for call to 'create_bind_group_layout'`

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

### 121. `missing-member-dae8c839` — 1 header(s)

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

