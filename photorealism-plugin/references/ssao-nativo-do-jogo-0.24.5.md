# SSAO nativo do jogo desligado - 0.24.5

Estado: entregue. Ainda nao rodou no jogo.

## O achado do usuario

O usuario instalou outro plugin grafico do mesmo genero lado a lado para comparar,
e percebeu que ele reseta a qualidade grafica do ETS2 para "medio baixo" no
`config.cfg` do jogo (fora da pasta do plugin, em Documents). A hipotese: quem
controla a imagem final e o plugin, e deixar o jogo tambem aplicando os proprios
efeitos por cima causa o "fantasma" do SSAO ja relatado.

## Confirmado: o ETS2 tem SSAO nativo, e ele soma com o do plugin

O `config.cfg` do jogo (Documents\Euro Truck Simulator 2\config.cfg) tem uma
chave `uset r_ssao "0"/"1"` propria do motor -- nada a ver com
`photorealism-plugin.cfg`. O plugin nunca olhou para essa chave. Se ela estiver
ligada (o padrao provavel para quem nunca mexeu nas opcoes graficas do jogo), o
motor faz a propria oclusao de ambiente e entrega isso ja misturado na cena antes
do plugin sequer comecar o pos-processo. O SSAO do plugin entao roda em cima de
uma imagem que ja tem oclusao — soma, nao substitui.

O outro plugin, instalado no mesmo `bin/win_x64`, tem strings de texto simples
dentro do proprio `dxgi.dll` com um bloco de `uset` que ele escreve no
`config.cfg` do jogo, incluindo `uset r_ssao "0"`. Isso confirma a hipotese do
usuario: ele desliga o SSAO nativo porque tem o proprio.

O mesmo bloco tambem tem `uset r_aa "0"` -- diferente do nosso, que forca
`r_aa "6"` (`native_aa.0.12.2`) porque o TAA nativo ligado e o que expoe o depth
como shader resource pro nosso SSAO e resolve temporal (documentado em
`src/native_aa/apply.cpp`). Copiar `r_aa "0"` do outro plugin quebraria essa
dependencia. Cada plugin resolve o acesso ao depth de um jeito, e sao
incompativeis entre si.

O bloco tambem tinha `uset r_color_correction "1"` -- mantido ligado, nao
desligado. Isso bate com a arquitetura do pre-tom (0.24.1), que assume que o tom
nativo do jogo continua rodando e desenha logo antes dele: desligar
`r_color_correction` provavelmente muda esse passe e quebraria a deteccao do
pre-tom. Por isso essa chave nao entra nesta versao.

O resto do bloco (`g_reflection`, `g_rain_reflect_*`, `r_fake_shadows`,
`r_normal_maps`, `g_veg_detail`, `g_truck_light_specular`, `r_dof`,
`r_windowed_borderless`, `r_fullscreen_borderless`) sao provavelmente pre-requisitos
das proprias features dele (espelho, chuva, normais de estrada, folhas/grama) ou
ajustes de janela para a propria tecnica de gancho. Nenhuma tem equivalente
implementado no plugin hoje, entao nao ha "soma" para evitar -- ficam de fora
por enquanto. Quando Espelhos, Chuva e Folhas/Grama (grupo 2 e 3 do roadmap)
forem implementados, cada um deve ser reavaliado.

## O que muda

Mecanismo novo, `src/native_graphics/`, irmao do `native_aa` existente e
reaproveitando o mesmo codigo de leitura/escrita/backup
(`native_aa/config_file.*`, `game_target.*`, `config_text.hpp`, `aa_log.*`):

- roda no bootstrap do `dinput8.dll`, antes do DXGI, junto com a configuracao de
  AA ja existente;
- forca `r_ssao=0` no `config.cfg` do jogo, so se a chave ja existir e estiver
  diferente (nunca cria uma chave nova, mesma regra do AA);
- politica configuravel em `[native_graphics.0.1.0]` no
  `photorealism-plugin.cfg`, com `manage=true` por padrao -- desligar
  (`manage=false`) devolve o controle ao usuario;
- faz backup do `config.cfg` original antes da primeira escrita, no mesmo
  arquivo que o AA ja usa (`config.photorealism-native-aa.backup.cfg` -- e o
  snapshot de antes de qualquer coisa do plugin, nao precisa de outro);
- escrita atomica, mesmo padrao do resto do cfg do jogo.

## O que nao muda

- `r_aa` continua sendo gerido so pelo `native_aa` (valor 6, TAA nativo ligado);
- `r_color_correction`, `r_dof` e as chaves de espelho/chuva/vegetacao/sombra
  do outro plugin nao sao tocadas por este pacote.

## Verificacao

- Build e validate: nova secao no cfg, guardas de que o bootstrap chama a nova
  funcao, de que a politica e as strings de log continuam presentes, e de que a
  escrita usa o mesmo caminho atomico do resto.
- **Guardas**, quebradas numa copia: falta do gancho no bootstrap, `r_ssao`
  fora da lista de chaves geridas, e secao ausente do cfg -- as tres acusam.

**Ainda nao rodou no jogo.** Falta confirmar que desligar o SSAO nativo reduz ou
elimina o "fantasma" em intensidade alta -- a causa dentro do proprio algoritmo
de amostragem do plugin, se sobrar alguma coisa, continua em aberto.
