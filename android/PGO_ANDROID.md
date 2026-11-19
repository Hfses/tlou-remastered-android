# PGO do renderizador no Android

O **PGO (Profile-Guided Optimization)** usa informações coletadas durante o jogo para orientar a otimização do código ARM64 do renderizador.

No port para PC, PGO + LTO foi associado a aproximadamente **10% de ganho** numa cena pesada (39,6 → 43,5 FPS, em `docs/PC_PORT_REFERENCE.md`). **Isso não é uma medição nem uma promessa de ganho no Android.** O Android já usa LTO. O relatório `android/EBOOT_PERFORMANCE_AUDIT_20261105.md` encontrou cerca de 21,42 ms de CPU por quadro na thread de comandos, mas o efeito de PGO ainda precisa ser medido.

**Sem `android/pgo/tlou.profdata`, o modo `auto` mantém PGO desligado.** A alteração do código por si só não aumenta FPS.

## Coletar um perfil ARM64

1. Abra GitHub → Actions → **TLOU Bionic Full Native and APK** → *Run workflow*; escolha `pgo=generate`.
2. No artefato **PGO-TRAINING**, obtenha e instale o APK instrumentado. Ele pode rodar mais lentamente; **não** use esse APK para comparar FPS.
3. No celular, crie a pasta `TLOU-PGO` na raiz do armazenamento interno, concedendo ao aplicativo o acesso a arquivos solicitado.
4. Jogue por 10–15 minutos, incluindo a mesma cena pesada que usará como referência. Encerre **normalmente** o jogo. O perfil LLVM costuma ser gravado no encerramento normal do processo; **não há gravação periódica de 30 segundos implementada aqui**. Um travamento ou encerramento forçado pode impedir a gravação.
5. Verifique se surgiram arquivos `TLOU-PGO/tlou-*.profraw`. Sem esses arquivos, **não prossiga**: é preciso investigar a instrumentação ou as permissões.
6. Transfira os `.profraw` ao PC e, na raiz do repositório, execute:

   ```bash
   TLOU_NDK=/caminho/ndk/29.0.14206865 bash android/bionic/pgo_merge.sh /caminho/pasta-dos-profraw
   ```

7. Faça commit de `android/pgo/tlou.profdata` e push. Uma compilação normal (`TLOU_ANDROID_PGO=auto`) passa a utilizar o perfil.
8. Instale o **APK normal**, repita a mesma cena, resolução, driver e configurações e compare FPS e estabilidade com a referência (log 32: ~35,75 flips/s em 720p). Se piorar ou apresentar crash/artefatos, volte ao build `off` e retire o perfil até investigar.

## Cuidados

- Mescle usando a ferramenta `llvm-profdata` do **mesmo NDK** usado para gerar a instrumentação.
- Perfis ARM64 do Android não podem ser substituídos por perfis x86 do Windows.
- Alterações extensas no renderizador exigem novo treinamento.
- As fontes de fibras no Android ficam fora de PGO/LTO tanto em `generate` quanto em `use`, para proteger o código que troca manualmente de pilha.
- Modos: `auto` (padrão), `generate`, `use` e `off`. No workflow manual estão disponíveis `auto`, `generate` e `off`.
- O perfil fica em `android/pgo/tlou.profdata`, separado do diretório do port para PC.
