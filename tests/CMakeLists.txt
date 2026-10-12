add_executable(flight_cpp_runtime_tests runtime_test.cpp)
target_link_libraries(flight_cpp_runtime_tests PRIVATE Flight::Cpp)
flight_cpp_set_project_warnings(flight_cpp_runtime_tests)

add_test(NAME flight_cpp.runtime COMMAND flight_cpp_runtime_tests)

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

add_executable(flight_cpp_generated_collision_tests generated_collision_test.cpp)
target_include_directories(
  flight_cpp_generated_collision_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_collision_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_collision COMMAND flight_cpp_generated_collision_tests)
set_tests_properties(flight_cpp.generated_collision PROPERTIES SKIP_RETURN_CODE 77)

add_executable(flight_cpp_generated_registry_tests generated_registry_test.cpp)
target_include_directories(
  flight_cpp_generated_registry_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_registry_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_registry COMMAND flight_cpp_generated_registry_tests)

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

add_executable(flight_cpp_generated_interaction_tests generated_interaction_test.cpp)
target_include_directories(
  flight_cpp_generated_interaction_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_interaction_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_interaction COMMAND flight_cpp_generated_interaction_tests)

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

add_executable(flight_cpp_generated_lifecycle_tests generated_lifecycle_test.cpp)
target_include_directories(
  flight_cpp_generated_lifecycle_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_lifecycle_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_lifecycle COMMAND flight_cpp_generated_lifecycle_tests)

add_executable(flight_cpp_generated_useragent_tests generated_useragent_test.cpp)
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

add_executable(flight_cpp_generated_compression_tests generated_compression_test.cpp)
target_include_directories(
  flight_cpp_generated_compression_tests
  BEFORE PRIVATE "${PROJECT_SOURCE_DIR}/overrides/include"
)
target_link_libraries(flight_cpp_generated_compression_tests PRIVATE Flight::Cpp Flight::SdkPreview)
add_test(NAME flight_cpp.generated_compression COMMAND flight_cpp_generated_compression_tests)

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

# Per-package labels: `ctest -L '^pkg:<package>$'` runs exactly the semantic tests that include that
# SDK package's generated headers, and `npm run pkg:compile -- <package> --ctest` builds and runs them.
# A label is the unscoped npm name (`pkg:scene2d-canvas`, not the C++ directory), and every package a
# test includes directly is listed; scripts/packageScope.test.mjs checks this table against the
# includes so a new test or include cannot go unlabeled.
function(flight_cpp_package_test_labels test)
  list(TRANSFORM ARGN PREPEND "pkg:" OUTPUT_VARIABLE labels)
  set_tests_properties("${test}" PROPERTIES LABELS "${labels}")
endfunction()

flight_cpp_package_test_labels(flight_cpp.structural_row types)
flight_cpp_package_test_labels(flight_cpp.generated_collision collision)
flight_cpp_package_test_labels(flight_cpp.generated_registry registry)
flight_cpp_package_test_labels(flight_cpp.generated_animation animation)
flight_cpp_package_test_labels(flight_cpp.generated_log log)
flight_cpp_package_test_labels(flight_cpp.generated_log_levels_and_sinks log)
flight_cpp_package_test_labels(flight_cpp.generated_render_state types)
flight_cpp_package_test_labels(flight_cpp.generated_canvas_texture_resolver scene2d-canvas)
flight_cpp_package_test_labels(flight_cpp.generated_texture texture)
flight_cpp_package_test_labels(flight_cpp.generated_materials materials)
flight_cpp_package_test_labels(flight_cpp.generated_shading shading)
flight_cpp_package_test_labels(flight_cpp.generated_scene3d materials mesh node scene3d)
flight_cpp_package_test_labels(flight_cpp.generated_interaction interaction)
flight_cpp_package_test_labels(flight_cpp.generated_intl_cache intl)
flight_cpp_package_test_labels(flight_cpp.generated_image_test_helper image)
flight_cpp_package_test_labels(flight_cpp.generated_statusbar statusbar)
flight_cpp_package_test_labels(flight_cpp.generated_audio audio)
flight_cpp_package_test_labels(flight_cpp.generated_input input)
flight_cpp_package_test_labels(flight_cpp.generated_app_render_view app)
flight_cpp_package_test_labels(flight_cpp.generated_mediasession mediasession)
flight_cpp_package_test_labels(flight_cpp.generated_midi midi)
flight_cpp_package_test_labels(flight_cpp.generated_dialog_modules dialog)
flight_cpp_package_test_labels(flight_cpp.generated_menu menu)
flight_cpp_package_test_labels(flight_cpp.generated_shortcut shortcut)
flight_cpp_package_test_labels(flight_cpp.generated_shell shell)
flight_cpp_package_test_labels(flight_cpp.generated_ipc ipc)
flight_cpp_package_test_labels(flight_cpp.generated_protocol protocol)
flight_cpp_package_test_labels(flight_cpp.generated_permissions permissions)
flight_cpp_package_test_labels(flight_cpp.generated_filesystem filesystem)
flight_cpp_package_test_labels(flight_cpp.generated_preferences preferences)
flight_cpp_package_test_labels(flight_cpp.generated_power power)
flight_cpp_package_test_labels(flight_cpp.generated_video video)
flight_cpp_package_test_labels(flight_cpp.generated_connectivity connectivity)
flight_cpp_package_test_labels(flight_cpp.generated_share share)
flight_cpp_package_test_labels(flight_cpp.generated_updater updater)
flight_cpp_package_test_labels(flight_cpp.generated_lifecycle lifecycle signals)
flight_cpp_package_test_labels(flight_cpp.generated_spring spring)
flight_cpp_package_test_labels(flight_cpp.generated_timeline signals timeline)
flight_cpp_package_test_labels(flight_cpp.generated_velocity geometry velocity)
flight_cpp_package_test_labels(flight_cpp.generated_motionpath geometry motionpath path)
flight_cpp_package_test_labels(flight_cpp.generated_clock clock signals)
flight_cpp_package_test_labels(flight_cpp.generated_movieclip movieclip signals timeline)
flight_cpp_package_test_labels(flight_cpp.generated_signals_throttle signals)
flight_cpp_package_test_labels(flight_cpp.generated_useragent useragent)
flight_cpp_package_test_labels(flight_cpp.generated_dialog app dialog filesystem geometry node render signals)
flight_cpp_package_test_labels(flight_cpp.generated_textshaper_cache textshaper)
flight_cpp_package_test_labels(flight_cpp.generated_net net)
flight_cpp_package_test_labels(flight_cpp.generated_socket signals socket)
flight_cpp_package_test_labels(flight_cpp.generated_encoding encoding)
flight_cpp_package_test_labels(flight_cpp.generated_compression compression)
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
flight_cpp_package_test_labels(flight_cpp.generated_surface surface)
flight_cpp_package_test_labels(flight_cpp.upstream_math_geometry_parity geometry math)

add_executable(flight_cpp_c_api_tests c_api_test.c)
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
