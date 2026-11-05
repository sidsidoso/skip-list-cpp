# Skip List em C++

[![CI](https://github.com/sidsidoso/skip-list-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/sidsidoso/skip-list-cpp/actions/workflows/ci.yml)
![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
[![Licenca: MIT](https://img.shields.io/badge/licen%C3%A7a-MIT-green.svg)](LICENSE)

Implementação header-only de uma **skip list** genérica (`SkipList<Chave, Valor, Comparador>`) em C++17, funcionando como um mapa ordenado chave → valor no estilo do `std::map`. Os níveis são sorteados com **p = 1/2** por um **gerador próprio (SplitMix64) com semente configurável**, o que torna a estrutura totalmente **determinística** para a mesma sequência de operações. Tem altura dinâmica, inserção/atualização, remoção, busca, menor/maior, **iterador em ordem**, **busca por intervalo**, regra dos 5, destruição iterativa sem vazamentos, desenho dos níveis em ASCII e uma função `validar()` que confere todos os invariantes nos testes. Projeto da disciplina de Estrutura de Dados.

## Sumário

- [Conceito](#conceito)
- [Busca, inserção e remoção](#busca-inserção-e-remoção)
- [Por que O(log n) esperado](#por-que-olog-n-esperado)
- [Complexidade](#complexidade)
- [Skip list × árvores balanceadas](#skip-list--árvores-balanceadas)
- [Decisões de implementação e otimizações](#decisões-de-implementação-e-otimizações)
- [Estrutura de pastas](#estrutura-de-pastas)
- [Como compilar, executar e testar](#como-compilar-executar-e-testar)
- [Exemplo de saída](#exemplo-de-saída)
- [Referências](#referências)

## Conceito

Numa lista encadeada ordenada, buscar custa O(n): é preciso andar nó por nó. A skip list (Pugh, 1990) empilha **várias listas ordenadas**: o nível 0 tem todos os elementos e cada elemento também aparece nos níveis de cima com probabilidade **1/2, 1/4, 1/8, ...** (uma "moeda" por nível, sorteada uma única vez quando o elemento entra). Os níveis altos funcionam como **vias expressas** que pulam muitos nós de uma vez.

```
nivel 3: H ------------------------------------------------> 65 -------------------> NULL
nivel 2: H -> 5 -------------------------------------------> 65 -------------------> NULL
nivel 1: H -> 5 -------> 20 -------------------> 50 -> 60 -> 65 -------------> 90 -> NULL
nivel 0: H -> 5 -> 10 -> 20 -> 25 -> 30 -> 40 -> 50 -> 60 -> 65 -> 70 -> 80 -> 90 -> NULL
```

`H` é a cabeça (presente em todos os níveis). Cada nó guarda o par chave/valor e um vetor `prox` com um ponteiro **por nível em que aparece** — o 65 acima tem 4 ponteiros, o 10 tem só 1.

Invariante (conferido por `validar()`): **cada nível é uma lista estritamente ordenada e é uma subsequência do nível imediatamente abaixo**; um nó de altura `h` aparece exatamente nos níveis `0..h-1`.

## Busca, inserção e remoção

**Busca** (exemplo: procurar 60). Começa no nível mais alto da cabeça; em cada nível anda para a direita **enquanto a próxima chave for menor** que a procurada e, quando não pode mais avançar, desce um nível:

```
nivel 3: H ================================================> 65    (65 > 60: desce)
nivel 2: H => 5 ===========================================> 65    (65 > 60: desce)
nivel 1:      5 =======> 20 ===================> 50 => 60          (60 nao e < 60: desce)
nivel 0:                                         50 -> 60          proxima chave = 60: encontrado
```

**Inserção**: durante a descida, guarda-se em `atualizar[i]` o último nó com chave menor em cada nível `i` (o "predecessor" no nível). Se a chave já existe, só o valor é trocado. Senão, sorteia-se a altura `h` do novo nó e ele é encaixado logo depois de `atualizar[i]` nos níveis `0..h-1` — dois ponteiros por nível, nenhuma rotação:

```
 inserir 45 com altura 2:

 nivel 1:  ... 20 ------------------> 50 ...      ... 20 ------------> 45 -> 50 ...
 nivel 0:  ... 30 -> 40 -> 50 ...          ==>    ... 30 -> 40 -> 45 -> 50 ...
                      ^ atualizar[0]                  (atualizar[1] = 20)
```

**Remoção**: mesma descida; em cada nível onde o nó aparece, `atualizar[i]->prox[i] = no->prox[i]`. Depois a altura da lista diminui enquanto o nível mais alto estiver vazio.

**Busca por intervalo [a, b]**: uma descida até o primeiro elemento `>= a` (O(log n)) e depois o nível 0 é percorrido em ordem até passar de `b`: O(log n + k).

## Por que O(log n) esperado

- Um elemento chega ao nível `i` com probabilidade `1/2^i`, então o nível `i` tem em média `n/2^i` nós e a **altura esperada é ≈ log2(n)** (a demonstração mostra 1.000.000 de chaves com contagens por nível praticamente iguais a `n/2^i`).
- **Espaço**: em média `1 + 1/2 + 1/4 + ... = 2` ponteiros por nó, ou seja, O(n).
- **Busca**: olhando o caminho de trás para frente, em cada nível dá-se em média **no máximo 2 passos** para a direita antes de subir (argumento de Pugh), logo ~`2 · log2(n)` comparações.

O custo é **esperado** sobre as moedas, **não depende da ordem das chaves**: inserir 1, 2, 3, ... em ordem dá a mesma distribuição de alturas que chaves aleatórias. O pior caso (moedas muito azaradas) é O(n), mas com probabilidade desprezível.

## Complexidade

| Operação                           | Esperado      | Pior caso | Observação                                           |
|------------------------------------|---------------|-----------|------------------------------------------------------|
| `buscar` / `contem`                | O(log n)      | O(n)      | iterativo                                            |
| `inserir` (ou atualizar)           | O(log n)      | O(n)      | uma alocação; nenhum rebalanceamento                 |
| `remover`                          | O(log n)      | O(n)      | altura encolhe se o nível de cima esvaziar           |
| `menor`                            | O(1)          | O(1)      | primeiro nó do nível 0                               |
| `maior`                            | O(log n)      | O(n)      | desce pela "escada" mais à direita                   |
| `limite_inferior` / `limite_superior` | O(log n)   | O(n)      |                                                      |
| `intervalo(a, b)`                  | O(log n + k)  | O(n)      | k = elementos devolvidos                             |
| iterador `++`                      | O(1)          | O(1)      | segue `prox[0]`                                      |
| `tamanho` / `vazia` / `altura`     | O(1)          | O(1)      | contadores mantidos na estrutura                     |
| cópia                              | O(n)          | O(n)      | preserva a altura de cada nó (mesma forma)           |
| destruição / `limpar`              | O(n)          | O(n)      | iterativa pelo nível 0, memória extra O(1)           |
| movimento / `trocar`               | O(1)          | O(1)      | `noexcept`                                           |
| `validar` / `nos_por_nivel`        | O(n · altura) / O(n) | — | diagnóstico (testes e demo)                       |
| **Espaço**                         | O(n)          | O(n log n)| em média 2 ponteiros por nó                          |

## Skip list × árvores balanceadas

Comparação com a [árvore rubro-negra](https://github.com/sidsidoso/arvore-rubro-negra-cpp) do mesmo curso (e com o `std::map`, que é uma rubro-negra):

|                             | Skip list                                    | Rubro-negra / AVL                          |
|-----------------------------|----------------------------------------------|--------------------------------------------|
| Garantia                    | O(log n) **esperado** (aleatorizada)         | O(log n) **no pior caso**                  |
| Rebalanceamento             | nenhum: só religa ponteiros                  | rotações e recolorações                    |
| Código de inserção/remoção  | curto, sem casos especiais                   | vários casos simétricos                    |
| Ponteiros por nó            | 2 em média (variável)                        | 2 ou 3 fixos                               |
| Percurso em ordem           | trivial (nível 0 é uma lista)                | sucessor via pai ou pilha                  |
| Concorrência                | fácil de fazer sem travas (ex.: `ConcurrentSkipListMap` do Java) | difícil (rotações mexem em vários nós) |
| Usada em                    | Redis (sorted sets), LevelDB/RocksDB (memtable), Java | `std::map`, `TreeMap`, kernel Linux |

Na prática, como mostra a demonstração, o `std::map` e a skip list ficam na mesma ordem de grandeza; qual ganha depende da operação, do tamanho e da máquina (a skip list faz mais saltos de ponteiro por busca, a árvore faz rotações).

## Decisões de implementação e otimizações

- **Nó em uma única alocação**: o vetor `prox` (com exatamente `nivel` ponteiros) fica logo depois do par chave/valor, no mesmo bloco de memória (`operator new` alinhado + *placement new*). Evita uma segunda alocação e o *overhead* de um `std::vector` por nó, e melhora a localidade. O deslocamento do vetor é arredondado para o alinhamento de ponteiro.
- **Cabeça e nós tratados pelo mesmo tipo**: a busca guarda em `atualizar[i]` o **vetor `prox`** do predecessor (`No**`), que pode ser tanto o de um nó quanto o `std::array` da cabeça. Inserir e remover viram `atualizar[i][i] = ...`, sem nenhum caso especial para "inserir no começo".
- **Gerador próprio SplitMix64**: poucas operações aritméticas, estado de 64 bits, aceita qualquer semente. O nível sai de **um único número aleatório**: `1 + (zeros à direita nos bits)`, já que cada bit é uma moeda com p = 1/2. A semente vem no construtor (`SkipList s(2026)`) ou em `semear()`: mesma semente + mesmas operações = mesma estrutura (testado).
- **Altura dinâmica**: a altura em uso cresce conforme os sorteios e **encolhe na remoção** quando o nível de cima esvazia; buscas começam só do nível em uso, não de um máximo fixo. Além disso o nível de um nó novo é limitado a `altura atual + 1` ("fixar os dados", sugerido por Pugh), o que impede que uma lista pequena ganhe muitos níveis por azar. O máximo absoluto é 32 níveis (suficiente para ~4 bilhões de elementos).
- **Nó guarda `std::pair<const Chave, Valor>`**, como o `std::map`: o iterador devolve referência de verdade e `for (auto& [k, v] : lista)` permite alterar o valor mas não a chave.
- **Comparador como parâmetro de template** (padrão `std::less<Chave>`); só `<` é usado, nunca `==`. Com `std::greater` a lista fica em ordem decrescente.
- **Regra dos 5**: cópia profunda que preserva a altura de cada nó (O(n), anexando no fim de cada nível); atribuição por copia-e-troca (garantia forte); se a cópia de um valor lançar exceção no meio, a parte já copiada é liberada (testado com um tipo que conta instâncias vivas); movimento `noexcept` deixando a origem vazia e utilizável.
- **Destruição iterativa** pelo nível 0: O(n), sem recursão (testado com 1 milhão de nós).
- **`inserir` por valor + `std::move`** e atualização do valor quando a chave já existe (como `insert_or_assign`); funciona com valores somente-móveis (`std::unique_ptr`).
- **`validar()`** confere: nível 0 estritamente ordenado e com `tamanho()` nós; cada nível `i ≥ 1` é **exatamente** a subsequência dos nós de altura `> i` (logo subsequência ordenada do nível de baixo); nível mais alto não vazio; ponteiros da cabeça acima da altura nulos.

## Estrutura de pastas

```
skip-list-cpp/
├── include/
│   └── skip_list.hpp          # implementação (header-only)
├── src/
│   └── main.cpp               # demonstração + comparação com std::map
├── tests/
│   └── test_skip_list.cpp     # testes (assert + validar() + std::map como oráculo)
├── .github/workflows/ci.yml   # CI: testes com sanitizers + demo
├── Makefile
├── LICENSE
└── README.md
```

## Como compilar, executar e testar

### Linux / macOS (make)

```bash
make            # compila demo e testes em build/
make run        # executa a demonstração
make test       # executa os testes
make test SANITIZE=1   # testes com AddressSanitizer + UndefinedBehaviorSanitizer
make clean
```

### Windows (g++ do MinGW-w64 / WinLibs)

```bat
mkdir build
g++ -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude src/main.cpp -o build/demo.exe
g++ -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude tests/test_skip_list.cpp -o build/testes.exe
build\demo.exe
build\testes.exe
```

Com `mingw32-make` também funcionam `mingw32-make run` e `mingw32-make test` (o alvo `clean` usa `rm`, disponível no Git Bash).

### Uso

```cpp
#include "skip_list.hpp"

SkipList<std::string, int> estoque(42);   // semente 42: estrutura reproduzivel
estoque.inserir("caneta", 10);
estoque.inserir("lapis", 25);
estoque.inserir("caneta", 12);            // chave existente: atualiza
if (int* q = estoque.buscar("lapis")) --*q;
for (const auto& [item, qtd] : estoque.intervalo("a", "d")) std::cout << item << '=' << qtd << '\n';
estoque.remover("lapis");
assert(estoque.validar());
```

## Exemplo de saída

`make run` (`H` = cabeça; uma seta que passa por cima de um nó significa que ele não está naquele nível). Os tempos da comparação com `std::map` **variam por máquina** e de uma execução para outra:

```
== Skip list (p = 1/2, semente fixa 2026) ==
Apos inserir 30 10 50 20 70 40 60 80 5 90 25 65 (H = cabeca):
nivel 3: H ------------------------------------------------> 65 -------------------> NULL
nivel 2: H -> 5 -------------------------------------------> 65 -------------------> NULL
nivel 1: H -> 5 -------> 20 -------------------> 50 -> 60 -> 65 -------------> 90 -> NULL
nivel 0: H -> 5 -> 10 -> 20 -> 25 -> 30 -> 40 -> 50 -> 60 -> 65 -> 70 -> 80 -> 90 -> NULL
tamanho: 12, altura: 4, invariantes ok? true

buscar(40) -> v40
contem(45)? false
menor: 5, maior: 90
intervalo [22, 62]: 25=v25 30=v30 40=v40 50=v50 60=v60
inserir(40, "novo") atualiza: buscar(40) -> novo

Apos remover 50, 5 e 90:
nivel 3: H -------------------------------------> 65 -------------> NULL
nivel 2: H -------------------------------------> 65 -------------> NULL
nivel 1: H -------> 20 -------------------> 60 -> 65 -------------> NULL
nivel 0: H -> 10 -> 20 -> 25 -> 30 -> 40 -> 60 -> 65 -> 70 -> 80 -> NULL
em ordem: 10 20 25 30 40 60 65 70 80

n = 1000000: altura 18 (log2 n = 19.9), 2.001 ponteiros por no em media
  nivel   nos      esperado n/2^i
      0  1000000      1000000
      1   499724       500000
      2   250120       250000
      3   125448       125000
      4    62602        62500
      5    31341        31250
  ...

Comparacao com std::map (chaves aleatorias; tempos variam por maquina):
n = 100000 (100000 insercoes, 100000 buscas, 100000 remocoes)
                  inserir       buscar      remover
  SkipList        37.3 ms      36.1 ms      31.7 ms
  std::map        38.0 ms      38.5 ms      29.8 ms
  buscas com sucesso: 50003 (skip) / 50003 (map)
n = 1000000 (1000000 insercoes, 1000000 buscas, 1000000 remocoes)
                  inserir       buscar      remover
  SkipList       961.4 ms     722.3 ms     643.1 ms
  std::map       753.6 ms     844.1 ms     798.8 ms
  buscas com sucesso: 500427 (skip) / 500427 (map)
```

`make test`:

```
[ok] lista vazia
[ok] um elemento
[ok] inserir chave existente atualiza o valor
[ok] remover e altura dinamica
[ok] iterador em ordem, menor e maior
[ok] busca por intervalo (limites, vazio e unitario)
[ok] 200 mil operacoes aleatorias vs std::map
[ok] cada nivel e subsequencia ordenada do nivel abaixo
[ok] altura media ~ log2(n) e distribuicao geometrica
[ok] semente configuravel e determinismo
[ok] copia e movimento
[ok] sem vazamentos (RAII e excecao na copia)
[ok] destruicao iterativa de 1 milhao de nos
[ok] comparador customizado e valor somente movel
[ok] limpar
15 testes passaram
```

Os testes executam **200 mil operações aleatórias** (inserir/atualizar, remover, buscar, intervalo, menor/maior) comparando cada resultado com `std::map` e conferindo `validar()` periodicamente; validam o invariante de níveis **a cada passo** em 3 mil operações; medem, em 20 sementes com 65 536 chaves, que a **altura média fica a no máximo 2 níveis de log2(n)**, que há em média 2 ponteiros por nó e que o nível `i` tem `≈ n/2^i` nós; e usam um tipo que conta instâncias para garantir que nada vaza, nem quando a cópia lança exceção no meio.

## Referências

- PUGH, W. Skip lists: a probabilistic alternative to balanced trees. *Communications of the ACM*, v. 33, n. 6, p. 668-676, 1990.
- PUGH, W. *A Skip List Cookbook*. Technical Report CS-TR-2286.1, University of Maryland, 1990.
- SEDGEWICK, R. *Algorithms in C++*. 3. ed. Addison-Wesley, 1998. Seção 13.5 (Skip Lists).
- MOTWANI, R.; RAGHAVAN, P. *Randomized Algorithms*. Cambridge University Press, 1995.
- STEELE, G. L.; LEA, D.; FLOOD, C. H. Fast splittable pseudorandom number generators. *OOPSLA 2014* (origem do SplitMix64).
- cppreference: [std::map](https://en.cppreference.com/w/cpp/container/map), [regra de três/cinco/zero](https://en.cppreference.com/w/cpp/language/rule_of_three).

## Licença

Distribuído sob a licença MIT. Veja [LICENSE](LICENSE).
