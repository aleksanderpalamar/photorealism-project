#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
zig_bin="${ZIG_BIN:-zig}"
build_dir="${project_dir}/build"

mkdir -p "${build_dir}"

common_flags=(
  -target x86_64-windows-gnu
  -std=c++20
  -O2
  -Wno-nullability-completeness
  -DUNICODE
  -D_UNICODE
  -DWIN32_LEAN_AND_MEAN
  -DNOMINMAX
  -shared
)

"${zig_bin}" c++ \
  "${common_flags[@]}" \
  -Wall \
  -Wextra \
  -Werror \
  -Wno-cast-function-type-mismatch \
  "${project_dir}/src/proxy.cpp" \
  "${project_dir}/src/dinput/input_gate.cpp" \
  "${project_dir}/src/dinput/mouse_report.cpp" \
  "${project_dir}/src/dinput/menu_gate.cpp" \
  "${project_dir}/src/hooks/vtable_patch.cpp" \
  "${project_dir}/src/native_aa/apply.cpp" \
  "${project_dir}/src/fsr/game_scale.cpp" \
  "${project_dir}/src/fsr/render_scale.cpp" \
  "${project_dir}/src/native_aa/policy.cpp" \
  "${project_dir}/src/native_aa/game_target.cpp" \
  "${project_dir}/src/native_aa/config_file.cpp" \
  "${project_dir}/src/config/file_io.cpp" \
  "${project_dir}/src/native_aa/aa_log.cpp" \
  "${project_dir}/src/config/path_utils.cpp" \
  "${project_dir}/src/native_graphics/graphics_apply.cpp" \
  "${project_dir}/src/native_graphics/graphics_policy.cpp" \
  "${project_dir}/src/dinput8.def" \
  -o "${build_dir}/dinput8.dll" \
  -lole32 \
  -lshell32

"${zig_bin}" c++ \
  "${common_flags[@]}" \
  -Wall \
  -Wextra \
  -Werror \
  -Wno-cast-function-type-mismatch \
  "${project_dir}/src/dxgi_proxy.cpp" \
  "${project_dir}/src/hooks/hook_install.cpp" \
  "${project_dir}/src/hooks/hook_state.cpp" \
  "${project_dir}/src/hooks/hook_audit.cpp" \
  "${project_dir}/src/hooks/swap_chain_hooks.cpp" \
  "${project_dir}/src/hooks/context_hooks.cpp" \
  "${project_dir}/src/frame_capture/bind_runs.cpp" \
  "${project_dir}/src/passfx/pass_effects.cpp" \
  "${project_dir}/src/passfx/pre_tone_effect.cpp" \
  "${project_dir}/src/frame_capture/capture_folder.cpp" \
  "${project_dir}/src/frame_capture/capture_manifest.cpp" \
  "${project_dir}/src/frame_capture/constant_snapshots.cpp" \
  "${project_dir}/src/frame_capture/dds_header.cpp" \
  "${project_dir}/src/frame_capture/frame_capture.cpp" \
  "${project_dir}/src/frame_capture/staging_snapshots.cpp" \
  "${project_dir}/src/frame_capture/target_description.cpp" \
  "${project_dir}/src/hooks/device_probe.cpp" \
  "${project_dir}/src/hooks/vtable_patch.cpp" \
  "${project_dir}/src/hooks/module_names.cpp" \
  "${project_dir}/src/hooks/present_target.cpp" \
  "${project_dir}/src/postprocess/postprocessor.cpp" \
  "${project_dir}/src/postprocess/device_state.cpp" \
  "${project_dir}/src/postprocess/gpu_timer.cpp" \
  "${project_dir}/src/postprocess/shader_library.cpp" \
  "${project_dir}/src/postprocess/shader_compiler.cpp" \
  "${project_dir}/src/postprocess/bloom_pyramid.cpp" \
  "${project_dir}/src/postprocess/bloom_resources.cpp" \
  "${project_dir}/src/postprocess/temporal_history.cpp" \
  "${project_dir}/src/postprocess/depth_capture.cpp" \
  "${project_dir}/src/postprocess/depth_liveness.cpp" \
  "${project_dir}/src/postprocess/frame_resources.cpp" \
  "${project_dir}/src/postprocess/condition_adapter.cpp" \
  "${project_dir}/src/postprocess/pipeline_state.cpp" \
  "${project_dir}/src/postprocess/frame_constants.cpp" \
  "${project_dir}/src/postprocess/frame_log.cpp" \
  "${project_dir}/src/postprocess/frame_passes.cpp" \
  "${project_dir}/src/postprocess/effect_chain.cpp" \
  "${project_dir}/src/postprocess/effect_draws.cpp" \
  "${project_dir}/src/postprocess/effect_runner.cpp" \
  "${project_dir}/src/postprocess/effect_shaders.cpp" \
  "${project_dir}/src/postprocess/effect_constants_upload.cpp" \
  "${project_dir}/src/postprocess/occlusion_target.cpp" \
  "${project_dir}/src/postprocess/effect_log.cpp" \
  "${project_dir}/src/steam/capture_pipeline.cpp" \
  "${project_dir}/src/steam/integration.cpp" \
  "${project_dir}/src/steam/capture_gate.cpp" \
  "${project_dir}/src/steam/capture_slots.cpp" \
  "${project_dir}/src/steam/conversion_worker.cpp" \
  "${project_dir}/src/steam/steam_api.cpp" \
  "${project_dir}/src/config/loader.cpp" \
  "${project_dir}/src/config/file_io.cpp" \
  "${project_dir}/src/config/writer.cpp" \
  "${project_dir}/src/config/path_utils.cpp" \
  "${project_dir}/src/config/defaults.cpp" \
  "${project_dir}/src/config/section_table.cpp" \
  "${project_dir}/src/config/photorealism_profile.cpp" \
  "${project_dir}/src/config/profile_fields.cpp" \
  "${project_dir}/src/config/profile_logging.cpp" \
  "${project_dir}/src/config/profile_reference.cpp" \
  "${project_dir}/src/config/profile_state.cpp" \
  "${project_dir}/src/config/effect_quality.cpp" \
  "${project_dir}/src/config/effect_logging.cpp" \
  "${project_dir}/src/config/limits.cpp" \
  "${project_dir}/src/config/logging.cpp" \
  "${project_dir}/src/resource_observer/discovery_control.cpp" \
  "${project_dir}/src/resource_observer/depth_observation.cpp" \
  "${project_dir}/src/resource_observer/candidate_access.cpp" \
  "${project_dir}/src/resource_observer/discovery.cpp" \
  "${project_dir}/src/resource_observer/discovery_scan.cpp" \
  "${project_dir}/src/resource_observer/discovery_report.cpp" \
  "${project_dir}/src/resource_observer/observer_state.cpp" \
  "${project_dir}/src/resource_observer/format_names.cpp" \
  "${project_dir}/src/resource_observer/color_observation.cpp" \
  "${project_dir}/src/resource_observer/color_capture.cpp" \
  "${project_dir}/src/resource_observer/pass_trace.cpp" \
  "${project_dir}/src/resource_observer/view_shape.cpp" \
  "${project_dir}/src/scene/sampler.cpp" \
  "${project_dir}/src/scene/sampler_resources.cpp" \
  "${project_dir}/src/scene/observer.cpp" \
  "${project_dir}/src/overlay/draw_list.cpp" \
  "${project_dir}/src/overlay/layout.cpp" \
  "${project_dir}/src/overlay/panel.cpp" \
  "${project_dir}/src/overlay/page_view.cpp" \
  "${project_dir}/src/overlay/page_rows.cpp" \
  "${project_dir}/src/overlay/page_keys.cpp" \
  "${project_dir}/src/overlay/persistence.cpp" \
  "${project_dir}/src/overlay/widgets/button.cpp" \
  "${project_dir}/src/overlay/widgets/slider.cpp" \
  "${project_dir}/src/overlay/widgets/choice.cpp" \
  "${project_dir}/src/overlay/bindings/binding_values.cpp" \
  "${project_dir}/src/overlay/bindings/menu_pages.cpp" \
  "${project_dir}/src/overlay/font.cpp" \
  "${project_dir}/src/overlay/font_bitmap.cpp" \
  "${project_dir}/src/overlay/text.cpp" \
  "${project_dir}/src/overlay/input.cpp" \
  "${project_dir}/src/overlay/renderer.cpp" \
  "${project_dir}/src/overlay/renderer_states.cpp" \
  "${project_dir}/src/overlay/renderer_buffers.cpp" \
  "${project_dir}/src/overlay/overlay.cpp" \
  "${project_dir}/src/overlay/pointer_feed.cpp" \
  "${project_dir}/src/overlay/menu_export.cpp" \
  "${project_dir}/src/fsr/render_scale.cpp" \
  "${project_dir}/src/fsr/fsr_telemetry.cpp" \
  "${project_dir}/src/fsr/upscale_resources.cpp" \
  "${project_dir}/src/fsr/fsr_shaders.cpp" \
  "${project_dir}/src/fsr/fsr_states.cpp" \
  "${project_dir}/src/fsr/blue_noise.cpp" \
  "${project_dir}/src/fsr/grain_texture.cpp" \
  "${project_dir}/src/fsr/output_target.cpp" \
  "${project_dir}/src/fsr/upscale_pipeline.cpp" \
  "${project_dir}/src/fsr/internal_frame.cpp" \
  "${project_dir}/src/fsr/upscaler.cpp" \
  "${project_dir}/src/runtime.cpp" \
  "${project_dir}/src/dxgi.def" \
  -o "${build_dir}/dxgi.dll" \
  -luser32 \
  -lgdi32 \
  -lshell32 \
  -lole32

echo "Gerado: ${build_dir}/dinput8.dll"
echo "Gerado: ${build_dir}/dxgi.dll"
