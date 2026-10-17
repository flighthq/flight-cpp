# Per-package labels: `ctest -L '^pkg:<package>$'` runs exactly the semantic tests that include that
# SDK package's generated headers, and `npm run pkg:compile -- <package> --ctest` builds and runs them.
# A label is the unscoped npm name (`pkg:scene2d-canvas`, not the C++ directory), and every package a
# test includes directly is listed; scripts/packageScope.test.mjs checks this table against the
# includes so a new test or include cannot go unlabeled.
function(flight_cpp_package_test_labels test)
  list(TRANSFORM ARGN PREPEND "pkg:" OUTPUT_VARIABLE labels)
  set_tests_properties("${test}" PROPERTIES LABELS "${labels}")
endfunction()

add_executable(flight_cpp_runtime_tests runtime_test.cpp)
target_link_libraries(flight_cpp_runtime_tests PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_runtime_tests)

add_test(NAME flight_cpp.runtime COMMAND flight_cpp_runtime_tests)

add_executable(flight_cpp_nullish_or_tests nullish_or_test.cpp)
target_link_libraries(flight_cpp_nullish_or_tests PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_nullish_or_tests)
add_test(NAME flight_cpp.nullish_or COMMAND flight_cpp_nullish_or_tests)

add_executable(flight_cpp_async_closure_tests async_closure_test.cpp)
target_link_libraries(flight_cpp_async_closure_tests PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_async_closure_tests)
add_test(NAME flight_cpp.async_closure COMMAND flight_cpp_async_closure_tests)

add_executable(
  flight_cpp_upstream_math_geometry_parity_tests
  generated/upstream_math_geometry_parity.cpp
)
target_include_directories(
  flight_cpp_upstream_math_geometry_parity_tests
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(flight_cpp_upstream_math_geometry_parity_tests PRIVATE Flight::SdkPreview)
flight_cpp_set_project_warnings(flight_cpp_upstream_math_geometry_parity_tests)
add_test(
  NAME flight_cpp.upstream_math_geometry_parity
  COMMAND flight_cpp_upstream_math_geometry_parity_tests
)

add_executable(flight_cpp_intl_type_tests intl_type_test.cpp)
target_link_libraries(flight_cpp_intl_type_tests PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_intl_type_tests)

add_test(NAME flight_cpp.intl_types COMMAND flight_cpp_intl_type_tests)

# Inherits the floating-point profile from Flight::Cpp alone, so it fails if that profile is dropped.
add_executable(flight_cpp_floating_point_contraction_tests floating_point_contraction_test.cpp)
target_link_libraries(flight_cpp_floating_point_contraction_tests PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_floating_point_contraction_tests)

add_test(NAME flight_cpp.floating_point_contraction COMMAND flight_cpp_floating_point_contraction_tests)

add_executable(flight_cpp_generated_runtime_tests generated_runtime_test.cpp)
target_link_libraries(flight_cpp_generated_runtime_tests PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_generated_runtime_tests)

add_test(NAME flight_cpp.generated_runtime COMMAND flight_cpp_generated_runtime_tests)

# Needs the generated structural member table, so it links the SDK preview rather than the runtime
# alone; named row members do not resolve without it.
add_executable(flight_cpp_structural_row_tests structural_row_test.cpp)
target_link_libraries(flight_cpp_structural_row_tests PRIVATE Flight::Cpp Flight::SdkPreview)
flight_cpp_set_project_warnings(flight_cpp_structural_row_tests)
add_test(NAME flight_cpp.structural_row COMMAND flight_cpp_structural_row_tests)

# Merged rows (`A & B`): construction, narrowed members and the subject rule, against the generated table.
add_executable(flight_cpp_merged_row_tests merged_row_test.cpp)
target_link_libraries(flight_cpp_merged_row_tests PRIVATE Flight::Cpp Flight::SdkPreview)
flight_cpp_set_project_warnings(flight_cpp_merged_row_tests)
add_test(NAME flight_cpp.merged_row COMMAND flight_cpp_merged_row_tests)

# `T['key']` for a type parameter: the indexed-member trait the compiler emits, against the generated table.
add_executable(flight_cpp_indexed_member_tests indexed_member_test.cpp)
target_link_libraries(flight_cpp_indexed_member_tests PRIVATE Flight::Cpp Flight::SdkPreview)
flight_cpp_set_project_warnings(flight_cpp_indexed_member_tests)
add_test(NAME flight_cpp.indexed_member COMMAND flight_cpp_indexed_member_tests)

add_executable(flight_cpp_task_resolve_domain_tests task_resolve_domain_test.cpp)
target_link_libraries(flight_cpp_task_resolve_domain_tests PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_task_resolve_domain_tests)
add_test(NAME flight_cpp.task_resolve_domain COMMAND flight_cpp_task_resolve_domain_tests)

add_executable(flight_cpp_caught_exception_tests caught_exception_test.cpp)
target_link_libraries(flight_cpp_caught_exception_tests PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_caught_exception_tests)
add_test(NAME flight_cpp.caught_exception COMMAND flight_cpp_caught_exception_tests)

add_executable(flight_cpp_union_kind_tests union_kind_test.cpp)
target_link_libraries(flight_cpp_union_kind_tests PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_union_kind_tests)
add_test(NAME flight_cpp.union_kind COMMAND flight_cpp_union_kind_tests)

add_executable(flight_cpp_variant_set_identity_tests variant_set_identity_test.cpp)
target_link_libraries(flight_cpp_variant_set_identity_tests PRIVATE Flight::Cpp Flight::SdkPreview)
flight_cpp_set_project_warnings(flight_cpp_variant_set_identity_tests)
add_test(NAME flight_cpp.variant_set_identity COMMAND flight_cpp_variant_set_identity_tests)

add_executable(flight_cpp_generated_clipboard_tests generated_clipboard_test.cpp)
target_include_directories(
  flight_cpp_generated_clipboard_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_clipboard_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_clipboard COMMAND flight_cpp_generated_clipboard_tests)

add_executable(flight_cpp_generated_collision_tests generated_collision_test.cpp)
target_include_directories(
  flight_cpp_generated_collision_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_collision_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_collision COMMAND flight_cpp_generated_collision_tests)

add_executable(flight_cpp_generated_collision_shapes_tests generated_collision_shapes_test.cpp)
target_include_directories(
  flight_cpp_generated_collision_shapes_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_collision_shapes_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_collision_shapes COMMAND flight_cpp_generated_collision_shapes_tests)
set_tests_properties(flight_cpp.generated_collision PROPERTIES SKIP_RETURN_CODE 77)

add_executable(flight_cpp_generated_collision_registries_tests generated_collision_registries_test.cpp)
target_include_directories(
  flight_cpp_generated_collision_registries_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_collision_registries_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_collision_registries COMMAND flight_cpp_generated_collision_registries_tests)

add_executable(flight_cpp_generated_registry_tests generated_registry_test.cpp)
target_include_directories(
  flight_cpp_generated_registry_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_registry_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_registry COMMAND flight_cpp_generated_registry_tests)

add_executable(flight_cpp_generated_requirement_tests generated_requirement_test.cpp)
target_include_directories(
  flight_cpp_generated_requirement_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_requirement_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_requirement COMMAND flight_cpp_generated_requirement_tests)

add_executable(flight_cpp_generated_requirement_codegen_tests generated_requirement_codegen_test.cpp)
target_include_directories(
  flight_cpp_generated_requirement_codegen_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_requirement_codegen_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_requirement_codegen COMMAND flight_cpp_generated_requirement_codegen_tests)

add_executable(flight_cpp_generated_animation_tests generated_animation_test.cpp)
target_include_directories(
  flight_cpp_generated_animation_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_animation_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_animation COMMAND flight_cpp_generated_animation_tests)

add_executable(flight_cpp_generated_log_tests generated_log_test.cpp)
target_include_directories(
  flight_cpp_generated_log_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_log_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_log COMMAND flight_cpp_generated_log_tests)

add_executable(flight_cpp_generated_log_levels_and_sinks_tests generated_log_levels_and_sinks_test.cpp)
target_include_directories(
  flight_cpp_generated_log_levels_and_sinks_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_log_levels_and_sinks_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_log_levels_and_sinks COMMAND flight_cpp_generated_log_levels_and_sinks_tests)

add_executable(
  flight_cpp_generated_render_state_tests
  generated_render_state_test.cpp
  generated_render_state_renderer_include_test.cpp
)
target_include_directories(
  flight_cpp_generated_render_state_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_render_state_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(NAME flight_cpp.generated_render_state COMMAND flight_cpp_generated_render_state_tests)

if(TARGET Flight::HostSdlGl)
  add_executable(flight_cpp_generated_gl_registries_tests generated_gl_registries_test.cpp)
  target_include_directories(flight_cpp_generated_gl_registries_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include")
  target_link_libraries(flight_cpp_generated_gl_registries_tests PRIVATE Flight::Cpp Flight::SdkPreview Flight::HostSdlGl)
  add_test(NAME flight_cpp.generated_gl_registries COMMAND flight_cpp_generated_gl_registries_tests)
  flight_cpp_package_test_labels(flight_cpp.generated_gl_registries render-gl scene3d-gl types)

  add_executable(flight_cpp_generated_effects_gl_blit_cache_tests generated_effects_gl_blit_cache_test.cpp)
  target_include_directories(flight_cpp_generated_effects_gl_blit_cache_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include")
  target_link_libraries(flight_cpp_generated_effects_gl_blit_cache_tests PRIVATE Flight::Cpp Flight::SdkPreview Flight::HostSdlGl)
  add_test(NAME flight_cpp.generated_effects_gl_blit_cache COMMAND flight_cpp_generated_effects_gl_blit_cache_tests)
  flight_cpp_package_test_labels(flight_cpp.generated_effects_gl_blit_cache effects-gl)
  if(UNIX AND NOT APPLE)
    set_tests_properties(flight_cpp.generated_effects_gl_blit_cache PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=offscreen;LIBGL_ALWAYS_SOFTWARE=1")
  endif()
  add_executable(flight_cpp_generated_scene2d_gl_clip_cache_tests generated_scene2d_gl_clip_cache_test.cpp)
  target_include_directories(flight_cpp_generated_scene2d_gl_clip_cache_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include")
  target_link_libraries(flight_cpp_generated_scene2d_gl_clip_cache_tests PRIVATE Flight::Cpp Flight::SdkPreview Flight::HostSdlGl)
  add_test(NAME flight_cpp.generated_scene2d_gl_clip_cache COMMAND flight_cpp_generated_scene2d_gl_clip_cache_tests)
  flight_cpp_package_test_labels(flight_cpp.generated_scene2d_gl_clip_cache scene2d-gl render-gl)
  if(UNIX AND NOT APPLE)
    set_tests_properties(flight_cpp.generated_scene2d_gl_clip_cache PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=offscreen;LIBGL_ALWAYS_SOFTWARE=1")
  endif()
  add_executable(flight_cpp_generated_scene2d_gl_scissor_rect_tests generated_scene2d_gl_scissor_rect_test.cpp)
  target_include_directories(flight_cpp_generated_scene2d_gl_scissor_rect_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include")
  target_link_libraries(flight_cpp_generated_scene2d_gl_scissor_rect_tests PRIVATE Flight::Cpp Flight::SdkPreview Flight::HostSdlGl)
  add_test(NAME flight_cpp.generated_scene2d_gl_scissor_rect COMMAND flight_cpp_generated_scene2d_gl_scissor_rect_tests)
  flight_cpp_package_test_labels(flight_cpp.generated_scene2d_gl_scissor_rect scene2d-gl render-gl)
  if(UNIX AND NOT APPLE)
    set_tests_properties(flight_cpp.generated_scene2d_gl_scissor_rect PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=offscreen;LIBGL_ALWAYS_SOFTWARE=1")
  endif()
  add_executable(flight_cpp_generated_scene2d_gl_owners_tests generated_scene2d_gl_owners_test.cpp)
  target_include_directories(flight_cpp_generated_scene2d_gl_owners_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include")
  target_link_libraries(flight_cpp_generated_scene2d_gl_owners_tests PRIVATE Flight::Cpp Flight::SdkPreview Flight::HostSdlGl)
  flight_cpp_set_project_warnings(flight_cpp_generated_scene2d_gl_owners_tests)
  add_test(NAME flight_cpp.generated_scene2d_gl_owners COMMAND flight_cpp_generated_scene2d_gl_owners_tests)
  flight_cpp_package_test_labels(flight_cpp.generated_scene2d_gl_owners render render-gl scene2d scene2d-gl shape text types)
endif()

add_executable(
  flight_cpp_generated_render_scene2d_coverage_tests
  generated_render_scene2d_coverage_test.cpp
)
target_include_directories(
  flight_cpp_generated_render_scene2d_coverage_tests
  BEFORE PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/render_scene2d_coverage_support"
    "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_render_scene2d_coverage_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_render_scene2d_coverage
  COMMAND flight_cpp_generated_render_scene2d_coverage_tests
)

add_executable(
  flight_cpp_generated_canvas_render_state_owner_tests
  generated_canvas_render_state_owner_test.cpp
)
target_include_directories(
  flight_cpp_generated_canvas_render_state_owner_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_canvas_render_state_owner_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_canvas_render_state_owner
  COMMAND flight_cpp_generated_canvas_render_state_owner_tests
)

add_executable(
  flight_cpp_generated_canvas_texture_resolver_tests
  generated_canvas_texture_resolver_test.cpp
)
target_include_directories(
  flight_cpp_generated_canvas_texture_resolver_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_canvas_texture_resolver_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_canvas_texture_resolver
  COMMAND flight_cpp_generated_canvas_texture_resolver_tests
)

add_executable(flight_cpp_generated_texture_tests generated_texture_test.cpp)
target_include_directories(
  flight_cpp_generated_texture_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_texture_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(NAME flight_cpp.generated_texture COMMAND flight_cpp_generated_texture_tests)

add_executable(flight_cpp_generated_texture_options_row_tests generated_texture_options_row_test.cpp)
target_link_libraries(flight_cpp_generated_texture_options_row_tests PRIVATE Flight::Cpp Flight::SdkPreview)
flight_cpp_set_project_warnings(flight_cpp_generated_texture_options_row_tests)
add_test(NAME flight_cpp.generated_texture_options_row COMMAND flight_cpp_generated_texture_options_row_tests)

add_executable(flight_cpp_generated_cube_texture_tests generated_cube_texture_test.cpp)
target_link_libraries(flight_cpp_generated_cube_texture_tests PRIVATE Flight::Cpp Flight::SdkPreview)
flight_cpp_set_project_warnings(flight_cpp_generated_cube_texture_tests)
add_test(NAME flight_cpp.generated_cube_texture COMMAND flight_cpp_generated_cube_texture_tests)

add_executable(flight_cpp_generated_video_texture_options_tests generated_video_texture_options_test.cpp)
target_link_libraries(flight_cpp_generated_video_texture_options_tests PRIVATE Flight::Cpp Flight::SdkPreview)
flight_cpp_set_project_warnings(flight_cpp_generated_video_texture_options_tests)
add_test(NAME flight_cpp.generated_video_texture_options COMMAND flight_cpp_generated_video_texture_options_tests)

add_executable(flight_cpp_generated_cube_texture_api_tests generated_cube_texture_api_test.cpp)
target_link_libraries(flight_cpp_generated_cube_texture_api_tests PRIVATE Flight::Cpp Flight::SdkPreview)
flight_cpp_set_project_warnings(flight_cpp_generated_cube_texture_api_tests)
add_test(NAME flight_cpp.generated_cube_texture_api COMMAND flight_cpp_generated_cube_texture_api_tests)

add_executable(flight_cpp_generated_bitmap_tests generated_bitmap_test.cpp)
target_include_directories(flight_cpp_generated_bitmap_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include")
target_link_libraries(flight_cpp_generated_bitmap_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_bitmap COMMAND flight_cpp_generated_bitmap_tests)

add_executable(
  flight_cpp_generated_bitmapfont_tests
  generated_bitmapfont_test.cpp
)
target_include_directories(
  flight_cpp_generated_bitmapfont_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_bitmapfont_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_bitmapfont
  COMMAND flight_cpp_generated_bitmapfont_tests
)

add_executable(flight_cpp_generated_clip_tests generated_clip_test.cpp)
target_include_directories(
  flight_cpp_generated_clip_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_clip_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(NAME flight_cpp.generated_clip COMMAND flight_cpp_generated_clip_tests)

# Additive edge coverage relative to the integrated generated_clip test. Keeping this as a
# separate target lets the live ownership/composition baseline remain byte-for-byte unchanged.
add_executable(flight_cpp_generated_clip_edge_tests generated_clip_edge_test.cpp)
target_include_directories(
  flight_cpp_generated_clip_edge_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_clip_edge_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_clip_edge COMMAND flight_cpp_generated_clip_edge_tests)

add_executable(flight_cpp_generated_camera_tests generated_camera_test.cpp)
target_include_directories(
  flight_cpp_generated_camera_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_camera_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(NAME flight_cpp.generated_camera COMMAND flight_cpp_generated_camera_tests)

# Additive edge coverage relative to the integrated generated_camera and
# generated_camera_controls tests. The live baseline remains byte-for-byte unchanged.
add_executable(flight_cpp_generated_camera_edge_tests generated_camera_edge_test.cpp)
target_include_directories(
  flight_cpp_generated_camera_edge_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_camera_edge_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(NAME flight_cpp.generated_camera_edge COMMAND flight_cpp_generated_camera_edge_tests)

add_executable(flight_cpp_generated_entity_tests generated_entity_test.cpp)
target_include_directories(
  flight_cpp_generated_entity_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_entity_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(NAME flight_cpp.generated_entity COMMAND flight_cpp_generated_entity_tests)

add_executable(
  flight_cpp_generated_camera_controls_tests
  generated_camera_controls_test.cpp
)
target_include_directories(
  flight_cpp_generated_camera_controls_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_camera_controls_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_camera_controls
  COMMAND flight_cpp_generated_camera_controls_tests
)

add_executable(
  flight_cpp_generated_textureatlas_tests
  generated_textureatlas_test.cpp
)
target_include_directories(
  flight_cpp_generated_textureatlas_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_textureatlas_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_textureatlas
  COMMAND flight_cpp_generated_textureatlas_tests
)

add_executable(
  flight_cpp_generated_texture_formats_tests
  generated_texture_formats_test.cpp
)
target_include_directories(
  flight_cpp_generated_texture_formats_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_texture_formats_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_texture_formats
  COMMAND flight_cpp_generated_texture_formats_tests
)

add_executable(
  flight_cpp_generated_textureatlas_formats_tests
  generated_textureatlas_formats_test.cpp
)
target_include_directories(
  flight_cpp_generated_textureatlas_formats_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_textureatlas_formats_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_textureatlas_formats
  COMMAND flight_cpp_generated_textureatlas_formats_tests
)

add_executable(
  flight_cpp_generated_tilemap_tests
  generated_tilemap_test.cpp
)
target_include_directories(
  flight_cpp_generated_tilemap_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_tilemap_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_tilemap
  COMMAND flight_cpp_generated_tilemap_tests
)

add_executable(
  flight_cpp_generated_tilemap_formats_tests
  generated_tilemap_formats_test.cpp
)
target_include_directories(
  flight_cpp_generated_tilemap_formats_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_tilemap_formats_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_tilemap_formats
  COMMAND flight_cpp_generated_tilemap_formats_tests
)

add_executable(
  flight_cpp_generated_gizmo_tests
  generated_gizmo_test.cpp
)
target_include_directories(
  flight_cpp_generated_gizmo_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_gizmo_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_gizmo
  COMMAND flight_cpp_generated_gizmo_tests
)

add_executable(
  flight_cpp_generated_selection_tests
  generated_selection_test.cpp
)
target_include_directories(
  flight_cpp_generated_selection_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_selection_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_selection
  COMMAND flight_cpp_generated_selection_tests
)

add_executable(
  flight_cpp_generated_spritesheet_formats_tests
  generated_spritesheet_formats_test.cpp
)
target_include_directories(
  flight_cpp_generated_spritesheet_formats_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_spritesheet_formats_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_spritesheet_formats
  COMMAND flight_cpp_generated_spritesheet_formats_tests
)

add_executable(flight_cpp_generated_spritesheet_tests generated_spritesheet_test.cpp)
target_include_directories(
  flight_cpp_generated_spritesheet_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_spritesheet_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(NAME flight_cpp.generated_spritesheet COMMAND flight_cpp_generated_spritesheet_tests)

add_executable(flight_cpp_generated_materials_tests generated_materials_test.cpp)
target_include_directories(
  flight_cpp_generated_materials_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_materials_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(NAME flight_cpp.generated_materials COMMAND flight_cpp_generated_materials_tests)

add_executable(flight_cpp_generated_lighting_tests generated_lighting_test.cpp)
target_include_directories(
  flight_cpp_generated_lighting_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_lighting_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(NAME flight_cpp.generated_lighting COMMAND flight_cpp_generated_lighting_tests)

add_executable(flight_cpp_generated_shading_tests generated_shading_test.cpp)
target_include_directories(
  flight_cpp_generated_shading_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_shading_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(NAME flight_cpp.generated_shading COMMAND flight_cpp_generated_shading_tests)

add_executable(flight_cpp_generated_scene3d_tests generated_scene3d_test.cpp)
target_include_directories(
  flight_cpp_generated_scene3d_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_scene3d_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(NAME flight_cpp.generated_scene3d COMMAND flight_cpp_generated_scene3d_tests)

add_executable(flight_cpp_generated_scene3d_kind_reexports_tests generated_scene3d_kind_reexports_test.cpp)
target_include_directories(
  flight_cpp_generated_scene3d_kind_reexports_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_scene3d_kind_reexports_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_scene3d_kind_reexports COMMAND flight_cpp_generated_scene3d_kind_reexports_tests)

add_executable(
  flight_cpp_generated_scene3d_formats_tests
  generated_scene3d_formats_test.cpp
)
target_include_directories(
  flight_cpp_generated_scene3d_formats_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_scene3d_formats_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_scene3d_formats
  COMMAND flight_cpp_generated_scene3d_formats_tests
)

add_executable(
  flight_cpp_generated_scene3d_resources_tests
  generated_scene3d_resources_test.cpp
)
target_include_directories(
  flight_cpp_generated_scene3d_resources_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_scene3d_resources_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_scene3d_resources
  COMMAND flight_cpp_generated_scene3d_resources_tests
)

add_executable(flight_cpp_generated_mesh_tests generated_mesh_test.cpp)
target_include_directories(
  flight_cpp_generated_mesh_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_mesh_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(NAME flight_cpp.generated_mesh COMMAND flight_cpp_generated_mesh_tests)

add_executable(flight_cpp_generated_skeleton3d_tests generated_skeleton3d_test.cpp)
target_include_directories(
  flight_cpp_generated_skeleton3d_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_skeleton3d_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(NAME flight_cpp.generated_skeleton3d COMMAND flight_cpp_generated_skeleton3d_tests)

add_executable(flight_cpp_generated_interaction_tests generated_interaction_test.cpp)
target_include_directories(
  flight_cpp_generated_interaction_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_interaction_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_interaction COMMAND flight_cpp_generated_interaction_tests)

add_executable(flight_cpp_generated_scene2d_display_object_tests generated_scene2d_display_object_test.cpp)
target_include_directories(
  flight_cpp_generated_scene2d_display_object_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_scene2d_display_object_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_scene2d_display_object COMMAND flight_cpp_generated_scene2d_display_object_tests)

add_executable(flight_cpp_generated_effect_interpolation_tests generated_effect_interpolation_test.cpp)
target_include_directories(
  flight_cpp_generated_effect_interpolation_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_effect_interpolation_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_effect_interpolation COMMAND flight_cpp_generated_effect_interpolation_tests)

add_executable(flight_cpp_generated_effect_padding_tests generated_effect_padding_test.cpp)
target_include_directories(
  flight_cpp_generated_effect_padding_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_effect_padding_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_effect_padding COMMAND flight_cpp_generated_effect_padding_tests)

add_executable(flight_cpp_generated_shape_bounds_tests generated_shape_bounds_test.cpp)
target_include_directories(
  flight_cpp_generated_shape_bounds_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_shape_bounds_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_shape_bounds COMMAND flight_cpp_generated_shape_bounds_tests)

add_executable(flight_cpp_generated_intl_cache_tests generated_intl_cache_test.cpp)
target_include_directories(
  flight_cpp_generated_intl_cache_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_intl_cache_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_intl_cache COMMAND flight_cpp_generated_intl_cache_tests)

add_executable(flight_cpp_generated_image_test_helper_tests generated_image_test_helper_test.cpp)
target_include_directories(
  flight_cpp_generated_image_test_helper_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_image_test_helper_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_image_test_helper
  COMMAND flight_cpp_generated_image_test_helper_tests
)

add_executable(flight_cpp_generated_image_tests generated_image_test.cpp)
target_include_directories(
  flight_cpp_generated_image_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_image_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(NAME flight_cpp.generated_image COMMAND flight_cpp_generated_image_tests)

add_executable(flight_cpp_generated_image_codec_tests generated_image_codec_test.cpp)
target_include_directories(
  flight_cpp_generated_image_codec_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_image_codec_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_image_codec
  COMMAND flight_cpp_generated_image_codec_tests
)

add_executable(flight_cpp_generated_statusbar_tests generated_statusbar_test.cpp)
target_include_directories(
  flight_cpp_generated_statusbar_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_statusbar_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_statusbar COMMAND flight_cpp_generated_statusbar_tests)

add_executable(flight_cpp_generated_audio_tests generated_audio_test.cpp)
target_include_directories(
  flight_cpp_generated_audio_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_audio_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_audio COMMAND flight_cpp_generated_audio_tests)

add_executable(flight_cpp_generated_input_tests generated_input_test.cpp)
target_include_directories(
  flight_cpp_generated_input_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_input_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_input COMMAND flight_cpp_generated_input_tests)

add_executable(
  flight_cpp_generated_app_render_view_tests
  generated_app_render_view_test.cpp
)
target_include_directories(
  flight_cpp_generated_app_render_view_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_app_render_view_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_app_render_view
  COMMAND flight_cpp_generated_app_render_view_tests
)

add_executable(flight_cpp_generated_mediasession_tests generated_mediasession_test.cpp)
target_include_directories(
  flight_cpp_generated_mediasession_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_mediasession_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_mediasession COMMAND flight_cpp_generated_mediasession_tests)

add_executable(flight_cpp_generated_video_tests generated_video_test.cpp)
target_include_directories(
  flight_cpp_generated_video_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_video_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_video COMMAND flight_cpp_generated_video_tests)

add_executable(flight_cpp_generated_sensors_tests generated_sensors_test.cpp)
target_include_directories(
  flight_cpp_generated_sensors_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_sensors_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_sensors COMMAND flight_cpp_generated_sensors_tests)

add_executable(flight_cpp_generated_geolocation_tests generated_geolocation_test.cpp)
target_include_directories(
  flight_cpp_generated_geolocation_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_geolocation_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_geolocation COMMAND flight_cpp_generated_geolocation_tests)

add_executable(flight_cpp_generated_haptics_tests generated_haptics_test.cpp)
target_include_directories(
  flight_cpp_generated_haptics_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_haptics_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_haptics COMMAND flight_cpp_generated_haptics_tests)

add_executable(flight_cpp_generated_device_tests generated_device_test.cpp)
target_include_directories(
  flight_cpp_generated_device_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_device_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_device COMMAND flight_cpp_generated_device_tests)

add_executable(flight_cpp_generated_notification_tests generated_notification_test.cpp)
target_include_directories(
  flight_cpp_generated_notification_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_notification_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_notification COMMAND flight_cpp_generated_notification_tests)

add_executable(flight_cpp_generated_midi_tests generated_midi_test.cpp)
target_include_directories(
  flight_cpp_generated_midi_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_midi_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_midi COMMAND flight_cpp_generated_midi_tests)

add_executable(flight_cpp_generated_dialog_modules_tests generated_dialog_modules_test.cpp)
target_include_directories(
  flight_cpp_generated_dialog_modules_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_dialog_modules_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_dialog_modules COMMAND flight_cpp_generated_dialog_modules_tests)

add_executable(flight_cpp_generated_menu_tests generated_menu_test.cpp)
target_include_directories(
  flight_cpp_generated_menu_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_menu_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_menu COMMAND flight_cpp_generated_menu_tests)

add_executable(flight_cpp_generated_shortcut_tests generated_shortcut_test.cpp)
target_include_directories(
  flight_cpp_generated_shortcut_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_shortcut_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_shortcut COMMAND flight_cpp_generated_shortcut_tests)

add_executable(flight_cpp_generated_shortcut_accelerator_tests generated_shortcut_accelerator_test.cpp)
target_include_directories(
  flight_cpp_generated_shortcut_accelerator_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_shortcut_accelerator_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_shortcut_accelerator COMMAND flight_cpp_generated_shortcut_accelerator_tests)

add_executable(flight_cpp_generated_shell_tests generated_shell_test.cpp)
target_include_directories(
  flight_cpp_generated_shell_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_shell_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_shell COMMAND flight_cpp_generated_shell_tests)

add_executable(flight_cpp_generated_ipc_tests generated_ipc_test.cpp)
target_include_directories(
  flight_cpp_generated_ipc_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_ipc_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_ipc COMMAND flight_cpp_generated_ipc_tests)

add_executable(flight_cpp_generated_protocol_tests generated_protocol_test.cpp)
target_include_directories(
  flight_cpp_generated_protocol_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_protocol_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_protocol COMMAND flight_cpp_generated_protocol_tests)

add_executable(flight_cpp_generated_quadbatch_tests generated_quadbatch_test.cpp)
target_include_directories(
  flight_cpp_generated_quadbatch_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_quadbatch_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_quadbatch COMMAND flight_cpp_generated_quadbatch_tests)

add_executable(flight_cpp_generated_permissions_tests generated_permissions_test.cpp)
target_include_directories(
  flight_cpp_generated_permissions_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_permissions_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_permissions COMMAND flight_cpp_generated_permissions_tests)

add_executable(flight_cpp_generated_filesystem_tests generated_filesystem_test.cpp)
target_include_directories(
  flight_cpp_generated_filesystem_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_filesystem_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_filesystem COMMAND flight_cpp_generated_filesystem_tests)

add_executable(flight_cpp_generated_preferences_tests generated_preferences_test.cpp)
target_include_directories(
  flight_cpp_generated_preferences_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_preferences_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_preferences COMMAND flight_cpp_generated_preferences_tests)

add_executable(flight_cpp_generated_power_tests generated_power_test.cpp)
target_include_directories(
  flight_cpp_generated_power_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_power_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_power COMMAND flight_cpp_generated_power_tests)

add_executable(flight_cpp_generated_connectivity_tests generated_connectivity_test.cpp)
target_include_directories(
  flight_cpp_generated_connectivity_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_connectivity_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_connectivity COMMAND flight_cpp_generated_connectivity_tests)

add_executable(flight_cpp_generated_share_tests generated_share_test.cpp)
target_include_directories(
  flight_cpp_generated_share_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_share_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_share COMMAND flight_cpp_generated_share_tests)

add_executable(flight_cpp_generated_updater_tests generated_updater_test.cpp)
target_include_directories(
  flight_cpp_generated_updater_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_updater_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_updater COMMAND flight_cpp_generated_updater_tests)

add_executable(flight_cpp_generated_signals_throttle_tests generated_signals_throttle_test.cpp)
target_include_directories(
  flight_cpp_generated_signals_throttle_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_signals_throttle_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_signals_throttle COMMAND flight_cpp_generated_signals_throttle_tests)

add_executable(flight_cpp_generated_signals_tests generated_signals_test.cpp)
target_include_directories(
  flight_cpp_generated_signals_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_signals_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_signals COMMAND flight_cpp_generated_signals_tests)

add_executable(flight_cpp_generated_spring_tests generated_spring_test.cpp)
target_include_directories(
  flight_cpp_generated_spring_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_spring_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_spring COMMAND flight_cpp_generated_spring_tests)

add_executable(flight_cpp_generated_timeline_tests generated_timeline_test.cpp)
target_include_directories(
  flight_cpp_generated_timeline_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_timeline_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_timeline COMMAND flight_cpp_generated_timeline_tests)

add_executable(flight_cpp_generated_velocity_tests generated_velocity_test.cpp)
target_include_directories(
  flight_cpp_generated_velocity_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_velocity_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_velocity COMMAND flight_cpp_generated_velocity_tests)

add_executable(flight_cpp_generated_motionpath_tests generated_motionpath_test.cpp)
target_include_directories(
  flight_cpp_generated_motionpath_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_motionpath_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_motionpath COMMAND flight_cpp_generated_motionpath_tests)

add_executable(flight_cpp_generated_path_tests generated_path_test.cpp)
target_include_directories(
  flight_cpp_generated_path_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_path_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_path COMMAND flight_cpp_generated_path_tests)

add_executable(
  flight_cpp_generated_path_formats_tests
  generated_path_formats_test.cpp
)
target_include_directories(
  flight_cpp_generated_path_formats_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_path_formats_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_path_formats
  COMMAND flight_cpp_generated_path_formats_tests
)

add_executable(
  flight_cpp_generated_scene_document_tests
  generated_scene_document_test.cpp
)
target_include_directories(
  flight_cpp_generated_scene_document_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_scene_document_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_scene_document
  COMMAND flight_cpp_generated_scene_document_tests
)

add_executable(
  flight_cpp_generated_scene2d_resources_tests
  generated_scene2d_resources_test.cpp
)
target_include_directories(
  flight_cpp_generated_scene2d_resources_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_scene2d_resources_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_scene2d_resources
  COMMAND flight_cpp_generated_scene2d_resources_tests
)

add_executable(
  flight_cpp_generated_scene2d_resource_loading_tests
  generated_scene2d_resource_loading_test.cpp
)
target_include_directories(
  flight_cpp_generated_scene2d_resource_loading_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_scene2d_resource_loading_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_scene2d_resource_loading
  COMMAND flight_cpp_generated_scene2d_resource_loading_tests
)

add_executable(flight_cpp_generated_clock_tests generated_clock_test.cpp)
target_include_directories(
  flight_cpp_generated_clock_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_clock_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_clock COMMAND flight_cpp_generated_clock_tests)

add_executable(flight_cpp_generated_movieclip_tests generated_movieclip_test.cpp)
target_include_directories(
  flight_cpp_generated_movieclip_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_movieclip_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_movieclip COMMAND flight_cpp_generated_movieclip_tests)

add_executable(flight_cpp_generated_swf_reader_tests generated_swf_reader_test.cpp)
target_include_directories(
  flight_cpp_generated_swf_reader_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_swf_reader_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_swf_reader COMMAND flight_cpp_generated_swf_reader_tests)

add_executable(flight_cpp_generated_swf_video_texture_tests generated_swf_video_texture_test.cpp)
target_include_directories(
  flight_cpp_generated_swf_video_texture_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_swf_video_texture_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_swf_video_texture COMMAND flight_cpp_generated_swf_video_texture_tests)

add_executable(flight_cpp_generated_swf_tests generated_swf_test.cpp)
target_include_directories(
  flight_cpp_generated_swf_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_swf_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_swf COMMAND flight_cpp_generated_swf_tests)

add_executable(flight_cpp_generated_physics2d_tests generated_physics2d_test.cpp)
target_include_directories(
  flight_cpp_generated_physics2d_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_physics2d_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_physics2d COMMAND flight_cpp_generated_physics2d_tests)

add_executable(flight_cpp_generated_spatial_tests generated_spatial_test.cpp)
target_include_directories(
  flight_cpp_generated_spatial_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_spatial_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_spatial COMMAND flight_cpp_generated_spatial_tests)

add_executable(flight_cpp_generated_spatial_notice_tests generated_spatial_notice_test.cpp)
target_include_directories(
  flight_cpp_generated_spatial_notice_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_spatial_notice_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_spatial_notice COMMAND flight_cpp_generated_spatial_notice_tests)

add_executable(flight_cpp_generated_input_manager_tests generated_input_manager_test.cpp)
target_include_directories(
  flight_cpp_generated_input_manager_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_input_manager_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_input_manager COMMAND flight_cpp_generated_input_manager_tests)

add_executable(flight_cpp_generated_input_state_tests generated_input_state_test.cpp)
target_include_directories(
  flight_cpp_generated_input_state_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_input_state_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_input_state COMMAND flight_cpp_generated_input_state_tests)

add_executable(flight_cpp_generated_input_binding_tests generated_input_binding_test.cpp)
target_include_directories(
  flight_cpp_generated_input_binding_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_input_binding_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_input_binding COMMAND flight_cpp_generated_input_binding_tests)

add_executable(flight_cpp_generated_keyboard_tests generated_keyboard_test.cpp)
target_include_directories(
  flight_cpp_generated_keyboard_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_keyboard_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_keyboard COMMAND flight_cpp_generated_keyboard_tests)

add_executable(flight_cpp_generated_skeleton2d_tests generated_skeleton2d_test.cpp)
target_include_directories(
  flight_cpp_generated_skeleton2d_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_skeleton2d_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_skeleton2d COMMAND flight_cpp_generated_skeleton2d_tests)

add_executable(flight_cpp_generated_spine_binary_reader_tests generated_spine_binary_reader_test.cpp)
target_include_directories(
  flight_cpp_generated_spine_binary_reader_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_spine_binary_reader_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_spine_binary_reader COMMAND flight_cpp_generated_spine_binary_reader_tests)

add_executable(flight_cpp_generated_animation_player_tests generated_animation_player_test.cpp)
target_include_directories(
  flight_cpp_generated_animation_player_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_animation_player_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_animation_player COMMAND flight_cpp_generated_animation_player_tests)

add_executable(flight_cpp_generated_animation_blend_tests generated_animation_blend_test.cpp)
target_include_directories(
  flight_cpp_generated_animation_blend_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_animation_blend_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_animation_blend COMMAND flight_cpp_generated_animation_blend_tests)

add_executable(flight_cpp_generated_animation_graph_tests generated_animation_graph_test.cpp)
target_include_directories(
  flight_cpp_generated_animation_graph_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_animation_graph_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_animation_graph COMMAND flight_cpp_generated_animation_graph_tests)

add_executable(flight_cpp_generated_animation_layers_tests generated_animation_layers_test.cpp)
target_include_directories(
  flight_cpp_generated_animation_layers_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_animation_layers_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_animation_layers COMMAND flight_cpp_generated_animation_layers_tests)

add_executable(flight_cpp_generated_timeline_target_tests generated_timeline_target_test.cpp)
target_include_directories(
  flight_cpp_generated_timeline_target_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_timeline_target_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_timeline_target COMMAND flight_cpp_generated_timeline_target_tests)

add_executable(flight_cpp_generated_tween_tests generated_tween_test.cpp)
target_include_directories(
  flight_cpp_generated_tween_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_tween_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_tween COMMAND flight_cpp_generated_tween_tests)

add_executable(flight_cpp_generated_easing_tests generated_easing_test.cpp)
target_include_directories(
  flight_cpp_generated_easing_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_easing_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_easing COMMAND flight_cpp_generated_easing_tests)

add_executable(flight_cpp_generated_flow_tests generated_flow_test.cpp)
target_include_directories(
  flight_cpp_generated_flow_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_flow_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_flow COMMAND flight_cpp_generated_flow_tests)

add_executable(flight_cpp_generated_statechart_tests generated_statechart_test.cpp)
target_include_directories(
  flight_cpp_generated_statechart_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_statechart_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_statechart COMMAND flight_cpp_generated_statechart_tests)

add_executable(flight_cpp_generated_log_core_tests generated_log_core_test.cpp)
target_include_directories(
  flight_cpp_generated_log_core_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_log_core_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_log_core COMMAND flight_cpp_generated_log_core_tests)

add_executable(flight_cpp_generated_particles_tests generated_particles_test.cpp)
target_include_directories(
  flight_cpp_generated_particles_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_particles_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_particles COMMAND flight_cpp_generated_particles_tests)

add_executable(flight_cpp_generated_particles_formats_tests generated_particles_formats_test.cpp)
target_include_directories(
  flight_cpp_generated_particles_formats_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_particles_formats_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_particles_formats COMMAND flight_cpp_generated_particles_formats_tests)

add_executable(flight_cpp_generated_particles_formats_libgdx_tests generated_particles_formats_libgdx_test.cpp)
target_include_directories(
  flight_cpp_generated_particles_formats_libgdx_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_particles_formats_libgdx_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_particles_formats_libgdx COMMAND flight_cpp_generated_particles_formats_libgdx_tests)

add_executable(flight_cpp_generated_particles_formats_particle_designer_tests generated_particles_formats_particle_designer_test.cpp)
target_include_directories(
  flight_cpp_generated_particles_formats_particle_designer_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_particles_formats_particle_designer_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_particles_formats_particle_designer COMMAND flight_cpp_generated_particles_formats_particle_designer_tests)

add_executable(flight_cpp_generated_particleemitter_tests generated_particleemitter_test.cpp)
target_include_directories(
  flight_cpp_generated_particleemitter_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_particleemitter_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_particleemitter COMMAND flight_cpp_generated_particleemitter_tests)

add_executable(flight_cpp_generated_skeleton2d_formats_spine_binary_tests generated_skeleton2d_formats_spine_binary_test.cpp)
target_include_directories(
  flight_cpp_generated_skeleton2d_formats_spine_binary_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_skeleton2d_formats_spine_binary_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_skeleton2d_formats_spine_binary COMMAND flight_cpp_generated_skeleton2d_formats_spine_binary_tests)

add_executable(flight_cpp_generated_skeleton2d_formats_registry_tests generated_skeleton2d_formats_registry_test.cpp)
target_include_directories(
  flight_cpp_generated_skeleton2d_formats_registry_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_skeleton2d_formats_registry_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_skeleton2d_formats_registry COMMAND flight_cpp_generated_skeleton2d_formats_registry_tests)

add_executable(flight_cpp_generated_physics2d_abi_tests generated_physics2d_abi_test.cpp)
target_include_directories(
  flight_cpp_generated_physics2d_abi_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_physics2d_abi_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_physics2d_abi COMMAND flight_cpp_generated_physics2d_abi_tests)

add_executable(flight_cpp_generated_physics2d_bodies_tests generated_physics2d_bodies_test.cpp)
target_include_directories(
  flight_cpp_generated_physics2d_bodies_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_physics2d_bodies_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_physics2d_bodies COMMAND flight_cpp_generated_physics2d_bodies_tests)

add_executable(flight_cpp_generated_physics2d_world_tests generated_physics2d_world_test.cpp)
target_include_directories(
  flight_cpp_generated_physics2d_world_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_physics2d_world_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_physics2d_world COMMAND flight_cpp_generated_physics2d_world_tests)

add_executable(flight_cpp_generated_physics2d_step_tests generated_physics2d_step_test.cpp)
target_include_directories(
  flight_cpp_generated_physics2d_step_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_physics2d_step_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_physics2d_step COMMAND flight_cpp_generated_physics2d_step_tests)

add_executable(flight_cpp_generated_physics2d_queries_tests generated_physics2d_queries_test.cpp)
target_include_directories(
  flight_cpp_generated_physics2d_queries_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_physics2d_queries_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_physics2d_queries COMMAND flight_cpp_generated_physics2d_queries_tests)

add_executable(flight_cpp_generated_physics2d_boundaries_tests generated_physics2d_boundaries_test.cpp)
target_include_directories(
  flight_cpp_generated_physics2d_boundaries_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_physics2d_boundaries_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_physics2d_boundaries COMMAND flight_cpp_generated_physics2d_boundaries_tests)

add_executable(flight_cpp_generated_lifecycle_tests generated_lifecycle_test.cpp)
target_include_directories(
  flight_cpp_generated_lifecycle_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_lifecycle_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_lifecycle COMMAND flight_cpp_generated_lifecycle_tests)

add_executable(flight_cpp_generated_useragent_tests generated_useragent_test.cpp)
add_executable(flight_cpp_generated_platform_tests generated_platform_test.cpp)
target_include_directories(flight_cpp_generated_platform_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include")
target_link_libraries(flight_cpp_generated_platform_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_platform COMMAND flight_cpp_generated_platform_tests)

add_executable(flight_cpp_generated_layout_tests generated_layout_test.cpp)
target_include_directories(flight_cpp_generated_layout_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include")
target_link_libraries(flight_cpp_generated_layout_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_layout COMMAND flight_cpp_generated_layout_tests)

add_executable(flight_cpp_generated_accessibility_tests generated_accessibility_test.cpp)
target_include_directories(flight_cpp_generated_accessibility_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include")
target_link_libraries(flight_cpp_generated_accessibility_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_accessibility COMMAND flight_cpp_generated_accessibility_tests)

add_executable(flight_cpp_generated_lifecycle_transitions_tests generated_lifecycle_transitions_test.cpp)
target_include_directories(flight_cpp_generated_lifecycle_transitions_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include")
target_link_libraries(flight_cpp_generated_lifecycle_transitions_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_lifecycle_transitions COMMAND flight_cpp_generated_lifecycle_transitions_tests)

add_executable(flight_cpp_generated_host_tests generated_host_test.cpp)
target_include_directories(flight_cpp_generated_host_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include")
target_link_libraries(flight_cpp_generated_host_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_host COMMAND flight_cpp_generated_host_tests)

add_executable(flight_cpp_generated_app_core_tests generated_app_core_test.cpp)
target_include_directories(flight_cpp_generated_app_core_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include")
target_link_libraries(flight_cpp_generated_app_core_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_app_core COMMAND flight_cpp_generated_app_core_tests)

add_executable(flight_cpp_generated_app_window_tests generated_app_window_test.cpp)
target_include_directories(flight_cpp_generated_app_window_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include")
target_link_libraries(flight_cpp_generated_app_window_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_app_window COMMAND flight_cpp_generated_app_window_tests)

add_executable(flight_cpp_generated_app_loop_tests generated_app_loop_test.cpp)
target_include_directories(flight_cpp_generated_app_loop_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include")
target_link_libraries(flight_cpp_generated_app_loop_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_app_loop COMMAND flight_cpp_generated_app_loop_tests)

target_include_directories(
  flight_cpp_generated_useragent_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_useragent_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_useragent COMMAND flight_cpp_generated_useragent_tests)

add_executable(flight_cpp_generated_dialog_tests generated_dialog_test.cpp)
target_include_directories(
  flight_cpp_generated_dialog_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_dialog_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_dialog COMMAND flight_cpp_generated_dialog_tests)

add_executable(flight_cpp_generated_textshaper_cache_tests generated_textshaper_cache_test.cpp)
target_include_directories(
  flight_cpp_generated_textshaper_cache_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_textshaper_cache_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_textshaper_cache COMMAND flight_cpp_generated_textshaper_cache_tests)

add_executable(flight_cpp_generated_assets_tests generated_assets_test.cpp)
target_include_directories(
  flight_cpp_generated_assets_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_assets_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_assets COMMAND flight_cpp_generated_assets_tests)

add_executable(flight_cpp_generated_importdiagnostics_tests generated_importdiagnostics_test.cpp)
target_include_directories(
  flight_cpp_generated_importdiagnostics_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_importdiagnostics_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_importdiagnostics COMMAND flight_cpp_generated_importdiagnostics_tests)

add_executable(flight_cpp_generated_loader_tests generated_loader_test.cpp)
target_include_directories(
  flight_cpp_generated_loader_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_loader_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_loader COMMAND flight_cpp_generated_loader_tests)

add_executable(flight_cpp_generated_net_tests generated_net_test.cpp)
target_include_directories(
  flight_cpp_generated_net_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_net_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_net COMMAND flight_cpp_generated_net_tests)

add_executable(flight_cpp_generated_socket_tests generated_socket_test.cpp)
target_include_directories(
  flight_cpp_generated_socket_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_socket_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_socket COMMAND flight_cpp_generated_socket_tests)

add_executable(flight_cpp_generated_encoding_tests generated_encoding_test.cpp)
target_include_directories(
  flight_cpp_generated_encoding_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_encoding_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_encoding COMMAND flight_cpp_generated_encoding_tests)

add_executable(flight_cpp_generated_binpack_tests generated_binpack_test.cpp)
target_include_directories(
  flight_cpp_generated_binpack_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_binpack_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_binpack COMMAND flight_cpp_generated_binpack_tests)

add_executable(flight_cpp_generated_compression_tests generated_compression_test.cpp)
target_include_directories(
  flight_cpp_generated_compression_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_compression_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_compression COMMAND flight_cpp_generated_compression_tests)

add_executable(flight_cpp_generated_path_boolean_tests generated_path_boolean_test.cpp)
target_include_directories(
  flight_cpp_generated_path_boolean_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_path_boolean_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_path_boolean COMMAND flight_cpp_generated_path_boolean_tests)

add_executable(flight_cpp_generated_abc_tests generated_abc_test.cpp)
target_include_directories(
  flight_cpp_generated_abc_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_abc_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_abc COMMAND flight_cpp_generated_abc_tests)

add_executable(flight_cpp_generated_xml_tests generated_xml_test.cpp)
target_include_directories(
  flight_cpp_generated_xml_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_xml_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_xml COMMAND flight_cpp_generated_xml_tests)

add_executable(flight_cpp_generated_command_tests generated_command_test.cpp)
target_include_directories(
  flight_cpp_generated_command_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_command_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_command COMMAND flight_cpp_generated_command_tests)

add_executable(flight_cpp_generated_math_tests generated_math_test.cpp)
target_include_directories(
  flight_cpp_generated_math_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_math_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_math COMMAND flight_cpp_generated_math_tests)

add_executable(flight_cpp_generated_geometry_matrix34_tests generated_geometry_matrix34_test.cpp)
target_include_directories(
  flight_cpp_generated_geometry_matrix34_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_geometry_matrix34_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_geometry_matrix34 COMMAND flight_cpp_generated_geometry_matrix34_tests)

add_executable(flight_cpp_generated_geometry_planar_tests generated_geometry_planar_test.cpp)
target_include_directories(
  flight_cpp_generated_geometry_planar_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_geometry_planar_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_geometry_planar COMMAND flight_cpp_generated_geometry_planar_tests)

add_executable(flight_cpp_generated_geometry_vectors_tests generated_geometry_vectors_test.cpp)
target_include_directories(
  flight_cpp_generated_geometry_vectors_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_geometry_vectors_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_geometry_vectors COMMAND flight_cpp_generated_geometry_vectors_tests)

add_executable(flight_cpp_generated_geometry_volumes_tests generated_geometry_volumes_test.cpp)
target_include_directories(
  flight_cpp_generated_geometry_volumes_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_geometry_volumes_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_geometry_volumes COMMAND flight_cpp_generated_geometry_volumes_tests)

add_executable(flight_cpp_generated_socket_statechart_tests generated_socket_statechart_test.cpp)
target_include_directories(
  flight_cpp_generated_socket_statechart_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_socket_statechart_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_socket_statechart COMMAND flight_cpp_generated_socket_statechart_tests)

add_executable(flight_cpp_generated_surface_tests generated_surface_test.cpp)
target_include_directories(
  flight_cpp_generated_surface_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_surface_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_surface COMMAND flight_cpp_generated_surface_tests)

add_executable(flight_cpp_generated_shape_tests generated_shape_test.cpp)
target_include_directories(
  flight_cpp_generated_shape_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_shape_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_shape COMMAND flight_cpp_generated_shape_tests)

add_executable(
  flight_cpp_generated_effects_wgpu_gradient_ramp_tests
  generated_effects_wgpu_gradient_ramp_test.cpp
)
target_include_directories(
  flight_cpp_generated_effects_wgpu_gradient_ramp_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_effects_wgpu_gradient_ramp_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_effects_wgpu_gradient_ramp
  COMMAND flight_cpp_generated_effects_wgpu_gradient_ramp_tests
)

add_executable(flight_cpp_generated_color_tests generated_color_test.cpp)
target_include_directories(
  flight_cpp_generated_color_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_color_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_color COMMAND flight_cpp_generated_color_tests)

add_executable(flight_cpp_generated_adjustments_tests generated_adjustments_test.cpp)
target_include_directories(
  flight_cpp_generated_adjustments_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_adjustments_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_adjustments COMMAND flight_cpp_generated_adjustments_tests)

add_executable(flight_cpp_generated_capture_tests generated_capture_test.cpp)
target_include_directories(
  flight_cpp_generated_capture_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_capture_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_capture COMMAND flight_cpp_generated_capture_tests)

add_executable(flight_cpp_generated_snapshot_tests generated_snapshot_test.cpp)
target_include_directories(
  flight_cpp_generated_snapshot_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_snapshot_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_snapshot COMMAND flight_cpp_generated_snapshot_tests)

add_executable(flight_cpp_generated_physics3d_tests generated_physics3d_test.cpp)
target_include_directories(
  flight_cpp_generated_physics3d_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_physics3d_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_physics3d COMMAND flight_cpp_generated_physics3d_tests)

add_executable(flight_cpp_generated_physics3d_abi_tests generated_physics3d_abi_test.cpp)
target_include_directories(
  flight_cpp_generated_physics3d_abi_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_physics3d_abi_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_physics3d_abi COMMAND flight_cpp_generated_physics3d_abi_tests)

add_executable(flight_cpp_generated_picking_tests generated_picking_test.cpp)
target_include_directories(
  flight_cpp_generated_picking_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_picking_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_picking COMMAND flight_cpp_generated_picking_tests)

flight_cpp_package_test_labels(flight_cpp.structural_row types)
flight_cpp_package_test_labels(flight_cpp.merged_row app entity types)
flight_cpp_package_test_labels(flight_cpp.indexed_member types)
flight_cpp_package_test_labels(flight_cpp.generated_collision collision)
flight_cpp_package_test_labels(flight_cpp.generated_collision_shapes collision)
flight_cpp_package_test_labels(flight_cpp.generated_collision_registries collision)
flight_cpp_package_test_labels(flight_cpp.generated_registry registry)
flight_cpp_package_test_labels(flight_cpp.generated_requirement requirement)
flight_cpp_package_test_labels(flight_cpp.generated_requirement_codegen requirement-codegen)
flight_cpp_package_test_labels(flight_cpp.generated_animation animation)
flight_cpp_package_test_labels(flight_cpp.generated_animation_player animation signals)
flight_cpp_package_test_labels(flight_cpp.generated_animation_blend animation)
flight_cpp_package_test_labels(flight_cpp.generated_animation_graph animation)
flight_cpp_package_test_labels(flight_cpp.generated_animation_layers animation)
flight_cpp_package_test_labels(flight_cpp.generated_log log)
flight_cpp_package_test_labels(flight_cpp.generated_log_levels_and_sinks log)
flight_cpp_package_test_labels(flight_cpp.generated_render_state render types)
flight_cpp_package_test_labels(flight_cpp.generated_render_scene2d_coverage render)
flight_cpp_package_test_labels(flight_cpp.generated_canvas_render_state_owner render types)
flight_cpp_package_test_labels(flight_cpp.generated_canvas_texture_resolver scene2d-canvas)
flight_cpp_package_test_labels(flight_cpp.generated_texture texture types)
flight_cpp_package_test_labels(flight_cpp.generated_texture_options_row texture)
flight_cpp_package_test_labels(flight_cpp.generated_cube_texture texture)
flight_cpp_package_test_labels(flight_cpp.generated_cube_texture_api texture)
flight_cpp_package_test_labels(flight_cpp.generated_bitmap bitmap)
flight_cpp_package_test_labels(flight_cpp.generated_bitmapfont bitmap bitmapfont)
flight_cpp_package_test_labels(flight_cpp.generated_texture_formats texture-formats)
flight_cpp_package_test_labels(flight_cpp.generated_clip clip)
flight_cpp_package_test_labels(flight_cpp.generated_clip_edge clip)
flight_cpp_package_test_labels(flight_cpp.generated_camera camera geometry)
flight_cpp_package_test_labels(flight_cpp.generated_camera_edge camera geometry types)
flight_cpp_package_test_labels(flight_cpp.generated_entity entity)
flight_cpp_package_test_labels(flight_cpp.generated_camera_controls camera camera-controls geometry)
flight_cpp_package_test_labels(flight_cpp.generated_textureatlas textureatlas)
flight_cpp_package_test_labels(flight_cpp.generated_textureatlas_formats textureatlas textureatlas-formats)
flight_cpp_package_test_labels(flight_cpp.generated_tilemap signals tilemap)
flight_cpp_package_test_labels(flight_cpp.generated_tilemap_formats tilemap-formats)
flight_cpp_package_test_labels(flight_cpp.generated_gizmo camera gizmo scene2d selection signals)
flight_cpp_package_test_labels(flight_cpp.generated_selection selection signals)
flight_cpp_package_test_labels(flight_cpp.generated_spritesheet_formats spritesheet-formats)
flight_cpp_package_test_labels(flight_cpp.generated_spritesheet signals spritesheet)
flight_cpp_package_test_labels(flight_cpp.generated_materials materials)
flight_cpp_package_test_labels(flight_cpp.generated_lighting geometry lighting)
flight_cpp_package_test_labels(flight_cpp.generated_shading shading)
flight_cpp_package_test_labels(flight_cpp.generated_scene3d materials mesh node scene3d)
flight_cpp_package_test_labels(flight_cpp.generated_scene3d_kind_reexports scene3d)
flight_cpp_package_test_labels(flight_cpp.generated_scene3d_formats scene3d-formats)
flight_cpp_package_test_labels(flight_cpp.generated_scene3d_resources scene3d-resources)
flight_cpp_package_test_labels(flight_cpp.generated_mesh geometry mesh)
flight_cpp_package_test_labels(flight_cpp.generated_skeleton3d skeleton3d)
flight_cpp_package_test_labels(flight_cpp.generated_interaction interaction)
flight_cpp_package_test_labels(flight_cpp.generated_scene2d_display_object scene2d)
flight_cpp_package_test_labels(flight_cpp.generated_effect_interpolation effects types)
flight_cpp_package_test_labels(flight_cpp.generated_effect_padding effects types)
flight_cpp_package_test_labels(flight_cpp.generated_shape_bounds shape)
flight_cpp_package_test_labels(flight_cpp.generated_intl_cache intl)
flight_cpp_package_test_labels(flight_cpp.generated_image_test_helper image)
flight_cpp_package_test_labels(flight_cpp.generated_image image)
flight_cpp_package_test_labels(flight_cpp.generated_image_codec image-codec)
flight_cpp_package_test_labels(flight_cpp.generated_statusbar statusbar)
flight_cpp_package_test_labels(flight_cpp.generated_audio audio)
flight_cpp_package_test_labels(flight_cpp.generated_input input)
flight_cpp_package_test_labels(flight_cpp.generated_app_render_view app)
flight_cpp_package_test_labels(flight_cpp.generated_mediasession mediasession)
flight_cpp_package_test_labels(flight_cpp.generated_midi midi)
flight_cpp_package_test_labels(flight_cpp.generated_dialog_modules dialog)
flight_cpp_package_test_labels(flight_cpp.generated_menu menu)
flight_cpp_package_test_labels(flight_cpp.generated_shortcut shortcut)
flight_cpp_package_test_labels(flight_cpp.generated_shortcut_accelerator shortcut)
flight_cpp_package_test_labels(flight_cpp.generated_shell shell)
flight_cpp_package_test_labels(flight_cpp.generated_ipc ipc)
flight_cpp_package_test_labels(flight_cpp.generated_protocol protocol)
flight_cpp_package_test_labels(flight_cpp.generated_permissions permissions)
flight_cpp_package_test_labels(flight_cpp.generated_filesystem filesystem)
flight_cpp_package_test_labels(flight_cpp.generated_preferences preferences)
flight_cpp_package_test_labels(flight_cpp.generated_quadbatch quadbatch signals)
flight_cpp_package_test_labels(flight_cpp.generated_clipboard clipboard signals)
flight_cpp_package_test_labels(flight_cpp.generated_power power)
flight_cpp_package_test_labels(flight_cpp.generated_video video)
flight_cpp_package_test_labels(flight_cpp.generated_sensors sensors signals)
flight_cpp_package_test_labels(flight_cpp.generated_geolocation geolocation)
flight_cpp_package_test_labels(flight_cpp.generated_haptics haptics)
flight_cpp_package_test_labels(flight_cpp.generated_device device)
flight_cpp_package_test_labels(flight_cpp.generated_notification notification)
flight_cpp_package_test_labels(flight_cpp.generated_connectivity connectivity)
flight_cpp_package_test_labels(flight_cpp.generated_share share)
flight_cpp_package_test_labels(flight_cpp.generated_updater updater)
flight_cpp_package_test_labels(flight_cpp.generated_lifecycle lifecycle signals)
flight_cpp_package_test_labels(flight_cpp.generated_lifecycle_transitions lifecycle signals)
flight_cpp_package_test_labels(flight_cpp.generated_accessibility accessibility)
flight_cpp_package_test_labels(flight_cpp.generated_layout layout)
flight_cpp_package_test_labels(flight_cpp.generated_platform platform)
flight_cpp_package_test_labels(flight_cpp.generated_spring spring)
flight_cpp_package_test_labels(flight_cpp.generated_particleemitter geometry math particleemitter particles scene2d scene3d)
flight_cpp_package_test_labels(flight_cpp.generated_particles_formats_particle_designer particles particles-formats)
flight_cpp_package_test_labels(flight_cpp.generated_particles_formats_libgdx particles particles-formats)
flight_cpp_package_test_labels(flight_cpp.generated_particles_formats particles particles-formats)
flight_cpp_package_test_labels(flight_cpp.generated_particles math particles)
flight_cpp_package_test_labels(flight_cpp.generated_log_core log signals)
flight_cpp_package_test_labels(flight_cpp.generated_statechart log signals statechart)
flight_cpp_package_test_labels(flight_cpp.generated_flow flow log)
flight_cpp_package_test_labels(flight_cpp.generated_easing easing log)
flight_cpp_package_test_labels(flight_cpp.generated_timeline signals timeline)
flight_cpp_package_test_labels(flight_cpp.generated_timeline_target scene2d signals timeline)
flight_cpp_package_test_labels(flight_cpp.generated_tween signals tween)
flight_cpp_package_test_labels(flight_cpp.generated_velocity geometry velocity)
flight_cpp_package_test_labels(flight_cpp.generated_motionpath geometry motionpath path)
flight_cpp_package_test_labels(flight_cpp.generated_path path)
flight_cpp_package_test_labels(flight_cpp.generated_path_formats path-formats)
flight_cpp_package_test_labels(flight_cpp.generated_scene_document scene-document)
flight_cpp_package_test_labels(flight_cpp.generated_scene2d_resources scene2d-resources)
flight_cpp_package_test_labels(flight_cpp.generated_scene2d_resource_loading scene2d-resources)
flight_cpp_package_test_labels(flight_cpp.generated_clock clock signals)
flight_cpp_package_test_labels(flight_cpp.generated_movieclip movieclip signals timeline)
flight_cpp_package_test_labels(flight_cpp.generated_swf_reader swf)
flight_cpp_package_test_labels(flight_cpp.generated_swf swf)
flight_cpp_package_test_labels(flight_cpp.generated_swf_video_texture swf)
flight_cpp_package_test_labels(flight_cpp.generated_physics2d physics2d)
flight_cpp_package_test_labels(flight_cpp.generated_spatial spatial)
flight_cpp_package_test_labels(flight_cpp.generated_spatial_notice spatial)
flight_cpp_package_test_labels(flight_cpp.generated_input_manager input)
flight_cpp_package_test_labels(flight_cpp.generated_input_state input)
flight_cpp_package_test_labels(flight_cpp.generated_input_binding input)
flight_cpp_package_test_labels(flight_cpp.generated_keyboard keyboard signals)
flight_cpp_package_test_labels(flight_cpp.generated_skeleton2d geometry skeleton2d)
flight_cpp_package_test_labels(flight_cpp.generated_spine_binary_reader skeleton2d-formats)
flight_cpp_package_test_labels(flight_cpp.generated_physics2d_abi collision physics2d-abi)
flight_cpp_package_test_labels(flight_cpp.generated_physics2d_queries physics2d)
flight_cpp_package_test_labels(flight_cpp.generated_physics2d_boundaries physics2d)
flight_cpp_package_test_labels(flight_cpp.generated_physics2d_step physics2d)
flight_cpp_package_test_labels(flight_cpp.generated_physics2d_world log physics2d)
flight_cpp_package_test_labels(flight_cpp.generated_physics2d_bodies physics2d scene2d)
flight_cpp_package_test_labels(flight_cpp.generated_skeleton2d_formats_registry skeleton2d-formats)
flight_cpp_package_test_labels(flight_cpp.generated_skeleton2d_formats_spine_binary skeleton2d-formats)
flight_cpp_package_test_labels(flight_cpp.generated_signals_throttle signals)
flight_cpp_package_test_labels(flight_cpp.generated_signals signals)
flight_cpp_package_test_labels(flight_cpp.generated_useragent useragent)
flight_cpp_package_test_labels(flight_cpp.generated_app_loop app signals)
flight_cpp_package_test_labels(flight_cpp.generated_app_window app signals)
flight_cpp_package_test_labels(flight_cpp.generated_app_core app signals)
flight_cpp_package_test_labels(flight_cpp.generated_host host)
flight_cpp_package_test_labels(flight_cpp.generated_dialog app dialog filesystem geometry node render signals)
flight_cpp_package_test_labels(flight_cpp.generated_textshaper_cache textshaper)

# Behavioural parity for @flighthq/textshaper: the capability queries, the shaped-run pool (whose
# acquire/release identity IS the contract), the cluster index map and the script/direction itemizer.
# The pool's release-guard seam is tested here; its log-emitting half is blocked by log's
# merge_span_fields boundary, recorded in-file rather than worked around.
add_executable(
  flight_cpp_generated_textshaper_parity_tests
  generated_textshaper_parity_test.cpp
)
target_include_directories(
  flight_cpp_generated_textshaper_parity_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_textshaper_parity_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_textshaper_parity
  COMMAND flight_cpp_generated_textshaper_parity_tests
)
flight_cpp_package_test_labels(flight_cpp.generated_textshaper_parity textshaper types)

# Behavioural parity for @flighthq/media, over the two upstream fixtures whose modules emit (the
# package is `partial`: audioMixer and videoChannel are refused and cannot be covered). Driven by a
# mock HostAudioDeviceCapability, the way the upstream fixtures are.
add_executable(
  flight_cpp_generated_media_parity_tests
  generated_media_parity_test.cpp
)
target_include_directories(
  flight_cpp_generated_media_parity_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_media_parity_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_media_parity
  COMMAND flight_cpp_generated_media_parity_tests
)
flight_cpp_package_test_labels(flight_cpp.generated_media_parity audio log media types)

# Behavioural parity for @flighthq/audio over the modules whose surface is reachable without a host
# (audioResource, audioDecodeSlot, audioFormat). Host seams are supplied by the fixture, never faked.
add_executable(
  flight_cpp_generated_audio_parity_tests
  generated_audio_parity_test.cpp
)
target_include_directories(
  flight_cpp_generated_audio_parity_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_audio_parity_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_audio_parity
  COMMAND flight_cpp_generated_audio_parity_tests
)
flight_cpp_package_test_labels(flight_cpp.generated_audio_parity audio types)

# Behavioural parity for @flighthq/screen, over the emitted code (no overrides exist for it). The host
# seam is a recording HostScreenQueryCapability supplied by the test, as upstream supplies it.
add_executable(
  flight_cpp_generated_screen_parity_tests
  generated_screen_parity_test.cpp
)
target_include_directories(
  flight_cpp_generated_screen_parity_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_screen_parity_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_screen_parity
  COMMAND flight_cpp_generated_screen_parity_tests
)
flight_cpp_package_test_labels(flight_cpp.generated_screen_parity screen types)
flight_cpp_package_test_labels(flight_cpp.generated_net net)
flight_cpp_package_test_labels(flight_cpp.generated_socket signals socket)
flight_cpp_package_test_labels(flight_cpp.generated_encoding encoding)
flight_cpp_package_test_labels(flight_cpp.generated_binpack binpack)
flight_cpp_package_test_labels(flight_cpp.generated_compression compression)
flight_cpp_package_test_labels(flight_cpp.generated_path_boolean path-boolean)
flight_cpp_package_test_labels(flight_cpp.generated_abc abc)
flight_cpp_package_test_labels(flight_cpp.generated_xml xml)
flight_cpp_package_test_labels(flight_cpp.generated_command command)
flight_cpp_package_test_labels(flight_cpp.generated_math math)
flight_cpp_package_test_labels(flight_cpp.generated_geometry_matrix34 geometry)
flight_cpp_package_test_labels(flight_cpp.generated_geometry_planar geometry)
flight_cpp_package_test_labels(flight_cpp.generated_geometry_vectors geometry)
flight_cpp_package_test_labels(flight_cpp.generated_geometry_volumes geometry log)
flight_cpp_package_test_labels(flight_cpp.generated_socket_statechart socket statechart)
flight_cpp_package_test_labels(flight_cpp.generated_assets assets)
flight_cpp_package_test_labels(flight_cpp.generated_importdiagnostics importdiagnostics)
flight_cpp_package_test_labels(flight_cpp.generated_loader loader)
flight_cpp_package_test_labels(flight_cpp.generated_surface surface)
flight_cpp_package_test_labels(flight_cpp.generated_shape shape)
flight_cpp_package_test_labels(flight_cpp.generated_effects_wgpu_gradient_ramp effects-wgpu)
flight_cpp_package_test_labels(flight_cpp.generated_color color)
flight_cpp_package_test_labels(flight_cpp.generated_adjustments adjustments)
flight_cpp_package_test_labels(flight_cpp.generated_capture capture)
flight_cpp_package_test_labels(flight_cpp.generated_snapshot snapshot log)
flight_cpp_package_test_labels(flight_cpp.generated_physics3d physics3d)
flight_cpp_package_test_labels(flight_cpp.generated_physics3d_abi physics3d-abi)
flight_cpp_package_test_labels(flight_cpp.generated_picking picking)
flight_cpp_package_test_labels(flight_cpp.generated_quadbatch quadbatch signals)
flight_cpp_package_test_labels(flight_cpp.upstream_math_geometry_parity geometry math)

# Behavioural parity for @flighthq/textbidi, driven by the upstream Vitest fixtures. It links the SDK
# preview because a BidiClassKernel is a generated entity and resolveBidiLevels is supplied by the
# textbidi override, so this asserts the hand-written resolver rather than the runtime alone.
add_executable(flight_cpp_generated_textbidi_parity_tests generated_textbidi_parity_test.cpp)
target_include_directories(
  flight_cpp_generated_textbidi_parity_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(
  flight_cpp_generated_textbidi_parity_tests
  PRIVATE Flight::Cpp Flight::SdkPreview
)
add_test(
  NAME flight_cpp.generated_textbidi_parity
  COMMAND flight_cpp_generated_textbidi_parity_tests
)
flight_cpp_package_test_labels(flight_cpp.generated_textbidi_parity log textbidi types)

add_executable(flight_cpp_c_api_tests c_api_test.c)
add_executable(flight_cpp_generated_textlayout_parity_tests generated_textlayout_parity_test.cpp)
target_include_directories(flight_cpp_generated_textlayout_parity_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include")
target_link_libraries(flight_cpp_generated_textlayout_parity_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_textlayout_parity COMMAND flight_cpp_generated_textlayout_parity_tests)
flight_cpp_package_test_labels(flight_cpp.generated_textlayout_parity textlayout types)

add_executable(flight_cpp_generated_textsegment_parity_tests generated_textsegment_parity_test.cpp)
target_include_directories(flight_cpp_generated_textsegment_parity_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include")
target_link_libraries(flight_cpp_generated_textsegment_parity_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_textsegment_parity COMMAND flight_cpp_generated_textsegment_parity_tests)
flight_cpp_package_test_labels(flight_cpp.generated_textsegment_parity log textsegment types)

target_link_libraries(flight_cpp_c_api_tests PRIVATE Flight::C)
flight_cpp_set_project_warnings(flight_cpp_c_api_tests)
add_test(NAME flight_cpp.c_api COMMAND flight_cpp_c_api_tests)

find_package(Threads REQUIRED)

add_executable(flight_cpp_runtime_conformance_tests runtime_conformance_test.cpp)
target_link_libraries(flight_cpp_runtime_conformance_tests PRIVATE Flight::Cpp Threads::Threads)
flight_cpp_set_project_warnings(flight_cpp_runtime_conformance_tests)
add_test(NAME flight_cpp.runtime_conformance COMMAND flight_cpp_runtime_conformance_tests)

add_executable(flight_cpp_c_api_thread_tests c_api_thread_test.cpp)
target_link_libraries(flight_cpp_c_api_thread_tests PRIVATE Flight::C Threads::Threads)
flight_cpp_set_project_warnings(flight_cpp_c_api_thread_tests)
add_test(NAME flight_cpp.c_api_threads COMMAND flight_cpp_c_api_thread_tests)

set(
  flight_cpp_public_headers
  abort
  any
  array
  array_buffer
  array_buffer_view
  attachment
  audio_buffer
  audio_context
  base64
  blob
  boolean
  callable
  canvas_2d
  conditional_facet_ref
  contract
  data_view
  date
  dom_exception
  equality
  erased_ref
  error
  executor
  font_face
  host
  image_data
  intl
  iterable
  iterator
  json
  map
  math
  number
  object
  optional_variant
  presence
  reference
  regexp
  record
  rejection
  set
  sequence_view
  string
  stream
  structural_ref
  structured_clone
  symbol
  runtime
  task
  template_argument
  text_decoder
  text_encoder
  typed_array
  uri
  url
  version
  weak_map
  weak_set
  web_types
)

foreach(header IN LISTS flight_cpp_public_headers)
  set(source "${CMAKE_CURRENT_BINARY_DIR}/header_${header}_test.cpp")
  file(GENERATE OUTPUT "${source}" CONTENT "#include <flight/${header}.hpp>\n")
  add_library("flight_cpp_header_${header}_test" OBJECT "${source}")
  target_link_libraries("flight_cpp_header_${header}_test" PRIVATE Flight::Cpp)
  flight_cpp_set_project_warnings("flight_cpp_header_${header}_test")
endforeach()

if(TARGET Flight::HostSdlWgpu)
  add_executable(flight_cpp_host_sdl_tests host_sdl_test.cpp)
  target_link_libraries(
    flight_cpp_host_sdl_tests
    PRIVATE
      Flight::HostSdlGl
      Flight::HostSdlSdkPreview
      Flight::HostSdlWgpu
  )
  flight_cpp_set_project_warnings(flight_cpp_host_sdl_tests)
  add_test(NAME flight_cpp.host_sdl COMMAND flight_cpp_host_sdl_tests)
  set_tests_properties(
    flight_cpp.host_sdl
    PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy"
  )
  flight_cpp_package_test_labels(flight_cpp.host_sdl types)
endif()

if(TARGET Flight::HostSdlGl)
  add_executable(flight_cpp_host_sdl_gl_app_tests host_sdl_gl_app_test.cpp)
  target_link_libraries(flight_cpp_host_sdl_gl_app_tests PRIVATE Flight::HostSdlGl)
  flight_cpp_set_project_warnings(flight_cpp_host_sdl_gl_app_tests)
  add_test(NAME flight_cpp.host_sdl_gl_app COMMAND flight_cpp_host_sdl_gl_app_tests)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_tests_properties(flight_cpp.host_sdl_gl_app PROPERTIES
      ENVIRONMENT "SDL_VIDEODRIVER=offscreen;LIBGL_ALWAYS_SOFTWARE=1")
  endif()

  set(source "${CMAKE_CURRENT_BINARY_DIR}/header_host_sdl_gl_app_test.cpp")
  file(GENERATE OUTPUT "${source}" CONTENT "#include <flight/host_sdl/gl_app.hpp>\n")
  add_library(flight_cpp_header_host_sdl_gl_app_test OBJECT "${source}")
  target_link_libraries(flight_cpp_header_host_sdl_gl_app_test PRIVATE Flight::HostSdlGl)
  flight_cpp_set_project_warnings(flight_cpp_header_host_sdl_gl_app_test)

  set(source "${CMAKE_CURRENT_BINARY_DIR}/header_host_sdl_image_test.cpp")
  file(GENERATE OUTPUT "${source}" CONTENT "#include <flight/host_sdl/image.hpp>\n")
  add_library(flight_cpp_header_host_sdl_image_test OBJECT "${source}")
  target_link_libraries(flight_cpp_header_host_sdl_image_test PRIVATE Flight::HostSdlImage)
  flight_cpp_set_project_warnings(flight_cpp_header_host_sdl_image_test)

  set(source "${CMAKE_CURRENT_BINARY_DIR}/header_host_sdl_webgl_test.cpp")
  file(GENERATE OUTPUT "${source}" CONTENT "#include <flight/host_sdl/webgl.hpp>\n")
  add_library(flight_cpp_header_host_sdl_webgl_test OBJECT "${source}")
  target_link_libraries(flight_cpp_header_host_sdl_webgl_test PRIVATE Flight::HostSdlGl)
  flight_cpp_set_project_warnings(flight_cpp_header_host_sdl_webgl_test)

  set(source "${CMAKE_CURRENT_BINARY_DIR}/header_host_sdl_web_platform_test.cpp")
  file(GENERATE OUTPUT "${source}" CONTENT "#include <flight/host_sdl/web_platform.hpp>\n")
  add_library(flight_cpp_header_host_sdl_web_platform_test OBJECT "${source}")
  target_link_libraries(flight_cpp_header_host_sdl_web_platform_test PRIVATE Flight::HostSdlGl)
  flight_cpp_set_project_warnings(flight_cpp_header_host_sdl_web_platform_test)
endif()

if(TARGET Flight::HostSdl)
  foreach(header IN ITEMS audio cursor input sdk_audio sdk_clipboard sdk_cursor sdk_device sdk_haptics sdk_keyboard sdk_platform sdk_screen sdk_window)
    set(source "${CMAKE_CURRENT_BINARY_DIR}/header_host_sdl_${header}_test.cpp")
    file(GENERATE OUTPUT "${source}" CONTENT "#include <flight/host_sdl/${header}.hpp>\n")
    add_library("flight_cpp_header_host_sdl_${header}_test" OBJECT "${source}")
    target_link_libraries("flight_cpp_header_host_sdl_${header}_test" PRIVATE Flight::HostSdl)
    flight_cpp_set_project_warnings("flight_cpp_header_host_sdl_${header}_test")
  endforeach()
endif()

flight_cpp_package_test_labels(flight_cpp.generated_video_texture_options texture types)

add_executable(flight_cpp_generated_video_texture_helpers_tests generated_video_texture_helpers_test.cpp)
target_link_libraries(flight_cpp_generated_video_texture_helpers_tests PRIVATE Flight::Cpp)
target_include_directories(flight_cpp_generated_video_texture_helpers_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include" "${PROJECT_SOURCE_DIR}/generated/include")
add_test(NAME flight_cpp.generated_video_texture_helpers COMMAND flight_cpp_generated_video_texture_helpers_tests)
flight_cpp_package_test_labels(flight_cpp.generated_video_texture_helpers texture types)

add_executable(flight_cpp_generated_video_texture_callable_proxies_tests generated_video_texture_callable_proxies_test.cpp)
target_link_libraries(flight_cpp_generated_video_texture_callable_proxies_tests PRIVATE Flight::Cpp)
target_include_directories(flight_cpp_generated_video_texture_callable_proxies_tests BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include" "${PROJECT_SOURCE_DIR}/generated/include")
add_test(NAME flight_cpp.generated_video_texture_callable_proxies COMMAND flight_cpp_generated_video_texture_callable_proxies_tests)
flight_cpp_package_test_labels(flight_cpp.generated_video_texture_callable_proxies texture)
