// Testes da SkipList. Usa assert, validar() e std::map como oraculo.
#undef NDEBUG
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "skip_list.hpp"

namespace {

int testes_passados = 0;

template <class F>
void executar(const char* nome, F teste) {
    teste();
    ++testes_passados;
    std::cout << "[ok] " << nome << '\n';
}

using Lista = SkipList<int, int>;

template <class K, class V, class C>
std::vector<std::pair<K, V>> conteudo(const SkipList<K, V, C>& s) {
    std::vector<std::pair<K, V>> v;
    for (const auto& [k, val] : s) v.emplace_back(k, val);
    return v;
}

template <class K, class V, class C>
std::vector<std::pair<K, V>> conteudo(const std::map<K, V, C>& m) {
    return {m.begin(), m.end()};
}

std::string desenho(const Lista& s) {
    std::ostringstream ss;
    s.imprimir_niveis(ss);
    return ss.str();
}

// Tipo que conta instancias vivas; a copia pode lancar quando o "orcamento"
// de copias acabar (para testar a garantia de excecao da copia da lista).
struct Contado {
    static inline int vivos = 0;
    static inline int copias_permitidas = -1;  // -1 = ilimitado
    int valor = 0;

    explicit Contado(int v) : valor(v) { ++vivos; }
    Contado(const Contado& o) : valor(o.valor) {
        if (copias_permitidas == 0) throw std::runtime_error("copia proibida");
        if (copias_permitidas > 0) --copias_permitidas;
        ++vivos;
    }
    Contado(Contado&& o) noexcept : valor(o.valor) { ++vivos; }
    Contado& operator=(const Contado&) = default;
    Contado& operator=(Contado&&) noexcept = default;
    ~Contado() { --vivos; }
};

void teste_vazia() {
    Lista s;
    assert(s.vazia() && s.tamanho() == 0 && s.altura() == 0);
    assert(s.validar());
    assert(s.buscar(1) == nullptr && !s.contem(1));
    assert(s.menor() == nullptr && s.maior() == nullptr);
    assert(!s.remover(1));
    assert(s.begin() == s.end());
    assert(s.intervalo(0, 100).vazia());
    assert(s.limite_inferior(5) == s.end() && s.limite_superior(5) == s.end());
    assert(desenho(s) == "nivel 0: H -> NULL\n");
    assert(s.nos_por_nivel().empty());
}

void teste_um_elemento() {
    Lista s;
    assert(s.inserir(7, 70));
    assert(s.tamanho() == 1 && s.altura() == 1 && s.validar());  // nivel <= altura + 1
    assert(*s.buscar(7) == 70 && s.contem(7) && !s.contem(6));
    assert(s.menor()->first == 7 && s.maior()->first == 7);
    assert(desenho(s) == "nivel 0: H -> 7 -> NULL\n");
    assert(s.remover(7));
    assert(s.vazia() && s.altura() == 0 && s.validar());
}

void teste_inserir_atualiza() {
    Lista s;
    assert(s.inserir(1, 10));
    assert(s.inserir(2, 20));
    assert(!s.inserir(1, 11));  // ja existe: atualiza
    assert(s.tamanho() == 2 && *s.buscar(1) == 11 && s.validar());
    *s.buscar(2) = 22;  // valor alteravel pelo ponteiro
    assert(*s.buscar(2) == 22);
    for (auto& [k, v] : s) v += k;  // e pelo iterador
    assert(*s.buscar(1) == 12 && *s.buscar(2) == 24);
}

void teste_remover() {
    Lista s;
    for (int k = 0; k < 100; ++k) s.inserir(k, k);
    for (int k = 0; k < 100; k += 2) assert(s.remover(k));
    assert(!s.remover(0) && !s.remover(1000) && !s.remover(-1));
    assert(s.tamanho() == 50 && s.validar());
    for (int k = 0; k < 100; ++k) assert(s.contem(k) == (k % 2 == 1));
    // remove do fim para o comeco: a altura deve encolher ate 0
    for (int k = 99; k >= 1; k -= 2) {
        assert(s.remover(k));
        assert(s.validar());
    }
    assert(s.vazia() && s.altura() == 0);
}

void teste_iterador_e_extremos() {
    Lista s(123);
    std::vector<int> chaves = {50, 10, 40, 20, 30, -5, 99};
    for (int k : chaves) s.inserir(k, k * 2);
    std::sort(chaves.begin(), chaves.end());
    std::vector<int> vistos;
    for (const auto& par : s) vistos.push_back(par.first);
    assert(vistos == chaves);
    assert(s.menor()->first == -5 && s.maior()->first == 99 && s.maior()->second == 198);

    Lista::const_iterator it = s.begin();  // conversao iterator -> const_iterator
    assert(it->first == -5);
    auto antigo = it++;
    assert(antigo->first == -5 && it->first == 10);
    assert(std::distance(s.cbegin(), s.cend()) == 7);
    static_assert(std::is_same_v<std::iterator_traits<Lista::iterator>::iterator_category,
                                 std::forward_iterator_tag>);
}

void teste_intervalos() {
    Lista s;
    for (int k = 0; k <= 100; k += 10) s.inserir(k, k);  // 0, 10, ..., 100
    auto chaves = [](auto faixa) {
        std::vector<int> v;
        for (const auto& par : faixa) v.push_back(par.first);
        return v;
    };
    assert(chaves(s.intervalo(15, 45)) == (std::vector<int>{20, 30, 40}));
    assert(chaves(s.intervalo(20, 40)) == (std::vector<int>{20, 30, 40}));  // fechado
    assert(chaves(s.intervalo(30, 30)) == (std::vector<int>{30}));          // unitario
    assert(s.intervalo(31, 39).vazia());                                    // entre chaves
    assert(s.intervalo(40, 20).vazia());                                    // invertido
    assert(chaves(s.intervalo(-100, 5)) == (std::vector<int>{0}));
    assert(chaves(s.intervalo(95, 1000)) == (std::vector<int>{100}));
    assert(s.intervalo(101, 1000).vazia() && s.intervalo(-9, -1).vazia());
    assert(chaves(s.intervalo(-1000, 1000)).size() == 11);
    assert(s.limite_inferior(10)->first == 10 && s.limite_superior(10)->first == 20);
    assert(s.limite_inferior(11)->first == 20 && s.limite_superior(100) == s.end());

    const Lista& c = s;
    assert(chaves(c.intervalo(55, 75)) == (std::vector<int>{60, 70}));
}

void teste_aleatorio_oraculo() {
    Lista s(2026);
    std::map<int, int> m;
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> chave(0, 4999);
    std::uniform_int_distribution<int> op(0, 99);
    for (int passo = 1; passo <= 200000; ++passo) {
        int k = chave(rng);
        int o = op(rng);
        if (o < 45) {
            bool novo = m.find(k) == m.end();
            m[k] = passo;
            assert(s.inserir(k, passo) == novo);
        } else if (o < 75) {
            assert(s.remover(k) == (m.erase(k) == 1));
        } else if (o < 90) {
            const int* v = s.buscar(k);
            auto it = m.find(k);
            assert((v != nullptr) == (it != m.end()));
            if (v) assert(*v == it->second);
        } else if (o < 97) {
            int b = k + chave(rng) % 200;
            std::vector<std::pair<int, int>> esperado(m.lower_bound(k), m.upper_bound(b));
            std::vector<std::pair<int, int>> obtido;
            for (const auto& par : s.intervalo(k, b)) obtido.push_back(par);
            assert(obtido == esperado);
        } else {
            if (m.empty()) {
                assert(s.menor() == nullptr && s.maior() == nullptr);
            } else {
                assert(s.menor()->first == m.begin()->first);
                assert(s.maior()->first == m.rbegin()->first);
            }
        }
        assert(s.tamanho() == m.size());
        if (passo % 20000 == 0) {
            assert(s.validar());
            assert(conteudo(s) == conteudo(m));
        }
    }
}

void teste_invariantes_niveis() {
    // Cada nivel e subsequencia ordenada do de baixo; conferido a cada passo.
    Lista s(7);
    std::mt19937 rng(7);
    for (int i = 0; i < 3000; ++i) {
        int k = static_cast<int>(rng() % 1000);
        if (rng() % 3 == 0) {
            s.remover(k);
        } else {
            s.inserir(k, i);
        }
        assert(s.validar());
    }
    // nos_por_nivel nao cresce de baixo para cima e o nivel 0 tem todos
    auto c = s.nos_por_nivel();
    assert(!c.empty() && c[0] == s.tamanho());
    for (std::size_t i = 1; i < c.size(); ++i) assert(c[i] <= c[i - 1] && c[i] > 0);
    assert(static_cast<int>(c.size()) == s.altura());
}

void teste_altura_e_distribuicao() {
    // Com p = 1/2: altura ~ log2(n), cerca de n/2^i nos no nivel i e, em
    // media, 2 ponteiros por no.
    const int n = 1 << 16;  // log2(n) = 16
    double soma_alturas = 0.0;
    const int rodadas = 20;
    double ponteiros = 0.0, nos = 0.0;
    std::vector<double> por_nivel(40, 0.0);
    for (int r = 0; r < rodadas; ++r) {
        Lista s(static_cast<std::uint64_t>(1000 + r));
        for (int k = 0; k < n; ++k) s.inserir(k, k);
        assert(s.validar());
        assert(s.altura() >= 10 && s.altura() <= 32);
        soma_alturas += s.altura();
        auto c = s.nos_por_nivel();
        for (std::size_t i = 0; i < c.size(); ++i) {
            ponteiros += static_cast<double>(c[i]);
            por_nivel[i] += static_cast<double>(c[i]);
        }
        nos += n;
    }
    double altura_media = soma_alturas / rodadas;
    // Sem limite, E[max de n geometricas] ~ log2(n) + 1,3; com o limite
    // "altura atual + 1" ela fica um pouco abaixo. Exige |media - log2 n| <= 2.
    assert(std::fabs(altura_media - std::log2(static_cast<double>(n))) <= 2.0);
    double media_ponteiros = ponteiros / nos;
    assert(std::fabs(media_ponteiros - 2.0) < 0.02);
    for (int i = 1; i <= 6; ++i) {
        double esperado = nos / std::pow(2.0, i);
        assert(std::fabs(por_nivel[static_cast<std::size_t>(i)] - esperado) < 0.03 * esperado);
    }
}

void teste_semente_determinismo() {
    Lista a(99), b(99), c(100);
    for (int k = 0; k < 40; ++k) {
        a.inserir(k, k);
        b.inserir(k, k);
        c.inserir(k, k);
    }
    assert(desenho(a) == desenho(b));  // mesma semente -> mesma estrutura
    assert(desenho(a) != desenho(c));  // outra semente -> outra estrutura
    assert(conteudo(a) == conteudo(c));

    a.limpar();
    a.semear(100);
    for (int k = 0; k < 40; ++k) a.inserir(k, k);
    assert(desenho(a) == desenho(c));
}

void teste_copia_movimento() {
    Lista a(5);
    for (int k = 0; k < 500; ++k) a.inserir(k * 3, k);
    Lista b(a);  // copia preserva conteudo E forma
    assert(b.validar() && conteudo(a) == conteudo(b) && a.nos_por_nivel() == b.nos_por_nivel());
    b.inserir(1, 1);
    b.remover(0);
    assert(a.contem(0) && !a.contem(1));  // independentes

    Lista c;
    c = a;
    assert(c.validar() && conteudo(c) == conteudo(a));
    c = c;  // autoatribuicao
    assert(c.validar() && c.tamanho() == 500);

    Lista d(std::move(c));
    assert(d.tamanho() == 500 && d.validar());
    assert(c.vazia() && c.altura() == 0 && c.validar() && c.begin() == c.end());  // NOLINT
    c.inserir(1, 1);  // a origem movida continua utilizavel
    assert(c.tamanho() == 1 && c.validar());

    Lista e;
    e.inserir(43, 43);
    e = std::move(d);
    assert(e.tamanho() == 500 && !e.contem(43) && e.validar());
    assert(d.vazia() && d.validar());  // NOLINT

    a.trocar(c);
    assert(a.tamanho() == 1 && c.tamanho() == 500);
    static_assert(std::is_nothrow_move_constructible_v<Lista>);
    static_assert(std::is_nothrow_move_assignable_v<Lista>);
}

void teste_sem_vazamentos() {
    Contado::vivos = 0;
    {
        SkipList<int, Contado> s;
        for (int k = 0; k < 1000; ++k) s.inserir(k, Contado(k));
        for (int k = 0; k < 1000; k += 3) s.remover(k);
        s.inserir(1, Contado(-1));  // atualizacao
        assert(Contado::vivos == static_cast<int>(s.tamanho()));
        SkipList<int, Contado> copia(s);
        assert(Contado::vivos == 2 * static_cast<int>(s.tamanho()));
        SkipList<int, Contado> movida(std::move(copia));
        assert(Contado::vivos == 2 * static_cast<int>(s.tamanho()));

        // copia que falha no meio: nada pode vazar e a excecao propaga
        Contado::copias_permitidas = 300;
        bool lancou = false;
        try {
            SkipList<int, Contado> falha(s);
        } catch (const std::runtime_error&) {
            lancou = true;
        }
        Contado::copias_permitidas = -1;
        assert(lancou);
        assert(Contado::vivos == 2 * static_cast<int>(s.tamanho()));

        // atribuicao por copia que falha nao altera o destino (garantia forte)
        SkipList<int, Contado> destino;
        destino.inserir(7, Contado(7));
        Contado::copias_permitidas = 10;
        try {
            destino = s;
            assert(false);
        } catch (const std::runtime_error&) {
        }
        Contado::copias_permitidas = -1;
        assert(destino.tamanho() == 1 && destino.buscar(7)->valor == 7 && destino.validar());
    }
    assert(Contado::vivos == 0);
}

void teste_destruicao_grande() {
    // Destruicao iterativa: 1 milhao de nos sem recursao.
    SkipList<int, int> s;
    for (int k = 0; k < 1000000; ++k) s.inserir(k, k);
    assert(s.tamanho() == 1000000 && s.contem(999999));
    s.limpar();
    assert(s.vazia() && s.validar());
    for (int k = 0; k < 1000000; ++k) s.inserir(-k, k);
}  // destrutor libera o segundo milhao

void teste_comparador_e_valor_movel() {
    SkipList<int, std::unique_ptr<int>, std::greater<int>> s(1, std::greater<int>());
    for (int k = 0; k < 20; ++k) s.inserir(k, std::make_unique<int>(k * k));
    assert(s.validar());
    assert(s.menor()->first == 19 && s.maior()->first == 0);  // ordem decrescente
    int esperado = 19;
    for (const auto& [k, v] : s) {
        assert(k == esperado && *v == k * k);
        --esperado;
    }
    std::vector<int> faixa;
    for (const auto& par : s.intervalo(10, 7)) faixa.push_back(par.first);  // "de 10 ate 7"
    assert(faixa == (std::vector<int>{10, 9, 8, 7}));
    assert(s.remover(10) && !s.contem(10) && s.validar());

    SkipList<std::string, int> nomes;
    nomes.inserir("pera", 1);
    nomes.inserir("abacaxi", 2);
    nomes.inserir("manga", 3);
    assert(nomes.menor()->first == "abacaxi" && nomes.maior()->first == "pera");
}

void teste_limpar() {
    Lista s;
    for (int k = 0; k < 50; ++k) s.inserir(k, k);
    s.limpar();
    assert(s.vazia() && s.altura() == 0 && s.validar() && s.begin() == s.end());
    s.inserir(3, 3);
    assert(s.tamanho() == 1 && s.validar());
}

}  // namespace

int main() {
    executar("lista vazia", teste_vazia);
    executar("um elemento", teste_um_elemento);
    executar("inserir chave existente atualiza o valor", teste_inserir_atualiza);
    executar("remover e altura dinamica", teste_remover);
    executar("iterador em ordem, menor e maior", teste_iterador_e_extremos);
    executar("busca por intervalo (limites, vazio e unitario)", teste_intervalos);
    executar("200 mil operacoes aleatorias vs std::map", teste_aleatorio_oraculo);
    executar("cada nivel e subsequencia ordenada do nivel abaixo", teste_invariantes_niveis);
    executar("altura media ~ log2(n) e distribuicao geometrica", teste_altura_e_distribuicao);
    executar("semente configuravel e determinismo", teste_semente_determinismo);
    executar("copia e movimento", teste_copia_movimento);
    executar("sem vazamentos (RAII e excecao na copia)", teste_sem_vazamentos);
    executar("destruicao iterativa de 1 milhao de nos", teste_destruicao_grande);
    executar("comparador customizado e valor somente movel", teste_comparador_e_valor_movel);
    executar("limpar", teste_limpar);
    std::cout << testes_passados << " testes passaram\n";
    return 0;
}
