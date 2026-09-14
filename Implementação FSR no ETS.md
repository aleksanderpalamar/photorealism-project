Pesquisei a documentação atual da AMP/GPUOpen e a conclusão é: a RX 6600 é de fato uma candidata para isso, porque o FSR é essencialmente baseado em shaders/compute e não depende de Tensor Cores. O FSR 2 é open source e a própria implemtação da AMD usa passes de compute HLSL. [FSR](https://github.com/GPUOpen-Effects/FidelityFX-FSR2?utm_source=chatgpt.com) [Clone do repo](/home/palamar/Projetos/FidelityFX-FSR2).
### A arquitetura que deve ser seguida.
```
                 ETS2 / Prism3D
                       │
                       ▼
              ┌─────────────────┐
              │ Frame em menor  │
              │ resolução       │
              └────────┬────────┘
                       │
          ┌────────────┼────────────┐
          │            │            │
          ▼            ▼            ▼
       Color         Depth       Motion
       buffer        buffer       vectors
          │            │            │
          └────────────┼────────────┘
                       ▼
             ┌──────────────────┐
             │ Temporal         │
             │ Reconstruction   │
             │                  │
             │ FSR 2.x          │
             └────────┬─────────┘
                      │
                      ▼
             ┌──────────────────┐
             │ RCAS / CAS       │
             │ sharpening       │
             └────────┬─────────┘
                      │
                      ▼
               Backbuffer 1080p
```

Para isso devemos seguir a implementação dessa forma, assim estamos criando uma a nossa versão inspirada na arquitetura do FSR.
A AMD documenta o FSR 2 como uma sequência de etapas:
1. liminance pyramid
2. reconstruction + dilation
3. depth clipping
4. locks
5. reprojection + accumulation
6. RCAS
[FSR](https://github.com/GPUOpen-Effects/FidelityFX-FSR2?utm_source=chatgpt.com)

## Roadmap

### Fase 1: FSR 1-style
Primeiro:
```
Render 1280x720
       ↓
EASU
       ↓
RCAS
       ↓
1920x1080
```
O Edge Adaptive Sptial Upscaling (EASU). Ele é especial, então não precisa de histórico temporal.
A implementação oficial da AMD inclusive disponibiliza o shader [EASU em HLSL](https://github.com/GPUOpen-Effects/FidelityFX-FSR/blob/master/sample/src/DX12/FSR_Pass.hlsl?utm_source=chatgpt.com).
Isso permitirá testar uma coisa fundamental:
> Quanto custa fazer upscaling na RX 6600?

Assim podemos medir diretamente:
```
Native 1920x1080
        ↓
GPU time

1280x720
        ↓
EASU
        ↓
RCAS
        ↓
GPU time
```
Se o ETS2 estiver limitado pelo custo de renderização da Prism3D, podemos ganhar bastante.

### Fase 2: Usar a GPU de verdade
Aqui entra exatamente as Compute Units da RX 6600.
Em hipotese nenhuma fazer upscaling na CPU.

O pipeline seria:
```cpp
DispatchCompute(
    upscaler_shader,
    output_width / 8,
    output_height / 8,
    1
);
```
E teriamos algo como :
```txt
[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    // samples
    // edge detection
    // reconstruction
    // output
}
```
Isso vira trabalho de compute da GPU.
E aí podemos otimizar especificamente para RDNA2, inclusive explorando wave operations quanto fizer sentindo.
A [documentação do FSR 2](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/main/Kits/FidelityFX/docs/techniques/super-resolution-temporal.md?utm_source=chatgpt.com) prevê HLSL compute shader e suporte a CS 6.2/6.6, incluido caminhos para hardware com wavefonts de 64 lanes.

### Fase 3: temporal reconstruction
Nesta fase começa a ficar realmente interessante, porque em vez de fazer `frame atual → upscale`, passamos para:
```txt
frame atual ───────────┐
                       │
frame anterior ────────┤
                       ▼
                 reprojection
                       │
                       ▼
                  accumulation
                       │
                       ▼
                    output
```
Para isso precisamos de:
```txt
Color
Depth
Motion vectors
Previous frame
Camera jitter
```
A AMD deixa claro que motion vectors e depth são recursos fundamentais para integração do [FSR 2](/home/palamar/Projetos/FidelityFX-FSR2).
E podemos fazer algo diferente do FSR, podemos chamar Prism Temporal Reconstruction (PTR)
A ideia é criar um algoritmo especifico para ETS2
Por exemplo:
```txt
                    PTR
                     │
       ┌─────────────┼─────────────┐
       │             │             │
     Depth         Motion        Color
       │             │             │
       └─────────────┼─────────────┘
                     ▼
             Motion confidence
                     │
                     ▼
              Temporal resolve
                     │
          ┌──────────┴──────────┐
          │                     │
       stable               unstable
          │                     │
      accumulate          current frame
          │                     │
          └──────────┬──────────┘
                     ▼
                  RCAS/CAS
                     │
                     ▼
                  output
```
E poderiamos fazer algo especifico para caminhões.
Por exemplo, identificar regiões com movimento rápido:
```txt
caminhão
rodas
árvores
guard-rails
postes
placas
```
e aplicar diferentes pesos temporais.
Dessa forma isso torna interessante para o ETS2 porque ele possui muito conteúdo que fica praticamente estático por vários frames:
```txt
estrada
montanhas
céu
prédios
terreno
painéis
vegetação distante
```
Enquanto:
```txt
rodas
tráfego
caminhão
pedestres
árvores próximas
```
mudam rapidamente.
E um algoritmo temporal pode explorar isso.
A AMD disponibiliza o códigodo [FSR e do SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/main/readme.md?utm_source=chatgpt.com) abertamente. O SDK atual inclusive possui FSR 2.3.4 e FSR 3.1.5 como técnicas de upscaling temporal.
Então o procedimento em paralelo deve ser exatamente este:
```txt
                plugin
                  │
       ┌──────────┴──────────┐
       │                     │
   FSR oficial          Prism PTR
       │                     │
       │                     │
   benchmark              pesquisa
       │                     │
       └──────────┬──────────┘
                  ▼
              comparação
```
Para medição:
```txt
Native
FSR 1
FSR 2.3
FSR 3.1
PTR v0.1
PTR v0.2
```
E coletamos:
```txt
GPU frame time
CPU frame time
FPS
1% low
VRAM
GPU utilization
artifact score
temporal stability
```
### Vantagens
No caso do meu plugin estamos mexendo diretamente com a Prism3D, em vez de tentar fazer isso genericamente como o ReShade.
Pois o ReShade possui API para add-ons e consegue trabalhar com recursos gráficos, mas identificar corrtamente o depth buffer de um jogo é um problema por si só; a própria documentação/discussão do ReShade explica que não existe simplemente "o depth buffer", sendo necessário identificar o recurso correto entre os recursos criados pelo jogo.
A primeira milestone que eu definiria seria extremamente pequena:

> Renderizar o ETS2 internamente em 1280x720 e reconstruir 1920x1080 usando um compute shader EASU na RX 6600. [FSR](/home/palamar/Projetos/FidelityFX-FSR2)
