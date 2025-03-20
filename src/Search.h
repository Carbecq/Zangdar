#ifndef SEARCH_H
#define SEARCH_H


class Search;

#include <cstring>
#include "Timer.h"
#include "Board.h"
#include "types.h"
#include "defines.h"
#include "ThreadData.h"


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
    template <Color C> void think(Board board, Timer timer, int _index);
    template <Color C> int  aspiration_window(Board& board, Timer& timer, ThreadData* td, SearchInfo* si);

private:
    template <Color C> void iterative_deepening(Board& board, Timer& timer, ThreadData* td, SearchInfo* si);
    template <Color C> int  alpha_beta(Board& board, Timer& timer, int alpha, int beta, int depth, ThreadData* td, SearchInfo* si);
    template <Color C> int  quiescence(Board& board, Timer& timer, int alpha, int beta, ThreadData* td, SearchInfo* si);

    void show_uci_result(const ThreadData *td, I64 elapsed, const PVariation &pv) const;
    void show_uci_best(const ThreadData *td) const;

    static constexpr int CONTEMPT    = 0;

    int Reductions[2][32][32];

    static constexpr int LateMovePruningDepth = 7;
    static constexpr int LateMovePruningCount[2][8] = {
        {0, 2, 3, 4, 6, 8, 13, 18},
        {0, 3, 4, 6, 8, 12, 20, 30}
    };

    static constexpr int ContinuationMovePruningDepth[] = { 3, 2 };
    static constexpr int ContinuationMoveHistoryLimit[] = { 0, -1000 };

    static constexpr int SEEPruningDepth =  9;
    static constexpr int SEEQuietMargin = -64;
    static constexpr int SEENoisyMargin = -19;

    static constexpr int ProbCutDepth  =   5;
    static constexpr int ProbCutMargin = 100;

};

#endif // SEARCH_H
