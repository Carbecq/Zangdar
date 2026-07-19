#ifndef TIMER_H
#define TIMER_H

class Timer;

#include <chrono>
#include "defines.h"
#include "types.h"

enum TimerMode {
    TIME,
    DEPTH,
    NODE
};

struct Limits {
    int  time[2];     // temps restant pour Noirs et Blancs
    int  incr[2];     // incrément pour Noirs et Blancs
    int  movestogo;   // coups restants
    int  depth;       // limite la recherche par profondeur
    U64  nodes;       // limite la recherche par nombre de nodes cherchés
    int  movetime;    // limite la recherche par temps
    bool infinite;    // ignore les limites (recherche infinie)

    Limits() : time{}, incr{}, movestogo(0), depth(0), nodes(0), movetime(0), infinite(false) {}
};

class Timer
{
public:
    Timer(bool infinite,
          int wtime,
          int btime,
          int winc,
          int binc,
          int movestogo,
          int depth,
          U64 nodes,
          int movetime,
          int moveOverhead = MOVE_OVERHEAD);

    //===========================================================
    //! \brief  Constructeur par défaut
    //! Délègue au constructeur principal avec toutes les limites à zéro
    //-----------------------------------------------------------
    Timer() : Timer(false,
               0,
               0,
               0,
               0,
               0,
               0,
               0,
               0) {}

    void debug(Color color) const;

    void start();
    void setup(Color color);
    void setup(U64 soft_limit, U64 hard_limit);
    bool check_limits(const int depth, const int index, const U64 total_nodes);
    bool finishOnThisDepth(int elapsed, int depth, U64 total_nodes, const int* pv_scores, const MOVE *pv_moves);

    //===========================================================
    //! \brief  Retourne la profondeur de recherche imposée
    //-----------------------------------------------------------
    int  getSearchDepth() const { return(searchDepth); }
    I64  elapsedTime() const;

    void updateMoveNodes(MOVE move, U64 nodes);
    void update(int depth, MOVE last_move, MOVE this_move);

private:
    Limits limits;

    constexpr static U32 MAX_COUNTER = 2048;
    U32 counter {MAX_COUNTER};

    // donne le moment exact où cette recherche a démarré.
    TimePoint::time_point startTime;

    int  mode;
    int  moveOverhead;           // temps de réserve pour l'interface (option UCI MoveOverhead)
    I64  timeForThisDepth;       // temps pour "iterative deepening"
    I64  timeForThisMove;        // temps pour une recherche "alpha-beta" ou "quiescence"
    int  searchDepth;
    U64  nodesForThisDepth;       // noeuds pour "iterative deepening"
    U64  nodesForThisMove;        // noeuds pour une recherche "alpha-beta" ou "quiescence"

    std::array<U64, 4096>   MoveNodeCounts;
    U32                     pv_stability;

};

#endif // TIMER_H
