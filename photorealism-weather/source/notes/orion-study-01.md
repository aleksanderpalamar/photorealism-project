# Estudo Orion Elite — passe técnico 0.8.0

## Escopo

O Orion Elite foi usado somente como referência comparativa. O pacote contém
perfis `nice.sii`, `bad.sii`, perfis subterrâneos e `env_data.sii`; seus caminhos
de skybox apontam para assets externos e não foram incorporados ao Photorealism.

## Diferenças estruturais

- Photorealism e baseline oficial: 13 variações por perfil em tempo limpo e
  10 variações por perfil em mau tempo.
- Orion: 12 variações por perfil em tempo limpo e 4 em mau tempo.
- A substituição integral dos arquivos Orion removeria variedade climática e
  dificultaria a manutenção sobre versões futuras do jogo.

## Tendências aproveitadas

- Adaptação ao escuro mais rápida e ao claro um pouco mais lenta.
- Ombro de altas luzes mais curto.
- Maior separação entre luz ambiente e luz solar direta em tempo limpo.
- Menos névoa sob sol alto.
- Contraste mais definido durante chuva diurna.

## Tendências rejeitadas

- Bloom e limites de brilho extremos durante o dia.
- Exposição noturna muito baixa.
- Redução da quantidade de variações meteorológicas.
- Substituição das nossas skyboxes já validadas.
- Data solar fixa e qualquer conteúdo pertencente ao mod de referência.

## Aplicação conservadora

Os ajustes são multiplicadores adicionais aplicados depois dos passes já
aprovados. A adaptação ao escuro recebe `1.12x`, a adaptação ao claro `0.96x`,
e o ombro do tonemapping varia entre `0.90x` e `0.96x`, conforme o período e o
clima. Contraste, luz direta, resposta ambiental e névoa recebem mudanças entre
1% e 4%, evitando uma mudança brusca da identidade visual da versão 0.7.0.
