# Photorealism FSR 0.5.0 - EASU e RCAS reais

## Fonte e licenca

O shader usa AMD FidelityFX-FSR v1.0.2 do repositorio oficial GPUOpen. Os
headers `ffx_a.h`, `ffx_fsr1.h` e a licenca MIT original acompanham o pacote em
`third_party/fidelityfx-fsr`. `fsr1.hlsl` deriva do passe de exemplo oficial e
mantem a atribuicao.

## Ponto de insercao

Nao existe API publica Prism3D para inserir FSR. A integracao e um hook D3D11
nao oficial e defensivo. O modulo cataloga recursos ligados em OM e observa
`PSSetShaderResources`. Um recurso so acumula evidencia de composicao quando
seu SRV e consumido enquanto a identidade exata do backbuffer esta em OM.
Depois de estabilidade e dos gates de formato/escala, o modulo despacha EASU,
despacha RCAS e substitui somente aquele SRV. Draws posteriores de GPS, texto,
menu e UI continuam na resolucao de apresentacao.

Essa relacao e evidência observavel. Ela nao prova o nome ou a estrutura
interna do render graph proprietario da SCS.

## Gates e fallback

- fonte menor nos dois eixos;
- escala 1,05x a 2,00x e diferenca entre eixos <=2,5%;
- erro de proporcao <=1,5%;
- sample count, mip e array iguais a um;
- R16F, R11G11B10, RGBA8/BGRA8 UNORM ou sRGB;
- doze confirmacoes e atividade recente no passe direto;
- correlacao dimensional com o depth ativo para resolucoes dinamicas novas.

Em resolucao nativa, 125%, supersampling, proporcao insegura, formato
incompativel, baixa confianca, lock contention ou falha de shader/recurso, a
chamada original segue sem substituicao. O modulo nunca reduz sozinho a
resolucao interna do Prism3D e nao alega ganho em pass-through.

## Estado e desempenho

O passe salva e restaura CS shader, SRV 0, UAV 0, constant buffer 0 e sampler
0; UAVs/SRVs temporarios sao desassociados entre EASU e RCAS. Recursos sao
recriados por dimensao, protegidos contra resize/troca de device e nunca usados
pelo worker diagnostico. Oito conjuntos de timestamp/disjoint queries medem
EASU e RCAS separadamente sem `Flush`.

RCAS usa 0,4 stop, intencionalmente conservador. Validacao visual real ainda e
necessaria em vegetacao, cabos, placas, GPS, noite, chuva e camera externa.
Para R16F/R11G11B10, os gathers EASU aplicam o SRTM reversivel oficial por
amostra; RCAS opera no dominio temporario 0-1 e `FsrSrtmInvF` restaura HDR na
saida. Isso evita aplicar RCAS cegamente a valores HDR sem normalizacao.
