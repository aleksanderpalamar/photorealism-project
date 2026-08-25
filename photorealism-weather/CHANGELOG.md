# Changelog

## 0.11.0

- Novo ícone no mesmo padrão visual do Photorealism Landscape.
- Identidade Weather & Lighting preservada com atmosfera de céu e iluminação.
- Fonte original da arte e ícone anterior preservados em `source/artwork/`.
- Toda a calibração gráfica, climática, chuva, skyboxes e assets da versão
  `0.10.0` preservados sem alterações.

## 0.10.0

- Primeiro passe de `/def/climate.sii` sobre a versão 0.9.0 consolidada.
- Frequência de mau tempo discretamente elevada nos perfis `default` e `cold`.
- Frequência oficial preservada nos perfis `arid` e `desert`.
- Molhamento da pista progressivo e secagem mais lenta nas regiões temperadas e frias.
- Secagem moderadamente acelerada nos climas árido e desértico.
- Perfis técnicos, pesos regionais, iluminação, skyboxes, gotas e spray preservados.
- Pipeline reproduzível em `tools/build_climate_profile.py`.
- Nome do pacote padronizado como
  `photorealism-<versão-do-mod>-<versão-do-jogo>.scs`.

## 0.9.0-dev

- Estudo do Orion Elite Rains usado somente como referência técnica.
- Densidade potencial de gotas aumentada moderadamente de 5.000 para 9.000,
  preservando visibilidade durante chuva forte.
- Tamanhos de gota explicitamente limitados entre 7 e 21 mm no espaço do vidro.
- Aderência, gravidade, deformação e aceleração aerodinâmica recalibradas.
- Efeito de velocidade distribuído progressivamente entre 15 e 90 km/h.
- Limpadores deixam umidade residual sutil sem manter o vidro artificialmente seco.
- Spray de rodas com quatro estágios e equivalentes leve, médio e forte para o
  sistema de partículas de asfalto do ETS2 1.60.
- Spray de asfalto composto por névoa fina e uma segunda camada esparsa de
  partículas maiores de água.
- Texturas e materiais de chuva e spray do Orion incorporados com crédito ao
  autor, juntamente com máscaras das cabines oficiais do ETS2.
- Máscaras e conteúdo exclusivo do ATS foram removidos do pacote.
- Os modelos compilados referenciados pelo Orion não estavam armazenados no
  arquivo `.scs`; o passe continua usando os modelos correspondentes do jogo-base.

## 0.8.0-dev

- Estudo comparativo do Orion Elite aplicado como referência técnica, sem
  incorporar seus arquivos ou reduzir as variações meteorológicas oficiais.
- Adaptação ao escuro 12% mais rápida e adaptação ao claro 4% mais lenta.
- Ombro do tonemapping encurtado moderadamente para controlar altas luzes.
- Luz difusa, especular e resposta ambiental discretamente reforçadas durante
  o dia limpo, preservando as skyboxes aprovadas da versão 0.7.0.
- Névoa de meio-dia levemente reduzida para recuperar profundidade e nitidez.
- Contraste da chuva recuperado de forma conservadora, preservando intensidade,
  gotas, spray e molhamento.
- Valores extremos de bloom, exposição noturna e contraste do mod de referência
  deliberadamente rejeitados.

## 0.7.0-dev

- Família coerente de skyboxes aplicada aos perfis `default.nice.16`–`.21`.
- Geometria e distribuição das nuvens preservadas durante toda a transição.
- Perfil 18 discretamente mais claro e azul no ápice solar.
- Perfis 19–21 progressivamente mais quentes e suaves no início da tarde.
- Máscara de nuvens compartilhada para evitar desalinhamento na interpolação.
- Perfis noturnos, amanhecer, mau tempo e parâmetros de iluminação preservados.

## 0.6.1-dev

- Corrigida a projeção do skybox: fonte esférica 2:1 recortada para hemisfério
  superior 4:1, sem esticamento horizontal.
- Nuvens menores, mais espaçadas e com picos HDR reduzidos.
- Maior presença de céu azul e contraste mais natural no meio-dia.
- Máscara de nuvens refeita e alinhada ao novo panorama.
- Continuidade horizontal validada com RMSE igual a zero.

## 0.6.0-dev

- Primeiro panorama fotorealista próprio para céu claro ao meio-dia.
- Panorama HDR contínuo em 360 graus com resolução de 4096×1024.
- Máscara de nuvens e névoa alinhada em 2048×512.
- Conversão validada com SCS Conversion Tools 2.21 para R9G9B9E5 e R8.
- Skybox limitado ao perfil solar `default.nice.17` para teste controlado.

## 0.5.0-dev

- Primeiro passe noturno nos perfis `.01`–`.04` e `.32`–`.34`.
- Halos de faróis e luzes urbanas mais estreitos e controlados.
- Atmosfera noturna levemente reforçada em tempo bom e chuva.
- Exposição, adaptação ocular, iluminação da cabine e potência dos faróis preservadas.
- Cobertura completa dos 34 perfis solares em `nice.sii` e `bad.sii`.

## 0.4.0-dev

- Primeiro passe de mau tempo nos perfis `default.bad.05`–`.31`.
- Bloom e raios solares reduzidos para melhorar a definição da chuva.
- Sombras solares enfraquecidas sob céu nublado.
- Atmosfera reforçada sem modificar intensidade, molhamento ou frequência da chuva.
- Pipeline completa e idempotente em `tools/build_climate.py`.

## 0.3.0-dev

- Primeiro passe neutro do amanhecer em `default.nice.05`–`.13`.
- Primeiro passe neutro do entardecer em `default.nice.23`–`.31`.
- Atmosfera matinal reforçada sem alterar as cores originais do céu e do sol.
- Pipeline única e idempotente em `tools/build_nice.py`.
- Passe de meio-dia preservado sem acumular multiplicadores.

## 0.2.0-dev

- Primeiro passe neutro nos perfis de meio-dia `default.nice.14`–`.22`.
- Bloom, raios solares, saturação e contraste reduzidos de forma conservadora.
- Sombras diurnas suavizadas e perspectiva atmosférica reforçada.
- Baselines oficiais preservadas em `source/baseline/default/`.
- Calibração reproduzível por `tools/calibrate_midday.py`.

## 0.1.0-dev

- Estrutura inicial do projeto.
- Perfis `nice.sii` e `bad.sii` copiados como baseline oficial.
- Pastas reservadas para skyboxes, nuvens, máscaras, sombras e estrelas.
