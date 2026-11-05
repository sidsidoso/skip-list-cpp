// Demonstracao da SkipList: estrutura em niveis, operacoes basicas e
// comparacao de tempo com std::map.
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "skip_list.hpp"

namespace {

using Relogio = std::chrono::steady_clock;

double ms_desde(Relogio::time_point inicio) {
    return std::chrono::duration<double, std::milli>(Relogio::now() - inicio).count();
}

struct Tempos {
    double inserir, buscar, remover;
    long long verificacao;  // evita que o otimizador descarte as buscas
};

template <class Mapa, class Inserir, class Buscar, class Remover>
Tempos medir(Mapa& mapa, const std::vector<int>& chaves, const std::vector<int>& consultas,
             Inserir inserir, Buscar buscar, Remover remover) {
    Tempos t{};
    auto inicio = Relogio::now();
    for (int k : chaves) inserir(mapa, k);
    t.inserir = ms_desde(inicio);

    inicio = Relogio::now();
    for (int k : consultas) t.verificacao += buscar(mapa, k);
    t.buscar = ms_desde(inicio);

    inicio = Relogio::now();
    for (int k : chaves) remover(mapa, k);
    t.remover = ms_desde(inicio);
    return t;
}

void comparar_com_map(std::size_t n) {
    std::mt19937 rng(static_cast<std::uint32_t>(n));
    std::uniform_int_distribution<int> dist(0, 1 << 30);
    std::vector<int> chaves(n), consultas(n);
    for (auto& k : chaves) k = dist(rng);
    for (std::size_t i = 0; i < n; ++i) {
        // metade das consultas acerta, metade (quase sempre) erra
        consultas[i] = (i % 2 == 0) ? chaves[rng() % n] : dist(rng);
    }

    SkipList<int, int> skip(2026);
    Tempos ts = medir(
        skip, chaves, consultas, [](auto& m, int k) { m.inserir(k, k); },
        [](auto& m, int k) { return m.contem(k) ? 1 : 0; },
        [](auto& m, int k) { m.remover(k); });

    std::map<int, int> mapa;
    Tempos tm = medir(
        mapa, chaves, consultas, [](auto& m, int k) { m.insert_or_assign(k, k); },
        [](auto& m, int k) { return m.count(k) ? 1 : 0; },
        [](auto& m, int k) { m.erase(k); });

    std::printf("n = %zu (%zu insercoes, %zu buscas, %zu remocoes)\n", n, n, n, n);
    std::printf("  %-10s %12s %12s %12s\n", "", "inserir", "buscar", "remover");
    std::printf("  %-10s %9.1f ms %9.1f ms %9.1f ms\n", "SkipList", ts.inserir, ts.buscar,
                ts.remover);
    std::printf("  %-10s %9.1f ms %9.1f ms %9.1f ms\n", "std::map", tm.inserir, tm.buscar,
                tm.remover);
    std::printf("  buscas com sucesso: %lld (skip) / %lld (map)\n", ts.verificacao,
                tm.verificacao);
}

}  // namespace

int main() {
    std::cout << "== Skip list (p = 1/2, semente fixa 2026) ==\n";
    SkipList<int, std::string> s(2026);
    for (int k : {30, 10, 50, 20, 70, 40, 60, 80, 5, 90, 25, 65}) {
        s.inserir(k, "v" + std::to_string(k));
    }
    std::cout << "Apos inserir 30 10 50 20 70 40 60 80 5 90 25 65 (H = cabeca):\n";
    s.imprimir_niveis(std::cout);
    std::cout << "tamanho: " << s.tamanho() << ", altura: " << s.altura()
              << ", invariantes ok? " << std::boolalpha << s.validar() << "\n\n";

    std::cout << "buscar(40) -> " << *s.buscar(40) << '\n';
    std::cout << "contem(45)? " << s.contem(45) << '\n';
    std::cout << "menor: " << s.menor()->first << ", maior: " << s.maior()->first << '\n';
    std::cout << "intervalo [22, 62]:";
    for (const auto& [k, v] : s.intervalo(22, 62)) std::cout << ' ' << k << '=' << v;
    std::cout << '\n';
    s.inserir(40, "novo");
    std::cout << "inserir(40, \"novo\") atualiza: buscar(40) -> " << *s.buscar(40) << "\n\n";

    for (int k : {50, 5, 90}) s.remover(k);
    std::cout << "Apos remover 50, 5 e 90:\n";
    s.imprimir_niveis(std::cout);
    std::cout << "em ordem:";
    for (const auto& par : s) std::cout << ' ' << par.first;
    std::cout << "\n\n";

    // Distribuicao dos niveis com muitos elementos.
    const int n = 1000000;
    SkipList<int, int> grande(7);
    for (int k = 0; k < n; ++k) grande.inserir(k, k);
    auto contagem = grande.nos_por_nivel();
    std::size_t ponteiros = 0;
    for (auto c : contagem) ponteiros += c;
    std::printf("n = %d: altura %d (log2 n = %.1f), %.3f ponteiros por no em media\n", n,
                grande.altura(), std::log2(static_cast<double>(n)),
                static_cast<double>(ponteiros) / n);
    std::printf("  nivel   nos      esperado n/2^i\n");
    for (std::size_t i = 0; i < contagem.size() && i < 6; ++i) {
        std::printf("  %5zu %8zu %12.0f\n", i, contagem[i], n / std::pow(2.0, static_cast<double>(i)));
    }
    std::printf("  ...\n\n");

    std::cout << "Comparacao com std::map (chaves aleatorias; tempos variam por maquina):\n";
    comparar_com_map(100000);
    comparar_com_map(1000000);
    return 0;
}
