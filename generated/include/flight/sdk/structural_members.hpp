// Generated from the structural keys used by the emitted Flight SDK. Do not edit.
#pragma once

#include <flight/structural_ref.hpp>

#include <concepts>
#include <cstddef>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace flight::detail {

template <typename Key, typename Object>
decltype(auto) generated_row_member(Object& object) {
  if constexpr (Key::name.view() == std::string_view("__brand") && requires { object.brand; }) return (object.brand);
  else if constexpr (Key::name.view() == std::string_view("a") && requires { object.a; }) return (object.a);
  else if constexpr (Key::name.view() == std::string_view("aberration") && requires { object.aberration; }) return (object.aberration);
  else if constexpr (Key::name.view() == std::string_view("absolute") && requires { object.absolute; }) return (object.absolute);
  else if constexpr (Key::name.view() == std::string_view("accuracy") && requires { object.accuracy; }) return (object.accuracy);
  else if constexpr (Key::name.view() == std::string_view("action") && requires { object.action; }) return (object.action);
  else if constexpr (Key::name.view() == std::string_view("adaptationSpeed") && requires { object.adaptation_speed; }) return (object.adaptation_speed);
  else if constexpr (Key::name.view() == std::string_view("additive") && requires { object.additive; }) return (object.additive);
  else if constexpr (Key::name.view() == std::string_view("addressed") && requires { object.addressed; }) return (object.addressed);
  else if constexpr (Key::name.view() == std::string_view("alpha") && requires { object.alpha; }) return (object.alpha);
  else if constexpr (Key::name.view() == std::string_view("alphaBias") && requires { object.alpha_bias; }) return (object.alpha_bias);
  else if constexpr (Key::name.view() == std::string_view("alphaCurve") && requires { object.alpha_curve; }) return (object.alpha_curve);
  else if constexpr (Key::name.view() == std::string_view("alphaEnd") && requires { object.alpha_end; }) return (object.alpha_end);
  else if constexpr (Key::name.view() == std::string_view("alphaScale") && requires { object.alpha_scale; }) return (object.alpha_scale);
  else if constexpr (Key::name.view() == std::string_view("alphaStart") && requires { object.alpha_start; }) return (object.alpha_start);
  else if constexpr (Key::name.view() == std::string_view("altitude") && requires { object.altitude; }) return (object.altitude);
  else if constexpr (Key::name.view() == std::string_view("altitudeAccuracy") && requires { object.altitude_accuracy; }) return (object.altitude_accuracy);
  else if constexpr (Key::name.view() == std::string_view("ambient") && requires { object.ambient; }) return (object.ambient);
  else if constexpr (Key::name.view() == std::string_view("amount") && requires { object.amount; }) return (object.amount);
  else if constexpr (Key::name.view() == std::string_view("angle") && requires { object.angle; }) return (object.angle);
  else if constexpr (Key::name.view() == std::string_view("angleVariance") && requires { object.angle_variance; }) return (object.angle_variance);
  else if constexpr (Key::name.view() == std::string_view("animation") && requires { object.animation; }) return (object.animation);
  else if constexpr (Key::name.view() == std::string_view("animations") && requires { object.animations; }) return (object.animations);
  else if constexpr (Key::name.view() == std::string_view("anisotropy") && requires { object.anisotropy; }) return (object.anisotropy);
  else if constexpr (Key::name.view() == std::string_view("anisotropyMap") && requires { object.anisotropy_map; }) return (object.anisotropy_map);
  else if constexpr (Key::name.view() == std::string_view("anisotropyMapUvSet") && requires { object.anisotropy_map_uv_set; }) return (object.anisotropy_map_uv_set);
  else if constexpr (Key::name.view() == std::string_view("anisotropyRotation") && requires { object.anisotropy_rotation; }) return (object.anisotropy_rotation);
  else if constexpr (Key::name.view() == std::string_view("anisotropyStrength") && requires { object.anisotropy_strength; }) return (object.anisotropy_strength);
  else if constexpr (Key::name.view() == std::string_view("announce") && requires { object.announce; }) return (object.announce);
  else if constexpr (Key::name.view() == std::string_view("applied") && requires { object.applied; }) return (object.applied);
  else if constexpr (Key::name.view() == std::string_view("arch") && requires { object.arch; }) return (object.arch);
  else if constexpr (Key::name.view() == std::string_view("ascent") && requires { object.ascent; }) return (object.ascent);
  else if constexpr (Key::name.view() == std::string_view("atlas") && requires { object.atlas; }) return (object.atlas);
  else if constexpr (Key::name.view() == std::string_view("attenuationColor") && requires { object.attenuation_color; }) return (object.attenuation_color);
  else if constexpr (Key::name.view() == std::string_view("attenuationDistance") && requires { object.attenuation_distance; }) return (object.attenuation_distance);
  else if constexpr (Key::name.view() == std::string_view("attributes") && requires { object.attributes; }) return (object.attributes);
  else if constexpr (Key::name.view() == std::string_view("availableMemory") && requires { object.available_memory; }) return (object.available_memory);
  else if constexpr (Key::name.view() == std::string_view("b") && requires { object.b; }) return (object.b);
  else if constexpr (Key::name.view() == std::string_view("beta") && requires { object.beta; }) return (object.beta);
  else if constexpr (Key::name.view() == std::string_view("bias") && requires { object.bias; }) return (object.bias);
  else if constexpr (Key::name.view() == std::string_view("bitmap") && requires { object.bitmap; }) return (object.bitmap);
  else if constexpr (Key::name.view() == std::string_view("blackTighten") && requires { object.black_tighten; }) return (object.black_tighten);
  else if constexpr (Key::name.view() == std::string_view("blendFuncDestination") && requires { object.blend_func_destination; }) return (object.blend_func_destination);
  else if constexpr (Key::name.view() == std::string_view("blendFuncSource") && requires { object.blend_func_source; }) return (object.blend_func_source);
  else if constexpr (Key::name.view() == std::string_view("blendMode") && requires { object.blend_mode; }) return (object.blend_mode);
  else if constexpr (Key::name.view() == std::string_view("blue") && requires { object.blue; }) return (object.blue);
  else if constexpr (Key::name.view() == std::string_view("blueBias") && requires { object.blue_bias; }) return (object.blue_bias);
  else if constexpr (Key::name.view() == std::string_view("blueScale") && requires { object.blue_scale; }) return (object.blue_scale);
  else if constexpr (Key::name.view() == std::string_view("blurX") && requires { object.blur_x; }) return (object.blur_x);
  else if constexpr (Key::name.view() == std::string_view("blurY") && requires { object.blur_y; }) return (object.blur_y);
  else if constexpr (Key::name.view() == std::string_view("boardName") && requires { object.board_name; }) return (object.board_name);
  else if constexpr (Key::name.view() == std::string_view("bodies") && requires { object.bodies; }) return (object.bodies);
  else if constexpr (Key::name.view() == std::string_view("bodyA") && requires { object.body_a; }) return (object.body_a);
  else if constexpr (Key::name.view() == std::string_view("bodyB") && requires { object.body_b; }) return (object.body_b);
  else if constexpr (Key::name.view() == std::string_view("bottom") && requires { object.bottom; }) return (object.bottom);
  else if constexpr (Key::name.view() == std::string_view("bounds") && requires { object.bounds; }) return (object.bounds);
  else if constexpr (Key::name.view() == std::string_view("breakForce") && requires { object.break_force; }) return (object.break_force);
  else if constexpr (Key::name.view() == std::string_view("breakTorque") && requires { object.break_torque; }) return (object.break_torque);
  else if constexpr (Key::name.view() == std::string_view("brightness") && requires { object.brightness; }) return (object.brightness);
  else if constexpr (Key::name.view() == std::string_view("burstCount") && requires { object.burst_count; }) return (object.burst_count);
  else if constexpr (Key::name.view() == std::string_view("burstInterval") && requires { object.burst_interval; }) return (object.burst_interval);
  else if constexpr (Key::name.view() == std::string_view("c") && requires { object.c; }) return (object.c);
  else if constexpr (Key::name.view() == std::string_view("cancel") && requires { object.cancel; }) return (object.cancel);
  else if constexpr (Key::name.view() == std::string_view("capabilities") && requires { object.capabilities; }) return (object.capabilities);
  else if constexpr (Key::name.view() == std::string_view("cascadeCount") && requires { object.cascade_count; }) return (object.cascade_count);
  else if constexpr (Key::name.view() == std::string_view("cascadeSplits") && requires { object.cascade_splits; }) return (object.cascade_splits);
  else if constexpr (Key::name.view() == std::string_view("castsShadow") && requires { object.casts_shadow; }) return (object.casts_shadow);
  else if constexpr (Key::name.view() == std::string_view("cellSize") && requires { object.cell_size; }) return (object.cell_size);
  else if constexpr (Key::name.view() == std::string_view("center") && requires { object.center; }) return (object.center);
  else if constexpr (Key::name.view() == std::string_view("centerX") && requires { object.center_x; }) return (object.center_x);
  else if constexpr (Key::name.view() == std::string_view("centerY") && requires { object.center_y; }) return (object.center_y);
  else if constexpr (Key::name.view() == std::string_view("centerZ") && requires { object.center_z; }) return (object.center_z);
  else if constexpr (Key::name.view() == std::string_view("child1") && requires { object.child1; }) return (object.child1);
  else if constexpr (Key::name.view() == std::string_view("child2") && requires { object.child2; }) return (object.child2);
  else if constexpr (Key::name.view() == std::string_view("children") && requires { object.children; }) return (object.children);
  else if constexpr (Key::name.view() == std::string_view("clear") && requires { object.clear; }) return (object.clear);
  else if constexpr (Key::name.view() == std::string_view("clearMetadata") && requires { object.clear_metadata; }) return (object.clear_metadata);
  else if constexpr (Key::name.view() == std::string_view("clearPositionState") && requires { object.clear_position_state; }) return (object.clear_position_state);
  else if constexpr (Key::name.view() == std::string_view("clearSpatialIndex") && requires { object.clear_spatial_index; }) return (object.clear_spatial_index);
  else if constexpr (Key::name.view() == std::string_view("clearWatch") && requires { object.clear_watch; }) return (object.clear_watch);
  else if constexpr (Key::name.view() == std::string_view("clearcoat") && requires { object.clearcoat; }) return (object.clearcoat);
  else if constexpr (Key::name.view() == std::string_view("clearcoatMap") && requires { object.clearcoat_map; }) return (object.clearcoat_map);
  else if constexpr (Key::name.view() == std::string_view("clearcoatMapUvSet") && requires { object.clearcoat_map_uv_set; }) return (object.clearcoat_map_uv_set);
  else if constexpr (Key::name.view() == std::string_view("clearcoatNormalMap") && requires { object.clearcoat_normal_map; }) return (object.clearcoat_normal_map);
  else if constexpr (Key::name.view() == std::string_view("clearcoatNormalMapUvSet") && requires { object.clearcoat_normal_map_uv_set; }) return (object.clearcoat_normal_map_uv_set);
  else if constexpr (Key::name.view() == std::string_view("clearcoatNormalScale") && requires { object.clearcoat_normal_scale; }) return (object.clearcoat_normal_scale);
  else if constexpr (Key::name.view() == std::string_view("clearcoatRoughness") && requires { object.clearcoat_roughness; }) return (object.clearcoat_roughness);
  else if constexpr (Key::name.view() == std::string_view("clearcoatRoughnessMap") && requires { object.clearcoat_roughness_map; }) return (object.clearcoat_roughness_map);
  else if constexpr (Key::name.view() == std::string_view("clearcoatRoughnessMapUvSet") && requires { object.clearcoat_roughness_map_uv_set; }) return (object.clearcoat_roughness_map_uv_set);
  else if constexpr (Key::name.view() == std::string_view("clip") && requires { object.clip; }) return (object.clip);
  else if constexpr (Key::name.view() == std::string_view("colliderA") && requires { object.collider_a; }) return (object.collider_a);
  else if constexpr (Key::name.view() == std::string_view("colliderB") && requires { object.collider_b; }) return (object.collider_b);
  else if constexpr (Key::name.view() == std::string_view("color") && requires { object.color; }) return (object.color);
  else if constexpr (Key::name.view() == std::string_view("colorCurve") && requires { object.color_curve; }) return (object.color_curve);
  else if constexpr (Key::name.view() == std::string_view("colorDepth") && requires { object.color_depth; }) return (object.color_depth);
  else if constexpr (Key::name.view() == std::string_view("colorEndB") && requires { object.color_end_b; }) return (object.color_end_b);
  else if constexpr (Key::name.view() == std::string_view("colorEndG") && requires { object.color_end_g; }) return (object.color_end_g);
  else if constexpr (Key::name.view() == std::string_view("colorEndR") && requires { object.color_end_r; }) return (object.color_end_r);
  else if constexpr (Key::name.view() == std::string_view("colorEndVarianceB") && requires { object.color_end_variance_b; }) return (object.color_end_variance_b);
  else if constexpr (Key::name.view() == std::string_view("colorEndVarianceG") && requires { object.color_end_variance_g; }) return (object.color_end_variance_g);
  else if constexpr (Key::name.view() == std::string_view("colorEndVarianceR") && requires { object.color_end_variance_r; }) return (object.color_end_variance_r);
  else if constexpr (Key::name.view() == std::string_view("colorGamut") && requires { object.color_gamut; }) return (object.color_gamut);
  else if constexpr (Key::name.view() == std::string_view("colorMatrix") && requires { object.color_matrix; }) return (object.color_matrix);
  else if constexpr (Key::name.view() == std::string_view("colorScaleBias") && requires { object.color_scale_bias; }) return (object.color_scale_bias);
  else if constexpr (Key::name.view() == std::string_view("colorStartB") && requires { object.color_start_b; }) return (object.color_start_b);
  else if constexpr (Key::name.view() == std::string_view("colorStartG") && requires { object.color_start_g; }) return (object.color_start_g);
  else if constexpr (Key::name.view() == std::string_view("colorStartR") && requires { object.color_start_r; }) return (object.color_start_r);
  else if constexpr (Key::name.view() == std::string_view("colorStartVarianceB") && requires { object.color_start_variance_b; }) return (object.color_start_variance_b);
  else if constexpr (Key::name.view() == std::string_view("colorStartVarianceG") && requires { object.color_start_variance_g; }) return (object.color_start_variance_g);
  else if constexpr (Key::name.view() == std::string_view("colorStartVarianceR") && requires { object.color_start_variance_r; }) return (object.color_start_variance_r);
  else if constexpr (Key::name.view() == std::string_view("commands") && requires { object.commands; }) return (object.commands);
  else if constexpr (Key::name.view() == std::string_view("componentX") && requires { object.component_x; }) return (object.component_x);
  else if constexpr (Key::name.view() == std::string_view("componentY") && requires { object.component_y; }) return (object.component_y);
  else if constexpr (Key::name.view() == std::string_view("compression") && requires { object.compression; }) return (object.compression);
  else if constexpr (Key::name.view() == std::string_view("connections") && requires { object.connections; }) return (object.connections);
  else if constexpr (Key::name.view() == std::string_view("contrast") && requires { object.contrast; }) return (object.contrast);
  else if constexpr (Key::name.view() == std::string_view("count") && requires { object.count; }) return (object.count);
  else if constexpr (Key::name.view() == std::string_view("cpuCores") && requires { object.cpu_cores; }) return (object.cpu_cores);
  else if constexpr (Key::name.view() == std::string_view("crop") && requires { object.crop; }) return (object.crop);
  else if constexpr (Key::name.view() == std::string_view("curvature") && requires { object.curvature; }) return (object.curvature);
  else if constexpr (Key::name.view() == std::string_view("d") && requires { object.d; }) return (object.d);
  else if constexpr (Key::name.view() == std::string_view("dampingRatio") && requires { object.damping_ratio; }) return (object.damping_ratio);
  else if constexpr (Key::name.view() == std::string_view("data") && requires { object.data; }) return (object.data);
  else if constexpr (Key::name.view() == std::string_view("deadzoneHalfHeight") && requires { object.deadzone_half_height; }) return (object.deadzone_half_height);
  else if constexpr (Key::name.view() == std::string_view("deadzoneHalfWidth") && requires { object.deadzone_half_width; }) return (object.deadzone_half_width);
  else if constexpr (Key::name.view() == std::string_view("decay") && requires { object.decay; }) return (object.decay);
  else if constexpr (Key::name.view() == std::string_view("declined") && requires { object.declined; }) return (object.declined);
  else if constexpr (Key::name.view() == std::string_view("defaultEase") && requires { object.default_ease; }) return (object.default_ease);
  else if constexpr (Key::name.view() == std::string_view("delay") && requires { object.delay; }) return (object.delay);
  else if constexpr (Key::name.view() == std::string_view("deltaTime") && requires { object.delta_time; }) return (object.delta_time);
  else if constexpr (Key::name.view() == std::string_view("density") && requires { object.density; }) return (object.density);
  else if constexpr (Key::name.view() == std::string_view("densityDpi") && requires { object.density_dpi; }) return (object.density_dpi);
  else if constexpr (Key::name.view() == std::string_view("depth") && requires { object.depth; }) return (object.depth);
  else if constexpr (Key::name.view() == std::string_view("descent") && requires { object.descent; }) return (object.descent);
  else if constexpr (Key::name.view() == std::string_view("destroy") && requires { object.destroy; }) return (object.destroy);
  else if constexpr (Key::name.view() == std::string_view("devicePixelRatio") && requires { object.device_pixel_ratio; }) return (object.device_pixel_ratio);
  else if constexpr (Key::name.view() == std::string_view("direction") && requires { object.direction; }) return (object.direction);
  else if constexpr (Key::name.view() == std::string_view("directionX") && requires { object.direction_x; }) return (object.direction_x);
  else if constexpr (Key::name.view() == std::string_view("directionY") && requires { object.direction_y; }) return (object.direction_y);
  else if constexpr (Key::name.view() == std::string_view("directionZ") && requires { object.direction_z; }) return (object.direction_z);
  else if constexpr (Key::name.view() == std::string_view("directional") && requires { object.directional; }) return (object.directional);
  else if constexpr (Key::name.view() == std::string_view("distance") && requires { object.distance; }) return (object.distance);
  else if constexpr (Key::name.view() == std::string_view("distro") && requires { object.distro; }) return (object.distro);
  else if constexpr (Key::name.view() == std::string_view("distroVersion") && requires { object.distro_version; }) return (object.distro_version);
  else if constexpr (Key::name.view() == std::string_view("divisor") && requires { object.divisor; }) return (object.divisor);
  else if constexpr (Key::name.view() == std::string_view("duration") && requires { object.duration; }) return (object.duration);
  else if constexpr (Key::name.view() == std::string_view("ease") && requires { object.ease; }) return (object.ease);
  else if constexpr (Key::name.view() == std::string_view("edge") && requires { object.edge; }) return (object.edge);
  else if constexpr (Key::name.view() == std::string_view("edgeMode") && requires { object.edge_mode; }) return (object.edge_mode);
  else if constexpr (Key::name.view() == std::string_view("edgeThreshold") && requires { object.edge_threshold; }) return (object.edge_threshold);
  else if constexpr (Key::name.view() == std::string_view("elapsed") && requires { object.elapsed; }) return (object.elapsed);
  else if constexpr (Key::name.view() == std::string_view("emission") && requires { object.emission; }) return (object.emission);
  else if constexpr (Key::name.view() == std::string_view("emit") && requires { object.emit; }) return (object.emit);
  else if constexpr (Key::name.view() == std::string_view("emitterConeAngle") && requires { object.emitter_cone_angle; }) return (object.emitter_cone_angle);
  else if constexpr (Key::name.view() == std::string_view("emitterDepth") && requires { object.emitter_depth; }) return (object.emitter_depth);
  else if constexpr (Key::name.view() == std::string_view("emitterHeight") && requires { object.emitter_height; }) return (object.emitter_height);
  else if constexpr (Key::name.view() == std::string_view("emitterRadius") && requires { object.emitter_radius; }) return (object.emitter_radius);
  else if constexpr (Key::name.view() == std::string_view("emitterShape") && requires { object.emitter_shape; }) return (object.emitter_shape);
  else if constexpr (Key::name.view() == std::string_view("emitterType") && requires { object.emitter_type; }) return (object.emitter_type);
  else if constexpr (Key::name.view() == std::string_view("emitterWidth") && requires { object.emitter_width; }) return (object.emitter_width);
  else if constexpr (Key::name.view() == std::string_view("enabled") && requires { object.enabled; }) return (object.enabled);
  else if constexpr (Key::name.view() == std::string_view("encoding") && requires { object.encoding; }) return (object.encoding);
  else if constexpr (Key::name.view() == std::string_view("end") && requires { object.end; }) return (object.end);
  else if constexpr (Key::name.view() == std::string_view("endIndex") && requires { object.end_index; }) return (object.end_index);
  else if constexpr (Key::name.view() == std::string_view("endX") && requires { object.end_x; }) return (object.end_x);
  else if constexpr (Key::name.view() == std::string_view("endY") && requires { object.end_y; }) return (object.end_y);
  else if constexpr (Key::name.view() == std::string_view("endZ") && requires { object.end_z; }) return (object.end_z);
  else if constexpr (Key::name.view() == std::string_view("endianness") && requires { object.endianness; }) return (object.endianness);
  else if constexpr (Key::name.view() == std::string_view("engine") && requires { object.engine; }) return (object.engine);
  else if constexpr (Key::name.view() == std::string_view("engineVersion") && requires { object.engine_version; }) return (object.engine_version);
  else if constexpr (Key::name.view() == std::string_view("explainSpatialIndexing") && requires { object.explain_spatial_indexing; }) return (object.explain_spatial_indexing);
  else if constexpr (Key::name.view() == std::string_view("exposure") && requires { object.exposure; }) return (object.exposure);
  else if constexpr (Key::name.view() == std::string_view("exposureCompensation") && requires { object.exposure_compensation; }) return (object.exposure_compensation);
  else if constexpr (Key::name.view() == std::string_view("far") && requires { object.far; }) return (object.far);
  else if constexpr (Key::name.view() == std::string_view("featureId") && requires { object.feature_id; }) return (object.feature_id);
  else if constexpr (Key::name.view() == std::string_view("feedback") && requires { object.feedback; }) return (object.feedback);
  else if constexpr (Key::name.view() == std::string_view("fillBounds") && requires { object.fill_bounds; }) return (object.fill_bounds);
  else if constexpr (Key::name.view() == std::string_view("fillColor") && requires { object.fill_color; }) return (object.fill_color);
  else if constexpr (Key::name.view() == std::string_view("finishColor") && requires { object.finish_color; }) return (object.finish_color);
  else if constexpr (Key::name.view() == std::string_view("finishColorVariance") && requires { object.finish_color_variance; }) return (object.finish_color_variance);
  else if constexpr (Key::name.view() == std::string_view("finishParticleSize") && requires { object.finish_particle_size; }) return (object.finish_particle_size);
  else if constexpr (Key::name.view() == std::string_view("finishParticleSizeVariance") && requires { object.finish_particle_size_variance; }) return (object.finish_particle_size_variance);
  else if constexpr (Key::name.view() == std::string_view("floorLevel") && requires { object.floor_level; }) return (object.floor_level);
  else if constexpr (Key::name.view() == std::string_view("fontScale") && requires { object.font_scale; }) return (object.font_scale);
  else if constexpr (Key::name.view() == std::string_view("forceX") && requires { object.force_x; }) return (object.force_x);
  else if constexpr (Key::name.view() == std::string_view("forceY") && requires { object.force_y; }) return (object.force_y);
  else if constexpr (Key::name.view() == std::string_view("forceZ") && requires { object.force_z; }) return (object.force_z);
  else if constexpr (Key::name.view() == std::string_view("formFactor") && requires { object.form_factor; }) return (object.form_factor);
  else if constexpr (Key::name.view() == std::string_view("format") && requires { object.format; }) return (object.format);
  else if constexpr (Key::name.view() == std::string_view("fovY") && requires { object.fov_y; }) return (object.fov_y);
  else if constexpr (Key::name.view() == std::string_view("fraction") && requires { object.fraction; }) return (object.fraction);
  else if constexpr (Key::name.view() == std::string_view("frameCount") && requires { object.frame_count; }) return (object.frame_count);
  else if constexpr (Key::name.view() == std::string_view("frameDuration") && requires { object.frame_duration; }) return (object.frame_duration);
  else if constexpr (Key::name.view() == std::string_view("frameDurations") && requires { object.frame_durations; }) return (object.frame_durations);
  else if constexpr (Key::name.view() == std::string_view("frameId") && requires { object.frame_id; }) return (object.frame_id);
  else if constexpr (Key::name.view() == std::string_view("frameNames") && requires { object.frame_names; }) return (object.frame_names);
  else if constexpr (Key::name.view() == std::string_view("frameRate") && requires { object.frame_rate; }) return (object.frame_rate);
  else if constexpr (Key::name.view() == std::string_view("frames") && requires { object.frames; }) return (object.frames);
  else if constexpr (Key::name.view() == std::string_view("frequency") && requires { object.frequency; }) return (object.frequency);
  else if constexpr (Key::name.view() == std::string_view("friction") && requires { object.friction; }) return (object.friction);
  else if constexpr (Key::name.view() == std::string_view("gain") && requires { object.gain; }) return (object.gain);
  else if constexpr (Key::name.view() == std::string_view("gamma") && requires { object.gamma; }) return (object.gamma);
  else if constexpr (Key::name.view() == std::string_view("gateWeave") && requires { object.gate_weave; }) return (object.gate_weave);
  else if constexpr (Key::name.view() == std::string_view("getCapabilities") && requires { object.get_capabilities; }) return (object.get_capabilities);
  else if constexpr (Key::name.view() == std::string_view("getCurrentPosition") && requires { object.get_current_position; }) return (object.get_current_position);
  else if constexpr (Key::name.view() == std::string_view("getCurrentPositionResult") && requires { object.get_current_position_result; }) return (object.get_current_position_result);
  else if constexpr (Key::name.view() == std::string_view("getDisplayMetrics") && requires { object.get_display_metrics; }) return (object.get_display_metrics);
  else if constexpr (Key::name.view() == std::string_view("getFormats") && requires { object.get_formats; }) return (object.get_formats);
  else if constexpr (Key::name.view() == std::string_view("getGlyphAtlasImage") && requires { object.get_glyph_atlas_image; }) return (object.get_glyph_atlas_image);
  else if constexpr (Key::name.view() == std::string_view("getGlyphEntry") && requires { object.get_glyph_entry; }) return (object.get_glyph_entry);
  else if constexpr (Key::name.view() == std::string_view("getGlyphKerning") && requires { object.get_glyph_kerning; }) return (object.get_glyph_kerning);
  else if constexpr (Key::name.view() == std::string_view("getGlyphLayoutVersion") && requires { object.get_glyph_layout_version; }) return (object.get_glyph_layout_version);
  else if constexpr (Key::name.view() == std::string_view("getGlyphMetrics") && requires { object.get_glyph_metrics; }) return (object.get_glyph_metrics);
  else if constexpr (Key::name.view() == std::string_view("getId") && requires { object.get_id; }) return (object.get_id);
  else if constexpr (Key::name.view() == std::string_view("getInfo") && requires { object.get_info; }) return (object.get_info);
  else if constexpr (Key::name.view() == std::string_view("getPermission") && requires { object.get_permission; }) return (object.get_permission);
  else if constexpr (Key::name.view() == std::string_view("getPermissionState") && requires { object.get_permission_state; }) return (object.get_permission_state);
  else if constexpr (Key::name.view() == std::string_view("getPersistence") && requires { object.get_persistence; }) return (object.get_persistence);
  else if constexpr (Key::name.view() == std::string_view("getSafeAreaInsets") && requires { object.get_safe_area_insets; }) return (object.get_safe_area_insets);
  else if constexpr (Key::name.view() == std::string_view("ghosts") && requires { object.ghosts; }) return (object.ghosts);
  else if constexpr (Key::name.view() == std::string_view("glyphs") && requires { object.glyphs; }) return (object.glyphs);
  else if constexpr (Key::name.view() == std::string_view("gpuRenderer") && requires { object.gpu_renderer; }) return (object.gpu_renderer);
  else if constexpr (Key::name.view() == std::string_view("gpuVendor") && requires { object.gpu_vendor; }) return (object.gpu_vendor);
  else if constexpr (Key::name.view() == std::string_view("grainIntensity") && requires { object.grain_intensity; }) return (object.grain_intensity);
  else if constexpr (Key::name.view() == std::string_view("gravity") && requires { object.gravity; }) return (object.gravity);
  else if constexpr (Key::name.view() == std::string_view("gravityX") && requires { object.gravity_x; }) return (object.gravity_x);
  else if constexpr (Key::name.view() == std::string_view("gravityY") && requires { object.gravity_y; }) return (object.gravity_y);
  else if constexpr (Key::name.view() == std::string_view("gravityZ") && requires { object.gravity_z; }) return (object.gravity_z);
  else if constexpr (Key::name.view() == std::string_view("gravityx") && requires { object.gravityx; }) return (object.gravityx);
  else if constexpr (Key::name.view() == std::string_view("gravityy") && requires { object.gravityy; }) return (object.gravityy);
  else if constexpr (Key::name.view() == std::string_view("green") && requires { object.green; }) return (object.green);
  else if constexpr (Key::name.view() == std::string_view("greenBias") && requires { object.green_bias; }) return (object.green_bias);
  else if constexpr (Key::name.view() == std::string_view("greenScale") && requires { object.green_scale; }) return (object.green_scale);
  else if constexpr (Key::name.view() == std::string_view("groundColor") && requires { object.ground_color; }) return (object.ground_color);
  else if constexpr (Key::name.view() == std::string_view("halationRadius") && requires { object.halation_radius; }) return (object.halation_radius);
  else if constexpr (Key::name.view() == std::string_view("halationStrength") && requires { object.halation_strength; }) return (object.halation_strength);
  else if constexpr (Key::name.view() == std::string_view("halfExtentX") && requires { object.half_extent_x; }) return (object.half_extent_x);
  else if constexpr (Key::name.view() == std::string_view("halfExtentY") && requires { object.half_extent_y; }) return (object.half_extent_y);
  else if constexpr (Key::name.view() == std::string_view("halfExtentZ") && requires { object.half_extent_z; }) return (object.half_extent_z);
  else if constexpr (Key::name.view() == std::string_view("halfH") && requires { object.half_h; }) return (object.half_h);
  else if constexpr (Key::name.view() == std::string_view("halfW") && requires { object.half_w; }) return (object.half_w);
  else if constexpr (Key::name.view() == std::string_view("halo") && requires { object.halo; }) return (object.halo);
  else if constexpr (Key::name.view() == std::string_view("handle") && requires { object.handle; }) return (object.handle);
  else if constexpr (Key::name.view() == std::string_view("hasFormat") && requires { object.has_format; }) return (object.has_format);
  else if constexpr (Key::name.view() == std::string_view("hasImage") && requires { object.has_image; }) return (object.has_image);
  else if constexpr (Key::name.view() == std::string_view("hasKeyboard") && requires { object.has_keyboard; }) return (object.has_keyboard);
  else if constexpr (Key::name.view() == std::string_view("hasMouse") && requires { object.has_mouse; }) return (object.has_mouse);
  else if constexpr (Key::name.view() == std::string_view("hasStylus") && requires { object.has_stylus; }) return (object.has_stylus);
  else if constexpr (Key::name.view() == std::string_view("hasText") && requires { object.has_text; }) return (object.has_text);
  else if constexpr (Key::name.view() == std::string_view("heading") && requires { object.heading; }) return (object.heading);
  else if constexpr (Key::name.view() == std::string_view("height") && requires { object.height; }) return (object.height);
  else if constexpr (Key::name.view() == std::string_view("hemisphere") && requires { object.hemisphere; }) return (object.hemisphere);
  else if constexpr (Key::name.view() == std::string_view("hide") && requires { object.hide; }) return (object.hide);
  else if constexpr (Key::name.view() == std::string_view("highMax") && requires { object.high_max; }) return (object.high_max);
  else if constexpr (Key::name.view() == std::string_view("highMin") && requires { object.high_min; }) return (object.high_min);
  else if constexpr (Key::name.view() == std::string_view("hue") && requires { object.hue; }) return (object.hue);
  else if constexpr (Key::name.view() == std::string_view("id") && requires { object.id; }) return (object.id);
  else if constexpr (Key::name.view() == std::string_view("illuminance") && requires { object.illuminance; }) return (object.illuminance);
  else if constexpr (Key::name.view() == std::string_view("imageCount") && requires { object.image_count; }) return (object.image_count);
  else if constexpr (Key::name.view() == std::string_view("imageFile") && requires { object.image_file; }) return (object.image_file);
  else if constexpr (Key::name.view() == std::string_view("imageHeight") && requires { object.image_height; }) return (object.image_height);
  else if constexpr (Key::name.view() == std::string_view("imagePath") && requires { object.image_path; }) return (object.image_path);
  else if constexpr (Key::name.view() == std::string_view("imageWidth") && requires { object.image_width; }) return (object.image_width);
  else if constexpr (Key::name.view() == std::string_view("impact") && requires { object.impact; }) return (object.impact);
  else if constexpr (Key::name.view() == std::string_view("influenceCounts") && requires { object.influence_counts; }) return (object.influence_counts);
  else if constexpr (Key::name.view() == std::string_view("influences") && requires { object.influences; }) return (object.influences);
  else if constexpr (Key::name.view() == std::string_view("innerConeCos") && requires { object.inner_cone_cos; }) return (object.inner_cone_cos);
  else if constexpr (Key::name.view() == std::string_view("innerConeDegrees") && requires { object.inner_cone_degrees; }) return (object.inner_cone_degrees);
  else if constexpr (Key::name.view() == std::string_view("insertSpatialObject") && requires { object.insert_spatial_object; }) return (object.insert_spatial_object);
  else if constexpr (Key::name.view() == std::string_view("intensity") && requires { object.intensity; }) return (object.intensity);
  else if constexpr (Key::name.view() == std::string_view("intensityUnit") && requires { object.intensity_unit; }) return (object.intensity_unit);
  else if constexpr (Key::name.view() == std::string_view("interval") && requires { object.interval; }) return (object.interval);
  else if constexpr (Key::name.view() == std::string_view("invoke") && requires { object.invoke; }) return (object.invoke);
  else if constexpr (Key::name.view() == std::string_view("ior") && requires { object.ior; }) return (object.ior);
  else if constexpr (Key::name.view() == std::string_view("iridescence") && requires { object.iridescence; }) return (object.iridescence);
  else if constexpr (Key::name.view() == std::string_view("iridescenceIor") && requires { object.iridescence_ior; }) return (object.iridescence_ior);
  else if constexpr (Key::name.view() == std::string_view("iridescenceMap") && requires { object.iridescence_map; }) return (object.iridescence_map);
  else if constexpr (Key::name.view() == std::string_view("iridescenceMapUvSet") && requires { object.iridescence_map_uv_set; }) return (object.iridescence_map_uv_set);
  else if constexpr (Key::name.view() == std::string_view("iridescenceThicknessMap") && requires { object.iridescence_thickness_map; }) return (object.iridescence_thickness_map);
  else if constexpr (Key::name.view() == std::string_view("iridescenceThicknessMapUvSet") && requires { object.iridescence_thickness_map_uv_set; }) return (object.iridescence_thickness_map_uv_set);
  else if constexpr (Key::name.view() == std::string_view("iridescenceThicknessMax") && requires { object.iridescence_thickness_max; }) return (object.iridescence_thickness_max);
  else if constexpr (Key::name.view() == std::string_view("iridescenceThicknessMin") && requires { object.iridescence_thickness_min; }) return (object.iridescence_thickness_min);
  else if constexpr (Key::name.view() == std::string_view("isAmbientLightSupported") && requires { object.is_ambient_light_supported; }) return (object.is_ambient_light_supported);
  else if constexpr (Key::name.view() == std::string_view("isAvailable") && requires { object.is_available; }) return (object.is_available);
  else if constexpr (Key::name.view() == std::string_view("isBarometerSupported") && requires { object.is_barometer_supported; }) return (object.is_barometer_supported);
  else if constexpr (Key::name.view() == std::string_view("isGravitySupported") && requires { object.is_gravity_supported; }) return (object.is_gravity_supported);
  else if constexpr (Key::name.view() == std::string_view("isGyroscopeSupported") && requires { object.is_gyroscope_supported; }) return (object.is_gyroscope_supported);
  else if constexpr (Key::name.view() == std::string_view("isHdr") && requires { object.is_hdr; }) return (object.is_hdr);
  else if constexpr (Key::name.view() == std::string_view("isJailbroken") && requires { object.is_jailbroken; }) return (object.is_jailbroken);
  else if constexpr (Key::name.view() == std::string_view("isLinearAccelerationSupported") && requires { object.is_linear_acceleration_supported; }) return (object.is_linear_acceleration_supported);
  else if constexpr (Key::name.view() == std::string_view("isLowEndDevice") && requires { object.is_low_end_device; }) return (object.is_low_end_device);
  else if constexpr (Key::name.view() == std::string_view("isMagnetometerSupported") && requires { object.is_magnetometer_supported; }) return (object.is_magnetometer_supported);
  else if constexpr (Key::name.view() == std::string_view("isMotionSupported") && requires { object.is_motion_supported; }) return (object.is_motion_supported);
  else if constexpr (Key::name.view() == std::string_view("isOrientationSupported") && requires { object.is_orientation_supported; }) return (object.is_orientation_supported);
  else if constexpr (Key::name.view() == std::string_view("isProximitySupported") && requires { object.is_proximity_supported; }) return (object.is_proximity_supported);
  else if constexpr (Key::name.view() == std::string_view("isRooted") && requires { object.is_rooted; }) return (object.is_rooted);
  else if constexpr (Key::name.view() == std::string_view("isSupported") && requires { object.is_supported; }) return (object.is_supported);
  else if constexpr (Key::name.view() == std::string_view("isTouch") && requires { object.is_touch; }) return (object.is_touch);
  else if constexpr (Key::name.view() == std::string_view("isVirtual") && requires { object.is_virtual; }) return (object.is_virtual);
  else if constexpr (Key::name.view() == std::string_view("jointCollisionSuppressions") && requires { object.joint_collision_suppressions; }) return (object.joint_collision_suppressions);
  else if constexpr (Key::name.view() == std::string_view("jointSolvers") && requires { object.joint_solvers; }) return (object.joint_solvers);
  else if constexpr (Key::name.view() == std::string_view("kerning") && requires { object.kerning; }) return (object.kerning);
  else if constexpr (Key::name.view() == std::string_view("key") && requires { object.key; }) return (object.key);
  else if constexpr (Key::name.view() == std::string_view("kind") && requires { object.kind; }) return (object.kind);
  else if constexpr (Key::name.view() == std::string_view("latitude") && requires { object.latitude; }) return (object.latitude);
  else if constexpr (Key::name.view() == std::string_view("layerMask") && requires { object.layer_mask; }) return (object.layer_mask);
  else if constexpr (Key::name.view() == std::string_view("leading") && requires { object.leading; }) return (object.leading);
  else if constexpr (Key::name.view() == std::string_view("leafByObject") && requires { object.leaf_by_object; }) return (object.leaf_by_object);
  else if constexpr (Key::name.view() == std::string_view("left") && requires { object.left; }) return (object.left);
  else if constexpr (Key::name.view() == std::string_view("levels") && requires { object.levels; }) return (object.levels);
  else if constexpr (Key::name.view() == std::string_view("life") && requires { object.life; }) return (object.life);
  else if constexpr (Key::name.view() == std::string_view("lifeOffset") && requires { object.life_offset; }) return (object.life_offset);
  else if constexpr (Key::name.view() == std::string_view("lifetimeMax") && requires { object.lifetime_max; }) return (object.lifetime_max);
  else if constexpr (Key::name.view() == std::string_view("lifetimeMin") && requires { object.lifetime_min; }) return (object.lifetime_min);
  else if constexpr (Key::name.view() == std::string_view("lift") && requires { object.lift; }) return (object.lift);
  else if constexpr (Key::name.view() == std::string_view("lightColor") && requires { object.light_color; }) return (object.light_color);
  else if constexpr (Key::name.view() == std::string_view("lightX") && requires { object.light_x; }) return (object.light_x);
  else if constexpr (Key::name.view() == std::string_view("lightY") && requires { object.light_y; }) return (object.light_y);
  else if constexpr (Key::name.view() == std::string_view("lightness") && requires { object.lightness; }) return (object.lightness);
  else if constexpr (Key::name.view() == std::string_view("lineIndex") && requires { object.line_index; }) return (object.line_index);
  else if constexpr (Key::name.view() == std::string_view("linearLength") && requires { object.linear_length; }) return (object.linear_length);
  else if constexpr (Key::name.view() == std::string_view("linearStart") && requires { object.linear_start; }) return (object.linear_start);
  else if constexpr (Key::name.view() == std::string_view("locale") && requires { object.locale; }) return (object.locale);
  else if constexpr (Key::name.view() == std::string_view("logicalHeight") && requires { object.logical_height; }) return (object.logical_height);
  else if constexpr (Key::name.view() == std::string_view("logicalWidth") && requires { object.logical_width; }) return (object.logical_width);
  else if constexpr (Key::name.view() == std::string_view("longitude") && requires { object.longitude; }) return (object.longitude);
  else if constexpr (Key::name.view() == std::string_view("loop") && requires { object.loop; }) return (object.loop);
  else if constexpr (Key::name.view() == std::string_view("lowMax") && requires { object.low_max; }) return (object.low_max);
  else if constexpr (Key::name.view() == std::string_view("lowMin") && requires { object.low_min; }) return (object.low_min);
  else if constexpr (Key::name.view() == std::string_view("lut") && requires { object.lut; }) return (object.lut);
  else if constexpr (Key::name.view() == std::string_view("m") && requires { object.m; }) return (object.m);
  else if constexpr (Key::name.view() == std::string_view("magFilter") && requires { object.mag_filter; }) return (object.mag_filter);
  else if constexpr (Key::name.view() == std::string_view("manufacturer") && requires { object.manufacturer; }) return (object.manufacturer);
  else if constexpr (Key::name.view() == std::string_view("map") && requires { object.map; }) return (object.map);
  else if constexpr (Key::name.view() == std::string_view("margin") && requires { object.margin; }) return (object.margin);
  else if constexpr (Key::name.view() == std::string_view("marketingName") && requires { object.marketing_name; }) return (object.marketing_name);
  else if constexpr (Key::name.view() == std::string_view("material") && requires { object.material; }) return (object.material);
  else if constexpr (Key::name.view() == std::string_view("materialData") && requires { object.material_data; }) return (object.material_data);
  else if constexpr (Key::name.view() == std::string_view("matrix") && requires { object.matrix; }) return (object.matrix);
  else if constexpr (Key::name.view() == std::string_view("matrixX") && requires { object.matrix_x; }) return (object.matrix_x);
  else if constexpr (Key::name.view() == std::string_view("matrixY") && requires { object.matrix_y; }) return (object.matrix_y);
  else if constexpr (Key::name.view() == std::string_view("max") && requires { object.max; }) return (object.max);
  else if constexpr (Key::name.view() == std::string_view("maxBrightness") && requires { object.max_brightness; }) return (object.max_brightness);
  else if constexpr (Key::name.view() == std::string_view("maxDistance") && requires { object.max_distance; }) return (object.max_distance);
  else if constexpr (Key::name.view() == std::string_view("maxEv") && requires { object.max_ev; }) return (object.max_ev);
  else if constexpr (Key::name.view() == std::string_view("maxExposure") && requires { object.max_exposure; }) return (object.max_exposure);
  else if constexpr (Key::name.view() == std::string_view("maxParticleCount") && requires { object.max_particle_count; }) return (object.max_particle_count);
  else if constexpr (Key::name.view() == std::string_view("maxParticles") && requires { object.max_particles; }) return (object.max_particles);
  else if constexpr (Key::name.view() == std::string_view("maxRadius") && requires { object.max_radius; }) return (object.max_radius);
  else if constexpr (Key::name.view() == std::string_view("maxRadiusVariance") && requires { object.max_radius_variance; }) return (object.max_radius_variance);
  else if constexpr (Key::name.view() == std::string_view("maxX") && requires { object.max_x; }) return (object.max_x);
  else if constexpr (Key::name.view() == std::string_view("maxY") && requires { object.max_y; }) return (object.max_y);
  else if constexpr (Key::name.view() == std::string_view("maxZ") && requires { object.max_z; }) return (object.max_z);
  else if constexpr (Key::name.view() == std::string_view("metadata") && requires { object.metadata; }) return (object.metadata);
  else if constexpr (Key::name.view() == std::string_view("metrics") && requires { object.metrics; }) return (object.metrics);
  else if constexpr (Key::name.view() == std::string_view("min") && requires { object.min; }) return (object.min);
  else if constexpr (Key::name.view() == std::string_view("minEv") && requires { object.min_ev; }) return (object.min_ev);
  else if constexpr (Key::name.view() == std::string_view("minExposure") && requires { object.min_exposure; }) return (object.min_exposure);
  else if constexpr (Key::name.view() == std::string_view("minFilter") && requires { object.min_filter; }) return (object.min_filter);
  else if constexpr (Key::name.view() == std::string_view("minParticleCount") && requires { object.min_particle_count; }) return (object.min_particle_count);
  else if constexpr (Key::name.view() == std::string_view("minRadius") && requires { object.min_radius; }) return (object.min_radius);
  else if constexpr (Key::name.view() == std::string_view("minRadiusVariance") && requires { object.min_radius_variance; }) return (object.min_radius_variance);
  else if constexpr (Key::name.view() == std::string_view("minX") && requires { object.min_x; }) return (object.min_x);
  else if constexpr (Key::name.view() == std::string_view("minY") && requires { object.min_y; }) return (object.min_y);
  else if constexpr (Key::name.view() == std::string_view("minZ") && requires { object.min_z; }) return (object.min_z);
  else if constexpr (Key::name.view() == std::string_view("mipmaps") && requires { object.mipmaps; }) return (object.mipmaps);
  else if constexpr (Key::name.view() == std::string_view("mode") && requires { object.mode; }) return (object.mode);
  else if constexpr (Key::name.view() == std::string_view("model") && requires { object.model; }) return (object.model);
  else if constexpr (Key::name.view() == std::string_view("modifiers") && requires { object.modifiers; }) return (object.modifiers);
  else if constexpr (Key::name.view() == std::string_view("name") && requires { object.name; }) return (object.name);
  else if constexpr (Key::name.view() == std::string_view("near") && requires { object.near; }) return (object.near);
  else if constexpr (Key::name.view() == std::string_view("normalBias") && requires { object.normal_bias; }) return (object.normal_bias);
  else if constexpr (Key::name.view() == std::string_view("normalX") && requires { object.normal_x; }) return (object.normal_x);
  else if constexpr (Key::name.view() == std::string_view("normalY") && requires { object.normal_y; }) return (object.normal_y);
  else if constexpr (Key::name.view() == std::string_view("normalZ") && requires { object.normal_z; }) return (object.normal_z);
  else if constexpr (Key::name.view() == std::string_view("notification") && requires { object.notification; }) return (object.notification);
  else if constexpr (Key::name.view() == std::string_view("numLines") && requires { object.num_lines; }) return (object.num_lines);
  else if constexpr (Key::name.view() == std::string_view("object") && requires { object.object; }) return (object.object);
  else if constexpr (Key::name.view() == std::string_view("offset") && requires { object.offset; }) return (object.offset);
  else if constexpr (Key::name.view() == std::string_view("offsetX") && requires { object.offset_x; }) return (object.offset_x);
  else if constexpr (Key::name.view() == std::string_view("offsetY") && requires { object.offset_y; }) return (object.offset_y);
  else if constexpr (Key::name.view() == std::string_view("offsets") && requires { object.offsets; }) return (object.offsets);
  else if constexpr (Key::name.view() == std::string_view("onAbsoluteOrientation") && requires { object.on_absolute_orientation; }) return (object.on_absolute_orientation);
  else if constexpr (Key::name.view() == std::string_view("onAccelerometer") && requires { object.on_accelerometer; }) return (object.on_accelerometer);
  else if constexpr (Key::name.view() == std::string_view("onAction") && requires { object.on_action; }) return (object.on_action);
  else if constexpr (Key::name.view() == std::string_view("onAmbientLight") && requires { object.on_ambient_light; }) return (object.on_ambient_light);
  else if constexpr (Key::name.view() == std::string_view("onBarometer") && requires { object.on_barometer; }) return (object.on_barometer);
  else if constexpr (Key::name.view() == std::string_view("onChange") && requires { object.on_change; }) return (object.on_change);
  else if constexpr (Key::name.view() == std::string_view("onEmitterComplete") && requires { object.on_emitter_complete; }) return (object.on_emitter_complete);
  else if constexpr (Key::name.view() == std::string_view("onGravity") && requires { object.on_gravity; }) return (object.on_gravity);
  else if constexpr (Key::name.view() == std::string_view("onGyroscope") && requires { object.on_gyroscope; }) return (object.on_gyroscope);
  else if constexpr (Key::name.view() == std::string_view("onHide") && requires { object.on_hide; }) return (object.on_hide);
  else if constexpr (Key::name.view() == std::string_view("onLinearAcceleration") && requires { object.on_linear_acceleration; }) return (object.on_linear_acceleration);
  else if constexpr (Key::name.view() == std::string_view("onMagnetometer") && requires { object.on_magnetometer; }) return (object.on_magnetometer);
  else if constexpr (Key::name.view() == std::string_view("onOrientation") && requires { object.on_orientation; }) return (object.on_orientation);
  else if constexpr (Key::name.view() == std::string_view("onParticleDeath") && requires { object.on_particle_death; }) return (object.on_particle_death);
  else if constexpr (Key::name.view() == std::string_view("onParticleSpawn") && requires { object.on_particle_spawn; }) return (object.on_particle_spawn);
  else if constexpr (Key::name.view() == std::string_view("onProximity") && requires { object.on_proximity; }) return (object.on_proximity);
  else if constexpr (Key::name.view() == std::string_view("onQuaternion") && requires { object.on_quaternion; }) return (object.on_quaternion);
  else if constexpr (Key::name.view() == std::string_view("onResize") && requires { object.on_resize; }) return (object.on_resize);
  else if constexpr (Key::name.view() == std::string_view("onShow") && requires { object.on_show; }) return (object.on_show);
  else if constexpr (Key::name.view() == std::string_view("onTick") && requires { object.on_tick; }) return (object.on_tick);
  else if constexpr (Key::name.view() == std::string_view("once") && requires { object.once; }) return (object.once);
  else if constexpr (Key::name.view() == std::string_view("operation") && requires { object.operation; }) return (object.operation);
  else if constexpr (Key::name.view() == std::string_view("operator") && requires { object.operator_; }) return (object.operator_);
  else if constexpr (Key::name.view() == std::string_view("orientationW") && requires { object.orientation_w; }) return (object.orientation_w);
  else if constexpr (Key::name.view() == std::string_view("orientationX") && requires { object.orientation_x; }) return (object.orientation_x);
  else if constexpr (Key::name.view() == std::string_view("orientationY") && requires { object.orientation_y; }) return (object.orientation_y);
  else if constexpr (Key::name.view() == std::string_view("orientationZ") && requires { object.orientation_z; }) return (object.orientation_z);
  else if constexpr (Key::name.view() == std::string_view("origin") && requires { object.origin; }) return (object.origin);
  else if constexpr (Key::name.view() == std::string_view("originX") && requires { object.origin_x; }) return (object.origin_x);
  else if constexpr (Key::name.view() == std::string_view("originY") && requires { object.origin_y; }) return (object.origin_y);
  else if constexpr (Key::name.view() == std::string_view("osBuild") && requires { object.os_build; }) return (object.os_build);
  else if constexpr (Key::name.view() == std::string_view("osName") && requires { object.os_name; }) return (object.os_name);
  else if constexpr (Key::name.view() == std::string_view("osVersion") && requires { object.os_version; }) return (object.os_version);
  else if constexpr (Key::name.view() == std::string_view("outerConeCos") && requires { object.outer_cone_cos; }) return (object.outer_cone_cos);
  else if constexpr (Key::name.view() == std::string_view("outerConeDegrees") && requires { object.outer_cone_degrees; }) return (object.outer_cone_degrees);
  else if constexpr (Key::name.view() == std::string_view("overlapping") && requires { object.overlapping; }) return (object.overlapping);
  else if constexpr (Key::name.view() == std::string_view("overlaysContent") && requires { object.overlays_content; }) return (object.overlays_content);
  else if constexpr (Key::name.view() == std::string_view("pages") && requires { object.pages; }) return (object.pages);
  else if constexpr (Key::name.view() == std::string_view("parent") && requires { object.parent; }) return (object.parent);
  else if constexpr (Key::name.view() == std::string_view("particleLifespan") && requires { object.particle_lifespan; }) return (object.particle_lifespan);
  else if constexpr (Key::name.view() == std::string_view("particleLifespanVariance") && requires { object.particle_lifespan_variance; }) return (object.particle_lifespan_variance);
  else if constexpr (Key::name.view() == std::string_view("paused") && requires { object.paused; }) return (object.paused);
  else if constexpr (Key::name.view() == std::string_view("pcfRadius") && requires { object.pcf_radius; }) return (object.pcf_radius);
  else if constexpr (Key::name.view() == std::string_view("pedestal") && requires { object.pedestal; }) return (object.pedestal);
  else if constexpr (Key::name.view() == std::string_view("physicalHeight") && requires { object.physical_height; }) return (object.physical_height);
  else if constexpr (Key::name.view() == std::string_view("physicalWidth") && requires { object.physical_width; }) return (object.physical_width);
  else if constexpr (Key::name.view() == std::string_view("pivotX") && requires { object.pivot_x; }) return (object.pivot_x);
  else if constexpr (Key::name.view() == std::string_view("pivotY") && requires { object.pivot_y; }) return (object.pivot_y);
  else if constexpr (Key::name.view() == std::string_view("pixelRatio") && requires { object.pixel_ratio; }) return (object.pixel_ratio);
  else if constexpr (Key::name.view() == std::string_view("platformString") && requires { object.platform_string; }) return (object.platform_string);
  else if constexpr (Key::name.view() == std::string_view("playbackRate") && requires { object.playback_rate; }) return (object.playback_rate);
  else if constexpr (Key::name.view() == std::string_view("point") && requires { object.point; }) return (object.point);
  else if constexpr (Key::name.view() == std::string_view("pointCount") && requires { object.point_count; }) return (object.point_count);
  else if constexpr (Key::name.view() == std::string_view("pointerWidth") && requires { object.pointer_width; }) return (object.pointer_width);
  else if constexpr (Key::name.view() == std::string_view("points") && requires { object.points; }) return (object.points);
  else if constexpr (Key::name.view() == std::string_view("position") && requires { object.position; }) return (object.position);
  else if constexpr (Key::name.view() == std::string_view("positions") && requires { object.positions; }) return (object.positions);
  else if constexpr (Key::name.view() == std::string_view("premultipliedAlpha") && requires { object.premultiplied_alpha; }) return (object.premultiplied_alpha);
  else if constexpr (Key::name.view() == std::string_view("prepare") && requires { object.prepare; }) return (object.prepare);
  else if constexpr (Key::name.view() == std::string_view("preserveAlpha") && requires { object.preserve_alpha; }) return (object.preserve_alpha);
  else if constexpr (Key::name.view() == std::string_view("pressure") && requires { object.pressure; }) return (object.pressure);
  else if constexpr (Key::name.view() == std::string_view("previousWorldTransform") && requires { object.previous_world_transform; }) return (object.previous_world_transform);
  else if constexpr (Key::name.view() == std::string_view("priority") && requires { object.priority; }) return (object.priority);
  else if constexpr (Key::name.view() == std::string_view("productName") && requires { object.product_name; }) return (object.product_name);
  else if constexpr (Key::name.view() == std::string_view("projection") && requires { object.projection; }) return (object.projection);
  else if constexpr (Key::name.view() == std::string_view("promptForAccess") && requires { object.prompt_for_access; }) return (object.prompt_for_access);
  else if constexpr (Key::name.view() == std::string_view("querySpatialPairs") && requires { object.query_spatial_pairs; }) return (object.query_spatial_pairs);
  else if constexpr (Key::name.view() == std::string_view("querySpatialPoint") && requires { object.query_spatial_point; }) return (object.query_spatial_point);
  else if constexpr (Key::name.view() == std::string_view("querySpatialRay") && requires { object.query_spatial_ray; }) return (object.query_spatial_ray);
  else if constexpr (Key::name.view() == std::string_view("querySpatialRegion") && requires { object.query_spatial_region; }) return (object.query_spatial_region);
  else if constexpr (Key::name.view() == std::string_view("rAX") && requires { object.r_ax; }) return (object.r_ax);
  else if constexpr (Key::name.view() == std::string_view("rAY") && requires { object.r_ay; }) return (object.r_ay);
  else if constexpr (Key::name.view() == std::string_view("rAZ") && requires { object.r_az; }) return (object.r_az);
  else if constexpr (Key::name.view() == std::string_view("rBX") && requires { object.r_bx; }) return (object.r_bx);
  else if constexpr (Key::name.view() == std::string_view("rBY") && requires { object.r_by; }) return (object.r_by);
  else if constexpr (Key::name.view() == std::string_view("rBZ") && requires { object.r_bz; }) return (object.r_bz);
  else if constexpr (Key::name.view() == std::string_view("radial") && requires { object.radial; }) return (object.radial);
  else if constexpr (Key::name.view() == std::string_view("radialAccelVariance") && requires { object.radial_accel_variance; }) return (object.radial_accel_variance);
  else if constexpr (Key::name.view() == std::string_view("radialAcceleration") && requires { object.radial_acceleration; }) return (object.radial_acceleration);
  else if constexpr (Key::name.view() == std::string_view("radius") && requires { object.radius; }) return (object.radius);
  else if constexpr (Key::name.view() == std::string_view("range") && requires { object.range; }) return (object.range);
  else if constexpr (Key::name.view() == std::string_view("readBookmark") && requires { object.read_bookmark; }) return (object.read_bookmark);
  else if constexpr (Key::name.view() == std::string_view("readFormat") && requires { object.read_format; }) return (object.read_format);
  else if constexpr (Key::name.view() == std::string_view("readHtml") && requires { object.read_html; }) return (object.read_html);
  else if constexpr (Key::name.view() == std::string_view("readImage") && requires { object.read_image; }) return (object.read_image);
  else if constexpr (Key::name.view() == std::string_view("readItems") && requires { object.read_items; }) return (object.read_items);
  else if constexpr (Key::name.view() == std::string_view("readRTF") && requires { object.read_rtf; }) return (object.read_rtf);
  else if constexpr (Key::name.view() == std::string_view("readText") && requires { object.read_text; }) return (object.read_text);
  else if constexpr (Key::name.view() == std::string_view("reason") && requires { object.reason; }) return (object.reason);
  else if constexpr (Key::name.view() == std::string_view("red") && requires { object.red; }) return (object.red);
  else if constexpr (Key::name.view() == std::string_view("redBias") && requires { object.red_bias; }) return (object.red_bias);
  else if constexpr (Key::name.view() == std::string_view("redScale") && requires { object.red_scale; }) return (object.red_scale);
  else if constexpr (Key::name.view() == std::string_view("refresh") && requires { object.refresh; }) return (object.refresh);
  else if constexpr (Key::name.view() == std::string_view("regionIdMax") && requires { object.region_id_max; }) return (object.region_id_max);
  else if constexpr (Key::name.view() == std::string_view("regionIdMin") && requires { object.region_id_min; }) return (object.region_id_min);
  else if constexpr (Key::name.view() == std::string_view("relative") && requires { object.relative; }) return (object.relative);
  else if constexpr (Key::name.view() == std::string_view("removeNode") && requires { object.remove_node; }) return (object.remove_node);
  else if constexpr (Key::name.view() == std::string_view("removeSpatialObject") && requires { object.remove_spatial_object; }) return (object.remove_spatial_object);
  else if constexpr (Key::name.view() == std::string_view("repeatCount") && requires { object.repeat_count; }) return (object.repeat_count);
  else if constexpr (Key::name.view() == std::string_view("requestPermission") && requires { object.request_permission; }) return (object.request_permission);
  else if constexpr (Key::name.view() == std::string_view("requestPersistence") && requires { object.request_persistence; }) return (object.request_persistence);
  else if constexpr (Key::name.view() == std::string_view("resize") && requires { object.resize; }) return (object.resize);
  else if constexpr (Key::name.view() == std::string_view("resolution") && requires { object.resolution; }) return (object.resolution);
  else if constexpr (Key::name.view() == std::string_view("restitution") && requires { object.restitution; }) return (object.restitution);
  else if constexpr (Key::name.view() == std::string_view("right") && requires { object.right; }) return (object.right);
  else if constexpr (Key::name.view() == std::string_view("root") && requires { object.root; }) return (object.root);
  else if constexpr (Key::name.view() == std::string_view("rotatePerSecond") && requires { object.rotate_per_second; }) return (object.rotate_per_second);
  else if constexpr (Key::name.view() == std::string_view("rotatePerSecondVariance") && requires { object.rotate_per_second_variance; }) return (object.rotate_per_second_variance);
  else if constexpr (Key::name.view() == std::string_view("rotated") && requires { object.rotated; }) return (object.rotated);
  else if constexpr (Key::name.view() == std::string_view("rotation") && requires { object.rotation; }) return (object.rotation);
  else if constexpr (Key::name.view() == std::string_view("rotationAmplitude") && requires { object.rotation_amplitude; }) return (object.rotation_amplitude);
  else if constexpr (Key::name.view() == std::string_view("rotationEnd") && requires { object.rotation_end; }) return (object.rotation_end);
  else if constexpr (Key::name.view() == std::string_view("rotationEndVariance") && requires { object.rotation_end_variance; }) return (object.rotation_end_variance);
  else if constexpr (Key::name.view() == std::string_view("rotationSpeedMax") && requires { object.rotation_speed_max; }) return (object.rotation_speed_max);
  else if constexpr (Key::name.view() == std::string_view("rotationSpeedMin") && requires { object.rotation_speed_min; }) return (object.rotation_speed_min);
  else if constexpr (Key::name.view() == std::string_view("rotationStart") && requires { object.rotation_start; }) return (object.rotation_start);
  else if constexpr (Key::name.view() == std::string_view("rotationStartVariance") && requires { object.rotation_start_variance; }) return (object.rotation_start_variance);
  else if constexpr (Key::name.view() == std::string_view("rotationX") && requires { object.rotation_x; }) return (object.rotation_x);
  else if constexpr (Key::name.view() == std::string_view("rotationY") && requires { object.rotation_y; }) return (object.rotation_y);
  else if constexpr (Key::name.view() == std::string_view("rotationZ") && requires { object.rotation_z; }) return (object.rotation_z);
  else if constexpr (Key::name.view() == std::string_view("runtime") && requires { object.runtime; }) return (object.runtime);
  else if constexpr (Key::name.view() == std::string_view("samples") && requires { object.samples; }) return (object.samples);
  else if constexpr (Key::name.view() == std::string_view("saturation") && requires { object.saturation; }) return (object.saturation);
  else if constexpr (Key::name.view() == std::string_view("scale") && requires { object.scale; }) return (object.scale);
  else if constexpr (Key::name.view() == std::string_view("scaleCurve") && requires { object.scale_curve; }) return (object.scale_curve);
  else if constexpr (Key::name.view() == std::string_view("scaleEnd") && requires { object.scale_end; }) return (object.scale_end);
  else if constexpr (Key::name.view() == std::string_view("scaleMax") && requires { object.scale_max; }) return (object.scale_max);
  else if constexpr (Key::name.view() == std::string_view("scaleMin") && requires { object.scale_min; }) return (object.scale_min);
  else if constexpr (Key::name.view() == std::string_view("scaleX") && requires { object.scale_x; }) return (object.scale_x);
  else if constexpr (Key::name.view() == std::string_view("scaleY") && requires { object.scale_y; }) return (object.scale_y);
  else if constexpr (Key::name.view() == std::string_view("scaling") && requires { object.scaling; }) return (object.scaling);
  else if constexpr (Key::name.view() == std::string_view("scanlineIntensity") && requires { object.scanline_intensity; }) return (object.scanline_intensity);
  else if constexpr (Key::name.view() == std::string_view("scattering") && requires { object.scattering; }) return (object.scattering);
  else if constexpr (Key::name.view() == std::string_view("scope") && requires { object.scope; }) return (object.scope);
  else if constexpr (Key::name.view() == std::string_view("seed") && requires { object.seed; }) return (object.seed);
  else if constexpr (Key::name.view() == std::string_view("segment") && requires { object.segment; }) return (object.segment);
  else if constexpr (Key::name.view() == std::string_view("selection") && requires { object.selection; }) return (object.selection);
  else if constexpr (Key::name.view() == std::string_view("send") && requires { object.send; }) return (object.send);
  else if constexpr (Key::name.view() == std::string_view("sensor") && requires { object.sensor; }) return (object.sensor);
  else if constexpr (Key::name.view() == std::string_view("setAccessoryBarVisible") && requires { object.set_accessory_bar_visible; }) return (object.set_accessory_bar_visible);
  else if constexpr (Key::name.view() == std::string_view("setBackgroundColor") && requires { object.set_background_color; }) return (object.set_background_color);
  else if constexpr (Key::name.view() == std::string_view("setDisplaySize") && requires { object.set_display_size; }) return (object.set_display_size);
  else if constexpr (Key::name.view() == std::string_view("setFocus") && requires { object.set_focus; }) return (object.set_focus);
  else if constexpr (Key::name.view() == std::string_view("setMetadata") && requires { object.set_metadata; }) return (object.set_metadata);
  else if constexpr (Key::name.view() == std::string_view("setNode") && requires { object.set_node; }) return (object.set_node);
  else if constexpr (Key::name.view() == std::string_view("setOverlaysContent") && requires { object.set_overlays_content; }) return (object.set_overlays_content);
  else if constexpr (Key::name.view() == std::string_view("setPlaybackState") && requires { object.set_playback_state; }) return (object.set_playback_state);
  else if constexpr (Key::name.view() == std::string_view("setPositionState") && requires { object.set_position_state; }) return (object.set_position_state);
  else if constexpr (Key::name.view() == std::string_view("setResizeMode") && requires { object.set_resize_mode; }) return (object.set_resize_mode);
  else if constexpr (Key::name.view() == std::string_view("setScrollAssistEnabled") && requires { object.set_scroll_assist_enabled; }) return (object.set_scroll_assist_enabled);
  else if constexpr (Key::name.view() == std::string_view("setStyle") && requires { object.set_style; }) return (object.set_style);
  else if constexpr (Key::name.view() == std::string_view("setVisible") && requires { object.set_visible; }) return (object.set_visible);
  else if constexpr (Key::name.view() == std::string_view("shaderKey") && requires { object.shader_key; }) return (object.shader_key);
  else if constexpr (Key::name.view() == std::string_view("shadowBias") && requires { object.shadow_bias; }) return (object.shadow_bias);
  else if constexpr (Key::name.view() == std::string_view("shadowFar") && requires { object.shadow_far; }) return (object.shadow_far);
  else if constexpr (Key::name.view() == std::string_view("shadowMapSize") && requires { object.shadow_map_size; }) return (object.shadow_map_size);
  else if constexpr (Key::name.view() == std::string_view("shadowNear") && requires { object.shadow_near; }) return (object.shadow_near);
  else if constexpr (Key::name.view() == std::string_view("shadowStrength") && requires { object.shadow_strength; }) return (object.shadow_strength);
  else if constexpr (Key::name.view() == std::string_view("sheenColor") && requires { object.sheen_color; }) return (object.sheen_color);
  else if constexpr (Key::name.view() == std::string_view("sheenColorMap") && requires { object.sheen_color_map; }) return (object.sheen_color_map);
  else if constexpr (Key::name.view() == std::string_view("sheenColorMapUvSet") && requires { object.sheen_color_map_uv_set; }) return (object.sheen_color_map_uv_set);
  else if constexpr (Key::name.view() == std::string_view("sheenRoughness") && requires { object.sheen_roughness; }) return (object.sheen_roughness);
  else if constexpr (Key::name.view() == std::string_view("sheenRoughnessMap") && requires { object.sheen_roughness_map; }) return (object.sheen_roughness_map);
  else if constexpr (Key::name.view() == std::string_view("sheenRoughnessMapUvSet") && requires { object.sheen_roughness_map_uv_set; }) return (object.sheen_roughness_map_uv_set);
  else if constexpr (Key::name.view() == std::string_view("show") && requires { object.show; }) return (object.show);
  else if constexpr (Key::name.view() == std::string_view("size") && requires { object.size; }) return (object.size);
  else if constexpr (Key::name.view() == std::string_view("skewX") && requires { object.skew_x; }) return (object.skew_x);
  else if constexpr (Key::name.view() == std::string_view("skewY") && requires { object.skew_y; }) return (object.skew_y);
  else if constexpr (Key::name.view() == std::string_view("skyColor") && requires { object.sky_color; }) return (object.sky_color);
  else if constexpr (Key::name.view() == std::string_view("slotIndex") && requires { object.slot_index; }) return (object.slot_index);
  else if constexpr (Key::name.view() == std::string_view("slots") && requires { object.slots; }) return (object.slots);
  else if constexpr (Key::name.view() == std::string_view("smoothTime") && requires { object.smooth_time; }) return (object.smooth_time);
  else if constexpr (Key::name.view() == std::string_view("softness") && requires { object.softness; }) return (object.softness);
  else if constexpr (Key::name.view() == std::string_view("sourceHeight") && requires { object.source_height; }) return (object.source_height);
  else if constexpr (Key::name.view() == std::string_view("sourceMode") && requires { object.source_mode; }) return (object.source_mode);
  else if constexpr (Key::name.view() == std::string_view("sourcePositionVariancex") && requires { object.source_position_variancex; }) return (object.source_position_variancex);
  else if constexpr (Key::name.view() == std::string_view("sourcePositionVariancey") && requires { object.source_position_variancey; }) return (object.source_position_variancey);
  else if constexpr (Key::name.view() == std::string_view("sourceWidth") && requires { object.source_width; }) return (object.source_width);
  else if constexpr (Key::name.view() == std::string_view("spawnHeight") && requires { object.spawn_height; }) return (object.spawn_height);
  else if constexpr (Key::name.view() == std::string_view("spawnRate") && requires { object.spawn_rate; }) return (object.spawn_rate);
  else if constexpr (Key::name.view() == std::string_view("spawnShape") && requires { object.spawn_shape; }) return (object.spawn_shape);
  else if constexpr (Key::name.view() == std::string_view("spawnWidth") && requires { object.spawn_width; }) return (object.spawn_width);
  else if constexpr (Key::name.view() == std::string_view("specular") && requires { object.specular; }) return (object.specular);
  else if constexpr (Key::name.view() == std::string_view("specularColor") && requires { object.specular_color; }) return (object.specular_color);
  else if constexpr (Key::name.view() == std::string_view("specularColorMap") && requires { object.specular_color_map; }) return (object.specular_color_map);
  else if constexpr (Key::name.view() == std::string_view("specularColorMapUvSet") && requires { object.specular_color_map_uv_set; }) return (object.specular_color_map_uv_set);
  else if constexpr (Key::name.view() == std::string_view("specularMap") && requires { object.specular_map; }) return (object.specular_map);
  else if constexpr (Key::name.view() == std::string_view("specularMapUvSet") && requires { object.specular_map_uv_set; }) return (object.specular_map_uv_set);
  else if constexpr (Key::name.view() == std::string_view("speed") && requires { object.speed; }) return (object.speed);
  else if constexpr (Key::name.view() == std::string_view("speedMax") && requires { object.speed_max; }) return (object.speed_max);
  else if constexpr (Key::name.view() == std::string_view("speedMin") && requires { object.speed_min; }) return (object.speed_min);
  else if constexpr (Key::name.view() == std::string_view("speedVariance") && requires { object.speed_variance; }) return (object.speed_variance);
  else if constexpr (Key::name.view() == std::string_view("spot") && requires { object.spot; }) return (object.spot);
  else if constexpr (Key::name.view() == std::string_view("spotBlend") && requires { object.spot_blend; }) return (object.spot_blend);
  else if constexpr (Key::name.view() == std::string_view("spread") && requires { object.spread; }) return (object.spread);
  else if constexpr (Key::name.view() == std::string_view("stack") && requires { object.stack; }) return (object.stack);
  else if constexpr (Key::name.view() == std::string_view("start") && requires { object.start; }) return (object.start);
  else if constexpr (Key::name.view() == std::string_view("startColor") && requires { object.start_color; }) return (object.start_color);
  else if constexpr (Key::name.view() == std::string_view("startColorVariance") && requires { object.start_color_variance; }) return (object.start_color_variance);
  else if constexpr (Key::name.view() == std::string_view("startIndex") && requires { object.start_index; }) return (object.start_index);
  else if constexpr (Key::name.view() == std::string_view("startParticleSize") && requires { object.start_particle_size; }) return (object.start_particle_size);
  else if constexpr (Key::name.view() == std::string_view("startParticleSizeVariance") && requires { object.start_particle_size_variance; }) return (object.start_particle_size_variance);
  else if constexpr (Key::name.view() == std::string_view("startX") && requires { object.start_x; }) return (object.start_x);
  else if constexpr (Key::name.view() == std::string_view("startY") && requires { object.start_y; }) return (object.start_y);
  else if constexpr (Key::name.view() == std::string_view("startZ") && requires { object.start_z; }) return (object.start_z);
  else if constexpr (Key::name.view() == std::string_view("stated") && requires { object.stated; }) return (object.stated);
  else if constexpr (Key::name.view() == std::string_view("steps") && requires { object.steps; }) return (object.steps);
  else if constexpr (Key::name.view() == std::string_view("strength") && requires { object.strength; }) return (object.strength);
  else if constexpr (Key::name.view() == std::string_view("strokeBounds") && requires { object.stroke_bounds; }) return (object.stroke_bounds);
  else if constexpr (Key::name.view() == std::string_view("style") && requires { object.style; }) return (object.style);
  else if constexpr (Key::name.view() == std::string_view("subject") && requires { object.subject; }) return (object.subject);
  else if constexpr (Key::name.view() == std::string_view("subpixel") && requires { object.subpixel; }) return (object.subpixel);
  else if constexpr (Key::name.view() == std::string_view("subscribe") && requires { object.subscribe; }) return (object.subscribe);
  else if constexpr (Key::name.view() == std::string_view("subscribeAbsoluteOrientation") && requires { object.subscribe_absolute_orientation; }) return (object.subscribe_absolute_orientation);
  else if constexpr (Key::name.view() == std::string_view("subscribeAmbientLight") && requires { object.subscribe_ambient_light; }) return (object.subscribe_ambient_light);
  else if constexpr (Key::name.view() == std::string_view("subscribeBarometer") && requires { object.subscribe_barometer; }) return (object.subscribe_barometer);
  else if constexpr (Key::name.view() == std::string_view("subscribeGravity") && requires { object.subscribe_gravity; }) return (object.subscribe_gravity);
  else if constexpr (Key::name.view() == std::string_view("subscribeLinearAcceleration") && requires { object.subscribe_linear_acceleration; }) return (object.subscribe_linear_acceleration);
  else if constexpr (Key::name.view() == std::string_view("subscribeMagnetometer") && requires { object.subscribe_magnetometer; }) return (object.subscribe_magnetometer);
  else if constexpr (Key::name.view() == std::string_view("subscribeMotion") && requires { object.subscribe_motion; }) return (object.subscribe_motion);
  else if constexpr (Key::name.view() == std::string_view("subscribeOrientation") && requires { object.subscribe_orientation; }) return (object.subscribe_orientation);
  else if constexpr (Key::name.view() == std::string_view("subscribeProximity") && requires { object.subscribe_proximity; }) return (object.subscribe_proximity);
  else if constexpr (Key::name.view() == std::string_view("subscribeQuaternion") && requires { object.subscribe_quaternion; }) return (object.subscribe_quaternion);
  else if constexpr (Key::name.view() == std::string_view("supportedAbis") && requires { object.supported_abis; }) return (object.supported_abis);
  else if constexpr (Key::name.view() == std::string_view("tangentialAccelVariance") && requires { object.tangential_accel_variance; }) return (object.tangential_accel_variance);
  else if constexpr (Key::name.view() == std::string_view("tangentialAcceleration") && requires { object.tangential_acceleration; }) return (object.tangential_acceleration);
  else if constexpr (Key::name.view() == std::string_view("temperature") && requires { object.temperature; }) return (object.temperature);
  else if constexpr (Key::name.view() == std::string_view("textHeight") && requires { object.text_height; }) return (object.text_height);
  else if constexpr (Key::name.view() == std::string_view("textWidth") && requires { object.text_width; }) return (object.text_width);
  else if constexpr (Key::name.view() == std::string_view("textureFileName") && requires { object.texture_file_name; }) return (object.texture_file_name);
  else if constexpr (Key::name.view() == std::string_view("thickness") && requires { object.thickness; }) return (object.thickness);
  else if constexpr (Key::name.view() == std::string_view("thicknessMap") && requires { object.thickness_map; }) return (object.thickness_map);
  else if constexpr (Key::name.view() == std::string_view("thicknessMapUvSet") && requires { object.thickness_map_uv_set; }) return (object.thickness_map_uv_set);
  else if constexpr (Key::name.view() == std::string_view("threshold") && requires { object.threshold; }) return (object.threshold);
  else if constexpr (Key::name.view() == std::string_view("tilesets") && requires { object.tilesets; }) return (object.tilesets);
  else if constexpr (Key::name.view() == std::string_view("time") && requires { object.time; }) return (object.time);
  else if constexpr (Key::name.view() == std::string_view("timeline") && requires { object.timeline; }) return (object.timeline);
  else if constexpr (Key::name.view() == std::string_view("timestamp") && requires { object.timestamp; }) return (object.timestamp);
  else if constexpr (Key::name.view() == std::string_view("tint") && requires { object.tint; }) return (object.tint);
  else if constexpr (Key::name.view() == std::string_view("top") && requires { object.top; }) return (object.top);
  else if constexpr (Key::name.view() == std::string_view("torque") && requires { object.torque; }) return (object.torque);
  else if constexpr (Key::name.view() == std::string_view("torqueX") && requires { object.torque_x; }) return (object.torque_x);
  else if constexpr (Key::name.view() == std::string_view("torqueY") && requires { object.torque_y; }) return (object.torque_y);
  else if constexpr (Key::name.view() == std::string_view("torqueZ") && requires { object.torque_z; }) return (object.torque_z);
  else if constexpr (Key::name.view() == std::string_view("totalMemory") && requires { object.total_memory; }) return (object.total_memory);
  else if constexpr (Key::name.view() == std::string_view("touching") && requires { object.touching; }) return (object.touching);
  else if constexpr (Key::name.view() == std::string_view("transform") && requires { object.transform; }) return (object.transform);
  else if constexpr (Key::name.view() == std::string_view("translationAmplitude") && requires { object.translation_amplitude; }) return (object.translation_amplitude);
  else if constexpr (Key::name.view() == std::string_view("transmission") && requires { object.transmission; }) return (object.transmission);
  else if constexpr (Key::name.view() == std::string_view("transmissionMap") && requires { object.transmission_map; }) return (object.transmission_map);
  else if constexpr (Key::name.view() == std::string_view("transmissionMapUvSet") && requires { object.transmission_map_uv_set; }) return (object.transmission_map_uv_set);
  else if constexpr (Key::name.view() == std::string_view("transparency") && requires { object.transparency; }) return (object.transparency);
  else if constexpr (Key::name.view() == std::string_view("trauma") && requires { object.trauma; }) return (object.trauma);
  else if constexpr (Key::name.view() == std::string_view("tweens") && requires { object.tweens; }) return (object.tweens);
  else if constexpr (Key::name.view() == std::string_view("tx") && requires { object.tx; }) return (object.tx);
  else if constexpr (Key::name.view() == std::string_view("ty") && requires { object.ty; }) return (object.ty);
  else if constexpr (Key::name.view() == std::string_view("type") && requires { object.type; }) return (object.type);
  else if constexpr (Key::name.view() == std::string_view("uniforms") && requires { object.uniforms; }) return (object.uniforms);
  else if constexpr (Key::name.view() == std::string_view("up") && requires { object.up; }) return (object.up);
  else if constexpr (Key::name.view() == std::string_view("updateSpatialObject") && requires { object.update_spatial_object; }) return (object.update_spatial_object);
  else if constexpr (Key::name.view() == std::string_view("value") && requires { object.value; }) return (object.value);
  else if constexpr (Key::name.view() == std::string_view("velocity") && requires { object.velocity; }) return (object.velocity);
  else if constexpr (Key::name.view() == std::string_view("velocityInheritance") && requires { object.velocity_inheritance; }) return (object.velocity_inheritance);
  else if constexpr (Key::name.view() == std::string_view("version") && requires { object.version; }) return (object.version);
  else if constexpr (Key::name.view() == std::string_view("vibrate") && requires { object.vibrate; }) return (object.vibrate);
  else if constexpr (Key::name.view() == std::string_view("vibratePattern") && requires { object.vibrate_pattern; }) return (object.vibrate_pattern);
  else if constexpr (Key::name.view() == std::string_view("vibrateWaveform") && requires { object.vibrate_waveform; }) return (object.vibrate_waveform);
  else if constexpr (Key::name.view() == std::string_view("view") && requires { object.view; }) return (object.view);
  else if constexpr (Key::name.view() == std::string_view("viewportHeight") && requires { object.viewport_height; }) return (object.viewport_height);
  else if constexpr (Key::name.view() == std::string_view("viewportWidth") && requires { object.viewport_width; }) return (object.viewport_width);
  else if constexpr (Key::name.view() == std::string_view("vignette") && requires { object.vignette; }) return (object.vignette);
  else if constexpr (Key::name.view() == std::string_view("visible") && requires { object.visible; }) return (object.visible);
  else if constexpr (Key::name.view() == std::string_view("w") && requires { object.w; }) return (object.w);
  else if constexpr (Key::name.view() == std::string_view("watchPosition") && requires { object.watch_position; }) return (object.watch_position);
  else if constexpr (Key::name.view() == std::string_view("webViewVersion") && requires { object.web_view_version; }) return (object.web_view_version);
  else if constexpr (Key::name.view() == std::string_view("weight") && requires { object.weight; }) return (object.weight);
  else if constexpr (Key::name.view() == std::string_view("white") && requires { object.white; }) return (object.white);
  else if constexpr (Key::name.view() == std::string_view("width") && requires { object.width; }) return (object.width);
  else if constexpr (Key::name.view() == std::string_view("wind") && requires { object.wind; }) return (object.wind);
  else if constexpr (Key::name.view() == std::string_view("winding") && requires { object.winding; }) return (object.winding);
  else if constexpr (Key::name.view() == std::string_view("worldBounds") && requires { object.world_bounds; }) return (object.world_bounds);
  else if constexpr (Key::name.view() == std::string_view("worldMatrices") && requires { object.world_matrices; }) return (object.world_matrices);
  else if constexpr (Key::name.view() == std::string_view("worldSpace") && requires { object.world_space; }) return (object.world_space);
  else if constexpr (Key::name.view() == std::string_view("wouldOccupyBucketCount") && requires { object.would_occupy_bucket_count; }) return (object.would_occupy_bucket_count);
  else if constexpr (Key::name.view() == std::string_view("wrapU") && requires { object.wrap_u; }) return (object.wrap_u);
  else if constexpr (Key::name.view() == std::string_view("wrapV") && requires { object.wrap_v; }) return (object.wrap_v);
  else if constexpr (Key::name.view() == std::string_view("wrappedDiffuseColor") && requires { object.wrapped_diffuse_color; }) return (object.wrapped_diffuse_color);
  else if constexpr (Key::name.view() == std::string_view("wrappedDiffuseMap") && requires { object.wrapped_diffuse_map; }) return (object.wrapped_diffuse_map);
  else if constexpr (Key::name.view() == std::string_view("wrappedDiffuseMapUvSet") && requires { object.wrapped_diffuse_map_uv_set; }) return (object.wrapped_diffuse_map_uv_set);
  else if constexpr (Key::name.view() == std::string_view("wrappedDiffuseStrength") && requires { object.wrapped_diffuse_strength; }) return (object.wrapped_diffuse_strength);
  else if constexpr (Key::name.view() == std::string_view("writeBookmark") && requires { object.write_bookmark; }) return (object.write_bookmark);
  else if constexpr (Key::name.view() == std::string_view("writeFormat") && requires { object.write_format; }) return (object.write_format);
  else if constexpr (Key::name.view() == std::string_view("writeHtml") && requires { object.write_html; }) return (object.write_html);
  else if constexpr (Key::name.view() == std::string_view("writeImage") && requires { object.write_image; }) return (object.write_image);
  else if constexpr (Key::name.view() == std::string_view("writeItems") && requires { object.write_items; }) return (object.write_items);
  else if constexpr (Key::name.view() == std::string_view("writeRTF") && requires { object.write_rtf; }) return (object.write_rtf);
  else if constexpr (Key::name.view() == std::string_view("writeText") && requires { object.write_text; }) return (object.write_text);
  else if constexpr (Key::name.view() == std::string_view("x") && requires { object.x; }) return (object.x);
  else if constexpr (Key::name.view() == std::string_view("x0") && requires { object.x0; }) return (object.x0);
  else if constexpr (Key::name.view() == std::string_view("x1") && requires { object.x1; }) return (object.x1);
  else if constexpr (Key::name.view() == std::string_view("xOffset") && requires { object.x_offset; }) return (object.x_offset);
  else if constexpr (Key::name.view() == std::string_view("y") && requires { object.y; }) return (object.y);
  else if constexpr (Key::name.view() == std::string_view("y0") && requires { object.y0; }) return (object.y0);
  else if constexpr (Key::name.view() == std::string_view("y1") && requires { object.y1; }) return (object.y1);
  else if constexpr (Key::name.view() == std::string_view("yOffset") && requires { object.y_offset; }) return (object.y_offset);
  else if constexpr (Key::name.view() == std::string_view("z") && requires { object.z; }) return (object.z);
  else if constexpr (Key::name.view() == std::string_view("zoom") && requires { object.zoom; }) return (object.zoom);
  else static_assert(dependent_false<Key>, "Flight SDK row key has no compatible generated C++ member");
}

template <typename Key, typename Object>
consteval auto generated_row_member_type_identity() {
  if constexpr (Key::name.view() == std::string_view("__brand") && requires(Object& object) { object.brand; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().brand)>>{};
  else if constexpr (Key::name.view() == std::string_view("a") && requires(Object& object) { object.a; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().a)>>{};
  else if constexpr (Key::name.view() == std::string_view("aberration") && requires(Object& object) { object.aberration; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().aberration)>>{};
  else if constexpr (Key::name.view() == std::string_view("absolute") && requires(Object& object) { object.absolute; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().absolute)>>{};
  else if constexpr (Key::name.view() == std::string_view("accuracy") && requires(Object& object) { object.accuracy; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().accuracy)>>{};
  else if constexpr (Key::name.view() == std::string_view("action") && requires(Object& object) { object.action; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().action)>>{};
  else if constexpr (Key::name.view() == std::string_view("adaptationSpeed") && requires(Object& object) { object.adaptation_speed; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().adaptation_speed)>>{};
  else if constexpr (Key::name.view() == std::string_view("additive") && requires(Object& object) { object.additive; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().additive)>>{};
  else if constexpr (Key::name.view() == std::string_view("addressed") && requires(Object& object) { object.addressed; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().addressed)>>{};
  else if constexpr (Key::name.view() == std::string_view("alpha") && requires(Object& object) { object.alpha; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().alpha)>>{};
  else if constexpr (Key::name.view() == std::string_view("alphaBias") && requires(Object& object) { object.alpha_bias; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().alpha_bias)>>{};
  else if constexpr (Key::name.view() == std::string_view("alphaCurve") && requires(Object& object) { object.alpha_curve; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().alpha_curve)>>{};
  else if constexpr (Key::name.view() == std::string_view("alphaEnd") && requires(Object& object) { object.alpha_end; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().alpha_end)>>{};
  else if constexpr (Key::name.view() == std::string_view("alphaScale") && requires(Object& object) { object.alpha_scale; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().alpha_scale)>>{};
  else if constexpr (Key::name.view() == std::string_view("alphaStart") && requires(Object& object) { object.alpha_start; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().alpha_start)>>{};
  else if constexpr (Key::name.view() == std::string_view("altitude") && requires(Object& object) { object.altitude; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().altitude)>>{};
  else if constexpr (Key::name.view() == std::string_view("altitudeAccuracy") && requires(Object& object) { object.altitude_accuracy; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().altitude_accuracy)>>{};
  else if constexpr (Key::name.view() == std::string_view("ambient") && requires(Object& object) { object.ambient; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().ambient)>>{};
  else if constexpr (Key::name.view() == std::string_view("amount") && requires(Object& object) { object.amount; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().amount)>>{};
  else if constexpr (Key::name.view() == std::string_view("angle") && requires(Object& object) { object.angle; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().angle)>>{};
  else if constexpr (Key::name.view() == std::string_view("angleVariance") && requires(Object& object) { object.angle_variance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().angle_variance)>>{};
  else if constexpr (Key::name.view() == std::string_view("animation") && requires(Object& object) { object.animation; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().animation)>>{};
  else if constexpr (Key::name.view() == std::string_view("animations") && requires(Object& object) { object.animations; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().animations)>>{};
  else if constexpr (Key::name.view() == std::string_view("anisotropy") && requires(Object& object) { object.anisotropy; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().anisotropy)>>{};
  else if constexpr (Key::name.view() == std::string_view("anisotropyMap") && requires(Object& object) { object.anisotropy_map; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().anisotropy_map)>>{};
  else if constexpr (Key::name.view() == std::string_view("anisotropyMapUvSet") && requires(Object& object) { object.anisotropy_map_uv_set; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().anisotropy_map_uv_set)>>{};
  else if constexpr (Key::name.view() == std::string_view("anisotropyRotation") && requires(Object& object) { object.anisotropy_rotation; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().anisotropy_rotation)>>{};
  else if constexpr (Key::name.view() == std::string_view("anisotropyStrength") && requires(Object& object) { object.anisotropy_strength; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().anisotropy_strength)>>{};
  else if constexpr (Key::name.view() == std::string_view("announce") && requires(Object& object) { object.announce; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().announce)>>{};
  else if constexpr (Key::name.view() == std::string_view("applied") && requires(Object& object) { object.applied; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().applied)>>{};
  else if constexpr (Key::name.view() == std::string_view("arch") && requires(Object& object) { object.arch; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().arch)>>{};
  else if constexpr (Key::name.view() == std::string_view("ascent") && requires(Object& object) { object.ascent; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().ascent)>>{};
  else if constexpr (Key::name.view() == std::string_view("atlas") && requires(Object& object) { object.atlas; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().atlas)>>{};
  else if constexpr (Key::name.view() == std::string_view("attenuationColor") && requires(Object& object) { object.attenuation_color; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().attenuation_color)>>{};
  else if constexpr (Key::name.view() == std::string_view("attenuationDistance") && requires(Object& object) { object.attenuation_distance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().attenuation_distance)>>{};
  else if constexpr (Key::name.view() == std::string_view("attributes") && requires(Object& object) { object.attributes; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().attributes)>>{};
  else if constexpr (Key::name.view() == std::string_view("availableMemory") && requires(Object& object) { object.available_memory; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().available_memory)>>{};
  else if constexpr (Key::name.view() == std::string_view("b") && requires(Object& object) { object.b; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().b)>>{};
  else if constexpr (Key::name.view() == std::string_view("beta") && requires(Object& object) { object.beta; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().beta)>>{};
  else if constexpr (Key::name.view() == std::string_view("bias") && requires(Object& object) { object.bias; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().bias)>>{};
  else if constexpr (Key::name.view() == std::string_view("bitmap") && requires(Object& object) { object.bitmap; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().bitmap)>>{};
  else if constexpr (Key::name.view() == std::string_view("blackTighten") && requires(Object& object) { object.black_tighten; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().black_tighten)>>{};
  else if constexpr (Key::name.view() == std::string_view("blendFuncDestination") && requires(Object& object) { object.blend_func_destination; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().blend_func_destination)>>{};
  else if constexpr (Key::name.view() == std::string_view("blendFuncSource") && requires(Object& object) { object.blend_func_source; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().blend_func_source)>>{};
  else if constexpr (Key::name.view() == std::string_view("blendMode") && requires(Object& object) { object.blend_mode; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().blend_mode)>>{};
  else if constexpr (Key::name.view() == std::string_view("blue") && requires(Object& object) { object.blue; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().blue)>>{};
  else if constexpr (Key::name.view() == std::string_view("blueBias") && requires(Object& object) { object.blue_bias; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().blue_bias)>>{};
  else if constexpr (Key::name.view() == std::string_view("blueScale") && requires(Object& object) { object.blue_scale; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().blue_scale)>>{};
  else if constexpr (Key::name.view() == std::string_view("blurX") && requires(Object& object) { object.blur_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().blur_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("blurY") && requires(Object& object) { object.blur_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().blur_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("boardName") && requires(Object& object) { object.board_name; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().board_name)>>{};
  else if constexpr (Key::name.view() == std::string_view("bodies") && requires(Object& object) { object.bodies; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().bodies)>>{};
  else if constexpr (Key::name.view() == std::string_view("bodyA") && requires(Object& object) { object.body_a; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().body_a)>>{};
  else if constexpr (Key::name.view() == std::string_view("bodyB") && requires(Object& object) { object.body_b; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().body_b)>>{};
  else if constexpr (Key::name.view() == std::string_view("bottom") && requires(Object& object) { object.bottom; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().bottom)>>{};
  else if constexpr (Key::name.view() == std::string_view("bounds") && requires(Object& object) { object.bounds; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().bounds)>>{};
  else if constexpr (Key::name.view() == std::string_view("breakForce") && requires(Object& object) { object.break_force; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().break_force)>>{};
  else if constexpr (Key::name.view() == std::string_view("breakTorque") && requires(Object& object) { object.break_torque; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().break_torque)>>{};
  else if constexpr (Key::name.view() == std::string_view("brightness") && requires(Object& object) { object.brightness; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().brightness)>>{};
  else if constexpr (Key::name.view() == std::string_view("burstCount") && requires(Object& object) { object.burst_count; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().burst_count)>>{};
  else if constexpr (Key::name.view() == std::string_view("burstInterval") && requires(Object& object) { object.burst_interval; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().burst_interval)>>{};
  else if constexpr (Key::name.view() == std::string_view("c") && requires(Object& object) { object.c; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().c)>>{};
  else if constexpr (Key::name.view() == std::string_view("cancel") && requires(Object& object) { object.cancel; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().cancel)>>{};
  else if constexpr (Key::name.view() == std::string_view("capabilities") && requires(Object& object) { object.capabilities; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().capabilities)>>{};
  else if constexpr (Key::name.view() == std::string_view("cascadeCount") && requires(Object& object) { object.cascade_count; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().cascade_count)>>{};
  else if constexpr (Key::name.view() == std::string_view("cascadeSplits") && requires(Object& object) { object.cascade_splits; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().cascade_splits)>>{};
  else if constexpr (Key::name.view() == std::string_view("castsShadow") && requires(Object& object) { object.casts_shadow; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().casts_shadow)>>{};
  else if constexpr (Key::name.view() == std::string_view("cellSize") && requires(Object& object) { object.cell_size; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().cell_size)>>{};
  else if constexpr (Key::name.view() == std::string_view("center") && requires(Object& object) { object.center; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().center)>>{};
  else if constexpr (Key::name.view() == std::string_view("centerX") && requires(Object& object) { object.center_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().center_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("centerY") && requires(Object& object) { object.center_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().center_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("centerZ") && requires(Object& object) { object.center_z; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().center_z)>>{};
  else if constexpr (Key::name.view() == std::string_view("child1") && requires(Object& object) { object.child1; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().child1)>>{};
  else if constexpr (Key::name.view() == std::string_view("child2") && requires(Object& object) { object.child2; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().child2)>>{};
  else if constexpr (Key::name.view() == std::string_view("children") && requires(Object& object) { object.children; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().children)>>{};
  else if constexpr (Key::name.view() == std::string_view("clear") && requires(Object& object) { object.clear; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().clear)>>{};
  else if constexpr (Key::name.view() == std::string_view("clearMetadata") && requires(Object& object) { object.clear_metadata; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().clear_metadata)>>{};
  else if constexpr (Key::name.view() == std::string_view("clearPositionState") && requires(Object& object) { object.clear_position_state; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().clear_position_state)>>{};
  else if constexpr (Key::name.view() == std::string_view("clearSpatialIndex") && requires(Object& object) { object.clear_spatial_index; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().clear_spatial_index)>>{};
  else if constexpr (Key::name.view() == std::string_view("clearWatch") && requires(Object& object) { object.clear_watch; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().clear_watch)>>{};
  else if constexpr (Key::name.view() == std::string_view("clearcoat") && requires(Object& object) { object.clearcoat; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().clearcoat)>>{};
  else if constexpr (Key::name.view() == std::string_view("clearcoatMap") && requires(Object& object) { object.clearcoat_map; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().clearcoat_map)>>{};
  else if constexpr (Key::name.view() == std::string_view("clearcoatMapUvSet") && requires(Object& object) { object.clearcoat_map_uv_set; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().clearcoat_map_uv_set)>>{};
  else if constexpr (Key::name.view() == std::string_view("clearcoatNormalMap") && requires(Object& object) { object.clearcoat_normal_map; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().clearcoat_normal_map)>>{};
  else if constexpr (Key::name.view() == std::string_view("clearcoatNormalMapUvSet") && requires(Object& object) { object.clearcoat_normal_map_uv_set; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().clearcoat_normal_map_uv_set)>>{};
  else if constexpr (Key::name.view() == std::string_view("clearcoatNormalScale") && requires(Object& object) { object.clearcoat_normal_scale; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().clearcoat_normal_scale)>>{};
  else if constexpr (Key::name.view() == std::string_view("clearcoatRoughness") && requires(Object& object) { object.clearcoat_roughness; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().clearcoat_roughness)>>{};
  else if constexpr (Key::name.view() == std::string_view("clearcoatRoughnessMap") && requires(Object& object) { object.clearcoat_roughness_map; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().clearcoat_roughness_map)>>{};
  else if constexpr (Key::name.view() == std::string_view("clearcoatRoughnessMapUvSet") && requires(Object& object) { object.clearcoat_roughness_map_uv_set; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().clearcoat_roughness_map_uv_set)>>{};
  else if constexpr (Key::name.view() == std::string_view("clip") && requires(Object& object) { object.clip; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().clip)>>{};
  else if constexpr (Key::name.view() == std::string_view("colliderA") && requires(Object& object) { object.collider_a; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().collider_a)>>{};
  else if constexpr (Key::name.view() == std::string_view("colliderB") && requires(Object& object) { object.collider_b; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().collider_b)>>{};
  else if constexpr (Key::name.view() == std::string_view("color") && requires(Object& object) { object.color; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorCurve") && requires(Object& object) { object.color_curve; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_curve)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorDepth") && requires(Object& object) { object.color_depth; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_depth)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorEndB") && requires(Object& object) { object.color_end_b; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_end_b)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorEndG") && requires(Object& object) { object.color_end_g; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_end_g)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorEndR") && requires(Object& object) { object.color_end_r; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_end_r)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorEndVarianceB") && requires(Object& object) { object.color_end_variance_b; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_end_variance_b)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorEndVarianceG") && requires(Object& object) { object.color_end_variance_g; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_end_variance_g)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorEndVarianceR") && requires(Object& object) { object.color_end_variance_r; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_end_variance_r)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorGamut") && requires(Object& object) { object.color_gamut; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_gamut)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorMatrix") && requires(Object& object) { object.color_matrix; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_matrix)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorScaleBias") && requires(Object& object) { object.color_scale_bias; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_scale_bias)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorStartB") && requires(Object& object) { object.color_start_b; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_start_b)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorStartG") && requires(Object& object) { object.color_start_g; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_start_g)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorStartR") && requires(Object& object) { object.color_start_r; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_start_r)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorStartVarianceB") && requires(Object& object) { object.color_start_variance_b; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_start_variance_b)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorStartVarianceG") && requires(Object& object) { object.color_start_variance_g; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_start_variance_g)>>{};
  else if constexpr (Key::name.view() == std::string_view("colorStartVarianceR") && requires(Object& object) { object.color_start_variance_r; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().color_start_variance_r)>>{};
  else if constexpr (Key::name.view() == std::string_view("commands") && requires(Object& object) { object.commands; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().commands)>>{};
  else if constexpr (Key::name.view() == std::string_view("componentX") && requires(Object& object) { object.component_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().component_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("componentY") && requires(Object& object) { object.component_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().component_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("compression") && requires(Object& object) { object.compression; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().compression)>>{};
  else if constexpr (Key::name.view() == std::string_view("connections") && requires(Object& object) { object.connections; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().connections)>>{};
  else if constexpr (Key::name.view() == std::string_view("contrast") && requires(Object& object) { object.contrast; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().contrast)>>{};
  else if constexpr (Key::name.view() == std::string_view("count") && requires(Object& object) { object.count; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().count)>>{};
  else if constexpr (Key::name.view() == std::string_view("cpuCores") && requires(Object& object) { object.cpu_cores; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().cpu_cores)>>{};
  else if constexpr (Key::name.view() == std::string_view("crop") && requires(Object& object) { object.crop; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().crop)>>{};
  else if constexpr (Key::name.view() == std::string_view("curvature") && requires(Object& object) { object.curvature; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().curvature)>>{};
  else if constexpr (Key::name.view() == std::string_view("d") && requires(Object& object) { object.d; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().d)>>{};
  else if constexpr (Key::name.view() == std::string_view("dampingRatio") && requires(Object& object) { object.damping_ratio; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().damping_ratio)>>{};
  else if constexpr (Key::name.view() == std::string_view("data") && requires(Object& object) { object.data; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().data)>>{};
  else if constexpr (Key::name.view() == std::string_view("deadzoneHalfHeight") && requires(Object& object) { object.deadzone_half_height; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().deadzone_half_height)>>{};
  else if constexpr (Key::name.view() == std::string_view("deadzoneHalfWidth") && requires(Object& object) { object.deadzone_half_width; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().deadzone_half_width)>>{};
  else if constexpr (Key::name.view() == std::string_view("decay") && requires(Object& object) { object.decay; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().decay)>>{};
  else if constexpr (Key::name.view() == std::string_view("declined") && requires(Object& object) { object.declined; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().declined)>>{};
  else if constexpr (Key::name.view() == std::string_view("defaultEase") && requires(Object& object) { object.default_ease; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().default_ease)>>{};
  else if constexpr (Key::name.view() == std::string_view("delay") && requires(Object& object) { object.delay; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().delay)>>{};
  else if constexpr (Key::name.view() == std::string_view("deltaTime") && requires(Object& object) { object.delta_time; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().delta_time)>>{};
  else if constexpr (Key::name.view() == std::string_view("density") && requires(Object& object) { object.density; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().density)>>{};
  else if constexpr (Key::name.view() == std::string_view("densityDpi") && requires(Object& object) { object.density_dpi; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().density_dpi)>>{};
  else if constexpr (Key::name.view() == std::string_view("depth") && requires(Object& object) { object.depth; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().depth)>>{};
  else if constexpr (Key::name.view() == std::string_view("descent") && requires(Object& object) { object.descent; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().descent)>>{};
  else if constexpr (Key::name.view() == std::string_view("destroy") && requires(Object& object) { object.destroy; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().destroy)>>{};
  else if constexpr (Key::name.view() == std::string_view("devicePixelRatio") && requires(Object& object) { object.device_pixel_ratio; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().device_pixel_ratio)>>{};
  else if constexpr (Key::name.view() == std::string_view("direction") && requires(Object& object) { object.direction; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().direction)>>{};
  else if constexpr (Key::name.view() == std::string_view("directionX") && requires(Object& object) { object.direction_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().direction_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("directionY") && requires(Object& object) { object.direction_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().direction_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("directionZ") && requires(Object& object) { object.direction_z; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().direction_z)>>{};
  else if constexpr (Key::name.view() == std::string_view("directional") && requires(Object& object) { object.directional; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().directional)>>{};
  else if constexpr (Key::name.view() == std::string_view("distance") && requires(Object& object) { object.distance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().distance)>>{};
  else if constexpr (Key::name.view() == std::string_view("distro") && requires(Object& object) { object.distro; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().distro)>>{};
  else if constexpr (Key::name.view() == std::string_view("distroVersion") && requires(Object& object) { object.distro_version; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().distro_version)>>{};
  else if constexpr (Key::name.view() == std::string_view("divisor") && requires(Object& object) { object.divisor; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().divisor)>>{};
  else if constexpr (Key::name.view() == std::string_view("duration") && requires(Object& object) { object.duration; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().duration)>>{};
  else if constexpr (Key::name.view() == std::string_view("ease") && requires(Object& object) { object.ease; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().ease)>>{};
  else if constexpr (Key::name.view() == std::string_view("edge") && requires(Object& object) { object.edge; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().edge)>>{};
  else if constexpr (Key::name.view() == std::string_view("edgeMode") && requires(Object& object) { object.edge_mode; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().edge_mode)>>{};
  else if constexpr (Key::name.view() == std::string_view("edgeThreshold") && requires(Object& object) { object.edge_threshold; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().edge_threshold)>>{};
  else if constexpr (Key::name.view() == std::string_view("elapsed") && requires(Object& object) { object.elapsed; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().elapsed)>>{};
  else if constexpr (Key::name.view() == std::string_view("emission") && requires(Object& object) { object.emission; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().emission)>>{};
  else if constexpr (Key::name.view() == std::string_view("emit") && requires(Object& object) { object.emit; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().emit)>>{};
  else if constexpr (Key::name.view() == std::string_view("emitterConeAngle") && requires(Object& object) { object.emitter_cone_angle; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().emitter_cone_angle)>>{};
  else if constexpr (Key::name.view() == std::string_view("emitterDepth") && requires(Object& object) { object.emitter_depth; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().emitter_depth)>>{};
  else if constexpr (Key::name.view() == std::string_view("emitterHeight") && requires(Object& object) { object.emitter_height; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().emitter_height)>>{};
  else if constexpr (Key::name.view() == std::string_view("emitterRadius") && requires(Object& object) { object.emitter_radius; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().emitter_radius)>>{};
  else if constexpr (Key::name.view() == std::string_view("emitterShape") && requires(Object& object) { object.emitter_shape; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().emitter_shape)>>{};
  else if constexpr (Key::name.view() == std::string_view("emitterType") && requires(Object& object) { object.emitter_type; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().emitter_type)>>{};
  else if constexpr (Key::name.view() == std::string_view("emitterWidth") && requires(Object& object) { object.emitter_width; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().emitter_width)>>{};
  else if constexpr (Key::name.view() == std::string_view("enabled") && requires(Object& object) { object.enabled; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().enabled)>>{};
  else if constexpr (Key::name.view() == std::string_view("encoding") && requires(Object& object) { object.encoding; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().encoding)>>{};
  else if constexpr (Key::name.view() == std::string_view("end") && requires(Object& object) { object.end; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().end)>>{};
  else if constexpr (Key::name.view() == std::string_view("endIndex") && requires(Object& object) { object.end_index; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().end_index)>>{};
  else if constexpr (Key::name.view() == std::string_view("endX") && requires(Object& object) { object.end_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().end_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("endY") && requires(Object& object) { object.end_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().end_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("endZ") && requires(Object& object) { object.end_z; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().end_z)>>{};
  else if constexpr (Key::name.view() == std::string_view("endianness") && requires(Object& object) { object.endianness; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().endianness)>>{};
  else if constexpr (Key::name.view() == std::string_view("engine") && requires(Object& object) { object.engine; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().engine)>>{};
  else if constexpr (Key::name.view() == std::string_view("engineVersion") && requires(Object& object) { object.engine_version; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().engine_version)>>{};
  else if constexpr (Key::name.view() == std::string_view("explainSpatialIndexing") && requires(Object& object) { object.explain_spatial_indexing; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().explain_spatial_indexing)>>{};
  else if constexpr (Key::name.view() == std::string_view("exposure") && requires(Object& object) { object.exposure; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().exposure)>>{};
  else if constexpr (Key::name.view() == std::string_view("exposureCompensation") && requires(Object& object) { object.exposure_compensation; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().exposure_compensation)>>{};
  else if constexpr (Key::name.view() == std::string_view("far") && requires(Object& object) { object.far; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().far)>>{};
  else if constexpr (Key::name.view() == std::string_view("featureId") && requires(Object& object) { object.feature_id; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().feature_id)>>{};
  else if constexpr (Key::name.view() == std::string_view("feedback") && requires(Object& object) { object.feedback; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().feedback)>>{};
  else if constexpr (Key::name.view() == std::string_view("fillBounds") && requires(Object& object) { object.fill_bounds; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().fill_bounds)>>{};
  else if constexpr (Key::name.view() == std::string_view("fillColor") && requires(Object& object) { object.fill_color; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().fill_color)>>{};
  else if constexpr (Key::name.view() == std::string_view("finishColor") && requires(Object& object) { object.finish_color; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().finish_color)>>{};
  else if constexpr (Key::name.view() == std::string_view("finishColorVariance") && requires(Object& object) { object.finish_color_variance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().finish_color_variance)>>{};
  else if constexpr (Key::name.view() == std::string_view("finishParticleSize") && requires(Object& object) { object.finish_particle_size; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().finish_particle_size)>>{};
  else if constexpr (Key::name.view() == std::string_view("finishParticleSizeVariance") && requires(Object& object) { object.finish_particle_size_variance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().finish_particle_size_variance)>>{};
  else if constexpr (Key::name.view() == std::string_view("floorLevel") && requires(Object& object) { object.floor_level; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().floor_level)>>{};
  else if constexpr (Key::name.view() == std::string_view("fontScale") && requires(Object& object) { object.font_scale; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().font_scale)>>{};
  else if constexpr (Key::name.view() == std::string_view("forceX") && requires(Object& object) { object.force_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().force_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("forceY") && requires(Object& object) { object.force_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().force_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("forceZ") && requires(Object& object) { object.force_z; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().force_z)>>{};
  else if constexpr (Key::name.view() == std::string_view("formFactor") && requires(Object& object) { object.form_factor; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().form_factor)>>{};
  else if constexpr (Key::name.view() == std::string_view("format") && requires(Object& object) { object.format; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().format)>>{};
  else if constexpr (Key::name.view() == std::string_view("fovY") && requires(Object& object) { object.fov_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().fov_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("fraction") && requires(Object& object) { object.fraction; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().fraction)>>{};
  else if constexpr (Key::name.view() == std::string_view("frameCount") && requires(Object& object) { object.frame_count; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().frame_count)>>{};
  else if constexpr (Key::name.view() == std::string_view("frameDuration") && requires(Object& object) { object.frame_duration; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().frame_duration)>>{};
  else if constexpr (Key::name.view() == std::string_view("frameDurations") && requires(Object& object) { object.frame_durations; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().frame_durations)>>{};
  else if constexpr (Key::name.view() == std::string_view("frameId") && requires(Object& object) { object.frame_id; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().frame_id)>>{};
  else if constexpr (Key::name.view() == std::string_view("frameNames") && requires(Object& object) { object.frame_names; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().frame_names)>>{};
  else if constexpr (Key::name.view() == std::string_view("frameRate") && requires(Object& object) { object.frame_rate; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().frame_rate)>>{};
  else if constexpr (Key::name.view() == std::string_view("frames") && requires(Object& object) { object.frames; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().frames)>>{};
  else if constexpr (Key::name.view() == std::string_view("frequency") && requires(Object& object) { object.frequency; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().frequency)>>{};
  else if constexpr (Key::name.view() == std::string_view("friction") && requires(Object& object) { object.friction; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().friction)>>{};
  else if constexpr (Key::name.view() == std::string_view("gain") && requires(Object& object) { object.gain; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().gain)>>{};
  else if constexpr (Key::name.view() == std::string_view("gamma") && requires(Object& object) { object.gamma; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().gamma)>>{};
  else if constexpr (Key::name.view() == std::string_view("gateWeave") && requires(Object& object) { object.gate_weave; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().gate_weave)>>{};
  else if constexpr (Key::name.view() == std::string_view("getCapabilities") && requires(Object& object) { object.get_capabilities; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().get_capabilities)>>{};
  else if constexpr (Key::name.view() == std::string_view("getCurrentPosition") && requires(Object& object) { object.get_current_position; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().get_current_position)>>{};
  else if constexpr (Key::name.view() == std::string_view("getCurrentPositionResult") && requires(Object& object) { object.get_current_position_result; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().get_current_position_result)>>{};
  else if constexpr (Key::name.view() == std::string_view("getDisplayMetrics") && requires(Object& object) { object.get_display_metrics; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().get_display_metrics)>>{};
  else if constexpr (Key::name.view() == std::string_view("getFormats") && requires(Object& object) { object.get_formats; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().get_formats)>>{};
  else if constexpr (Key::name.view() == std::string_view("getGlyphAtlasImage") && requires(Object& object) { object.get_glyph_atlas_image; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().get_glyph_atlas_image)>>{};
  else if constexpr (Key::name.view() == std::string_view("getGlyphEntry") && requires(Object& object) { object.get_glyph_entry; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().get_glyph_entry)>>{};
  else if constexpr (Key::name.view() == std::string_view("getGlyphKerning") && requires(Object& object) { object.get_glyph_kerning; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().get_glyph_kerning)>>{};
  else if constexpr (Key::name.view() == std::string_view("getGlyphLayoutVersion") && requires(Object& object) { object.get_glyph_layout_version; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().get_glyph_layout_version)>>{};
  else if constexpr (Key::name.view() == std::string_view("getGlyphMetrics") && requires(Object& object) { object.get_glyph_metrics; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().get_glyph_metrics)>>{};
  else if constexpr (Key::name.view() == std::string_view("getId") && requires(Object& object) { object.get_id; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().get_id)>>{};
  else if constexpr (Key::name.view() == std::string_view("getInfo") && requires(Object& object) { object.get_info; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().get_info)>>{};
  else if constexpr (Key::name.view() == std::string_view("getPermission") && requires(Object& object) { object.get_permission; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().get_permission)>>{};
  else if constexpr (Key::name.view() == std::string_view("getPermissionState") && requires(Object& object) { object.get_permission_state; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().get_permission_state)>>{};
  else if constexpr (Key::name.view() == std::string_view("getPersistence") && requires(Object& object) { object.get_persistence; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().get_persistence)>>{};
  else if constexpr (Key::name.view() == std::string_view("getSafeAreaInsets") && requires(Object& object) { object.get_safe_area_insets; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().get_safe_area_insets)>>{};
  else if constexpr (Key::name.view() == std::string_view("ghosts") && requires(Object& object) { object.ghosts; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().ghosts)>>{};
  else if constexpr (Key::name.view() == std::string_view("glyphs") && requires(Object& object) { object.glyphs; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().glyphs)>>{};
  else if constexpr (Key::name.view() == std::string_view("gpuRenderer") && requires(Object& object) { object.gpu_renderer; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().gpu_renderer)>>{};
  else if constexpr (Key::name.view() == std::string_view("gpuVendor") && requires(Object& object) { object.gpu_vendor; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().gpu_vendor)>>{};
  else if constexpr (Key::name.view() == std::string_view("grainIntensity") && requires(Object& object) { object.grain_intensity; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().grain_intensity)>>{};
  else if constexpr (Key::name.view() == std::string_view("gravity") && requires(Object& object) { object.gravity; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().gravity)>>{};
  else if constexpr (Key::name.view() == std::string_view("gravityX") && requires(Object& object) { object.gravity_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().gravity_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("gravityY") && requires(Object& object) { object.gravity_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().gravity_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("gravityZ") && requires(Object& object) { object.gravity_z; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().gravity_z)>>{};
  else if constexpr (Key::name.view() == std::string_view("gravityx") && requires(Object& object) { object.gravityx; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().gravityx)>>{};
  else if constexpr (Key::name.view() == std::string_view("gravityy") && requires(Object& object) { object.gravityy; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().gravityy)>>{};
  else if constexpr (Key::name.view() == std::string_view("green") && requires(Object& object) { object.green; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().green)>>{};
  else if constexpr (Key::name.view() == std::string_view("greenBias") && requires(Object& object) { object.green_bias; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().green_bias)>>{};
  else if constexpr (Key::name.view() == std::string_view("greenScale") && requires(Object& object) { object.green_scale; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().green_scale)>>{};
  else if constexpr (Key::name.view() == std::string_view("groundColor") && requires(Object& object) { object.ground_color; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().ground_color)>>{};
  else if constexpr (Key::name.view() == std::string_view("halationRadius") && requires(Object& object) { object.halation_radius; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().halation_radius)>>{};
  else if constexpr (Key::name.view() == std::string_view("halationStrength") && requires(Object& object) { object.halation_strength; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().halation_strength)>>{};
  else if constexpr (Key::name.view() == std::string_view("halfExtentX") && requires(Object& object) { object.half_extent_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().half_extent_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("halfExtentY") && requires(Object& object) { object.half_extent_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().half_extent_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("halfExtentZ") && requires(Object& object) { object.half_extent_z; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().half_extent_z)>>{};
  else if constexpr (Key::name.view() == std::string_view("halfH") && requires(Object& object) { object.half_h; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().half_h)>>{};
  else if constexpr (Key::name.view() == std::string_view("halfW") && requires(Object& object) { object.half_w; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().half_w)>>{};
  else if constexpr (Key::name.view() == std::string_view("halo") && requires(Object& object) { object.halo; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().halo)>>{};
  else if constexpr (Key::name.view() == std::string_view("handle") && requires(Object& object) { object.handle; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().handle)>>{};
  else if constexpr (Key::name.view() == std::string_view("hasFormat") && requires(Object& object) { object.has_format; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().has_format)>>{};
  else if constexpr (Key::name.view() == std::string_view("hasImage") && requires(Object& object) { object.has_image; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().has_image)>>{};
  else if constexpr (Key::name.view() == std::string_view("hasKeyboard") && requires(Object& object) { object.has_keyboard; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().has_keyboard)>>{};
  else if constexpr (Key::name.view() == std::string_view("hasMouse") && requires(Object& object) { object.has_mouse; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().has_mouse)>>{};
  else if constexpr (Key::name.view() == std::string_view("hasStylus") && requires(Object& object) { object.has_stylus; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().has_stylus)>>{};
  else if constexpr (Key::name.view() == std::string_view("hasText") && requires(Object& object) { object.has_text; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().has_text)>>{};
  else if constexpr (Key::name.view() == std::string_view("heading") && requires(Object& object) { object.heading; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().heading)>>{};
  else if constexpr (Key::name.view() == std::string_view("height") && requires(Object& object) { object.height; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().height)>>{};
  else if constexpr (Key::name.view() == std::string_view("hemisphere") && requires(Object& object) { object.hemisphere; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().hemisphere)>>{};
  else if constexpr (Key::name.view() == std::string_view("hide") && requires(Object& object) { object.hide; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().hide)>>{};
  else if constexpr (Key::name.view() == std::string_view("highMax") && requires(Object& object) { object.high_max; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().high_max)>>{};
  else if constexpr (Key::name.view() == std::string_view("highMin") && requires(Object& object) { object.high_min; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().high_min)>>{};
  else if constexpr (Key::name.view() == std::string_view("hue") && requires(Object& object) { object.hue; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().hue)>>{};
  else if constexpr (Key::name.view() == std::string_view("id") && requires(Object& object) { object.id; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().id)>>{};
  else if constexpr (Key::name.view() == std::string_view("illuminance") && requires(Object& object) { object.illuminance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().illuminance)>>{};
  else if constexpr (Key::name.view() == std::string_view("imageCount") && requires(Object& object) { object.image_count; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().image_count)>>{};
  else if constexpr (Key::name.view() == std::string_view("imageFile") && requires(Object& object) { object.image_file; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().image_file)>>{};
  else if constexpr (Key::name.view() == std::string_view("imageHeight") && requires(Object& object) { object.image_height; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().image_height)>>{};
  else if constexpr (Key::name.view() == std::string_view("imagePath") && requires(Object& object) { object.image_path; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().image_path)>>{};
  else if constexpr (Key::name.view() == std::string_view("imageWidth") && requires(Object& object) { object.image_width; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().image_width)>>{};
  else if constexpr (Key::name.view() == std::string_view("impact") && requires(Object& object) { object.impact; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().impact)>>{};
  else if constexpr (Key::name.view() == std::string_view("influenceCounts") && requires(Object& object) { object.influence_counts; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().influence_counts)>>{};
  else if constexpr (Key::name.view() == std::string_view("influences") && requires(Object& object) { object.influences; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().influences)>>{};
  else if constexpr (Key::name.view() == std::string_view("innerConeCos") && requires(Object& object) { object.inner_cone_cos; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().inner_cone_cos)>>{};
  else if constexpr (Key::name.view() == std::string_view("innerConeDegrees") && requires(Object& object) { object.inner_cone_degrees; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().inner_cone_degrees)>>{};
  else if constexpr (Key::name.view() == std::string_view("insertSpatialObject") && requires(Object& object) { object.insert_spatial_object; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().insert_spatial_object)>>{};
  else if constexpr (Key::name.view() == std::string_view("intensity") && requires(Object& object) { object.intensity; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().intensity)>>{};
  else if constexpr (Key::name.view() == std::string_view("intensityUnit") && requires(Object& object) { object.intensity_unit; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().intensity_unit)>>{};
  else if constexpr (Key::name.view() == std::string_view("interval") && requires(Object& object) { object.interval; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().interval)>>{};
  else if constexpr (Key::name.view() == std::string_view("invoke") && requires(Object& object) { object.invoke; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().invoke)>>{};
  else if constexpr (Key::name.view() == std::string_view("ior") && requires(Object& object) { object.ior; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().ior)>>{};
  else if constexpr (Key::name.view() == std::string_view("iridescence") && requires(Object& object) { object.iridescence; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().iridescence)>>{};
  else if constexpr (Key::name.view() == std::string_view("iridescenceIor") && requires(Object& object) { object.iridescence_ior; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().iridescence_ior)>>{};
  else if constexpr (Key::name.view() == std::string_view("iridescenceMap") && requires(Object& object) { object.iridescence_map; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().iridescence_map)>>{};
  else if constexpr (Key::name.view() == std::string_view("iridescenceMapUvSet") && requires(Object& object) { object.iridescence_map_uv_set; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().iridescence_map_uv_set)>>{};
  else if constexpr (Key::name.view() == std::string_view("iridescenceThicknessMap") && requires(Object& object) { object.iridescence_thickness_map; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().iridescence_thickness_map)>>{};
  else if constexpr (Key::name.view() == std::string_view("iridescenceThicknessMapUvSet") && requires(Object& object) { object.iridescence_thickness_map_uv_set; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().iridescence_thickness_map_uv_set)>>{};
  else if constexpr (Key::name.view() == std::string_view("iridescenceThicknessMax") && requires(Object& object) { object.iridescence_thickness_max; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().iridescence_thickness_max)>>{};
  else if constexpr (Key::name.view() == std::string_view("iridescenceThicknessMin") && requires(Object& object) { object.iridescence_thickness_min; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().iridescence_thickness_min)>>{};
  else if constexpr (Key::name.view() == std::string_view("isAmbientLightSupported") && requires(Object& object) { object.is_ambient_light_supported; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_ambient_light_supported)>>{};
  else if constexpr (Key::name.view() == std::string_view("isAvailable") && requires(Object& object) { object.is_available; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_available)>>{};
  else if constexpr (Key::name.view() == std::string_view("isBarometerSupported") && requires(Object& object) { object.is_barometer_supported; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_barometer_supported)>>{};
  else if constexpr (Key::name.view() == std::string_view("isGravitySupported") && requires(Object& object) { object.is_gravity_supported; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_gravity_supported)>>{};
  else if constexpr (Key::name.view() == std::string_view("isGyroscopeSupported") && requires(Object& object) { object.is_gyroscope_supported; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_gyroscope_supported)>>{};
  else if constexpr (Key::name.view() == std::string_view("isHdr") && requires(Object& object) { object.is_hdr; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_hdr)>>{};
  else if constexpr (Key::name.view() == std::string_view("isJailbroken") && requires(Object& object) { object.is_jailbroken; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_jailbroken)>>{};
  else if constexpr (Key::name.view() == std::string_view("isLinearAccelerationSupported") && requires(Object& object) { object.is_linear_acceleration_supported; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_linear_acceleration_supported)>>{};
  else if constexpr (Key::name.view() == std::string_view("isLowEndDevice") && requires(Object& object) { object.is_low_end_device; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_low_end_device)>>{};
  else if constexpr (Key::name.view() == std::string_view("isMagnetometerSupported") && requires(Object& object) { object.is_magnetometer_supported; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_magnetometer_supported)>>{};
  else if constexpr (Key::name.view() == std::string_view("isMotionSupported") && requires(Object& object) { object.is_motion_supported; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_motion_supported)>>{};
  else if constexpr (Key::name.view() == std::string_view("isOrientationSupported") && requires(Object& object) { object.is_orientation_supported; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_orientation_supported)>>{};
  else if constexpr (Key::name.view() == std::string_view("isProximitySupported") && requires(Object& object) { object.is_proximity_supported; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_proximity_supported)>>{};
  else if constexpr (Key::name.view() == std::string_view("isRooted") && requires(Object& object) { object.is_rooted; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_rooted)>>{};
  else if constexpr (Key::name.view() == std::string_view("isSupported") && requires(Object& object) { object.is_supported; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_supported)>>{};
  else if constexpr (Key::name.view() == std::string_view("isTouch") && requires(Object& object) { object.is_touch; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_touch)>>{};
  else if constexpr (Key::name.view() == std::string_view("isVirtual") && requires(Object& object) { object.is_virtual; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().is_virtual)>>{};
  else if constexpr (Key::name.view() == std::string_view("jointCollisionSuppressions") && requires(Object& object) { object.joint_collision_suppressions; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().joint_collision_suppressions)>>{};
  else if constexpr (Key::name.view() == std::string_view("jointSolvers") && requires(Object& object) { object.joint_solvers; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().joint_solvers)>>{};
  else if constexpr (Key::name.view() == std::string_view("kerning") && requires(Object& object) { object.kerning; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().kerning)>>{};
  else if constexpr (Key::name.view() == std::string_view("key") && requires(Object& object) { object.key; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().key)>>{};
  else if constexpr (Key::name.view() == std::string_view("kind") && requires(Object& object) { object.kind; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().kind)>>{};
  else if constexpr (Key::name.view() == std::string_view("latitude") && requires(Object& object) { object.latitude; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().latitude)>>{};
  else if constexpr (Key::name.view() == std::string_view("layerMask") && requires(Object& object) { object.layer_mask; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().layer_mask)>>{};
  else if constexpr (Key::name.view() == std::string_view("leading") && requires(Object& object) { object.leading; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().leading)>>{};
  else if constexpr (Key::name.view() == std::string_view("leafByObject") && requires(Object& object) { object.leaf_by_object; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().leaf_by_object)>>{};
  else if constexpr (Key::name.view() == std::string_view("left") && requires(Object& object) { object.left; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().left)>>{};
  else if constexpr (Key::name.view() == std::string_view("levels") && requires(Object& object) { object.levels; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().levels)>>{};
  else if constexpr (Key::name.view() == std::string_view("life") && requires(Object& object) { object.life; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().life)>>{};
  else if constexpr (Key::name.view() == std::string_view("lifeOffset") && requires(Object& object) { object.life_offset; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().life_offset)>>{};
  else if constexpr (Key::name.view() == std::string_view("lifetimeMax") && requires(Object& object) { object.lifetime_max; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().lifetime_max)>>{};
  else if constexpr (Key::name.view() == std::string_view("lifetimeMin") && requires(Object& object) { object.lifetime_min; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().lifetime_min)>>{};
  else if constexpr (Key::name.view() == std::string_view("lift") && requires(Object& object) { object.lift; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().lift)>>{};
  else if constexpr (Key::name.view() == std::string_view("lightColor") && requires(Object& object) { object.light_color; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().light_color)>>{};
  else if constexpr (Key::name.view() == std::string_view("lightX") && requires(Object& object) { object.light_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().light_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("lightY") && requires(Object& object) { object.light_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().light_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("lightness") && requires(Object& object) { object.lightness; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().lightness)>>{};
  else if constexpr (Key::name.view() == std::string_view("lineIndex") && requires(Object& object) { object.line_index; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().line_index)>>{};
  else if constexpr (Key::name.view() == std::string_view("linearLength") && requires(Object& object) { object.linear_length; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().linear_length)>>{};
  else if constexpr (Key::name.view() == std::string_view("linearStart") && requires(Object& object) { object.linear_start; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().linear_start)>>{};
  else if constexpr (Key::name.view() == std::string_view("locale") && requires(Object& object) { object.locale; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().locale)>>{};
  else if constexpr (Key::name.view() == std::string_view("logicalHeight") && requires(Object& object) { object.logical_height; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().logical_height)>>{};
  else if constexpr (Key::name.view() == std::string_view("logicalWidth") && requires(Object& object) { object.logical_width; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().logical_width)>>{};
  else if constexpr (Key::name.view() == std::string_view("longitude") && requires(Object& object) { object.longitude; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().longitude)>>{};
  else if constexpr (Key::name.view() == std::string_view("loop") && requires(Object& object) { object.loop; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().loop)>>{};
  else if constexpr (Key::name.view() == std::string_view("lowMax") && requires(Object& object) { object.low_max; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().low_max)>>{};
  else if constexpr (Key::name.view() == std::string_view("lowMin") && requires(Object& object) { object.low_min; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().low_min)>>{};
  else if constexpr (Key::name.view() == std::string_view("lut") && requires(Object& object) { object.lut; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().lut)>>{};
  else if constexpr (Key::name.view() == std::string_view("m") && requires(Object& object) { object.m; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().m)>>{};
  else if constexpr (Key::name.view() == std::string_view("magFilter") && requires(Object& object) { object.mag_filter; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().mag_filter)>>{};
  else if constexpr (Key::name.view() == std::string_view("manufacturer") && requires(Object& object) { object.manufacturer; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().manufacturer)>>{};
  else if constexpr (Key::name.view() == std::string_view("map") && requires(Object& object) { object.map; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().map)>>{};
  else if constexpr (Key::name.view() == std::string_view("margin") && requires(Object& object) { object.margin; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().margin)>>{};
  else if constexpr (Key::name.view() == std::string_view("marketingName") && requires(Object& object) { object.marketing_name; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().marketing_name)>>{};
  else if constexpr (Key::name.view() == std::string_view("material") && requires(Object& object) { object.material; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().material)>>{};
  else if constexpr (Key::name.view() == std::string_view("materialData") && requires(Object& object) { object.material_data; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().material_data)>>{};
  else if constexpr (Key::name.view() == std::string_view("matrix") && requires(Object& object) { object.matrix; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().matrix)>>{};
  else if constexpr (Key::name.view() == std::string_view("matrixX") && requires(Object& object) { object.matrix_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().matrix_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("matrixY") && requires(Object& object) { object.matrix_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().matrix_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("max") && requires(Object& object) { object.max; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().max)>>{};
  else if constexpr (Key::name.view() == std::string_view("maxBrightness") && requires(Object& object) { object.max_brightness; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().max_brightness)>>{};
  else if constexpr (Key::name.view() == std::string_view("maxDistance") && requires(Object& object) { object.max_distance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().max_distance)>>{};
  else if constexpr (Key::name.view() == std::string_view("maxEv") && requires(Object& object) { object.max_ev; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().max_ev)>>{};
  else if constexpr (Key::name.view() == std::string_view("maxExposure") && requires(Object& object) { object.max_exposure; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().max_exposure)>>{};
  else if constexpr (Key::name.view() == std::string_view("maxParticleCount") && requires(Object& object) { object.max_particle_count; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().max_particle_count)>>{};
  else if constexpr (Key::name.view() == std::string_view("maxParticles") && requires(Object& object) { object.max_particles; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().max_particles)>>{};
  else if constexpr (Key::name.view() == std::string_view("maxRadius") && requires(Object& object) { object.max_radius; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().max_radius)>>{};
  else if constexpr (Key::name.view() == std::string_view("maxRadiusVariance") && requires(Object& object) { object.max_radius_variance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().max_radius_variance)>>{};
  else if constexpr (Key::name.view() == std::string_view("maxX") && requires(Object& object) { object.max_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().max_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("maxY") && requires(Object& object) { object.max_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().max_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("maxZ") && requires(Object& object) { object.max_z; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().max_z)>>{};
  else if constexpr (Key::name.view() == std::string_view("metadata") && requires(Object& object) { object.metadata; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().metadata)>>{};
  else if constexpr (Key::name.view() == std::string_view("metrics") && requires(Object& object) { object.metrics; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().metrics)>>{};
  else if constexpr (Key::name.view() == std::string_view("min") && requires(Object& object) { object.min; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().min)>>{};
  else if constexpr (Key::name.view() == std::string_view("minEv") && requires(Object& object) { object.min_ev; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().min_ev)>>{};
  else if constexpr (Key::name.view() == std::string_view("minExposure") && requires(Object& object) { object.min_exposure; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().min_exposure)>>{};
  else if constexpr (Key::name.view() == std::string_view("minFilter") && requires(Object& object) { object.min_filter; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().min_filter)>>{};
  else if constexpr (Key::name.view() == std::string_view("minParticleCount") && requires(Object& object) { object.min_particle_count; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().min_particle_count)>>{};
  else if constexpr (Key::name.view() == std::string_view("minRadius") && requires(Object& object) { object.min_radius; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().min_radius)>>{};
  else if constexpr (Key::name.view() == std::string_view("minRadiusVariance") && requires(Object& object) { object.min_radius_variance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().min_radius_variance)>>{};
  else if constexpr (Key::name.view() == std::string_view("minX") && requires(Object& object) { object.min_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().min_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("minY") && requires(Object& object) { object.min_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().min_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("minZ") && requires(Object& object) { object.min_z; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().min_z)>>{};
  else if constexpr (Key::name.view() == std::string_view("mipmaps") && requires(Object& object) { object.mipmaps; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().mipmaps)>>{};
  else if constexpr (Key::name.view() == std::string_view("mode") && requires(Object& object) { object.mode; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().mode)>>{};
  else if constexpr (Key::name.view() == std::string_view("model") && requires(Object& object) { object.model; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().model)>>{};
  else if constexpr (Key::name.view() == std::string_view("modifiers") && requires(Object& object) { object.modifiers; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().modifiers)>>{};
  else if constexpr (Key::name.view() == std::string_view("name") && requires(Object& object) { object.name; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().name)>>{};
  else if constexpr (Key::name.view() == std::string_view("near") && requires(Object& object) { object.near; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().near)>>{};
  else if constexpr (Key::name.view() == std::string_view("normalBias") && requires(Object& object) { object.normal_bias; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().normal_bias)>>{};
  else if constexpr (Key::name.view() == std::string_view("normalX") && requires(Object& object) { object.normal_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().normal_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("normalY") && requires(Object& object) { object.normal_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().normal_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("normalZ") && requires(Object& object) { object.normal_z; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().normal_z)>>{};
  else if constexpr (Key::name.view() == std::string_view("notification") && requires(Object& object) { object.notification; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().notification)>>{};
  else if constexpr (Key::name.view() == std::string_view("numLines") && requires(Object& object) { object.num_lines; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().num_lines)>>{};
  else if constexpr (Key::name.view() == std::string_view("object") && requires(Object& object) { object.object; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().object)>>{};
  else if constexpr (Key::name.view() == std::string_view("offset") && requires(Object& object) { object.offset; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().offset)>>{};
  else if constexpr (Key::name.view() == std::string_view("offsetX") && requires(Object& object) { object.offset_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().offset_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("offsetY") && requires(Object& object) { object.offset_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().offset_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("offsets") && requires(Object& object) { object.offsets; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().offsets)>>{};
  else if constexpr (Key::name.view() == std::string_view("onAbsoluteOrientation") && requires(Object& object) { object.on_absolute_orientation; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_absolute_orientation)>>{};
  else if constexpr (Key::name.view() == std::string_view("onAccelerometer") && requires(Object& object) { object.on_accelerometer; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_accelerometer)>>{};
  else if constexpr (Key::name.view() == std::string_view("onAction") && requires(Object& object) { object.on_action; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_action)>>{};
  else if constexpr (Key::name.view() == std::string_view("onAmbientLight") && requires(Object& object) { object.on_ambient_light; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_ambient_light)>>{};
  else if constexpr (Key::name.view() == std::string_view("onBarometer") && requires(Object& object) { object.on_barometer; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_barometer)>>{};
  else if constexpr (Key::name.view() == std::string_view("onChange") && requires(Object& object) { object.on_change; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_change)>>{};
  else if constexpr (Key::name.view() == std::string_view("onEmitterComplete") && requires(Object& object) { object.on_emitter_complete; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_emitter_complete)>>{};
  else if constexpr (Key::name.view() == std::string_view("onGravity") && requires(Object& object) { object.on_gravity; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_gravity)>>{};
  else if constexpr (Key::name.view() == std::string_view("onGyroscope") && requires(Object& object) { object.on_gyroscope; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_gyroscope)>>{};
  else if constexpr (Key::name.view() == std::string_view("onHide") && requires(Object& object) { object.on_hide; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_hide)>>{};
  else if constexpr (Key::name.view() == std::string_view("onLinearAcceleration") && requires(Object& object) { object.on_linear_acceleration; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_linear_acceleration)>>{};
  else if constexpr (Key::name.view() == std::string_view("onMagnetometer") && requires(Object& object) { object.on_magnetometer; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_magnetometer)>>{};
  else if constexpr (Key::name.view() == std::string_view("onOrientation") && requires(Object& object) { object.on_orientation; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_orientation)>>{};
  else if constexpr (Key::name.view() == std::string_view("onParticleDeath") && requires(Object& object) { object.on_particle_death; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_particle_death)>>{};
  else if constexpr (Key::name.view() == std::string_view("onParticleSpawn") && requires(Object& object) { object.on_particle_spawn; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_particle_spawn)>>{};
  else if constexpr (Key::name.view() == std::string_view("onProximity") && requires(Object& object) { object.on_proximity; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_proximity)>>{};
  else if constexpr (Key::name.view() == std::string_view("onQuaternion") && requires(Object& object) { object.on_quaternion; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_quaternion)>>{};
  else if constexpr (Key::name.view() == std::string_view("onResize") && requires(Object& object) { object.on_resize; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_resize)>>{};
  else if constexpr (Key::name.view() == std::string_view("onShow") && requires(Object& object) { object.on_show; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_show)>>{};
  else if constexpr (Key::name.view() == std::string_view("onTick") && requires(Object& object) { object.on_tick; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().on_tick)>>{};
  else if constexpr (Key::name.view() == std::string_view("once") && requires(Object& object) { object.once; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().once)>>{};
  else if constexpr (Key::name.view() == std::string_view("operation") && requires(Object& object) { object.operation; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().operation)>>{};
  else if constexpr (Key::name.view() == std::string_view("operator") && requires(Object& object) { object.operator_; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().operator_)>>{};
  else if constexpr (Key::name.view() == std::string_view("orientationW") && requires(Object& object) { object.orientation_w; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().orientation_w)>>{};
  else if constexpr (Key::name.view() == std::string_view("orientationX") && requires(Object& object) { object.orientation_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().orientation_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("orientationY") && requires(Object& object) { object.orientation_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().orientation_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("orientationZ") && requires(Object& object) { object.orientation_z; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().orientation_z)>>{};
  else if constexpr (Key::name.view() == std::string_view("origin") && requires(Object& object) { object.origin; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().origin)>>{};
  else if constexpr (Key::name.view() == std::string_view("originX") && requires(Object& object) { object.origin_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().origin_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("originY") && requires(Object& object) { object.origin_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().origin_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("osBuild") && requires(Object& object) { object.os_build; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().os_build)>>{};
  else if constexpr (Key::name.view() == std::string_view("osName") && requires(Object& object) { object.os_name; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().os_name)>>{};
  else if constexpr (Key::name.view() == std::string_view("osVersion") && requires(Object& object) { object.os_version; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().os_version)>>{};
  else if constexpr (Key::name.view() == std::string_view("outerConeCos") && requires(Object& object) { object.outer_cone_cos; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().outer_cone_cos)>>{};
  else if constexpr (Key::name.view() == std::string_view("outerConeDegrees") && requires(Object& object) { object.outer_cone_degrees; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().outer_cone_degrees)>>{};
  else if constexpr (Key::name.view() == std::string_view("overlapping") && requires(Object& object) { object.overlapping; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().overlapping)>>{};
  else if constexpr (Key::name.view() == std::string_view("overlaysContent") && requires(Object& object) { object.overlays_content; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().overlays_content)>>{};
  else if constexpr (Key::name.view() == std::string_view("pages") && requires(Object& object) { object.pages; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().pages)>>{};
  else if constexpr (Key::name.view() == std::string_view("parent") && requires(Object& object) { object.parent; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().parent)>>{};
  else if constexpr (Key::name.view() == std::string_view("particleLifespan") && requires(Object& object) { object.particle_lifespan; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().particle_lifespan)>>{};
  else if constexpr (Key::name.view() == std::string_view("particleLifespanVariance") && requires(Object& object) { object.particle_lifespan_variance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().particle_lifespan_variance)>>{};
  else if constexpr (Key::name.view() == std::string_view("paused") && requires(Object& object) { object.paused; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().paused)>>{};
  else if constexpr (Key::name.view() == std::string_view("pcfRadius") && requires(Object& object) { object.pcf_radius; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().pcf_radius)>>{};
  else if constexpr (Key::name.view() == std::string_view("pedestal") && requires(Object& object) { object.pedestal; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().pedestal)>>{};
  else if constexpr (Key::name.view() == std::string_view("physicalHeight") && requires(Object& object) { object.physical_height; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().physical_height)>>{};
  else if constexpr (Key::name.view() == std::string_view("physicalWidth") && requires(Object& object) { object.physical_width; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().physical_width)>>{};
  else if constexpr (Key::name.view() == std::string_view("pivotX") && requires(Object& object) { object.pivot_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().pivot_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("pivotY") && requires(Object& object) { object.pivot_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().pivot_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("pixelRatio") && requires(Object& object) { object.pixel_ratio; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().pixel_ratio)>>{};
  else if constexpr (Key::name.view() == std::string_view("platformString") && requires(Object& object) { object.platform_string; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().platform_string)>>{};
  else if constexpr (Key::name.view() == std::string_view("playbackRate") && requires(Object& object) { object.playback_rate; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().playback_rate)>>{};
  else if constexpr (Key::name.view() == std::string_view("point") && requires(Object& object) { object.point; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().point)>>{};
  else if constexpr (Key::name.view() == std::string_view("pointCount") && requires(Object& object) { object.point_count; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().point_count)>>{};
  else if constexpr (Key::name.view() == std::string_view("pointerWidth") && requires(Object& object) { object.pointer_width; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().pointer_width)>>{};
  else if constexpr (Key::name.view() == std::string_view("points") && requires(Object& object) { object.points; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().points)>>{};
  else if constexpr (Key::name.view() == std::string_view("position") && requires(Object& object) { object.position; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().position)>>{};
  else if constexpr (Key::name.view() == std::string_view("positions") && requires(Object& object) { object.positions; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().positions)>>{};
  else if constexpr (Key::name.view() == std::string_view("premultipliedAlpha") && requires(Object& object) { object.premultiplied_alpha; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().premultiplied_alpha)>>{};
  else if constexpr (Key::name.view() == std::string_view("prepare") && requires(Object& object) { object.prepare; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().prepare)>>{};
  else if constexpr (Key::name.view() == std::string_view("preserveAlpha") && requires(Object& object) { object.preserve_alpha; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().preserve_alpha)>>{};
  else if constexpr (Key::name.view() == std::string_view("pressure") && requires(Object& object) { object.pressure; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().pressure)>>{};
  else if constexpr (Key::name.view() == std::string_view("previousWorldTransform") && requires(Object& object) { object.previous_world_transform; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().previous_world_transform)>>{};
  else if constexpr (Key::name.view() == std::string_view("priority") && requires(Object& object) { object.priority; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().priority)>>{};
  else if constexpr (Key::name.view() == std::string_view("productName") && requires(Object& object) { object.product_name; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().product_name)>>{};
  else if constexpr (Key::name.view() == std::string_view("projection") && requires(Object& object) { object.projection; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().projection)>>{};
  else if constexpr (Key::name.view() == std::string_view("promptForAccess") && requires(Object& object) { object.prompt_for_access; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().prompt_for_access)>>{};
  else if constexpr (Key::name.view() == std::string_view("querySpatialPairs") && requires(Object& object) { object.query_spatial_pairs; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().query_spatial_pairs)>>{};
  else if constexpr (Key::name.view() == std::string_view("querySpatialPoint") && requires(Object& object) { object.query_spatial_point; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().query_spatial_point)>>{};
  else if constexpr (Key::name.view() == std::string_view("querySpatialRay") && requires(Object& object) { object.query_spatial_ray; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().query_spatial_ray)>>{};
  else if constexpr (Key::name.view() == std::string_view("querySpatialRegion") && requires(Object& object) { object.query_spatial_region; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().query_spatial_region)>>{};
  else if constexpr (Key::name.view() == std::string_view("rAX") && requires(Object& object) { object.r_ax; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().r_ax)>>{};
  else if constexpr (Key::name.view() == std::string_view("rAY") && requires(Object& object) { object.r_ay; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().r_ay)>>{};
  else if constexpr (Key::name.view() == std::string_view("rAZ") && requires(Object& object) { object.r_az; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().r_az)>>{};
  else if constexpr (Key::name.view() == std::string_view("rBX") && requires(Object& object) { object.r_bx; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().r_bx)>>{};
  else if constexpr (Key::name.view() == std::string_view("rBY") && requires(Object& object) { object.r_by; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().r_by)>>{};
  else if constexpr (Key::name.view() == std::string_view("rBZ") && requires(Object& object) { object.r_bz; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().r_bz)>>{};
  else if constexpr (Key::name.view() == std::string_view("radial") && requires(Object& object) { object.radial; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().radial)>>{};
  else if constexpr (Key::name.view() == std::string_view("radialAccelVariance") && requires(Object& object) { object.radial_accel_variance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().radial_accel_variance)>>{};
  else if constexpr (Key::name.view() == std::string_view("radialAcceleration") && requires(Object& object) { object.radial_acceleration; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().radial_acceleration)>>{};
  else if constexpr (Key::name.view() == std::string_view("radius") && requires(Object& object) { object.radius; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().radius)>>{};
  else if constexpr (Key::name.view() == std::string_view("range") && requires(Object& object) { object.range; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().range)>>{};
  else if constexpr (Key::name.view() == std::string_view("readBookmark") && requires(Object& object) { object.read_bookmark; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().read_bookmark)>>{};
  else if constexpr (Key::name.view() == std::string_view("readFormat") && requires(Object& object) { object.read_format; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().read_format)>>{};
  else if constexpr (Key::name.view() == std::string_view("readHtml") && requires(Object& object) { object.read_html; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().read_html)>>{};
  else if constexpr (Key::name.view() == std::string_view("readImage") && requires(Object& object) { object.read_image; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().read_image)>>{};
  else if constexpr (Key::name.view() == std::string_view("readItems") && requires(Object& object) { object.read_items; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().read_items)>>{};
  else if constexpr (Key::name.view() == std::string_view("readRTF") && requires(Object& object) { object.read_rtf; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().read_rtf)>>{};
  else if constexpr (Key::name.view() == std::string_view("readText") && requires(Object& object) { object.read_text; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().read_text)>>{};
  else if constexpr (Key::name.view() == std::string_view("reason") && requires(Object& object) { object.reason; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().reason)>>{};
  else if constexpr (Key::name.view() == std::string_view("red") && requires(Object& object) { object.red; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().red)>>{};
  else if constexpr (Key::name.view() == std::string_view("redBias") && requires(Object& object) { object.red_bias; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().red_bias)>>{};
  else if constexpr (Key::name.view() == std::string_view("redScale") && requires(Object& object) { object.red_scale; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().red_scale)>>{};
  else if constexpr (Key::name.view() == std::string_view("refresh") && requires(Object& object) { object.refresh; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().refresh)>>{};
  else if constexpr (Key::name.view() == std::string_view("regionIdMax") && requires(Object& object) { object.region_id_max; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().region_id_max)>>{};
  else if constexpr (Key::name.view() == std::string_view("regionIdMin") && requires(Object& object) { object.region_id_min; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().region_id_min)>>{};
  else if constexpr (Key::name.view() == std::string_view("relative") && requires(Object& object) { object.relative; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().relative)>>{};
  else if constexpr (Key::name.view() == std::string_view("removeNode") && requires(Object& object) { object.remove_node; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().remove_node)>>{};
  else if constexpr (Key::name.view() == std::string_view("removeSpatialObject") && requires(Object& object) { object.remove_spatial_object; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().remove_spatial_object)>>{};
  else if constexpr (Key::name.view() == std::string_view("repeatCount") && requires(Object& object) { object.repeat_count; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().repeat_count)>>{};
  else if constexpr (Key::name.view() == std::string_view("requestPermission") && requires(Object& object) { object.request_permission; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().request_permission)>>{};
  else if constexpr (Key::name.view() == std::string_view("requestPersistence") && requires(Object& object) { object.request_persistence; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().request_persistence)>>{};
  else if constexpr (Key::name.view() == std::string_view("resize") && requires(Object& object) { object.resize; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().resize)>>{};
  else if constexpr (Key::name.view() == std::string_view("resolution") && requires(Object& object) { object.resolution; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().resolution)>>{};
  else if constexpr (Key::name.view() == std::string_view("restitution") && requires(Object& object) { object.restitution; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().restitution)>>{};
  else if constexpr (Key::name.view() == std::string_view("right") && requires(Object& object) { object.right; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().right)>>{};
  else if constexpr (Key::name.view() == std::string_view("root") && requires(Object& object) { object.root; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().root)>>{};
  else if constexpr (Key::name.view() == std::string_view("rotatePerSecond") && requires(Object& object) { object.rotate_per_second; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().rotate_per_second)>>{};
  else if constexpr (Key::name.view() == std::string_view("rotatePerSecondVariance") && requires(Object& object) { object.rotate_per_second_variance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().rotate_per_second_variance)>>{};
  else if constexpr (Key::name.view() == std::string_view("rotated") && requires(Object& object) { object.rotated; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().rotated)>>{};
  else if constexpr (Key::name.view() == std::string_view("rotation") && requires(Object& object) { object.rotation; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().rotation)>>{};
  else if constexpr (Key::name.view() == std::string_view("rotationAmplitude") && requires(Object& object) { object.rotation_amplitude; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().rotation_amplitude)>>{};
  else if constexpr (Key::name.view() == std::string_view("rotationEnd") && requires(Object& object) { object.rotation_end; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().rotation_end)>>{};
  else if constexpr (Key::name.view() == std::string_view("rotationEndVariance") && requires(Object& object) { object.rotation_end_variance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().rotation_end_variance)>>{};
  else if constexpr (Key::name.view() == std::string_view("rotationSpeedMax") && requires(Object& object) { object.rotation_speed_max; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().rotation_speed_max)>>{};
  else if constexpr (Key::name.view() == std::string_view("rotationSpeedMin") && requires(Object& object) { object.rotation_speed_min; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().rotation_speed_min)>>{};
  else if constexpr (Key::name.view() == std::string_view("rotationStart") && requires(Object& object) { object.rotation_start; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().rotation_start)>>{};
  else if constexpr (Key::name.view() == std::string_view("rotationStartVariance") && requires(Object& object) { object.rotation_start_variance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().rotation_start_variance)>>{};
  else if constexpr (Key::name.view() == std::string_view("rotationX") && requires(Object& object) { object.rotation_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().rotation_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("rotationY") && requires(Object& object) { object.rotation_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().rotation_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("rotationZ") && requires(Object& object) { object.rotation_z; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().rotation_z)>>{};
  else if constexpr (Key::name.view() == std::string_view("runtime") && requires(Object& object) { object.runtime; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().runtime)>>{};
  else if constexpr (Key::name.view() == std::string_view("samples") && requires(Object& object) { object.samples; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().samples)>>{};
  else if constexpr (Key::name.view() == std::string_view("saturation") && requires(Object& object) { object.saturation; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().saturation)>>{};
  else if constexpr (Key::name.view() == std::string_view("scale") && requires(Object& object) { object.scale; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().scale)>>{};
  else if constexpr (Key::name.view() == std::string_view("scaleCurve") && requires(Object& object) { object.scale_curve; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().scale_curve)>>{};
  else if constexpr (Key::name.view() == std::string_view("scaleEnd") && requires(Object& object) { object.scale_end; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().scale_end)>>{};
  else if constexpr (Key::name.view() == std::string_view("scaleMax") && requires(Object& object) { object.scale_max; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().scale_max)>>{};
  else if constexpr (Key::name.view() == std::string_view("scaleMin") && requires(Object& object) { object.scale_min; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().scale_min)>>{};
  else if constexpr (Key::name.view() == std::string_view("scaleX") && requires(Object& object) { object.scale_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().scale_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("scaleY") && requires(Object& object) { object.scale_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().scale_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("scaling") && requires(Object& object) { object.scaling; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().scaling)>>{};
  else if constexpr (Key::name.view() == std::string_view("scanlineIntensity") && requires(Object& object) { object.scanline_intensity; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().scanline_intensity)>>{};
  else if constexpr (Key::name.view() == std::string_view("scattering") && requires(Object& object) { object.scattering; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().scattering)>>{};
  else if constexpr (Key::name.view() == std::string_view("scope") && requires(Object& object) { object.scope; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().scope)>>{};
  else if constexpr (Key::name.view() == std::string_view("seed") && requires(Object& object) { object.seed; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().seed)>>{};
  else if constexpr (Key::name.view() == std::string_view("segment") && requires(Object& object) { object.segment; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().segment)>>{};
  else if constexpr (Key::name.view() == std::string_view("selection") && requires(Object& object) { object.selection; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().selection)>>{};
  else if constexpr (Key::name.view() == std::string_view("send") && requires(Object& object) { object.send; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().send)>>{};
  else if constexpr (Key::name.view() == std::string_view("sensor") && requires(Object& object) { object.sensor; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().sensor)>>{};
  else if constexpr (Key::name.view() == std::string_view("setAccessoryBarVisible") && requires(Object& object) { object.set_accessory_bar_visible; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().set_accessory_bar_visible)>>{};
  else if constexpr (Key::name.view() == std::string_view("setBackgroundColor") && requires(Object& object) { object.set_background_color; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().set_background_color)>>{};
  else if constexpr (Key::name.view() == std::string_view("setDisplaySize") && requires(Object& object) { object.set_display_size; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().set_display_size)>>{};
  else if constexpr (Key::name.view() == std::string_view("setFocus") && requires(Object& object) { object.set_focus; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().set_focus)>>{};
  else if constexpr (Key::name.view() == std::string_view("setMetadata") && requires(Object& object) { object.set_metadata; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().set_metadata)>>{};
  else if constexpr (Key::name.view() == std::string_view("setNode") && requires(Object& object) { object.set_node; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().set_node)>>{};
  else if constexpr (Key::name.view() == std::string_view("setOverlaysContent") && requires(Object& object) { object.set_overlays_content; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().set_overlays_content)>>{};
  else if constexpr (Key::name.view() == std::string_view("setPlaybackState") && requires(Object& object) { object.set_playback_state; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().set_playback_state)>>{};
  else if constexpr (Key::name.view() == std::string_view("setPositionState") && requires(Object& object) { object.set_position_state; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().set_position_state)>>{};
  else if constexpr (Key::name.view() == std::string_view("setResizeMode") && requires(Object& object) { object.set_resize_mode; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().set_resize_mode)>>{};
  else if constexpr (Key::name.view() == std::string_view("setScrollAssistEnabled") && requires(Object& object) { object.set_scroll_assist_enabled; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().set_scroll_assist_enabled)>>{};
  else if constexpr (Key::name.view() == std::string_view("setStyle") && requires(Object& object) { object.set_style; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().set_style)>>{};
  else if constexpr (Key::name.view() == std::string_view("setVisible") && requires(Object& object) { object.set_visible; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().set_visible)>>{};
  else if constexpr (Key::name.view() == std::string_view("shaderKey") && requires(Object& object) { object.shader_key; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().shader_key)>>{};
  else if constexpr (Key::name.view() == std::string_view("shadowBias") && requires(Object& object) { object.shadow_bias; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().shadow_bias)>>{};
  else if constexpr (Key::name.view() == std::string_view("shadowFar") && requires(Object& object) { object.shadow_far; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().shadow_far)>>{};
  else if constexpr (Key::name.view() == std::string_view("shadowMapSize") && requires(Object& object) { object.shadow_map_size; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().shadow_map_size)>>{};
  else if constexpr (Key::name.view() == std::string_view("shadowNear") && requires(Object& object) { object.shadow_near; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().shadow_near)>>{};
  else if constexpr (Key::name.view() == std::string_view("shadowStrength") && requires(Object& object) { object.shadow_strength; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().shadow_strength)>>{};
  else if constexpr (Key::name.view() == std::string_view("sheenColor") && requires(Object& object) { object.sheen_color; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().sheen_color)>>{};
  else if constexpr (Key::name.view() == std::string_view("sheenColorMap") && requires(Object& object) { object.sheen_color_map; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().sheen_color_map)>>{};
  else if constexpr (Key::name.view() == std::string_view("sheenColorMapUvSet") && requires(Object& object) { object.sheen_color_map_uv_set; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().sheen_color_map_uv_set)>>{};
  else if constexpr (Key::name.view() == std::string_view("sheenRoughness") && requires(Object& object) { object.sheen_roughness; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().sheen_roughness)>>{};
  else if constexpr (Key::name.view() == std::string_view("sheenRoughnessMap") && requires(Object& object) { object.sheen_roughness_map; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().sheen_roughness_map)>>{};
  else if constexpr (Key::name.view() == std::string_view("sheenRoughnessMapUvSet") && requires(Object& object) { object.sheen_roughness_map_uv_set; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().sheen_roughness_map_uv_set)>>{};
  else if constexpr (Key::name.view() == std::string_view("show") && requires(Object& object) { object.show; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().show)>>{};
  else if constexpr (Key::name.view() == std::string_view("size") && requires(Object& object) { object.size; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().size)>>{};
  else if constexpr (Key::name.view() == std::string_view("skewX") && requires(Object& object) { object.skew_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().skew_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("skewY") && requires(Object& object) { object.skew_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().skew_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("skyColor") && requires(Object& object) { object.sky_color; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().sky_color)>>{};
  else if constexpr (Key::name.view() == std::string_view("slotIndex") && requires(Object& object) { object.slot_index; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().slot_index)>>{};
  else if constexpr (Key::name.view() == std::string_view("slots") && requires(Object& object) { object.slots; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().slots)>>{};
  else if constexpr (Key::name.view() == std::string_view("smoothTime") && requires(Object& object) { object.smooth_time; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().smooth_time)>>{};
  else if constexpr (Key::name.view() == std::string_view("softness") && requires(Object& object) { object.softness; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().softness)>>{};
  else if constexpr (Key::name.view() == std::string_view("sourceHeight") && requires(Object& object) { object.source_height; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().source_height)>>{};
  else if constexpr (Key::name.view() == std::string_view("sourceMode") && requires(Object& object) { object.source_mode; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().source_mode)>>{};
  else if constexpr (Key::name.view() == std::string_view("sourcePositionVariancex") && requires(Object& object) { object.source_position_variancex; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().source_position_variancex)>>{};
  else if constexpr (Key::name.view() == std::string_view("sourcePositionVariancey") && requires(Object& object) { object.source_position_variancey; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().source_position_variancey)>>{};
  else if constexpr (Key::name.view() == std::string_view("sourceWidth") && requires(Object& object) { object.source_width; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().source_width)>>{};
  else if constexpr (Key::name.view() == std::string_view("spawnHeight") && requires(Object& object) { object.spawn_height; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().spawn_height)>>{};
  else if constexpr (Key::name.view() == std::string_view("spawnRate") && requires(Object& object) { object.spawn_rate; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().spawn_rate)>>{};
  else if constexpr (Key::name.view() == std::string_view("spawnShape") && requires(Object& object) { object.spawn_shape; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().spawn_shape)>>{};
  else if constexpr (Key::name.view() == std::string_view("spawnWidth") && requires(Object& object) { object.spawn_width; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().spawn_width)>>{};
  else if constexpr (Key::name.view() == std::string_view("specular") && requires(Object& object) { object.specular; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().specular)>>{};
  else if constexpr (Key::name.view() == std::string_view("specularColor") && requires(Object& object) { object.specular_color; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().specular_color)>>{};
  else if constexpr (Key::name.view() == std::string_view("specularColorMap") && requires(Object& object) { object.specular_color_map; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().specular_color_map)>>{};
  else if constexpr (Key::name.view() == std::string_view("specularColorMapUvSet") && requires(Object& object) { object.specular_color_map_uv_set; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().specular_color_map_uv_set)>>{};
  else if constexpr (Key::name.view() == std::string_view("specularMap") && requires(Object& object) { object.specular_map; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().specular_map)>>{};
  else if constexpr (Key::name.view() == std::string_view("specularMapUvSet") && requires(Object& object) { object.specular_map_uv_set; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().specular_map_uv_set)>>{};
  else if constexpr (Key::name.view() == std::string_view("speed") && requires(Object& object) { object.speed; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().speed)>>{};
  else if constexpr (Key::name.view() == std::string_view("speedMax") && requires(Object& object) { object.speed_max; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().speed_max)>>{};
  else if constexpr (Key::name.view() == std::string_view("speedMin") && requires(Object& object) { object.speed_min; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().speed_min)>>{};
  else if constexpr (Key::name.view() == std::string_view("speedVariance") && requires(Object& object) { object.speed_variance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().speed_variance)>>{};
  else if constexpr (Key::name.view() == std::string_view("spot") && requires(Object& object) { object.spot; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().spot)>>{};
  else if constexpr (Key::name.view() == std::string_view("spotBlend") && requires(Object& object) { object.spot_blend; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().spot_blend)>>{};
  else if constexpr (Key::name.view() == std::string_view("spread") && requires(Object& object) { object.spread; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().spread)>>{};
  else if constexpr (Key::name.view() == std::string_view("stack") && requires(Object& object) { object.stack; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().stack)>>{};
  else if constexpr (Key::name.view() == std::string_view("start") && requires(Object& object) { object.start; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().start)>>{};
  else if constexpr (Key::name.view() == std::string_view("startColor") && requires(Object& object) { object.start_color; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().start_color)>>{};
  else if constexpr (Key::name.view() == std::string_view("startColorVariance") && requires(Object& object) { object.start_color_variance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().start_color_variance)>>{};
  else if constexpr (Key::name.view() == std::string_view("startIndex") && requires(Object& object) { object.start_index; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().start_index)>>{};
  else if constexpr (Key::name.view() == std::string_view("startParticleSize") && requires(Object& object) { object.start_particle_size; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().start_particle_size)>>{};
  else if constexpr (Key::name.view() == std::string_view("startParticleSizeVariance") && requires(Object& object) { object.start_particle_size_variance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().start_particle_size_variance)>>{};
  else if constexpr (Key::name.view() == std::string_view("startX") && requires(Object& object) { object.start_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().start_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("startY") && requires(Object& object) { object.start_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().start_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("startZ") && requires(Object& object) { object.start_z; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().start_z)>>{};
  else if constexpr (Key::name.view() == std::string_view("stated") && requires(Object& object) { object.stated; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().stated)>>{};
  else if constexpr (Key::name.view() == std::string_view("steps") && requires(Object& object) { object.steps; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().steps)>>{};
  else if constexpr (Key::name.view() == std::string_view("strength") && requires(Object& object) { object.strength; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().strength)>>{};
  else if constexpr (Key::name.view() == std::string_view("strokeBounds") && requires(Object& object) { object.stroke_bounds; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().stroke_bounds)>>{};
  else if constexpr (Key::name.view() == std::string_view("style") && requires(Object& object) { object.style; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().style)>>{};
  else if constexpr (Key::name.view() == std::string_view("subject") && requires(Object& object) { object.subject; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().subject)>>{};
  else if constexpr (Key::name.view() == std::string_view("subpixel") && requires(Object& object) { object.subpixel; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().subpixel)>>{};
  else if constexpr (Key::name.view() == std::string_view("subscribe") && requires(Object& object) { object.subscribe; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().subscribe)>>{};
  else if constexpr (Key::name.view() == std::string_view("subscribeAbsoluteOrientation") && requires(Object& object) { object.subscribe_absolute_orientation; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().subscribe_absolute_orientation)>>{};
  else if constexpr (Key::name.view() == std::string_view("subscribeAmbientLight") && requires(Object& object) { object.subscribe_ambient_light; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().subscribe_ambient_light)>>{};
  else if constexpr (Key::name.view() == std::string_view("subscribeBarometer") && requires(Object& object) { object.subscribe_barometer; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().subscribe_barometer)>>{};
  else if constexpr (Key::name.view() == std::string_view("subscribeGravity") && requires(Object& object) { object.subscribe_gravity; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().subscribe_gravity)>>{};
  else if constexpr (Key::name.view() == std::string_view("subscribeLinearAcceleration") && requires(Object& object) { object.subscribe_linear_acceleration; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().subscribe_linear_acceleration)>>{};
  else if constexpr (Key::name.view() == std::string_view("subscribeMagnetometer") && requires(Object& object) { object.subscribe_magnetometer; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().subscribe_magnetometer)>>{};
  else if constexpr (Key::name.view() == std::string_view("subscribeMotion") && requires(Object& object) { object.subscribe_motion; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().subscribe_motion)>>{};
  else if constexpr (Key::name.view() == std::string_view("subscribeOrientation") && requires(Object& object) { object.subscribe_orientation; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().subscribe_orientation)>>{};
  else if constexpr (Key::name.view() == std::string_view("subscribeProximity") && requires(Object& object) { object.subscribe_proximity; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().subscribe_proximity)>>{};
  else if constexpr (Key::name.view() == std::string_view("subscribeQuaternion") && requires(Object& object) { object.subscribe_quaternion; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().subscribe_quaternion)>>{};
  else if constexpr (Key::name.view() == std::string_view("supportedAbis") && requires(Object& object) { object.supported_abis; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().supported_abis)>>{};
  else if constexpr (Key::name.view() == std::string_view("tangentialAccelVariance") && requires(Object& object) { object.tangential_accel_variance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().tangential_accel_variance)>>{};
  else if constexpr (Key::name.view() == std::string_view("tangentialAcceleration") && requires(Object& object) { object.tangential_acceleration; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().tangential_acceleration)>>{};
  else if constexpr (Key::name.view() == std::string_view("temperature") && requires(Object& object) { object.temperature; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().temperature)>>{};
  else if constexpr (Key::name.view() == std::string_view("textHeight") && requires(Object& object) { object.text_height; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().text_height)>>{};
  else if constexpr (Key::name.view() == std::string_view("textWidth") && requires(Object& object) { object.text_width; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().text_width)>>{};
  else if constexpr (Key::name.view() == std::string_view("textureFileName") && requires(Object& object) { object.texture_file_name; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().texture_file_name)>>{};
  else if constexpr (Key::name.view() == std::string_view("thickness") && requires(Object& object) { object.thickness; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().thickness)>>{};
  else if constexpr (Key::name.view() == std::string_view("thicknessMap") && requires(Object& object) { object.thickness_map; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().thickness_map)>>{};
  else if constexpr (Key::name.view() == std::string_view("thicknessMapUvSet") && requires(Object& object) { object.thickness_map_uv_set; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().thickness_map_uv_set)>>{};
  else if constexpr (Key::name.view() == std::string_view("threshold") && requires(Object& object) { object.threshold; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().threshold)>>{};
  else if constexpr (Key::name.view() == std::string_view("tilesets") && requires(Object& object) { object.tilesets; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().tilesets)>>{};
  else if constexpr (Key::name.view() == std::string_view("time") && requires(Object& object) { object.time; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().time)>>{};
  else if constexpr (Key::name.view() == std::string_view("timeline") && requires(Object& object) { object.timeline; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().timeline)>>{};
  else if constexpr (Key::name.view() == std::string_view("timestamp") && requires(Object& object) { object.timestamp; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().timestamp)>>{};
  else if constexpr (Key::name.view() == std::string_view("tint") && requires(Object& object) { object.tint; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().tint)>>{};
  else if constexpr (Key::name.view() == std::string_view("top") && requires(Object& object) { object.top; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().top)>>{};
  else if constexpr (Key::name.view() == std::string_view("torque") && requires(Object& object) { object.torque; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().torque)>>{};
  else if constexpr (Key::name.view() == std::string_view("torqueX") && requires(Object& object) { object.torque_x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().torque_x)>>{};
  else if constexpr (Key::name.view() == std::string_view("torqueY") && requires(Object& object) { object.torque_y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().torque_y)>>{};
  else if constexpr (Key::name.view() == std::string_view("torqueZ") && requires(Object& object) { object.torque_z; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().torque_z)>>{};
  else if constexpr (Key::name.view() == std::string_view("totalMemory") && requires(Object& object) { object.total_memory; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().total_memory)>>{};
  else if constexpr (Key::name.view() == std::string_view("touching") && requires(Object& object) { object.touching; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().touching)>>{};
  else if constexpr (Key::name.view() == std::string_view("transform") && requires(Object& object) { object.transform; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().transform)>>{};
  else if constexpr (Key::name.view() == std::string_view("translationAmplitude") && requires(Object& object) { object.translation_amplitude; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().translation_amplitude)>>{};
  else if constexpr (Key::name.view() == std::string_view("transmission") && requires(Object& object) { object.transmission; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().transmission)>>{};
  else if constexpr (Key::name.view() == std::string_view("transmissionMap") && requires(Object& object) { object.transmission_map; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().transmission_map)>>{};
  else if constexpr (Key::name.view() == std::string_view("transmissionMapUvSet") && requires(Object& object) { object.transmission_map_uv_set; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().transmission_map_uv_set)>>{};
  else if constexpr (Key::name.view() == std::string_view("transparency") && requires(Object& object) { object.transparency; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().transparency)>>{};
  else if constexpr (Key::name.view() == std::string_view("trauma") && requires(Object& object) { object.trauma; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().trauma)>>{};
  else if constexpr (Key::name.view() == std::string_view("tweens") && requires(Object& object) { object.tweens; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().tweens)>>{};
  else if constexpr (Key::name.view() == std::string_view("tx") && requires(Object& object) { object.tx; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().tx)>>{};
  else if constexpr (Key::name.view() == std::string_view("ty") && requires(Object& object) { object.ty; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().ty)>>{};
  else if constexpr (Key::name.view() == std::string_view("type") && requires(Object& object) { object.type; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().type)>>{};
  else if constexpr (Key::name.view() == std::string_view("uniforms") && requires(Object& object) { object.uniforms; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().uniforms)>>{};
  else if constexpr (Key::name.view() == std::string_view("up") && requires(Object& object) { object.up; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().up)>>{};
  else if constexpr (Key::name.view() == std::string_view("updateSpatialObject") && requires(Object& object) { object.update_spatial_object; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().update_spatial_object)>>{};
  else if constexpr (Key::name.view() == std::string_view("value") && requires(Object& object) { object.value; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().value)>>{};
  else if constexpr (Key::name.view() == std::string_view("velocity") && requires(Object& object) { object.velocity; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().velocity)>>{};
  else if constexpr (Key::name.view() == std::string_view("velocityInheritance") && requires(Object& object) { object.velocity_inheritance; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().velocity_inheritance)>>{};
  else if constexpr (Key::name.view() == std::string_view("version") && requires(Object& object) { object.version; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().version)>>{};
  else if constexpr (Key::name.view() == std::string_view("vibrate") && requires(Object& object) { object.vibrate; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().vibrate)>>{};
  else if constexpr (Key::name.view() == std::string_view("vibratePattern") && requires(Object& object) { object.vibrate_pattern; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().vibrate_pattern)>>{};
  else if constexpr (Key::name.view() == std::string_view("vibrateWaveform") && requires(Object& object) { object.vibrate_waveform; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().vibrate_waveform)>>{};
  else if constexpr (Key::name.view() == std::string_view("view") && requires(Object& object) { object.view; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().view)>>{};
  else if constexpr (Key::name.view() == std::string_view("viewportHeight") && requires(Object& object) { object.viewport_height; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().viewport_height)>>{};
  else if constexpr (Key::name.view() == std::string_view("viewportWidth") && requires(Object& object) { object.viewport_width; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().viewport_width)>>{};
  else if constexpr (Key::name.view() == std::string_view("vignette") && requires(Object& object) { object.vignette; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().vignette)>>{};
  else if constexpr (Key::name.view() == std::string_view("visible") && requires(Object& object) { object.visible; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().visible)>>{};
  else if constexpr (Key::name.view() == std::string_view("w") && requires(Object& object) { object.w; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().w)>>{};
  else if constexpr (Key::name.view() == std::string_view("watchPosition") && requires(Object& object) { object.watch_position; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().watch_position)>>{};
  else if constexpr (Key::name.view() == std::string_view("webViewVersion") && requires(Object& object) { object.web_view_version; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().web_view_version)>>{};
  else if constexpr (Key::name.view() == std::string_view("weight") && requires(Object& object) { object.weight; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().weight)>>{};
  else if constexpr (Key::name.view() == std::string_view("white") && requires(Object& object) { object.white; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().white)>>{};
  else if constexpr (Key::name.view() == std::string_view("width") && requires(Object& object) { object.width; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().width)>>{};
  else if constexpr (Key::name.view() == std::string_view("wind") && requires(Object& object) { object.wind; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().wind)>>{};
  else if constexpr (Key::name.view() == std::string_view("winding") && requires(Object& object) { object.winding; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().winding)>>{};
  else if constexpr (Key::name.view() == std::string_view("worldBounds") && requires(Object& object) { object.world_bounds; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().world_bounds)>>{};
  else if constexpr (Key::name.view() == std::string_view("worldMatrices") && requires(Object& object) { object.world_matrices; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().world_matrices)>>{};
  else if constexpr (Key::name.view() == std::string_view("worldSpace") && requires(Object& object) { object.world_space; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().world_space)>>{};
  else if constexpr (Key::name.view() == std::string_view("wouldOccupyBucketCount") && requires(Object& object) { object.would_occupy_bucket_count; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().would_occupy_bucket_count)>>{};
  else if constexpr (Key::name.view() == std::string_view("wrapU") && requires(Object& object) { object.wrap_u; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().wrap_u)>>{};
  else if constexpr (Key::name.view() == std::string_view("wrapV") && requires(Object& object) { object.wrap_v; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().wrap_v)>>{};
  else if constexpr (Key::name.view() == std::string_view("wrappedDiffuseColor") && requires(Object& object) { object.wrapped_diffuse_color; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().wrapped_diffuse_color)>>{};
  else if constexpr (Key::name.view() == std::string_view("wrappedDiffuseMap") && requires(Object& object) { object.wrapped_diffuse_map; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().wrapped_diffuse_map)>>{};
  else if constexpr (Key::name.view() == std::string_view("wrappedDiffuseMapUvSet") && requires(Object& object) { object.wrapped_diffuse_map_uv_set; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().wrapped_diffuse_map_uv_set)>>{};
  else if constexpr (Key::name.view() == std::string_view("wrappedDiffuseStrength") && requires(Object& object) { object.wrapped_diffuse_strength; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().wrapped_diffuse_strength)>>{};
  else if constexpr (Key::name.view() == std::string_view("writeBookmark") && requires(Object& object) { object.write_bookmark; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().write_bookmark)>>{};
  else if constexpr (Key::name.view() == std::string_view("writeFormat") && requires(Object& object) { object.write_format; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().write_format)>>{};
  else if constexpr (Key::name.view() == std::string_view("writeHtml") && requires(Object& object) { object.write_html; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().write_html)>>{};
  else if constexpr (Key::name.view() == std::string_view("writeImage") && requires(Object& object) { object.write_image; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().write_image)>>{};
  else if constexpr (Key::name.view() == std::string_view("writeItems") && requires(Object& object) { object.write_items; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().write_items)>>{};
  else if constexpr (Key::name.view() == std::string_view("writeRTF") && requires(Object& object) { object.write_rtf; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().write_rtf)>>{};
  else if constexpr (Key::name.view() == std::string_view("writeText") && requires(Object& object) { object.write_text; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().write_text)>>{};
  else if constexpr (Key::name.view() == std::string_view("x") && requires(Object& object) { object.x; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().x)>>{};
  else if constexpr (Key::name.view() == std::string_view("x0") && requires(Object& object) { object.x0; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().x0)>>{};
  else if constexpr (Key::name.view() == std::string_view("x1") && requires(Object& object) { object.x1; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().x1)>>{};
  else if constexpr (Key::name.view() == std::string_view("xOffset") && requires(Object& object) { object.x_offset; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().x_offset)>>{};
  else if constexpr (Key::name.view() == std::string_view("y") && requires(Object& object) { object.y; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().y)>>{};
  else if constexpr (Key::name.view() == std::string_view("y0") && requires(Object& object) { object.y0; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().y0)>>{};
  else if constexpr (Key::name.view() == std::string_view("y1") && requires(Object& object) { object.y1; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().y1)>>{};
  else if constexpr (Key::name.view() == std::string_view("yOffset") && requires(Object& object) { object.y_offset; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().y_offset)>>{};
  else if constexpr (Key::name.view() == std::string_view("z") && requires(Object& object) { object.z; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().z)>>{};
  else if constexpr (Key::name.view() == std::string_view("zoom") && requires(Object& object) { object.zoom; }) return std::type_identity<std::remove_cvref_t<decltype(std::declval<Object&>().zoom)>>{};
  else return std::type_identity<void>{};
}

template <typename Key, typename Object>
using generated_row_member_t = typename decltype(generated_row_member_type_identity<Key, Object>())::type;

template <typename Object>
void bind_generated_row_members(RowOwner& owner, const std::shared_ptr<Object>& object) {
  if constexpr (requires { object->brand; }) owner.bind_named("__brand", [object]() -> decltype(auto) { return (object->brand); });
  if constexpr (requires { object->a; }) owner.bind_named("a", [object]() -> decltype(auto) { return (object->a); });
  if constexpr (requires { object->aberration; }) owner.bind_named("aberration", [object]() -> decltype(auto) { return (object->aberration); });
  if constexpr (requires { object->absolute; }) owner.bind_named("absolute", [object]() -> decltype(auto) { return (object->absolute); });
  if constexpr (requires { object->accuracy; }) owner.bind_named("accuracy", [object]() -> decltype(auto) { return (object->accuracy); });
  if constexpr (requires { object->action; }) owner.bind_named("action", [object]() -> decltype(auto) { return (object->action); });
  if constexpr (requires { object->adaptation_speed; }) owner.bind_named("adaptationSpeed", [object]() -> decltype(auto) { return (object->adaptation_speed); });
  if constexpr (requires { object->additive; }) owner.bind_named("additive", [object]() -> decltype(auto) { return (object->additive); });
  if constexpr (requires { object->addressed; }) owner.bind_named("addressed", [object]() -> decltype(auto) { return (object->addressed); });
  if constexpr (requires { object->alpha; }) owner.bind_named("alpha", [object]() -> decltype(auto) { return (object->alpha); });
  if constexpr (requires { object->alpha_bias; }) owner.bind_named("alphaBias", [object]() -> decltype(auto) { return (object->alpha_bias); });
  if constexpr (requires { object->alpha_curve; }) owner.bind_named("alphaCurve", [object]() -> decltype(auto) { return (object->alpha_curve); });
  if constexpr (requires { object->alpha_end; }) owner.bind_named("alphaEnd", [object]() -> decltype(auto) { return (object->alpha_end); });
  if constexpr (requires { object->alpha_scale; }) owner.bind_named("alphaScale", [object]() -> decltype(auto) { return (object->alpha_scale); });
  if constexpr (requires { object->alpha_start; }) owner.bind_named("alphaStart", [object]() -> decltype(auto) { return (object->alpha_start); });
  if constexpr (requires { object->altitude; }) owner.bind_named("altitude", [object]() -> decltype(auto) { return (object->altitude); });
  if constexpr (requires { object->altitude_accuracy; }) owner.bind_named("altitudeAccuracy", [object]() -> decltype(auto) { return (object->altitude_accuracy); });
  if constexpr (requires { object->ambient; }) owner.bind_named("ambient", [object]() -> decltype(auto) { return (object->ambient); });
  if constexpr (requires { object->amount; }) owner.bind_named("amount", [object]() -> decltype(auto) { return (object->amount); });
  if constexpr (requires { object->angle; }) owner.bind_named("angle", [object]() -> decltype(auto) { return (object->angle); });
  if constexpr (requires { object->angle_variance; }) owner.bind_named("angleVariance", [object]() -> decltype(auto) { return (object->angle_variance); });
  if constexpr (requires { object->animation; }) owner.bind_named("animation", [object]() -> decltype(auto) { return (object->animation); });
  if constexpr (requires { object->animations; }) owner.bind_named("animations", [object]() -> decltype(auto) { return (object->animations); });
  if constexpr (requires { object->anisotropy; }) owner.bind_named("anisotropy", [object]() -> decltype(auto) { return (object->anisotropy); });
  if constexpr (requires { object->anisotropy_map; }) owner.bind_named("anisotropyMap", [object]() -> decltype(auto) { return (object->anisotropy_map); });
  if constexpr (requires { object->anisotropy_map_uv_set; }) owner.bind_named("anisotropyMapUvSet", [object]() -> decltype(auto) { return (object->anisotropy_map_uv_set); });
  if constexpr (requires { object->anisotropy_rotation; }) owner.bind_named("anisotropyRotation", [object]() -> decltype(auto) { return (object->anisotropy_rotation); });
  if constexpr (requires { object->anisotropy_strength; }) owner.bind_named("anisotropyStrength", [object]() -> decltype(auto) { return (object->anisotropy_strength); });
  if constexpr (requires { object->announce; }) owner.bind_named("announce", [object]() -> decltype(auto) { return (object->announce); });
  if constexpr (requires { object->applied; }) owner.bind_named("applied", [object]() -> decltype(auto) { return (object->applied); });
  if constexpr (requires { object->arch; }) owner.bind_named("arch", [object]() -> decltype(auto) { return (object->arch); });
  if constexpr (requires { object->ascent; }) owner.bind_named("ascent", [object]() -> decltype(auto) { return (object->ascent); });
  if constexpr (requires { object->atlas; }) owner.bind_named("atlas", [object]() -> decltype(auto) { return (object->atlas); });
  if constexpr (requires { object->attenuation_color; }) owner.bind_named("attenuationColor", [object]() -> decltype(auto) { return (object->attenuation_color); });
  if constexpr (requires { object->attenuation_distance; }) owner.bind_named("attenuationDistance", [object]() -> decltype(auto) { return (object->attenuation_distance); });
  if constexpr (requires { object->attributes; }) owner.bind_named("attributes", [object]() -> decltype(auto) { return (object->attributes); });
  if constexpr (requires { object->available_memory; }) owner.bind_named("availableMemory", [object]() -> decltype(auto) { return (object->available_memory); });
  if constexpr (requires { object->b; }) owner.bind_named("b", [object]() -> decltype(auto) { return (object->b); });
  if constexpr (requires { object->beta; }) owner.bind_named("beta", [object]() -> decltype(auto) { return (object->beta); });
  if constexpr (requires { object->bias; }) owner.bind_named("bias", [object]() -> decltype(auto) { return (object->bias); });
  if constexpr (requires { object->bitmap; }) owner.bind_named("bitmap", [object]() -> decltype(auto) { return (object->bitmap); });
  if constexpr (requires { object->black_tighten; }) owner.bind_named("blackTighten", [object]() -> decltype(auto) { return (object->black_tighten); });
  if constexpr (requires { object->blend_func_destination; }) owner.bind_named("blendFuncDestination", [object]() -> decltype(auto) { return (object->blend_func_destination); });
  if constexpr (requires { object->blend_func_source; }) owner.bind_named("blendFuncSource", [object]() -> decltype(auto) { return (object->blend_func_source); });
  if constexpr (requires { object->blend_mode; }) owner.bind_named("blendMode", [object]() -> decltype(auto) { return (object->blend_mode); });
  if constexpr (requires { object->blue; }) owner.bind_named("blue", [object]() -> decltype(auto) { return (object->blue); });
  if constexpr (requires { object->blue_bias; }) owner.bind_named("blueBias", [object]() -> decltype(auto) { return (object->blue_bias); });
  if constexpr (requires { object->blue_scale; }) owner.bind_named("blueScale", [object]() -> decltype(auto) { return (object->blue_scale); });
  if constexpr (requires { object->blur_x; }) owner.bind_named("blurX", [object]() -> decltype(auto) { return (object->blur_x); });
  if constexpr (requires { object->blur_y; }) owner.bind_named("blurY", [object]() -> decltype(auto) { return (object->blur_y); });
  if constexpr (requires { object->board_name; }) owner.bind_named("boardName", [object]() -> decltype(auto) { return (object->board_name); });
  if constexpr (requires { object->bodies; }) owner.bind_named("bodies", [object]() -> decltype(auto) { return (object->bodies); });
  if constexpr (requires { object->body_a; }) owner.bind_named("bodyA", [object]() -> decltype(auto) { return (object->body_a); });
  if constexpr (requires { object->body_b; }) owner.bind_named("bodyB", [object]() -> decltype(auto) { return (object->body_b); });
  if constexpr (requires { object->bottom; }) owner.bind_named("bottom", [object]() -> decltype(auto) { return (object->bottom); });
  if constexpr (requires { object->bounds; }) owner.bind_named("bounds", [object]() -> decltype(auto) { return (object->bounds); });
  if constexpr (requires { object->break_force; }) owner.bind_named("breakForce", [object]() -> decltype(auto) { return (object->break_force); });
  if constexpr (requires { object->break_torque; }) owner.bind_named("breakTorque", [object]() -> decltype(auto) { return (object->break_torque); });
  if constexpr (requires { object->brightness; }) owner.bind_named("brightness", [object]() -> decltype(auto) { return (object->brightness); });
  if constexpr (requires { object->burst_count; }) owner.bind_named("burstCount", [object]() -> decltype(auto) { return (object->burst_count); });
  if constexpr (requires { object->burst_interval; }) owner.bind_named("burstInterval", [object]() -> decltype(auto) { return (object->burst_interval); });
  if constexpr (requires { object->c; }) owner.bind_named("c", [object]() -> decltype(auto) { return (object->c); });
  if constexpr (requires { object->cancel; }) owner.bind_named("cancel", [object]() -> decltype(auto) { return (object->cancel); });
  if constexpr (requires { object->capabilities; }) owner.bind_named("capabilities", [object]() -> decltype(auto) { return (object->capabilities); });
  if constexpr (requires { object->cascade_count; }) owner.bind_named("cascadeCount", [object]() -> decltype(auto) { return (object->cascade_count); });
  if constexpr (requires { object->cascade_splits; }) owner.bind_named("cascadeSplits", [object]() -> decltype(auto) { return (object->cascade_splits); });
  if constexpr (requires { object->casts_shadow; }) owner.bind_named("castsShadow", [object]() -> decltype(auto) { return (object->casts_shadow); });
  if constexpr (requires { object->cell_size; }) owner.bind_named("cellSize", [object]() -> decltype(auto) { return (object->cell_size); });
  if constexpr (requires { object->center; }) owner.bind_named("center", [object]() -> decltype(auto) { return (object->center); });
  if constexpr (requires { object->center_x; }) owner.bind_named("centerX", [object]() -> decltype(auto) { return (object->center_x); });
  if constexpr (requires { object->center_y; }) owner.bind_named("centerY", [object]() -> decltype(auto) { return (object->center_y); });
  if constexpr (requires { object->center_z; }) owner.bind_named("centerZ", [object]() -> decltype(auto) { return (object->center_z); });
  if constexpr (requires { object->child1; }) owner.bind_named("child1", [object]() -> decltype(auto) { return (object->child1); });
  if constexpr (requires { object->child2; }) owner.bind_named("child2", [object]() -> decltype(auto) { return (object->child2); });
  if constexpr (requires { object->children; }) owner.bind_named("children", [object]() -> decltype(auto) { return (object->children); });
  if constexpr (requires { object->clear; }) owner.bind_named("clear", [object]() -> decltype(auto) { return (object->clear); });
  if constexpr (requires { object->clear_metadata; }) owner.bind_named("clearMetadata", [object]() -> decltype(auto) { return (object->clear_metadata); });
  if constexpr (requires { object->clear_position_state; }) owner.bind_named("clearPositionState", [object]() -> decltype(auto) { return (object->clear_position_state); });
  if constexpr (requires { object->clear_spatial_index; }) owner.bind_named("clearSpatialIndex", [object]() -> decltype(auto) { return (object->clear_spatial_index); });
  if constexpr (requires { object->clear_watch; }) owner.bind_named("clearWatch", [object]() -> decltype(auto) { return (object->clear_watch); });
  if constexpr (requires { object->clearcoat; }) owner.bind_named("clearcoat", [object]() -> decltype(auto) { return (object->clearcoat); });
  if constexpr (requires { object->clearcoat_map; }) owner.bind_named("clearcoatMap", [object]() -> decltype(auto) { return (object->clearcoat_map); });
  if constexpr (requires { object->clearcoat_map_uv_set; }) owner.bind_named("clearcoatMapUvSet", [object]() -> decltype(auto) { return (object->clearcoat_map_uv_set); });
  if constexpr (requires { object->clearcoat_normal_map; }) owner.bind_named("clearcoatNormalMap", [object]() -> decltype(auto) { return (object->clearcoat_normal_map); });
  if constexpr (requires { object->clearcoat_normal_map_uv_set; }) owner.bind_named("clearcoatNormalMapUvSet", [object]() -> decltype(auto) { return (object->clearcoat_normal_map_uv_set); });
  if constexpr (requires { object->clearcoat_normal_scale; }) owner.bind_named("clearcoatNormalScale", [object]() -> decltype(auto) { return (object->clearcoat_normal_scale); });
  if constexpr (requires { object->clearcoat_roughness; }) owner.bind_named("clearcoatRoughness", [object]() -> decltype(auto) { return (object->clearcoat_roughness); });
  if constexpr (requires { object->clearcoat_roughness_map; }) owner.bind_named("clearcoatRoughnessMap", [object]() -> decltype(auto) { return (object->clearcoat_roughness_map); });
  if constexpr (requires { object->clearcoat_roughness_map_uv_set; }) owner.bind_named("clearcoatRoughnessMapUvSet", [object]() -> decltype(auto) { return (object->clearcoat_roughness_map_uv_set); });
  if constexpr (requires { object->clip; }) owner.bind_named("clip", [object]() -> decltype(auto) { return (object->clip); });
  if constexpr (requires { object->collider_a; }) owner.bind_named("colliderA", [object]() -> decltype(auto) { return (object->collider_a); });
  if constexpr (requires { object->collider_b; }) owner.bind_named("colliderB", [object]() -> decltype(auto) { return (object->collider_b); });
  if constexpr (requires { object->color; }) owner.bind_named("color", [object]() -> decltype(auto) { return (object->color); });
  if constexpr (requires { object->color_curve; }) owner.bind_named("colorCurve", [object]() -> decltype(auto) { return (object->color_curve); });
  if constexpr (requires { object->color_depth; }) owner.bind_named("colorDepth", [object]() -> decltype(auto) { return (object->color_depth); });
  if constexpr (requires { object->color_end_b; }) owner.bind_named("colorEndB", [object]() -> decltype(auto) { return (object->color_end_b); });
  if constexpr (requires { object->color_end_g; }) owner.bind_named("colorEndG", [object]() -> decltype(auto) { return (object->color_end_g); });
  if constexpr (requires { object->color_end_r; }) owner.bind_named("colorEndR", [object]() -> decltype(auto) { return (object->color_end_r); });
  if constexpr (requires { object->color_end_variance_b; }) owner.bind_named("colorEndVarianceB", [object]() -> decltype(auto) { return (object->color_end_variance_b); });
  if constexpr (requires { object->color_end_variance_g; }) owner.bind_named("colorEndVarianceG", [object]() -> decltype(auto) { return (object->color_end_variance_g); });
  if constexpr (requires { object->color_end_variance_r; }) owner.bind_named("colorEndVarianceR", [object]() -> decltype(auto) { return (object->color_end_variance_r); });
  if constexpr (requires { object->color_gamut; }) owner.bind_named("colorGamut", [object]() -> decltype(auto) { return (object->color_gamut); });
  if constexpr (requires { object->color_matrix; }) owner.bind_named("colorMatrix", [object]() -> decltype(auto) { return (object->color_matrix); });
  if constexpr (requires { object->color_scale_bias; }) owner.bind_named("colorScaleBias", [object]() -> decltype(auto) { return (object->color_scale_bias); });
  if constexpr (requires { object->color_start_b; }) owner.bind_named("colorStartB", [object]() -> decltype(auto) { return (object->color_start_b); });
  if constexpr (requires { object->color_start_g; }) owner.bind_named("colorStartG", [object]() -> decltype(auto) { return (object->color_start_g); });
  if constexpr (requires { object->color_start_r; }) owner.bind_named("colorStartR", [object]() -> decltype(auto) { return (object->color_start_r); });
  if constexpr (requires { object->color_start_variance_b; }) owner.bind_named("colorStartVarianceB", [object]() -> decltype(auto) { return (object->color_start_variance_b); });
  if constexpr (requires { object->color_start_variance_g; }) owner.bind_named("colorStartVarianceG", [object]() -> decltype(auto) { return (object->color_start_variance_g); });
  if constexpr (requires { object->color_start_variance_r; }) owner.bind_named("colorStartVarianceR", [object]() -> decltype(auto) { return (object->color_start_variance_r); });
  if constexpr (requires { object->commands; }) owner.bind_named("commands", [object]() -> decltype(auto) { return (object->commands); });
  if constexpr (requires { object->component_x; }) owner.bind_named("componentX", [object]() -> decltype(auto) { return (object->component_x); });
  if constexpr (requires { object->component_y; }) owner.bind_named("componentY", [object]() -> decltype(auto) { return (object->component_y); });
  if constexpr (requires { object->compression; }) owner.bind_named("compression", [object]() -> decltype(auto) { return (object->compression); });
  if constexpr (requires { object->connections; }) owner.bind_named("connections", [object]() -> decltype(auto) { return (object->connections); });
  if constexpr (requires { object->contrast; }) owner.bind_named("contrast", [object]() -> decltype(auto) { return (object->contrast); });
  if constexpr (requires { object->count; }) owner.bind_named("count", [object]() -> decltype(auto) { return (object->count); });
  if constexpr (requires { object->cpu_cores; }) owner.bind_named("cpuCores", [object]() -> decltype(auto) { return (object->cpu_cores); });
  if constexpr (requires { object->crop; }) owner.bind_named("crop", [object]() -> decltype(auto) { return (object->crop); });
  if constexpr (requires { object->curvature; }) owner.bind_named("curvature", [object]() -> decltype(auto) { return (object->curvature); });
  if constexpr (requires { object->d; }) owner.bind_named("d", [object]() -> decltype(auto) { return (object->d); });
  if constexpr (requires { object->damping_ratio; }) owner.bind_named("dampingRatio", [object]() -> decltype(auto) { return (object->damping_ratio); });
  if constexpr (requires { object->data; }) owner.bind_named("data", [object]() -> decltype(auto) { return (object->data); });
  if constexpr (requires { object->deadzone_half_height; }) owner.bind_named("deadzoneHalfHeight", [object]() -> decltype(auto) { return (object->deadzone_half_height); });
  if constexpr (requires { object->deadzone_half_width; }) owner.bind_named("deadzoneHalfWidth", [object]() -> decltype(auto) { return (object->deadzone_half_width); });
  if constexpr (requires { object->decay; }) owner.bind_named("decay", [object]() -> decltype(auto) { return (object->decay); });
  if constexpr (requires { object->declined; }) owner.bind_named("declined", [object]() -> decltype(auto) { return (object->declined); });
  if constexpr (requires { object->default_ease; }) owner.bind_named("defaultEase", [object]() -> decltype(auto) { return (object->default_ease); });
  if constexpr (requires { object->delay; }) owner.bind_named("delay", [object]() -> decltype(auto) { return (object->delay); });
  if constexpr (requires { object->delta_time; }) owner.bind_named("deltaTime", [object]() -> decltype(auto) { return (object->delta_time); });
  if constexpr (requires { object->density; }) owner.bind_named("density", [object]() -> decltype(auto) { return (object->density); });
  if constexpr (requires { object->density_dpi; }) owner.bind_named("densityDpi", [object]() -> decltype(auto) { return (object->density_dpi); });
  if constexpr (requires { object->depth; }) owner.bind_named("depth", [object]() -> decltype(auto) { return (object->depth); });
  if constexpr (requires { object->descent; }) owner.bind_named("descent", [object]() -> decltype(auto) { return (object->descent); });
  if constexpr (requires { object->destroy; }) owner.bind_named("destroy", [object]() -> decltype(auto) { return (object->destroy); });
  if constexpr (requires { object->device_pixel_ratio; }) owner.bind_named("devicePixelRatio", [object]() -> decltype(auto) { return (object->device_pixel_ratio); });
  if constexpr (requires { object->direction; }) owner.bind_named("direction", [object]() -> decltype(auto) { return (object->direction); });
  if constexpr (requires { object->direction_x; }) owner.bind_named("directionX", [object]() -> decltype(auto) { return (object->direction_x); });
  if constexpr (requires { object->direction_y; }) owner.bind_named("directionY", [object]() -> decltype(auto) { return (object->direction_y); });
  if constexpr (requires { object->direction_z; }) owner.bind_named("directionZ", [object]() -> decltype(auto) { return (object->direction_z); });
  if constexpr (requires { object->directional; }) owner.bind_named("directional", [object]() -> decltype(auto) { return (object->directional); });
  if constexpr (requires { object->distance; }) owner.bind_named("distance", [object]() -> decltype(auto) { return (object->distance); });
  if constexpr (requires { object->distro; }) owner.bind_named("distro", [object]() -> decltype(auto) { return (object->distro); });
  if constexpr (requires { object->distro_version; }) owner.bind_named("distroVersion", [object]() -> decltype(auto) { return (object->distro_version); });
  if constexpr (requires { object->divisor; }) owner.bind_named("divisor", [object]() -> decltype(auto) { return (object->divisor); });
  if constexpr (requires { object->duration; }) owner.bind_named("duration", [object]() -> decltype(auto) { return (object->duration); });
  if constexpr (requires { object->ease; }) owner.bind_named("ease", [object]() -> decltype(auto) { return (object->ease); });
  if constexpr (requires { object->edge; }) owner.bind_named("edge", [object]() -> decltype(auto) { return (object->edge); });
  if constexpr (requires { object->edge_mode; }) owner.bind_named("edgeMode", [object]() -> decltype(auto) { return (object->edge_mode); });
  if constexpr (requires { object->edge_threshold; }) owner.bind_named("edgeThreshold", [object]() -> decltype(auto) { return (object->edge_threshold); });
  if constexpr (requires { object->elapsed; }) owner.bind_named("elapsed", [object]() -> decltype(auto) { return (object->elapsed); });
  if constexpr (requires { object->emission; }) owner.bind_named("emission", [object]() -> decltype(auto) { return (object->emission); });
  if constexpr (requires { object->emit; }) owner.bind_named("emit", [object]() -> decltype(auto) { return (object->emit); });
  if constexpr (requires { object->emitter_cone_angle; }) owner.bind_named("emitterConeAngle", [object]() -> decltype(auto) { return (object->emitter_cone_angle); });
  if constexpr (requires { object->emitter_depth; }) owner.bind_named("emitterDepth", [object]() -> decltype(auto) { return (object->emitter_depth); });
  if constexpr (requires { object->emitter_height; }) owner.bind_named("emitterHeight", [object]() -> decltype(auto) { return (object->emitter_height); });
  if constexpr (requires { object->emitter_radius; }) owner.bind_named("emitterRadius", [object]() -> decltype(auto) { return (object->emitter_radius); });
  if constexpr (requires { object->emitter_shape; }) owner.bind_named("emitterShape", [object]() -> decltype(auto) { return (object->emitter_shape); });
  if constexpr (requires { object->emitter_type; }) owner.bind_named("emitterType", [object]() -> decltype(auto) { return (object->emitter_type); });
  if constexpr (requires { object->emitter_width; }) owner.bind_named("emitterWidth", [object]() -> decltype(auto) { return (object->emitter_width); });
  if constexpr (requires { object->enabled; }) owner.bind_named("enabled", [object]() -> decltype(auto) { return (object->enabled); });
  if constexpr (requires { object->encoding; }) owner.bind_named("encoding", [object]() -> decltype(auto) { return (object->encoding); });
  if constexpr (requires { object->end; }) owner.bind_named("end", [object]() -> decltype(auto) { return (object->end); });
  if constexpr (requires { object->end_index; }) owner.bind_named("endIndex", [object]() -> decltype(auto) { return (object->end_index); });
  if constexpr (requires { object->end_x; }) owner.bind_named("endX", [object]() -> decltype(auto) { return (object->end_x); });
  if constexpr (requires { object->end_y; }) owner.bind_named("endY", [object]() -> decltype(auto) { return (object->end_y); });
  if constexpr (requires { object->end_z; }) owner.bind_named("endZ", [object]() -> decltype(auto) { return (object->end_z); });
  if constexpr (requires { object->endianness; }) owner.bind_named("endianness", [object]() -> decltype(auto) { return (object->endianness); });
  if constexpr (requires { object->engine; }) owner.bind_named("engine", [object]() -> decltype(auto) { return (object->engine); });
  if constexpr (requires { object->engine_version; }) owner.bind_named("engineVersion", [object]() -> decltype(auto) { return (object->engine_version); });
  if constexpr (requires { object->explain_spatial_indexing; }) owner.bind_named("explainSpatialIndexing", [object]() -> decltype(auto) { return (object->explain_spatial_indexing); });
  if constexpr (requires { object->exposure; }) owner.bind_named("exposure", [object]() -> decltype(auto) { return (object->exposure); });
  if constexpr (requires { object->exposure_compensation; }) owner.bind_named("exposureCompensation", [object]() -> decltype(auto) { return (object->exposure_compensation); });
  if constexpr (requires { object->far; }) owner.bind_named("far", [object]() -> decltype(auto) { return (object->far); });
  if constexpr (requires { object->feature_id; }) owner.bind_named("featureId", [object]() -> decltype(auto) { return (object->feature_id); });
  if constexpr (requires { object->feedback; }) owner.bind_named("feedback", [object]() -> decltype(auto) { return (object->feedback); });
  if constexpr (requires { object->fill_bounds; }) owner.bind_named("fillBounds", [object]() -> decltype(auto) { return (object->fill_bounds); });
  if constexpr (requires { object->fill_color; }) owner.bind_named("fillColor", [object]() -> decltype(auto) { return (object->fill_color); });
  if constexpr (requires { object->finish_color; }) owner.bind_named("finishColor", [object]() -> decltype(auto) { return (object->finish_color); });
  if constexpr (requires { object->finish_color_variance; }) owner.bind_named("finishColorVariance", [object]() -> decltype(auto) { return (object->finish_color_variance); });
  if constexpr (requires { object->finish_particle_size; }) owner.bind_named("finishParticleSize", [object]() -> decltype(auto) { return (object->finish_particle_size); });
  if constexpr (requires { object->finish_particle_size_variance; }) owner.bind_named("finishParticleSizeVariance", [object]() -> decltype(auto) { return (object->finish_particle_size_variance); });
  if constexpr (requires { object->floor_level; }) owner.bind_named("floorLevel", [object]() -> decltype(auto) { return (object->floor_level); });
  if constexpr (requires { object->font_scale; }) owner.bind_named("fontScale", [object]() -> decltype(auto) { return (object->font_scale); });
  if constexpr (requires { object->force_x; }) owner.bind_named("forceX", [object]() -> decltype(auto) { return (object->force_x); });
  if constexpr (requires { object->force_y; }) owner.bind_named("forceY", [object]() -> decltype(auto) { return (object->force_y); });
  if constexpr (requires { object->force_z; }) owner.bind_named("forceZ", [object]() -> decltype(auto) { return (object->force_z); });
  if constexpr (requires { object->form_factor; }) owner.bind_named("formFactor", [object]() -> decltype(auto) { return (object->form_factor); });
  if constexpr (requires { object->format; }) owner.bind_named("format", [object]() -> decltype(auto) { return (object->format); });
  if constexpr (requires { object->fov_y; }) owner.bind_named("fovY", [object]() -> decltype(auto) { return (object->fov_y); });
  if constexpr (requires { object->fraction; }) owner.bind_named("fraction", [object]() -> decltype(auto) { return (object->fraction); });
  if constexpr (requires { object->frame_count; }) owner.bind_named("frameCount", [object]() -> decltype(auto) { return (object->frame_count); });
  if constexpr (requires { object->frame_duration; }) owner.bind_named("frameDuration", [object]() -> decltype(auto) { return (object->frame_duration); });
  if constexpr (requires { object->frame_durations; }) owner.bind_named("frameDurations", [object]() -> decltype(auto) { return (object->frame_durations); });
  if constexpr (requires { object->frame_id; }) owner.bind_named("frameId", [object]() -> decltype(auto) { return (object->frame_id); });
  if constexpr (requires { object->frame_names; }) owner.bind_named("frameNames", [object]() -> decltype(auto) { return (object->frame_names); });
  if constexpr (requires { object->frame_rate; }) owner.bind_named("frameRate", [object]() -> decltype(auto) { return (object->frame_rate); });
  if constexpr (requires { object->frames; }) owner.bind_named("frames", [object]() -> decltype(auto) { return (object->frames); });
  if constexpr (requires { object->frequency; }) owner.bind_named("frequency", [object]() -> decltype(auto) { return (object->frequency); });
  if constexpr (requires { object->friction; }) owner.bind_named("friction", [object]() -> decltype(auto) { return (object->friction); });
  if constexpr (requires { object->gain; }) owner.bind_named("gain", [object]() -> decltype(auto) { return (object->gain); });
  if constexpr (requires { object->gamma; }) owner.bind_named("gamma", [object]() -> decltype(auto) { return (object->gamma); });
  if constexpr (requires { object->gate_weave; }) owner.bind_named("gateWeave", [object]() -> decltype(auto) { return (object->gate_weave); });
  if constexpr (requires { object->get_capabilities; }) owner.bind_named("getCapabilities", [object]() -> decltype(auto) { return (object->get_capabilities); });
  if constexpr (requires { object->get_current_position; }) owner.bind_named("getCurrentPosition", [object]() -> decltype(auto) { return (object->get_current_position); });
  if constexpr (requires { object->get_current_position_result; }) owner.bind_named("getCurrentPositionResult", [object]() -> decltype(auto) { return (object->get_current_position_result); });
  if constexpr (requires { object->get_display_metrics; }) owner.bind_named("getDisplayMetrics", [object]() -> decltype(auto) { return (object->get_display_metrics); });
  if constexpr (requires { object->get_formats; }) owner.bind_named("getFormats", [object]() -> decltype(auto) { return (object->get_formats); });
  if constexpr (requires { object->get_glyph_atlas_image; }) owner.bind_named("getGlyphAtlasImage", [object]() -> decltype(auto) { return (object->get_glyph_atlas_image); });
  if constexpr (requires { object->get_glyph_entry; }) owner.bind_named("getGlyphEntry", [object]() -> decltype(auto) { return (object->get_glyph_entry); });
  if constexpr (requires { object->get_glyph_kerning; }) owner.bind_named("getGlyphKerning", [object]() -> decltype(auto) { return (object->get_glyph_kerning); });
  if constexpr (requires { object->get_glyph_layout_version; }) owner.bind_named("getGlyphLayoutVersion", [object]() -> decltype(auto) { return (object->get_glyph_layout_version); });
  if constexpr (requires { object->get_glyph_metrics; }) owner.bind_named("getGlyphMetrics", [object]() -> decltype(auto) { return (object->get_glyph_metrics); });
  if constexpr (requires { object->get_id; }) owner.bind_named("getId", [object]() -> decltype(auto) { return (object->get_id); });
  if constexpr (requires { object->get_info; }) owner.bind_named("getInfo", [object]() -> decltype(auto) { return (object->get_info); });
  if constexpr (requires { object->get_permission; }) owner.bind_named("getPermission", [object]() -> decltype(auto) { return (object->get_permission); });
  if constexpr (requires { object->get_permission_state; }) owner.bind_named("getPermissionState", [object]() -> decltype(auto) { return (object->get_permission_state); });
  if constexpr (requires { object->get_persistence; }) owner.bind_named("getPersistence", [object]() -> decltype(auto) { return (object->get_persistence); });
  if constexpr (requires { object->get_safe_area_insets; }) owner.bind_named("getSafeAreaInsets", [object]() -> decltype(auto) { return (object->get_safe_area_insets); });
  if constexpr (requires { object->ghosts; }) owner.bind_named("ghosts", [object]() -> decltype(auto) { return (object->ghosts); });
  if constexpr (requires { object->glyphs; }) owner.bind_named("glyphs", [object]() -> decltype(auto) { return (object->glyphs); });
  if constexpr (requires { object->gpu_renderer; }) owner.bind_named("gpuRenderer", [object]() -> decltype(auto) { return (object->gpu_renderer); });
  if constexpr (requires { object->gpu_vendor; }) owner.bind_named("gpuVendor", [object]() -> decltype(auto) { return (object->gpu_vendor); });
  if constexpr (requires { object->grain_intensity; }) owner.bind_named("grainIntensity", [object]() -> decltype(auto) { return (object->grain_intensity); });
  if constexpr (requires { object->gravity; }) owner.bind_named("gravity", [object]() -> decltype(auto) { return (object->gravity); });
  if constexpr (requires { object->gravity_x; }) owner.bind_named("gravityX", [object]() -> decltype(auto) { return (object->gravity_x); });
  if constexpr (requires { object->gravity_y; }) owner.bind_named("gravityY", [object]() -> decltype(auto) { return (object->gravity_y); });
  if constexpr (requires { object->gravity_z; }) owner.bind_named("gravityZ", [object]() -> decltype(auto) { return (object->gravity_z); });
  if constexpr (requires { object->gravityx; }) owner.bind_named("gravityx", [object]() -> decltype(auto) { return (object->gravityx); });
  if constexpr (requires { object->gravityy; }) owner.bind_named("gravityy", [object]() -> decltype(auto) { return (object->gravityy); });
  if constexpr (requires { object->green; }) owner.bind_named("green", [object]() -> decltype(auto) { return (object->green); });
  if constexpr (requires { object->green_bias; }) owner.bind_named("greenBias", [object]() -> decltype(auto) { return (object->green_bias); });
  if constexpr (requires { object->green_scale; }) owner.bind_named("greenScale", [object]() -> decltype(auto) { return (object->green_scale); });
  if constexpr (requires { object->ground_color; }) owner.bind_named("groundColor", [object]() -> decltype(auto) { return (object->ground_color); });
  if constexpr (requires { object->halation_radius; }) owner.bind_named("halationRadius", [object]() -> decltype(auto) { return (object->halation_radius); });
  if constexpr (requires { object->halation_strength; }) owner.bind_named("halationStrength", [object]() -> decltype(auto) { return (object->halation_strength); });
  if constexpr (requires { object->half_extent_x; }) owner.bind_named("halfExtentX", [object]() -> decltype(auto) { return (object->half_extent_x); });
  if constexpr (requires { object->half_extent_y; }) owner.bind_named("halfExtentY", [object]() -> decltype(auto) { return (object->half_extent_y); });
  if constexpr (requires { object->half_extent_z; }) owner.bind_named("halfExtentZ", [object]() -> decltype(auto) { return (object->half_extent_z); });
  if constexpr (requires { object->half_h; }) owner.bind_named("halfH", [object]() -> decltype(auto) { return (object->half_h); });
  if constexpr (requires { object->half_w; }) owner.bind_named("halfW", [object]() -> decltype(auto) { return (object->half_w); });
  if constexpr (requires { object->halo; }) owner.bind_named("halo", [object]() -> decltype(auto) { return (object->halo); });
  if constexpr (requires { object->handle; }) owner.bind_named("handle", [object]() -> decltype(auto) { return (object->handle); });
  if constexpr (requires { object->has_format; }) owner.bind_named("hasFormat", [object]() -> decltype(auto) { return (object->has_format); });
  if constexpr (requires { object->has_image; }) owner.bind_named("hasImage", [object]() -> decltype(auto) { return (object->has_image); });
  if constexpr (requires { object->has_keyboard; }) owner.bind_named("hasKeyboard", [object]() -> decltype(auto) { return (object->has_keyboard); });
  if constexpr (requires { object->has_mouse; }) owner.bind_named("hasMouse", [object]() -> decltype(auto) { return (object->has_mouse); });
  if constexpr (requires { object->has_stylus; }) owner.bind_named("hasStylus", [object]() -> decltype(auto) { return (object->has_stylus); });
  if constexpr (requires { object->has_text; }) owner.bind_named("hasText", [object]() -> decltype(auto) { return (object->has_text); });
  if constexpr (requires { object->heading; }) owner.bind_named("heading", [object]() -> decltype(auto) { return (object->heading); });
  if constexpr (requires { object->height; }) owner.bind_named("height", [object]() -> decltype(auto) { return (object->height); });
  if constexpr (requires { object->hemisphere; }) owner.bind_named("hemisphere", [object]() -> decltype(auto) { return (object->hemisphere); });
  if constexpr (requires { object->hide; }) owner.bind_named("hide", [object]() -> decltype(auto) { return (object->hide); });
  if constexpr (requires { object->high_max; }) owner.bind_named("highMax", [object]() -> decltype(auto) { return (object->high_max); });
  if constexpr (requires { object->high_min; }) owner.bind_named("highMin", [object]() -> decltype(auto) { return (object->high_min); });
  if constexpr (requires { object->hue; }) owner.bind_named("hue", [object]() -> decltype(auto) { return (object->hue); });
  if constexpr (requires { object->id; }) owner.bind_named("id", [object]() -> decltype(auto) { return (object->id); });
  if constexpr (requires { object->illuminance; }) owner.bind_named("illuminance", [object]() -> decltype(auto) { return (object->illuminance); });
  if constexpr (requires { object->image_count; }) owner.bind_named("imageCount", [object]() -> decltype(auto) { return (object->image_count); });
  if constexpr (requires { object->image_file; }) owner.bind_named("imageFile", [object]() -> decltype(auto) { return (object->image_file); });
  if constexpr (requires { object->image_height; }) owner.bind_named("imageHeight", [object]() -> decltype(auto) { return (object->image_height); });
  if constexpr (requires { object->image_path; }) owner.bind_named("imagePath", [object]() -> decltype(auto) { return (object->image_path); });
  if constexpr (requires { object->image_width; }) owner.bind_named("imageWidth", [object]() -> decltype(auto) { return (object->image_width); });
  if constexpr (requires { object->impact; }) owner.bind_named("impact", [object]() -> decltype(auto) { return (object->impact); });
  if constexpr (requires { object->influence_counts; }) owner.bind_named("influenceCounts", [object]() -> decltype(auto) { return (object->influence_counts); });
  if constexpr (requires { object->influences; }) owner.bind_named("influences", [object]() -> decltype(auto) { return (object->influences); });
  if constexpr (requires { object->inner_cone_cos; }) owner.bind_named("innerConeCos", [object]() -> decltype(auto) { return (object->inner_cone_cos); });
  if constexpr (requires { object->inner_cone_degrees; }) owner.bind_named("innerConeDegrees", [object]() -> decltype(auto) { return (object->inner_cone_degrees); });
  if constexpr (requires { object->insert_spatial_object; }) owner.bind_named("insertSpatialObject", [object]() -> decltype(auto) { return (object->insert_spatial_object); });
  if constexpr (requires { object->intensity; }) owner.bind_named("intensity", [object]() -> decltype(auto) { return (object->intensity); });
  if constexpr (requires { object->intensity_unit; }) owner.bind_named("intensityUnit", [object]() -> decltype(auto) { return (object->intensity_unit); });
  if constexpr (requires { object->interval; }) owner.bind_named("interval", [object]() -> decltype(auto) { return (object->interval); });
  if constexpr (requires { object->invoke; }) owner.bind_named("invoke", [object]() -> decltype(auto) { return (object->invoke); });
  if constexpr (requires { object->ior; }) owner.bind_named("ior", [object]() -> decltype(auto) { return (object->ior); });
  if constexpr (requires { object->iridescence; }) owner.bind_named("iridescence", [object]() -> decltype(auto) { return (object->iridescence); });
  if constexpr (requires { object->iridescence_ior; }) owner.bind_named("iridescenceIor", [object]() -> decltype(auto) { return (object->iridescence_ior); });
  if constexpr (requires { object->iridescence_map; }) owner.bind_named("iridescenceMap", [object]() -> decltype(auto) { return (object->iridescence_map); });
  if constexpr (requires { object->iridescence_map_uv_set; }) owner.bind_named("iridescenceMapUvSet", [object]() -> decltype(auto) { return (object->iridescence_map_uv_set); });
  if constexpr (requires { object->iridescence_thickness_map; }) owner.bind_named("iridescenceThicknessMap", [object]() -> decltype(auto) { return (object->iridescence_thickness_map); });
  if constexpr (requires { object->iridescence_thickness_map_uv_set; }) owner.bind_named("iridescenceThicknessMapUvSet", [object]() -> decltype(auto) { return (object->iridescence_thickness_map_uv_set); });
  if constexpr (requires { object->iridescence_thickness_max; }) owner.bind_named("iridescenceThicknessMax", [object]() -> decltype(auto) { return (object->iridescence_thickness_max); });
  if constexpr (requires { object->iridescence_thickness_min; }) owner.bind_named("iridescenceThicknessMin", [object]() -> decltype(auto) { return (object->iridescence_thickness_min); });
  if constexpr (requires { object->is_ambient_light_supported; }) owner.bind_named("isAmbientLightSupported", [object]() -> decltype(auto) { return (object->is_ambient_light_supported); });
  if constexpr (requires { object->is_available; }) owner.bind_named("isAvailable", [object]() -> decltype(auto) { return (object->is_available); });
  if constexpr (requires { object->is_barometer_supported; }) owner.bind_named("isBarometerSupported", [object]() -> decltype(auto) { return (object->is_barometer_supported); });
  if constexpr (requires { object->is_gravity_supported; }) owner.bind_named("isGravitySupported", [object]() -> decltype(auto) { return (object->is_gravity_supported); });
  if constexpr (requires { object->is_gyroscope_supported; }) owner.bind_named("isGyroscopeSupported", [object]() -> decltype(auto) { return (object->is_gyroscope_supported); });
  if constexpr (requires { object->is_hdr; }) owner.bind_named("isHdr", [object]() -> decltype(auto) { return (object->is_hdr); });
  if constexpr (requires { object->is_jailbroken; }) owner.bind_named("isJailbroken", [object]() -> decltype(auto) { return (object->is_jailbroken); });
  if constexpr (requires { object->is_linear_acceleration_supported; }) owner.bind_named("isLinearAccelerationSupported", [object]() -> decltype(auto) { return (object->is_linear_acceleration_supported); });
  if constexpr (requires { object->is_low_end_device; }) owner.bind_named("isLowEndDevice", [object]() -> decltype(auto) { return (object->is_low_end_device); });
  if constexpr (requires { object->is_magnetometer_supported; }) owner.bind_named("isMagnetometerSupported", [object]() -> decltype(auto) { return (object->is_magnetometer_supported); });
  if constexpr (requires { object->is_motion_supported; }) owner.bind_named("isMotionSupported", [object]() -> decltype(auto) { return (object->is_motion_supported); });
  if constexpr (requires { object->is_orientation_supported; }) owner.bind_named("isOrientationSupported", [object]() -> decltype(auto) { return (object->is_orientation_supported); });
  if constexpr (requires { object->is_proximity_supported; }) owner.bind_named("isProximitySupported", [object]() -> decltype(auto) { return (object->is_proximity_supported); });
  if constexpr (requires { object->is_rooted; }) owner.bind_named("isRooted", [object]() -> decltype(auto) { return (object->is_rooted); });
  if constexpr (requires { object->is_supported; }) owner.bind_named("isSupported", [object]() -> decltype(auto) { return (object->is_supported); });
  if constexpr (requires { object->is_touch; }) owner.bind_named("isTouch", [object]() -> decltype(auto) { return (object->is_touch); });
  if constexpr (requires { object->is_virtual; }) owner.bind_named("isVirtual", [object]() -> decltype(auto) { return (object->is_virtual); });
  if constexpr (requires { object->joint_collision_suppressions; }) owner.bind_named("jointCollisionSuppressions", [object]() -> decltype(auto) { return (object->joint_collision_suppressions); });
  if constexpr (requires { object->joint_solvers; }) owner.bind_named("jointSolvers", [object]() -> decltype(auto) { return (object->joint_solvers); });
  if constexpr (requires { object->kerning; }) owner.bind_named("kerning", [object]() -> decltype(auto) { return (object->kerning); });
  if constexpr (requires { object->key; }) owner.bind_named("key", [object]() -> decltype(auto) { return (object->key); });
  if constexpr (requires { object->kind; }) owner.bind_named("kind", [object]() -> decltype(auto) { return (object->kind); });
  if constexpr (requires { object->latitude; }) owner.bind_named("latitude", [object]() -> decltype(auto) { return (object->latitude); });
  if constexpr (requires { object->layer_mask; }) owner.bind_named("layerMask", [object]() -> decltype(auto) { return (object->layer_mask); });
  if constexpr (requires { object->leading; }) owner.bind_named("leading", [object]() -> decltype(auto) { return (object->leading); });
  if constexpr (requires { object->leaf_by_object; }) owner.bind_named("leafByObject", [object]() -> decltype(auto) { return (object->leaf_by_object); });
  if constexpr (requires { object->left; }) owner.bind_named("left", [object]() -> decltype(auto) { return (object->left); });
  if constexpr (requires { object->levels; }) owner.bind_named("levels", [object]() -> decltype(auto) { return (object->levels); });
  if constexpr (requires { object->life; }) owner.bind_named("life", [object]() -> decltype(auto) { return (object->life); });
  if constexpr (requires { object->life_offset; }) owner.bind_named("lifeOffset", [object]() -> decltype(auto) { return (object->life_offset); });
  if constexpr (requires { object->lifetime_max; }) owner.bind_named("lifetimeMax", [object]() -> decltype(auto) { return (object->lifetime_max); });
  if constexpr (requires { object->lifetime_min; }) owner.bind_named("lifetimeMin", [object]() -> decltype(auto) { return (object->lifetime_min); });
  if constexpr (requires { object->lift; }) owner.bind_named("lift", [object]() -> decltype(auto) { return (object->lift); });
  if constexpr (requires { object->light_color; }) owner.bind_named("lightColor", [object]() -> decltype(auto) { return (object->light_color); });
  if constexpr (requires { object->light_x; }) owner.bind_named("lightX", [object]() -> decltype(auto) { return (object->light_x); });
  if constexpr (requires { object->light_y; }) owner.bind_named("lightY", [object]() -> decltype(auto) { return (object->light_y); });
  if constexpr (requires { object->lightness; }) owner.bind_named("lightness", [object]() -> decltype(auto) { return (object->lightness); });
  if constexpr (requires { object->line_index; }) owner.bind_named("lineIndex", [object]() -> decltype(auto) { return (object->line_index); });
  if constexpr (requires { object->linear_length; }) owner.bind_named("linearLength", [object]() -> decltype(auto) { return (object->linear_length); });
  if constexpr (requires { object->linear_start; }) owner.bind_named("linearStart", [object]() -> decltype(auto) { return (object->linear_start); });
  if constexpr (requires { object->locale; }) owner.bind_named("locale", [object]() -> decltype(auto) { return (object->locale); });
  if constexpr (requires { object->logical_height; }) owner.bind_named("logicalHeight", [object]() -> decltype(auto) { return (object->logical_height); });
  if constexpr (requires { object->logical_width; }) owner.bind_named("logicalWidth", [object]() -> decltype(auto) { return (object->logical_width); });
  if constexpr (requires { object->longitude; }) owner.bind_named("longitude", [object]() -> decltype(auto) { return (object->longitude); });
  if constexpr (requires { object->loop; }) owner.bind_named("loop", [object]() -> decltype(auto) { return (object->loop); });
  if constexpr (requires { object->low_max; }) owner.bind_named("lowMax", [object]() -> decltype(auto) { return (object->low_max); });
  if constexpr (requires { object->low_min; }) owner.bind_named("lowMin", [object]() -> decltype(auto) { return (object->low_min); });
  if constexpr (requires { object->lut; }) owner.bind_named("lut", [object]() -> decltype(auto) { return (object->lut); });
  if constexpr (requires { object->m; }) owner.bind_named("m", [object]() -> decltype(auto) { return (object->m); });
  if constexpr (requires { object->mag_filter; }) owner.bind_named("magFilter", [object]() -> decltype(auto) { return (object->mag_filter); });
  if constexpr (requires { object->manufacturer; }) owner.bind_named("manufacturer", [object]() -> decltype(auto) { return (object->manufacturer); });
  if constexpr (requires { object->map; }) owner.bind_named("map", [object]() -> decltype(auto) { return (object->map); });
  if constexpr (requires { object->margin; }) owner.bind_named("margin", [object]() -> decltype(auto) { return (object->margin); });
  if constexpr (requires { object->marketing_name; }) owner.bind_named("marketingName", [object]() -> decltype(auto) { return (object->marketing_name); });
  if constexpr (requires { object->material; }) owner.bind_named("material", [object]() -> decltype(auto) { return (object->material); });
  if constexpr (requires { object->material_data; }) owner.bind_named("materialData", [object]() -> decltype(auto) { return (object->material_data); });
  if constexpr (requires { object->matrix; }) owner.bind_named("matrix", [object]() -> decltype(auto) { return (object->matrix); });
  if constexpr (requires { object->matrix_x; }) owner.bind_named("matrixX", [object]() -> decltype(auto) { return (object->matrix_x); });
  if constexpr (requires { object->matrix_y; }) owner.bind_named("matrixY", [object]() -> decltype(auto) { return (object->matrix_y); });
  if constexpr (requires { object->max; }) owner.bind_named("max", [object]() -> decltype(auto) { return (object->max); });
  if constexpr (requires { object->max_brightness; }) owner.bind_named("maxBrightness", [object]() -> decltype(auto) { return (object->max_brightness); });
  if constexpr (requires { object->max_distance; }) owner.bind_named("maxDistance", [object]() -> decltype(auto) { return (object->max_distance); });
  if constexpr (requires { object->max_ev; }) owner.bind_named("maxEv", [object]() -> decltype(auto) { return (object->max_ev); });
  if constexpr (requires { object->max_exposure; }) owner.bind_named("maxExposure", [object]() -> decltype(auto) { return (object->max_exposure); });
  if constexpr (requires { object->max_particle_count; }) owner.bind_named("maxParticleCount", [object]() -> decltype(auto) { return (object->max_particle_count); });
  if constexpr (requires { object->max_particles; }) owner.bind_named("maxParticles", [object]() -> decltype(auto) { return (object->max_particles); });
  if constexpr (requires { object->max_radius; }) owner.bind_named("maxRadius", [object]() -> decltype(auto) { return (object->max_radius); });
  if constexpr (requires { object->max_radius_variance; }) owner.bind_named("maxRadiusVariance", [object]() -> decltype(auto) { return (object->max_radius_variance); });
  if constexpr (requires { object->max_x; }) owner.bind_named("maxX", [object]() -> decltype(auto) { return (object->max_x); });
  if constexpr (requires { object->max_y; }) owner.bind_named("maxY", [object]() -> decltype(auto) { return (object->max_y); });
  if constexpr (requires { object->max_z; }) owner.bind_named("maxZ", [object]() -> decltype(auto) { return (object->max_z); });
  if constexpr (requires { object->metadata; }) owner.bind_named("metadata", [object]() -> decltype(auto) { return (object->metadata); });
  if constexpr (requires { object->metrics; }) owner.bind_named("metrics", [object]() -> decltype(auto) { return (object->metrics); });
  if constexpr (requires { object->min; }) owner.bind_named("min", [object]() -> decltype(auto) { return (object->min); });
  if constexpr (requires { object->min_ev; }) owner.bind_named("minEv", [object]() -> decltype(auto) { return (object->min_ev); });
  if constexpr (requires { object->min_exposure; }) owner.bind_named("minExposure", [object]() -> decltype(auto) { return (object->min_exposure); });
  if constexpr (requires { object->min_filter; }) owner.bind_named("minFilter", [object]() -> decltype(auto) { return (object->min_filter); });
  if constexpr (requires { object->min_particle_count; }) owner.bind_named("minParticleCount", [object]() -> decltype(auto) { return (object->min_particle_count); });
  if constexpr (requires { object->min_radius; }) owner.bind_named("minRadius", [object]() -> decltype(auto) { return (object->min_radius); });
  if constexpr (requires { object->min_radius_variance; }) owner.bind_named("minRadiusVariance", [object]() -> decltype(auto) { return (object->min_radius_variance); });
  if constexpr (requires { object->min_x; }) owner.bind_named("minX", [object]() -> decltype(auto) { return (object->min_x); });
  if constexpr (requires { object->min_y; }) owner.bind_named("minY", [object]() -> decltype(auto) { return (object->min_y); });
  if constexpr (requires { object->min_z; }) owner.bind_named("minZ", [object]() -> decltype(auto) { return (object->min_z); });
  if constexpr (requires { object->mipmaps; }) owner.bind_named("mipmaps", [object]() -> decltype(auto) { return (object->mipmaps); });
  if constexpr (requires { object->mode; }) owner.bind_named("mode", [object]() -> decltype(auto) { return (object->mode); });
  if constexpr (requires { object->model; }) owner.bind_named("model", [object]() -> decltype(auto) { return (object->model); });
  if constexpr (requires { object->modifiers; }) owner.bind_named("modifiers", [object]() -> decltype(auto) { return (object->modifiers); });
  if constexpr (requires { object->name; }) owner.bind_named("name", [object]() -> decltype(auto) { return (object->name); });
  if constexpr (requires { object->near; }) owner.bind_named("near", [object]() -> decltype(auto) { return (object->near); });
  if constexpr (requires { object->normal_bias; }) owner.bind_named("normalBias", [object]() -> decltype(auto) { return (object->normal_bias); });
  if constexpr (requires { object->normal_x; }) owner.bind_named("normalX", [object]() -> decltype(auto) { return (object->normal_x); });
  if constexpr (requires { object->normal_y; }) owner.bind_named("normalY", [object]() -> decltype(auto) { return (object->normal_y); });
  if constexpr (requires { object->normal_z; }) owner.bind_named("normalZ", [object]() -> decltype(auto) { return (object->normal_z); });
  if constexpr (requires { object->notification; }) owner.bind_named("notification", [object]() -> decltype(auto) { return (object->notification); });
  if constexpr (requires { object->num_lines; }) owner.bind_named("numLines", [object]() -> decltype(auto) { return (object->num_lines); });
  if constexpr (requires { object->object; }) owner.bind_named("object", [object]() -> decltype(auto) { return (object->object); });
  if constexpr (requires { object->offset; }) owner.bind_named("offset", [object]() -> decltype(auto) { return (object->offset); });
  if constexpr (requires { object->offset_x; }) owner.bind_named("offsetX", [object]() -> decltype(auto) { return (object->offset_x); });
  if constexpr (requires { object->offset_y; }) owner.bind_named("offsetY", [object]() -> decltype(auto) { return (object->offset_y); });
  if constexpr (requires { object->offsets; }) owner.bind_named("offsets", [object]() -> decltype(auto) { return (object->offsets); });
  if constexpr (requires { object->on_absolute_orientation; }) owner.bind_named("onAbsoluteOrientation", [object]() -> decltype(auto) { return (object->on_absolute_orientation); });
  if constexpr (requires { object->on_accelerometer; }) owner.bind_named("onAccelerometer", [object]() -> decltype(auto) { return (object->on_accelerometer); });
  if constexpr (requires { object->on_action; }) owner.bind_named("onAction", [object]() -> decltype(auto) { return (object->on_action); });
  if constexpr (requires { object->on_ambient_light; }) owner.bind_named("onAmbientLight", [object]() -> decltype(auto) { return (object->on_ambient_light); });
  if constexpr (requires { object->on_barometer; }) owner.bind_named("onBarometer", [object]() -> decltype(auto) { return (object->on_barometer); });
  if constexpr (requires { object->on_change; }) owner.bind_named("onChange", [object]() -> decltype(auto) { return (object->on_change); });
  if constexpr (requires { object->on_emitter_complete; }) owner.bind_named("onEmitterComplete", [object]() -> decltype(auto) { return (object->on_emitter_complete); });
  if constexpr (requires { object->on_gravity; }) owner.bind_named("onGravity", [object]() -> decltype(auto) { return (object->on_gravity); });
  if constexpr (requires { object->on_gyroscope; }) owner.bind_named("onGyroscope", [object]() -> decltype(auto) { return (object->on_gyroscope); });
  if constexpr (requires { object->on_hide; }) owner.bind_named("onHide", [object]() -> decltype(auto) { return (object->on_hide); });
  if constexpr (requires { object->on_linear_acceleration; }) owner.bind_named("onLinearAcceleration", [object]() -> decltype(auto) { return (object->on_linear_acceleration); });
  if constexpr (requires { object->on_magnetometer; }) owner.bind_named("onMagnetometer", [object]() -> decltype(auto) { return (object->on_magnetometer); });
  if constexpr (requires { object->on_orientation; }) owner.bind_named("onOrientation", [object]() -> decltype(auto) { return (object->on_orientation); });
  if constexpr (requires { object->on_particle_death; }) owner.bind_named("onParticleDeath", [object]() -> decltype(auto) { return (object->on_particle_death); });
  if constexpr (requires { object->on_particle_spawn; }) owner.bind_named("onParticleSpawn", [object]() -> decltype(auto) { return (object->on_particle_spawn); });
  if constexpr (requires { object->on_proximity; }) owner.bind_named("onProximity", [object]() -> decltype(auto) { return (object->on_proximity); });
  if constexpr (requires { object->on_quaternion; }) owner.bind_named("onQuaternion", [object]() -> decltype(auto) { return (object->on_quaternion); });
  if constexpr (requires { object->on_resize; }) owner.bind_named("onResize", [object]() -> decltype(auto) { return (object->on_resize); });
  if constexpr (requires { object->on_show; }) owner.bind_named("onShow", [object]() -> decltype(auto) { return (object->on_show); });
  if constexpr (requires { object->on_tick; }) owner.bind_named("onTick", [object]() -> decltype(auto) { return (object->on_tick); });
  if constexpr (requires { object->once; }) owner.bind_named("once", [object]() -> decltype(auto) { return (object->once); });
  if constexpr (requires { object->operation; }) owner.bind_named("operation", [object]() -> decltype(auto) { return (object->operation); });
  if constexpr (requires { object->operator_; }) owner.bind_named("operator", [object]() -> decltype(auto) { return (object->operator_); });
  if constexpr (requires { object->orientation_w; }) owner.bind_named("orientationW", [object]() -> decltype(auto) { return (object->orientation_w); });
  if constexpr (requires { object->orientation_x; }) owner.bind_named("orientationX", [object]() -> decltype(auto) { return (object->orientation_x); });
  if constexpr (requires { object->orientation_y; }) owner.bind_named("orientationY", [object]() -> decltype(auto) { return (object->orientation_y); });
  if constexpr (requires { object->orientation_z; }) owner.bind_named("orientationZ", [object]() -> decltype(auto) { return (object->orientation_z); });
  if constexpr (requires { object->origin; }) owner.bind_named("origin", [object]() -> decltype(auto) { return (object->origin); });
  if constexpr (requires { object->origin_x; }) owner.bind_named("originX", [object]() -> decltype(auto) { return (object->origin_x); });
  if constexpr (requires { object->origin_y; }) owner.bind_named("originY", [object]() -> decltype(auto) { return (object->origin_y); });
  if constexpr (requires { object->os_build; }) owner.bind_named("osBuild", [object]() -> decltype(auto) { return (object->os_build); });
  if constexpr (requires { object->os_name; }) owner.bind_named("osName", [object]() -> decltype(auto) { return (object->os_name); });
  if constexpr (requires { object->os_version; }) owner.bind_named("osVersion", [object]() -> decltype(auto) { return (object->os_version); });
  if constexpr (requires { object->outer_cone_cos; }) owner.bind_named("outerConeCos", [object]() -> decltype(auto) { return (object->outer_cone_cos); });
  if constexpr (requires { object->outer_cone_degrees; }) owner.bind_named("outerConeDegrees", [object]() -> decltype(auto) { return (object->outer_cone_degrees); });
  if constexpr (requires { object->overlapping; }) owner.bind_named("overlapping", [object]() -> decltype(auto) { return (object->overlapping); });
  if constexpr (requires { object->overlays_content; }) owner.bind_named("overlaysContent", [object]() -> decltype(auto) { return (object->overlays_content); });
  if constexpr (requires { object->pages; }) owner.bind_named("pages", [object]() -> decltype(auto) { return (object->pages); });
  if constexpr (requires { object->parent; }) owner.bind_named("parent", [object]() -> decltype(auto) { return (object->parent); });
  if constexpr (requires { object->particle_lifespan; }) owner.bind_named("particleLifespan", [object]() -> decltype(auto) { return (object->particle_lifespan); });
  if constexpr (requires { object->particle_lifespan_variance; }) owner.bind_named("particleLifespanVariance", [object]() -> decltype(auto) { return (object->particle_lifespan_variance); });
  if constexpr (requires { object->paused; }) owner.bind_named("paused", [object]() -> decltype(auto) { return (object->paused); });
  if constexpr (requires { object->pcf_radius; }) owner.bind_named("pcfRadius", [object]() -> decltype(auto) { return (object->pcf_radius); });
  if constexpr (requires { object->pedestal; }) owner.bind_named("pedestal", [object]() -> decltype(auto) { return (object->pedestal); });
  if constexpr (requires { object->physical_height; }) owner.bind_named("physicalHeight", [object]() -> decltype(auto) { return (object->physical_height); });
  if constexpr (requires { object->physical_width; }) owner.bind_named("physicalWidth", [object]() -> decltype(auto) { return (object->physical_width); });
  if constexpr (requires { object->pivot_x; }) owner.bind_named("pivotX", [object]() -> decltype(auto) { return (object->pivot_x); });
  if constexpr (requires { object->pivot_y; }) owner.bind_named("pivotY", [object]() -> decltype(auto) { return (object->pivot_y); });
  if constexpr (requires { object->pixel_ratio; }) owner.bind_named("pixelRatio", [object]() -> decltype(auto) { return (object->pixel_ratio); });
  if constexpr (requires { object->platform_string; }) owner.bind_named("platformString", [object]() -> decltype(auto) { return (object->platform_string); });
  if constexpr (requires { object->playback_rate; }) owner.bind_named("playbackRate", [object]() -> decltype(auto) { return (object->playback_rate); });
  if constexpr (requires { object->point; }) owner.bind_named("point", [object]() -> decltype(auto) { return (object->point); });
  if constexpr (requires { object->point_count; }) owner.bind_named("pointCount", [object]() -> decltype(auto) { return (object->point_count); });
  if constexpr (requires { object->pointer_width; }) owner.bind_named("pointerWidth", [object]() -> decltype(auto) { return (object->pointer_width); });
  if constexpr (requires { object->points; }) owner.bind_named("points", [object]() -> decltype(auto) { return (object->points); });
  if constexpr (requires { object->position; }) owner.bind_named("position", [object]() -> decltype(auto) { return (object->position); });
  if constexpr (requires { object->positions; }) owner.bind_named("positions", [object]() -> decltype(auto) { return (object->positions); });
  if constexpr (requires { object->premultiplied_alpha; }) owner.bind_named("premultipliedAlpha", [object]() -> decltype(auto) { return (object->premultiplied_alpha); });
  if constexpr (requires { object->prepare; }) owner.bind_named("prepare", [object]() -> decltype(auto) { return (object->prepare); });
  if constexpr (requires { object->preserve_alpha; }) owner.bind_named("preserveAlpha", [object]() -> decltype(auto) { return (object->preserve_alpha); });
  if constexpr (requires { object->pressure; }) owner.bind_named("pressure", [object]() -> decltype(auto) { return (object->pressure); });
  if constexpr (requires { object->previous_world_transform; }) owner.bind_named("previousWorldTransform", [object]() -> decltype(auto) { return (object->previous_world_transform); });
  if constexpr (requires { object->priority; }) owner.bind_named("priority", [object]() -> decltype(auto) { return (object->priority); });
  if constexpr (requires { object->product_name; }) owner.bind_named("productName", [object]() -> decltype(auto) { return (object->product_name); });
  if constexpr (requires { object->projection; }) owner.bind_named("projection", [object]() -> decltype(auto) { return (object->projection); });
  if constexpr (requires { object->prompt_for_access; }) owner.bind_named("promptForAccess", [object]() -> decltype(auto) { return (object->prompt_for_access); });
  if constexpr (requires { object->query_spatial_pairs; }) owner.bind_named("querySpatialPairs", [object]() -> decltype(auto) { return (object->query_spatial_pairs); });
  if constexpr (requires { object->query_spatial_point; }) owner.bind_named("querySpatialPoint", [object]() -> decltype(auto) { return (object->query_spatial_point); });
  if constexpr (requires { object->query_spatial_ray; }) owner.bind_named("querySpatialRay", [object]() -> decltype(auto) { return (object->query_spatial_ray); });
  if constexpr (requires { object->query_spatial_region; }) owner.bind_named("querySpatialRegion", [object]() -> decltype(auto) { return (object->query_spatial_region); });
  if constexpr (requires { object->r_ax; }) owner.bind_named("rAX", [object]() -> decltype(auto) { return (object->r_ax); });
  if constexpr (requires { object->r_ay; }) owner.bind_named("rAY", [object]() -> decltype(auto) { return (object->r_ay); });
  if constexpr (requires { object->r_az; }) owner.bind_named("rAZ", [object]() -> decltype(auto) { return (object->r_az); });
  if constexpr (requires { object->r_bx; }) owner.bind_named("rBX", [object]() -> decltype(auto) { return (object->r_bx); });
  if constexpr (requires { object->r_by; }) owner.bind_named("rBY", [object]() -> decltype(auto) { return (object->r_by); });
  if constexpr (requires { object->r_bz; }) owner.bind_named("rBZ", [object]() -> decltype(auto) { return (object->r_bz); });
  if constexpr (requires { object->radial; }) owner.bind_named("radial", [object]() -> decltype(auto) { return (object->radial); });
  if constexpr (requires { object->radial_accel_variance; }) owner.bind_named("radialAccelVariance", [object]() -> decltype(auto) { return (object->radial_accel_variance); });
  if constexpr (requires { object->radial_acceleration; }) owner.bind_named("radialAcceleration", [object]() -> decltype(auto) { return (object->radial_acceleration); });
  if constexpr (requires { object->radius; }) owner.bind_named("radius", [object]() -> decltype(auto) { return (object->radius); });
  if constexpr (requires { object->range; }) owner.bind_named("range", [object]() -> decltype(auto) { return (object->range); });
  if constexpr (requires { object->read_bookmark; }) owner.bind_named("readBookmark", [object]() -> decltype(auto) { return (object->read_bookmark); });
  if constexpr (requires { object->read_format; }) owner.bind_named("readFormat", [object]() -> decltype(auto) { return (object->read_format); });
  if constexpr (requires { object->read_html; }) owner.bind_named("readHtml", [object]() -> decltype(auto) { return (object->read_html); });
  if constexpr (requires { object->read_image; }) owner.bind_named("readImage", [object]() -> decltype(auto) { return (object->read_image); });
  if constexpr (requires { object->read_items; }) owner.bind_named("readItems", [object]() -> decltype(auto) { return (object->read_items); });
  if constexpr (requires { object->read_rtf; }) owner.bind_named("readRTF", [object]() -> decltype(auto) { return (object->read_rtf); });
  if constexpr (requires { object->read_text; }) owner.bind_named("readText", [object]() -> decltype(auto) { return (object->read_text); });
  if constexpr (requires { object->reason; }) owner.bind_named("reason", [object]() -> decltype(auto) { return (object->reason); });
  if constexpr (requires { object->red; }) owner.bind_named("red", [object]() -> decltype(auto) { return (object->red); });
  if constexpr (requires { object->red_bias; }) owner.bind_named("redBias", [object]() -> decltype(auto) { return (object->red_bias); });
  if constexpr (requires { object->red_scale; }) owner.bind_named("redScale", [object]() -> decltype(auto) { return (object->red_scale); });
  if constexpr (requires { object->refresh; }) owner.bind_named("refresh", [object]() -> decltype(auto) { return (object->refresh); });
  if constexpr (requires { object->region_id_max; }) owner.bind_named("regionIdMax", [object]() -> decltype(auto) { return (object->region_id_max); });
  if constexpr (requires { object->region_id_min; }) owner.bind_named("regionIdMin", [object]() -> decltype(auto) { return (object->region_id_min); });
  if constexpr (requires { object->relative; }) owner.bind_named("relative", [object]() -> decltype(auto) { return (object->relative); });
  if constexpr (requires { object->remove_node; }) owner.bind_named("removeNode", [object]() -> decltype(auto) { return (object->remove_node); });
  if constexpr (requires { object->remove_spatial_object; }) owner.bind_named("removeSpatialObject", [object]() -> decltype(auto) { return (object->remove_spatial_object); });
  if constexpr (requires { object->repeat_count; }) owner.bind_named("repeatCount", [object]() -> decltype(auto) { return (object->repeat_count); });
  if constexpr (requires { object->request_permission; }) owner.bind_named("requestPermission", [object]() -> decltype(auto) { return (object->request_permission); });
  if constexpr (requires { object->request_persistence; }) owner.bind_named("requestPersistence", [object]() -> decltype(auto) { return (object->request_persistence); });
  if constexpr (requires { object->resize; }) owner.bind_named("resize", [object]() -> decltype(auto) { return (object->resize); });
  if constexpr (requires { object->resolution; }) owner.bind_named("resolution", [object]() -> decltype(auto) { return (object->resolution); });
  if constexpr (requires { object->restitution; }) owner.bind_named("restitution", [object]() -> decltype(auto) { return (object->restitution); });
  if constexpr (requires { object->right; }) owner.bind_named("right", [object]() -> decltype(auto) { return (object->right); });
  if constexpr (requires { object->root; }) owner.bind_named("root", [object]() -> decltype(auto) { return (object->root); });
  if constexpr (requires { object->rotate_per_second; }) owner.bind_named("rotatePerSecond", [object]() -> decltype(auto) { return (object->rotate_per_second); });
  if constexpr (requires { object->rotate_per_second_variance; }) owner.bind_named("rotatePerSecondVariance", [object]() -> decltype(auto) { return (object->rotate_per_second_variance); });
  if constexpr (requires { object->rotated; }) owner.bind_named("rotated", [object]() -> decltype(auto) { return (object->rotated); });
  if constexpr (requires { object->rotation; }) owner.bind_named("rotation", [object]() -> decltype(auto) { return (object->rotation); });
  if constexpr (requires { object->rotation_amplitude; }) owner.bind_named("rotationAmplitude", [object]() -> decltype(auto) { return (object->rotation_amplitude); });
  if constexpr (requires { object->rotation_end; }) owner.bind_named("rotationEnd", [object]() -> decltype(auto) { return (object->rotation_end); });
  if constexpr (requires { object->rotation_end_variance; }) owner.bind_named("rotationEndVariance", [object]() -> decltype(auto) { return (object->rotation_end_variance); });
  if constexpr (requires { object->rotation_speed_max; }) owner.bind_named("rotationSpeedMax", [object]() -> decltype(auto) { return (object->rotation_speed_max); });
  if constexpr (requires { object->rotation_speed_min; }) owner.bind_named("rotationSpeedMin", [object]() -> decltype(auto) { return (object->rotation_speed_min); });
  if constexpr (requires { object->rotation_start; }) owner.bind_named("rotationStart", [object]() -> decltype(auto) { return (object->rotation_start); });
  if constexpr (requires { object->rotation_start_variance; }) owner.bind_named("rotationStartVariance", [object]() -> decltype(auto) { return (object->rotation_start_variance); });
  if constexpr (requires { object->rotation_x; }) owner.bind_named("rotationX", [object]() -> decltype(auto) { return (object->rotation_x); });
  if constexpr (requires { object->rotation_y; }) owner.bind_named("rotationY", [object]() -> decltype(auto) { return (object->rotation_y); });
  if constexpr (requires { object->rotation_z; }) owner.bind_named("rotationZ", [object]() -> decltype(auto) { return (object->rotation_z); });
  if constexpr (requires { object->runtime; }) owner.bind_named("runtime", [object]() -> decltype(auto) { return (object->runtime); });
  if constexpr (requires { object->samples; }) owner.bind_named("samples", [object]() -> decltype(auto) { return (object->samples); });
  if constexpr (requires { object->saturation; }) owner.bind_named("saturation", [object]() -> decltype(auto) { return (object->saturation); });
  if constexpr (requires { object->scale; }) owner.bind_named("scale", [object]() -> decltype(auto) { return (object->scale); });
  if constexpr (requires { object->scale_curve; }) owner.bind_named("scaleCurve", [object]() -> decltype(auto) { return (object->scale_curve); });
  if constexpr (requires { object->scale_end; }) owner.bind_named("scaleEnd", [object]() -> decltype(auto) { return (object->scale_end); });
  if constexpr (requires { object->scale_max; }) owner.bind_named("scaleMax", [object]() -> decltype(auto) { return (object->scale_max); });
  if constexpr (requires { object->scale_min; }) owner.bind_named("scaleMin", [object]() -> decltype(auto) { return (object->scale_min); });
  if constexpr (requires { object->scale_x; }) owner.bind_named("scaleX", [object]() -> decltype(auto) { return (object->scale_x); });
  if constexpr (requires { object->scale_y; }) owner.bind_named("scaleY", [object]() -> decltype(auto) { return (object->scale_y); });
  if constexpr (requires { object->scaling; }) owner.bind_named("scaling", [object]() -> decltype(auto) { return (object->scaling); });
  if constexpr (requires { object->scanline_intensity; }) owner.bind_named("scanlineIntensity", [object]() -> decltype(auto) { return (object->scanline_intensity); });
  if constexpr (requires { object->scattering; }) owner.bind_named("scattering", [object]() -> decltype(auto) { return (object->scattering); });
  if constexpr (requires { object->scope; }) owner.bind_named("scope", [object]() -> decltype(auto) { return (object->scope); });
  if constexpr (requires { object->seed; }) owner.bind_named("seed", [object]() -> decltype(auto) { return (object->seed); });
  if constexpr (requires { object->segment; }) owner.bind_named("segment", [object]() -> decltype(auto) { return (object->segment); });
  if constexpr (requires { object->selection; }) owner.bind_named("selection", [object]() -> decltype(auto) { return (object->selection); });
  if constexpr (requires { object->send; }) owner.bind_named("send", [object]() -> decltype(auto) { return (object->send); });
  if constexpr (requires { object->sensor; }) owner.bind_named("sensor", [object]() -> decltype(auto) { return (object->sensor); });
  if constexpr (requires { object->set_accessory_bar_visible; }) owner.bind_named("setAccessoryBarVisible", [object]() -> decltype(auto) { return (object->set_accessory_bar_visible); });
  if constexpr (requires { object->set_background_color; }) owner.bind_named("setBackgroundColor", [object]() -> decltype(auto) { return (object->set_background_color); });
  if constexpr (requires { object->set_display_size; }) owner.bind_named("setDisplaySize", [object]() -> decltype(auto) { return (object->set_display_size); });
  if constexpr (requires { object->set_focus; }) owner.bind_named("setFocus", [object]() -> decltype(auto) { return (object->set_focus); });
  if constexpr (requires { object->set_metadata; }) owner.bind_named("setMetadata", [object]() -> decltype(auto) { return (object->set_metadata); });
  if constexpr (requires { object->set_node; }) owner.bind_named("setNode", [object]() -> decltype(auto) { return (object->set_node); });
  if constexpr (requires { object->set_overlays_content; }) owner.bind_named("setOverlaysContent", [object]() -> decltype(auto) { return (object->set_overlays_content); });
  if constexpr (requires { object->set_playback_state; }) owner.bind_named("setPlaybackState", [object]() -> decltype(auto) { return (object->set_playback_state); });
  if constexpr (requires { object->set_position_state; }) owner.bind_named("setPositionState", [object]() -> decltype(auto) { return (object->set_position_state); });
  if constexpr (requires { object->set_resize_mode; }) owner.bind_named("setResizeMode", [object]() -> decltype(auto) { return (object->set_resize_mode); });
  if constexpr (requires { object->set_scroll_assist_enabled; }) owner.bind_named("setScrollAssistEnabled", [object]() -> decltype(auto) { return (object->set_scroll_assist_enabled); });
  if constexpr (requires { object->set_style; }) owner.bind_named("setStyle", [object]() -> decltype(auto) { return (object->set_style); });
  if constexpr (requires { object->set_visible; }) owner.bind_named("setVisible", [object]() -> decltype(auto) { return (object->set_visible); });
  if constexpr (requires { object->shader_key; }) owner.bind_named("shaderKey", [object]() -> decltype(auto) { return (object->shader_key); });
  if constexpr (requires { object->shadow_bias; }) owner.bind_named("shadowBias", [object]() -> decltype(auto) { return (object->shadow_bias); });
  if constexpr (requires { object->shadow_far; }) owner.bind_named("shadowFar", [object]() -> decltype(auto) { return (object->shadow_far); });
  if constexpr (requires { object->shadow_map_size; }) owner.bind_named("shadowMapSize", [object]() -> decltype(auto) { return (object->shadow_map_size); });
  if constexpr (requires { object->shadow_near; }) owner.bind_named("shadowNear", [object]() -> decltype(auto) { return (object->shadow_near); });
  if constexpr (requires { object->shadow_strength; }) owner.bind_named("shadowStrength", [object]() -> decltype(auto) { return (object->shadow_strength); });
  if constexpr (requires { object->sheen_color; }) owner.bind_named("sheenColor", [object]() -> decltype(auto) { return (object->sheen_color); });
  if constexpr (requires { object->sheen_color_map; }) owner.bind_named("sheenColorMap", [object]() -> decltype(auto) { return (object->sheen_color_map); });
  if constexpr (requires { object->sheen_color_map_uv_set; }) owner.bind_named("sheenColorMapUvSet", [object]() -> decltype(auto) { return (object->sheen_color_map_uv_set); });
  if constexpr (requires { object->sheen_roughness; }) owner.bind_named("sheenRoughness", [object]() -> decltype(auto) { return (object->sheen_roughness); });
  if constexpr (requires { object->sheen_roughness_map; }) owner.bind_named("sheenRoughnessMap", [object]() -> decltype(auto) { return (object->sheen_roughness_map); });
  if constexpr (requires { object->sheen_roughness_map_uv_set; }) owner.bind_named("sheenRoughnessMapUvSet", [object]() -> decltype(auto) { return (object->sheen_roughness_map_uv_set); });
  if constexpr (requires { object->show; }) owner.bind_named("show", [object]() -> decltype(auto) { return (object->show); });
  if constexpr (requires { object->size; }) owner.bind_named("size", [object]() -> decltype(auto) { return (object->size); });
  if constexpr (requires { object->skew_x; }) owner.bind_named("skewX", [object]() -> decltype(auto) { return (object->skew_x); });
  if constexpr (requires { object->skew_y; }) owner.bind_named("skewY", [object]() -> decltype(auto) { return (object->skew_y); });
  if constexpr (requires { object->sky_color; }) owner.bind_named("skyColor", [object]() -> decltype(auto) { return (object->sky_color); });
  if constexpr (requires { object->slot_index; }) owner.bind_named("slotIndex", [object]() -> decltype(auto) { return (object->slot_index); });
  if constexpr (requires { object->slots; }) owner.bind_named("slots", [object]() -> decltype(auto) { return (object->slots); });
  if constexpr (requires { object->smooth_time; }) owner.bind_named("smoothTime", [object]() -> decltype(auto) { return (object->smooth_time); });
  if constexpr (requires { object->softness; }) owner.bind_named("softness", [object]() -> decltype(auto) { return (object->softness); });
  if constexpr (requires { object->source_height; }) owner.bind_named("sourceHeight", [object]() -> decltype(auto) { return (object->source_height); });
  if constexpr (requires { object->source_mode; }) owner.bind_named("sourceMode", [object]() -> decltype(auto) { return (object->source_mode); });
  if constexpr (requires { object->source_position_variancex; }) owner.bind_named("sourcePositionVariancex", [object]() -> decltype(auto) { return (object->source_position_variancex); });
  if constexpr (requires { object->source_position_variancey; }) owner.bind_named("sourcePositionVariancey", [object]() -> decltype(auto) { return (object->source_position_variancey); });
  if constexpr (requires { object->source_width; }) owner.bind_named("sourceWidth", [object]() -> decltype(auto) { return (object->source_width); });
  if constexpr (requires { object->spawn_height; }) owner.bind_named("spawnHeight", [object]() -> decltype(auto) { return (object->spawn_height); });
  if constexpr (requires { object->spawn_rate; }) owner.bind_named("spawnRate", [object]() -> decltype(auto) { return (object->spawn_rate); });
  if constexpr (requires { object->spawn_shape; }) owner.bind_named("spawnShape", [object]() -> decltype(auto) { return (object->spawn_shape); });
  if constexpr (requires { object->spawn_width; }) owner.bind_named("spawnWidth", [object]() -> decltype(auto) { return (object->spawn_width); });
  if constexpr (requires { object->specular; }) owner.bind_named("specular", [object]() -> decltype(auto) { return (object->specular); });
  if constexpr (requires { object->specular_color; }) owner.bind_named("specularColor", [object]() -> decltype(auto) { return (object->specular_color); });
  if constexpr (requires { object->specular_color_map; }) owner.bind_named("specularColorMap", [object]() -> decltype(auto) { return (object->specular_color_map); });
  if constexpr (requires { object->specular_color_map_uv_set; }) owner.bind_named("specularColorMapUvSet", [object]() -> decltype(auto) { return (object->specular_color_map_uv_set); });
  if constexpr (requires { object->specular_map; }) owner.bind_named("specularMap", [object]() -> decltype(auto) { return (object->specular_map); });
  if constexpr (requires { object->specular_map_uv_set; }) owner.bind_named("specularMapUvSet", [object]() -> decltype(auto) { return (object->specular_map_uv_set); });
  if constexpr (requires { object->speed; }) owner.bind_named("speed", [object]() -> decltype(auto) { return (object->speed); });
  if constexpr (requires { object->speed_max; }) owner.bind_named("speedMax", [object]() -> decltype(auto) { return (object->speed_max); });
  if constexpr (requires { object->speed_min; }) owner.bind_named("speedMin", [object]() -> decltype(auto) { return (object->speed_min); });
  if constexpr (requires { object->speed_variance; }) owner.bind_named("speedVariance", [object]() -> decltype(auto) { return (object->speed_variance); });
  if constexpr (requires { object->spot; }) owner.bind_named("spot", [object]() -> decltype(auto) { return (object->spot); });
  if constexpr (requires { object->spot_blend; }) owner.bind_named("spotBlend", [object]() -> decltype(auto) { return (object->spot_blend); });
  if constexpr (requires { object->spread; }) owner.bind_named("spread", [object]() -> decltype(auto) { return (object->spread); });
  if constexpr (requires { object->stack; }) owner.bind_named("stack", [object]() -> decltype(auto) { return (object->stack); });
  if constexpr (requires { object->start; }) owner.bind_named("start", [object]() -> decltype(auto) { return (object->start); });
  if constexpr (requires { object->start_color; }) owner.bind_named("startColor", [object]() -> decltype(auto) { return (object->start_color); });
  if constexpr (requires { object->start_color_variance; }) owner.bind_named("startColorVariance", [object]() -> decltype(auto) { return (object->start_color_variance); });
  if constexpr (requires { object->start_index; }) owner.bind_named("startIndex", [object]() -> decltype(auto) { return (object->start_index); });
  if constexpr (requires { object->start_particle_size; }) owner.bind_named("startParticleSize", [object]() -> decltype(auto) { return (object->start_particle_size); });
  if constexpr (requires { object->start_particle_size_variance; }) owner.bind_named("startParticleSizeVariance", [object]() -> decltype(auto) { return (object->start_particle_size_variance); });
  if constexpr (requires { object->start_x; }) owner.bind_named("startX", [object]() -> decltype(auto) { return (object->start_x); });
  if constexpr (requires { object->start_y; }) owner.bind_named("startY", [object]() -> decltype(auto) { return (object->start_y); });
  if constexpr (requires { object->start_z; }) owner.bind_named("startZ", [object]() -> decltype(auto) { return (object->start_z); });
  if constexpr (requires { object->stated; }) owner.bind_named("stated", [object]() -> decltype(auto) { return (object->stated); });
  if constexpr (requires { object->steps; }) owner.bind_named("steps", [object]() -> decltype(auto) { return (object->steps); });
  if constexpr (requires { object->strength; }) owner.bind_named("strength", [object]() -> decltype(auto) { return (object->strength); });
  if constexpr (requires { object->stroke_bounds; }) owner.bind_named("strokeBounds", [object]() -> decltype(auto) { return (object->stroke_bounds); });
  if constexpr (requires { object->style; }) owner.bind_named("style", [object]() -> decltype(auto) { return (object->style); });
  if constexpr (requires { object->subject; }) owner.bind_named("subject", [object]() -> decltype(auto) { return (object->subject); });
  if constexpr (requires { object->subpixel; }) owner.bind_named("subpixel", [object]() -> decltype(auto) { return (object->subpixel); });
  if constexpr (requires { object->subscribe; }) owner.bind_named("subscribe", [object]() -> decltype(auto) { return (object->subscribe); });
  if constexpr (requires { object->subscribe_absolute_orientation; }) owner.bind_named("subscribeAbsoluteOrientation", [object]() -> decltype(auto) { return (object->subscribe_absolute_orientation); });
  if constexpr (requires { object->subscribe_ambient_light; }) owner.bind_named("subscribeAmbientLight", [object]() -> decltype(auto) { return (object->subscribe_ambient_light); });
  if constexpr (requires { object->subscribe_barometer; }) owner.bind_named("subscribeBarometer", [object]() -> decltype(auto) { return (object->subscribe_barometer); });
  if constexpr (requires { object->subscribe_gravity; }) owner.bind_named("subscribeGravity", [object]() -> decltype(auto) { return (object->subscribe_gravity); });
  if constexpr (requires { object->subscribe_linear_acceleration; }) owner.bind_named("subscribeLinearAcceleration", [object]() -> decltype(auto) { return (object->subscribe_linear_acceleration); });
  if constexpr (requires { object->subscribe_magnetometer; }) owner.bind_named("subscribeMagnetometer", [object]() -> decltype(auto) { return (object->subscribe_magnetometer); });
  if constexpr (requires { object->subscribe_motion; }) owner.bind_named("subscribeMotion", [object]() -> decltype(auto) { return (object->subscribe_motion); });
  if constexpr (requires { object->subscribe_orientation; }) owner.bind_named("subscribeOrientation", [object]() -> decltype(auto) { return (object->subscribe_orientation); });
  if constexpr (requires { object->subscribe_proximity; }) owner.bind_named("subscribeProximity", [object]() -> decltype(auto) { return (object->subscribe_proximity); });
  if constexpr (requires { object->subscribe_quaternion; }) owner.bind_named("subscribeQuaternion", [object]() -> decltype(auto) { return (object->subscribe_quaternion); });
  if constexpr (requires { object->supported_abis; }) owner.bind_named("supportedAbis", [object]() -> decltype(auto) { return (object->supported_abis); });
  if constexpr (requires { object->tangential_accel_variance; }) owner.bind_named("tangentialAccelVariance", [object]() -> decltype(auto) { return (object->tangential_accel_variance); });
  if constexpr (requires { object->tangential_acceleration; }) owner.bind_named("tangentialAcceleration", [object]() -> decltype(auto) { return (object->tangential_acceleration); });
  if constexpr (requires { object->temperature; }) owner.bind_named("temperature", [object]() -> decltype(auto) { return (object->temperature); });
  if constexpr (requires { object->text_height; }) owner.bind_named("textHeight", [object]() -> decltype(auto) { return (object->text_height); });
  if constexpr (requires { object->text_width; }) owner.bind_named("textWidth", [object]() -> decltype(auto) { return (object->text_width); });
  if constexpr (requires { object->texture_file_name; }) owner.bind_named("textureFileName", [object]() -> decltype(auto) { return (object->texture_file_name); });
  if constexpr (requires { object->thickness; }) owner.bind_named("thickness", [object]() -> decltype(auto) { return (object->thickness); });
  if constexpr (requires { object->thickness_map; }) owner.bind_named("thicknessMap", [object]() -> decltype(auto) { return (object->thickness_map); });
  if constexpr (requires { object->thickness_map_uv_set; }) owner.bind_named("thicknessMapUvSet", [object]() -> decltype(auto) { return (object->thickness_map_uv_set); });
  if constexpr (requires { object->threshold; }) owner.bind_named("threshold", [object]() -> decltype(auto) { return (object->threshold); });
  if constexpr (requires { object->tilesets; }) owner.bind_named("tilesets", [object]() -> decltype(auto) { return (object->tilesets); });
  if constexpr (requires { object->time; }) owner.bind_named("time", [object]() -> decltype(auto) { return (object->time); });
  if constexpr (requires { object->timeline; }) owner.bind_named("timeline", [object]() -> decltype(auto) { return (object->timeline); });
  if constexpr (requires { object->timestamp; }) owner.bind_named("timestamp", [object]() -> decltype(auto) { return (object->timestamp); });
  if constexpr (requires { object->tint; }) owner.bind_named("tint", [object]() -> decltype(auto) { return (object->tint); });
  if constexpr (requires { object->top; }) owner.bind_named("top", [object]() -> decltype(auto) { return (object->top); });
  if constexpr (requires { object->torque; }) owner.bind_named("torque", [object]() -> decltype(auto) { return (object->torque); });
  if constexpr (requires { object->torque_x; }) owner.bind_named("torqueX", [object]() -> decltype(auto) { return (object->torque_x); });
  if constexpr (requires { object->torque_y; }) owner.bind_named("torqueY", [object]() -> decltype(auto) { return (object->torque_y); });
  if constexpr (requires { object->torque_z; }) owner.bind_named("torqueZ", [object]() -> decltype(auto) { return (object->torque_z); });
  if constexpr (requires { object->total_memory; }) owner.bind_named("totalMemory", [object]() -> decltype(auto) { return (object->total_memory); });
  if constexpr (requires { object->touching; }) owner.bind_named("touching", [object]() -> decltype(auto) { return (object->touching); });
  if constexpr (requires { object->transform; }) owner.bind_named("transform", [object]() -> decltype(auto) { return (object->transform); });
  if constexpr (requires { object->translation_amplitude; }) owner.bind_named("translationAmplitude", [object]() -> decltype(auto) { return (object->translation_amplitude); });
  if constexpr (requires { object->transmission; }) owner.bind_named("transmission", [object]() -> decltype(auto) { return (object->transmission); });
  if constexpr (requires { object->transmission_map; }) owner.bind_named("transmissionMap", [object]() -> decltype(auto) { return (object->transmission_map); });
  if constexpr (requires { object->transmission_map_uv_set; }) owner.bind_named("transmissionMapUvSet", [object]() -> decltype(auto) { return (object->transmission_map_uv_set); });
  if constexpr (requires { object->transparency; }) owner.bind_named("transparency", [object]() -> decltype(auto) { return (object->transparency); });
  if constexpr (requires { object->trauma; }) owner.bind_named("trauma", [object]() -> decltype(auto) { return (object->trauma); });
  if constexpr (requires { object->tweens; }) owner.bind_named("tweens", [object]() -> decltype(auto) { return (object->tweens); });
  if constexpr (requires { object->tx; }) owner.bind_named("tx", [object]() -> decltype(auto) { return (object->tx); });
  if constexpr (requires { object->ty; }) owner.bind_named("ty", [object]() -> decltype(auto) { return (object->ty); });
  if constexpr (requires { object->type; }) owner.bind_named("type", [object]() -> decltype(auto) { return (object->type); });
  if constexpr (requires { object->uniforms; }) owner.bind_named("uniforms", [object]() -> decltype(auto) { return (object->uniforms); });
  if constexpr (requires { object->up; }) owner.bind_named("up", [object]() -> decltype(auto) { return (object->up); });
  if constexpr (requires { object->update_spatial_object; }) owner.bind_named("updateSpatialObject", [object]() -> decltype(auto) { return (object->update_spatial_object); });
  if constexpr (requires { object->value; }) owner.bind_named("value", [object]() -> decltype(auto) { return (object->value); });
  if constexpr (requires { object->velocity; }) owner.bind_named("velocity", [object]() -> decltype(auto) { return (object->velocity); });
  if constexpr (requires { object->velocity_inheritance; }) owner.bind_named("velocityInheritance", [object]() -> decltype(auto) { return (object->velocity_inheritance); });
  if constexpr (requires { object->version; }) owner.bind_named("version", [object]() -> decltype(auto) { return (object->version); });
  if constexpr (requires { object->vibrate; }) owner.bind_named("vibrate", [object]() -> decltype(auto) { return (object->vibrate); });
  if constexpr (requires { object->vibrate_pattern; }) owner.bind_named("vibratePattern", [object]() -> decltype(auto) { return (object->vibrate_pattern); });
  if constexpr (requires { object->vibrate_waveform; }) owner.bind_named("vibrateWaveform", [object]() -> decltype(auto) { return (object->vibrate_waveform); });
  if constexpr (requires { object->view; }) owner.bind_named("view", [object]() -> decltype(auto) { return (object->view); });
  if constexpr (requires { object->viewport_height; }) owner.bind_named("viewportHeight", [object]() -> decltype(auto) { return (object->viewport_height); });
  if constexpr (requires { object->viewport_width; }) owner.bind_named("viewportWidth", [object]() -> decltype(auto) { return (object->viewport_width); });
  if constexpr (requires { object->vignette; }) owner.bind_named("vignette", [object]() -> decltype(auto) { return (object->vignette); });
  if constexpr (requires { object->visible; }) owner.bind_named("visible", [object]() -> decltype(auto) { return (object->visible); });
  if constexpr (requires { object->w; }) owner.bind_named("w", [object]() -> decltype(auto) { return (object->w); });
  if constexpr (requires { object->watch_position; }) owner.bind_named("watchPosition", [object]() -> decltype(auto) { return (object->watch_position); });
  if constexpr (requires { object->web_view_version; }) owner.bind_named("webViewVersion", [object]() -> decltype(auto) { return (object->web_view_version); });
  if constexpr (requires { object->weight; }) owner.bind_named("weight", [object]() -> decltype(auto) { return (object->weight); });
  if constexpr (requires { object->white; }) owner.bind_named("white", [object]() -> decltype(auto) { return (object->white); });
  if constexpr (requires { object->width; }) owner.bind_named("width", [object]() -> decltype(auto) { return (object->width); });
  if constexpr (requires { object->wind; }) owner.bind_named("wind", [object]() -> decltype(auto) { return (object->wind); });
  if constexpr (requires { object->winding; }) owner.bind_named("winding", [object]() -> decltype(auto) { return (object->winding); });
  if constexpr (requires { object->world_bounds; }) owner.bind_named("worldBounds", [object]() -> decltype(auto) { return (object->world_bounds); });
  if constexpr (requires { object->world_matrices; }) owner.bind_named("worldMatrices", [object]() -> decltype(auto) { return (object->world_matrices); });
  if constexpr (requires { object->world_space; }) owner.bind_named("worldSpace", [object]() -> decltype(auto) { return (object->world_space); });
  if constexpr (requires { object->would_occupy_bucket_count; }) owner.bind_named("wouldOccupyBucketCount", [object]() -> decltype(auto) { return (object->would_occupy_bucket_count); });
  if constexpr (requires { object->wrap_u; }) owner.bind_named("wrapU", [object]() -> decltype(auto) { return (object->wrap_u); });
  if constexpr (requires { object->wrap_v; }) owner.bind_named("wrapV", [object]() -> decltype(auto) { return (object->wrap_v); });
  if constexpr (requires { object->wrapped_diffuse_color; }) owner.bind_named("wrappedDiffuseColor", [object]() -> decltype(auto) { return (object->wrapped_diffuse_color); });
  if constexpr (requires { object->wrapped_diffuse_map; }) owner.bind_named("wrappedDiffuseMap", [object]() -> decltype(auto) { return (object->wrapped_diffuse_map); });
  if constexpr (requires { object->wrapped_diffuse_map_uv_set; }) owner.bind_named("wrappedDiffuseMapUvSet", [object]() -> decltype(auto) { return (object->wrapped_diffuse_map_uv_set); });
  if constexpr (requires { object->wrapped_diffuse_strength; }) owner.bind_named("wrappedDiffuseStrength", [object]() -> decltype(auto) { return (object->wrapped_diffuse_strength); });
  if constexpr (requires { object->write_bookmark; }) owner.bind_named("writeBookmark", [object]() -> decltype(auto) { return (object->write_bookmark); });
  if constexpr (requires { object->write_format; }) owner.bind_named("writeFormat", [object]() -> decltype(auto) { return (object->write_format); });
  if constexpr (requires { object->write_html; }) owner.bind_named("writeHtml", [object]() -> decltype(auto) { return (object->write_html); });
  if constexpr (requires { object->write_image; }) owner.bind_named("writeImage", [object]() -> decltype(auto) { return (object->write_image); });
  if constexpr (requires { object->write_items; }) owner.bind_named("writeItems", [object]() -> decltype(auto) { return (object->write_items); });
  if constexpr (requires { object->write_rtf; }) owner.bind_named("writeRTF", [object]() -> decltype(auto) { return (object->write_rtf); });
  if constexpr (requires { object->write_text; }) owner.bind_named("writeText", [object]() -> decltype(auto) { return (object->write_text); });
  if constexpr (requires { object->x; }) owner.bind_named("x", [object]() -> decltype(auto) { return (object->x); });
  if constexpr (requires { object->x0; }) owner.bind_named("x0", [object]() -> decltype(auto) { return (object->x0); });
  if constexpr (requires { object->x1; }) owner.bind_named("x1", [object]() -> decltype(auto) { return (object->x1); });
  if constexpr (requires { object->x_offset; }) owner.bind_named("xOffset", [object]() -> decltype(auto) { return (object->x_offset); });
  if constexpr (requires { object->y; }) owner.bind_named("y", [object]() -> decltype(auto) { return (object->y); });
  if constexpr (requires { object->y0; }) owner.bind_named("y0", [object]() -> decltype(auto) { return (object->y0); });
  if constexpr (requires { object->y1; }) owner.bind_named("y1", [object]() -> decltype(auto) { return (object->y1); });
  if constexpr (requires { object->y_offset; }) owner.bind_named("yOffset", [object]() -> decltype(auto) { return (object->y_offset); });
  if constexpr (requires { object->z; }) owner.bind_named("z", [object]() -> decltype(auto) { return (object->z); });
  if constexpr (requires { object->zoom; }) owner.bind_named("zoom", [object]() -> decltype(auto) { return (object->zoom); });
}

// A computed cell that BOTH subjects declare must be declared at the same type, and unlike a row
// key a disagreement is fatal to the proof rather than merely uncounted. The owner holds one
// cell of one type, so a read through the other row asks for a type the typed lookup cannot
// find and is handed a default instead of the value sitting on the object -- silently.
//
// A cell only ONE subject declares is not a disagreement. Computed cells are declared optional
// in the source, so an object without one is assignable to a row that names it, and both cases
// read honestly: the subject that has the member answers from it, the one that does not answers
// from its attachment.
#define FLIGHT_SDK_ROW_COMPUTED(member)                                                        \
  if constexpr (requires(Base& base) { base.member; } &&                                       \
                requires(Derived& derived) { derived.member; }) {                              \
    if constexpr (!std::same_as<std::remove_cvref_t<decltype(std::declval<Base&>().member)>,   \
                               std::remove_cvref_t<decltype(std::declval<Derived&>().member)>>) \
      return false;                                                                            \
  }

#define FLIGHT_SDK_ROW_WIDENS(member)                                                          \
  if constexpr (requires(Base& base) { base.member; }) {                                       \
    if constexpr (!requires(Derived& derived) { derived.member; }) return false;                \
    else if constexpr (!std::same_as<std::remove_cvref_t<decltype(std::declval<Base&>().member)>, \
                                     std::remove_cvref_t<decltype(std::declval<Derived&>().member)>>) \
      return false;                                                                            \
    else ++matched;                                                                            \
  }

template <typename Base, typename Derived>
consteval bool generated_row_widening_matches() {
  FLIGHT_SDK_ROW_COMPUTED(entity_runtime_key)
  std::size_t matched = 0;
  FLIGHT_SDK_ROW_WIDENS(brand)
  FLIGHT_SDK_ROW_WIDENS(a)
  FLIGHT_SDK_ROW_WIDENS(aberration)
  FLIGHT_SDK_ROW_WIDENS(absolute)
  FLIGHT_SDK_ROW_WIDENS(accuracy)
  FLIGHT_SDK_ROW_WIDENS(action)
  FLIGHT_SDK_ROW_WIDENS(adaptation_speed)
  FLIGHT_SDK_ROW_WIDENS(additive)
  FLIGHT_SDK_ROW_WIDENS(addressed)
  FLIGHT_SDK_ROW_WIDENS(alpha)
  FLIGHT_SDK_ROW_WIDENS(alpha_bias)
  FLIGHT_SDK_ROW_WIDENS(alpha_curve)
  FLIGHT_SDK_ROW_WIDENS(alpha_end)
  FLIGHT_SDK_ROW_WIDENS(alpha_scale)
  FLIGHT_SDK_ROW_WIDENS(alpha_start)
  FLIGHT_SDK_ROW_WIDENS(altitude)
  FLIGHT_SDK_ROW_WIDENS(altitude_accuracy)
  FLIGHT_SDK_ROW_WIDENS(ambient)
  FLIGHT_SDK_ROW_WIDENS(amount)
  FLIGHT_SDK_ROW_WIDENS(angle)
  FLIGHT_SDK_ROW_WIDENS(angle_variance)
  FLIGHT_SDK_ROW_WIDENS(animation)
  FLIGHT_SDK_ROW_WIDENS(animations)
  FLIGHT_SDK_ROW_WIDENS(anisotropy)
  FLIGHT_SDK_ROW_WIDENS(anisotropy_map)
  FLIGHT_SDK_ROW_WIDENS(anisotropy_map_uv_set)
  FLIGHT_SDK_ROW_WIDENS(anisotropy_rotation)
  FLIGHT_SDK_ROW_WIDENS(anisotropy_strength)
  FLIGHT_SDK_ROW_WIDENS(announce)
  FLIGHT_SDK_ROW_WIDENS(applied)
  FLIGHT_SDK_ROW_WIDENS(arch)
  FLIGHT_SDK_ROW_WIDENS(ascent)
  FLIGHT_SDK_ROW_WIDENS(atlas)
  FLIGHT_SDK_ROW_WIDENS(attenuation_color)
  FLIGHT_SDK_ROW_WIDENS(attenuation_distance)
  FLIGHT_SDK_ROW_WIDENS(attributes)
  FLIGHT_SDK_ROW_WIDENS(available_memory)
  FLIGHT_SDK_ROW_WIDENS(b)
  FLIGHT_SDK_ROW_WIDENS(beta)
  FLIGHT_SDK_ROW_WIDENS(bias)
  FLIGHT_SDK_ROW_WIDENS(bitmap)
  FLIGHT_SDK_ROW_WIDENS(black_tighten)
  FLIGHT_SDK_ROW_WIDENS(blend_func_destination)
  FLIGHT_SDK_ROW_WIDENS(blend_func_source)
  FLIGHT_SDK_ROW_WIDENS(blend_mode)
  FLIGHT_SDK_ROW_WIDENS(blue)
  FLIGHT_SDK_ROW_WIDENS(blue_bias)
  FLIGHT_SDK_ROW_WIDENS(blue_scale)
  FLIGHT_SDK_ROW_WIDENS(blur_x)
  FLIGHT_SDK_ROW_WIDENS(blur_y)
  FLIGHT_SDK_ROW_WIDENS(board_name)
  FLIGHT_SDK_ROW_WIDENS(bodies)
  FLIGHT_SDK_ROW_WIDENS(body_a)
  FLIGHT_SDK_ROW_WIDENS(body_b)
  FLIGHT_SDK_ROW_WIDENS(bottom)
  FLIGHT_SDK_ROW_WIDENS(bounds)
  FLIGHT_SDK_ROW_WIDENS(break_force)
  FLIGHT_SDK_ROW_WIDENS(break_torque)
  FLIGHT_SDK_ROW_WIDENS(brightness)
  FLIGHT_SDK_ROW_WIDENS(burst_count)
  FLIGHT_SDK_ROW_WIDENS(burst_interval)
  FLIGHT_SDK_ROW_WIDENS(c)
  FLIGHT_SDK_ROW_WIDENS(cancel)
  FLIGHT_SDK_ROW_WIDENS(capabilities)
  FLIGHT_SDK_ROW_WIDENS(cascade_count)
  FLIGHT_SDK_ROW_WIDENS(cascade_splits)
  FLIGHT_SDK_ROW_WIDENS(casts_shadow)
  FLIGHT_SDK_ROW_WIDENS(cell_size)
  FLIGHT_SDK_ROW_WIDENS(center)
  FLIGHT_SDK_ROW_WIDENS(center_x)
  FLIGHT_SDK_ROW_WIDENS(center_y)
  FLIGHT_SDK_ROW_WIDENS(center_z)
  FLIGHT_SDK_ROW_WIDENS(child1)
  FLIGHT_SDK_ROW_WIDENS(child2)
  FLIGHT_SDK_ROW_WIDENS(children)
  FLIGHT_SDK_ROW_WIDENS(clear)
  FLIGHT_SDK_ROW_WIDENS(clear_metadata)
  FLIGHT_SDK_ROW_WIDENS(clear_position_state)
  FLIGHT_SDK_ROW_WIDENS(clear_spatial_index)
  FLIGHT_SDK_ROW_WIDENS(clear_watch)
  FLIGHT_SDK_ROW_WIDENS(clearcoat)
  FLIGHT_SDK_ROW_WIDENS(clearcoat_map)
  FLIGHT_SDK_ROW_WIDENS(clearcoat_map_uv_set)
  FLIGHT_SDK_ROW_WIDENS(clearcoat_normal_map)
  FLIGHT_SDK_ROW_WIDENS(clearcoat_normal_map_uv_set)
  FLIGHT_SDK_ROW_WIDENS(clearcoat_normal_scale)
  FLIGHT_SDK_ROW_WIDENS(clearcoat_roughness)
  FLIGHT_SDK_ROW_WIDENS(clearcoat_roughness_map)
  FLIGHT_SDK_ROW_WIDENS(clearcoat_roughness_map_uv_set)
  FLIGHT_SDK_ROW_WIDENS(clip)
  FLIGHT_SDK_ROW_WIDENS(collider_a)
  FLIGHT_SDK_ROW_WIDENS(collider_b)
  FLIGHT_SDK_ROW_WIDENS(color)
  FLIGHT_SDK_ROW_WIDENS(color_curve)
  FLIGHT_SDK_ROW_WIDENS(color_depth)
  FLIGHT_SDK_ROW_WIDENS(color_end_b)
  FLIGHT_SDK_ROW_WIDENS(color_end_g)
  FLIGHT_SDK_ROW_WIDENS(color_end_r)
  FLIGHT_SDK_ROW_WIDENS(color_end_variance_b)
  FLIGHT_SDK_ROW_WIDENS(color_end_variance_g)
  FLIGHT_SDK_ROW_WIDENS(color_end_variance_r)
  FLIGHT_SDK_ROW_WIDENS(color_gamut)
  FLIGHT_SDK_ROW_WIDENS(color_matrix)
  FLIGHT_SDK_ROW_WIDENS(color_scale_bias)
  FLIGHT_SDK_ROW_WIDENS(color_start_b)
  FLIGHT_SDK_ROW_WIDENS(color_start_g)
  FLIGHT_SDK_ROW_WIDENS(color_start_r)
  FLIGHT_SDK_ROW_WIDENS(color_start_variance_b)
  FLIGHT_SDK_ROW_WIDENS(color_start_variance_g)
  FLIGHT_SDK_ROW_WIDENS(color_start_variance_r)
  FLIGHT_SDK_ROW_WIDENS(commands)
  FLIGHT_SDK_ROW_WIDENS(component_x)
  FLIGHT_SDK_ROW_WIDENS(component_y)
  FLIGHT_SDK_ROW_WIDENS(compression)
  FLIGHT_SDK_ROW_WIDENS(connections)
  FLIGHT_SDK_ROW_WIDENS(contrast)
  FLIGHT_SDK_ROW_WIDENS(count)
  FLIGHT_SDK_ROW_WIDENS(cpu_cores)
  FLIGHT_SDK_ROW_WIDENS(crop)
  FLIGHT_SDK_ROW_WIDENS(curvature)
  FLIGHT_SDK_ROW_WIDENS(d)
  FLIGHT_SDK_ROW_WIDENS(damping_ratio)
  FLIGHT_SDK_ROW_WIDENS(data)
  FLIGHT_SDK_ROW_WIDENS(deadzone_half_height)
  FLIGHT_SDK_ROW_WIDENS(deadzone_half_width)
  FLIGHT_SDK_ROW_WIDENS(decay)
  FLIGHT_SDK_ROW_WIDENS(declined)
  FLIGHT_SDK_ROW_WIDENS(default_ease)
  FLIGHT_SDK_ROW_WIDENS(delay)
  FLIGHT_SDK_ROW_WIDENS(delta_time)
  FLIGHT_SDK_ROW_WIDENS(density)
  FLIGHT_SDK_ROW_WIDENS(density_dpi)
  FLIGHT_SDK_ROW_WIDENS(depth)
  FLIGHT_SDK_ROW_WIDENS(descent)
  FLIGHT_SDK_ROW_WIDENS(destroy)
  FLIGHT_SDK_ROW_WIDENS(device_pixel_ratio)
  FLIGHT_SDK_ROW_WIDENS(direction)
  FLIGHT_SDK_ROW_WIDENS(direction_x)
  FLIGHT_SDK_ROW_WIDENS(direction_y)
  FLIGHT_SDK_ROW_WIDENS(direction_z)
  FLIGHT_SDK_ROW_WIDENS(directional)
  FLIGHT_SDK_ROW_WIDENS(distance)
  FLIGHT_SDK_ROW_WIDENS(distro)
  FLIGHT_SDK_ROW_WIDENS(distro_version)
  FLIGHT_SDK_ROW_WIDENS(divisor)
  FLIGHT_SDK_ROW_WIDENS(duration)
  FLIGHT_SDK_ROW_WIDENS(ease)
  FLIGHT_SDK_ROW_WIDENS(edge)
  FLIGHT_SDK_ROW_WIDENS(edge_mode)
  FLIGHT_SDK_ROW_WIDENS(edge_threshold)
  FLIGHT_SDK_ROW_WIDENS(elapsed)
  FLIGHT_SDK_ROW_WIDENS(emission)
  FLIGHT_SDK_ROW_WIDENS(emit)
  FLIGHT_SDK_ROW_WIDENS(emitter_cone_angle)
  FLIGHT_SDK_ROW_WIDENS(emitter_depth)
  FLIGHT_SDK_ROW_WIDENS(emitter_height)
  FLIGHT_SDK_ROW_WIDENS(emitter_radius)
  FLIGHT_SDK_ROW_WIDENS(emitter_shape)
  FLIGHT_SDK_ROW_WIDENS(emitter_type)
  FLIGHT_SDK_ROW_WIDENS(emitter_width)
  FLIGHT_SDK_ROW_WIDENS(enabled)
  FLIGHT_SDK_ROW_WIDENS(encoding)
  FLIGHT_SDK_ROW_WIDENS(end)
  FLIGHT_SDK_ROW_WIDENS(end_index)
  FLIGHT_SDK_ROW_WIDENS(end_x)
  FLIGHT_SDK_ROW_WIDENS(end_y)
  FLIGHT_SDK_ROW_WIDENS(end_z)
  FLIGHT_SDK_ROW_WIDENS(endianness)
  FLIGHT_SDK_ROW_WIDENS(engine)
  FLIGHT_SDK_ROW_WIDENS(engine_version)
  FLIGHT_SDK_ROW_WIDENS(explain_spatial_indexing)
  FLIGHT_SDK_ROW_WIDENS(exposure)
  FLIGHT_SDK_ROW_WIDENS(exposure_compensation)
  FLIGHT_SDK_ROW_WIDENS(far)
  FLIGHT_SDK_ROW_WIDENS(feature_id)
  FLIGHT_SDK_ROW_WIDENS(feedback)
  FLIGHT_SDK_ROW_WIDENS(fill_bounds)
  FLIGHT_SDK_ROW_WIDENS(fill_color)
  FLIGHT_SDK_ROW_WIDENS(finish_color)
  FLIGHT_SDK_ROW_WIDENS(finish_color_variance)
  FLIGHT_SDK_ROW_WIDENS(finish_particle_size)
  FLIGHT_SDK_ROW_WIDENS(finish_particle_size_variance)
  FLIGHT_SDK_ROW_WIDENS(floor_level)
  FLIGHT_SDK_ROW_WIDENS(font_scale)
  FLIGHT_SDK_ROW_WIDENS(force_x)
  FLIGHT_SDK_ROW_WIDENS(force_y)
  FLIGHT_SDK_ROW_WIDENS(force_z)
  FLIGHT_SDK_ROW_WIDENS(form_factor)
  FLIGHT_SDK_ROW_WIDENS(format)
  FLIGHT_SDK_ROW_WIDENS(fov_y)
  FLIGHT_SDK_ROW_WIDENS(fraction)
  FLIGHT_SDK_ROW_WIDENS(frame_count)
  FLIGHT_SDK_ROW_WIDENS(frame_duration)
  FLIGHT_SDK_ROW_WIDENS(frame_durations)
  FLIGHT_SDK_ROW_WIDENS(frame_id)
  FLIGHT_SDK_ROW_WIDENS(frame_names)
  FLIGHT_SDK_ROW_WIDENS(frame_rate)
  FLIGHT_SDK_ROW_WIDENS(frames)
  FLIGHT_SDK_ROW_WIDENS(frequency)
  FLIGHT_SDK_ROW_WIDENS(friction)
  FLIGHT_SDK_ROW_WIDENS(gain)
  FLIGHT_SDK_ROW_WIDENS(gamma)
  FLIGHT_SDK_ROW_WIDENS(gate_weave)
  FLIGHT_SDK_ROW_WIDENS(get_capabilities)
  FLIGHT_SDK_ROW_WIDENS(get_current_position)
  FLIGHT_SDK_ROW_WIDENS(get_current_position_result)
  FLIGHT_SDK_ROW_WIDENS(get_display_metrics)
  FLIGHT_SDK_ROW_WIDENS(get_formats)
  FLIGHT_SDK_ROW_WIDENS(get_glyph_atlas_image)
  FLIGHT_SDK_ROW_WIDENS(get_glyph_entry)
  FLIGHT_SDK_ROW_WIDENS(get_glyph_kerning)
  FLIGHT_SDK_ROW_WIDENS(get_glyph_layout_version)
  FLIGHT_SDK_ROW_WIDENS(get_glyph_metrics)
  FLIGHT_SDK_ROW_WIDENS(get_id)
  FLIGHT_SDK_ROW_WIDENS(get_info)
  FLIGHT_SDK_ROW_WIDENS(get_permission)
  FLIGHT_SDK_ROW_WIDENS(get_permission_state)
  FLIGHT_SDK_ROW_WIDENS(get_persistence)
  FLIGHT_SDK_ROW_WIDENS(get_safe_area_insets)
  FLIGHT_SDK_ROW_WIDENS(ghosts)
  FLIGHT_SDK_ROW_WIDENS(glyphs)
  FLIGHT_SDK_ROW_WIDENS(gpu_renderer)
  FLIGHT_SDK_ROW_WIDENS(gpu_vendor)
  FLIGHT_SDK_ROW_WIDENS(grain_intensity)
  FLIGHT_SDK_ROW_WIDENS(gravity)
  FLIGHT_SDK_ROW_WIDENS(gravity_x)
  FLIGHT_SDK_ROW_WIDENS(gravity_y)
  FLIGHT_SDK_ROW_WIDENS(gravity_z)
  FLIGHT_SDK_ROW_WIDENS(gravityx)
  FLIGHT_SDK_ROW_WIDENS(gravityy)
  FLIGHT_SDK_ROW_WIDENS(green)
  FLIGHT_SDK_ROW_WIDENS(green_bias)
  FLIGHT_SDK_ROW_WIDENS(green_scale)
  FLIGHT_SDK_ROW_WIDENS(ground_color)
  FLIGHT_SDK_ROW_WIDENS(halation_radius)
  FLIGHT_SDK_ROW_WIDENS(halation_strength)
  FLIGHT_SDK_ROW_WIDENS(half_extent_x)
  FLIGHT_SDK_ROW_WIDENS(half_extent_y)
  FLIGHT_SDK_ROW_WIDENS(half_extent_z)
  FLIGHT_SDK_ROW_WIDENS(half_h)
  FLIGHT_SDK_ROW_WIDENS(half_w)
  FLIGHT_SDK_ROW_WIDENS(halo)
  FLIGHT_SDK_ROW_WIDENS(handle)
  FLIGHT_SDK_ROW_WIDENS(has_format)
  FLIGHT_SDK_ROW_WIDENS(has_image)
  FLIGHT_SDK_ROW_WIDENS(has_keyboard)
  FLIGHT_SDK_ROW_WIDENS(has_mouse)
  FLIGHT_SDK_ROW_WIDENS(has_stylus)
  FLIGHT_SDK_ROW_WIDENS(has_text)
  FLIGHT_SDK_ROW_WIDENS(heading)
  FLIGHT_SDK_ROW_WIDENS(height)
  FLIGHT_SDK_ROW_WIDENS(hemisphere)
  FLIGHT_SDK_ROW_WIDENS(hide)
  FLIGHT_SDK_ROW_WIDENS(high_max)
  FLIGHT_SDK_ROW_WIDENS(high_min)
  FLIGHT_SDK_ROW_WIDENS(hue)
  FLIGHT_SDK_ROW_WIDENS(id)
  FLIGHT_SDK_ROW_WIDENS(illuminance)
  FLIGHT_SDK_ROW_WIDENS(image_count)
  FLIGHT_SDK_ROW_WIDENS(image_file)
  FLIGHT_SDK_ROW_WIDENS(image_height)
  FLIGHT_SDK_ROW_WIDENS(image_path)
  FLIGHT_SDK_ROW_WIDENS(image_width)
  FLIGHT_SDK_ROW_WIDENS(impact)
  FLIGHT_SDK_ROW_WIDENS(influence_counts)
  FLIGHT_SDK_ROW_WIDENS(influences)
  FLIGHT_SDK_ROW_WIDENS(inner_cone_cos)
  FLIGHT_SDK_ROW_WIDENS(inner_cone_degrees)
  FLIGHT_SDK_ROW_WIDENS(insert_spatial_object)
  FLIGHT_SDK_ROW_WIDENS(intensity)
  FLIGHT_SDK_ROW_WIDENS(intensity_unit)
  FLIGHT_SDK_ROW_WIDENS(interval)
  FLIGHT_SDK_ROW_WIDENS(invoke)
  FLIGHT_SDK_ROW_WIDENS(ior)
  FLIGHT_SDK_ROW_WIDENS(iridescence)
  FLIGHT_SDK_ROW_WIDENS(iridescence_ior)
  FLIGHT_SDK_ROW_WIDENS(iridescence_map)
  FLIGHT_SDK_ROW_WIDENS(iridescence_map_uv_set)
  FLIGHT_SDK_ROW_WIDENS(iridescence_thickness_map)
  FLIGHT_SDK_ROW_WIDENS(iridescence_thickness_map_uv_set)
  FLIGHT_SDK_ROW_WIDENS(iridescence_thickness_max)
  FLIGHT_SDK_ROW_WIDENS(iridescence_thickness_min)
  FLIGHT_SDK_ROW_WIDENS(is_ambient_light_supported)
  FLIGHT_SDK_ROW_WIDENS(is_available)
  FLIGHT_SDK_ROW_WIDENS(is_barometer_supported)
  FLIGHT_SDK_ROW_WIDENS(is_gravity_supported)
  FLIGHT_SDK_ROW_WIDENS(is_gyroscope_supported)
  FLIGHT_SDK_ROW_WIDENS(is_hdr)
  FLIGHT_SDK_ROW_WIDENS(is_jailbroken)
  FLIGHT_SDK_ROW_WIDENS(is_linear_acceleration_supported)
  FLIGHT_SDK_ROW_WIDENS(is_low_end_device)
  FLIGHT_SDK_ROW_WIDENS(is_magnetometer_supported)
  FLIGHT_SDK_ROW_WIDENS(is_motion_supported)
  FLIGHT_SDK_ROW_WIDENS(is_orientation_supported)
  FLIGHT_SDK_ROW_WIDENS(is_proximity_supported)
  FLIGHT_SDK_ROW_WIDENS(is_rooted)
  FLIGHT_SDK_ROW_WIDENS(is_supported)
  FLIGHT_SDK_ROW_WIDENS(is_touch)
  FLIGHT_SDK_ROW_WIDENS(is_virtual)
  FLIGHT_SDK_ROW_WIDENS(joint_collision_suppressions)
  FLIGHT_SDK_ROW_WIDENS(joint_solvers)
  FLIGHT_SDK_ROW_WIDENS(kerning)
  FLIGHT_SDK_ROW_WIDENS(key)
  FLIGHT_SDK_ROW_WIDENS(kind)
  FLIGHT_SDK_ROW_WIDENS(latitude)
  FLIGHT_SDK_ROW_WIDENS(layer_mask)
  FLIGHT_SDK_ROW_WIDENS(leading)
  FLIGHT_SDK_ROW_WIDENS(leaf_by_object)
  FLIGHT_SDK_ROW_WIDENS(left)
  FLIGHT_SDK_ROW_WIDENS(levels)
  FLIGHT_SDK_ROW_WIDENS(life)
  FLIGHT_SDK_ROW_WIDENS(life_offset)
  FLIGHT_SDK_ROW_WIDENS(lifetime_max)
  FLIGHT_SDK_ROW_WIDENS(lifetime_min)
  FLIGHT_SDK_ROW_WIDENS(lift)
  FLIGHT_SDK_ROW_WIDENS(light_color)
  FLIGHT_SDK_ROW_WIDENS(light_x)
  FLIGHT_SDK_ROW_WIDENS(light_y)
  FLIGHT_SDK_ROW_WIDENS(lightness)
  FLIGHT_SDK_ROW_WIDENS(line_index)
  FLIGHT_SDK_ROW_WIDENS(linear_length)
  FLIGHT_SDK_ROW_WIDENS(linear_start)
  FLIGHT_SDK_ROW_WIDENS(locale)
  FLIGHT_SDK_ROW_WIDENS(logical_height)
  FLIGHT_SDK_ROW_WIDENS(logical_width)
  FLIGHT_SDK_ROW_WIDENS(longitude)
  FLIGHT_SDK_ROW_WIDENS(loop)
  FLIGHT_SDK_ROW_WIDENS(low_max)
  FLIGHT_SDK_ROW_WIDENS(low_min)
  FLIGHT_SDK_ROW_WIDENS(lut)
  FLIGHT_SDK_ROW_WIDENS(m)
  FLIGHT_SDK_ROW_WIDENS(mag_filter)
  FLIGHT_SDK_ROW_WIDENS(manufacturer)
  FLIGHT_SDK_ROW_WIDENS(map)
  FLIGHT_SDK_ROW_WIDENS(margin)
  FLIGHT_SDK_ROW_WIDENS(marketing_name)
  FLIGHT_SDK_ROW_WIDENS(material)
  FLIGHT_SDK_ROW_WIDENS(material_data)
  FLIGHT_SDK_ROW_WIDENS(matrix)
  FLIGHT_SDK_ROW_WIDENS(matrix_x)
  FLIGHT_SDK_ROW_WIDENS(matrix_y)
  FLIGHT_SDK_ROW_WIDENS(max)
  FLIGHT_SDK_ROW_WIDENS(max_brightness)
  FLIGHT_SDK_ROW_WIDENS(max_distance)
  FLIGHT_SDK_ROW_WIDENS(max_ev)
  FLIGHT_SDK_ROW_WIDENS(max_exposure)
  FLIGHT_SDK_ROW_WIDENS(max_particle_count)
  FLIGHT_SDK_ROW_WIDENS(max_particles)
  FLIGHT_SDK_ROW_WIDENS(max_radius)
  FLIGHT_SDK_ROW_WIDENS(max_radius_variance)
  FLIGHT_SDK_ROW_WIDENS(max_x)
  FLIGHT_SDK_ROW_WIDENS(max_y)
  FLIGHT_SDK_ROW_WIDENS(max_z)
  FLIGHT_SDK_ROW_WIDENS(metadata)
  FLIGHT_SDK_ROW_WIDENS(metrics)
  FLIGHT_SDK_ROW_WIDENS(min)
  FLIGHT_SDK_ROW_WIDENS(min_ev)
  FLIGHT_SDK_ROW_WIDENS(min_exposure)
  FLIGHT_SDK_ROW_WIDENS(min_filter)
  FLIGHT_SDK_ROW_WIDENS(min_particle_count)
  FLIGHT_SDK_ROW_WIDENS(min_radius)
  FLIGHT_SDK_ROW_WIDENS(min_radius_variance)
  FLIGHT_SDK_ROW_WIDENS(min_x)
  FLIGHT_SDK_ROW_WIDENS(min_y)
  FLIGHT_SDK_ROW_WIDENS(min_z)
  FLIGHT_SDK_ROW_WIDENS(mipmaps)
  FLIGHT_SDK_ROW_WIDENS(mode)
  FLIGHT_SDK_ROW_WIDENS(model)
  FLIGHT_SDK_ROW_WIDENS(modifiers)
  FLIGHT_SDK_ROW_WIDENS(name)
  FLIGHT_SDK_ROW_WIDENS(near)
  FLIGHT_SDK_ROW_WIDENS(normal_bias)
  FLIGHT_SDK_ROW_WIDENS(normal_x)
  FLIGHT_SDK_ROW_WIDENS(normal_y)
  FLIGHT_SDK_ROW_WIDENS(normal_z)
  FLIGHT_SDK_ROW_WIDENS(notification)
  FLIGHT_SDK_ROW_WIDENS(num_lines)
  FLIGHT_SDK_ROW_WIDENS(object)
  FLIGHT_SDK_ROW_WIDENS(offset)
  FLIGHT_SDK_ROW_WIDENS(offset_x)
  FLIGHT_SDK_ROW_WIDENS(offset_y)
  FLIGHT_SDK_ROW_WIDENS(offsets)
  FLIGHT_SDK_ROW_WIDENS(on_absolute_orientation)
  FLIGHT_SDK_ROW_WIDENS(on_accelerometer)
  FLIGHT_SDK_ROW_WIDENS(on_action)
  FLIGHT_SDK_ROW_WIDENS(on_ambient_light)
  FLIGHT_SDK_ROW_WIDENS(on_barometer)
  FLIGHT_SDK_ROW_WIDENS(on_change)
  FLIGHT_SDK_ROW_WIDENS(on_emitter_complete)
  FLIGHT_SDK_ROW_WIDENS(on_gravity)
  FLIGHT_SDK_ROW_WIDENS(on_gyroscope)
  FLIGHT_SDK_ROW_WIDENS(on_hide)
  FLIGHT_SDK_ROW_WIDENS(on_linear_acceleration)
  FLIGHT_SDK_ROW_WIDENS(on_magnetometer)
  FLIGHT_SDK_ROW_WIDENS(on_orientation)
  FLIGHT_SDK_ROW_WIDENS(on_particle_death)
  FLIGHT_SDK_ROW_WIDENS(on_particle_spawn)
  FLIGHT_SDK_ROW_WIDENS(on_proximity)
  FLIGHT_SDK_ROW_WIDENS(on_quaternion)
  FLIGHT_SDK_ROW_WIDENS(on_resize)
  FLIGHT_SDK_ROW_WIDENS(on_show)
  FLIGHT_SDK_ROW_WIDENS(on_tick)
  FLIGHT_SDK_ROW_WIDENS(once)
  FLIGHT_SDK_ROW_WIDENS(operation)
  FLIGHT_SDK_ROW_WIDENS(operator_)
  FLIGHT_SDK_ROW_WIDENS(orientation_w)
  FLIGHT_SDK_ROW_WIDENS(orientation_x)
  FLIGHT_SDK_ROW_WIDENS(orientation_y)
  FLIGHT_SDK_ROW_WIDENS(orientation_z)
  FLIGHT_SDK_ROW_WIDENS(origin)
  FLIGHT_SDK_ROW_WIDENS(origin_x)
  FLIGHT_SDK_ROW_WIDENS(origin_y)
  FLIGHT_SDK_ROW_WIDENS(os_build)
  FLIGHT_SDK_ROW_WIDENS(os_name)
  FLIGHT_SDK_ROW_WIDENS(os_version)
  FLIGHT_SDK_ROW_WIDENS(outer_cone_cos)
  FLIGHT_SDK_ROW_WIDENS(outer_cone_degrees)
  FLIGHT_SDK_ROW_WIDENS(overlapping)
  FLIGHT_SDK_ROW_WIDENS(overlays_content)
  FLIGHT_SDK_ROW_WIDENS(pages)
  FLIGHT_SDK_ROW_WIDENS(parent)
  FLIGHT_SDK_ROW_WIDENS(particle_lifespan)
  FLIGHT_SDK_ROW_WIDENS(particle_lifespan_variance)
  FLIGHT_SDK_ROW_WIDENS(paused)
  FLIGHT_SDK_ROW_WIDENS(pcf_radius)
  FLIGHT_SDK_ROW_WIDENS(pedestal)
  FLIGHT_SDK_ROW_WIDENS(physical_height)
  FLIGHT_SDK_ROW_WIDENS(physical_width)
  FLIGHT_SDK_ROW_WIDENS(pivot_x)
  FLIGHT_SDK_ROW_WIDENS(pivot_y)
  FLIGHT_SDK_ROW_WIDENS(pixel_ratio)
  FLIGHT_SDK_ROW_WIDENS(platform_string)
  FLIGHT_SDK_ROW_WIDENS(playback_rate)
  FLIGHT_SDK_ROW_WIDENS(point)
  FLIGHT_SDK_ROW_WIDENS(point_count)
  FLIGHT_SDK_ROW_WIDENS(pointer_width)
  FLIGHT_SDK_ROW_WIDENS(points)
  FLIGHT_SDK_ROW_WIDENS(position)
  FLIGHT_SDK_ROW_WIDENS(positions)
  FLIGHT_SDK_ROW_WIDENS(premultiplied_alpha)
  FLIGHT_SDK_ROW_WIDENS(prepare)
  FLIGHT_SDK_ROW_WIDENS(preserve_alpha)
  FLIGHT_SDK_ROW_WIDENS(pressure)
  FLIGHT_SDK_ROW_WIDENS(previous_world_transform)
  FLIGHT_SDK_ROW_WIDENS(priority)
  FLIGHT_SDK_ROW_WIDENS(product_name)
  FLIGHT_SDK_ROW_WIDENS(projection)
  FLIGHT_SDK_ROW_WIDENS(prompt_for_access)
  FLIGHT_SDK_ROW_WIDENS(query_spatial_pairs)
  FLIGHT_SDK_ROW_WIDENS(query_spatial_point)
  FLIGHT_SDK_ROW_WIDENS(query_spatial_ray)
  FLIGHT_SDK_ROW_WIDENS(query_spatial_region)
  FLIGHT_SDK_ROW_WIDENS(r_ax)
  FLIGHT_SDK_ROW_WIDENS(r_ay)
  FLIGHT_SDK_ROW_WIDENS(r_az)
  FLIGHT_SDK_ROW_WIDENS(r_bx)
  FLIGHT_SDK_ROW_WIDENS(r_by)
  FLIGHT_SDK_ROW_WIDENS(r_bz)
  FLIGHT_SDK_ROW_WIDENS(radial)
  FLIGHT_SDK_ROW_WIDENS(radial_accel_variance)
  FLIGHT_SDK_ROW_WIDENS(radial_acceleration)
  FLIGHT_SDK_ROW_WIDENS(radius)
  FLIGHT_SDK_ROW_WIDENS(range)
  FLIGHT_SDK_ROW_WIDENS(read_bookmark)
  FLIGHT_SDK_ROW_WIDENS(read_format)
  FLIGHT_SDK_ROW_WIDENS(read_html)
  FLIGHT_SDK_ROW_WIDENS(read_image)
  FLIGHT_SDK_ROW_WIDENS(read_items)
  FLIGHT_SDK_ROW_WIDENS(read_rtf)
  FLIGHT_SDK_ROW_WIDENS(read_text)
  FLIGHT_SDK_ROW_WIDENS(reason)
  FLIGHT_SDK_ROW_WIDENS(red)
  FLIGHT_SDK_ROW_WIDENS(red_bias)
  FLIGHT_SDK_ROW_WIDENS(red_scale)
  FLIGHT_SDK_ROW_WIDENS(refresh)
  FLIGHT_SDK_ROW_WIDENS(region_id_max)
  FLIGHT_SDK_ROW_WIDENS(region_id_min)
  FLIGHT_SDK_ROW_WIDENS(relative)
  FLIGHT_SDK_ROW_WIDENS(remove_node)
  FLIGHT_SDK_ROW_WIDENS(remove_spatial_object)
  FLIGHT_SDK_ROW_WIDENS(repeat_count)
  FLIGHT_SDK_ROW_WIDENS(request_permission)
  FLIGHT_SDK_ROW_WIDENS(request_persistence)
  FLIGHT_SDK_ROW_WIDENS(resize)
  FLIGHT_SDK_ROW_WIDENS(resolution)
  FLIGHT_SDK_ROW_WIDENS(restitution)
  FLIGHT_SDK_ROW_WIDENS(right)
  FLIGHT_SDK_ROW_WIDENS(root)
  FLIGHT_SDK_ROW_WIDENS(rotate_per_second)
  FLIGHT_SDK_ROW_WIDENS(rotate_per_second_variance)
  FLIGHT_SDK_ROW_WIDENS(rotated)
  FLIGHT_SDK_ROW_WIDENS(rotation)
  FLIGHT_SDK_ROW_WIDENS(rotation_amplitude)
  FLIGHT_SDK_ROW_WIDENS(rotation_end)
  FLIGHT_SDK_ROW_WIDENS(rotation_end_variance)
  FLIGHT_SDK_ROW_WIDENS(rotation_speed_max)
  FLIGHT_SDK_ROW_WIDENS(rotation_speed_min)
  FLIGHT_SDK_ROW_WIDENS(rotation_start)
  FLIGHT_SDK_ROW_WIDENS(rotation_start_variance)
  FLIGHT_SDK_ROW_WIDENS(rotation_x)
  FLIGHT_SDK_ROW_WIDENS(rotation_y)
  FLIGHT_SDK_ROW_WIDENS(rotation_z)
  FLIGHT_SDK_ROW_WIDENS(runtime)
  FLIGHT_SDK_ROW_WIDENS(samples)
  FLIGHT_SDK_ROW_WIDENS(saturation)
  FLIGHT_SDK_ROW_WIDENS(scale)
  FLIGHT_SDK_ROW_WIDENS(scale_curve)
  FLIGHT_SDK_ROW_WIDENS(scale_end)
  FLIGHT_SDK_ROW_WIDENS(scale_max)
  FLIGHT_SDK_ROW_WIDENS(scale_min)
  FLIGHT_SDK_ROW_WIDENS(scale_x)
  FLIGHT_SDK_ROW_WIDENS(scale_y)
  FLIGHT_SDK_ROW_WIDENS(scaling)
  FLIGHT_SDK_ROW_WIDENS(scanline_intensity)
  FLIGHT_SDK_ROW_WIDENS(scattering)
  FLIGHT_SDK_ROW_WIDENS(scope)
  FLIGHT_SDK_ROW_WIDENS(seed)
  FLIGHT_SDK_ROW_WIDENS(segment)
  FLIGHT_SDK_ROW_WIDENS(selection)
  FLIGHT_SDK_ROW_WIDENS(send)
  FLIGHT_SDK_ROW_WIDENS(sensor)
  FLIGHT_SDK_ROW_WIDENS(set_accessory_bar_visible)
  FLIGHT_SDK_ROW_WIDENS(set_background_color)
  FLIGHT_SDK_ROW_WIDENS(set_display_size)
  FLIGHT_SDK_ROW_WIDENS(set_focus)
  FLIGHT_SDK_ROW_WIDENS(set_metadata)
  FLIGHT_SDK_ROW_WIDENS(set_node)
  FLIGHT_SDK_ROW_WIDENS(set_overlays_content)
  FLIGHT_SDK_ROW_WIDENS(set_playback_state)
  FLIGHT_SDK_ROW_WIDENS(set_position_state)
  FLIGHT_SDK_ROW_WIDENS(set_resize_mode)
  FLIGHT_SDK_ROW_WIDENS(set_scroll_assist_enabled)
  FLIGHT_SDK_ROW_WIDENS(set_style)
  FLIGHT_SDK_ROW_WIDENS(set_visible)
  FLIGHT_SDK_ROW_WIDENS(shader_key)
  FLIGHT_SDK_ROW_WIDENS(shadow_bias)
  FLIGHT_SDK_ROW_WIDENS(shadow_far)
  FLIGHT_SDK_ROW_WIDENS(shadow_map_size)
  FLIGHT_SDK_ROW_WIDENS(shadow_near)
  FLIGHT_SDK_ROW_WIDENS(shadow_strength)
  FLIGHT_SDK_ROW_WIDENS(sheen_color)
  FLIGHT_SDK_ROW_WIDENS(sheen_color_map)
  FLIGHT_SDK_ROW_WIDENS(sheen_color_map_uv_set)
  FLIGHT_SDK_ROW_WIDENS(sheen_roughness)
  FLIGHT_SDK_ROW_WIDENS(sheen_roughness_map)
  FLIGHT_SDK_ROW_WIDENS(sheen_roughness_map_uv_set)
  FLIGHT_SDK_ROW_WIDENS(show)
  FLIGHT_SDK_ROW_WIDENS(size)
  FLIGHT_SDK_ROW_WIDENS(skew_x)
  FLIGHT_SDK_ROW_WIDENS(skew_y)
  FLIGHT_SDK_ROW_WIDENS(sky_color)
  FLIGHT_SDK_ROW_WIDENS(slot_index)
  FLIGHT_SDK_ROW_WIDENS(slots)
  FLIGHT_SDK_ROW_WIDENS(smooth_time)
  FLIGHT_SDK_ROW_WIDENS(softness)
  FLIGHT_SDK_ROW_WIDENS(source_height)
  FLIGHT_SDK_ROW_WIDENS(source_mode)
  FLIGHT_SDK_ROW_WIDENS(source_position_variancex)
  FLIGHT_SDK_ROW_WIDENS(source_position_variancey)
  FLIGHT_SDK_ROW_WIDENS(source_width)
  FLIGHT_SDK_ROW_WIDENS(spawn_height)
  FLIGHT_SDK_ROW_WIDENS(spawn_rate)
  FLIGHT_SDK_ROW_WIDENS(spawn_shape)
  FLIGHT_SDK_ROW_WIDENS(spawn_width)
  FLIGHT_SDK_ROW_WIDENS(specular)
  FLIGHT_SDK_ROW_WIDENS(specular_color)
  FLIGHT_SDK_ROW_WIDENS(specular_color_map)
  FLIGHT_SDK_ROW_WIDENS(specular_color_map_uv_set)
  FLIGHT_SDK_ROW_WIDENS(specular_map)
  FLIGHT_SDK_ROW_WIDENS(specular_map_uv_set)
  FLIGHT_SDK_ROW_WIDENS(speed)
  FLIGHT_SDK_ROW_WIDENS(speed_max)
  FLIGHT_SDK_ROW_WIDENS(speed_min)
  FLIGHT_SDK_ROW_WIDENS(speed_variance)
  FLIGHT_SDK_ROW_WIDENS(spot)
  FLIGHT_SDK_ROW_WIDENS(spot_blend)
  FLIGHT_SDK_ROW_WIDENS(spread)
  FLIGHT_SDK_ROW_WIDENS(stack)
  FLIGHT_SDK_ROW_WIDENS(start)
  FLIGHT_SDK_ROW_WIDENS(start_color)
  FLIGHT_SDK_ROW_WIDENS(start_color_variance)
  FLIGHT_SDK_ROW_WIDENS(start_index)
  FLIGHT_SDK_ROW_WIDENS(start_particle_size)
  FLIGHT_SDK_ROW_WIDENS(start_particle_size_variance)
  FLIGHT_SDK_ROW_WIDENS(start_x)
  FLIGHT_SDK_ROW_WIDENS(start_y)
  FLIGHT_SDK_ROW_WIDENS(start_z)
  FLIGHT_SDK_ROW_WIDENS(stated)
  FLIGHT_SDK_ROW_WIDENS(steps)
  FLIGHT_SDK_ROW_WIDENS(strength)
  FLIGHT_SDK_ROW_WIDENS(stroke_bounds)
  FLIGHT_SDK_ROW_WIDENS(style)
  FLIGHT_SDK_ROW_WIDENS(subject)
  FLIGHT_SDK_ROW_WIDENS(subpixel)
  FLIGHT_SDK_ROW_WIDENS(subscribe)
  FLIGHT_SDK_ROW_WIDENS(subscribe_absolute_orientation)
  FLIGHT_SDK_ROW_WIDENS(subscribe_ambient_light)
  FLIGHT_SDK_ROW_WIDENS(subscribe_barometer)
  FLIGHT_SDK_ROW_WIDENS(subscribe_gravity)
  FLIGHT_SDK_ROW_WIDENS(subscribe_linear_acceleration)
  FLIGHT_SDK_ROW_WIDENS(subscribe_magnetometer)
  FLIGHT_SDK_ROW_WIDENS(subscribe_motion)
  FLIGHT_SDK_ROW_WIDENS(subscribe_orientation)
  FLIGHT_SDK_ROW_WIDENS(subscribe_proximity)
  FLIGHT_SDK_ROW_WIDENS(subscribe_quaternion)
  FLIGHT_SDK_ROW_WIDENS(supported_abis)
  FLIGHT_SDK_ROW_WIDENS(tangential_accel_variance)
  FLIGHT_SDK_ROW_WIDENS(tangential_acceleration)
  FLIGHT_SDK_ROW_WIDENS(temperature)
  FLIGHT_SDK_ROW_WIDENS(text_height)
  FLIGHT_SDK_ROW_WIDENS(text_width)
  FLIGHT_SDK_ROW_WIDENS(texture_file_name)
  FLIGHT_SDK_ROW_WIDENS(thickness)
  FLIGHT_SDK_ROW_WIDENS(thickness_map)
  FLIGHT_SDK_ROW_WIDENS(thickness_map_uv_set)
  FLIGHT_SDK_ROW_WIDENS(threshold)
  FLIGHT_SDK_ROW_WIDENS(tilesets)
  FLIGHT_SDK_ROW_WIDENS(time)
  FLIGHT_SDK_ROW_WIDENS(timeline)
  FLIGHT_SDK_ROW_WIDENS(timestamp)
  FLIGHT_SDK_ROW_WIDENS(tint)
  FLIGHT_SDK_ROW_WIDENS(top)
  FLIGHT_SDK_ROW_WIDENS(torque)
  FLIGHT_SDK_ROW_WIDENS(torque_x)
  FLIGHT_SDK_ROW_WIDENS(torque_y)
  FLIGHT_SDK_ROW_WIDENS(torque_z)
  FLIGHT_SDK_ROW_WIDENS(total_memory)
  FLIGHT_SDK_ROW_WIDENS(touching)
  FLIGHT_SDK_ROW_WIDENS(transform)
  FLIGHT_SDK_ROW_WIDENS(translation_amplitude)
  FLIGHT_SDK_ROW_WIDENS(transmission)
  FLIGHT_SDK_ROW_WIDENS(transmission_map)
  FLIGHT_SDK_ROW_WIDENS(transmission_map_uv_set)
  FLIGHT_SDK_ROW_WIDENS(transparency)
  FLIGHT_SDK_ROW_WIDENS(trauma)
  FLIGHT_SDK_ROW_WIDENS(tweens)
  FLIGHT_SDK_ROW_WIDENS(tx)
  FLIGHT_SDK_ROW_WIDENS(ty)
  FLIGHT_SDK_ROW_WIDENS(type)
  FLIGHT_SDK_ROW_WIDENS(uniforms)
  FLIGHT_SDK_ROW_WIDENS(up)
  FLIGHT_SDK_ROW_WIDENS(update_spatial_object)
  FLIGHT_SDK_ROW_WIDENS(value)
  FLIGHT_SDK_ROW_WIDENS(velocity)
  FLIGHT_SDK_ROW_WIDENS(velocity_inheritance)
  FLIGHT_SDK_ROW_WIDENS(version)
  FLIGHT_SDK_ROW_WIDENS(vibrate)
  FLIGHT_SDK_ROW_WIDENS(vibrate_pattern)
  FLIGHT_SDK_ROW_WIDENS(vibrate_waveform)
  FLIGHT_SDK_ROW_WIDENS(view)
  FLIGHT_SDK_ROW_WIDENS(viewport_height)
  FLIGHT_SDK_ROW_WIDENS(viewport_width)
  FLIGHT_SDK_ROW_WIDENS(vignette)
  FLIGHT_SDK_ROW_WIDENS(visible)
  FLIGHT_SDK_ROW_WIDENS(w)
  FLIGHT_SDK_ROW_WIDENS(watch_position)
  FLIGHT_SDK_ROW_WIDENS(web_view_version)
  FLIGHT_SDK_ROW_WIDENS(weight)
  FLIGHT_SDK_ROW_WIDENS(white)
  FLIGHT_SDK_ROW_WIDENS(width)
  FLIGHT_SDK_ROW_WIDENS(wind)
  FLIGHT_SDK_ROW_WIDENS(winding)
  FLIGHT_SDK_ROW_WIDENS(world_bounds)
  FLIGHT_SDK_ROW_WIDENS(world_matrices)
  FLIGHT_SDK_ROW_WIDENS(world_space)
  FLIGHT_SDK_ROW_WIDENS(would_occupy_bucket_count)
  FLIGHT_SDK_ROW_WIDENS(wrap_u)
  FLIGHT_SDK_ROW_WIDENS(wrap_v)
  FLIGHT_SDK_ROW_WIDENS(wrapped_diffuse_color)
  FLIGHT_SDK_ROW_WIDENS(wrapped_diffuse_map)
  FLIGHT_SDK_ROW_WIDENS(wrapped_diffuse_map_uv_set)
  FLIGHT_SDK_ROW_WIDENS(wrapped_diffuse_strength)
  FLIGHT_SDK_ROW_WIDENS(write_bookmark)
  FLIGHT_SDK_ROW_WIDENS(write_format)
  FLIGHT_SDK_ROW_WIDENS(write_html)
  FLIGHT_SDK_ROW_WIDENS(write_image)
  FLIGHT_SDK_ROW_WIDENS(write_items)
  FLIGHT_SDK_ROW_WIDENS(write_rtf)
  FLIGHT_SDK_ROW_WIDENS(write_text)
  FLIGHT_SDK_ROW_WIDENS(x)
  FLIGHT_SDK_ROW_WIDENS(x0)
  FLIGHT_SDK_ROW_WIDENS(x1)
  FLIGHT_SDK_ROW_WIDENS(x_offset)
  FLIGHT_SDK_ROW_WIDENS(y)
  FLIGHT_SDK_ROW_WIDENS(y0)
  FLIGHT_SDK_ROW_WIDENS(y1)
  FLIGHT_SDK_ROW_WIDENS(y_offset)
  FLIGHT_SDK_ROW_WIDENS(z)
  FLIGHT_SDK_ROW_WIDENS(zoom)
  return matched > 0;
}

#undef FLIGHT_SDK_ROW_WIDENS
#undef FLIGHT_SDK_ROW_COMPUTED

// The only specialization of the runtime trait: yes, for the pairs proven above.
template <typename Base, typename Derived>
  requires(generated_row_widening_matches<Base, Derived>())
struct GeneratedRowWidening<Base, Derived> : std::true_type {};

} // namespace flight::detail
