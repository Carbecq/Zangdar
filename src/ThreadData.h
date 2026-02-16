#ifndef THREADDATA_H
#define THREADDATA_H

class ThreadData;

#include <cstring>
#include <thread>
#include "defines.h"
#include "types.h"
#include "History.h"
#include "Search.h"

constexpr int STACK_OFFSET = 4;
constexpr int STACK_SIZE   = MAX_PLY + 2*STACK_OFFSET;  // taille un peu trop grande, mais multiple de 8


//! \brief  Données d'une thread
class ThreadData
{
public:
    ThreadData();

    std::thread thread;
    Search*     search;     // pointeur sur la classe de recherche
    U64         nodes;
    U64         tbhits;
    History     history;

    int         index;
    int         depth;
    int         seldepth;
    int         score;
    bool        stopped;

    MOVE        best_move;
    int         best_score;
    int         best_depth;

    NNUE*       nnue;

    //==============================================
    //  Evaluation
    [[nodiscard]] int evaluate(const Board &board);
    [[nodiscard]] int do_evaluate(const Board &board, Accumulator& acc);

    template <Color US, bool Update_NNUE> void make_move(Board& board, const MOVE move) noexcept;
    template <Color US, bool Update_NNUE> void undo_move(Board& board) noexcept;

private:
    inline const Accumulator& get_accumulator() const { return nnue->get_accumulator(); }
    inline       Accumulator& get_accumulator()       { return nnue->get_accumulator(); }

}__attribute__((aligned(64)));


#endif // THREADDATA_H
