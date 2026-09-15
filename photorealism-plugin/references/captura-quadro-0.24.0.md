# Captura do quadro 0.24.0

Estado: entregue. Ainda nao rodou no jogo. Grupo 2 do plano "todas as opcoes do menu funcionando".

## Para que serve

Os efeitos que faltam (pre-exposicao, pre-contraste, contraste dinamico, saturacao do albedo, espelhos,
motion blur, SSS e normais da estrada) precisam agir dentro do quadro do jogo, antes do tom. Para isso e
preciso saber em qual alvo de render o ETS2 guarda cada coisa. A captura grava um quadro inteiro, passe
a passe, e a ferramenta ajuda a identificar o conteudo de cada alvo.

## Como rodar no jogo

1. Dirigir de dia, com a camera dentro da cabine e um espelho aparecendo.
2. Ctrl+P, pagina inicial, **Capturar quadro para analise**.
3. O jogo pausa enquanto grava. O botao passa a mostrar `Captura salva: N alvos`.
4. A pasta `captura-quadro-AAAAMMDD-HHMMSS` fica em `bin/win_x64/photorealism-plugin/`.

## O que e gravado

- **Momento da copia**: em cada `OMSetRenderTargets` ou `OMSetRenderTargetsAndUnorderedAccessViews` do
  jogo, cada alvo que estava ligado e deixou de estar e copiado para uma textura staging. A copia guarda o
  resultado do passe que o usou; um alvo religado depois gera nova copia.
- **Fim do quadro**: no Present, antes do upscale e do pos-processo do plugin, os alvos ainda ligados sao
  copiados, e so entao tudo e escrito em disco. Passes do proprio plugin nao entram.
- **Arquivos**: `NNN_bPPP-QQQ_idII_rtS_LxA_fF.dds`, sendo PPP-QQQ os binds em que o alvo ficou ligado, II a
  identidade da textura no quadro, `rt`/`ds` cor ou depth, S o slot e F o formato DXGI da textura. DDS com
  cabecalho DX10 e os bytes crus do mip e da fatia que a view usava.
- **Manifesto** `manifesto.json`: a lista de binds com todos os alvos de cada um e a lista de copias, com
  formato da view (necessario para texturas typeless), mip, fatia e falha, quando houver.
- **Limites**: 160 copias ou 1,5 GB. Multiamostra, formato sem tamanho conhecido ou staging recusado ficam
  no manifesto com o motivo e sem arquivo.

## Relatorio

`python3 tools/gbuffer_report.py <pasta>` escreve `relatorio.md` e `previews/`:

- **Tabela por copia**: binds, identidade, slot, tamanho, formato e etiquetas.
- **Estatisticas por canal**: min, p5, p50, p95, max, media e valores distintos.
- **Previews**: RGB normalizado entre p1 e p99, e cada canal lado a lado.
- **Etiquetas** (pistas, nao conclusao):

| Etiqueta | Criterio |
|---|---|
| `profundidade` | alvo de depth |
| `normal_3c` | 70% dos pixels fora do fundo com vetor de comprimento 1 +-0.08 (decodificado de 0-1 quando o alvo e 0-1) |
| `possivel_normal_2c` | dois canais 0-1 centrados, com xy dentro do circulo unitario |
| `id_material_cN` | canal com ate 12 valores distintos |
| `possivel_velocidade` | dois canais, 60% dos pixels a menos de 0.02 do repouso |
| `hdr` | formato float com p95 acima de 1.05 ou maximo acima de 1.5 |
| `cor_ldr` | tres canais 0-1 que nao sao normal nem HDR |
| `valor_medio` | alvo de ate 64 pixels |

O autoteste (`--auto-teste`, rodado pelo `validate.sh`) confere as etiquetas em capturas sinteticas de
resposta conhecida: normal float e 0-1, albedo com id de material no alfa, iluminacao `R11G11B10`,
velocidade `R16G16_FLOAT` e depth `R32G8X24`.

## Proximo passo

Com a captura do usuario, os previews sao conferidos a olho e `references/gbuffer-ets2-0.24.0.md`
registra o alvo de cada coisa. So depois disso entra o primeiro efeito entre passes (0.24.1).
