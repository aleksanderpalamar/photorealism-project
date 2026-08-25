# Descoberta de profundidade 0.6.1

## Causa da correcao

O teste da 0.6.0 confirmou que os quatro hooks estavam ativos e que a
telemetria permanecia estavel. Entretanto, depois de entrar no mundo 3D e
reiniciar a coleta com `End`, o observador registrou zero candidatos. A regra
daquela versao aceitava somente texturas exatamente iguais ao backbuffer de
1920x1080.

O ETS2 estava configurado com escala interna de renderizacao. Portanto, a
resolucao da textura usada para o mundo 3D nao precisava coincidir com a
resolucao final apresentada. O catalogo tambem tinha atingido o limite de 64
depth-stencil views, embora varias views pudessem apontar para a mesma textura.

## Modelo corrigido

A 0.6.1 identifica primeiro a textura `ID3D11Texture2D` subjacente e agrupa
todas as views que apontam para ela. O catalogo aceita qualquer dimensao e
classifica cada recurso combinando:

- area em pixels;
- proximidade da proporcao do backbuffer;
- frequencia de bindings durante a janela;
- penalidade para texturas quadradas quando a tela nao e quadrada.

O cache de views e separado do catalogo de texturas. Se o catalogo atingir seu
limite, um recurso novo substitui somente um candidato com prioridade menor.

## Relatorio esperado

Depois de entrar no mundo 3D, pressionar `End` e dirigir por mais de 30
segundos, o log deve conter:

- `Descoberta depth 0.6.1 reiniciada via End`;
- `Descoberta depth 0.6.1 concluida`;
- linhas `Depth grupo`, ordenadas pela relevancia conjunta;
- linhas `Depth recurso`, ordenadas pela relevancia individual.

Os campos decisivos para o proximo passo sao `size`, `area_scale`,
`aspect_error`, `texture_format`, `view_format`, `samples`,
`shader_readable`, `views` e `bindings`.

## Limite desta versao

Esta versao continua estritamente diagnostica. Ela nao cria SRV, nao copia
profundidade e nao adiciona SSAO. A leitura ou interceptacao do recurso sera
implementada somente depois de o log identificar de forma consistente a
textura usada no mundo 3D.
