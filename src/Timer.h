#ifndef TIMER_H
#define TIMER_H

class Timer;

#include <chrono>
#include "defines.h"
#include "types.h"

enum TimerMode {
    TIME,
    NODE
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
                 int nodes,
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
    Limits limits;

    void show_time();
    void debug(Color color) const;

    void start();
    void setup(Color color);
    void setup(int soft_limit, int hard_limit);
    bool finishOnThisMove(U64 nodes) const;
    bool finishOnThisDepth(int elapsed, int depth, const PVariation pvs[], U64 total_nodes);

    int  getSearchDepth() const { return(searchDepth); }
    int  elapsedTime() const;
    void setMoveOverhead(int n) { MoveOverhead = n;     }
    int  getMoveOverhead() const { return MoveOverhead; }

    void updateMoveNodes(MOVE move, U64 nodes);
    void update(int depth, const PVariation pvs[]);


private:

    // gives the exact moment this search was started.
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;

    int  mode;
    int  MoveOverhead;           // temps de réserve pour l'interface
    int  timeForThisDepth;       // temps pour "iterative deepening"
    int  timeForThisMove;        // temps pour une recherche "alpha-beta" ou "quiescence"
    int  searchDepth;
    U64  nodesForThisDepth;       // noeuds pour "iterative deepening"
    U64  nodesForThisMove;        // noeuds pour une recherche "alpha-beta" ou "quiescence"

    std::array<U64, 4096>   MoveNodeCounts;
    int                     pv_stability;

};

#endif // TIMER_H
