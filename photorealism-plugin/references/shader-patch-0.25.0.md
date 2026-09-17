# Reescrita dos shaders do jogo 0.25.0

Estado: mecanismo medido, transpilado e compilado fora do jogo. **Ainda nao rodou no ETS2.**

O plugin passa a interceptar `ID3D11Device::CreatePixelShader` e devolver ao jogo um shader reescrito,
em vez de so fazer pos-processo no Present. E o mesmo lugar de atuacao do SnowyMoon, por um caminho
diferente e mais amplo.

## O que o SnowyMoon faz

Medido em `ets2ats_lighting_v2_5_9` (`dinput8.dll` 730 KB, `dxgi.dll` 18 MB), com `winedump`, `objdump`
e `strings`.

- `dinput8.dll`: proxy dos 6 exports reais, importando so `KERNEL32`. O conjunto
  `CreateToolhelp32Snapshot` + `Thread32First/Next` + `SuspendThread` + `Get/SetThreadContext` +
  `VirtualProtect` + `FlushInstructionCache` e motor de hook inline com congelamento de threads.
- `dxgi.dll`: proxy de `CreateDXGIFactory/1/2` **mais 29 exports `NVSDK_NGX_D3D11_*`** (DLSS embutido).
  Traz ImGui (DX11+Win32), wxWidgets (janela de configuracao separada) e esta protegido com Oreans Code
  Virtualizer (secao `.vlizer`).
- Importa `D3DCompile` e `D3DCompileFromFile` de `D3DCOMPILER_47.dll` e carrega o RTTI
  `.?AVDecompileError@@` -- a classe de excecao do decompilador HLSL do 3Dmigoto.
- Tem 87 caminhos de efeito do jogo em texto (`/effect/eut2/dif_spec_weight/...rfx1`,
  `/effect/deferred/def_tonemap/...`, `truckpaint`, `leaves`, `water`, `glass`, `lamp`).
- Tem um `main` HLSL substituto escrito a mao, em estilo de registrador de DXBC, com quatro variantes
  selecionadas por `#define SHADERTYPE 0..3`, e um prologo de 15 macros que liga os registradores
  daquela variante ao significado semantico:

  ```
  #define DEF_NORMAL v…      #define DEF_UV1 v…        #define DEF_SAMPLER_ALB1 s…
  #define DEF_VIEWPOS v…     #define DEF_TINT_ALB1 cb… #define DEF_RAIN1 cb0[…]
  ```

- As funcoes injetadas (`apply_road1`, `apply_road2`, `apply_rain`, `calcnormalmap`, `calcripple`)
  recebem `o0..o3` como `in out` e alteram o G-buffer no lugar. Ha cache em disco
  (`g_rain_reflect_cache`, erro `"Cache road shader error!"`).
- Liga cbuffers proprios em b9, b11, b12 e uma textura de ripple em t9.
- O `.cfg` dele (`snowymoon.io_lighting_v1_0_0.cfg`) bate campo a campo com o `cb9` do binario:
  `roads_defaultnormals` e `roads_normalintensity` sao `roadcfg.xy`, `vegetation_*` e `vegetationcfg`,
  `LightingMethod` e `surface_albedosaturation` sao os outros dois campos.

### Prova de que a leitura esta certa

O `defattr` real de `eut2.dif.spec.weight` (`7cea1883ec1c7f4c050b1fbcd032f21b.sm5x.fso`, extraido do
`effect.scs` do ETS2 instalado) desassembla em:

```
dp3 r0.x, v2.xyzx, v2.xyzx        mov  o1.w, cb0[1].w
rsq r0.x, r0.x                    sample r0.xyzw, v4.xyxx, t6.xyzw, s0
mul o0.xyz, r0.xxxx, v2.xyzx      mul  r1.xyz, r0.wwww, cb0[1].xyzx
mov o0.w, v3.z                    mul  r0.xyz, r0.xyzx, cb0[0].xyzx
```

O bloco `SHADERTYPE == 0` do SnowyMoon reproduz essas linhas uma a uma, com `DEF_NORMAL` no lugar de
`v2`, `DEF_VIEWPOS` de `v3`, `DEF_UV1` de `v4`, `DEF_SAMPLER_ALB1` de `s0`, `DEF_TINT_ALB1` de `cb0[0]`
e `DEF_TINT_SPEC1` de `cb0[1]`. Ele decompilou o shader do jogo, colou e editou.

## O que este plugin faz em vez disso

Reimplementar o shader do jogo a mao nao escala: sao 41 shaders `defattr` em 11 formas de entrada
diferentes, fora os de tonemap, fog, decal e vegetacao. Como o transpilador ja existe, o shader
original e transpilado **inteiro** e a injecao entra como uma chamada antes do fecho.

```
CreatePixelShader(DXBC do jogo)
  -> Container::parse      chunks, ISGN, OSGN, hash FNV-1a de 64 bits
  -> decode_program        SHEX -> instrucoes
  -> inspect_gbuffer_shader  elegibilidade + textura/sampler/UV de albedo
  -> emit_hlsl             HLSL em estilo de registrador + biblioteca + chamada
  -> D3DCompile ps_5_0     cache em disco por (hash do DXBC, hash da biblioteca)
  -> CreatePixelShader(bytecode reescrito)
```

Como a injecao roda **depois** do codigo do jogo, ela le o G-buffer ja escrito e nao precisa de quase
nenhuma ligacao por variante. Onde o SnowyMoon usa 15 macros `DEF_*`, aqui bastam tres valores:

| O SnowyMoon precisa de | Aqui sai de | Por que |
|---|---|---|
| `DEF_NORMAL` | `o0.xyz` | o jogo ja escreveu a normal final de espaco de vista |
| `DEF_VIEWPOS` | `o0.w` + `SV_Position` | `o0.w` e a profundidade linear negativa em metros (0.24.0) |
| `DEF_RAIN1` | `o3.z` | o jogo ja empacotou `refam * 4096` ali |
| `DEF_TINT_*`, `DEF_BLEND` | nada | o albedo e o especular ja estao em `o2.rgb` e `o1` |
| `DEF_UV1`, `DEF_SAMPLER_ALB1` | inspecao do primeiro `sample` | unica ligacao que sobra |

A mascara de estrada nao vem do material: vem de `o3.y == 32`, medida nas capturas da 0.24.0 e
confirmada em duas gravacoes com o caminhao andando.

## Medidas que sustentam as escolhas

Tudo abaixo saiu do `effect.scs` do ETS2 instalado (63 MB, 24371 arquivos, 1703 pixel shaders `sm5x`),
extraido com o mesmo extractor ja usado para os mods de referencia.

- **Formato**: `.rfx` e texto e mapeia `predicate { path: sm5x }` -> passe `defattr` ->
  `_shd/<md5>.sm5x.fso`. O `.fso` e um contentor `SHDO` que embrulha DXBC cru.
- **Saida do G-buffer**: os **41** shaders `defattr` dos alvos do SnowyMoon tem assinatura de saida
  **identica** -- `o0 float4, o1 float4, o2 float4, o3 uint4`. E por isso que uma unica chamada de
  injecao serve para todos.
- **Entrada**: 11 formas distintas, todas comecando em `v0:COLOR0` + `v1:SV_Position0` seguidas de
  `TEXCOORD`. O `DEF_INPUT` do SnowyMoon e exatamente essa lista.
- **Controle de fluxo**: **zero** nos 41 `defattr` (mediana de 85 instrucoes). Nos 1703 shaders do jogo
  aparecem `if` (645) e `loop` (60), entao o transpilador trata os dois.
- **Slots livres**: os pixel shaders do jogo declaram **so `cb0`**, texturas `t0..t15` e samplers
  `s0..s8`. Dai b13 para o buffer do plugin. O SnowyMoon usa t9, que o jogo ocupa em parte dos shaders.

## Cobertura do transpilador

| Etapa | Resultado |
|---|---|
| Transpila para HLSL | **1703 / 1703** pixel shaders `sm5x` do jogo |
| Compila de volta com `D3DCompile` | **1699 / 1699**, zero falhas |
| Shader de referencia, ida e volta | compila; `bfi` desce para `ishl`+`and`+`or`, mesmo resultado |
| Shader de referencia com injecao | compila |

(1699 e o numero de arquivos distintos: quatro nomes md5 se repetem entre os varios diretorios `_shd`.)
A compilacao usou `tools/shader_batch_compile.exe` sob Wine, com o mesmo `d3dcompiler_47.dll` e os
mesmos flags que o plugin usa em tempo de execucao.

Quatro defeitos apareceram nessa medicao e nenhum teria sido visto so olhando o shader de referencia:

1. **Broadcast em construtor**: HLSL recusa `float2(1.0)` -- precisa dos dois componentes. 601 shaders.
2. **`SamplerComparisonState`**: `sample_c`/`sample_c_lz` exigem declaracao propria. 384 shaders.
3. **Registradores empacotados**: o ISGN pode ter dois semanticos no mesmo `v1` (ex. `TEXCOORD0.xy` e
   `TEXCOORD7.z`). Deduplicar por registrador perde o segundo. 5 shaders.
4. **`nan`/`inf` imediatos**: `%g` imprime `nan`, que o HLSL nao conhece; viram `asfloat(<bits>)`.
   27 shaders.

## Dominio de tipo

O DXBC nao tem tipo no registrador: `r0` e 4 palavras de 32 bits e cada instrucao decide se le float,
int ou uint. O emissor declara todo temporario como `float4` e insere `asfloat`/`asint`/`asuint` na
fronteira de cada instrucao. Sem isso, `bfi`, `and`, `ubfe` e `ftou` -- que e como o jogo empacota o
`o3` do material -- saem com os bits errados e o material vira lixo, sem nenhum erro de compilacao.

As comparacoes (`lt`, `ge`, `ne`, `eq`) devolvem `0xFFFFFFFF` ou `0`, nao `0`/`1`, porque 57 instrucoes
`and` nos shaders do jogo consomem esse resultado como mascara de bits.

## O que isso destrava no menu

`Intensidade das normais` e `Normais padrao` (pagina Objetos / Estradas) existiam desde a 0.23.0
marcadas `PendingReason::GBuffer` -- declaradas e sem mecanismo. Agora alimentam `roadcfg` de verdade,
e sairam da lista de pendentes. A pagina ganhou `Piso molhado`, `Quantidade de agua`, `Molhado minimo`,
`Ondulacao da chuva`, `Brilho molhado` e `Escurecimento molhado`.

## O que falta

- **Rodar no jogo.** Nada aqui foi visto em tela; a cobertura e de compilacao, nao de imagem.
- A quantidade de agua e um controle manual. Ligar na condicao de chuva do observador de cena exige
  medir que o peso `rain` do `condition_model` (hoje `daylight * overcast`) separa chuva de dia
  nublado -- hoje nao separa.
- O ripple e procedural, parametrizado pela UV da estrada. Sem matriz de vista nao da para ancorar no
  mundo; a UV da malha resolve porque ja e ancorada, mas emenda em troca de textura.
- Segunda textura de albedo (`has_second_albedo`) e detectada e ainda nao usada.
- Shaders com `ret` antes do fim sao recusados: a injecao ficaria depois do retorno. Nenhum dos 41
  `defattr` cai nesse caso.
