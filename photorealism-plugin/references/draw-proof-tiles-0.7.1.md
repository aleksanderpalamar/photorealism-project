# Prova passiva 0.7.1 - o passe final e dividido em quatro tiles

Origem: `revisao-passiva-draw-proof-scissor.md`, que definiu a captura, e a
entrega 0.11.4 + FSR/AA 0.7.1, que a executou.

## Pergunta

A 0.7.0 registrou janelas inteiras em que todos os draws observados eram
rejeitados exclusivamente pelo criterio de scissor: `observed=2400 valid=0`
com `scissor=2400` e todos os outros gates em zero.

Duas hipoteses estavam em aberto. Na primeira, o Prism3D chamaria
`RSSetScissorRects(0, nullptr)` com `ScissorEnable = FALSE`, e a regra estaria
exigindo um retangulo que o rasterizador nem usa. Na segunda, o scissor estaria
habilitado cobrindo a tela inteira, e a regra deveria aceitar.

A revisao passiva existiu para decidir entre as duas antes de tocar na regra.
Ela nao alterou nenhum criterio: `replacement=0` e `dispatch=0` em toda a
captura.

## Medicao

ETS2 1.60 sob Proton, AMD Radeon RX 6600 (RADV NAVI23), 1920x1080, backbuffer
B8G8R8A8_UNORM, aproximadamente 60 quadros por segundo.

Numa janela de dez segundos: `observed=2400`, ou seja 600 frames vezes quatro
draws. A agregacao por assinatura reduziu esses 2400 eventos a quatro linhas,
com aproximadamente 600 hits cada -- exatamente uma ocorrencia por frame por
assinatura.

## O que foi observado

Os quatro draws de cada frame diferem em um unico campo, o retangulo de
scissor, e esses quatro retangulos ladrilham a tela sem sobreposicao nem folga:

```
[   0,   0,  960,  540]      [ 960,   0, 1920,  540]
[   0, 540,  960, 1080]      [ 960, 540, 1920, 1080]
```

Todo o resto e identico entre eles:

- viewport `x=0 y=0 1920x1080`, `min_depth=0 max_depth=1`, fullscreen nos quatro;
- `ScissorEnable = TRUE` com `scissor_count = 1`;
- `Draw` de tres vertices, `TRIANGLELIST`, sem instanciamento;
- o mesmo pixel shader em todos os draws de todas as janelas;
- SRV no slot 0 = candidato `probable-scene`, R11G11B10_FLOAT 1920x1080;
- RTV = recurso com `exact_backbuffer=sim`, B8G8R8A8_UNORM_SRGB 1920x1080,
  alternando entre os dois buffers da swap chain;
- nenhum depth-stencil view ligado.

Todos os demais gates da prova passaram. O draw chegou ate o criterio de
scissor tendo ja provado RTV no backbuffer exato, RTV unico, ausencia de depth,
source elegivel no slot 0, viewport fullscreen, pixel shader presente e
topologia de triangulo fullscreen. Somente o scissor reprovou, porque nenhum
tile isolado cobre a render target.

## Nenhuma das duas hipoteses estava certa

O padrao observado nao e o Caso A nem o Caso B. `ScissorEnable` esta ligado,
entao o rasterizador realmente usa o retangulo, e o retangulo nao cobre a tela.
E um terceiro padrao que a revisao nao havia antecipado: a composicao final
existe, e fullscreen, e esta dividida em quatro passagens scissoradas.

A consequencia importante e que a regra de scissor nao esta lendo o estado de
rasterizacao errado. Ela detecta corretamente que nenhum daqueles draws cobre a
tela. O que esta errado e a conclusao tirada disso, porque os quatro juntos
sao a composicao final.

## Relacao com o artefato de quadrantes da 0.11.2

A 0.11.2 corrigiu uma substituicao insegura de scene-SRV que repetia a imagem
em quadrantes no menu, na garagem e em outras composicoes internas. O tiling
descrito aqui e a explicacao provavel daquele sintoma: substituir o SRV
acertava um subconjunto dos tiles, e os quadrantes terminavam com conteudos
diferentes.

Os dois fatos sao a mesma estrutura vista de lados opostos. Isso reforca que a
decisao de falhar fechado ate existir prova de draw estava correta.

## O que a captura confirmou da propria instrumentacao

- `raster_seed=0` em todas as janelas. Os hooks de `RSSetState`,
  `RSSetViewports` e `RSSetScissorRects` sempre alimentaram o shadow antes do
  primeiro draw observado, entao a semeadura sob demanda nunca precisou
  disparar. Ela e a garantia da semantica, nao o caminho normal.
- `raster_shadow=0`. O canario permaneceu mudo: o estado de rasterizacao foi
  conhecido em todos os draws.
- `contention=0` em toda a captura. Os tres setters compartilharem o lock do
  catalogo nao produziu contencao mensuravel nesta carga.
- `signature_overflow=0` nas janelas de scissor. A agregacao por identidade
  estrutural coube nos 64 slots e foi o que tornou o padrao visivel.

## Limite conhecido

Em janelas de outra fase da cena, com rejeicoes exclusivamente por depth
ligado, o `signature_overflow` chegou a variar entre 30 e 107. Sao render
targets auxiliares de 512x512, 512x256, 128x128, 1024x512 e outros tamanhos,
cada um com identidade legitimamente distinta, saturando a tabela.

Isso nao contaminou o achado, porque essas janelas tinham `scissor=0`. Mas a
capacidade de 64 assinaturas e insuficiente para analisar a classe de draws
com depth ligado, e precisara aumentar ou ganhar particao por motivo antes de
qualquer estudo daquele conjunto.

## Alcance da evidencia

Isto e evidencia observavel de uma execucao. Nao prova o nome nem a estrutura
interna do render graph proprietario da SCS.

A divisao em quatro tiles de 960x540 pode ser especifica desta resolucao, deste
driver, desta versao do Proton ou desta configuracao grafica. A 0.7.2 nao deve
assumir quatro tiles nem tamanho fixo: deve descobrir a particao observando a
uniao dos retangulos dentro do frame.

## Consequencia para a 0.7.2

A regra correta nao e aceitar qualquer scissor. Isso deixaria HUD, interface,
espelhos e reflexos passarem, que e exatamente o que a validacao existe para
barrar.

A regra correta e aceitar um scissor que seja sub-retangulo de um viewport
fullscreen quando todos os outros criterios ja provaram composicao final, e
considerar o passe provado quando a uniao dos retangulos observados cobre a
render target dentro do mesmo frame, com assinatura estavel entre frames.

Para a ativacao, a consequencia e mais pesada: nao e possivel substituir o SRV
em um tile. EASU, Temporal e RCAS precisam executar uma unica vez, antes do
primeiro tile do frame, para uma textura processada; a substituicao entao vale
para todos os tiles daquele frame e o SRV original e restaurado depois do
ultimo. Substituir por tile reproduz o artefato de quadrantes da 0.11.2.

Enquanto a prova nao validar, `direct_composition_hits` permanece zero em todos
os relatorios de cor, porque o candidato de cena nunca acumula evidencia de
composicao direta. Esse contador saindo de zero e o primeiro sinal de que a
regra por tiles funcionou.
