#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dinput_dll="${project_dir}/build/dinput8.dll"
dxgi_dll="${project_dir}/build/dxgi.dll"

for dll in "${dinput_dll}" "${dxgi_dll}"; do
  if [[ ! -f "${dll}" ]]; then
    echo "DLL ausente: execute tools/build.sh primeiro: ${dll}" >&2
    exit 1
  fi
  grep -q 'PE32+.*DLL.*x86-64' <<<"$(file "${dll}")"
done

# As tabelas de literais abaixo sao consultadas dezenas de vezes por DLL. Um
# "strings ... | grep -Fq" encerra o grep no primeiro acerto, o strings recebe
# SIGPIPE e o pipeline sai 141: sob "set -o pipefail" isso reprova a checagem
# justamente quando ela passa. Extrair uma vez para arquivo elimina o SIGPIPE
# e evita reexecutar o strings a cada literal.
strings_cache="$(mktemp -d)"
trap 'rm -rf -- "${strings_cache}"' EXIT
dxgi_strings="${strings_cache}/dxgi.txt"
strings "${dxgi_dll}" >"${dxgi_strings}"

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


if grep -qi 'snowymoon' <<<"${dinput_metadata}${dxgi_metadata}"; then
  echo "Dependencia inesperada de Snowymoon encontrada." >&2
  exit 1
fi




if ! grep -Fq \
    'Hooks Present/Present1/ResizeBuffers/OMSetRenderTargets*/ClearDepthStencilView instalados; feature level=0x%X' \
    "${dxgi_strings}"; then
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
  if ! grep -Fq "${core_0110_message}" "${dxgi_strings}"; then
    echo "Core overlay-aware 0.11.0 incompleto: ${core_0110_message}" >&2
    exit 1
  fi
done

# F12 e a tecla de screenshot do Steam e continua sendo dele. Quem garante
# isso e o HookScreenshots(true) verificado mais abaixo, nao uma proibicao do
# literal VK_F12 no codigo -- a captura e delegada ao Steam por API.
#
# A unica invariante necessaria aqui e que o modulo de captura nao passe a
# disputar teclado com o Steam.
if rg -n 'VK_|GetAsyncKeyState' \
    "${project_dir}/src/steam" >/dev/null; then
  echo "A captura Steam nao pode consultar teclado." >&2
  exit 1
fi
if rg -n 'void Run\(void\* parameter, bool, std::uint64_t\).*Run\(parameter\)' \
    -U "${project_dir}/src/steam/capture_gate.cpp" >/dev/null; then
  echo "Callback Steam call-result nao pode criar outra captura." >&2
  exit 1
fi

for screenshot_gate_marker in \
  'g_capture_cycle_active.compare_exchange_strong' \
  'request_is_duplicate(now, previous, false)' \
  'g_requests.store(1u' \
  'g_capture_cycle_active.store(false'; do
  if ! grep -Fq "${screenshot_gate_marker}" \
      "${project_dir}/src/steam/capture_gate.cpp"; then
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
  if ! grep -Fq "${steam_screenshot_message}" "${dxgi_strings}"; then
    echo "Integracao Steam screenshot incompleta: ${steam_screenshot_message}" >&2
    exit 1
  fi
done

# Ordem do Present: upscale -> grade -> captura do Steam -> menu. Desde a
# 0.22.3 a reconstrucao roda no meio do quadro, antes da interface, e o passo
# de upscale do Present so fecha o quadro da captura -- mas continua antes do
# grade: ate a 0.22.1 o upscale vinha DEPOIS do grade e apagava a coloracao
# inteira. A captura do Steam continua sendo o ultimo passe visual, e o menu
# fica fora dela.
present_order="$(rg -N -U -o \
  'upscale_present_frame\(swap_chain\);[\s\S]*?draw_overlay_frame\(swap_chain\);' \
  "${project_dir}/src/hooks/swap_chain_hooks.cpp" | head -20)"
for present_step in \
  'upscale_present_frame(swap_chain);' \
  'process_frame(swap_chain);' \
  'observe_postprocessed_frame(swap_chain);' \
  'draw_overlay_frame(swap_chain);'; do
  if ! grep -Fq "${present_step}" <<<"${present_order}"; then
    echo "Passe ausente ou fora de ordem no Present: ${present_step}." >&2
    exit 1
  fi
done
upscale_line="$(grep -n 'upscale_present_frame' <<<"${present_order}" | head -1 | cut -d: -f1)"
grade_line="$(grep -n 'process_frame' <<<"${present_order}" | head -1 | cut -d: -f1)"
capture_line="$(grep -n 'observe_postprocessed_frame' <<<"${present_order}" | head -1 | cut -d: -f1)"
overlay_line="$(grep -n 'draw_overlay_frame' <<<"${present_order}" | head -1 | cut -d: -f1)"
if [[ "${upscale_line}" -gt "${grade_line}" ]]; then
  echo "O upscale voltou para depois do grade: com o FSR ativo, a reconstrucao \
sobrescreve o backbuffer e apaga a coloracao inteira do plugin." >&2
  exit 1
fi
if [[ "${grade_line}" -gt "${capture_line}" ]]; then
  echo "A captura do Steam passou para antes do grade: o screenshot sairia sem \
a coloracao do plugin." >&2
  exit 1
fi
if [[ "${capture_line}" -gt "${overlay_line}" ]]; then
  echo "A captura do Steam passou para depois do menu: o screenshot sairia com \
a janela do menu em cima." >&2
  exit 1
fi

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
  if ! grep -Fq "${observer_message}" "${dxgi_strings}"; then
    echo "Observador de profundidade ausente: ${observer_message}" >&2
    exit 1
  fi
done

# 0.18.0. A linha 'Cena 0.18.0:' e o produto inteiro deste modulo -- e dela,
# colhida jogando ETS2 em climas e horarios diferentes, que sai a calibracao da
# adaptacao por condicao. Sem ela o modulo gasta GPU e nao entrega nada.
#
# 'Perfil efetivo (cor)' entra aqui junto porque tint, rolloff e black_lift
# ficaram fora do log desde a 0.14.0, e nao havia como confirmar em runtime
# qual cor estava rodando.
for scene_observer_message in \
  'Observador de cena 0.18.0 ativo' \
  'Modulo observador de cena 0.18.0' \
  'Cena 0.18.0: ceu_R/B=%.3f mediana=%.1f faixa_p90-p10=%.1f' \
  'Perfil efetivo (cor): tint=%.3f' \
  'Balanco de branco 0.18.2: bruto=%.4f/%.4f/%.4f luma_bruta=%.6f' \
  'Detector de noite 0.19.0: tau=%.0fs' \
  'Condicao 0.19.0: sol=%.3f chuva=%.3f noite=%.3f'; do
  if ! grep -Fq "${scene_observer_message}" "${dxgi_strings}"; then
    echo "Observador de cena 0.18.0 incompleto: ${scene_observer_message}" >&2
    exit 1
  fi
done

for telemetry_message in \
  'Telemetria GPU inicializada' \
  'Custo GPU do passe'; do
  if ! grep -Fq "${telemetry_message}" "${dxgi_strings}"; then
    echo "Telemetria ausente no nucleo DXGI: ${telemetry_message}" >&2
    exit 1
  fi
done

cfg="${project_dir}/config/photorealism-plugin.cfg"
# 0.23.2: o cfg so tem as secoes que o perfil comanda. SSAO, resolve temporal,
# depth e o detector de noite rodam com valores internos, e as camadas de cor
# medidas sairam: o grade vem inteiro do perfil.
for section in \
  '[plugin]' \
  '[profile.photorealism.0.23.0]' \
  '[module.fsr.0.21.0]' \
  '[native_aa.0.12.2]' \
  '[module.bloom.0.17.0]' \
  '[module.scene_observer.0.18.0]'; do
  if ! grep -Fqx "${section}" "${cfg}"; then
    echo "Secao sumiu do cfg: ${section}" >&2
    exit 1
  fi
done
for retired_section in \
  '[base.0.1.2]' '[module.visual.0.2.0]' '[module.rain_overcast.0.3.0]' \
  '[module.user.0.20.0]' '[depth.0.6.4]' '[module.ssao.0.7.0]' \
  '[module.ssao_refinement.0.8.0]' '[module.ssao_interior.0.9.0]' \
  '[module.temporal.0.10.0]' '[module.condition_adaptation.0.19.0]'; do
  if grep -Fqx "${retired_section}" "${cfg}"; then
    echo "Secao aposentada na 0.23.2 voltou ao cfg: ${retired_section}. Ela nao \
e mais lida e parece mandar na imagem." >&2
    exit 1
  fi
done
for ssao_pin in 'settings->ssao_radius = 0.8f;' 'settings->ssao_intensity = 0.28f;' \
  'settings->ssao_interior_intensity = 0.20f;' 'settings.temporal_history_weight = 0.65f;'; do
  if ! grep -Fq "${ssao_pin}" "${project_dir}/src/config/defaults.cpp"; then
    echo "Valor interno aprovado do SSAO/temporal mudou: ${ssao_pin}" >&2
    exit 1
  fi
done

# 0.23.1: os cinco conjuntos de tom estao no cfg.
for profile_set in 1 2 3 4 5; do
  if ! grep -Eq "^tonemap_exposure_${profile_set}=" "${cfg}"; then
    echo "O conjunto de tom ${profile_set} sumiu do cfg: a iluminacao que usa \
esse conjunto cai nos neutros internos sem aviso." >&2
    exit 1
  fi
done

# Bloom 0.17.0. Os valores ainda sao PROVISORIOS -- derivacao fisica e nao
# medicao -- e por isso o que se guarda aqui e a FORMA, e nao o numero exato:
# o que nao pode acontecer e o modulo continuar ligado com um parametro que o
# torna inerte ou nocivo. Quando a 0.17.1 medir as referencias, os pinos exatos
# entram aqui, no molde dos da curva de tom acima.
if ! grep -Fqx '[module.bloom.0.17.0]' "${cfg}"; then
  echo "Secao do bloom 0.17.0 sumiu do cfg: o modulo cai para os defaults \
internos de config.cpp sem ninguem notar." >&2
  exit 1
fi
if grep -Eq '^intensity=0(\.0+)?$' "${cfg}"; then
  echo "intensity do bloom em zero: a piramide inteira roda todo frame e o \
resultado e multiplicado por zero. O log diria 'ativo' e a tela nao mudaria -- \
que e exatamente o modo de falha que custou tres versoes na serie 0.13.x." >&2
  exit 1
fi
# threshold=1.0 e o outro jeito de o modulo ficar ligado sem fazer nada: nada
# da cena passa do limiar. O clamp de config.cpp segura em 0.98, e esta guarda
# impede que o cfg peca isso em primeiro lugar.
if grep -Eq '^threshold=(1(\.0+)?|[2-9])' "${cfg}"; then
  echo "threshold do bloom em 1.0 ou acima: nenhum pixel da cena passa do \
limiar e o modulo fica ativo sem produzir nada." >&2
  exit 1
fi
# A ressalva de que o modulo contraria a medicao saiu do cfg na 0.23.1, com os
# demais comentarios, e fica na linha de log do modulo: as referencias foram
# medidas e nao tem bloom.
if ! grep -Fq 'licenca artistica; so o limiar e medido' "${dxgi_strings}"; then
  echo "A ressalva do bloom sumiu do log. Ela registra que as referencias \
foram medidas e nao tem bloom; sem ela alguem vai subir intensity achando que \
esta se aproximando do alvo, quando esta se afastando." >&2
  exit 1
fi
# O limiar e o unico dos quatro que a medicao sustenta: 0.85 em sRGB fica acima
# do p95 das cinco referencias (117 a 212). Abaixo disso o bloom passa a pegar
# o ceu de golden hour, e nao mais o disco do sol.
if ! grep -Fqx 'threshold=0.85' "${cfg}"; then
  echo "threshold do bloom fora de 0.85: e o unico parametro do modulo que a \
medicao das referencias sustenta, e abaixo dele a faixa 191-212 entra -- essa \
faixa e o ceu nas duas capturas de golden hour, e nao uma fonte de luz." >&2
  exit 1
fi

# Os hashes de depth-preview, ssao e temporal mudaram na 0.12.0: os tres
# perderam suas copias de linearize_reversed_depth/reconstruct_view_* para o
# header compartilhado depth_view_space.hlsli. A igualdade foi provada em
# bytecode com tools/shader_check.sh: ssao e temporal ficaram byte-identicos,
# e depth-preview mudou apenas por +1 max (guarda do rsqrt) e +2 lt/+2 movc
# (validade da normal, que antes nao existia).
#
# O header entra no pino porque agora e a unica fonte da matematica usada
# pelos tres shaders aprovados: alterar so ele mudaria os tres em silencio.
depth_view_space_header="${project_dir}/shaders/depth_view_space.hlsli"
expected_depth_view_space_sha256="6c11226174139a68117eaa038ade1365e606e639077629213a1e1439889109a8"
actual_depth_view_space_sha256="$(sha256sum "${depth_view_space_header}" | awk '{print $1}')"
if [[ "${actual_depth_view_space_sha256}" != "${expected_depth_view_space_sha256}" ]]; then
  echo "Header depth/view-space aprovado foi alterado: ${actual_depth_view_space_sha256}" >&2
  exit 1
fi


depth_preview_shader="${project_dir}/shaders/depth-preview.hlsl"
expected_depth_preview_shader_sha256="43b5abf25045f8d6951f670cc6d9768f74c11d60db9bd781a5c14811739239a3"
actual_depth_preview_shader_sha256="$(sha256sum "${depth_preview_shader}" | awk '{print $1}')"
if [[ "${actual_depth_preview_shader_sha256}" != "${expected_depth_preview_shader_sha256}" ]]; then
  echo "Shader depth preview aprovado foi alterado: ${actual_depth_preview_shader_sha256}" >&2
  exit 1
fi

ssao_shader="${project_dir}/shaders/ssao.hlsl"
expected_ssao_shader_sha256="8528e57b3dba89f3b905a5c0338d905e13f1514c78e766dd3e160af991134192"
actual_ssao_shader_sha256="$(sha256sum "${ssao_shader}" | awk '{print $1}')"
if [[ "${actual_ssao_shader_sha256}" != "${expected_ssao_shader_sha256}" ]]; then
  echo "Shader SSAO aprovado foi alterado: ${actual_ssao_shader_sha256}" >&2
  exit 1
fi

temporal_shader="${project_dir}/shaders/temporal.hlsl"
expected_temporal_shader_sha256="999b9766bd3f391a121e70421c204ba4a3a5a8dea14f4792f81d9d71d81b7181"
actual_temporal_shader_sha256="$(sha256sum "${temporal_shader}" | awk '{print $1}')"
if [[ "${actual_temporal_shader_sha256}" != "${expected_temporal_shader_sha256}" ]]; then
  echo "Shader temporal aprovado foi alterado: ${actual_temporal_shader_sha256}" >&2
  exit 1
fi


# A elegibilidade do depth de camera: forma e veto, tamanho nativo e
# suficiente, e o log diz por que cada candidato caiu.
for depth_eligibility_marker in \
  'kMinimumSceneAreaPercent' \
  'is_plausible_scene_shape' \
  'depth_candidate_rejection' \
  'forma-incompativel'; do
  if ! grep -Fq "${depth_eligibility_marker}" \
      "${project_dir}/src/depth_scoring.hpp"; then
    echo "Elegibilidade de depth incompleta: ${depth_eligibility_marker}" >&2
    exit 1
  fi
done
if ! grep -Fq 'elegibilidade=%s' \
    "${project_dir}/src/resource_observer/discovery_report.cpp"; then
  echo "Motivo de rejeicao ausente no log de recursos depth." >&2
  exit 1
fi

depth_scoring_test="/tmp/photorealism-plugin-depth-scoring-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/depth_scoring_test.cpp" \
  -o "${depth_scoring_test}"
"${depth_scoring_test}"

native_aa_config_test="/tmp/photorealism-native-aa-config-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/native_aa_config_test.cpp" \
  -o "${native_aa_config_test}"
"${native_aa_config_test}"

# A curva em si. Sem o lift o shader volta ao saturate() sem toe da 0.13.3.
for tone_marker in \
  'float3 apply_black_lift(float3 color, float3 lift)' \
  'float3 apply_highlight_rolloff(float3 color, float strength)'; do
  if ! grep -Fq "${tone_marker}" \
    "${project_dir}/shaders/photorealism.hlsl"; then
    echo "Curva de tom 0.14.0 incompleta em photorealism.hlsl: \
${tone_marker}" >&2
    exit 1
  fi
done
# 0.17.1: o contraste em potencia. A forma antiga -- reta com max(...,0) --
# mandava tudo abaixo de 0,01178 linear para o mesmo zero, e era ELA, e nao o
# black_lift, que destruia a sombra. Medido nas capturas da 0.17.0: 72 a 90%
# dos pixels escuros com os tres canais identicos e 12 a 13 niveis distintos
# abaixo de 12/255, contra 24 a 31 nas referencias. Se alguem reescrever a
# linha na forma afim, o platô volta em silencio.
if ! grep -Fq 'color = pivot * pow(max(color, 1e-6) / pivot, Contrast);' \
  "${project_dir}/shaders/photorealism.hlsl"; then
  echo "O contraste saiu da forma em potencia: na forma afim com clamp toda \
sombra abaixo de 0,01178 linear volta a colapsar num unico valor, e nenhum \
black_lift recupera isso." >&2
  exit 1
fi
# O sed tira os comentarios antes do grep: o proprio comentario da linha nova
# cita a forma antiga para explicar por que ela saiu, e sem isso a guarda
# acusaria a explicacao dela mesma.
if sed 's|//.*||' "${project_dir}/shaders/photorealism.hlsl" |
  grep -Fq '(color - pivot) * Contrast + pivot'; then
  echo "A reta de contraste da 0.17.0 voltou a photorealism.hlsl." >&2
  exit 1
fi

# A ordem importa: o lift e o piso da imagem FINAL, entao vem depois da
# vignette. Antes dela os cantos escureceriam abaixo do piso.
#
# O "|| true" das capturas abaixo nao e decoracao: sob "set -euo pipefail" um
# grep que nao acha nada derruba o script SEM IMPRIMIR NADA, e a guarda que
# existe justamente para explicar o problema morre calada. Com ele a variavel
# fica vazia e o teste de vazio adiante e quem fala.
visual_shader_source="${project_dir}/shaders/photorealism.hlsl"
vignette_line="$(grep -n 'color \*= lerp(1.0, smoothstep' \
  "${visual_shader_source}" | head -1 | cut -d: -f1 || true)"
lift_call_line="$(grep -n 'color = apply_black_lift(color, BlackLift);' \
  "${visual_shader_source}" | head -1 | cut -d: -f1 || true)"
if [[ -z "${vignette_line}" || -z "${lift_call_line}" ]] ||
  (( lift_call_line < vignette_line )); then
  echo "apply_black_lift saiu de depois da vignette: os cantos voltam a \
escurecer abaixo do piso de preto, e o piso deixa de ser piso." >&2
  exit 1
fi

# Bloom 0.17.0: a ordem da composicao dentro do PSMain, guardada por numero de
# linha como a do black_lift acima. As tres fronteiras importam e cada uma
# quebra de um jeito diferente:
#
#   depois do sharpening -- senao o realce morde a borda do glow e devolve um
#   halo duplo;
#   antes de apply_tonal_controls -- para o brilho receber exposicao,
#   temperatura e tint junto com a cena. Depois dele o flare do sol sairia
#   cinza sobre uma imagem quente;
#   e portanto antes de apply_highlight_rolloff, que comprime a soma. Somar
#   luz depois do ombro seria somar depois da unica coisa que impede o estouro.
bloom_call_line="$(grep -n 'center += bloom \* BloomIntensity;' \
  "${visual_shader_source}" | head -1 | cut -d: -f1 || true)"
sharpen_line="$(grep -n 'Sharpness + LocalContrast \* edge_mask' \
  "${visual_shader_source}" | head -1 | cut -d: -f1 || true)"
tonal_call_line="$(grep -n 'float3 color = apply_tonal_controls(center);' \
  "${visual_shader_source}" | head -1 | cut -d: -f1 || true)"
if [[ -z "${bloom_call_line}" || -z "${sharpen_line}" ||
  -z "${tonal_call_line}" ]]; then
  echo "A composicao do bloom sumiu do PSMain, ou os marcadores da ordem \
mudaram de forma. Sem ela a piramide roda todo frame e nada e somado." >&2
  exit 1
fi
if (( bloom_call_line < sharpen_line ||
  bloom_call_line > tonal_call_line )); then
  echo "A composicao do bloom saiu da faixa entre o sharpening e os controles \
tonais. Antes do realce ela ganha halo duplo; depois dos controles tonais o \
glow deixa de ser graduado com a cena e o flare quente sai cinza." >&2
  exit 1
fi
# O modulo tem que continuar desligavel de verdade: sem o ramo em BloomEnabled
# a saida com bloom desligado deixa de ser identica a 0.16.0.
if ! grep -Fq 'if (BloomEnabled > 0.5)' "${visual_shader_source}"; then
  echo "O ramo de BloomEnabled sumiu do PSMain: com o modulo desligado a \
imagem deixa de ser identica a 0.16.0 pixel a pixel." >&2
  exit 1
fi

# O hash fecha o shader visual DEPOIS das guardas de curva de tom, pela mesma
# razao que o do cfg: vindo antes, qualquer edicao do arquivo saia com
# "Shader visual aprovado foi alterado" e as guardas nomeadas nunca falavam.
# Uma guarda muda nao guarda coisa alguma.
visual_shader="${project_dir}/shaders/photorealism.hlsl"
expected_visual_shader_sha256="cc221815e206ffe96c50613d7b8fda72831f23bcb4ee61063bc205df279b4e6a"
actual_visual_shader_sha256="$(sha256sum "${visual_shader}" | awk '{print $1}')"
if [[ "${actual_visual_shader_sha256}" != "${expected_visual_shader_sha256}" ]]; then
  echo "Shader visual aprovado foi alterado: ${actual_visual_shader_sha256}" >&2
  exit 1
fi

# O alvo medido, dentro do teste. Sem esta guarda alguem apaga o bloco inteiro
# e a build segue verde sem nada provando o piso de preto.
if ! grep -Fq 'floor_code >= 6 && floor_code <= 12' \
  "${project_dir}/tests/tone_curve_test.cpp"; then
  echo "O teste parou de exigir o piso de preto medido nas referencias (p1 \
entre 6 e 12): e o unico numero que sozinho separa aquele visual do nosso." >&2
  exit 1
fi

# A propriedade central do limiar, dentro do teste: contribuicao exatamente
# zero abaixo do joelho. Sem ela o bloom vira veu cinza uniforme em vez de
# brilho em volta de fontes.
if ! grep -Fq 'assert(contribution(0.0, kThreshold, kKnee) == 0.0);' \
  "${project_dir}/tests/bloom_curve_test.cpp"; then
  echo "O teste do bloom parou de exigir contribuicao zero abaixo do joelho: \
e o que separa brilho em volta de fontes de uma nevoa sobre a cena inteira." >&2
  exit 1
fi

tone_curve_test="/tmp/photorealism-tone-curve-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/tone_curve_test.cpp" \
  -o "${tone_curve_test}"
"${tone_curve_test}"

bloom_curve_test="/tmp/photorealism-bloom-curve-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/bloom_curve_test.cpp" \
  -o "${bloom_curve_test}"
"${bloom_curve_test}"

screenshot_request_gate_test="/tmp/photorealism-screenshot-request-gate-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/screenshot_request_gate_test.cpp" \
  -o "${screenshot_request_gate_test}"
"${screenshot_request_gate_test}"

# O observador tem que medir o frame PRE-grade. Medir a saida fecharia uma
# realimentacao: a cor seria funcao das features e as features funcao da cor, e
# a imagem caminharia sozinha sem que nada no cfg mudasse. A chamada tem que
# ficar colada no CopyResource que enche scene_texture_.
if ! grep -Fq 'device_, context_, frame_resources_.scene_texture());' \
  "${project_dir}/src/postprocess/postprocessor.cpp"; then
  echo "O observador de cena parou de medir a textura pre-grade: medir a \
saida do grade fecha uma realimentacao entre a cor e as features." >&2
  exit 1
fi

# A guarda acima fixa a LINHA DA CHAMADA, e na 0.18.0 a chamada estava certa: o
# que estava errado era a tabela de formatos logo depois dela. O observador
# recusava DXGI 90 (B8G8R8A8_TYPELESS), que e exatamente o formato que
# ensure_frame_resources cria para a copia da cena, e passou a versao inteira
# desligado com validate.sh verde. Por isso a tabela saiu do .cpp para um
# cabecalho testavel, e por isso estas duas guardas existem.
if ! grep -Fq 'scene_formats::is_readable' \
  "${project_dir}/src/scene/sampler_resources.cpp"; then
  echo "O observador voltou a decidir formato dentro do .cpp, onde nenhum \
teste alcanca: foi assim que a 0.18.0 saiu desligada." >&2
  exit 1
fi
if ! grep -Fq 'assert(is_readable(kB8G8R8A8Typeless));' \
  "${project_dir}/tests/scene_formats_test.cpp"; then
  echo "O teste parou de exigir que o formato TYPELESS da copia da cena seja \
legivel: e o caminho principal, nao um caso exotico." >&2
  exit 1
fi

# A recusa tem que ser registrada UMA vez por assinatura de fonte. Na 0.18.0 o
# release() vinha antes da comparacao e zerava a assinatura, entao a recusa era
# reavaliada por frame: 663 mil linhas e 67 MB de log numa sessao.
if ! grep -Fq 'return !resources_failed_;' \
  "${project_dir}/src/scene/sampler_resources.cpp"; then
  echo "O observador parou de lembrar que ja falhou: sem isso a recusa volta a \
ser registrada uma vez por frame." >&2
  exit 1
fi

scene_features_test="/tmp/photorealism-scene-features-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/scene_features_test.cpp" \
  -o "${scene_features_test}"
"${scene_features_test}"

# 0.18.2. O balanco de branco tem que sair do shader dividido pela propria
# luminancia. Sem isto o vetor volta a carregar exposicao: no perfil aprovado
# sao +0,0411 EV contra um exposure=-0,030 no cfg, ou seja o sinal trocado. E
# a partir da 0.19.0, com tint se movendo com o clima, o erro se move junto e a
# imagem clareia sozinha ao ficar esverdeada.
if ! grep -Fq 'color * (balance / max(luminance(balance), 1e-4))' \
  "${project_dir}/shaders/photorealism.hlsl"; then
  echo "apply_temperature parou de normalizar o balanco em luminancia: o \
balanco volta a carregar exposicao escondida." >&2
  exit 1
fi

# white_balance_test.cpp ESPELHA a funcao do shader, porque ela vive em HLSL.
# Esta guarda e o que amarra as duas copias: o teste tem que continuar fixando
# o mesmo numero medido que o shader produz.
if ! grep -Fq 'assert(near(luminance(raw), 1.028920f, 1e-5f));' \
  "${project_dir}/tests/white_balance_test.cpp"; then
  echo "O teste parou de fixar a luminancia medida do balanco bruto (1,028920 \
no perfil aprovado): e o numero que estava escondido desde a 0.1.2." >&2
  exit 1
fi

white_balance_test="/tmp/photorealism-white-balance-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/white_balance_test.cpp" \
  -o "${white_balance_test}"
"${white_balance_test}"

# 0.19.0. A porta de jogo tem que ser SO ESTRUTURA. A porta anterior exigia
# saturacao > 0,09 e descartava 57 das 60 amostras de chuva medidas no ETS2 --
# saturacao baixa nao e quadro invalido, e o que encoberto e chuva parecem.
# Usar uma feature como criterio de validade remove do conjunto justamente os
# extremos que ela deveria medir.
if grep -Fq 'features.saturation <=' "${project_dir}/src/scene/condition_model.hpp"; then
  echo "A porta de jogo voltou a usar saturacao: isso descarta a chuva, que e \
a condicao que a adaptacao existe para detectar." >&2
  exit 1
fi

# A interpolacao tem que ser continua. Medido em jogo: limiar duro troca de
# classe 16 vezes por hora, e cada troca e um salto de cor.
if ! grep -Fq 'return t * t * (3.0f - 2.0f * t);' \
  "${project_dir}/src/scene/condition_model.hpp"; then
  echo "compute_condition_weights parou de interpolar continuamente: classe \
dura salta a cor ao virar a cabine." >&2
  exit 1
fi
if ! grep -Fq 'assert(biggest_step < 60.0f);' \
  "${project_dir}/tests/scene_conditions_test.cpp"; then
  echo "O teste parou de exigir que a saida nao de degrau ao varrer a \
saturacao." >&2
  exit 1
fi

# A adaptacao le o frame PRE-GRADE, pelo mesmo motivo do observador: alimentar
# com a saida fecharia a realimentacao entre a cor e as features.
if ! grep -Fq 'condition_.update(settings_, scene_observer_.latest());' \
  "${project_dir}/src/postprocess/postprocessor.cpp"; then
  echo "A adaptacao por condicao nao e mais chamada no ponto pre-grade." >&2
  exit 1
fi
# 0.23.2: a cor vem do perfil. O detector so pesa a exposicao noturna; se
# voltar a produzir temperatura ou matiz, briga com o conjunto de tom escolhido.
if ! grep -Fq 'input.temperature = settings_.temperature;' \
  "${project_dir}/src/postprocess/postprocessor.cpp" ||
   ! grep -Fq 'input.tint = settings_.tint;' \
  "${project_dir}/src/postprocess/postprocessor.cpp" ||
   grep -Fq 'blend_condition_grade' \
  "${project_dir}/src/postprocess/condition_adapter.cpp"; then
  echo "A adaptacao por condicao voltou a mandar na cor: ela briga com o conjunto \
de tom do perfil." >&2
  exit 1
fi

# 0.19.1. config.cpp passou a ter teste de verdade, e nao so grep.
#
# O grep confirma que um numero esta escrito no arquivo; nao confirma que ele
# chega a Settings. Foi essa lacuna que deixou a 0.18.2 mudar `exposure` no cfg
# e esquecer o default interno: os dois greps passavam e o fallback renderizava
# 0,041 EV mais escuro que o arquivo.
if ! grep -Fq 'assert(near(internal.exposure, shipped.exposure, 1e-5f));' \
  "${project_dir}/tests/config_load_test.cpp"; then
  echo "O teste parou de exigir que os defaults internos e o cfg entregue \
produzam o mesmo perfil: e assim que os dois divergem sem ninguem ver." >&2
  exit 1
fi

# A leitura e dirigida por tabela desde a 0.19.1. Se voltarem as cadeias de
# else-if, um parametro novo volta a precisar de cinco lugares certos.
if ! grep -Fq 'const SectionSpec kSections[]' "${project_dir}/src/config/section_table.cpp"; then
  echo "config.cpp deixou de ser dirigido por tabela de secoes." >&2
  exit 1
fi
if ! grep -Fq 'constexpr ToneLink kToneLinks[]' "${project_dir}/src/config/profile_state.cpp" ||
  ! grep -Fq 'const TonemapField kTonemapFields[]' "${project_dir}/src/config/photorealism_profile.cpp"; then
  echo "O perfil deixou de ter as tabelas unicas do conjunto de tom: leitura, \
menu e gravacao voltam a poder divergir." >&2
  exit 1
fi

if ! grep -Fq 'current != nullptr && current == previous.module' \
  "${project_dir}/src/overlay_watch.hpp"; then
  echo "advance_overlay_watch parou de exigir o MESMO endereco: um overlay \
recarregado passaria por estavel e os hooks entrariam cedo demais." >&2
  exit 1
fi

overlay_watch_test="/tmp/photorealism-overlay-watch-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/overlay_watch_test.cpp" \
  -o "${overlay_watch_test}"
"${overlay_watch_test}"

config_load_test="/tmp/photorealism-config-load-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  -I"${project_dir}/tests/support" -I"${project_dir}/src" \
  "${project_dir}/tests/config_load_test.cpp" \
  "${project_dir}/src/config/loader.cpp" \
  "${project_dir}/src/config/defaults.cpp" \
  "${project_dir}/src/config/section_table.cpp" \
  "${project_dir}/src/config/photorealism_profile.cpp" \
  "${project_dir}/src/config/profile_fields.cpp" \
  "${project_dir}/src/config/profile_logging.cpp" \
  "${project_dir}/src/config/profile_reference.cpp" \
  "${project_dir}/src/config/profile_state.cpp" \
  "${project_dir}/src/config/limits.cpp" \
  "${project_dir}/src/config/logging.cpp" \
  -o "${config_load_test}"
PHOTOREALISM_PROJECT_DIR="${project_dir}" "${config_load_test}"

# Perfil de tom 0.23.0: chaves por conjunto, escalas convertidas, e a composicao
# troca as camadas medidas pelo perfil sem tocar na camada do usuario.
photorealism_profile_test="/tmp/photorealism-profile-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  -I"${project_dir}/tests/support" -I"${project_dir}/src" \
  "${project_dir}/tests/photorealism_profile_test.cpp" \
  "${project_dir}/src/config/loader.cpp" \
  "${project_dir}/src/config/defaults.cpp" \
  "${project_dir}/src/config/section_table.cpp" \
  "${project_dir}/src/config/photorealism_profile.cpp" \
  "${project_dir}/src/config/profile_fields.cpp" \
  "${project_dir}/src/config/profile_logging.cpp" \
  "${project_dir}/src/config/profile_reference.cpp" \
  "${project_dir}/src/config/profile_state.cpp" \
  "${project_dir}/src/config/limits.cpp" \
  "${project_dir}/src/config/logging.cpp" \
  -o "${photorealism_profile_test}"
PHOTOREALISM_PROJECT_DIR="${project_dir}" "${photorealism_profile_test}"

# 0.19.13. A suavizacao nao pode depender da taxa de quadros. GetTickCount64
# tem resolucao de ~15,6 ms: acima de 64 fps muitos quadros chegam com
# elapsed_seconds == 0, e um alpha que valha 1.0 nesse caso faz um salto
# completo a cada um deles -- a suavizacao deixa de existir sem nada acusar.
if ! grep -Fq 'if (elapsed_seconds <= 0.0f) {' \
  "${project_dir}/src/scene/condition_smoother.hpp"; then
  echo "O suavizador voltou a integrar sem tempo decorrido: acima de 64 fps \
isso salta para a amostra do momento e a suavizacao some." >&2
  exit 1
fi
if ! grep -Fq 'assert(smoother.median() > 40.0f);' \
  "${project_dir}/tests/scene_conditions_test.cpp"; then
  echo "O teste parou de exigir que a suavizacao independa do fps." >&2
  exit 1
fi

scene_conditions_test="/tmp/photorealism-scene-conditions-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/scene_conditions_test.cpp" \
  -o "${scene_conditions_test}"
"${scene_conditions_test}"

scene_formats_test="/tmp/photorealism-scene-formats-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/scene_formats_test.cpp" \
  -o "${scene_formats_test}"
"${scene_formats_test}"



# O hash fecha o cfg depois das guardas por chave, e nao antes.
#
# Ate a 0.13.3 ele vinha primeiro, e por isso nenhuma das guardas nomeadas
# acima chegava a falar: qualquer edicao do arquivo batia no hash e saia com
# "Configuracao consolidada foi alterada", que nao diz o que quebrou nem por
# que importa. Uma guarda que explica uma regressao sutil so serve se for ela
# a falar. Nesta ordem o hash continua pegando tudo que as guardas nao
# cobrem, e so isso.
expected_cfg_sha256="7aecea3db59ca851a6d35bb3da2597dd5a879c65843ac611098b1d477f9912bd"
actual_cfg_sha256="$(sha256sum "${cfg}" | awk '{print $1}')"
if [[ "${actual_cfg_sha256}" != "${expected_cfg_sha256}" ]]; then
  echo "Configuracao consolidada foi alterada: ${actual_cfg_sha256}" >&2
  exit 1
fi
# As camadas somadas sao nomeadas uma a uma de proposito. Ate a 0.21.0 este awk
# somava qualquer chave de qualquer secao [module.*], e passava porque nenhuma
# chave de modulo tinha nome de campo de cor. O `sharpness` do FSR foi o
# primeiro a colidir: 0.35 do RCAS entrava na nitidez do grade e o perfil
# acusava 0.550 onde o medido e 0.200.
# 0.14.0: blacks cumulativo saiu de -0.060 para 0.000 -- somado, empurrava os
# pretos para baixo contra o alvo, e o piso passou a ser black_lift. Os tres
# ultimos campos sao a curva de tom, e entraram no perfil justamente para que
# uma mudanca neles nao passe por uma camada de delta sem ser vista.
# exposure passou de -0.030 para 0.011 na 0.18.2 e a IMAGEM NAO MUDOU. O
# balanco de branco carregava +0,0411 EV escondidos e agora e normalizado em
# luminancia; os +0,0411 EV vieram para a exposicao base, onde da para ler.
# Exposicao e balanco sao multiplicacao em linear e comutam.

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
fi

# A escrita do config.cfg do jogo precisa continuar atomica: temporario mais
# MoveFileEx. Um write direto deixa o config.cfg do usuario truncado se o jogo
# fechar no meio. A implementacao desceu para src/config/file_io.cpp na 0.20.0,
# quando o menu passou a precisar do mesmo IO -- a guarda segue o efeito.
if ! grep -Fq 'MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH' \
  "${project_dir}/src/config/file_io.cpp"; then
  echo "A escrita de configuracao deixou de ser atomica: sem temporario mais \
MoveFileEx, um fechamento no meio trunca o config.cfg do usuario." >&2
  exit 1
fi
if ! grep -rFq 'config_io::write_atomic' "${project_dir}/src/native_aa"; then
  echo "O AA nativo parou de usar a escrita atomica de configuracao." >&2
  exit 1
fi

native_aa_source="${project_dir}/src/native_aa"
for native_aa_marker in \
  'eurotrucks2.exe' \
  'amtrucks.exe' \
  'config.photorealism-native-aa.backup.cfg' \
  'kNativeAaSection = "native_aa.0.12.2"' \
  'read_native_aa_policy' \
  'policy.manage' \
  'policy.desired[index].c_str()' \
  'plugin_config_value' \
  'nenhuma alteracao no ' \
  'politica=photorealism-plugin.cfg'; do
  if ! grep -rFq "${native_aa_marker}" "${native_aa_source}"; then
    echo "Gestao automatica AA nativo incompleta: ${native_aa_marker}" >&2
    exit 1
  fi
done

# O numero de versao e carimbo de chegada, nao de agenda. Duas vezes o ROADMAP
# prometeu uma entrega num numero que um pacote ja tinha consumido -- a fase de
# composicao do RTGI presa na 0.12.2, que virou a politica de AA nativo, e a
# recalibracao do SSAO presa na 0.13.1, que virou o conserto do Page Down.
# Promessa apontando para numero ja entregue e documentacao que mente.
changelog="${project_dir}/CHANGELOG.md"
roadmap="${project_dir}/ROADMAP.md"
shipped_versions="$(grep -E '^## ' "${changelog}" \
  | grep -oE '^## (Pacote |Core )?[0-9]+\.[0-9]+\.[0-9]+' \
  | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | sort -Vu)"
promised_versions="$(grep -E '^- \*\*[0-9]+\.[0-9]+\.[0-9]+' "${roadmap}" \
  | grep -v '(entregue)' \
  | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | sort -Vu)"
version_collisions="$(printf '%s\n' "${promised_versions}" \
  | grep -Fxf <(printf '%s\n' "${shipped_versions}") || true)"
if [[ -n "${version_collisions}" ]]; then
  echo "ROADMAP promete versao ja entregue no CHANGELOG: \
${version_collisions//$'\n'/ }" >&2
  echo "Renumere as fases nao entregues do ROADMAP." >&2
  exit 1
fi
if ! grep -Fq 'carimbo de' "${roadmap}"; then
  echo "ROADMAP sem a regra de renumeracao das fases nao entregues." >&2
  exit 1
fi

# O RTGI saiu inteiro na 0.16.0, pela mesma razao medida na 0.14.0: nenhuma das
# cinco referencias que definem o alvo visual mostra efeito que exija tracado
# de raios -- a luz de preenchimento da cabine e uniforme e sem sangramento de
# cor. O padrao e so 'rtgi', sem termo generico: na 0.15.0 um 'easu' no padrao
# casou com "measure" e derrubou grade_report.py.
rtgi_leftovers="$(grep -rli 'rtgi' \
  "${project_dir}/src" "${project_dir}/shaders" "${project_dir}/tests" \
  "${project_dir}/tools" "${project_dir}/config" 2>/dev/null |
  grep -v '/tools/validate\.sh$' || true)"
if [[ -n "${rtgi_leftovers}" ]]; then
  echo "RTGI reapareceu no codigo: ele foi removido na 0.16.0 porque o alvo \
visual nao precisa dele, e voltar custa 1.252 linhas e um passe por frame." >&2
  echo "${rtgi_leftovers}" >&2
  exit 1
fi
# As duas teclas que ele usava tambem nao podem voltar sozinhas.
for retired_key in 'VK_PRIOR' 'VK_NEXT'; do
  if grep -Fq "${retired_key}" "${project_dir}/src/postprocess/postprocessor.cpp"; then
    echo "Tecla aposentada na 0.16.0 voltou: ${retired_key}. Page Up e Page \
Down existiam so para o RTGI." >&2
    exit 1
  fi
done

# O FSR voltou na 0.21.0, a pedido do usuario e com plano escrito por ele. A
# guarda de nao-retorno da 0.15.0 saiu, mas a LICAO dela fica: o modulo antigo
# eram 5.833 linhas cuja propria telemetria dizia `replacement=0 dispatch=0` --
# nunca substituiu um draw nem despachou um passe, e mesmo assim build e
# validate ficavam verdes. Trocar o veto a palavra por uma guarda ao efeito.
#
# O que se exige agora: o modulo tem que CONTAR o que fez e por o numero no
# log. Sem esses contadores, um FSR que nao roda volta a passar despercebido.
for fsr_effect_marker in \
  'fsr.replacement=' \
  'fsr.dispatch=' \
  'record_replacement' \
  'record_dispatch'; do
  if ! grep -rFq "${fsr_effect_marker}" "${project_dir}/src/fsr"; then
    echo "O FSR perdeu ${fsr_effect_marker}. A versao removida na 0.15.0 \
passou por build e validate dizendo replacement=0 dispatch=0 -- sem contador no \
log, um modulo que nao faz nada e indistinguivel de um que funciona." >&2
    exit 1
  fi
done

# Os oito hooks per-draw continuam proibidos. A 0.15.0 os removeu porque o ETS2
# emite milhares de draws por frame e cada um pagava indirecao, load atomico e
# branch para alimentar o FSR. Os hooks do swap chain que o upscale usa sao
# outra coisa: rodam por resize e por frame, nao por draw.
# Os oito hooks tambem nao podem voltar: o de PSSetShaderResources e os de
# Draw*/RSSet* nao servem a mais nada agora que o depth vem de
# OMSetRenderTargets.
for retired_hook in \
  'hooked_ps_set_shader_resources' \
  'hooked_rs_set_state' \
  'hooked_rs_set_viewports' \
  'hooked_rs_set_scissor_rects' \
  'hooked_draw_indexed' \
  'hooked_draw_instanced'; do
  if grep -Fq "${retired_hook}" "${project_dir}/src/hooks/swap_chain_hooks.cpp"; then
    echo "Hook aposentado na 0.15.0 voltou a hook.cpp: ${retired_hook}. Ele \
roda em toda chamada de desenho do jogo." >&2
    exit 1
  fi
done

# Todo .cpp sob src/ precisa estar em build.sh. A 0.19.4 mostrou o custo de nao
# ter esta guarda: os modulos extraidos foram compilados fora do binario, o
# original continuou vivo no namespace anonimo, e build e validate ficaram
# verdes sobre codigo morto.
# Todo teste em tests/ precisa estar neste arquivo. A 0.20.4 mostrou o custo de
# nao ter esta guarda: uma edicao removeu um bloco e levou junto o registro do
# menu_roundtrip_test, que ficou no repo sem nunca mais rodar.
missing_from_validate=""
while IFS= read -r test_file; do
  test_name="$(basename "${test_file}")"
  if ! grep -Fq "${test_name}" "${project_dir}/tools/validate.sh"; then
    missing_from_validate="${missing_from_validate}${test_name}"$'\n'
  fi
done < <(find "${project_dir}/tests" -maxdepth 1 -name '*_test.cpp' | sort)
if [[ -n "${missing_from_validate}" ]]; then
  echo "Teste em tests/ que este arquivo nao roda: ele fica no repo dando \
impressao de cobertura e nunca executa." >&2
  echo "${missing_from_validate}" >&2
  exit 1
fi

missing_from_build=""
while IFS= read -r source_file; do
  relative="${source_file#${project_dir}/}"
  if ! grep -Fq "${relative}" "${project_dir}/tools/build.sh"; then
    missing_from_build="${missing_from_build}${relative}"$'\n'
  fi
done < <(find "${project_dir}/src" -name '*.cpp' | sort)
if [[ -n "${missing_from_build}" ]]; then
  echo "Arquivo .cpp fora de tools/build.sh: ele compila nos testes mas nao \
entra no DLL, e o binario roda sem ele sem nenhum aviso." >&2
  echo "${missing_from_build}" >&2
  exit 1
fi

# O menu desenha DEPOIS da captura do Steam. Se subir para antes, ele passa a
# aparecer dentro dos screenshots do jogo.
for present_hook in 'hooked_present' 'hooked_present1'; do
  capture_line="$(grep -n "observe_postprocessed_frame" \
    "${project_dir}/src/hooks/swap_chain_hooks.cpp" | head -1 | cut -d: -f1)"
  overlay_line="$(grep -n "draw_overlay_frame" \
    "${project_dir}/src/hooks/swap_chain_hooks.cpp" | head -1 | cut -d: -f1)"
  if [[ -z "${overlay_line}" || -z "${capture_line}" ]]; then
    echo "O menu ou a captura do Steam sumiu de ${present_hook}." >&2
    exit 1
  fi
  if [[ "${overlay_line}" -lt "${capture_line}" ]]; then
    echo "draw_overlay_frame subiu para antes de observe_postprocessed_frame: \
o menu passa a ser gravado dentro dos screenshots do Steam." >&2
    exit 1
  fi
done

# O menu roda fora do process_frame, entao precisa levantar a mesma flag. Sem
# ela is_processing_frame() fica falso e o proprio OMSetRenderTargets do menu
# entra no observador de depth como candidato.
if ! grep -A 4 'void draw_overlay_frame' \
  "${project_dir}/src/postprocess/postprocessor.cpp" | grep -Fq 'ProcessorScope'; then
  echo "draw_overlay_frame parou de usar ProcessorScope: o menu passa a \
desenhar com is_processing_frame() falso e envenena a descoberta de depth." >&2
  exit 1
fi

# O menu liga uma SRV no vertex shader. Sem salvar esse slot, o proximo desenho
# do jogo herda o buffer de vertices do overlay.
for vertex_slot_call in 'VSGetShaderResources' 'VSSetShaderResources'; do
  if ! grep -Fq "${vertex_slot_call}" \
    "${project_dir}/src/postprocess/device_state.cpp"; then
    echo "SavedState parou de cobrir a SRV do vertex shader \
(${vertex_slot_call}): o menu vaza o proprio buffer de vertices para o jogo." >&2
    exit 1
  fi
done


# Aplicar um ajuste do menu nao pode passar por reload_configuration: ela
# recompila sete entry points de shader dentro do Present e reinicia a
# descoberta de depth.
if grep -rFq 'reload_configuration' "${project_dir}/src/overlay"; then
  echo "O menu chamou reload_configuration: mover um slider passaria a \
recompilar sete shaders dentro do Present." >&2
  exit 1
fi

# A fonte embutida e assada por GDI, que so entra no DLL com -lgdi32.
if ! grep -Fq -- '-lgdi32' "${project_dir}/tools/build.sh"; then
  echo "dxgi.dll perdeu -lgdi32: o atlas de fonte do menu nao linka." >&2
  exit 1
fi

# O vertex do menu atravessa a fronteira CPU/GPU sem input layout: o VS le o
# StructuredBuffer por SV_VertexID. Nada no compilador liga os dois lados, e um
# campo a mais de um lado so aparece como menu embaralhado na tela. O
# static_assert prende o lado C++; esta contagem prende o lado HLSL.
overlay_vertex_floats="$(awk '/^struct OverlayVertex/,/^};/' \
  "${project_dir}/shaders/overlay.hlsl" | grep -oE '^ +float[234]?' |
  sed 's/[^0-9]//g' | awk '{ s += ($1 == "" ? 1 : $1) } END { print s+0 }')"
if [[ "${overlay_vertex_floats}" -ne 16 ]]; then
  echo "OverlayVertex no shader tem ${overlay_vertex_floats} floats e o Vertex \
em C++ tem 16: o menu passa a ler os campos deslocados." >&2
  exit 1
fi
if ! grep -Fq 'static_assert(sizeof(Vertex) == 64)' \
  "${project_dir}/src/overlay/draw_list.hpp"; then
  echo "O Vertex do overlay perdeu o static_assert de 64 bytes, que e o unico \
lado C++ do acordo de layout com o StructuredBuffer do shader." >&2
  exit 1
fi

# O menu so vale se cada controle que ele mostra puder de fato ser gravado. O
# teste percorre a tabela de paginas e exige que todo binding resolva: os 17 de
# cor para uma chave _delta da camada do usuario, o resto para a secao e chave
# que o proprio leitor usa.
# A conta inteira do menu de uma ponta a outra: o que o slider mostrava tem que
# voltar identico depois de gravar e reler, e as tres camadas medidas e os
# comentarios do arquivo tem que sair intactos.
# O dinput8.dll pergunta ao dxgi.dll se o menu esta aberto, por nome, em tempo
# de execucao. Um erro de digitacao de um lado nao quebra build nem link: o
# GetProcAddress volta nulo, a porteira fica muda e o mouse continua girando a
# camera sem nenhum aviso. Os nomes tem que bater com src/dxgi.def.
while IFS= read -r exported_name; do
  if ! grep -Fq "${exported_name}" "${project_dir}/src/dxgi.def"; then
    echo "O dinput8.dll procura o export ${exported_name}, que nao existe em \
src/dxgi.def. GetProcAddress voltaria nulo em silencio e a porteira de \
DirectInput nunca silenciaria o mouse." >&2
    exit 1
  fi
done < <(grep -oE '"photorealism_[a-z_]+"' "${project_dir}/src/dinput/menu_gate.cpp" |
  tr -d '"' | sort -u)

# A porteira so existe se o DirectInput8Create a instalar.
if ! grep -Fq 'install_input_gate' "${project_dir}/src/proxy.cpp"; then
  echo "DirectInput8Create parou de instalar a porteira de entrada: o menu \
volta a dividir o mouse com o jogo." >&2
  exit 1
fi

# Os tres slots de vtable sao a unica coisa que liga a porteira ao DirectInput,
# e errar um deles chama a funcao errada com os argumentos de outra.
for gate_slot in \
  'kCreateDeviceSlot = 3' \
  'kGetDeviceStateSlot = 9' \
  'kGetDeviceDataSlot = 10'; do
  if ! grep -Fq "${gate_slot}" "${project_dir}/src/dinput/input_gate.cpp"; then
    echo "Slot de vtable do DirectInput mudou: ${gate_slot}. Um slot errado \
chama outro metodo com os argumentos deste." >&2
    exit 1
  fi
done

# O ponteiro do menu anda por DELTA, nao por posicao. O jogo recentraliza o
# cursor do sistema a cada quadro para o DirectInput funcionar em modo
# relativo, entao ler GetCursorPos devolve o centro da tela sempre.
#
# E o menu NAO pode mexer no registro de entrada bruta do processo: no Wine e
# de la que o proprio DirectInput se alimenta, e remover o registro fez o
# buffer do mouse chegar zerado, deixando a seta parada no centro. Foi o
# defeito da 0.20.3.
if grep -rFq 'RIDEV_REMOVE' "${project_dir}/src"; then
  echo "Alguem voltou a remover o registro de entrada bruta do processo. No Wine isso mata a fonte que alimenta o DirectInput, e a seta do menu para de andar." >&2
  exit 1
fi
# A conta inteira do menu de uma ponta a outra: o que o slider mostrava tem que
# voltar identico depois de gravar e reler, e as tres camadas medidas e os
# comentarios do arquivo tem que sair intactos.
menu_roundtrip_test="/tmp/photorealism-menu-roundtrip-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  -I"${project_dir}/tests/support" -I"${project_dir}/src" \
  "${project_dir}/tests/menu_roundtrip_test.cpp" \
  "${project_dir}/src/config/loader.cpp" \
  "${project_dir}/src/config/defaults.cpp" \
  "${project_dir}/src/config/section_table.cpp" \
  "${project_dir}/src/config/photorealism_profile.cpp" \
  "${project_dir}/src/config/profile_fields.cpp" \
  "${project_dir}/src/config/profile_logging.cpp" \
  "${project_dir}/src/config/profile_reference.cpp" \
  "${project_dir}/src/config/profile_state.cpp" \
  "${project_dir}/src/config/limits.cpp" \
  "${project_dir}/src/config/logging.cpp" \
  -o "${menu_roundtrip_test}"
"${menu_roundtrip_test}"

# Gravar so o que mudou EM RELACAO AO DISCO, e nao em relacao ao valor de
# referencia. Comparando com a referencia, apertar o "R" de um slider e salvar
# nao escrevia nada: o delta antigo continuava no cfg e voltava no proximo
# carregamento, com o menu tendo dito que reiniciou.
# A escala interna e a unica parte do FSR que roda fora da GPU, e a que decide
# se o upscale vale a pena. 1920x1080 a 0.6667 tem que dar 1280x720 exatos --
# a milestone do plano -- e todo lado tem que cair num multiplo do grupo 8x8.
# A reconstrucao so pode rodar quando existir um quadro interno para ler. O
# invariante substitui o da 0.21.2, que guardava a substituicao de backbuffer
# -- mecanismo aposentado na 0.21.5 em favor do r_scale do proprio ETS2.
upscaler_reconstruct_body="$(awk '/^bool Upscaler::reconstruct/,/^}/' \
  "${project_dir}/src/fsr/upscaler.cpp")"
if ! grep -Fq 'internal_.acquire()' <<<"${upscaler_reconstruct_body}"; then
  echo "A reconstrucao parou de exigir o quadro interno capturado NESTE quadro: \
reconstruiria uma copia velha por cima do quadro do jogo." >&2
  exit 1
fi
if ! grep -Fq 'record_replacement()' <<<"${upscaler_reconstruct_body}"; then
  echo "A reconstrucao parou de contar os quadros substituidos: fsr.replacement \
volta a ser zero por construcao e o aviso de sem efeito dispara a toa, que foi \
o defeito da 0.22.1." >&2
  exit 1
fi

# A descoberta do quadro interno reusa o hook de OMSetRenderTargets que o depth
# ja usa. Nenhum dos oito hooks per-draw removidos na 0.15.0 volta por causa
# disso -- se alguem precisar de um, e sinal de que o desenho esta errado.
for color_discovery_site in \
  'observe_color_targets' \
  'enable_color_capture' \
  'acquire_captured_frame' \
  'end_color_frame' \
  'reset_color_discovery'; do
  if ! grep -rFq "${color_discovery_site}" "${project_dir}/src"; then
    echo "A descoberta do quadro interno perdeu ${color_discovery_site}: sem \
ela o EASU nao tem de onde ler e o upscale nunca roda." >&2
    exit 1
  fi
done

# A PISCADA DA 0.22.1. O ETS2 reaproveita texturas de um pool: a mesma textura
# fisica guarda a cena num quadro e uma mascara de bordas no seguinte. Escolher
# "a textura mais ligada" alternava cena e bordas a cada quadro. O que e estavel
# e a POSICAO no quadro: o ultimo alvo na resolucao interna antes de o jogo
# passar para a de saida. A regra mora em FrameTransition, e o teste simula o
# pool trocando as texturas.
if ! grep -Fq 'g_transition.observe' \
  "${project_dir}/src/resource_observer/color_observation.cpp"; then
  echo "A captura deixou de ser decidida pela posicao no quadro: volta a \
escolher textura pela identidade, e o pool do jogo faz a imagem piscar." >&2
  exit 1
fi
for context_hook in 'hooked_set_render_targets(' 'hooked_set_render_targets_and_uavs('; do
  hook_body="$(awk -v name="${context_hook}" 'index($0, name) {inside=1} inside {print} inside && /^}/ {exit}' \
    "${project_dir}/src/hooks/context_hooks.cpp")"
  original_line="$(grep -n 'original(' <<<"${hook_body}" | head -1 | cut -d: -f1)"
  color_line="$(grep -n 'observe_color_targets(' <<<"${hook_body}" | head -1 | cut -d: -f1)"
  if [[ -z "${original_line}" || -z "${color_line}" || "${color_line}" -lt "${original_line}" ]]; then
    echo "${context_hook%(} observa a cor antes de repassar o bind ao jogo: a \
copia tem que acontecer depois que o alvo interno deixou de estar ligado." >&2
    exit 1
  fi
done
observe_body="$(awk '/^bool observe_color_targets/,/^}/' \
  "${project_dir}/src/resource_observer/color_observation.cpp")"
active_line="$(grep -n 'g_color_capture_active.load' <<<"${observe_body}" | head -1 | cut -d: -f1)"
describe_line="$(grep -n 'describe_view' <<<"${observe_body}" | head -1 | cut -d: -f1)"
if [[ -z "${active_line}" || -z "${describe_line}" || "${describe_line}" -lt "${active_line}" ]]; then
  echo "observe_color_targets consulta a textura antes de ver se a captura esta \
ativa: com o FSR desligado, todo OMSetRenderTargets do jogo volta a pagar \
chamadas COM a toa." >&2
  exit 1
fi
if ! awk '/^void upscale_present_frame/,/^}/' \
  "${project_dir}/src/postprocess/postprocessor.cpp" | grep -Fq 'end_color_frame();'; then
  echo "O Present parou de fechar o quadro da captura: uma copia de quadro \
anterior passaria a ser reconstruida por cima do atual." >&2
  exit 1
fi
if grep -Fq 'is_supported_format' \
  "${project_dir}/src/resource_observer/color_observation.cpp" \
  "${project_dir}/src/resource_observer/color_capture.cpp" \
  "${project_dir}/src/resource_observer/frame_transition.hpp"; then
  echo "A captura de cor voltou a usar a tabela so de formatos tipados: um \
alvo TYPELESS, que e o caso normal, some da busca em silencio." >&2
  exit 1
fi

# Gamma: o RTV do backbuffer e sRGB e o hardware codifica na escrita. O RCAS le
# valores ja codificados; escrever sem decodificar antes codifica duas vezes e a
# imagem sai lavada. Foi assim ate a 0.22.1.
if ! grep -Fq 'DecodeBeforeWrite' "${project_dir}/shaders/fsr_rcas.hlsl"; then
  echo "O RCAS parou de decodificar antes de escrever num RTV sRGB: a imagem \
reconstruida sai com gamma dupla." >&2
  exit 1
fi
if ! grep -Fq 'is_srgb(' "${project_dir}/src/fsr/output_target.cpp"; then
  echo "A reconstrucao parou de ler o estado sRGB do RTV que o proprio jogo \
ligou: a gamma sai dupla ou crua conforme o formato da view." >&2
  exit 1
fi
upscaler_reconstruct_run="$(awk '/^bool Upscaler::reconstruct/,/^}/' \
  "${project_dir}/src/fsr/upscaler.cpp")"
if ! grep -Fq 'finish.output_is_srgb_view = target.srgb_view;' <<<"${upscaler_reconstruct_run}"; then
  echo "O upscale parou de receber o estado sRGB do RTV de saida." >&2
  exit 1
fi

# A PISCADA DA 0.22.2. O RCAS desenhava no backbuffer sem ligar estado proprio
# e herdava o que a interface do jogo deixou: blend, depth e, sobretudo, o
# scissor do ultimo elemento desenhado. A reconstrucao cobria so aquele
# retangulo -- a mensagem de dormir -- e so nos quadros em que ela era a
# ultima. Desligar o FSR pelo menu parava a piscada.
rcas_body="$(awk '/^void UpscalePipeline::draw_rcas/,/^}/' \
  "${project_dir}/src/fsr/upscale_pipeline.cpp")"
if ! grep -Fq 'states_.bind(context);' <<<"${rcas_body}"; then
  echo "draw_rcas desenha sem ligar os proprios estados: herda o scissor e o \
blend da interface do jogo e a reconstrucao volta a piscar." >&2
  exit 1
fi
for fsr_state_rule in \
  'OMSetBlendState' \
  'OMSetDepthStencilState' \
  'RSSetState' \
  'ScissorEnable = FALSE' \
  'CullMode = D3D11_CULL_NONE' \
  'BlendEnable = FALSE' \
  'DepthEnable = FALSE'; do
  if ! grep -Fq "${fsr_state_rule}" "${project_dir}/src/fsr/fsr_states.cpp"; then
    echo "Os estados do FSR perderam ${fsr_state_rule}: o desenho volta a \
depender do que o jogo deixou ligado." >&2
    exit 1
  fi
done

# A INTERFACE. O jogo desenha o HUD no backbuffer depois do proprio upscale.
# Reconstruir no Present cobre o HUD; por isso a reconstrucao roda dentro do
# OMSetRenderTargets, na segunda passagem do jogo pelo backbuffer, e a
# interface continua sendo desenhada por cima.
upscale_frame_body="$(awk '/^    void upscale_frame\(/,/^    }/' \
  "${project_dir}/src/postprocess/postprocessor.cpp")"
if grep -Eq 'OMSetRenderTargets|reconstruct\(' <<<"${upscale_frame_body}"; then
  echo "O Present voltou a escrever no backbuffer: a reconstrucao cobre a \
interface que o jogo ja desenhou." >&2
  exit 1
fi
for context_hook in 'hooked_set_render_targets(' 'hooked_set_render_targets_and_uavs('; do
  hook_body="$(awk -v name="${context_hook}" 'index($0, name) {inside=1} inside {print} inside && /^}/ {exit}' \
    "${project_dir}/src/hooks/context_hooks.cpp")"
  observe_line="$({ grep -n 'observe_color_targets(' <<<"${hook_body}" || true; } | head -1 | cut -d: -f1)"
  rebuild_line="$({ grep -n 'reconstruct_game_frame(' <<<"${hook_body}" || true; } | head -1 | cut -d: -f1)"
  if [[ -z "${observe_line}" || -z "${rebuild_line}" || "${rebuild_line}" -lt "${observe_line}" ]]; then
    echo "${context_hook%(} parou de reconstruir no bind que a observacao \
decide." >&2
    exit 1
  fi
done
if ! grep -Fq 'reconstruct && uav_count == 0' \
  "${project_dir}/src/hooks/context_hooks.cpp"; then
  echo "A reconstrucao passou a rodar num bind que tambem liga UAVs: restaurar \
o estado com OMSetRenderTargets desligaria as UAVs do jogo." >&2
  exit 1
fi
rebuild_body="$(awk '/^void reconstruct_game_frame/,/^}/' \
  "${project_dir}/src/postprocess/postprocessor.cpp")"
if ! grep -Fq 'ProcessorScope scope;' <<<"${rebuild_body}"; then
  echo "reconstruct_game_frame roda sem ProcessorScope: os binds do proprio \
FSR entram na observacao como se fossem do jogo." >&2
  exit 1
fi
rebuild_state_body="$(awk '/^    void reconstruct_in_frame\(/,/^    }/' \
  "${project_dir}/src/postprocess/postprocessor.cpp")"
for rebuild_state_call in 'capture_state(' 'restore_state('; do
  if ! grep -Fq "${rebuild_state_call}" <<<"${rebuild_state_body}"; then
    echo "A reconstrucao no meio do quadro perdeu ${rebuild_state_call}: o jogo \
desenharia a interface com o estado do FSR." >&2
    exit 1
  fi
done
if ! grep -Fq 'texture == output_' \
  "${project_dir}/src/resource_observer/frame_transition.hpp"; then
  echo "A saida voltou a ser reconhecida pelo tamanho: um alvo qualquer de \
1920x1080 no meio do quadro receberia a reconstrucao." >&2
  exit 1
fi

# O rastreio so arma com o mapa carregado.
if grep -Fq 'kTraceFallbackFrames' \
  "${project_dir}/src/resource_observer/color_observation.cpp"; then
  echo "O rastreio voltou a armar por contagem de quadros quaisquer: cai na \
tela de carregamento, como nas duas sessoes da 0.22.2." >&2
  exit 1
fi
if ! grep -Fq 'g_arming.end_frame(g_transition.binds())' \
  "${project_dir}/src/resource_observer/color_observation.cpp"; then
  echo "O rastreio deixou de esperar os quadros seguidos de cena." >&2
  exit 1
fi

# Contador que ninguem chama e diagnostico falso. A 0.22.1 imprimia
# fsr.replacement=0 e aquisicoes_do_jogo=0 enquanto o EASU rodava 59 vezes por
# segundo, porque os chamadores tinham saido junto com o hook de backbuffer.
while IFS= read -r recorder; do
  if ! grep -rFq "${recorder}(" "${project_dir}/src" --exclude=fsr_telemetry.cpp; then
    echo "Telemetry::${recorder} nao tem chamador: o contador fica zero por \
construcao e o log mente." >&2
    exit 1
  fi
done < <(grep -oE 'void Telemetry::record_[a-z_]+' \
  "${project_dir}/src/fsr/fsr_telemetry.cpp" | sed 's/void Telemetry:://' | sort -u)
if ! awk '/^void Telemetry::report/,/^}/' "${project_dir}/src/fsr/fsr_telemetry.cpp" |
  grep -Fq 'window_reason_ = nullptr;'; then
  echo "O relatorio do FSR parou de limpar o motivo a cada janela: o motivo de \
um descarte antigo continua aparecendo depois que o problema passou." >&2
  exit 1
fi

# A escala interna e do ETS2, escrita no config dele no bootstrap. E a caixa
# "Frame em menor resolucao" do plano: quem reduz e o Prism3D, nao o plugin
# enganando o jogo sobre o tamanho do backbuffer.
# Arrastar um slider no menu chama configure() a cada movimento do mouse. A
# 0.21.6 registrava uma linha por chamada e um arraste do slider de escala
# rendeu 76 linhas de log. So a troca de ligado/desligado merece linha; o valor
# da escala esta na tela e sai no log do bootstrap quando e aplicado.
configure_body="$(awk '/^void Upscaler::configure/,/^}/' \
  "${project_dir}/src/fsr/upscaler.cpp")"
if grep -Fq 'was_scale' <<<"${configure_body}"; then
  echo "configure() voltou a registrar mudanca de escala: arrastar o slider no \
menu enche o log com uma linha por quadro." >&2
  exit 1
fi

# O r_scale e o Scaling das opcoes graficas do ETS2 -- config do usuario. O
# plugin pode toma-lo emprestado quando o FSR esta ligado, mas tem que guardar
# o valor anterior e devolver quando for desligado. A 0.21.5 escrevia 1.0 em
# toda abertura mesmo com o FSR desligado, e apagava um Scaling de 83% que o
# usuario tinha escolhido.
game_scale_body="$(awk '/^void apply_render_scale_to_game/,/^}/' \
  "${project_dir}/src/fsr/game_scale.cpp")"
for scale_contract in 'fsr_is_enabled' 'remember_scale' 'forget_scale'; do
  if ! grep -Fq "${scale_contract}" <<<"${game_scale_body}"; then
    echo "A escala perdeu ${scale_contract}: o plugin volta a escrever o \
Scaling do usuario sem guardar nem devolver o valor que era dele." >&2
    exit 1
  fi
done
if ! grep -Fq 'if (!enabled && axes_empty(saved))' <<<"${game_scale_body}"; then
  echo "O plugin voltou a mexer no r_scale com o FSR desligado e nada \
emprestado: isso apaga o Scaling que o usuario escolheu." >&2
  exit 1
fi

# O r_scale do ETS2 tem dois eixos, e os presets do menu grafico usam valores
# diferentes em cada um: o de 75% grava x=0.75 e y=1. Ate a 0.22.7 o plugin
# guardava so o x e devolvia esse valor aos dois, e 75% voltava como 56%.
for scale_axis_call in 'read_game_scale(game_config)' \
  'ScaleAxes wanted = saved;' \
  'write_game_scale(&game_config, wanted)' \
  'remember_scale(target.config_path, before)'; do
  if ! grep -Fq "${scale_axis_call}" <<<"${game_scale_body}"; then
    echo "A escala deixou de guardar e devolver os dois eixos separados: falta \
${scale_axis_call}" >&2
    exit 1
  fi
done
if grep -Fq 'kScaleKeys' "${project_dir}/src/fsr/game_scale.cpp"; then
  echo "A escala voltou a percorrer os eixos com um valor unico." >&2
  exit 1
fi

# Troca de dispositivo D3D11 com o FSR ligado: a copia do quadro interno era
# reaproveitada por tamanho e formato, e o proximo quadro copiava uma textura
# do dispositivo novo para dentro de um recurso do antigo. Reproduzido na GPU:
# com a 0.22.7 a copia continuava no dispositivo antigo.
adopt_body="$(awk '/^    bool adopt_device\(/,/^    }/' \
  "${project_dir}/src/postprocess/postprocessor.cpp")"
for device_swap_call in 'fsr::upscaler().release();' 'reset_color_discovery();'; do
  if ! grep -Fq "${device_swap_call}" <<<"${adopt_body}"; then
    echo "A troca de dispositivo deixou de reiniciar o FSR: falta ${device_swap_call}" >&2
    exit 1
  fi
done
if ! grep -Fq 'owner_ == context' "${project_dir}/src/resource_observer/color_capture.cpp"; then
  echo "A copia do quadro interno voltou a ignorar a qual contexto pertence." >&2
  exit 1
fi

# Perfil de tom. O multiplicador do SSAO entra so na hora de desenhar, e a
# exposicao noturna do conjunto so existe se o peso de noite chegar ao shader.
if ! grep -Fq 'settings.ssao_intensity * settings.ssao_intensity_scale' \
  "${project_dir}/src/postprocess/frame_constants.cpp" ||
  ! grep -Fq 'settings.ssao_interior_intensity * settings.ssao_intensity_scale' \
  "${project_dir}/src/postprocess/frame_constants.cpp"; then
  echo "O multiplicador de SSAO do perfil deixou de ser aplicado no desenho." >&2
  exit 1
fi
if ! grep -Fq 'night_weight_ = weights.night;' \
  "${project_dir}/src/postprocess/condition_adapter.cpp" ||
  ! grep -Fq 'input.night_weight = condition_.night_weight();' \
  "${project_dir}/src/postprocess/postprocessor.cpp" ||
  ! grep -Fq 'night_adjusted_exposure(settings, input.night_weight)' \
  "${project_dir}/src/postprocess/frame_constants.cpp"; then
  echo "A exposicao noturna do perfil deixou de chegar ao shader." >&2
  exit 1
fi
# 0.23.2: editar a cor no menu muda o conjunto da iluminacao escolhida, e trocar
# a iluminacao carrega o conjunto dela. Sem as duas pontas, o Salvar grava um
# conjunto e a tela mostra outro.
if ! grep -Fq 'store_active_tonemap(&settings_);' \
  "${project_dir}/src/postprocess/postprocessor.cpp" ||
  ! awk '/^void finish_settings/,/^}/' "${project_dir}/src/config/loader.cpp" |
  grep -Fq 'apply_active_tonemap(settings);'; then
  echo "A cor do menu deixou de ir e voltar do conjunto da iluminacao ativa." >&2
  exit 1
fi
for profile_message in \
  'Perfil photorealism 0.23.0 sem efeito (%s):%s.' \
  'exposicao_noturna=%+.2fEV' \
  'Perfil photorealism 0.23.0: iluminacao %c (conjunto de tom %u)'; do
  if ! grep -Fq "${profile_message}" "${dxgi_strings}"; then
    echo "Linha do perfil sumiu do log: ${profile_message}" >&2
    exit 1
  fi
done
third_party_hits="$(grep -rli 'snowy' "${project_dir}/src" "${project_dir}/shaders" \
  "${project_dir}/config" "${project_dir}/tests" "${project_dir}/tools/build.sh" \
  "${project_dir}/tools/package.sh" 2>/dev/null || true)"
if [[ -n "${third_party_hits}" ]]; then
  echo "Nome de plugin de terceiros no codigo do photorealism-plugin: \
${third_party_hits//$'\n'/ }" >&2
  exit 1
fi

# O cfg que vai no pacote e o default interno do codigo tem que concordar sobre
# o FSR. Divergindo, quem apaga o cfg ganha um comportamento diferente de quem
# nao apaga, e ninguem percebe ate ir atras.
cfg_fsr_enabled="$(awk '/^\[module\.fsr\./,/^$/' \
  "${project_dir}/config/photorealism-plugin.cfg" | grep -E '^enabled=' |
  cut -d= -f2)"
code_fsr_enabled="$(grep -oE 'settings\.fsr_enabled = (true|false)' \
  "${project_dir}/src/config/defaults.cpp" | awk '{print $3}' | tr -d ';')"
if [[ "${cfg_fsr_enabled}" != "${code_fsr_enabled}" ]]; then
  echo "O cfg empacotado diz FSR=${cfg_fsr_enabled} e o default interno diz \
${code_fsr_enabled}. Quem apagar o cfg ganha outro comportamento." >&2
  exit 1
fi

# A nitidez do RCAS saiu de 0.35 para 0.60 na 0.22.5, escolhida pelo usuario no
# jogo com o EASU ja igual ao da AMD. O cfg e o default do codigo tem que dizer o
# mesmo, senao o "R" do menu devolve um valor e o pacote traz outro.
cfg_fsr_sharpness="$(awk '/^\[module\.fsr\./,/^$/' \
  "${project_dir}/config/photorealism-plugin.cfg" | grep -E '^sharpness=' |
  cut -d= -f2)"
code_fsr_sharpness="$(grep -oE 'settings\.fsr_sharpness = [0-9.]+f' \
  "${project_dir}/src/config/defaults.cpp" | awk '{print $3}' | tr -d 'f')"
if [[ "${cfg_fsr_sharpness}" != "0.60" || "${code_fsr_sharpness}" != "0.60" ]]; then
  echo "A nitidez padrao do RCAS diverge: cfg=${cfg_fsr_sharpness} \
codigo=${code_fsr_sharpness}, e o padrao escolhido no jogo e 0.60." >&2
  exit 1
fi

# 0.22.6: escala padrao 0.8660 (75% dos pixels), escolhida pelo usuario. 0.22.7:
# granulacao LFGA 0.30, escolhida no jogo. Mesma regra: cfg e codigo dizem o mesmo.
for fsr_default in 'render_scale=0.8660=fsr_render_scale = 0.8660f' 'grain=0.30=fsr_grain = 0.30f'; do
  cfg_line="${fsr_default%%=fsr_*}"
  code_line="settings.fsr_${fsr_default##*=fsr_}"
  if ! awk '/^\[module\.fsr\./,/^$/' "${project_dir}/config/photorealism-plugin.cfg" |
    grep -Fxq "${cfg_line}" ||
    ! grep -Fq "${code_line};" "${project_dir}/src/config/defaults.cpp"; then
    echo "Padrao do FSR diverge entre cfg e codigo: esperado ${cfg_line} e ${code_line}." >&2
    exit 1
  fi
done

for game_scale_key in '"r_scale_x"' '"r_scale_y"'; do
  if ! grep -Fq "${game_scale_key}" "${project_dir}/src/fsr/scale_axes.hpp"; then
    echo "A escala interna do jogo perdeu ${game_scale_key}: sem escrever \
r_scale no config do ETS2, o jogo desenha em resolucao cheia e nao ha ganho." >&2
    exit 1
  fi
done
if ! grep -Fq 'apply_render_scale_to_game(g_proxy_module)' \
  "${project_dir}/src/proxy.cpp"; then
  echo "O bootstrap parou de aplicar a escala interna: o modulo existe e nunca \
e chamado, que e como o FSR removido na 0.15.0 viveu." >&2
  exit 1
fi

# E o backbuffer nao pode voltar a ser substituido: com o r_scale ligado, as
# duas reducoes se empilham e o jogo desenha a 480p achando que e 720p.
if grep -rFq 'kGetBufferSlot' "${project_dir}/src"; then
  echo "A substituicao do backbuffer voltou: junto com o r_scale do ETS2 ela \
empilha duas reducoes no mesmo quadro." >&2
  exit 1
fi

# Mexer num ajuste no menu tem que chegar a quem usa esse ajuste. Na 0.21.0 o
# menu avisava o host so para os campos do observador de cena, entao ligar o
# FSR pelo menu gravava no cfg e nao chegava ao modulo -- o log ficava sem uma
# linha sequer e parecia que nada tinha acontecido.
if ! grep -Fq 'settings_changed' "${project_dir}/src/overlay/overlay.cpp"; then
  echo "O menu parou de avisar o host quando um ajuste muda: quem le a config \
fora do caminho por quadro nunca fica sabendo." >&2
  exit 1
fi
menu_change_body="$(awk '/^void Menu::apply_change/,/^}/' \
  "${project_dir}/src/overlay/overlay.cpp")"
if grep -Fq 'binding_touches_observer' <<<"${menu_change_body}"; then
  echo "apply_change voltou a filtrar o aviso por grupo de ajuste: quem nao \
estiver na lista volta a nao ser avisado, que foi o defeito do FSR na 0.21.0." >&2
  exit 1
fi
if ! grep -Fq 'fsr::upscaler().configure' \
  "${project_dir}/src/postprocess/postprocessor.cpp"; then
  echo "O host parou de repassar a config ao FSR." >&2
  exit 1
fi

# Quem precisa do backbuffer para gravar ou desenhar em cima -- a captura do
# Steam e o menu -- pede por um caminho unico, para nao ficar espalhado.
for real_target_user in \
  "${project_dir}/src/steam/capture_pipeline.cpp" \
  "${project_dir}/src/postprocess/postprocessor.cpp"; do
  if ! grep -Fq 'present_back_buffer' "${real_target_user}"; then
    echo "$(basename "${real_target_user}") parou de pedir o backbuffer pelo \
caminho comum." >&2
    exit 1
  fi
done

# O EASU e compute, que e o ponto do plano: usar as Compute Units. Um EASU que
# virasse pixel shader continuaria funcionando e perderia a razao de existir.
if ! grep -Fq 'numthreads(8, 8, 1)' "${project_dir}/shaders/fsr_easu.hlsl"; then
  echo "O EASU deixou de ser compute com grupo 8x8." >&2
  exit 1
fi
if ! grep -Fq 'CreateComputeShader' "${project_dir}/src/fsr/fsr_shaders.cpp"; then
  echo "O EASU deixou de ser criado como compute shader." >&2
  exit 1
fi
easu_body="$(awk '/^void UpscalePipeline::dispatch_easu/,/^}/' \
  "${project_dir}/src/fsr/upscale_pipeline.cpp")"
if ! grep -E '^[[:space:]]*context->Dispatch\(' <<<"${easu_body}" >/dev/null; then
  echo "dispatch_easu parou de despachar compute: fsr.dispatch ficaria em zero, \
que foi como o modulo removido na 0.15.0 viveu ate ser apagado." >&2
  exit 1
fi
if ! grep -Fq 'record_dispatch' <<<"${easu_body}"; then
  echo "dispatch_easu despacha e nao conta: sem o contador, um upscale que nao \
roda fica indistinguivel de um que roda." >&2
  exit 1
fi

# O EASU e o da AMD, transcrito de ffx_fsr1.h (fsrEasuSetFloat, fsrEasuTapFloat,
# ffxFsrEasuFloat). Ate a 0.22.3 ele somava a borda com max() em vez de soma e
# detectava borda so pelo verde: a adaptacao chegava a 25% da AMD e as
# diagonais saiam em degrau. Na 0.22.4 a saida foi comparada na GPU contra o
# ffx_fsr1.h original, compilado pelo mesmo compilador do Proton: zero bits
# diferentes. As guardas prendem os pontos que tinham divergido.
easu_shader="${project_dir}/shaders/fsr_easu.hlsl"
for easu_rule in \
  'edge += edge_x * weight;' \
  'edge += edge_y * weight;' \
  'return color.b * float(0.5) + (color.r * float(0.5) + color.g);' \
  'asfloat(uint(0x7ef07ebb) - asuint(value))' \
  'asfloat(uint(0x5f347d74) - (asuint(value) >> uint(1)))' \
  'dir.x = zro ? float(1.0) : dir.x;' \
  'float lob = float(0.5) + float((1.0 / 4.0 - 0.04) - 0.5) * len;'; do
  if ! grep -Fq "${easu_rule}" "${easu_shader}"; then
    echo "O EASU deixou de ser o da AMD: falta ${easu_rule}" >&2
    exit 1
  fi
done
if grep -Eq 'max\(horizontal_len|return color\.g;|saturate\(shaped|max\(accumulated_weight' \
  "${easu_shader}"; then
  echo "O EASU voltou a ter a borda por max(), a luma so do verde ou os \
limitadores que a AMD nao tem: a adaptacao a borda cai para 25%." >&2
  exit 1
fi
easu_tap_order="$(grep -oE 'easu_tap\(ac, aw, .*, ([a-z])\);' "${easu_shader}" |
  sed -E 's/.*, ([a-z])\);/\1/' | tr -d '\n')"
if [[ "${easu_tap_order}" != "bcijfeklhgon" ]]; then
  echo "A ordem de acumulacao dos 12 taps do EASU mudou (${easu_tap_order}): a \
AMD acumula b c i j f e k l h g o n, e a soma em ponto flutuante depende da \
ordem." >&2
  exit 1
fi
easu_dispatch_body="$(awk '/^void UpscalePipeline::dispatch_easu/,/^}/' \
  "${project_dir}/src/fsr/upscale_pipeline.cpp")"
if ! grep -Fq 'populate_easu_constants(' <<<"${easu_dispatch_body}"; then
  echo "dispatch_easu deixou de montar as constantes como a AMD." >&2
  exit 1
fi
if grep -Eq 'constants\.con0\[[23]\] = 0\.5f \*' "${project_dir}/src/fsr/easu_constants.hpp"; then
  echo "O deslocamento de con0 voltou a ser uma expressao unica: o clang funde \
multiplicacao e subtracao ao dobrar constantes e o bit final muda." >&2
  exit 1
fi
if ! grep -Fq 'Advanced Micro Devices' \
  "${project_dir}/references/licenses/FidelityFX-FSR2-LICENSE.txt"; then
  echo "Falta a licenca MIT da AMD: o EASU e transcrito do FidelityFX." >&2
  exit 1
fi
if ! grep -Fq 'FidelityFX-FSR2-LICENSE.txt' "${project_dir}/tools/package.sh"; then
  echo "O pacote saiu sem a licenca da AMD que o EASU exige." >&2
  exit 1
fi

# O RCAS e o da AMD desde a 0.22.6, transcrito de FsrRcasF em ffx_fsr1.h e
# comparado na GPU contra o original em quatro nitidezes: zero bits diferentes.
# Ate a 0.22.5 ele aplicava a reducao de ruido que a AMD deixa desligada
# (FSR_RCAS_DENOISE), dividia sem a aproximacao media e lia a nitidez como
# multiplicador linear. A 0.22.4 afirmou que ele ja batia com a AMD; nao batia.
rcas_shader="${project_dir}/shaders/fsr_rcas.hlsl"
for rcas_rule in \
  'asfloat(uint(0x7ef19fff) - asuint(value))' \
  '#define FSR_RCAS_LIMIT (0.25 - (1.0 / 16.0))' \
  'float hit_min_r = mn4r * rcp(float(4.0) * mx4r);' \
  'float hit_max_r = (peak_c.x - mx4r) * rcp(float(4.0) * mn4r + peak_c.y);' \
  'float rcp_l = approximate_reciprocal_medium(float(4.0) * lobe + float(1.0));' \
  'pix.r = (lobe * br + lobe * dr + lobe * hr + lobe * fr + er) * rcp_l;'; do
  if ! grep -Fq "${rcas_rule}" "${rcas_shader}"; then
    echo "O RCAS deixou de ser o da AMD: falta ${rcas_rule}" >&2
    exit 1
  fi
done
if grep -Eq 'noise|lobe \*= nz' "${rcas_shader}"; then
  echo "O RCAS voltou a reduzir nitidez em ruido: a AMD deixa FSR_RCAS_DENOISE \
desligado e recomenda granulacao depois do RCAS no lugar dele." >&2
  exit 1
fi
if ! grep -Fq 'return (-2.0f * sharpness) + 2.0f;' \
  "${project_dir}/src/fsr/rcas_constants.hpp" ||
  ! grep -Fq 'return std::exp2(-stops);' "${project_dir}/src/fsr/rcas_constants.hpp"; then
  echo "A nitidez do RCAS deixou de seguir FsrRcasCon com o remapeamento da API \
do FSR 2 (stops = 2 - 2 * nitidez)." >&2
  exit 1
fi

# LFGA, a granulacao do FSR: formula de FsrLfgaF, aplicada depois do RCAS e em
# espaco linear, como o ffx_fsr1.h pede, com ruido azul animado pela razao
# aurea para a soma temporal nao ter vies. Medido no PS inteiro: sem granulacao
# a saida fica a 1 byte do RCAS; com 0.15, 1,16 byte de desvio por quadro e
# vies de -0,003 byte em 16 quadros.
if ! grep -Fq 'c += (t * float3(a, a, a)) * min(float3(1.0, 1.0, 1.0) - c, c);' \
  "${rcas_shader}"; then
  echo "A granulacao deixou de ser o FsrLfgaF da AMD." >&2
  exit 1
fi
rcas_ps_body="$(awk '/^float4 PSRcas/,/^}/' "${rcas_shader}")"
filter_line="$({ grep -n 'rcas_filter(ip)' <<<"${rcas_ps_body}" || true; } | head -1 | cut -d: -f1)"
linear_line="$({ grep -n 'srgb_to_linear(' <<<"${rcas_ps_body}" || true; } | head -1 | cut -d: -f1)"
grain_line="$({ grep -n 'lfga(' <<<"${rcas_ps_body}" || true; } | head -1 | cut -d: -f1)"
encode_line="$({ grep -n 'linear_to_srgb(' <<<"${rcas_ps_body}" || true; } | head -1 | cut -d: -f1)"
if [[ -z "${filter_line}" || -z "${linear_line}" || -z "${grain_line}" || -z "${encode_line}" ]] ||
  (( linear_line < filter_line || grain_line < linear_line || encode_line < grain_line )); then
  echo "A granulacao saiu da ordem da AMD: RCAS, depois linear, depois LFGA, \
depois a codificacao de saida." >&2
  exit 1
fi
if ! grep -Fq 'generate_blue_noise(kGrainTileSize)' "${project_dir}/src/fsr/grain_texture.cpp" ||
  ! grep -Fq 'kGoldenRatioConjugate' "${project_dir}/src/fsr/rcas_constants.hpp"; then
  echo "A granulacao perdeu o ruido azul ou a animacao sem vies." >&2
  exit 1
fi
if ! grep -Fq 'PSSetShaderResources(0, 2, inputs);' "${project_dir}/src/fsr/upscale_pipeline.cpp"; then
  echo "O RCAS parou de ligar a textura de ruido azul no slot 1." >&2
  exit 1
fi

# O compute do menu e do FSR escrevem em slots que o SavedState precisa cobrir,
# senao o proximo desenho do jogo herda a UAV do upscale.
for compute_slot_call in 'CSGetUnorderedAccessViews' 'CSSetUnorderedAccessViews'; do
  if ! grep -Fq "${compute_slot_call}" \
    "${project_dir}/src/postprocess/device_state.cpp"; then
    echo "SavedState parou de cobrir a UAV de compute (${compute_slot_call}): \
o jogo herda a saida do upscale no proximo dispatch dele." >&2
    exit 1
  fi
done

# A regra que acabou com a piscada: o teste simula o pool do jogo trocando as
# texturas a cada quadro e exige que a captura pegue a cena em todos eles -- e
# mostra que a regra antiga, pela identidade da textura, alternava.
frame_transition_test="/tmp/photorealism-frame-transition-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/frame_transition_test.cpp" \
  -o "${frame_transition_test}"
"${frame_transition_test}"

# A PISCADA DA 0.22.2 e a interface. O teste modela o quadro medido no jogo:
# o bind do backbuffer que captura e o do upscale do proprio jogo; a
# reconstrucao vai no bind seguinte, antes da interface; um alvo qualquer do
# tamanho da tela nunca recebe a reconstrucao.
frame_reconstruction_test="/tmp/photorealism-frame-reconstruction-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/frame_reconstruction_test.cpp" \
  -o "${frame_reconstruction_test}"
"${frame_reconstruction_test}"

# O rastreio de passes da 0.22.2 disparava depois de 600 quadros quaisquer e
# caiu duas vezes na tela de carregamento, com 3 passes por quadro. O teste
# exige quadros seguidos de cena antes de armar.
trace_arming_test="/tmp/photorealism-trace-arming-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/trace_arming_test.cpp" \
  -o "${trace_arming_test}"
"${trace_arming_test}"

fsr_easu_constants_test="/tmp/photorealism-fsr-easu-constants-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/fsr_easu_constants_test.cpp" \
  -o "${fsr_easu_constants_test}"
"${fsr_easu_constants_test}"

fsr_scale_axes_test="/tmp/photorealism-fsr-scale-axes-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/fsr_scale_axes_test.cpp" \
  -o "${fsr_scale_axes_test}"
"${fsr_scale_axes_test}"

fsr_rcas_constants_test="/tmp/photorealism-fsr-rcas-constants-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/fsr_rcas_constants_test.cpp" \
  -o "${fsr_rcas_constants_test}"
"${fsr_rcas_constants_test}"

blue_noise_test="/tmp/photorealism-blue-noise-test"
g++ -std=c++20 -O2 -Wall -Wextra -Werror \
  "${project_dir}/tests/blue_noise_test.cpp" \
  -o "${blue_noise_test}"
"${blue_noise_test}"

fsr_render_scale_test="/tmp/photorealism-fsr-render-scale-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/fsr_render_scale_test.cpp" \
  -o "${fsr_render_scale_test}"
"${fsr_render_scale_test}"

menu_save_test="/tmp/photorealism-menu-save-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  -I"${project_dir}/tests/support" -I"${project_dir}/src" \
  "${project_dir}/tests/menu_save_test.cpp" \
  "${project_dir}/src/config/section_table.cpp" \
  "${project_dir}/src/config/photorealism_profile.cpp" \
  "${project_dir}/src/config/profile_fields.cpp" \
  -o "${menu_save_test}"
"${menu_save_test}"

# A eleicao da fonte do ponteiro vale por sessao de menu, nao por sessao de
# jogo: se ela sobreviver ao fechamento e o jogo passar a reportar pelo outro
# gancho, todo delta novo e descartado e a seta congela.
if ! awk '/^void PointerFeed::reset/,/^}/' \
  "${project_dir}/src/overlay/pointer_feed.cpp" | grep -Fq 'PointerSource::None'; then
  echo "PointerFeed::reset parou de soltar a fonte eleita: se o jogo trocar de \
gancho entre uma abertura e outra do menu, a seta congela." >&2
  exit 1
fi

# Um clique sem mexer o mouse antes tem que eleger a fonte igual a um
# movimento, senao o primeiro clique depois de abrir o menu se perde.
if ! grep -Fq 'buttons != 0' "${project_dir}/src/overlay/pointer_feed.cpp"; then
  echo "Um clique parado deixou de eleger a fonte do ponteiro: o primeiro \
clique depois de abrir o menu se perde." >&2
  exit 1
fi

overlay_pointer_feed_test="/tmp/photorealism-overlay-pointer-feed-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  -I"${project_dir}/tests/support" \
  "${project_dir}/tests/overlay_pointer_feed_test.cpp" \
  -o "${overlay_pointer_feed_test}"
"${overlay_pointer_feed_test}"

# A ordem importa e nao aparece em teste nenhum: o delta tem que ser lido ANTES
# de o buffer do DirectInput ser apagado para o jogo. Invertido, o menu zera o
# proprio movimento e o ponteiro fica parado. A comparacao e DENTRO de cada
# gancho -- os dois ficam no mesmo arquivo e comparar o arquivo inteiro compara
# funcoes diferentes.
gate_state_body="$(awk '/^HRESULT STDMETHODCALLTYPE hooked_get_device_state/,/^}/' \
  "${project_dir}/src/dinput/input_gate.cpp")"
gate_data_body="$(awk '/^HRESULT STDMETHODCALLTYPE hooked_get_device_data/,/^}/' \
  "${project_dir}/src/dinput/input_gate.cpp")"

state_read="$(grep -n 'report_state(' <<<"${gate_state_body}" | head -1 | cut -d: -f1)"
state_clear="$(grep -n 'clear_state(' <<<"${gate_state_body}" | head -1 | cut -d: -f1)"
data_read="$(grep -n 'report_data(' <<<"${gate_data_body}" | head -1 | cut -d: -f1)"
data_clear="$(grep -n '\*count = 0' <<<"${gate_data_body}" | head -1 | cut -d: -f1)"

if [[ -z "${state_read}" || -z "${data_read}" ]]; then
  echo "A porteira parou de ler o mouse antes de silencia-lo: o ponteiro do \
menu fica parado no meio da tela." >&2
  exit 1
fi
if [[ -z "${state_clear}" || -z "${data_clear}" ]]; then
  echo "A porteira parou de silenciar o mouse para o jogo: a camera volta a \
girar com o menu aberto." >&2
  exit 1
fi
if [[ "${state_read}" -gt "${state_clear}" || "${data_read}" -gt "${data_clear}" ]]; then
  echo "A porteira apaga o buffer do DirectInput antes de ler o delta: o menu \
zera o proprio movimento e o ponteiro nao anda." >&2
  exit 1
fi

# O menu tem que continuar utilizavel sem mouse nenhum. Tres vezes seguidas o
# caminho do mouse quebrou por uma razao diferente, e em todas o menu ficou
# inutilizavel. A navegacao por teclado nao depende de DirectInput, de entrada
# bruta nem de posicao de cursor.
for keyboard_path in 'kKeyUp' 'kKeyDown' 'kKeyLeft' 'kKeyRight' 'kKeyEnter' 'kKeyTab'; do
  if ! grep -rFqw "${keyboard_path}" "${project_dir}/src/overlay/page_keys.cpp" \
    "${project_dir}/src/overlay/overlay.cpp"; then
    echo "A navegacao por teclado do menu perdeu ${keyboard_path}: sem ela, \
qualquer defeito no caminho do mouse deixa o menu inutilizavel." >&2
    exit 1
  fi
done
if ! grep -Fqw 'poll_keys' "${project_dir}/src/overlay/input.cpp"; then
  echo "O menu parou de ler o teclado da janela: some o unico caminho que nao \
depende do mouse." >&2
  exit 1
fi

# E o menu tem que preferir esse delta a posicao do cursor do sistema.
if ! grep -Fq 'pointer_feed().active()' "${project_dir}/src/overlay/overlay.cpp"; then
  echo "O menu voltou a usar so a posicao do cursor do sistema: com o jogo \
recentralizando o cursor, a seta trava no meio da tela." >&2
  exit 1
fi

overlay_bindings_test="/tmp/photorealism-overlay-bindings-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  -I"${project_dir}/tests/support" -I"${project_dir}/src" \
  "${project_dir}/tests/overlay_bindings_test.cpp" \
  "${project_dir}/src/config/loader.cpp" \
  "${project_dir}/src/config/defaults.cpp" \
  "${project_dir}/src/config/section_table.cpp" \
  "${project_dir}/src/config/photorealism_profile.cpp" \
  "${project_dir}/src/config/profile_fields.cpp" \
  "${project_dir}/src/config/profile_logging.cpp" \
  "${project_dir}/src/config/profile_reference.cpp" \
  "${project_dir}/src/config/profile_state.cpp" \
  "${project_dir}/src/config/limits.cpp" \
  "${project_dir}/src/config/logging.cpp" \
  -o "${overlay_bindings_test}"
"${overlay_bindings_test}"

# O escritor de INI mexe num arquivo em que ~60% das linhas sao a justificativa
# medida de cada numero. Ele so pode trocar o texto do valor.
config_writer_test="/tmp/photorealism-config-writer-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/config_writer_test.cpp" \
  -o "${config_writer_test}"
"${config_writer_test}"

# O menu precisa ter controles. Um menu vazio compila, passa no validate e nao
# serve para nada -- foi o que a 0.20.0 entregou na primeira tentativa.
menu_controls="$(grep -cE '(toggle|slider|tone_slider|choice)\(' \
  "${project_dir}/src/overlay/bindings/menu_pages.cpp")"
if [[ "${menu_controls}" -lt 30 ]]; then
  echo "O menu tem so ${menu_controls} controles declarados. Ele existe para \
ajustar o plugin no jogo; uma janela sem opcoes nao entrega isso." >&2
  exit 1
fi

overlay_draw_list_test="/tmp/photorealism-overlay-draw-list-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/overlay_draw_list_test.cpp" \
  -o "${overlay_draw_list_test}"
"${overlay_draw_list_test}"

echo "Proxies, core Photorealism, captura Steam, depth, SSAO, telemetria, perfil, shaders e numeracao de versao validados."
