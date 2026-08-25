# Protecao do ciclo de vida do depth 0.7.1

## Problema confirmado

O ETS2 pode trocar a textura de profundidade ao alternar entre o menu e o
mundo 3D sem chamar `ResizeBuffers`. A 0.7.0 mantinha uma referencia COM valida
ao recurso anterior e continuava copiando seu conteudo. O SSAO entao combinava
a cor da cena atual com profundidade antiga, produzindo manchas e silhuetas
fantasma.

Pressionar `End` corrigia o quadro porque liberava a copia, descartava o
candidato e iniciava outra descoberta. Reiniciar o jogo tinha o mesmo efeito
ao reconstruir todo o estado D3D11.

## Correcao

Depois da descoberta de 30 segundos, cada binding real do candidato recebe um
numero serial. O `Present` considera a profundidade atual somente quando esse
serial avancou desde o frame anterior.

As chamadas `OMSetRenderTargets*` produzidas internamente pelo Photorealism
Plugin sao excluidas do observador. Assim, restaurar o estado D3D11 nao pode
manter artificialmente vivo um recurso abandonado pelo jogo.

No primeiro frame sem um binding novo:

1. a copia de profundidade nao acontece;
2. previews espaciais e SSAO ficam suspensos;
3. o shader visual photorealista aprovado continua diretamente no backbuffer.

Apos 30 frames consecutivos sem atividade, o candidato e invalidado somente
se sua geracao e seu serial ainda forem os mesmos. Em seguida, a janela de
descoberta reinicia automaticamente. Se um binding ocorrer antes da
invalidacao, o contador muda e a operacao e cancelada com seguranca.

## Invariantes visuais

- `photorealism.hlsl` permanece byte a byte igual ao shader aprovado;
- `ssao.hlsl` e os valores de `[module.ssao.0.7.0]` nao foram recalibrados;
- nenhuma camada cumulativa anterior foi sobrescrita;
- a cor, iluminacao e o SSAO aprovados permanecem iguais quando o depth esta
  valido;
- seguranca de ciclo de vida tem prioridade sobre aplicar SSAO com dados de
  outra cena.
