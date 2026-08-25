# Escopo tecnico observado

O arquivo de configuracao de referencia foi usado para catalogar grupos de
recursos. A implementacao e independente.

| Grupo observado | Situacao na 0.4.0 |
| --- | --- |
| bootstrap DirectInput + nucleo DXGI | implementado em DLLs separadas |
| calibracoes historicas | implementadas como camadas cumulativas |
| exposicao, contraste, temperatura e cor | implementado no passe final |
| sombras, realces, pretos e brancos | implementado de forma conservadora |
| nitidez e contraste local | implementado com amostragem de vizinhos |
| TAA, DLSS e presets temporais | adiado; dependem da cadeia temporal |
| SSAO | adiado; depende de profundidade confiavel |
| motion blur | adiado; depende de movimento/profundidade |
| normais de estrada e vegetacao | fora do passe final; dependem de materiais |
| iluminacao interior e espelhos | adiado; exigem classificacao de cena |
| chuva e reflexos | adiado; exigem buffers/estados especificos |
