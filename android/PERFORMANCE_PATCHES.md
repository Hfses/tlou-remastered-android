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
| 720p | `0x9103a0`, `0x9104f2` | Inicialização e todos os modos usam 1280×720 |
| 540p e 360p | `0x910677`–`0x9106ed` | Inteiros, RAX combinado, floats e downsample coerentes |

A seleção de resolução é exclusiva. Em 540p/360p, os floats são convertidos
com VCVTSI2SS a partir dos próprios campos inteiros alterados. XORPS limpa as
lanes superiores de XMM0/XMM1 antes da conversão, preservando o resultado
completo das instruções VMOVSS originais, inclusive quando a entrada contém
registradores com lanes não zeradas. A variante antiga
que gravava dimensões inteiras sobre instruções VMOVSS de 360p não é usada.
Não foram transportados o workaround de memória de macOS nem a remoção total
do watchdog: as correções existentes de jobs e timeout continuam ativas.

Validação real executada com Unicorn 2.1.4 sobre o eboot fornecido:
720p = 1280/720, 540p = 960/540, 360p = 640/360; confirmados os campos inteiros,
os floats XMM0/XMM1 e o tamanho combinado em RAX. A revisão 0.9 executa o
construtor e a função de seleção desde sua entrada: 5 modos × 4 variantes
de frequência × 3 resoluções (60 combinações), incluindo a cauda compartilhada
até a notificação externa. A versão anterior só testava o trecho a partir de
`0x9105fa`: não cobria a inicialização 1080p nem os desvios dos modos 2/3.
Agora os campos de inicialização são alterados e o despacho em `0x9104f2`
usa o bloco reduzido após preservar o registro de modo/frequência.
Todas as assinaturas dos
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
compilação assíncrona de shaders e readback 1/2, independente da resolução. Os padrões mantêm
resolução original, efeitos originais e o caminho de renderização já funcional.
HUD, opacidade/posição dos controles e proporção da apresentação mudam na hora;
patches e opções do runtime são aplicados na próxima abertura. FSR temporal,
leituras atrasadas e ajustes de temporização da lógica do jogo não foram ativados.


## A/B de novembro de 2026: não misturar resolução com readback

O launcher antigo mudava automaticamente `GOW3_READBACKS` para 2 sempre que a resolução
era diferente de Original. O log de 03/11 mostra 540p selecionado, porém saída
`1920x1080`, sincronizações de SRT e páginas protegidas; portanto não havia
isolamento de variáveis na comparação de FPS. A saída 1080p pode ser o buffer
de apresentação e não prova a resolução efetiva da cena.

Agora a escolha de readback é **independente** da resolução. Isso remove o
sobrecusto obrigatório de Precise ao testar 720p/540p/360p, mas **não**
comprova que os patches de 540p/360p estejam corretos graficamente.
Alterar os campos do eboot não garante que os alvos de post-processing,
vídeo e composição sejam escalados. Todos os três presets reduzidos
continuam experimentais; Original é a referência confiável.

Experimentos adicionais, **desligados por padrão**, já têm implementações
nativas no renderizador desta branch e agora aparecem separadamente no menu:
`GOW3_PREP_POOL` (reuso de alocações), `GOW3_CPU_TRIMS` (menos CPU por draw),
`GOW3_PREFETCH_PIPELINES` (pré-compilação), `GOW3_PARALLEL_PIPELINES`
(compilação paralela, risco visual), e `GOW3_PREP_WORKERS` (Auto/0/2/4/6).
Nenhum altera o código do jogo, e nenhum tem ganho Android confirmado.

Procedimento: começar com Original, readback 1 e experimentos desligados;
medir o mesmo trecho de jogo; ativar somente uma opção, reiniciar e
comparar; se degradar ou causar glitch, desativá-la. Os valores efetivos
são registrados no `launch.log` como `TLOU A/B`. A ação
“Desativar todos os experimentos” restaura os knobs novos.


## Logs 7/8 e revisão 0.9

540p carregou 24 escritas/106 bytes; Original carregou 12 escritas/48 bytes,
com os seis efeitos desligados nos dois testes. Não era uma falha de salvar a
seleção: o perfil antigo não cobria todos os caminhos de configuração.
`Output resolution: 1920x1080 (live)` informa a saída do upscaler, não mede os
alvos internos do jogo. Os dois logs têm os quatro experimentos desligados e
não contêm tempos de quadro CPU/GPU; não sustentam promessas de ganho desses
knobs. A revisão corrige o trabalho de renderização solicitado ao jogo sem
ativar novos experimentos. Validação no ARM64 real ainda é necessária para
confirmar imagem, resolução efetivamente consumida e FPS. O setter separado
de dimensões personalizadas em `0x910740` permanece intacto; não há chamada
direta a ele no executável analisado. A validação cobre configuração por enum
e inicialização, não prova cobertura de chamadas indiretas durante o jogo.
