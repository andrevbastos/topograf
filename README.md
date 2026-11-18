# TopografDSL

Linguagem para compor ruídos Perlin e gerar terrenos, exportados em PNG ou exibidos em 3D.

## Construção do topograf

Dependências: CMake ≥ 3.20, compilador com C++20, Bison ≥ 3.8, Flex ≥ 2.6, OpenGL, GLFW e GLM.
O motor gráfico [ifcg](https://github.com/andrevbastos/ifcg) é baixado pelo CMake.

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build        # testes (opcional)
```

O executável fica em `build/topograf`.

## Compilar um programa

```sh
build/topograf programa.topo -o programa     # gera o executável ./programa
./programa                                   # exporta os PNGs e abre as janelas
./programa --no-display                      # só exporta
./programa --frames 120                      # fecha cada janela após 120 frames
```

O topograf traduz o programa para C++ e o compila com o compilador do sistema, ligando com
o runtime do topograf. Para ver o C++ gerado, sem compilar:

```sh
build/topograf programa.topo --emit-cpp
```

O compilador usado é o mesmo do build do topograf; a variável `CXX` o substitui.
O runtime do topograf é ligado estaticamente; o executável só precisa das bibliotecas do
sistema (GLFW, OpenGL). Para o `display`, ele procura os shaders do ifcg em `resources/` ao
lado do executável e, se não houver, em `build/resources`. Para usar o executável em outro
lugar sem o `build/`, copie `build/resources` para junto dele.

## Interpretar um programa

```sh
build/topograf programa.topo               # executa direto, sem gerar executável
build/topograf programa.topo --no-display  # executa sem abrir janelas
build/topograf programa.topo --check       # só valida e mostra os ruídos declarados
build/topograf programa.topo --ast         # só mostra a árvore sintática
```

Interpretado ou compilado, o programa produz exatamente os mesmos arquivos.

Os caminhos dos arquivos exportados são relativos ao diretório onde o comando é executado.
Cada `display` abre a sua própria janela, e todas abrem ao mesmo tempo. Para fechar uma
janela, use ESC.

## Exemplo

```
// declarações
terrain(width: 256, length: 256, height: 256);

// geradores
perlin A(frequency: 1, octaves: 6, seed: 28082004);
perlin B(frequency: 2, octaves: 5, seed: 28082004, exponent: 2);

// composição
noise grama = A * 0.7;
noise montanhas = above(B, 0.35) * 0.8;

// exibição e exportação
display grama#green, montanhas#white, (0.35)#cyan depth;
export montanhas as "resultados/montanhas.png";
export octaves B to "resultados/camadas/";
```

## Regras de escrita

### Estrutura

- Todo comando termina com `;`.
- Comentários vão de `//` até o fim da linha.
- O programa começa com exatamente um `terrain(...)`.
- Nomes de ruídos começam com letra ou `_` e não podem repetir nem coincidir com uma função
  ou palavra-chave (`terrain`, `perlin`, `noise`, `display`, `export`, `as`, `to`, `octaves`, `depth`).
- Um nome só pode ser usado depois de ser declarado.

### `terrain`

```
terrain(width: 256, length: 256);              // só exportação
terrain(width: 256, length: 256, height: 64);  // necessário para display
```

| Parâmetro | Obrigatório | Valor |
|---|---|---|
| `width` | sim | inteiro > 0 |
| `length` | sim | inteiro > 0 |
| `height` | só para `display` | inteiro > 0 (altura máxima, em voxels) |

Todos os ruídos usam essa mesma grade.

### Ruído primitivo: `perlin`

```
perlin A(frequency: 4, octaves: 3, seed: 42, persistence: 0.5);
perlin B(seed: 7);
perlin C(lacunarity: 2.5, amplitude: 1.2, exponent: 1.5, wave: 120);
```

| Parâmetro | Padrão | Valor |
|---|---|---|
| `frequency` | 4 | número > 0 |
| `octaves` | 3 | inteiro ≥ 1 |
| `seed` | 0 | inteiro ≥ 0 |
| `persistence` | 0.5 | número > 0 |
| `lacunarity` | 2 | número > 0 |
| `amplitude` | 1 | número > 0 |
| `exponent` | 1 | número > 0 |
| `wave` | 250 | inteiro > 0 |

- `frequency`: frequência da primeira oitava. Valores maiores geram detalhes menores.
- `octaves`: quantas camadas de ruído são somadas.
- `seed`: escolhe o padrão gerado. A mesma semente gera sempre o mesmo ruído.
- `persistence`: fator aplicado à amplitude de uma oitava para a seguinte.
- `lacunarity`: fator aplicado à frequência de uma oitava para a seguinte.
- `amplitude`: multiplica o ruído antes do corte em [-1, 1]. Acima de 1 aumenta o contraste e satura os extremos.
- `exponent`: o valor final é elevado a ele. Acima de 1 achata as partes baixas e realça os picos; abaixo de 1 faz o contrário.
- `wave`: divide as coordenadas da grade antes da amostragem. Valores maiores esticam o ruído.

É preciso informar ao menos um parâmetro. Os omitidos usam o valor padrão.
`width`, `length` e `height` não são aceitos aqui.

### Ruído composto: `noise`

```
noise C = max(A, B) * 0.5 + above(A, 0.3);
```

Um composto é sempre declarado com `noise` e uma expressão; um primitivo, com `perlin` e
parâmetros. Trocar um pelo outro (`perlin C = ...`, `noise A(...)`) é erro sintático.

Todo valor fica em [0, 1]. Cada operação é feita ponto a ponto e limitada a esse intervalo.

| Expressão | Resultado em cada ponto |
|---|---|
| `A + B` | `min(1, A + B)` |
| `A - B` | `max(0, A - B)` |
| `k * A` ou `A * k` | `k · A`, limitado a [0, 1] |
| `max(A, B)` | o maior dos dois |
| `min(A, B)` | o menor dos dois |
| `above(A, B)` | `A` se `A > B`, senão 0 |
| `below(A, B)` | `A` se `A < B`, senão 0 |
| `avg(A, B)` | `(A + B) / 2` |
| `mix(A, B, nA, nB)` | `(A·nA + B·nB) / (nA + nB)` |

- Um número pode ocupar o lugar de um ruído e vale como campo constante: `above(A, 0.3)`, `A + 0.2`.
- Em `*`, pelo menos um lado precisa ser um número literal. `A * B` é erro.
- Em `mix`, `nA` e `nB` precisam ser números literais e não podem ser ambos 0.
- Divisão (`/`) não é suportada.
- Não há sinal negativo: `-A` e `-1` são erro.
- `+` e `-` têm a mesma precedência, abaixo de `*`, e associam à esquerda. Parênteses mudam a ordem.

### `display`

```
display A;                                             // cinza
display A#red, B#0000FF;                               // uma camada por expressão
display A#green, above(A, 0.6)#white depth;
```

- Exige `height` no `terrain`.
- Cada camada é uma expressão, com cor opcional colada nela: `A#red` é válido, `A #red` é erro.
- Cores: `#RRGGBB` ou `red`, `green`, `blue`, `white`, `black`, `yellow`, `cyan`,
  `magenta`, `brown`, `gray`. Sem cor, a camada fica cinza.
- `depth` (opcional, no máximo uma vez) escurece as partes baixas de cada camada.
- Camadas posteriores ficam por cima onde coincidem.
- As janelas abrem depois de todos os `export`, não importa a ordem no programa.

### `export`

```
export C as "saida/c.png";          // PNG em cinza, width × length, 0 a 255
export octaves A to "camadas/";     // camadas/A_octave_0.png, A_octave_1.png, ...
```

- `export ... as` aceita qualquer ruído, primitivo ou composto.
- `export octaves` só aceita primitivos (`perlin`) e grava uma imagem por oitava.
- Os diretórios que faltarem são criados.

## Erros

Os erros mostram a linha, a coluna e o motivo. Um erro léxico ou sintático interrompe a
análise. Os erros semânticos são reportados todos de uma vez, e nesse caso o programa não é
executado.

```
prog.topo: linha 4, coluna 11: erro léxico: espaço antes da cor (a cor deve vir colada à expressão, ex.: A#red)
prog.topo: linha 3, coluna 13: erro semântico: '*' exige que ao menos um lado seja número literal (ruído × ruído não é suportado)
prog.topo: linha 5, coluna 1: erro semântico: display exige 'height' em terrain
2 erro(s) semântico(s)
```
