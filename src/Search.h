#ifndef SEARCH_H
#define SEARCH_H


class Search;

#include <thread>
#include "defines.h"
#include "Timer.h"
#include "OrderInfo.h"
#include "Board.h"
#include "types.h"

constexpr int STACK_OFFSET = 4;
constexpr int STACK_SIZE   = MAX_PLY + STACK_OFFSET;

struct SearchInfo {
    MOVE   killer1;     // killer moves
    MOVE   killer2;
    MOVE   excluded;    // coup à éviter
    int    eval;        // évaluation statique
    MOVE   move;        // coup cherché


}__attribute__((aligned(64)));

//! \brief  Données d'une thread
struct ThreadData {
    std::thread thread;
    U64         nodes;
    U64         tbhits;
    int         index;
    MOVE        best_move;
    int         best_score;
    int         best_depth;
    int         score;
    int         depth;
    int         seldepth;
    bool        stopped;
    
    OrderInfo   order;
    SearchInfo  info[STACK_SIZE];


}__attribute__((aligned(64)));


// classe permettant de redéfinir mon 'locale'
// en effet, je n'en ai pas trouvé (windows ? mingw ?)
// de qui permet d'écrire un entier avec un séparateur : 1.000.000

class MyNumPunct : public std::numpunct<char>
{
protected:
    virtual char do_thousands_sep() const { return '.'; }
    virtual std::string do_grouping() const { return "\03"; }
};


class Search
{
public:
    Search();
    ~Search();

    // Point de départ de la recherche
    template<Color C>
    void think(const Board &m_board, const Timer &m_timer, int _index);

private:
    Timer   timer;
    Board   board;

    template <Color C> void iterative_deepening(ThreadData* td, SearchInfo* si);
    template <Color C> int  aspiration_window(PVariation& pv, ThreadData* td, SearchInfo* si);
    template <Color C> int  alpha_beta(int ply, int alpha, int beta, int depth, PVariation& pv, ThreadData* td, SearchInfo* si);
    template <Color C> int  quiescence(int ply, int alpha, int beta, ThreadData* td, SearchInfo* si);

    void show_uci_result(const ThreadData *td, U64 elapsed, PVariation &pv) const;
    void show_uci_best(const ThreadData *td) const;
    void show_uci_current(MOVE move, int currmove, int depth) const;
    bool check_limits(const ThreadData *td) const;
    void update_pv(PVariation &pv, const PVariation &new_pv, const MOVE move) const;

    void update_killers(ThreadData* td, int ply, MOVE move);
    MOVE get_counter(ThreadData* td, Color color, MOVE prev_move);

    static constexpr int CONTEMPT    = 0;

    int Reductions[2][32][32];

};

#endif // SEARCH_H
