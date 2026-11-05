# Handoff: linguagem de geração de terreno (DSL em Flex/Bison)

## 1. Objetivo

Implementar uma pequena linguagem para compor campos de ruído Perlin e produzir terrenos. O compilador lê um programa, constrói uma árvore sintática, valida tipos e avalia a árvore numa grade. O resultado é mostrado no motor gráfico (`display`) ou gravado em PNG (`exportar`).

Projeto da disciplina de compiladores. Entrega até o fim de novembro de 2026. Precisa de base teórica e justificativa lógica do design.

## 2. Escopo

Dentro do escopo:

- Léxico em Flex, gramática em Bison.
- AST, análise semântica (tipos e símbolos), avaliação por grade.
- Ruído Perlin já existente no TCC. Não usar Simplex nem outros geradores.
- Operações ponto a ponto descritas abaixo.
- `display` com cor por camada e flag `profundidade`.
- `exportar` para PNG e para oitavas.

Fora do escopo por enquanto:

- Multiplicação e divisão entre dois ruídos.
- Porcentagem como operador.
- Pesos variáveis (ruído como peso).
- Cor como atributo de ruído. A cor existe só no `display`.
- Paleta de cor na exportação PNG.

## 3. Tipos

| Tipo | Origem | Oitavas | Observação |
|---|---|---|---|
| `ruido` primitivo | `ruido NOME(params);` | Sim | Gerador Perlin com parâmetros |
| `ruido` composto | `ruido NOME = expr;` | Não | Guarda a sub-árvore, avaliado por amostragem |
| número | literal | — | Só em operações com ruído, ou como argumento numérico de `mistura` |

Todo valor de ruído fica em [0, 1]. O Perlin já é normalizado nesse intervalo, então não há remapeamento.

### 3.1 Cabeçalho `terreno`

Todo programa começa com exatamente um comando `terreno`. Ele define a grade de todos os ruídos do programa:

```
terreno(width: 256, length: 256);              // obrigatório
terreno(width: 256, length: 256, height: 64);  // com height, para 3D
```

- `width` e `length` são obrigatórios e inteiros positivos. Definem a grade N × M de todos os ruídos.
- `height` é opcional. Define a altura máxima do volume, em voxels. Só é usado na conversão para 3D (seção 3.2).
- Todos os ruídos compartilham a mesma grade. Operações entre ruídos sempre têm dimensões iguais, e não existe operação entre tamanhos diferentes.
- `terreno` precisa ser o primeiro comando e aparecer uma única vez. A gramática rejeita `terreno` fora do início.
- `ruido` primitivo não recebe `width`, `length` nem `height` como parâmetros.

### 3.2 Height e conversão para voxels

O valor do ruído fica em [0, 1]. Para virar um volume 3D, ele é escalado pelo `height`:

```
H_real(i, j) = H(i, j) × height
```

Cada coluna (i, j) é preenchida de y = 0 até y = H_real(i, j):

```
para cada (i, j):
    para y de 0 até floor(H_real(i, j)):
        voxel(i, y, j) = preenchido
```

- `H(i, j)` é o valor da grade em [0, 1], e `height` é o limite vertical em voxels.
- O resultado é uma grade 3D de ocupação `width` × `height` × `length`.
- Discretização proposta: `floor`, ou seja, a coluna preenche apenas os voxels inteiros abaixo de `H_real`.
- A malha a partir da grade de voxels (Marching Cubes) fica fora desta versão. Esta versão entrega só a grade de ocupação.

Regras semânticas:

- `exportar oitavas X` exige que X seja primitivo. Composto gera erro.
- Nomes precisam ser declarados antes do uso.
- Parâmetros de `ruido(...)` precisam existir e ter valor numérico válido.

## 4. Operações

Todas são ponto a ponto: para cada (i, j) da grade, calculam um valor a partir dos valores de cada operando naquele ponto.

Notação: `A(i,j)` é o valor do ruído A no ponto (i, j).

### 4.1 Soma e subtração (literais, com clamp)

```
A + B   →  min(1, A(i,j) + B(i,j))
A - B   →  max(0, A(i,j) - B(i,j))
```

### 4.2 Multiplicação por escalar

Só escalar vezes ruído. Ruído vezes ruído e divisão não entram.

```
k * A   →  clamp(k * A(i,j), 0, 1)
```

Exemplo: `0.5 * detalhe`.

### 4.3 Máximo e mínimo

```
max(A, B)  →  maior entre A(i,j) e B(i,j)
min(A, B)  →  menor entre A(i,j) e B(i,j)
```

### 4.4 Acima e abaixo (a ordem importa)

Seleção pela altura. O valor de A sobrevive no ponto onde a condição vale; senão o ponto vira zero.

```
acima(A, B)   →  A(i,j)  se A(i,j) > B(i,j)   senão  0
abaixo(A, B)  →  A(i,j)  se A(i,j) < B(i,j)   senão  0
```

O valor vem sempre de A. B serve só como limite. Trocar a ordem muda o resultado.

### 4.5 Média

```
media(A, B)  →  (A(i,j) + B(i,j)) / 2
```

### 4.6 Mistura com pesos numéricos

```
mistura(A, B, nA, nB)  →  (A(i,j) · nA + B(i,j) · nB) / (nA + nB)
```

- `nA` e `nB` são números literais, não ruídos.
- O denominador é a soma dos pesos. Logo `mistura(A, B, 30, 70)` é igual a `mistura(A, B, 3, 7)`.
- `nA + nB = 0` é erro semântico.
- `media(A, B)` equivale a `mistura(A, B, 1, 1)`.

## 5. Sintaxe

### 5.1 Declarações

```
ruido montanhao(escala: 0.5, oitavas: 2, semente: 1);   // primitivo
ruido cadeia = acima(base, 0.3);                        // composto
```

Parâmetros nomeados do primitivo: `escala`, `oitavas`, `semente`, `persistencia` (opcional, se o gerador TCC expuser).

### 5.2 Display

```
display A;                                    // cinza
display A#red;                                // A em vermelho
display A#red, B#blue;                        // duas camadas
display A#red, B#blue, N#green profundidade;  // com sombreamento por profundidade
```

- Sem espaço entre o nome e o `#`.
- Camadas separadas por vírgula, com espaço depois.
- Cores nomeadas: `red`, `green`, `blue` (lista a definir). Também aceita `#RRGGBB`.
- Cada camada contribui com `altura(i,j) × cor` em RGB. As camadas são somadas e o resultado é limitado a 1.
- Sem cor, o valor vai para os três canais (cinza).
- `display` exige `height` no cabeçalho `terreno`. Sem ele, é erro semântico.
- `profundidade` é flag exclusiva do `display`. Escurece as regiões baixas de cada camada usando `cor × (k + (1 − k) × h)`, em que `h` é a altura e `k` é um parâmetro (valor padrão sugerido: 0,3).

### 5.3 Exportação

```
exportar final como "terreno.png";        // grade de 0–1 mapeada para 0–255, cinza
exportar oitavas montanhao em "camadas/"; // uma imagem por oitava, só primitivos
```

### 5.4 Saídas e `height`

- `display` exige `height` no cabeçalho `terreno`.
- `exportar ... como` e `exportar oitavas` não exigem `height`. Gravam a grade `width` × `length`.

## 6. Gramática (esboço para o Bison)

```
programa : cabecalho comandos ;
cabecalho : TERRENO '(' params ')' ';' ;
comandos : comandos comando | ;
comando  : decl_primitivo
         | decl_composto
         | DISPLAY camadas opcoes ';'
         | EXPORTAR IDENT COMO STRING ';'
         | EXPORTAR OITAVAS IDENT EM STRING ';' ;

decl_primitivo : RUIDO IDENT '(' params ')' ';' ;
params         : param | params ',' param ;
param          : IDENT ':' NUMERO ;

decl_composto  : RUIDO IDENT '=' expr ';' ;

expr    : expr '+' expr
        | expr '-' expr
        | NUMERO '*' expr
        | expr '*' NUMERO
        | '(' expr ')'
        | NUMERO
        | IDENT
        | IDENT '(' args ')' ;
args    : expr | args ',' expr ;

camadas : camada | camadas ',' camada ;
camada  : expr opt_cor ;
opt_cor : /* vazio */ | COR ;
opcoes  : /* vazio */ | opcoes PROFUNDIDADE ;

%left '+' '-'
%left '*'
%right NEG
```

Notas:

- `NUMERO '*' expr` e `expr '*' NUMERO` cobrem escalar vezes ruído. Ruído vezes ruído é erro semântico.
- Funções embutidas (`max`, `min`, `acima`, `abaixo`, `media`, `mistura`) entram como `IDENT '(' args ')'`. A análise semântica confere aridade e tipos. Assim a gramática não cresce a cada função.
- Argumentos de `mistura` nA e nB precisam ser literais. A análise semântica confere.

Tokens do Flex:

```
COR          "#"[0-9A-Fa-f]{6} | "#"(red|green|blue)
PROFUNDIDADE "profundidade"
TERRENO      "terreno"
RUIDO        "ruido"
DISPLAY      "display"
EXPORTAR     "exportar"
COMO         "como"
EM           "em"
OITAVAS      "oitavas"
NUMERO       [0-9]+("."[0-9]+)?
IDENT        [A-Za-z_][A-Za-z0-9_]*
STRING       \"[^\"]*\"
```

Comentários `//` até o fim da linha são descartados. Espaços e quebras de linha são ignorados.

## 7. Pipeline

1. **Léxico e parser (Flex/Bison)** constroem a AST. Cada nó guarda tipo, operador e filhos.
2. **Análise semântica** percorre a AST e:
   - resolve nomes de ruídos e funções embutidas;
   - confere aridade e tipos;
   - rejeita ruído × ruído e divisão;
   - rejeita `exportar oitavas` sobre composto;
   - rejeita `nA + nB = 0` em `mistura`.
3. **Avaliação** amostra a AST numa grade `width` × `length` e produz um vetor de floats em [0, 1]. Primitivos chamam o gerador Perlin do TCC. Operações combinam os valores dos filhos conforme a seção 4.
4. **Ações de saída:**
   - `display`: envia a grade e as camadas ao motor gráfico, que abre a janela.
   - `exportar ... como`: converte a grade para 0–255 em cinza e grava PNG.
   - `exportar oitavas`: para cada oitava do primitivo, grava um PNG separado.

## 8. Exemplo completo

```
terreno(width: 256, length: 256, height: 64);

// geradores
ruido montanhao(escala: 0.5, oitavas: 2, semente: 1);
ruido cadeia(escala: 4.0, oitavas: 6, semente: 42);
ruido planicie(escala: 8.0, oitavas: 3, semente: 7);

// combinações
ruido anel  = cadeia - montanhao;            // clamp em 0: cordilheira sem o centro
ruido final = anel + 0.5 * planicie;         // soma com clamp em 1

// saída
display final#6B8E23 profundidade;
exportar final como "terreno.png";
exportar oitavas montanhao em "camadas/";
```

Exemplo de operações em 2 pontos, com A = [0,2; 0,8] e B = [0,5; 0,5]:

| Operação | Resultado |
|---|---|
| `A + B` | [0,7; 1,0] |
| `A - B` | [0,0; 0,3] |
| `max(A, B)` | [0,5; 0,8] |
| `min(A, B)` | [0,2; 0,5] |
| `acima(A, B)` | [0,0; 0,8] |
| `abaixo(A, B)` | [0,2; 0,0] |
| `media(A, B)` | [0,35; 0,65] |
| `mistura(A, B, 30, 70)` | [0,41; 0,59] |

Programas que devem ser rejeitados:

```
ruido x = montanhao * cadeia;        // ruído × ruído
ruido y = montanhao / cadeia;        // divisão
exportar oitavas anel em "x/";       // anel é composto
ruido z = mistura(A, B, 0, 0);       // soma dos pesos zero
terreno(width: 256, length: 256);    // seguido de display final: erro, falta height
terreno(width: 0, length: 256);      // width precisa ser inteiro positivo
ruido w = montanhao;                 // terreno precisa vir antes de qualquer ruído
terreno(width: 256, length: 256);
```

## 9. Critérios de aceitação

- Os exemplos da seção 8 compilam e produzem o resultado descrito.
- Os programas da seção 8 que devem ser rejeitados geram erro semântico com linha e motivo.
- `display` abre a janela do motor com a grade e as cores das camadas.
- `exportar ... como` gera PNG em escala de cinza, valores 0–255.
- `exportar oitavas` gera uma imagem por oitava, só para primitivos.
- Programas sem `terreno`, com `terreno` fora do início ou com `width`/`length` inválidos geram erro.
- `display` sem `height` no cabeçalho gera erro semântico com linha e motivo.
- A gramática passa em Bison sem conflitos não resolvidos.

## 10. Pontos a definir

- Lista de cores nomeadas além de `red`, `green` e `blue`.
- Valor padrão de `k` no `profundidade`.
- Resolução padrão da grade.
- Parâmetro `persistencia` do Perlin, se o gerador do TCC expuser.
- Confirmar a discretização (`floor` ou arredondamento) da altura de cada coluna.
