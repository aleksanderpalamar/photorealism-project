# Roadmap

O desenvolvimento do modulo espacial FSR possui numeracao e roadmap proprios
em `FSR_ROADMAP.md`. A fundacao FSR 0.1.0 e o observador color 0.2.0 acompanham
o nucleo 0.10.1 sem modificar sua pilha visual.

## 0.1.x a 0.3.x - Fundacao e calibracao visual

- hook do `IDXGISwapChain::Present` em Direct3D 11;
- passe de cor, tons, contraste local e nitidez;
- recarga em tempo real e comparacao A/B;
- calibracoes de base, visual geral e chuva/tempo nublado;
- validacao no ETS2 1.60 sob Proton/DXVK.

## 0.4.x - Arquitetura dual e calibracao cumulativa

- `dinput8.dll` dedicado ao bootstrap e DirectInput;
- `dxgi.dll` dedicado ao proxy DXGI e ao nucleo grafico;
- fabricas `CreateDXGIFactory*` encaminhadas ao DXGI do sistema;
- camadas historicas somadas sem sobrescrever calibracoes anteriores;
- resultado visual efetivo da 0.3.0 preservado durante a migracao.

## 0.5.0 - Robustez do ciclo de swap chain

- hook e tratamento de `ResizeBuffers`;
- deteccao de recriacao/redimensionamento do swap chain;
- invalidacao seletiva e reconstrucao automatica dos recursos de frame;
- sincronizacao entre apresentacao e redimensionamento.

## 0.5.1 - Linha de base de desempenho

- queries D3D11 de timestamp sem bloqueio;
- anel de amostras compativel com entrega atrasada do DXVK;
- media, minimo, pico e descartes registrados a cada dez segundos;
- continuidade do passe quando a telemetria estiver indisponivel.

## 0.5.x - Robustez DXGI complementar

- compatibilidade com formatos adicionais de backbuffer;
- fallback de hook caso a vtable compartilhada nao funcione no DXVK usado.

## 0.6.0 - Descoberta de profundidade

- hooks da familia `OMSetRenderTargets` no contexto D3D11;
- catalogo temporario de depth-stencil views;
- classificacao de candidatos em resolucao nativa;
- verificacao de formato, MSAA e permissao de leitura por shader;
- nenhuma aplicacao espacial antes de validar o recurso correto.

## 0.6.1 - Descoberta com escala interna

- agrupamento das views pela textura de profundidade real;
- suporte a resolucoes internas diferentes do backbuffer;
- classificacao por area, proporcao de tela e frequencia de uso;
- retencao dos candidatos relevantes quando o catalogo atinge o limite;
- relatorio separado de grupos de resolucao e recursos individuais.

## 0.6.2 - Captura nao destrutiva e preview

- retencao segura do candidato principal depois da descoberta;
- copia para textura typeless legivel por shader sem modificar o jogo;
- visualizacoes raw, forward-Z e reversed-Z em escala de cinza;
- fallback automatico para o passe visual em qualquer falha;
- confirmacao visual da geometria antes de integrar efeitos espaciais.

## 0.6.3 - Linearizacao reversed-Z

- consolidacao da orientacao reversed-Z confirmada pelas imagens;
- realce logaritmico da profundidade reversed-Z;
- conversao para distancia pelo modelo de plano distante infinito;
- parametros diagnosticos separados da calibracao visual;
- validacao da distribuicao de distancia antes do primeiro SSAO.

## 0.6.4 - Normais reconstruidas

- alcance do preview linear reduzido para destacar os primeiros 50 metros;
- reconstrucao aproximada da posicao em espaco de camera;
- derivadas escolhidas pelo menor salto de profundidade nas silhuetas;
- visualizacao RGB das normais sem alterar o shader visual aprovado;
- validacao estrutural final antes de experimentar SSAO.

## 0.7.x - Iluminacao espacial

- primeiro SSAO baseado na profundidade e nas normais validadas;
- composicao em passe separado, preservando o shader visual aprovado;
- monitor de atividade do depth e recuperacao automatica em transicoes de
  menu sem `ResizeBuffers`;
- atividade confirmada por binding ou limpeza real do depth, com tolerancia
  curta para continuidade entre frames;
- suspensao preventiva do SSAO quando a profundidade nao pertence ao frame
  atual, preservando o tratamento photorealista sem manchas;
- oito amostras, raio curto, rejeicao de silhuetas e fade por distancia;
- mascara de visibilidade para calibracao visual e deteccao de halos;
- refinamento separado para interior e exterior;
- preservacao de farois, flares e chuva em cenas noturnas.

## 0.8.x - Refinamento SSAO photorealista

- dois aneis simetricos e 16 amostras para contatos menos direcionais;
- intensidade media preservada em relacao ao SSAO aprovado;
- protecao adaptativa de altas luzes para farois, flares, chuva e reflexos;
- configuracao cumulativa independente, com retorno ao SSAO 0.7.0;
- refinamento separado para interior e exterior depois da validacao.

## 0.9.0 - Refinamento SSAO interior e exterior

- perfil exterior 0.8.0 preservado para a geometria alem do campo proximo;
- perfil de cabine/interior de raio curto, menor intensidade e maior rejeicao
  de bordas;
- transicao continua baseada na distancia linear do depth, sem etiqueta ou
  alteracao semantica do ETS2;
- configuracao independente, recarregavel por `End` e desativavel sem perder
  o SSAO exterior consolidado.

## 0.9.1 - Estabilidade do depth em ETS2 e ATS

- ranking orientado pela frequencia real de bindings, com suporte a escala
  interna assimetrica;
- retencao temporaria de todos os candidatos observados e consolidacao somente
  do vencedor final;
- confianca minima para rejeitar depths de menu e interface;
- testes de regressao com os candidatos registrados nos logs dos dois jogos;
- nenhuma alteracao nos shaders ou na calibracao visual da 0.9.0.

## 0.10.0 - Primeira integracao espacial e temporal

- resolve temporal posterior e compativel com o TAA nativo do jogo;
- historicos nao destrutivos de cor e profundidade;
- clamp por vizinhanca e rejeicao por cor/depth para limitar ghosting;
- invalidacao automatica nas mesmas transicoes protegidas pelo SSAO;
- primeira calibracao voltada a cintilacao de vegetacao e detalhes distantes.

## 0.10.1 - Inicializacao espacial e temporal automatica

- descoberta permanece ativa por ciclos enquanto o jogo estiver no menu;
- entrada no mundo 3D consolida automaticamente um depth seguro;
- escala interna e taxa de bindings separam a cena principal da interface;
- SSAO e resolve temporal iniciam sem qualquer acionamento de `End`;
- atalhos continuam disponiveis somente para comparacao e diagnostico;
- nenhuma alteracao na calibracao visual consolidada da 0.10.0.

## 0.11.0 - AA espacial/temporal proprio antes da interface

- desativacao automatica e reversivel dos cvars AA/TAA nativos somente em
  ETS2/ATS, com backup, escrita atomica e log detected/applied;
- selecao comprovada do scene-color no passe de composicao anterior a UI;
- AA espacial edge-aware, historico ping-pong, clamp/rejeicao e busca local
  de correspondencia temporal, sem depender de tecla;
- RCAS conservador depois do temporal; EASU permanece condicional a upscale
  seguro e fonte realmente menor;
- telemetria separada antes de qualquer afirmacao de superioridade visual;
- F12 Steam unico e a pilha visual consolidada permanecem preservados.

## Etapa final - Refinamento temporal

- validar reducao de cintilacao em movimento nos dois jogos;
- calibrar separadamente estabilidade e rejeicao de historico, caso necessario;
- avaliar acesso seguro a jitter ou vetores de movimento antes de evoluir o AA
  proprio para reprojecao completa.

Cada etapa so avancara apos comparacao A/B, teste de chuva, amanhecer,
entardecer, noite e verificacao de frame time.

## 0.12.0 - Screen-Space Ray-Traced Global Illumination (SSRTGI)

Luz indireta em screen-space, de curto/medio alcance (0.5 m a 15 m), sobre
cabine, caminhao, asfalto, paredes, postos, edificios e vegetacao proxima. O
nome e deliberado: screen-space, nao hardware ray tracing. DXR/D3D12 sobre os
RT cores da RX 6600 fica explicitamente fora do escopo.

O GI roda **antes** do grading. O diagrama original da tecnica pedia
`SSAO -> GI -> Temporal -> grading`, mas a cadeia real do plugin sempre foi
`grading -> SSAO -> temporal`: inverte-la invalidaria a calibracao consolidada
da base 0.1.2 + 0.2.0 + 0.3.0, os limiares de highlight do SSAO e o
`color_rejection` do temporal. Pondo o GI antes do grading, o motivo declarado
-- o grading alcancar tanto a luz direta quanto a indireta -- e atendido sem
custo de recalibracao:

```
scene color -> SSRTGI -> grading -> SSAO -> temporal -> backbuffer
```

As fases:

- **0.12.0 (entregue)** consolidacao da matematica depth/view-space numa fonte
  unica, configuracao, buffers em meia resolucao e o passe inerte; nenhum raio
  e tracado;
- **0.12.1 (entregue)** ray march de raio unico em screen-space, com acerto por
  espessura, contribuicao de ceu no miss e confianca separando "vazio" de
  "desconhecido"; o resultado preenche o buffer mas ainda nao e composto;
- **0.12.2** GI difusa multi-raio, protecao de iluminancia e composicao com
  `gi_intensity`;
- **0.12.3** acumulacao temporal com rotacao de raios por frame, somando
  `normal_rejection` a rejeicao de depth e cor que ja existe;
- **0.12.4** denoiser bilateral depth-aware e normal-aware, que nao pode
  atravessar bordas;
- **0.12.5** traversal Hi-Z sobre mips de depth; e o ponto em que compute
  shader passa a valer o custo de introduzir UAVs no plugin;
- **0.12.6** qualidade adaptativa e presets Low/Medium/High para a RX 6600.

A partir da 0.12.2 o RTGI precisa ser executado uma unica vez por frame, antes
dos quatro draws de composicao ladrilhados do Prism3D -- substituir tile a tile
reproduziria o artefato de quadrantes corrigido na 0.11.2. Isso depende da
prova de composicao, em investigacao na branch `fsr-0.7.2-tiles`.
