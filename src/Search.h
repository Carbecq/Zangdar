#ifndef SEARCH_H
#define SEARCH_H


#include <thread>
#include <atomic>
#include <cstring>
#include "Timer.h"
#include "Board.h"
#include "SearchInfo.h"
#include "History.h"
#include "TranspositionTable.h"



constexpr int STACK_OFFSET = 8;   // >= 6 : nécessaire pour la continuation history 6-ply (regard arrière (info-6))
constexpr int STACK_SIZE   = MAX_PLY + 2*STACK_OFFSET;  // taille un peu trop grande, mais multiple de 8


//! \brief  Données d'une thread
class Search
{
public:
    Search();
    ~Search();

    NNUE                nnue;
    std::thread         thread;
    TranspositionTable* table = nullptr;
    History             history;

    //==============================================
    //  Evaluation
    [[nodiscard]] int evaluate(const Board &board);
    [[nodiscard]] int do_evaluate(const Board &board, Accumulator& acc);

    template <Color US, bool Update_NNUE> void make_move(Board& board, const MOVE move) noexcept;
    template <Color US, bool Update_NNUE> void undo_move(Board& board) noexcept;

    U64         nodes;          // nombre de neuds recherchés
    U64         tbhits;

    int         index;          // indice de la thread
    int         seldepth;       // selective depth
    std::atomic<bool>  stopped{false};  // flag local (DataGen)
    std::atomic<bool>* stopFlagPtr;     // pointe vers stopped (DataGen) ou ThreadPool::searchStopped (engine)

    //! \brief  Indique si la recherche doit s'arrêter
    //! \return true si le flag d'arrêt est levé
    bool is_stopped()  const noexcept { return stopFlagPtr->load(std::memory_order_relaxed); }

    //! \brief  Lève le flag d'arrêt de la recherche
    void signal_stop()       noexcept { stopFlagPtr->store(true, std::memory_order_relaxed); }

    int         iter_depth;     // profondeur demandée dans iterative  deepening
    int         best_depth;     // profondeur du meilleur coup trouvé jusqu'à présent

    // Historique par profondeur
    int         pv_scores[MAX_PLY+1];
    MOVE        pv_moves[MAX_PLY+1];

    // PV complète de la dernière itération terminée.
    PVariation  last_pv;

    // Point de départ de la recherche
    template <Color C> void think(Board board, Timer timer, size_t _index);
    template <Color C> int  aspiration_window(Board& board, Timer& timer, SearchInfo* si, int prev_score);

    //! \brief  Accès en lecture seule à l'accumulateur NNUE courant
    inline const Accumulator& get_accumulator() const { return nnue.get_accumulator(); }

    //! \brief  Accès en lecture/écriture à l'accumulateur NNUE courant
    inline       Accumulator& get_accumulator()       { return nnue.get_accumulator(); }

    void init_reductions();

private:

    template <Color C> void iterative_deepening(Board& board, Timer& timer, SearchInfo* si);
    template <Color C> int  alpha_beta(Board& board, Timer& timer, int alpha, int beta, int depth, bool cut_node, SearchInfo* si);
    template <Color C> int  quiescence(Board& board, Timer& timer, int alpha, int beta, SearchInfo* si);

    void show_uci_result(I64 elapsed, const PVariation &pv) const;
    void show_uci_best(MOVE best_move) const;
    void update_pv(SearchInfo* si, const MOVE move) const;

    static constexpr int LateMovePruningDepth = 7;
    static constexpr int LateMovePruningCount[2][8] = {
        {0, 2, 3, 4, 6, 8, 13, 18},
        {0, 3, 4, 6, 8, 12, 20, 30}
    };

    int Reductions[2][32][32];

}__attribute__((aligned(64)));


#endif // SEARCH_H
