#ifndef SEARCH_H
#define SEARCH_H


#include <thread>
#include <cstring>
#include "Timer.h"
#include "Board.h"
#include "SearchInfo.h"
#include "History.h"
#include "TranspositionTable.h"



// classe permettant de redéfinir mon 'locale'
// en effet, je n'en ai pas trouvé (windows ? mingw ?)
// de qui permet d'écrire un entier avec un séparateur : 1.000.000

class MyNumPunct : public std::numpunct<char>
{
protected:
    virtual char do_thousands_sep() const { return '.'; }
    virtual std::string do_grouping() const { return "\03"; }
};

constexpr int STACK_OFFSET = 4;
constexpr int STACK_SIZE   = MAX_PLY + 2*STACK_OFFSET;  // taille un peu trop grande, mais multiple de 8


//! \brief  Données d'une thread
class Search
{
public:
    Search();
    ~Search();

    std::thread         thread;
    History             history;
    NNUE                nnue;
    TranspositionTable* table = nullptr;

    //==============================================
    //  Evaluation
    [[nodiscard]] int evaluate(const Board &board);
    [[nodiscard]] int do_evaluate(const Board &board, Accumulator& acc);

    template <Color US, bool Update_NNUE> void make_move(Board& board, const MOVE move) noexcept;
    template <Color US, bool Update_NNUE> void undo_move(Board& board) noexcept;

    U64         td_nodes;
    U64         td_tbhits;

    int         td_index;
    int         td_depth;       // profondeur demandée dans iterative  deepening
    int         td_seldepth;
    int         td_score;
    bool        td_stopped;

    MOVE        td_best_move;
    int         td_best_score;
    int         td_best_depth;


    // Point de départ de la recherche
    template <Color C> void think(Board board, Timer timer, size_t _index);
    template <Color C> int  aspiration_window(Board& board, Timer& timer, SearchInfo* si);

private:
    inline const Accumulator& get_accumulator() const { return nnue.get_accumulator(); }
    inline       Accumulator& get_accumulator()       { return nnue.get_accumulator(); }

private:
    template <Color C> void iterative_deepening(Board& board, Timer& timer, SearchInfo* si);
    template <Color C> int  alpha_beta(Board& board, Timer& timer, int alpha, int beta, int depth, bool cut_node, SearchInfo* si);
    template <Color C> int  quiescence(Board& board, Timer& timer, int alpha, int beta, SearchInfo* si);

    void show_uci_result(I64 elapsed, const PVariation &pv) const;
    void show_uci_best() const;
    void update_pv(SearchInfo* si, const MOVE move) const;

    static constexpr int AspirationWindowsDepth    = 6;
    static constexpr int AspirationWindowsInitial  = 12;
    static constexpr int AspirationWindowsDelta    = 16;
    static constexpr int AspirationWindowsExpand   = 6667;

    static constexpr int LateMovePruningDepth = 7;
    static constexpr int LateMovePruningCount[2][8] = {
        {0, 2, 3, 4, 6, 8, 13, 18},
        {0, 3, 4, 6, 8, 12, 20, 30}
    };

    int Reductions[2][32][32];



}__attribute__((aligned(64)));


#endif // SEARCH_H
