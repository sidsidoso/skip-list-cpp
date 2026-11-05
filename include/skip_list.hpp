// skip_list.hpp - skip list generica (mapa ordenado chave -> valor), header-only.
//
// Uma skip list e uma pilha de listas encadeadas ordenadas. O nivel 0 contem
// todos os elementos; cada elemento tambem aparece nos niveis 1, 2, ... com
// probabilidade 1/2, 1/4, ... (sorteio independente por elemento). As listas
// de cima funcionam como "vias expressas": a busca comeca no nivel mais alto,
// anda para a direita enquanto a proxima chave for menor que a procurada e
// desce um nivel quando nao puder mais avancar. Com p = 1/2 a altura esperada
// e ~log2(n) e busca/insercao/remocao custam O(log n) ESPERADO, sem nenhuma
// rotacao ou rebalanceamento.
//
// Invariante (conferido por validar()): cada nivel e uma lista estritamente
// ordenada e e uma subsequencia do nivel imediatamente abaixo; um no de
// altura h aparece exatamente nos niveis 0..h-1.
#ifndef SKIP_LIST_HPP
#define SKIP_LIST_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <new>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

template <class Chave, class Valor, class Comparador = std::less<Chave>>
class SkipList {
public:
    // Altura maxima absoluta: com p = 1/2, 32 niveis bastam para ~4 bilhoes
    // de elementos. A altura EFETIVA e dinamica (veja sortear_nivel).
    static constexpr int NIVEL_MAXIMO = 32;
    static constexpr std::uint64_t SEMENTE_PADRAO = 0x5EED5EED2026ULL;

    using key_type = Chave;
    using mapped_type = Valor;
    using value_type = std::pair<const Chave, Valor>;
    using size_type = std::size_t;

private:
    // ---------- gerador pseudoaleatorio proprio (SplitMix64) ----------
    // Rapido (algumas operacoes aritmeticas), com estado de 64 bits e boa
    // qualidade estatistica; qualquer semente (inclusive 0) e valida.
    struct Gerador {
        std::uint64_t estado;
        std::uint64_t proximo() noexcept {
            std::uint64_t z = (estado += 0x9E3779B97F4A7C15ULL);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return z ^ (z >> 31);
        }
    };

    // ---------- no: par + vetor de ponteiros na MESMA alocacao ----------
    //
    //   +-----------+-------+---------+---------+-----+-----------+
    //   | par(k, v) | nivel | prox[0] | prox[1] | ... | prox[h-1] |
    //   +-----------+-------+---------+---------+-----+-----------+
    //
    // O vetor prox tem exatamente 'nivel' posicoes e comeca no primeiro
    // endereco alinhado para ponteiro depois do struct (DESLOCAMENTO_PROX);
    // a alocacao usa o maior dos dois alinhamentos. Uma unica alocacao por
    // no, sem o custo de um std::vector (ponteiro + tamanho + capacidade).
    struct No {
        value_type par;
        int nivel;

        template <class K, class V>
        No(K&& k, V&& v, int h) : par(std::forward<K>(k), std::forward<V>(v)), nivel(h) {}

        No** prox() noexcept {
            return reinterpret_cast<No**>(reinterpret_cast<unsigned char*>(this) +
                                          DESLOCAMENTO_PROX);
        }
        No* const* prox() const noexcept {
            return reinterpret_cast<No* const*>(reinterpret_cast<const unsigned char*>(this) +
                                                DESLOCAMENTO_PROX);
        }
    };

    static constexpr std::size_t DESLOCAMENTO_PROX =
        (sizeof(No) + alignof(No*) - 1) / alignof(No*) * alignof(No*);
    static constexpr std::align_val_t ALINHAMENTO_NO{alignof(No) > alignof(No*) ? alignof(No)
                                                                                : alignof(No*)};

    static std::size_t bytes_do_no(int nivel) noexcept {
        return DESLOCAMENTO_PROX + static_cast<std::size_t>(nivel) * sizeof(No*);
    }

    template <class K, class V>
    static No* criar_no(K&& k, V&& v, int nivel) {
        void* memoria = ::operator new(bytes_do_no(nivel), ALINHAMENTO_NO);
        No* no;
        try {
            no = ::new (memoria) No(std::forward<K>(k), std::forward<V>(v), nivel);
        } catch (...) {
            ::operator delete(memoria, ALINHAMENTO_NO);
            throw;
        }
        No** prox = no->prox();
        for (int i = 0; i < nivel; ++i) ::new (static_cast<void*>(prox + i)) No*(nullptr);
        return no;
    }

    static void destruir_no(No* no) noexcept {
        no->~No();
        ::operator delete(static_cast<void*>(no), ALINHAMENTO_NO);
    }

public:
    // ---------- iterador em ordem (forward: segue prox[0]) ----------

    template <bool Constante>
    class IteradorBase {
        using PtrNo = std::conditional_t<Constante, const No*, No*>;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = SkipList::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = std::conditional_t<Constante, const value_type*, value_type*>;
        using reference = std::conditional_t<Constante, const value_type&, value_type&>;

        IteradorBase() noexcept = default;
        // iterador -> iterador constante
        template <bool C = Constante, class = std::enable_if_t<C>>
        IteradorBase(const IteradorBase<false>& outro) noexcept : no_(outro.no_) {}

        reference operator*() const noexcept { return no_->par; }
        pointer operator->() const noexcept { return &no_->par; }

        IteradorBase& operator++() noexcept {
            no_ = no_->prox()[0];
            return *this;
        }
        IteradorBase operator++(int) noexcept {
            IteradorBase copia = *this;
            ++*this;
            return copia;
        }

        friend bool operator==(const IteradorBase& a, const IteradorBase& b) noexcept {
            return a.no_ == b.no_;
        }
        friend bool operator!=(const IteradorBase& a, const IteradorBase& b) noexcept {
            return a.no_ != b.no_;
        }

    private:
        friend class SkipList;
        template <bool>
        friend class IteradorBase;
        explicit IteradorBase(PtrNo no) noexcept : no_(no) {}
        PtrNo no_ = nullptr;
    };

    using iterator = IteradorBase<false>;
    using const_iterator = IteradorBase<true>;

    // Intervalo [inicio, fim) utilizavel em for por intervalo.
    template <class It>
    struct Faixa {
        It inicio, fim;
        It begin() const noexcept { return inicio; }
        It end() const noexcept { return fim; }
        bool vazia() const noexcept { return inicio == fim; }
    };

    // ---------- construcao, copia, movimento (regra dos 5) ----------

    SkipList() = default;
    explicit SkipList(std::uint64_t semente, Comparador comp = Comparador())
        : gerador_{semente}, comp_(std::move(comp)) {}

    SkipList(const SkipList& outra) : gerador_(outra.gerador_), comp_(outra.comp_) {
        try {
            copiar_de(outra);
        } catch (...) {
            limpar();  // tudo o que ja foi copiado esta ligado no nivel 0
            throw;
        }
    }

    SkipList(SkipList&& outra) noexcept
        : cabeca_(outra.cabeca_),
          altura_(std::exchange(outra.altura_, 0)),
          tamanho_(std::exchange(outra.tamanho_, 0)),
          gerador_(outra.gerador_),
          comp_(std::move(outra.comp_)) {
        outra.cabeca_.fill(nullptr);
    }

    SkipList& operator=(const SkipList& outra) {
        if (this != &outra) {
            SkipList temp(outra);  // copia-e-troca: garantia forte
            trocar(temp);
        }
        return *this;
    }

    SkipList& operator=(SkipList&& outra) noexcept {
        if (this != &outra) {
            limpar();
            cabeca_ = outra.cabeca_;
            outra.cabeca_.fill(nullptr);
            altura_ = std::exchange(outra.altura_, 0);
            tamanho_ = std::exchange(outra.tamanho_, 0);
            gerador_ = outra.gerador_;
            comp_ = std::move(outra.comp_);
        }
        return *this;
    }

    ~SkipList() { limpar(); }

    void trocar(SkipList& outra) noexcept {
        using std::swap;
        swap(cabeca_, outra.cabeca_);
        swap(altura_, outra.altura_);
        swap(tamanho_, outra.tamanho_);
        swap(gerador_, outra.gerador_);
        swap(comp_, outra.comp_);
    }

    // Reinicia o gerador: a mesma semente + a mesma sequencia de operacoes
    // produz exatamente a mesma estrutura.
    void semear(std::uint64_t semente) noexcept { gerador_.estado = semente; }

    // ---------- consultas ----------

    size_type tamanho() const noexcept { return tamanho_; }
    bool vazia() const noexcept { return tamanho_ == 0; }
    // Numero de niveis em uso (0 quando vazia).
    int altura() const noexcept { return altura_; }

    // Ponteiro para o valor associado a chave, ou nullptr. O(log n) esperado.
    Valor* buscar(const Chave& chave) noexcept {
        No* no = localizar_primeiro_nao_menor(chave);
        return (no != nullptr && !comp_(chave, no->par.first)) ? &no->par.second : nullptr;
    }
    const Valor* buscar(const Chave& chave) const noexcept {
        return const_cast<SkipList*>(this)->buscar(chave);
    }

    bool contem(const Chave& chave) const noexcept { return buscar(chave) != nullptr; }

    // Menor elemento: primeiro no do nivel 0. O(1)
    const value_type* menor() const noexcept {
        return cabeca_[0] != nullptr ? &cabeca_[0]->par : nullptr;
    }

    // Maior elemento: desce pela "escada" mais a direita. O(log n) esperado.
    const value_type* maior() const noexcept {
        if (altura_ == 0) return nullptr;
        No* const* prox = cabeca_.data();
        const No* ultimo = nullptr;
        for (int i = altura_ - 1; i >= 0; --i) {
            while (prox[i] != nullptr) {
                ultimo = prox[i];
                prox = prox[i]->prox();
            }
        }
        return &ultimo->par;
    }

    // ---------- modificacao ----------

    // Insere (chave, valor). Se a chave ja existir, apenas atualiza o valor.
    // Retorna true se inseriu um elemento novo. O(log n) esperado.
    bool inserir(const Chave& chave, Valor valor) {
        std::array<No**, NIVEL_MAXIMO> atualizar;
        No** pred = localizar_predecessores(chave, atualizar);
        No* candidato = pred[0];
        if (candidato != nullptr && !comp_(chave, candidato->par.first)) {
            candidato->par.second = std::move(valor);
            return false;
        }

        int nivel = sortear_nivel();
        No* novo = criar_no(chave, std::move(valor), nivel);  // pode lancar: nada mudou ainda
        for (int i = altura_; i < nivel; ++i) atualizar[i] = cabeca_.data();
        if (nivel > altura_) altura_ = nivel;

        No** prox_novo = novo->prox();
        for (int i = 0; i < nivel; ++i) {
            prox_novo[i] = atualizar[i][i];
            atualizar[i][i] = novo;
        }
        ++tamanho_;
        return true;
    }

    // Remove a chave. Retorna false se ela nao existir. O(log n) esperado.
    bool remover(const Chave& chave) noexcept {
        std::array<No**, NIVEL_MAXIMO> atualizar;
        No* alvo = localizar_predecessores(chave, atualizar)[0];
        if (alvo == nullptr || comp_(chave, alvo->par.first)) return false;

        No** prox_alvo = alvo->prox();
        for (int i = 0; i < alvo->nivel; ++i) atualizar[i][i] = prox_alvo[i];
        destruir_no(alvo);
        --tamanho_;
        while (altura_ > 0 && cabeca_[altura_ - 1] == nullptr) --altura_;  // altura dinamica
        return true;
    }

    // Remove tudo. Destruicao ITERATIVA pelo nivel 0: O(n), sem recursao.
    void limpar() noexcept {
        No* no = cabeca_[0];
        while (no != nullptr) {
            No* prox = no->prox()[0];
            destruir_no(no);
            no = prox;
        }
        cabeca_.fill(nullptr);
        altura_ = 0;
        tamanho_ = 0;
    }

    // ---------- iteracao e intervalos ----------

    iterator begin() noexcept { return iterator(cabeca_[0]); }
    iterator end() noexcept { return iterator(nullptr); }
    const_iterator begin() const noexcept { return const_iterator(cabeca_[0]); }
    const_iterator end() const noexcept { return const_iterator(nullptr); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }

    // Primeiro elemento com chave >= chave. O(log n) esperado.
    iterator limite_inferior(const Chave& chave) noexcept {
        return iterator(localizar_primeiro_nao_menor(chave));
    }
    const_iterator limite_inferior(const Chave& chave) const noexcept {
        return const_iterator(const_cast<SkipList*>(this)->localizar_primeiro_nao_menor(chave));
    }

    // Primeiro elemento com chave > chave. O(log n) esperado.
    iterator limite_superior(const Chave& chave) noexcept {
        No* const* prox = cabeca_.data();
        for (int i = altura_ - 1; i >= 0; --i) {
            while (prox[i] != nullptr && !comp_(chave, prox[i]->par.first)) prox = prox[i]->prox();
        }
        return iterator(altura_ > 0 ? prox[0] : nullptr);
    }
    const_iterator limite_superior(const Chave& chave) const noexcept {
        return const_iterator(const_cast<SkipList*>(this)->limite_superior(chave).no_);
    }

    // Elementos com minimo <= chave <= maximo (intervalo FECHADO), em ordem.
    // O(log n + k) esperado, k = quantidade devolvida. Se maximo < minimo a
    // faixa e vazia.
    Faixa<iterator> intervalo(const Chave& minimo, const Chave& maximo) noexcept {
        if (comp_(maximo, minimo)) return {end(), end()};
        return {limite_inferior(minimo), limite_superior(maximo)};
    }
    Faixa<const_iterator> intervalo(const Chave& minimo, const Chave& maximo) const noexcept {
        auto f = const_cast<SkipList*>(this)->intervalo(minimo, maximo);
        return {f.inicio, f.fim};
    }

    // ---------- diagnostico ----------

    // contagem[i] = numero de nos presentes no nivel i. O(n)
    std::vector<size_type> nos_por_nivel() const {
        std::vector<size_type> contagem(static_cast<std::size_t>(altura_), 0);
        for (const No* no = cabeca_[0]; no != nullptr; no = no->prox()[0]) {
            for (int i = 0; i < no->nivel; ++i) ++contagem[static_cast<std::size_t>(i)];
        }
        return contagem;
    }

    // Confere todos os invariantes. O(n * altura). Usado nos testes.
    bool validar() const {
        if (altura_ < 0 || altura_ > NIVEL_MAXIMO) return false;
        for (int i = altura_; i < NIVEL_MAXIMO; ++i) {
            if (cabeca_[static_cast<std::size_t>(i)] != nullptr) return false;
        }
        if (altura_ > 0 && cabeca_[static_cast<std::size_t>(altura_ - 1)] == nullptr) {
            return false;  // nivel de cima vazio: altura deveria ter diminuido
        }
        if ((altura_ == 0) != (tamanho_ == 0)) return false;

        // Nivel 0: estritamente ordenado e com 'tamanho_' nos.
        size_type contagem = 0;
        int maior_nivel = 0;
        const No* anterior = nullptr;
        for (const No* no = cabeca_[0]; no != nullptr; no = no->prox()[0]) {
            if (no->nivel < 1 || no->nivel > altura_) return false;
            if (anterior != nullptr && !comp_(anterior->par.first, no->par.first)) return false;
            if (no->nivel > maior_nivel) maior_nivel = no->nivel;
            anterior = no;
            ++contagem;
        }
        if (contagem != tamanho_ || maior_nivel != altura_) return false;

        // Nivel i >= 1: deve ser exatamente a subsequencia do nivel 0 formada
        // pelos nos de altura > i (logo tambem subsequencia do nivel i-1).
        for (int i = 1; i < altura_; ++i) {
            const No* no_nivel = cabeca_[static_cast<std::size_t>(i)];
            for (const No* no = cabeca_[0]; no != nullptr; no = no->prox()[0]) {
                if (no->nivel > i) {
                    if (no_nivel != no) return false;
                    no_nivel = no_nivel->prox()[i];
                }
            }
            if (no_nivel != nullptr) return false;
        }
        return true;
    }

    // Desenha os niveis em ASCII (o mais alto em cima), alinhando cada no na
    // mesma coluna em todos os niveis. Pensado para poucos elementos; exige
    // que Chave possa ser escrita com operator<<.
    void imprimir_niveis(std::ostream& saida) const {
        std::vector<std::string> rotulos;
        std::vector<int> alturas;
        for (const No* no = cabeca_[0]; no != nullptr; no = no->prox()[0]) {
            std::ostringstream ss;
            ss << no->par.first;
            rotulos.push_back(ss.str());
            alturas.push_back(no->nivel);
        }
        for (int i = altura_ - 1; i >= 0; --i) {
            std::string linha = "nivel " + std::to_string(i) + (altura_ > 10 && i < 10 ? " " : "") +
                                ": H";
            bool anterior_presente = true;  // a cabeca esta em todos os niveis
            for (std::size_t j = 0; j < rotulos.size(); ++j) {
                if (alturas[j] > i) {  // no presente: seta chegando nele
                    linha += (anterior_presente ? " -> " : "--> ") + rotulos[j];
                    anterior_presente = true;
                } else {  // ausente: a seta passa por cima, mesma largura
                    linha += (anterior_presente ? " " : "-") +
                             std::string(3 + rotulos[j].size(), '-');
                    anterior_presente = false;
                }
            }
            saida << linha << (anterior_presente ? " -> NULL\n" : "--> NULL\n");
        }
        if (altura_ == 0) saida << "nivel 0: H -> NULL\n";
    }

private:
    // Busca descendente: em cada nivel i, atualizar[i] recebe o vetor prox do
    // ultimo no com chave < 'chave' (ou o vetor da cabeca). Tratar a cabeca e
    // os nos pelo mesmo tipo (No**) elimina casos especiais na insercao e na
    // remocao. Retorna o vetor prox do predecessor no nivel 0.
    No** localizar_predecessores(const Chave& chave,
                                 std::array<No**, NIVEL_MAXIMO>& atualizar) noexcept {
        No** prox = cabeca_.data();
        for (int i = altura_ - 1; i >= 0; --i) {
            while (prox[i] != nullptr && comp_(prox[i]->par.first, chave)) prox = prox[i]->prox();
            atualizar[static_cast<std::size_t>(i)] = prox;
        }
        return prox;
    }

    No* localizar_primeiro_nao_menor(const Chave& chave) noexcept {
        No** prox = cabeca_.data();
        for (int i = altura_ - 1; i >= 0; --i) {
            while (prox[i] != nullptr && comp_(prox[i]->par.first, chave)) prox = prox[i]->prox();
        }
        return prox[0];
    }

    // Nivel geometrico com p = 1/2: cada bit aleatorio e uma "moeda", e o
    // nivel e 1 + numero de zeros consecutivos nos bits baixos. Um unico
    // numero de 64 bits por insercao. O nivel e limitado a altura atual + 1
    // ("fixar os dados", Pugh 1990): a altura cresce aos poucos, um nivel por
    // vez, e uma lista pequena nunca ganha 20 niveis por azar.
    int sortear_nivel() noexcept {
        int limite = altura_ + 1 < NIVEL_MAXIMO ? altura_ + 1 : NIVEL_MAXIMO;
        std::uint64_t bits = gerador_.proximo() | (std::uint64_t{1} << 63);
        int nivel = 1 + zeros_a_direita(bits);
        return nivel < limite ? nivel : limite;
    }

    static int zeros_a_direita(std::uint64_t x) noexcept {
#if defined(__GNUC__) || defined(__clang__)
        return __builtin_ctzll(x);
#else
        int n = 0;
        while ((x & 1u) == 0) {
            x >>= 1;
            ++n;
        }
        return n;
#endif
    }

    // Copia preservando o nivel de cada no (mesma forma). O(n): os nos sao
    // anexados no fim de cada nivel usando 'fim[i]' (o ultimo campo prox).
    void copiar_de(const SkipList& outra) {
        std::array<No**, NIVEL_MAXIMO> fim;
        for (int i = 0; i < NIVEL_MAXIMO; ++i) fim[i] = &cabeca_[i];
        for (const No* no = outra.cabeca_[0]; no != nullptr; no = no->prox()[0]) {
            No* novo = criar_no(no->par.first, no->par.second, no->nivel);
            No** prox_novo = novo->prox();
            for (int i = 0; i < no->nivel; ++i) {
                *fim[i] = novo;
                fim[i] = &prox_novo[i];
            }
            ++tamanho_;
            if (no->nivel > altura_) altura_ = no->nivel;
        }
    }

    std::array<No*, NIVEL_MAXIMO> cabeca_{};  // cabeca: prox de cada nivel
    int altura_ = 0;
    size_type tamanho_ = 0;
    Gerador gerador_{SEMENTE_PADRAO};
    Comparador comp_{};
};

#endif  // SKIP_LIST_HPP
