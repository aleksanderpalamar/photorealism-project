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

if ! rg -n 'process_frame\(swap_chain\);[[:space:]]*observe_postprocessed_frame\(swap_chain\);' \
    -U "${project_dir}/src/hooks/swap_chain_hooks.cpp" >/dev/null; then
  echo "Fronteira pos-processada ausente depois de todos os passes visuais." >&2
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
  'Modulo adaptacao por condicao 0.19.0' \
  'Ancoras 0.19.0: sol=%.0fK/%.3f chuva=%.0fK/%.3f noite=%.0fK/%.3f' \
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

# As guardas da curva de tom vivem AQUI, junto dos outros pinos do cfg, e nao
# no meio das chaves de outro modulo. Na 0.14.0 elas foram colocadas logo
# depois de max_indirect_luma, que era chave do RTGI -- e sairam junto com ele
# na 0.16.0, silenciosamente. Guarda misturada com modulo alheio morre com o
# modulo alheio.
# A curva de tom da 0.14.0. black_lift e o piso do preto: em zero o shader
# volta ao saturate() sem toe da 0.13.3, que esmaga a sombra em 0 e transforma
# o painel em massa preta. As quatro referencias do ATS medidas para esta
# versao tem o 1% mais escuro entre 8 e 11 de 255, e 0.0027 em linear cai
# exatamente ali depois do encode sRGB.
for lift_channel in black_lift_r black_lift_g black_lift_b; do
  if grep -Eq "^${lift_channel}=0(\.0+)?$" "${cfg}"; then
    echo "${lift_channel} voltou a zero: a sombra volta a ser esmagada em 0 e \
o visual medido nas referencias (p1 entre 8 e 11) fica inalcancavel." >&2
    exit 1
  fi
done
# 0.17.1: o piso tem cor. R abaixo de G nas cinco referencias -- se os tres
# voltarem a ser iguais o piso e acromatico de novo, que foi o que as capturas
# da 0.17.0 mostraram (8/8/8 e 9/9/9, R/G e B/G exatamente 1,000).
lift_r="$(grep -E '^black_lift_r=' "${cfg}" | head -1 | cut -d= -f2 || true)"
lift_g="$(grep -E '^black_lift_g=' "${cfg}" | head -1 | cut -d= -f2 || true)"
if [[ -z "${lift_r}" || -z "${lift_g}" ]] ||
  ! awk -v r="${lift_r}" -v g="${lift_g}" 'BEGIN { exit !(r + 0 < g + 0) }'; then
  echo "black_lift_r nao esta abaixo de black_lift_g: o piso volta a ser \
cinza, e o alvo medido tem R entre 29% e 64% de G nas cinco referencias." >&2
  exit 1
fi
for tone_pin in 'black_lift_r=0.001017' 'black_lift_g=0.001982' \
  'black_lift_b=0.001888' 'highlight_rolloff=0.35'; do
  if ! grep -Fqx "${tone_pin}" "${cfg}"; then
    echo "Curva de tom fora do valor aprovado: ${tone_pin}. A calibracao da \
0.14.0 foi medida contra as referencias; mudar sem medir de novo a perde." >&2
    exit 1
  fi
done
# tint e o eixo verde-magenta. Em zero sobra so temperature, que troca R contra
# B e nunca toca em G -- e as quatro referencias tem G como canal mais alto.
if grep -Eq '^tint=0(\.0+)?$' "${cfg}"; then
  echo "tint voltou a zero: sem o eixo verde-magenta nenhum ajuste de \
temperature alcanca o balanco medido nas referencias." >&2
  exit 1
fi
if ! grep -Fqx 'tint=0.35' "${cfg}"; then
  echo "tint fora do valor aprovado (0.35)." >&2
  exit 1
fi
# blacks somado das tres camadas era -0.06 e empurrava os pretos para baixo,
# contra o alvo. A base leva 0.05 para a soma dar zero.
if ! grep -Fqx 'blacks=0.05' "${cfg}"; then
  echo "blacks da base saiu de 0.05: somado aos dois deltas ele volta a ser \
negativo e empurra os pretos para baixo, contra o piso de black_lift." >&2
  exit 1
fi

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
# A ressalva de que o modulo contraria a medicao. Ela e o registro de que as
# cinco referencias do ATS foram medidas e NAO tem bloom -- bordas nitidas, sem
# cauda no lado escuro. Sem ela, o proximo a ler o arquivo assume que estes
# numeros perseguem o alvo medido, quando na verdade se afastam dele por
# escolha.
if ! grep -Fq 'ESTE MODULO E LICENCA ARTISTICA, E NAO O ALVO MEDIDO' "${cfg}"; then
  echo "A ressalva do bloom sumiu do cfg. Ela registra que as referencias \
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

# Os defaults internos de config.cpp valem quando o cfg some, e tone_curve_test
# nao consegue ve-los: aquele arquivo e Windows-only e nao linka no Linux. A
# igualdade entre as duas copias fica por conta destas guardas.
for tone_default in \
  'layer.black_lift_r = 0.001017f;' \
  'layer.black_lift_g = 0.001982f;' \
  'layer.black_lift_b = 0.001888f;' \
  'layer.highlight_rolloff = 0.35f;' \
  'layer.tint = 0.35f;'; do
  if ! grep -Fq "${tone_default}" "${project_dir}/src/config/defaults.cpp"; then
    echo "Default interno da curva de tom 0.14.0 divergiu do cfg: \
${tone_default}" >&2
    exit 1
  fi
done
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
if ! grep -Fq 'constants.temperature = input.temperature;' \
  "${project_dir}/src/postprocess/frame_constants.cpp" ||
   ! grep -Fq 'input.temperature = condition_.temperature();' \
  "${project_dir}/src/postprocess/postprocessor.cpp"; then
  echo "O cbuffer voltou a receber a temperatura estatica: a adaptacao \
calcularia e ninguem usaria." >&2
  exit 1
fi
if ! grep -Fq 'constants.tint = input.tint;' \
  "${project_dir}/src/postprocess/frame_constants.cpp" ||
   ! grep -Fq 'input.tint = condition_.tint();' \
  "${project_dir}/src/postprocess/postprocessor.cpp"; then
  echo "O cbuffer voltou a receber o tint estatico." >&2
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
if ! grep -Fq 'constexpr GradeField kGradeFields[]' "${project_dir}/src/config/grade_fields.cpp"; then
  echo "config.cpp deixou de ter a tabela unica de parametros de cor: parse e \
composicao voltam a poder divergir." >&2
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
  "${project_dir}/src/config/grade_fields.cpp" \
  "${project_dir}/src/config/limits.cpp" \
  "${project_dir}/src/config/logging.cpp" \
  -o "${config_load_test}"
PHOTOREALISM_PROJECT_DIR="${project_dir}" "${config_load_test}"

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
    printf " %.6f %.6f %.6f %.3f %.3f", \
      total["black_lift_r"], total["black_lift_g"], total["black_lift_b"], \
      total["highlight_rolloff"], total["tint"]
  }
' "${cfg}")"

# O hash fecha o cfg depois das guardas por chave, e nao antes.
#
# Ate a 0.13.3 ele vinha primeiro, e por isso nenhuma das guardas nomeadas
# acima chegava a falar: qualquer edicao do arquivo batia no hash e saia com
# "Configuracao consolidada foi alterada", que nao diz o que quebrou nem por
# que importa. Uma guarda que explica uma regressao sutil so serve se for ela
# a falar. Nesta ordem o hash continua pegando tudo que as guardas nao
# cobrem, e so isso.
expected_cfg_sha256="64f968a89ca6ed633b4678b835024e12c473e067645ef1b599be84ee869ee799"
actual_cfg_sha256="$(sha256sum "${cfg}" | awk '{print $1}')"
if [[ "${actual_cfg_sha256}" != "${expected_cfg_sha256}" ]]; then
  echo "Configuracao consolidada foi alterada: ${actual_cfg_sha256}" >&2
  exit 1
fi
# 0.14.0: blacks cumulativo saiu de -0.060 para 0.000 -- somado, empurrava os
# pretos para baixo contra o alvo, e o piso passou a ser black_lift. Os tres
# ultimos campos sao a curva de tom, e entraram no perfil justamente para que
# uma mudanca neles nao passe por uma camada de delta sem ser vista.
# exposure passou de -0.030 para 0.011 na 0.18.2 e a IMAGEM NAO MUDOU. O
# balanco de branco carregava +0,0411 EV escondidos e agora e normalizado em
# luminancia; os +0,0411 EV vieram para a exposicao base, onde da para ler.
# Exposicao e balanco sao multiplicacao em linear e comutam.
expected_profile="6400.0 0.011 1.070 0.970 0.050 0.100 -0.180 0.000"
expected_profile="${expected_profile} 0.080 0.240 0.200 0.030"
expected_profile="${expected_profile} 0.001398 0.002480 0.002268 0.350 0.500"
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

# O FSR saiu inteiro na 0.15.0: 5.833 linhas, um DLL e oito hooks de vtable que
# existiam so para alimenta-lo, custando uma indirecao em toda chamada de
# desenho do jogo. Uma remocao sem guarda volta sozinha na primeira vez que
# alguem colar um trecho antigo, e o modulo nunca substituiu um draw sequer.
# 'easu' e 'rcas' ficam FORA do padrao de propósito: casam com "measure" e
# derrubariam grade_report.py. E este proprio arquivo se exclui, porque uma
# guarda precisa nomear o que proibe.
fsr_leftovers="$(grep -rli 'fsr\|fidelityfx' \
  "${project_dir}/src" "${project_dir}/shaders" "${project_dir}/tests" \
  "${project_dir}/tools" 2>/dev/null | grep -v '/tools/validate\.sh$' || true)"
if [[ -n "${fsr_leftovers}" ]]; then
  echo "FSR reapareceu no codigo: ele foi removido na 0.15.0 por nunca ter \
substituido um draw, e cada hook que ele exigia custa uma indirecao em toda \
chamada de desenho do jogo." >&2
  echo "${fsr_leftovers}" >&2
  exit 1
fi
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

# O menu nunca grava sobre a calibracao medida. As tres camadas abaixo sao
# resultado de 541 amostras de jogo e so mudam por medicao nova.
for measured_layer in 'base.0.1.2' 'module.visual.0.2.0' 'module.rain_overcast.0.3.0'; do
  if grep -rFq "${measured_layer}" "${project_dir}/src/overlay"; then
    echo "O menu citou a camada medida ${measured_layer}. Ele so pode escrever \
na camada do usuario -- as medidas nao se ajustam por slider." >&2
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

# Entrada bruta: suspender e devolver sao as duas metades, e a segunda e a que
# machuca se sumir -- o mouse do jogo morre depois do primeiro Ctrl+P. Guardar
# o nome da funcao nao serve: um corpo vazio mantem o nome. O que se exige e a
# chamada de registro DENTRO de cada corpo.
raw_input_suspend="$(awk '/^void RawInputBlock::suspend/,/^}/' \
  "${project_dir}/src/overlay/raw_input.cpp")"
raw_input_restore="$(awk '/^void RawInputBlock::restore/,/^}/' \
  "${project_dir}/src/overlay/raw_input.cpp")"
if ! grep -Fq 'RIDEV_REMOVE' <<<"${raw_input_suspend}"; then
  echo "RawInputBlock::suspend parou de remover o registro de entrada bruta: \
o jogo continua recebendo o mouse com o menu aberto." >&2
  exit 1
fi
if ! grep -Fq 'RegisterRawInputDevices' <<<"${raw_input_restore}"; then
  echo "RawInputBlock::restore parou de devolver o registro ao jogo: o mouse \
do jogo morre depois do primeiro Ctrl+P." >&2
  exit 1
fi

menu_roundtrip_test="/tmp/photorealism-menu-roundtrip-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  -I"${project_dir}/tests/support" -I"${project_dir}/src" \
  "${project_dir}/tests/menu_roundtrip_test.cpp" \
  "${project_dir}/src/config/loader.cpp" \
  "${project_dir}/src/config/defaults.cpp" \
  "${project_dir}/src/config/section_table.cpp" \
  "${project_dir}/src/config/grade_fields.cpp" \
  "${project_dir}/src/config/limits.cpp" \
  "${project_dir}/src/config/logging.cpp" \
  -o "${menu_roundtrip_test}"
"${menu_roundtrip_test}"

# O ponteiro do menu anda por DELTA, nao por posicao. O jogo recentraliza o
# cursor do sistema a cada quadro para o DirectInput funcionar em modo
# relativo, entao ler GetCursorPos devolve o centro da tela sempre.
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
  "${project_dir}/src/config/section_table.cpp" \
  "${project_dir}/src/config/grade_fields.cpp" \
  -o "${overlay_bindings_test}"
"${overlay_bindings_test}"

# O escritor de INI mexe num arquivo em que ~60% das linhas sao a justificativa
# medida de cada numero. Ele so pode trocar o texto do valor.
config_writer_test="/tmp/photorealism-config-writer-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/config_writer_test.cpp" \
  -o "${config_writer_test}"
"${config_writer_test}"

# A camada do usuario tem que ser a ULTIMA soma: o menu grava a diferenca entre
# o que o usuario escolheu e o que as medidas dizem, e essa conta so fecha se
# nada vier depois dela.
if ! grep -A 12 'Settings compose_layers' "${project_dir}/src/config/loader.cpp" |
  grep -A 2 'stack.rain_overcast_0_3' | grep -Fq 'stack.user_0_20'; then
  echo "A camada do usuario deixou de ser somada por ultimo em compose_layers: \
o delta que o menu grava para de reproduzir o valor que estava na tela." >&2
  exit 1
fi

# O menu precisa ter controles. Um menu vazio compila, passa no validate e nao
# serve para nada -- foi o que a 0.20.0 entregou na primeira tentativa.
menu_controls="$(grep -c 'BindingKind::' "${project_dir}"/src/overlay/bindings/*_bindings.cpp |
  awk -F: '{ total += $2 } END { print total+0 }')"
if [[ "${menu_controls}" -lt 50 ]]; then
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
