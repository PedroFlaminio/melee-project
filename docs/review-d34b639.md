# Revisao das implementacoes desde `d34b639`

Data da revisao: 22 de setembro de 2026  
Intervalo analisado: `d34b639^..e3ff7f50d` (inclui `d34b639`, 26 commits)  
Objetivo: revisar as alteracoes com o rigor aplicado a codigo entregue por uma
pessoa desenvolvedora junior, procurando erros de corretude, lacunas de teste,
regressoes de plataforma e oportunidades de simplificacao.

## Resumo executivo

O intervalo fez progresso real: adicionou o fluxo inicial de assets no Windows,
ampliou o renderer, criou diagnostico sistematico de estagios, materializou novos
tipos de dados e corrigiu varios erros de tamanho de ponteiro encontrados por
sanitizers. O build Linux e a suite existente passam.

Ainda assim, eu nao consideraria o conjunto pronto para entrega. Foram encontrados
quatro problemas de prioridade alta:

1. o renderer usa uma funcao OpenGL nova que nao passa pelo loader do Windows;
2. o combo de taxa de apresentacao le seis itens de arrays que possuem cinco;
3. o gerador de `yakumono_param` corrompe tres listas `s16` de Icicle Mountain;
4. a tela de selecao bloqueia dois estagios que a propria medicao atual considera
   confiaveis.

| Prioridade | Quantidade | Significado nesta revisao |
| --- | ---: | --- |
| P1 | 4 | Pode bloquear plataforma, causar acesso fora dos limites, corromper dados ou impedir funcionalidade comprovadamente operacional |
| P2 | 3 | Implementacao funcionalmente incompleta ou fluxo importante fragil |
| P3 | 2 | Qualidade de teste/documentacao e manutencao |

## Validacao executada

- `cmake --preset host-debug`: passou.
- `cmake --build --preset host-debug`: passou.
- `ctest --preset host-debug`: **27/27 passaram** em aproximadamente 202 s.
- `cmake --preset host-sanitize`: passou.
- `cmake --build --preset host-sanitize -j2`: passou.
- `ctest --preset host-sanitize`: 25/27 passaram diretamente. Os dois abortos
  (`melee-host-mario-data-asset` e `melee-host-link-data-asset`) foram do
  LeakSanitizer por o executor estar sob `ptrace`, sem relato de vazamento ou
  acesso invalido. Ambos passaram ao repetir com `ASAN_OPTIONS=detect_leaks=0`.
- Sweep direcionado, build debug, 60 frames e audio desligado: os StKinds 16 e
  20 entraram em partida, 2/2.
- Sweep direcionado sanitizado de Icicle Mountain, 60 frames: 3/3 entraram em
  partida. Isso nao invalida R03: o dado traduzido esta objetivamente errado e
  o caminho que o consome nao foi denunciado nesse horizonte curto.
- `git diff --check d34b639^..HEAD`: encontrou somente os dois espacos finais
  usados como quebra de linha Markdown em `docs/native_pc_port_plan.md`.

Limitacoes da validacao: esta maquina e Linux; portanto a conclusao de Windows em
R01 e uma analise estatica do contrato do loader/OpenGL. A interface ImGui nao foi
automatizada e nenhum teste atual percorre a sexta entrada incorreta de R02.

## Cobertura do intervalo

O diff acumulado tem 260 arquivos, 10.517 adicoes e 45.478 remocoes. Grande parte
das remocoes e mecanica (toolchain de decompilacao, runtime PowerPC e artefatos),
por isso a revisao separou limpeza estrutural de mudanca de comportamento. Todos
os 26 commits estao cobertos nos grupos abaixo:

| Area revisada | Commits | Resultado principal |
| --- | --- | --- |
| Windows, extracao, OpenGL e renderer | `d34b639df`, `5e63f0c52` | R01, R02, R05 e R06 |
| Remocao do build/decomp, runtime PowerPC, artefatos e reorganizacao documental | `8cd6be115`, `069d620ad`, `144762970`, `72c221316`, `3fdca09f1`, `57e3e71e6`, `c0abfe979` | Build host continuou verde; R09 nas instrucoes resultantes |
| Sweep, diagnostico, classificacao e bloqueio de estagios | `c93245df9`, `296635097`, `b4691c0a7`, `f1c26a498`, `36fa9d422`, `4cbf62027`, `45c8b4126`, `b457edfb6`, `c18f5ec3f`, `eb8930191`, `e3ff7f50d` | R04, R07 e R08 |
| Tradutores, layouts, arrays de animacao e correcoes de largura/tamanho host | `0c637820e`, `86a194399`, `de7d116eb`, `a0fec0719`, `731c2130e`, `960c38d90` | R03; demais correcoes compilaram e passaram nos testes exercitados |

Nao foi encontrado erro funcional causado apenas pelas remocoes do sistema de
decompilacao: os alvos host configuram, compilam e testam sem os arquivos
retirados. Tambem nao foram reabertos nesta revisao problemas anteriores que nao
foram alterados pelo intervalo; o foco foi regressao ou contrato novo/tocado.

## Achados

### R01 — P1 — `glUniform3fv` ignora o loader OpenGL do Windows

**Evidencia**

O commit `d34b639` criou um loader manual para funcoes posteriores ao OpenGL 1.1
em `port/src/render/sdl_gl_renderer.cpp:42-180`. Cada funcao precisa aparecer em
tres lugares: armazenamento `PFN...PROC`, chamada a `SDL_GL_GetProcAddress` e
macro que redireciona `glNome` para o ponteiro carregado.

O commit `5e63f0c52` passou a chamar `glUniform3fv` em
`port/src/render/sdl_gl_renderer.cpp:618`, mas essa funcao nao aparece em nenhum
dos tres blocos do loader. Ela e a unica chamada moderna atualmente usada nesse
arquivo que ficou fora da lista.

No Windows, `opengl32` expoe diretamente apenas a API OpenGL 1.1. Funcoes como
`glUniform3fv` precisam ser obtidas pelo endereco do contexto. Assim, essa chamada
vira uma referencia externa direta e pode impedir o link do `melee-pc`; caso uma
toolchain forneca uma resolucao diferente, ainda contorna a verificacao de
disponibilidade feita por `load_modern_gl_functions`.

**Correcao sugerida**

- Adicionar `PFNGLUNIFORM3FVPROC` ao armazenamento, ao carregamento e ao macro.
- Substituir as tres listas copiadas por uma unica X-macro ou adotar um loader
  consolidado. O desenho atual torna esse tipo de omissao provavel a cada nova
  chamada OpenGL.
- Fazer o configure falhar se `MELEE_HOST_BUILD_SDL_RENDERER=ON` e o renderer nao
  for realmente construido, e manter um job Windows que obrigatoriamente linke
  `melee-pc` com o renderer habilitado.

### R02 — P1 — combo do ImGui acessa o sexto elemento de arrays com cinco

**Evidencia**

Em `port/src/render/settings_ui.cpp:171-187`, `rates` e `rate_items` possuem cinco
elementos, mas a chamada e:

```cpp
ImGui::Combo("Presentation Rate", &rate_idx, rate_items, 6)
```

O erro foi introduzido em `5e63f0c52`: a opcao `Unlimited` foi removida dos dois
arrays e dos demais loops, mas o literal `6` ficou para tras. Ao abrir o combo, o
ImGui pode ler `rate_items[5]`. Se a entrada fantasma for selecionada, a linha
seguinte tambem le `rates[5]`. Ambos sao comportamento indefinido.

Um valor de taxa corrompido ainda chega ao calculo de `frame_ns` em
`port/src/render/sdl_gl_renderer.cpp:2466-2469`; valor zero causaria divisao por
zero.

**Correcao sugerida**

- Correcao minima: trocar `6` por `std::size(rate_items)`.
- Correcao robusta: usar um unico `std::array` de pares `{PresentationRate,
  label}` e derivar dele combo, busca e navegacao. Nao manter valor, label e
  tamanho em estruturas paralelas.
- Adicionar um teste da UI que abra o combo e selecione cada entrada sob ASan.
  Os testes de `play_window.hpp` nao compilam nem exercitam este widget.

### R03 — P1 — o gerador trata listas `s16*` como streams de comandos de 32 bits

**Evidencia**

`port/tools/gen_yakumono_layout.py:260-268` assume que todo ponteiro de todos os
`yakumono_param` e um script de animacao de cor. Ao encontrar um ponteiro, o
parser descarta o tipo apontado e grava `ctype="void*"`.

Isso nao e verdade para `grIceMt_YakumonoParam`. Em
`src/melee/gr/gricemt.c:115-117`, os tres campos sao:

```c
s16* field_ixs;
s16* xB0;
s16* xB4;
```

O codigo gerado em `port/src/game/yakumono_param.c.inc:1282-1297` materializa os
tres com `melee_host_hsd_reader_command_stream`. Esse materializador converte
palavras big-endian de quatro bytes. Uma lista de `s16` precisa converter cada
elemento de dois bytes independentemente.

O proprio `assets-local/GrIm.dat` confirma a diferenca:

- `field_ixs`, alvo `data+0x87508`, comeca em disco com `0, 1, 2, 3, 4`;
- a conversao por word transforma os bytes para que o `s16*` host observe
  `1, 0, 3, 2, ...`;
- `xB0` e `xB4` sofrem a mesma troca por pares.

Esses valores sao indexados diretamente em `src/melee/gr/gricemt.c:446-527` e
`1435-1437`. Portanto o problema e de corretude mesmo quando o estagio consegue
entrar em uma partida curta. O teste `--check` do gerador somente prova que o
arquivo gerado esta sincronizado com a logica do gerador; ele nao prova que a
logica escolheu o materializador correto.

**Correcao sugerida**

- Preservar no AST do gerador tanto o tipo do ponteiro quanto o tipo apontado.
- Tornar a estrategia explicita por campo/tipo: `command_stream`, `s16_array`,
  tabela de ponteiros etc. Um tipo desconhecido deve falhar na geracao, nunca cair
  silenciosamente em `command_stream`.
- Criar um materializador limitado para arrays big-endian de `s16`. O limite pode
  vir da proxima fronteira de relocacao/simbolo e, onde aplicavel, do sentinela
  conhecido pelo consumidor.
- Adicionar um teste de fixture de `GrIm.dat` que confira ao menos
  `field_ixs[0..4] == {0,1,2,3,4}` e valores conhecidos de `xB0/xB4`, alem de
  executar o estagio por tempo suficiente para atingir seus hazards.

### R04 — P1 — allowlist bloqueia dois estagios que a medicao atual aprova

**Evidencia**

`port/src/game/game_tables.c:256-280` ainda declara a medicao de 21 de setembro,
15 estagios, e nao inclui:

- StKind 16, Yoshi's Island;
- StKind 20, Mushroom Kingdom II.

`docs/project-progress.md:91-96` e `docs/native_port_status.md:599-601` dizem que
17 estagios sao confiaveis e incluem ambos. A revisao tambem executou:

```text
ok   16 Yoshi's Island           ok
ok   20 Mushroom Kingdom II      ok
2 of 2 stages entered a match
```

Na interface real, `src/melee/mn/mnstagesel.c:147-158` chama
`melee_host_stage_is_playable` e recusa a selecao antes da partida. Logo os dois
estagios funcionam pelo caminho forcado do sweep, mas continuam indisponiveis ao
jogador.

**Correcao sugerida**

- Incluir imediatamente 16 e 20, depois de repetir o criterio completo usado
  para declarar estabilidade.
- Nao manter a medicao manualmente em codigo e em varios documentos. Gerar a
  allowlist de um artefato versionado e revisavel, por exemplo
  `port/data/stage-status.json`.
- Adicionar teste unitario para todos os StKinds esperados, inclusive uma lista
  negativa, em vez de deixar a funcao sem teste direto.

### R05 — P2 — suporte a texturas indiretas captura estados que nao implementa

**Evidencia**

`GXSetTevIndirect` grava `matrix`, `unmodified_lod` e `alpha_select` em
`port/src/gx/state_recorder.cpp:879-901`. O recorder tambem compara esses campos,
entao eles participam da identidade do estado.

Porem, CPU e shader so distinguem as matrizes numericas 1 a 3:

- `port/src/gx/tev.cpp:345-347` trata qualquer outro ID como offset cru;
- `port/src/gx/tev.cpp:789-799` faz o mesmo no GLSL;
- `GX_ITM_OFF`, `GX_ITM_S0..S2` e `GX_ITM_T0..T2` acabam compartilhando o mesmo
  comportamento, embora sejam seletores distintos do GX;
- `unmodified_lod` e `alpha_select` nao sao consumidos por `tev.cpp`.

Os testes de geracao em `port/tests/gx_tev_test.cpp:461-492` percorrem somente
`GX_ITM_OFF` e `GX_ITM_0..2`. Eles verificam delimitadores balanceados do texto
GLSL, nao compilacao nem equivalencia de pixels. O proprio SDK usa seletores S/T
em `GXSetTevIndBumpST` (`extern/dolphin/src/dolphin/gx/GXBump.c:277-306`).

O caminho de refracao hoje encontrado no jogo usa `GX_ITM_0`, LOD falso e alpha
desligado, portanto esta lacuna nao invalida esse caso especifico; ela invalida a
afirmacao mais ampla de suporte ao estado GX gravado.

**Correcao sugerida**

- Implementar separadamente `OFF`, matrizes normais e matrizes S/T, alem da
  selecao de alpha e da regra de LOD.
- Enquanto isso, rejeitar ou diagnosticar combinacoes ainda nao suportadas em
  vez de renderiza-las como se fossem equivalentes.
- Criar testes tabelados para todos os IDs e flags, comparando referencia CPU e
  shader compilado; teste de string balanceada e insuficiente para semantica GLSL.

### R06 — P2 — o fluxo de primeiro uso do Windows depende da arvore de fontes

**Evidencia**

O README afirma que, no primeiro uso, o port extrai os assets. Entretanto,
`port/src/windows_asset_setup.cpp:282-297` procura
`tools/melee_extract.py` nos ancestrais do diretorio atual/executavel. Se nao
achar, retorna o diretorio do executavel. Depois,
`port/src/windows_asset_setup.cpp:143-161` exige um `python.exe` instalado e
monta o caminho para o script sem verificar que ele existe.

Isso funciona em um checkout de desenvolvimento, pois o executavel esta sob a
raiz do repositorio. Um executavel copiado para outra pasta, um ZIP ou um futuro
instalador nao tera `tools/melee_extract.py` e falhara na primeira execucao.
`has_local_assets` (`:300-305`) tambem verifica apenas a existencia de tres
arquivos, sem validar schema/hash do manifesto.

**Correcao sugerida**

- Definir explicitamente se esse fluxo e apenas de desenvolvimento. Se for,
  ajustar a mensagem/README.
- Para distribuicao, empacotar o extrator e seu runtime, ou mover a extracao
  para uma biblioteca nativa usada pelo executavel.
- Verificar a existencia do extrator antes de abrir o seletor/processo e mostrar
  erro acionavel.
- Guardar assets e logs em um diretorio de dados do usuario, nao ao lado de uma
  instalacao potencialmente sem permissao de escrita.
- Validar `schema_version`, identificacao do jogo e hash de `main.dol` antes de
  aceitar assets existentes.

### R07 — P2 — resultados de estagio nao possuem uma fonte de verdade unica

**Evidencia**

O mesmo resultado e copiado para codigo, tabela-resumo, texto detalhado e log de
progresso. Eles ja divergiram:

- a allowlist tem 15, enquanto os documentos dizem 17 (R04);
- `docs/project-progress.md:93` inclui 16 e 20 entre os que carregam, mas
  `:96` tambem os inclui entre os que quebram;
- `docs/project-progress.md:94-95` ainda diz que Castle/Icicle tem layout errado
  e que `image_desc` nao possui tradutor, apesar dos commits posteriores
  `a0fec0719` e `960c38d90`;
- `docs/native_port_status.md:599-605` repete essas informacoes antigas.

Isso nao e apenas cosmetico: a mesma duplicacao ja deixou o comportamento da
tela de selecao atrasado em relacao ao estado medido.

**Correcao sugerida**

- Versionar a saida consolidada do sweep com metadados: commit/binario, modo de
  build, audio, ASLR, numero de repeticoes, frames e status por StKind.
- Gerar desse arquivo a allowlist C e a tabela resumida da documentacao.
- Manter diagnosticos longos em uma secao historica, sem repeti-los como estado
  atual.
- Fazer `--check` no CI para impedir que artefatos gerados fiquem desatualizados.

### R08 — P3 — `--repeat` nao mede todas as repeticoes solicitadas

**Evidencia**

Em `port/tools/sweep_stages.py:160-170`, o loop interrompe no primeiro resultado
diferente de `ok`. Isso e suficiente para a regra binaria “so conta se todas
passarem”, mas nao mede confiabilidade: `--repeat 10` pode produzir `[0/1]`, nao
`[0/10]`. O proprio trabalho deste intervalo usa razoes como 2/3, 4/4 e 2/6 para
diagnosticar comportamento dependente de endereco.

O criterio padrao tambem observa somente 240 frames depois de entrar na cena.
Hazards tardios — incluindo os consumidores de R03 — podem nao ser alcancados.

**Melhoria sugerida**

- Separar `--fail-fast` de `--repeat`; sem `--fail-fast`, executar exatamente N.
- Registrar `requested_attempts` e `completed_attempts` separadamente.
- Permitir perfis de duracao (entrada, 10 s, partida longa/hazards) e nao chamar
  simplesmente de “reliably” o que so sobreviveu a entrada curta.
- Salvar todos os resultados das tentativas, nao apenas a primeira falha no
  relatorio principal.

### R09 — P3 — instrucoes de build ficaram inconsistentes

**Evidencia**

- `README.md:33` pede SDL2, mas o CMake usa e baixa SDL3.
- A secao release em `README.md:42-46` manda apenas executar
  `cmake --build --preset host-release`; em uma arvore nova falta antes
  `cmake --preset host-release`.

**Correcao sugerida**

- Trocar SDL2 por SDL3 e explicar que o CMake pode obter a dependencia quando
  ela nao estiver instalada.
- Mostrar configure, build e execucao completos para debug e release.
- Executar os blocos de comandos do README em um job limpo de CI ou em um teste
  simples de documentacao.

## Ordem recomendada de correcao

1. Corrigir R01 e R02 antes de qualquer entrega Windows/GUI.
2. Corrigir R03 e criar a fixture semantica de `GrIm.dat`; depois repetir Icicle
   Mountain por mais frames e com hazards ativos.
3. Atualizar/gerar a allowlist (R04) a partir de uma fonte unica (R07).
4. Delimitar ou completar o suporte GX indireto (R05).
5. Definir o contrato de distribuicao do extrator (R06).
6. Melhorar a medicao e a documentacao (R08 e R09).

## Pontos positivos que devem ser preservados

- O uso de ASan/UBSan para descobrir erros de tamanho de ponteiro foi eficaz e
  produziu correcoes concretas.
- Os checks de extensao e geracao de layout falham de forma explicita, em vez de
  aceitar silenciosamente um bloco incompatível.
- O sweep por estagio transformou relatos manuais em casos reproduziveis.
- A recusa antecipada de estagios desconhecidos e uma protecao razoavel enquanto
  o engine nao consegue desfazer um carregamento parcial; o problema e somente a
  lista ter ficado desatualizada.
- Os arquivos extraidos continuam fora do repositorio e a extracao valida a
  revisao suportada do disco antes de copiar os dados.

## Criterio de aceite sugerido para a proxima rodada

- Windows: `melee-pc` linka com renderer habilitado e abre um contexto OpenGL.
- UI: todas as opcoes do menu sao percorridas sob ASan sem acesso invalido.
- Icicle Mountain: fixture comprova as tres listas `s16` e uma execucao longa
  passa sob sanitizers.
- Stage select: 16 e 20 podem ser escolhidos pela interface real; allowlist e
  docs sao gerados da mesma entrada.
- GX indireto: combinacoes nao implementadas falham explicitamente ou possuem
  testes de equivalencia CPU/GPU.
- Build limpo Linux: 27/27; sanitizer: 27/27 com politica de LeakSanitizer
  adequada ao ambiente de execucao.
