# Levantamento de requisitos: linguagem de geração de terreno

Base: `handoff-linguagem-terreno.md` e análise do diretório em 04/11/2026.
Prazo: fim de novembro de 2026 (cerca de 4 semanas).

## 1. Estado atual do diretório (verificado)

| Item | Situação |
|---|---|
| `src/main.cpp` | Apenas "hello world". Nada da linguagem implementado. |
| `src/CMakeLists.txt` | Referencia `terrain_viewer.cpp` e `stb_impl.cpp`, que não existem aqui (estão em `~/projects/tcc`). Build quebrado. |
| `cmake/Dependencies.cmake` | Só `ifcg`. `util.hpp` inclui `<graph/...>` (`andrevbastos/graph`), não declarado. |
| `build/` | Resto de configuração antiga, sem `_deps`. Não serve de referência. |
| Ferramentas | flex 2.6.4 e bison 3.8.2 instalados. |
| `include/core/noise_gen.hpp` | `perlin2D::generateNoiseMap`, `generateOctaveMaps`, `saveNoiseAsPNG`. Suficiente para a DSL. |
| `include/core/util.hpp` | Contém `getMarchingCubeData(noise, width, height, depth, Color)` com sombreamento por altura (`dim = 0.35 + 0.65·t`). |
| `include/core/search.hpp`, `statistics.hpp` | Experimentos do TCC. Não usados pela DSL. |
| `include/transvoxel/transvoxel.hpp` | Tabelas usadas por `getMarchingCubeData`. |

### 1.1 Problemas no código reaproveitado

1. `noise_gen.hpp` não tem `#pragma once` e as funções não são `inline`. Incluir em mais de um `.cpp` quebra o link (ODR). O mesmo vale para `createGrid2_5D` em `util.hpp`.
2. `saveNoiseAsPNG("terreno.png", ...)` lança exceção: `create_directories("")` gera `filesystem error: cannot create directories: Invalid argument []` (testado).
3. Nomes de eixo divergentes: `NoiseConfig.height` = `length` da DSL; `height` da DSL = `intensity`/`height` de `getMarchingCubeData`.
4. O Perlin 2D bruto fica em ±~0,707. Com 1 oitava o valor final fica em ≈[0,15; 0,85], não em [0, 1] inteiro. Limiares como `acima(A, 0.3)` devem ser lidos com isso em mente. Registrar no relatório.

## 2. Decisões tomadas

| # | Decisão |
|---|---|
| D1 | `display` multicamada: cada camada vira uma malha própria via `getMarchingCubeData`, com sua cor, e as malhas são sobrepostas na mesma cena. |
| D2 | Correções de gramática aceitas (seção 4). |
| D3 | Remover do projeto o que não for usado. Manter só o necessário de `noise_gen.hpp`, `getMarchingCubeData` (em header próprio) e `transvoxel.hpp`. Sai `search.hpp`, `statistics.hpp`, o restante de `util.hpp` e a dependência `graph`. |
| D4 | `exportar oitavas X em "dir/"` grava `dir/X_oitava_<i>.png`, com `i` a partir de 0. |
| D5 | Erros semânticos são acumulados. Todos são reportados ao final da análise, e a avaliação não roda se houver erro. |
| D6 | Cores nomeadas: `red, green, blue, white, black, yellow, cyan, magenta, brown, gray`, além de `#RRGGBB`. |
| D7 | Os parâmetros do primitivo correspondem aos campos de `NoiseConfig`: `frequencia` → `freq` (substitui `escala` do handoff), `oitavas` → `octaves`, `semente` → `seed`, `persistencia` → `pers`, `lacunaridade` → `lac`, `amplitude` → `amp`, `expoente` → `exp`, `onda` → `wave`. Todos são opcionais e os omitidos usam os padrões de `NoiseConfig`. Os exemplos do handoff devem trocar `escala:` por `frequencia:`. |
| D8 | Discretização e sombreamento seguem `getMarchingCubeData`: coluna `round(1 + h·(height − 1))`, `k ≈ 0,35`. A função ganha um parâmetro para ligar e desligar o sombreamento (flag `profundidade`). |
| D9 | Número em posição de ruído é promovido a campo constante (`acima(base, 0.3)`, `A + 0.3`). |
| D10 | Ordem de execução: todos os `exportar` primeiro, `display` por último (a janela bloqueia). |
| D11 | A cor fica colada à expressão: `A#red` é válido, `A #red` é erro léxico ("espaço antes da cor"). |
| D12 | `oitavas` é palavra-chave e também nome de parâmetro. A gramática usa `nome_param : IDENT \| OITAVAS`. |
| D13 | Lista de parâmetros com pelo menos 1 item. `ruido A();` é erro sintático. |
| D14 | "Literal numérico" (T6, T7) é só o nó `NUMERO` da AST, descontados parênteses: `(2)` é literal, `2*3` não é. |
| D15 | Cada `display` abre uma janela própria, todas ao mesmo tempo, cada uma num processo filho (`fork`). O `Engine` do ifcg é singleton e o GLFW exige a thread principal, então há uma janela por processo. O pai espera todas as janelas fecharem. |
| D16 | `profundidade` aparece no máximo uma vez por `display` (gramática `opt_prof`). Repetição é erro sintático. |
| D17 | Erro léxico ou sintático interrompe a análise no primeiro erro. Só os erros semânticos são acumulados (D5). |
| D18 | Bison com skeleton C++ (`%language "c++"`, `api.value.type variant`) e AST em `std::unique_ptr`. |
| D19 | Cada camada do `display` é deslocada 0,05 em y por índice (`translate`), contra z-fighting. Onde duas camadas coincidem, a posterior fica por cima. |
| D20 | O ifcg carrega shaders de `../resources/` relativo ao cwd. O processo do `display` troca o cwd para `<dir do executável>/resources` antes de abrir a janela. |
| D21 | Back-end: `perlang prog -o saida` traduz a AST para C++ e compila com o compilador do sistema, ligando com o runtime (`perlang_rtcore`, `perlang_display`, ifcg). Cada primitivo vira uma chamada a `generateNoiseMap`; cada composto vira um único laço sobre a grade com a expressão inteira embutida; cada camada de `display` vira uma lambda avaliada no processo da janela. Interpretador e código gerado usam as mesmas funções do runtime (`prt::add`, `prt::acima`, ...) e o código gerado é compilado com `-ffp-contract=off`, então os resultados são idênticos. Sem `-o`, o programa é interpretado. |

## 3. Requisitos funcionais

### 3.1 Léxico (RF-L)

- **L1.** Tokens: `TERRENO RUIDO DISPLAY EXPORTAR COMO EM OITAVAS PROFUNDIDADE NUMERO IDENT STRING COR` e `+ - * / ( ) , ; : =`.
- **L2.** Comentários `//` até o fim da linha e espaços são descartados.
- **L3.** `COR`: `#[0-9A-Fa-f]{6}` ou `#` seguido de um nome da lista D6. Deve vir imediatamente após o token anterior, sem espaço, quebra de linha ou comentário entre eles (D11). Implementação: o léxico guarda se o último trecho consumido foi espaço ou comentário; se `COR` vier depois de espaço, gera erro léxico com linha.
- **L4.** Caractere inválido gera erro léxico com linha.
- **L5.** `STRING` vai entre aspas duplas, sem escapes e sem quebra de linha. String sem fechamento gera erro léxico com linha.

### 3.2 Sintaxe (RF-S)

- **S1.** `terreno(...)` é obrigatório, único e primeiro. Outra posição é erro sintático.
- **S2.** Comandos: `ruido` primitivo, `ruido` composto, `display`, `exportar ... como`, `exportar oitavas ... em`.
- **S3.** Erros sintáticos com linha, coluna e motivo em português (`%define parse.error custom`, `%locations`). A análise para no primeiro erro (D17).
- **S4.** Gramática sem conflitos (`bison -Wall` limpo).

### 3.3 AST e análise semântica (RF-T)

- **T1.** Tabela de símbolos: nome, tipo (primitivo/composto), parâmetros, linha de declaração.
- **T2.** Nome usado antes de declarado gera erro.
- **T3.** Redeclaração, nome igual a função embutida e parâmetro repetido geram erro.
- **T4.** `terreno`: `width` e `length` obrigatórios, inteiros e > 0 ("inteiro" quer dizer sem parte fracionária: `8.0` é aceito, `8.5` é erro; vale também para T5). `height` opcional, inteiro e > 0. Parâmetro desconhecido é erro.
- **T5.** `ruido` primitivo: aceita apenas `frequencia`, `persistencia`, `lacunaridade`, `amplitude` e `expoente` (> 0), `oitavas` (inteiro ≥ 1), `semente` (inteiro ≥ 0) e `onda` (inteiro > 0). Os omitidos usam os padrões de `NoiseConfig`. `width`, `length` e `height` são proibidos.
- **T6.** Funções embutidas: `max`, `min`, `acima`, `abaixo` e `media` têm aridade 2. `mistura` tem aridade 4, com dois operandos de ruído e dois literais numéricos (D14).
- **T7.** `*` exige ao menos um lado que seja literal numérico (D14). Ruído × ruído é erro.
- **T8.** `/` é sempre erro semântico ("divisão não suportada").
- **T9.** `mistura` com `nA + nB = 0` é erro. Como `NUMERO` não tem sinal, só ocorre com `0, 0`.
- **T10.** `exportar oitavas` exige um primitivo.
- **T11.** `display` sem `height` no cabeçalho é erro.
- **T12.** Toda mensagem contém linha e motivo. Os erros são acumulados (D5).

### 3.4 Avaliação (RF-A)

- **A1.** Cada primitivo é gerado uma vez (`generateNoiseMap`) e guardado em cache por nome.
- **A2.** Composto é avaliado ponto a ponto conforme a seção 4 do handoff, com clamp em [0, 1].
- **A3.** A avaliação não depende do motor gráfico. Pode ser testada sem janela.

### 3.5 Saídas (RF-O)

- **O1.** `exportar X como "f.png"`: PNG em cinza 0–255, `width` × `length`, funciona com caminho sem diretório.
- **O2.** `exportar oitavas X em "dir/"`: `generateOctaveMaps` e um PNG por oitava (D4).
- **O3.** `display`: janela do ifcg com uma malha por camada (D1), cor da camada ou cinza se omitida, sombreamento só com `profundidade` (D8). Com vários `display`, uma janela por comando, todas abertas ao mesmo tempo (D15).

## 4. Requisitos não funcionais

- **N1.** C++20 e CMake com `find_package(BISON)` / `find_package(FLEX)`, `BISON_TARGET`, `FLEX_TARGET`, `ADD_FLEX_BISON_DEPENDENCY`.
- **N2.** CLI: `perlang <programa>` interpreta; `perlang <programa> -o saida` compila para um executável (D21); `--emit-cpp` imprime o C++ gerado. O executável gerado aceita `--no-display` e `--frames N`. A flag `--no-display` pula a janela (para testes). A flag `--ast` imprime a AST e encerra (testes de sintaxe). A flag `--check` roda a análise semântica, imprime a tabela de símbolos e encerra sem executar (testes semânticos). A flag `--frames N` fecha cada janela após N frames (verificação do display sem interação).
- **N3.** Suíte de testes (CTest; cada programa `.terreno` declara as saídas esperadas em linhas `// espera: <texto>`) com os programas válidos e inválidos da seção 8 do handoff, os valores da tabela de 2 pontos, e a comparação de PNGs e mensagens de erro.
- **N4.** Documento teórico: gramática em BNF, regras de tipo, justificativa do design e limitações (item 1.1.4).

## 5. Gramática revisada

```
programa : cabecalho comandos ;
cabecalho : TERRENO '(' params ')' ';' ;
comandos : comandos comando | ;
comando  : RUIDO IDENT '(' params ')' ';'
         | RUIDO IDENT '=' expr ';'
         | DISPLAY camadas opt_prof ';'
         | EXPORTAR IDENT COMO STRING ';'
         | EXPORTAR OITAVAS IDENT EM STRING ';' ;

params     : param | params ',' param ;
param      : nome_param ':' NUMERO ;
nome_param : IDENT | OITAVAS ;

expr : expr '+' expr
     | expr '-' expr
     | expr '*' expr        // semântica: um lado precisa ser número
     | expr '/' expr        // semântica: sempre erro
     | '(' expr ')'
     | NUMERO
     | IDENT
     | IDENT '(' args ')' ;
args : expr | args ',' expr ;

camadas : camada | camadas ',' camada ;
camada  : expr opt_cor ;
opt_cor : /* vazio */ | COR ;
opt_prof : /* vazio */ | PROFUNDIDADE ;

%left '+' '-'
%left '*' '/'
```

Mudanças em relação ao handoff:

- `NUMERO '*' expr | expr '*' NUMERO` foi trocado por `expr '*' expr`. A versão original gera 1 conflito shift/reduce não resolvido em `2 * 3` (testado com bison 3.8.2).
- Foi incluído `'/'` para que a divisão vire erro semântico, como a seção 8 espera, e não erro léxico.
- `%right NEG` foi removido porque não há menos unário.
- `nome_param` aceita `OITAVAS`, que é palavra-chave (D12).
- `opcoes` virou `opt_prof` (D16).

## 6. Ajustes nos critérios de aceitação

- O critério "programas rejeitados geram erro semântico" passa a ser "geram erro com linha e motivo". `terreno` fora do início é erro sintático.
- Os demais critérios da seção 9 do handoff continuam válidos.

## 7. Riscos e pontos em aberto

- **Z-fighting entre camadas (D1).** Resolvido com o deslocamento vertical por camada (D19). Não verificado visualmente com camadas coincidentes.
- O desempenho de várias malhas MC de 256 × 256 × `height` não foi medido.
- O efeito visual de valores extremos dos parâmetros do primitivo (`frequencia`, `persistencia`, `lacunaridade`, `amplitude`, `expoente`, `onda`) não foi validado.
- **Janelas simultâneas (D15).** Resolvido com um processo por `display`. Verificado: dois `display` abrem 3 processos (pai + 2 filhos) e o pai sai com 0; filhos encerrados por sinal fazem o pai sair com 1. A solução depende de `fork`, logo de POSIX.

## 8. Etapas propostas para o plano

1. Limpeza (D3) e build: CMake com Flex/Bison, `noise_gen.hpp` com `#pragma once` + `inline`, correção do `create_directories`.
2. Léxico + parser + AST, com testes de aceitação e rejeição sintática.
3. Análise semântica com erros acumulados.
4. Avaliação e `exportar` (PNG e oitavas), conferidos contra a tabela de 2 pontos.
5. `display` com ifcg (malhas por camada, flag `profundidade`).
6. Suíte de testes completa e documento teórico.
