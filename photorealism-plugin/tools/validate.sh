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
    "${project_dir}/src/steam_screenshots.cpp" >/dev/null; then
  echo "A captura Steam nao pode consultar teclado." >&2
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
  if ! grep -Fq "${steam_screenshot_message}" "${dxgi_strings}"; then
    echo "Integracao Steam screenshot incompleta: ${steam_screenshot_message}" >&2
    exit 1
  fi
done

if ! rg -n 'process_frame\(swap_chain\);[[:space:]]*observe_postprocessed_frame\(swap_chain\);' \
    -U "${project_dir}/src/hook.cpp" >/dev/null; then
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
  'Perfil efetivo (cor): tint=%.3f'; do
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
expected_depth_view_space_sha256="fda4531182a5b46b74c21eda30d679cd3952dc99a8cabba03ebf7041e24f4bd2"
actual_depth_view_space_sha256="$(sha256sum "${depth_view_space_header}" | awk '{print $1}')"
if [[ "${actual_depth_view_space_sha256}" != "${expected_depth_view_space_sha256}" ]]; then
  echo "Header depth/view-space aprovado foi alterado: ${actual_depth_view_space_sha256}" >&2
  exit 1
fi


depth_preview_shader="${project_dir}/shaders/depth-preview.hlsl"
expected_depth_preview_shader_sha256="e12de14a45ce2781507963c8834ca53a1503aa25ed58a5e90b51ffd02c7b0f61"
actual_depth_preview_shader_sha256="$(sha256sum "${depth_preview_shader}" | awk '{print $1}')"
if [[ "${actual_depth_preview_shader_sha256}" != "${expected_depth_preview_shader_sha256}" ]]; then
  echo "Shader depth preview aprovado foi alterado: ${actual_depth_preview_shader_sha256}" >&2
  exit 1
fi

ssao_shader="${project_dir}/shaders/ssao.hlsl"
expected_ssao_shader_sha256="97e0434b739789210eb4e77896b21d55301f331309f3c89779a6bf74fe050314"
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
    "${project_dir}/src/resource_observer.cpp"; then
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
  if ! grep -Fq "${tone_default}" "${project_dir}/src/config.cpp"; then
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
expected_visual_shader_sha256="a93788e3924ac235170816f03803d138306a212aae0290bcfc1cdc482a2ca911"
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
if ! grep -Fq 'scene_observer_.observe(device_, context_, scene_texture_);' \
  "${project_dir}/src/postprocess.cpp"; then
  echo "O observador de cena parou de medir scene_texture_: medir a saida do \
grade fecha uma realimentacao entre a cor e as features." >&2
  exit 1
fi

# A guarda acima fixa a LINHA DA CHAMADA, e na 0.18.0 a chamada estava certa: o
# que estava errado era a tabela de formatos logo depois dela. O observador
# recusava DXGI 90 (B8G8R8A8_TYPELESS), que e exatamente o formato que
# ensure_frame_resources cria para a copia da cena, e passou a versao inteira
# desligado com validate.sh verde. Por isso a tabela saiu do .cpp para um
# cabecalho testavel, e por isso estas duas guardas existem.
if ! grep -Fq 'scene_formats::is_readable' \
  "${project_dir}/src/scene_observer.cpp"; then
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
  "${project_dir}/src/scene_observer.cpp"; then
  echo "O observador parou de lembrar que ja falhou: sem isso a recusa volta a \
ser registrada uma vez por frame." >&2
  exit 1
fi

scene_features_test="/tmp/photorealism-scene-features-test"
g++ -std=c++20 -Wall -Wextra -Werror \
  "${project_dir}/tests/scene_features_test.cpp" \
  -o "${scene_features_test}"
"${scene_features_test}"

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
expected_cfg_sha256="5491d9c98e7e66e474cfafd4ed8ca876636f7942310d4b47b2255d11d04c2579"
actual_cfg_sha256="$(sha256sum "${cfg}" | awk '{print $1}')"
if [[ "${actual_cfg_sha256}" != "${expected_cfg_sha256}" ]]; then
  echo "Configuracao consolidada foi alterada: ${actual_cfg_sha256}" >&2
  exit 1
fi
# 0.14.0: blacks cumulativo saiu de -0.060 para 0.000 -- somado, empurrava os
# pretos para baixo contra o alvo, e o piso passou a ser black_lift. Os tres
# ultimos campos sao a curva de tom, e entraram no perfil justamente para que
# uma mudanca neles nao passe por uma camada de delta sem ser vista.
expected_profile="6400.0 -0.030 1.070 0.970 0.050 0.100 -0.180 0.000"
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

native_aa_source="${project_dir}/src/native_aa_config.cpp"
for native_aa_marker in \
  'eurotrucks2.exe' \
  'amtrucks.exe' \
  'config.photorealism-native-aa.backup.cfg' \
  'MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH' \
  'kNativeAaSection = "native_aa.0.12.2"' \
  'read_native_aa_policy' \
  'policy.manage' \
  'policy.aa.c_str()' \
  'policy.taa_sharpen.c_str()' \
  'plugin_config_value' \
  'nenhuma alteracao no ' \
  'politica=photorealism-plugin.cfg'; do
  if ! grep -Fq "${native_aa_marker}" "${native_aa_source}"; then
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
  if grep -Fq "${retired_key}" "${project_dir}/src/postprocess.cpp"; then
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
  if grep -Fq "${retired_hook}" "${project_dir}/src/hook.cpp"; then
    echo "Hook aposentado na 0.15.0 voltou a hook.cpp: ${retired_hook}. Ele \
roda em toda chamada de desenho do jogo." >&2
    exit 1
  fi
done

echo "Proxies, core Photorealism, captura Steam, depth, SSAO, telemetria, perfil, shaders e numeracao de versao validados."
