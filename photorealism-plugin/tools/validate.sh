#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dinput_dll="${project_dir}/build/dinput8.dll"
dxgi_dll="${project_dir}/build/dxgi.dll"
fsr_dll="${project_dir}/build/photorealism-fsr.dll"

for dll in "${dinput_dll}" "${dxgi_dll}" "${fsr_dll}"; do
  if [[ ! -f "${dll}" ]]; then
    echo "DLL ausente: execute tools/build.sh primeiro: ${dll}" >&2
    exit 1
  fi
  file "${dll}" | grep -q 'PE32+.*DLL.*x86-64'
done

dinput_metadata="$(objdump -p "${dinput_dll}")"
expected_dinput_exports=(
  '1:DirectInput8Create'
  '2:DllRegisterServer'
  '3:DllUnregisterServer'
  '4:DllCanUnloadNow'
  '5:DllGetClassObject'
  '6:GetdfDIJoystick'
)

for expected in "${expected_dinput_exports[@]}"; do
  ordinal="${expected%%:*}"
  name="${expected#*:}"
  if ! grep -Eq "\\+base\\[[[:space:]]*${ordinal}\\].*[[:space:]]${name}$" \
      <<<"${dinput_metadata}"; then
    echo "Export dinput8 ausente ou com ordinal incorreto: ${name} @${ordinal}" >&2
    exit 1
  fi
done

if grep -Eqi 'DLL Name: (d3d11|dxgi|d3dcompiler)' <<<"${dinput_metadata}"; then
  echo "O bootstrap dinput8 possui dependencia grafica inesperada." >&2
  exit 1
fi

dxgi_metadata="$(objdump -p "${dxgi_dll}")"
expected_dxgi_exports=(
  '1:CreateDXGIFactory'
  '2:CreateDXGIFactory1'
  '3:CreateDXGIFactory2'
)

for expected in "${expected_dxgi_exports[@]}"; do
  ordinal="${expected%%:*}"
  name="${expected#*:}"
  if ! grep -Eq "\\+base\\[[[:space:]]*${ordinal}\\].*[[:space:]]${name}$" \
      <<<"${dxgi_metadata}"; then
    echo "Export dxgi ausente ou com ordinal incorreto: ${name} @${ordinal}" >&2
    exit 1
  fi
done

if grep -Eqi 'DLL Name: dxgi\.dll' <<<"${dxgi_metadata}"; then
  echo "O proxy dxgi possui uma auto-dependencia recursiva." >&2
  exit 1
fi

if grep -Fqi 'DLL Name: photorealism-fsr.dll' <<<"${dxgi_metadata}"; then
  echo "O nucleo possui dependencia estatica do modulo FSR opcional." >&2
  exit 1
fi

fsr_metadata="$(objdump -p "${fsr_dll}")"
if ! grep -Eq \
    '\+base\[[[:space:]]*1\].*[[:space:]]PhotorealismFsrGetApi$' \
    <<<"${fsr_metadata}"; then
  echo "Export FSR ausente ou com ordinal incorreto: PhotorealismFsrGetApi @1" >&2
  exit 1
fi

for forbidden_export in \
  'CreateDXGIFactory' \
  'CreateDXGIFactory1' \
  'CreateDXGIFactory2' \
  'DirectInput8Create'; do
  if grep -Eq "[[:space:]]${forbidden_export}$" <<<"${fsr_metadata}"; then
    echo "O modulo auxiliar FSR nao pode atuar como proxy: ${forbidden_export}" >&2
    exit 1
  fi
done

if grep -qi 'snowymoon' \
    <<<"${dinput_metadata}${dxgi_metadata}${fsr_metadata}"; then
  echo "Dependencia inesperada de Snowymoon encontrada." >&2
  exit 1
fi

for core_fsr_message in \
  'Photorealism FSR/AA 0.6.0: modulo auxiliar ausente ou indisponivel' \
  'nucleo 0.11.0 continua normalmente' \
  'PhotorealismFsrGetApi' \
  'ABI incompativel ou incompleta' \
  'modulo auxiliar carregado' \
  'color_observer=%s automatic_selection=%s aa_easu_rcas=%s' \
  'dispositivo real do jogo entregue ao modulo' \
  'inicializacao do dispositivo falhou' \
  'dispositivo encerrado para reinicializacao segura'; do
  if ! strings "${dxgi_dll}" | grep -Fq "${core_fsr_message}"; then
    echo "Integracao FSR ausente no nucleo DXGI: ${core_fsr_message}" >&2
    exit 1
  fi
done

for module_fsr_message in \
  'Photorealism FSR/AA 0.6.0 inicializado' \
  'ABI=v1+v2+v3+v4' \
  'feature_level=%s' \
  '12_1' \
  'Adapter: name=' \
  'Capacidades D3D11' \
  'R16G16B16A16_FLOAT' \
  'R11G11B10_FLOAT' \
  'R8G8B8A8_UNORM' \
  'Observador color 0.6.0 pronto' \
  'diagnostic_queue=2 worker=%s' \
  'Janela color 0.6.0 %s' \
  'Relatorio color 0.6.0' \
  'async_job_drops=%llu' \
  'report_queue_drops=%llu' \
  'Color target #%llu' \
  'presentation-evidence' \
  'probable-scene' \
  'probable-mirror-reflection' \
  'probable-interface' \
  'consultas COM apenas em cache miss' \
  'AA/FSR source selecionado' \
  'fallback pass-through' \
  'selecao automatica estabilizada por composicao direta' \
  'AA Photorealism ativo antes da UI' \
  'temporal=history-clamp+screenspace-3x3' \
  'engine_motion_vectors=indisponiveis jitter=indisponivel' \
  'Telemetria GPU AA/FSR 0.6.0' \
  'TemporalAA_avg=%.3fms' \
  '0.4-stops' \
  'worker diagnostico drenado com seguranca'; do
  if ! strings "${fsr_dll}" | grep -Fq "${module_fsr_message}"; then
    echo "Diagnostico ausente no modulo FSR: ${module_fsr_message}" >&2
    exit 1
  fi
done

fsr_source="${project_dir}/src/fsr_module.cpp"
observe_frame_source="$(sed -n \
  '/void WINAPI observe_frame(/,/^}/p' "${fsr_source}")"
for forbidden_present_work in \
  'log_message(' \
  'std::sort' \
  'CreateFileW' \
  'WriteFile' \
  'CloseHandle'; do
  if grep -Fq "${forbidden_present_work}" <<<"${observe_frame_source}"; then
    echo "Trabalho bloqueante voltou ao observe_frame: ${forbidden_present_work}" >&2
    exit 1
  fi
done

automatic_selection_source="$(sed -n \
  '/HRESULT WINAPI update_automatic_selection(/,/^}/p' "${fsr_source}")"
for forbidden_hot_path_work in \
  'log_message(' \
  'new ' \
  'CreateFileW' \
  'WriteFile' \
  'CloseHandle' \
  'GetResource(' \
  'QueryInterface('; do
  if grep -Fq "${forbidden_hot_path_work}" <<<"${automatic_selection_source}"; then
    echo "Trabalho indevido voltou a selecao automatica: ${forbidden_hot_path_work}" >&2
    exit 1
  fi
done

shader_resource_source="$(sed -n \
  '/HRESULT WINAPI process_shader_resources(/,/^}/p' "${fsr_source}")"
for forbidden_hot_path_work in \
  'log_message(' \
  'new ' \
  'CreateFileW' \
  'WriteFile' \
  'CloseHandle'; do
  if grep -Fq "${forbidden_hot_path_work}" <<<"${shader_resource_source}"; then
    echo "Trabalho indevido voltou ao hook de composicao: ${forbidden_hot_path_work}" >&2
    exit 1
  fi
done

if [[ "$(grep -Fc '++g_uav_event_count;' "${fsr_source}")" -ne 1 ]]; then
  echo "uav_event_count deve ser incrementado exatamente uma vez por evento UAV." >&2
  exit 1
fi

for async_architecture_marker in \
  'kDiagnosticQueueCapacity = 2' \
  'DWORD WINAPI diagnostic_worker' \
  'WaitForMultipleObjects' \
  'write_color_report(&job.snapshot' \
  'SleepConditionVariableSRW' \
  'take_report_snapshot(&job.snapshot)'; do
  if ! grep -Fq "${async_architecture_marker}" "${fsr_source}"; then
    echo "Arquitetura assincrona FSR ausente: ${async_architecture_marker}" >&2
    exit 1
  fi
done

if ! strings "${dxgi_dll}" | grep -Fq \
    'Hooks Present/Present1/ResizeBuffers/OMSetRenderTargets*/ClearDepthStencilView instalados; PSSetShaderResources=%s'; then
  echo "Conjunto de hooks DXGI/D3D11 incompleto no nucleo." >&2
  exit 1
fi


for core_0110_message in \
  'Photorealism Plugin 0.11.0' \
  'gameoverlayrenderer64.dll' \
  'Steam overlay detectado e estabilizado antes dos hooks' \
  'fallback nao-Steam' \
  'Present1=%s' \
  'ativo-slot22' \
  'Auditoria Present 0.11.0' \
  'nosso-hook-externo' \
  'substituido-ou-encadeado' \
  'post-install-5000ms'; do
  if ! strings "${dxgi_dll}" | grep -Fq "${core_0110_message}"; then
    echo "Core overlay-aware 0.11.0 incompleto: ${core_0110_message}" >&2
    exit 1
  fi
done

if rg -n 'VK_F12|VK_PRIOR|PageUp' \
    "${project_dir}/src" >/dev/null; then
  echo "Atalho/captura manual proibido encontrado no core ou FSR." >&2
  exit 1
fi

if rg -n 'void Run\(void\* parameter, bool, std::uint64_t\).*Run\(parameter\)' \
    -U "${project_dir}/src/steam_screenshots.cpp" >/dev/null; then
  echo "Callback Steam call-result nao pode criar outra captura." >&2
  exit 1
fi

for screenshot_gate_marker in \
  'g_capture_cycle_active.compare_exchange_strong' \
  'request_is_duplicate(now, previous, false)' \
  'g_requests.store(1u' \
  'g_capture_cycle_active.store(false'; do
  if ! grep -Fq "${screenshot_gate_marker}" \
      "${project_dir}/src/steam_screenshots.cpp"; then
    echo "Token/deduplicacao de screenshot ausente: ${screenshot_gate_marker}" >&2
    exit 1
  fi
done

for steam_screenshot_message in \
  'SteamAPI_SteamScreenshots_v003' \
  'SteamAPI_ISteamScreenshots_HookScreenshots' \
  'SteamAPI_ISteamScreenshots_WriteScreenshot' \
  'SteamAPI_ISteamScreenshots_TriggerScreenshot' \
  'ScreenshotRequested_t=2302' \
  'exatamente um WriteScreenshot por toque' \
  'deduplicacao=750ms' \
  'write_handle=%u' \
  'accepted=%llu coalesced=%llu' \
  'result=ok' \
  'F12 nativo preservado'; do
  if ! strings "${dxgi_dll}" | grep -Fq "${steam_screenshot_message}"; then
    echo "Integracao Steam screenshot incompleta: ${steam_screenshot_message}" >&2
    exit 1
  fi
done

if ! rg -n 'process_frame\(swap_chain\);[[:space:]]*observe_postprocessed_frame\(swap_chain\);' \
    -U "${project_dir}/src/hook.cpp" >/dev/null; then
  echo "Fronteira pos-processada ausente depois de todos os passes visuais." >&2
  exit 1
fi

for home_gate_marker in \
  'set_fsr_processing_enabled(settings_.enabled)' \
  'fsr_processing_enabled()' \
  'PHOTOREALISM_FSR_RESET_PLUGIN_DISABLED'; do
  if ! rg -Fq "${home_gate_marker}" "${project_dir}/src"; then
    echo "Gate Home para AA/FSR ausente: ${home_gate_marker}" >&2
    exit 1
  fi
done

for observer_message in \
  'Descoberta depth 0.10.1 %s' \
  'early=%llums' \
  'iniciada por backbuffer' \
  'reiniciada via End' \
  'reiniciada por troca de dispositivo' \
  'reiniciada automaticamente por depth obsoleto' \
  'Descoberta depth 0.10.1 concluida' \
  'mode=%s' \
  'minimum_bindings=%llu' \
  'minimum_rate=%llu/s' \
  'scaled_area=%llu%%' \
  'Depth grupo' \
  'Depth recurso' \
  'Depth principal 0.10.1 selecionado automaticamente' \
  'descoberta continuara automaticamente' \
  'Recursos de copia depth criados' \
  'Depth sem atividade confirmada por %u frames' \
  'Depth obsoleto invalidado apos %u frames' \
  'Preview depth ativo' \
  'Depth linearization 0.6.4' \
  'reversed-z-enhanced' \
  'linear-distance' \
  'reconstructed-normals' \
  'ssao-visibility' \
  'Modulo SSAO 0.7.0' \
  'Modulo SSAO refinement 0.8.0' \
  'Modulo SSAO interior 0.9.0' \
  'Modulo temporal 0.10.0' \
  'SSAO 0.9.1 ativo' \
  'ssao_0.9.1' \
  'Resolve temporal 0.10.0 ativo' \
  'Historico temporal 0.10.0 inicializado' \
  'temporal_0.10.0'; do
  if ! strings "${dxgi_dll}" | grep -Fq "${observer_message}"; then
    echo "Observador de profundidade ausente: ${observer_message}" >&2
    exit 1
  fi
done

for telemetry_message in \
  'Telemetria GPU inicializada' \
  'Custo GPU do passe'; do
  if ! strings "${dxgi_dll}" | grep -Fq "${telemetry_message}"; then
    echo "Telemetria ausente no nucleo DXGI: ${telemetry_message}" >&2
    exit 1
  fi
done

cfg="${project_dir}/config/photorealism-plugin.cfg"
expected_cfg_sha256="b43b4a495d0252801c515b76e9c430848ed9bed0d5a398ed27db306b1e298cfe"
actual_cfg_sha256="$(sha256sum "${cfg}" | awk '{print $1}')"
if [[ "${actual_cfg_sha256}" != "${expected_cfg_sha256}" ]]; then
  echo "Configuracao consolidada foi alterada: ${actual_cfg_sha256}" >&2
  exit 1
fi
for section in \
  '[base.0.1.2]' \
  '[module.visual.0.2.0]' \
  '[module.rain_overcast.0.3.0]' \
  '[depth.0.6.4]' \
  '[module.ssao.0.7.0]' \
  '[module.ssao_refinement.0.8.0]' \
  '[module.ssao_interior.0.9.0]' \
  '[module.temporal.0.10.0]'; do
  grep -Fqx "${section}" "${cfg}"
done
grep -Fqx 'near_plane=0.1' "${cfg}"
grep -Fqx 'preview_distance=50.0' "${cfg}"
grep -Fqx 'vertical_fov=60.0' "${cfg}"
grep -Fqx 'radius=0.8' "${cfg}"
grep -Fqx 'intensity=0.28' "${cfg}"
grep -Fqx 'bias=0.04' "${cfg}"
grep -Fqx 'fade_start=30.0' "${cfg}"
grep -Fqx 'fade_end=70.0' "${cfg}"
grep -Fqx 'edge_rejection=1.5' "${cfg}"
grep -Fqx 'highlight_start=0.55' "${cfg}"
grep -Fqx 'highlight_end=0.95' "${cfg}"
grep -Fqx 'highlight_ao_floor=0.35' "${cfg}"
grep -Fqx 'near_start=2.0' "${cfg}"
grep -Fqx 'near_end=8.0' "${cfg}"
grep -Fqx 'radius=0.45' "${cfg}"
grep -Fqx 'intensity=0.20' "${cfg}"
grep -Fqx 'bias=0.05' "${cfg}"
grep -Fqx 'edge_rejection=1.75' "${cfg}"
grep -Fqx 'history_weight=0.65' "${cfg}"
grep -Fqx 'depth_rejection=0.02' "${cfg}"
grep -Fqx 'color_rejection=0.08' "${cfg}"

visual_shader="${project_dir}/shaders/photorealism.hlsl"
expected_visual_shader_sha256="d748d8b92645f847ce2a8431f97187b089e5aad34e349f31d6a4c87bfc2c5c95"
actual_visual_shader_sha256="$(sha256sum "${visual_shader}" | awk '{print $1}')"
if [[ "${actual_visual_shader_sha256}" != "${expected_visual_shader_sha256}" ]]; then
  echo "Shader visual aprovado foi alterado: ${actual_visual_shader_sha256}" >&2
  exit 1
fi

depth_preview_shader="${project_dir}/shaders/depth-preview.hlsl"
expected_depth_preview_shader_sha256="253a95e876cd8564ed4260ff461cf9deef0d25bcac2fbd1373d43fefbf9d4679"
actual_depth_preview_shader_sha256="$(sha256sum "${depth_preview_shader}" | awk '{print $1}')"
if [[ "${actual_depth_preview_shader_sha256}" != "${expected_depth_preview_shader_sha256}" ]]; then
  echo "Shader depth preview aprovado foi alterado: ${actual_depth_preview_shader_sha256}" >&2
  exit 1
fi

ssao_shader="${project_dir}/shaders/ssao.hlsl"
expected_ssao_shader_sha256="8416e7a2338c3a945eaf3b27dd4b9414e87558611bfbd1913b9171da9f2ce96c"
actual_ssao_shader_sha256="$(sha256sum "${ssao_shader}" | awk '{print $1}')"
if [[ "${actual_ssao_shader_sha256}" != "${expected_ssao_shader_sha256}" ]]; then
  echo "Shader SSAO aprovado foi alterado: ${actual_ssao_shader_sha256}" >&2
  exit 1
fi

temporal_shader="${project_dir}/shaders/temporal.hlsl"
expected_temporal_shader_sha256="437e2b68b222b2fa5d98c10e67c8f3c0054be48a77eb30ff36e29a445578b89e"
actual_temporal_shader_sha256="$(sha256sum "${temporal_shader}" | awk '{print $1}')"
if [[ "${actual_temporal_shader_sha256}" != "${expected_temporal_shader_sha256}" ]]; then
  echo "Shader temporal aprovado foi alterado: ${actual_temporal_shader_sha256}" >&2
  exit 1
fi

declare -A fsr_official_hashes=(
  ["third_party/fidelityfx-fsr/ffx_a.h"]="f60e2722fcd13989523b9164d776ab382b3692791767f3bf8bb19967f763f3fb"
  ["third_party/fidelityfx-fsr/ffx_fsr1.h"]="93c3922362ea7fc99cbcc698ca30c98de4f8c246d1fbb0b09e015ddef38ce3a5"
  ["third_party/fidelityfx-fsr/LICENSE.txt"]="db089274ce766da70f5b7d791029c3486f9f9e27c8c79c652689603d3192e802"
)
for relative in "${!fsr_official_hashes[@]}"; do
  actual="$(sha256sum "${project_dir}/${relative}" | awk '{print $1}')"
  if [[ "${actual}" != "${fsr_official_hashes[${relative}]}" ]]; then
    echo "Fonte oficial FidelityFX-FSR alterada: ${relative} ${actual}" >&2
    exit 1
  fi
done

depth_scoring_test="/tmp/photorealism-plugin-depth-scoring-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/depth_scoring_test.cpp" \
  -o "${depth_scoring_test}"
"${depth_scoring_test}"

fsr_color_scoring_test="/tmp/photorealism-fsr-color-scoring-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/fsr_color_scoring_test.cpp" \
  -o "${fsr_color_scoring_test}"
"${fsr_color_scoring_test}"

native_aa_config_test="/tmp/photorealism-native-aa-config-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/native_aa_config_test.cpp" \
  -o "${native_aa_config_test}"
"${native_aa_config_test}"

screenshot_request_gate_test="/tmp/photorealism-screenshot-request-gate-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/screenshot_request_gate_test.cpp" \
  -o "${screenshot_request_gate_test}"
"${screenshot_request_gate_test}"

effective_profile="$(awk -F= '
  /^\[/ { section=$0; next }
  /^[[:space:]]*(#|;|$)/ { next }
  {
    key=$1
    value=$2
    gsub(/^[[:space:]]+|[[:space:]]+$/, "", key)
    gsub(/^[[:space:]]+|[[:space:]]+$/, "", value)
    if (key == "enabled") next
    if (section == "[base.0.1.2]") {
      total[key]=value + 0
    } else if (section ~ /^\[module\./) {
      sub(/_delta$/, "", key)
      total[key]+=value + 0
    }
  }
  END {
    printf "%.1f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f", \
      total["temperature"], total["exposure"], total["contrast"], \
      total["saturation"], total["vibrance"], total["shadows"], \
      total["highlights"], total["blacks"], total["whites"], \
      total["local_contrast"], total["sharpness"], total["vignette"]
  }
' "${cfg}")"
expected_profile="6400.0 -0.030 1.070 0.970 0.050 0.100 -0.180 -0.060 0.080 0.240 0.200 0.030"
if [[ "${effective_profile}" != "${expected_profile}" ]]; then
  echo "Perfil cumulativo divergiu da 0.3.0 aprovada: ${effective_profile}" >&2
  exit 1
fi

if command -v glslangValidator >/dev/null 2>&1; then
  glslangValidator -D -S vert -e VSMain -V \
    "${project_dir}/shaders/photorealism.hlsl" \
    -o /tmp/photorealism-plugin-vs.spv >/dev/null
  glslangValidator -D -S frag -e PSMain -V \
    "${project_dir}/shaders/photorealism.hlsl" \
    -o /tmp/photorealism-plugin-ps.spv >/dev/null
  glslangValidator -D -S frag -e PSDepthPreview -V \
    "${project_dir}/shaders/depth-preview.hlsl" \
    -o /tmp/photorealism-plugin-depth-preview.spv >/dev/null
  glslangValidator -D -S frag -e PSSSAO -V \
    "${project_dir}/shaders/ssao.hlsl" \
    -o /tmp/photorealism-plugin-ssao.spv >/dev/null
  glslangValidator -D -S frag -e PSTemporal -V \
    "${project_dir}/shaders/temporal.hlsl" \
    -o /tmp/photorealism-plugin-temporal.spv >/dev/null
  glslangValidator -D -S comp -e CSEasu -V \
    "${project_dir}/shaders/fsr1.hlsl" \
    -o /tmp/photorealism-fsr-easu.spv >/dev/null
  glslangValidator -D -S comp -e CSTemporalAa -V \
    "${project_dir}/shaders/fsr1.hlsl" \
    -o /tmp/photorealism-aa-temporal.spv >/dev/null
  glslangValidator -D -S comp -e CSRcas -V \
    "${project_dir}/shaders/fsr1.hlsl" \
    -o /tmp/photorealism-fsr-rcas.spv >/dev/null
fi

native_aa_source="${project_dir}/src/native_aa_config.cpp"
for native_aa_marker in \
  'eurotrucks2.exe' \
  'amtrucks.exe' \
  'config.photorealism-native-aa.backup.cfg' \
  'MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH' \
  '"r_aa", "0"' \
  '"r_taa_tuning", "0"' \
  '"r_taa_luma_sharpen", "0.0"' \
  '"r_taa_modulated_drr_strength", "0.0"' \
  'native_fallback=disabled'; do
  if ! grep -Fq "${native_aa_marker}" "${native_aa_source}"; then
    echo "Gestao automatica AA nativo incompleta: ${native_aa_marker}" >&2
    exit 1
  fi
done

echo "Proxies, core 0.11.0 Steam screenshot unico, AA temporal Photorealism e FSR 0.6.0 automaticos, depth, SSAO, telemetria, perfil e shaders validados."
