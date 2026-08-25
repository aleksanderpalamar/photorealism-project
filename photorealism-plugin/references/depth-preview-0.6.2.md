# Preview de profundidade 0.6.2

## Evidencia usada

Na coleta reiniciada dentro do mundo 3D, o recurso dominante apresentou:

- dimensoes `1920x2160`;
- formato `D32_FLOAT_S8X24_UINT`;
- uma amostra, sem MSAA;
- `39140` bindings em 30 segundos;
- bind flags `0x40`, somente `D3D11_BIND_DEPTH_STENCIL`.

A altura duplicada coincide com `r_scale_y=2` no perfil de 200%. Os recursos
quadrados legiveis por shader foram tratados como provaveis mapas de sombra.

## Acesso nao destrutivo

O formato e os bind flags do recurso original nao sao alterados. Depois de
salvar o estado D3D11 e desassociar os render targets, o plugin copia o recurso
para uma textura `R32G8X24_TYPELESS` com
`D3D11_BIND_SHADER_RESOURCE`. A SRV usa
`R32_FLOAT_X8X24_TYPELESS` e expoe somente o componente de profundidade.

Troca de dispositivo, `ResizeBuffers` ou reinicio com `End` libera a copia. A
referencia ao recurso selecionado usa contagem COM e uma geracao que impede
reuso acidental depois de uma nova descoberta.

## Modos de teste

`Insert` percorre quatro estados:

1. normal;
2. raw;
3. forward-Z realcado;
4. reversed-Z realcado.

O modo correto deve mostrar silhuetas coerentes da estrada, veiculos,
vegetacao e horizonte. Se raw tiver pouco contraste, os dois modos realcados
permitem determinar se a profundidade cresce ou diminui com a distancia.

## Criterio para o proximo passo

Somente um preview geometricamente coerente autoriza a linearizacao e o SSAO.
Tela uniforme, dados desalinhados ou geometria de sombras significam que o
candidato ou o momento da copia ainda precisa ser corrigido.
