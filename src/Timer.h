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
    int  time[2];     // time left for black and white
    int  incr[2];     // increment for black and white
    int  movestogo;   // moves
    int  depth;       // limit search by depth
    U64  nodes;       // limit search by nodes searched
    int  movetime;    // limit search by time
    bool infinite;    // ignore limits (infinite search)

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
          int movetime);

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

    int  getSearchDepth() const { return(searchDepth); }
    I64  elapsedTime() const;
    void setMoveOverhead(I64 n) { MoveOverhead = n;     }
    I64  getMoveOverhead() const { return MoveOverhead; }

    void updateMoveNodes(MOVE move, U64 nodes);
    void update(int depth, MOVE last_move, MOVE this_move);

private:
    Limits limits;

    constexpr static U32 MAX_COUNTER = 2048;
    U32 counter {MAX_COUNTER};

    // gives the exact moment this search was started.
    TimePoint::time_point startTime;

    int  mode;
    I64  MoveOverhead;           // temps de réserve pour l'interface
    I64  timeForThisDepth;       // temps pour "iterative deepening"
    I64  timeForThisMove;        // temps pour une recherche "alpha-beta" ou "quiescence"
    int  searchDepth;
    U64  nodesForThisDepth;       // noeuds pour "iterative deepening"
    U64  nodesForThisMove;        // noeuds pour une recherche "alpha-beta" ou "quiescence"

    std::array<U64, 4096>   MoveNodeCounts;
    U32                     pv_stability;

};

#endif // TIMER_H
