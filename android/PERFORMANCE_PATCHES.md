# TLOU Android: perfil de desempenho validado

Perfil exclusivo do SELF fornecido em 03/11/2026:
`e5f0aa322f60bd90637cc10d83de08c77885b1921084021c630e04ed3d6febf7`.
Título CUSA00552, versão base 1.00, entrada ELF `0x56d9f0`.

Origem: XMLs de TLOU incluídos no runtime do Bachata S4+ v0.2.7 P36,
`TheLastOfUsRemastered_CUSA00552_Performance.xml` e assinaturas equivalentes
em `TheLastOfUsRemastered_CUSA00554.xml`. Foram removidos os `0x400000`
da base usada pelo Bachata; os endereços abaixo são relativos ao ELF.

| Opção | Endereço principal | Validação |
|---|---|---|
| SSAO desativado | `0x946363` | Imediato e instrução MOV completos |
| Motion blur desativado | `0x9463e2`, `0x946824` | Duas instruções MOV |
| Profundidade de campo desativada | `0x9463fe` | MOV do campo de configuração |
| Bloom desativado | `0x9464a4` | MOV completo com máscara dos campos |
| Sombras solares desativadas | `0x8efd22` | Salto e bloco anterior; destino preservado |
| Subsurface desativado | `0x8f2490` | Salto e teste anterior; destino preservado |
| 720p | `0x9105fd` | Perfil 1280×720 existente no jogo |
| 540p e 360p | `0x910677`–`0x9106ed` | Inteiros, RAX combinado, floats e downsample coerentes |

A seleção de resolução é exclusiva. Em 540p/360p, os floats são convertidos
com VCVTSI2SS a partir dos próprios campos inteiros alterados. A variante antiga
que gravava dimensões inteiras sobre instruções VMOVSS de 360p não é usada.
Não foram transportados o workaround de memória de macOS nem a remoção total
do watchdog: as correções existentes de jobs e timeout continuam ativas.

Validação real executada com Unicorn 2.1.4 sobre o trecho do eboot fornecido:
720p = 1280/720, 540p = 960/540, 360p = 640/360; confirmados os campos inteiros,
os floats XMM0/XMM1 e o tamanho combinado em RAX. Todas as assinaturas dos
seis efeitos e dos três perfis coincidem com o SELF. Isso valida o código e os
endereços, sem afirmar qualidade visual ou ganho de FPS já medidos no aparelho.

Para repetir com a própria cópia: `python tools/validate_tlou_performance.py /caminho/eboot.bin`
(requer Unicorn). Nenhum binário do jogo é armazenado no repositório.

O launcher gera `patches.bin` por troca atômica e registra `patch-validation.json`.
Alterar patches não repete a extração nem a ligação do jogo. Original regenera
somente as correções de estabilidade. Uma assinatura divergente impede o início
com o perfil solicitado e preserva o arquivo de patches anterior.

## Opções adicionais de desempenho

O menu expõe VSync, limite de apresentação, fila de renderização 0/1/2,
compilação assíncrona de shaders e readback 1/2. A resolução reduzida usa readback
preciso (2), conforme a recomendação dos perfis do Bachata. Os padrões mantêm
resolução original, efeitos originais e o caminho de renderização já funcional.
HUD, opacidade/posição dos controles e proporção da apresentação mudam na hora;
patches e opções do runtime são aplicados na próxima abertura. FSR temporal,
leituras atrasadas e ajustes de temporização da lógica do jogo não foram ativados.
