# Efeitos do pos-processo 0.23.3

Estado: entregue. Ainda nao rodou no jogo. Grupo 1 do plano "todas as opcoes do menu funcionando".

## Por que as opcoes nao mudavam nada na 0.23.2

| Opcao | Causa |
|---|---|
| Brancos | soma `Whites * mascara * 0.06` so acima de luma 0.55, saturada: no maximo ~6 codigos |
| Exposicao noturna | correta; so age com peso de noite > 0, e a sessao era de dia |
| Intensidade do SSAO | multiplicava uma base medida para ser sutil (0.28 fora, 0.20 dentro) |
| Temporal x Temporal nitido | os dois ligavam o mesmo resolve |
| Qualidade, FXAA, preset/detalhe/meia resolucao do SSAO, luz de interior | nao havia implementacao |

## Cadeia de passes

`FXAA -> grade -> luz de interior -> SSAO (oclusao + composicao) -> resolve temporal -> nitidez`.
Cada passo le o que o anterior escreveu, alternando dois alvos intermediarios; o ultimo escreve no
backbuffer (`src/postprocess/effect_chain.cpp`). Um passo desligado sai da cadeia. O historico
temporal guarda a saida do resolve, nunca a da nitidez, para a nitidez nao se acumular quadro a quadro.

## Valores

| Opcao | Implementacao |
|---|---|
| Brancos | `color / lerp(1, 1 - 0.5*Whites, smoothstep(0.30, 1.0, luma))` |
| Qualidade | alta: SSAO 16 amostras, bloom ate 5 niveis; media: 12 e 4; baixa: 8, meia resolucao e 3 |
| Detalhe do SSAO | alto/medio/baixo = 16/12/8 amostras; vale o menor entre qualidade e detalhe |
| Preset do SSAO | suave x1.00 raio, x1.00 vies, curva 1.00; medio x1.35, x0.75, 0.85; forte x1.75, x0.50, 0.70 |
| Intensidade do SSAO | base medida x `ssao_intensity` x 2.2 |
| Anti-aliasing | 0 sem resolve; 1 resolve; 2 resolve + RCAS 0.90; 3-6 (DLAA/DLSS) nao selecionaveis, e 3-6 no cfg valem 2 |
| FXAA | bordas por luma perceptual, busca de 10 passos ao longo da borda, mistura subpixel 0.75 |
| Luz de interior | ganho `1 + forca * 3 * media_externa` nos pixels a 1.5-4 m, suavizado; ceu e fundo intactos |

Os nomes medio e forte do preset do SSAO sao do photorealism-plugin: das telas de referencia so se
conhece "Soft".

## Medicoes na GPU

Harness em Wine + DXVK na RX 6600, com o `d3dcompiler_47.dll` do ETS2 compilando os arquivos do pacote,
entradas sinteticas e leitura dos pixels:

- **Brancos**, saida para as entradas 150/200/230/250:
  - -1.00: 150 186 199 209
  - -0.13: 150 198 225 243
  - 0: 150 200 230 250
  - +0.13: 150 202 236 255
  - +1.00: 150 218 255 255
- **FXAA**: diagonal de inclinacao 0.4 em 64x64; 0 pixels intermediarios na entrada e 91 na saida;
  nenhum pixel a mais de 4 px da borda alterado.
- **SSAO**, canto de sala em 320x180, visibilidade media/minima:
  - forca 0: 1.000 / 1.000
  - forca 1.0: 0.988 / 0.694
  - forca 1.5: 0.981 / 0.537
  - forca 4.0: 0.951 / 0.000
- **SSAO em meia resolucao**: media 0.982 contra 0.981 na cheia. Composicao cheia x meia difere 0.72
  codigo em media, e 1.9% dos pixels (bordas) passam de 12 codigos.
- **Luz de interior**, entrada 128, luminancia externa 0.4:
  - forca 0.17: cabine 139, fora e ceu 128;
  - forca 2.0: cabine 222, fora e ceu 128.
- **Nitidez do Temporal nitido** numa rampa de 6 px:
  - realce de 3 codigos com 0.50, 6 com 0.75, 10 com 0.90 e 16 com 1.00; areas planas iguais;
  - escolhido 0.90, que se distingue do Temporal sem estourar as bordas.

## Como verificar no jogo

- A linha `Efeitos 0.23.3` do log mostra qualidade, anti-aliasing, FXAA, forca e amostras do SSAO,
  meia resolucao, preset, luz de interior e niveis de bloom a cada mudanca no menu.
- SSAO: `Insert` ate o preview da mascara de visibilidade, mudando a intensidade.
- Qualidade: linha `Custo GPU do passe` nas tres opcoes.
- Exposicao noturna: a noite, com `Condicao 0.19.0: noite` acima de zero.
