#ifndef TIMER_H
#define TIMER_H

class Timer;

#include <chrono>
#include "defines.h"
#include "types.h"

class Timer
{
public:
    Timer();
    void init(bool infinite,
                      int wtime,
                      int btime,
                      int winc,
                      int binc,
                      int movestogo,
                      int depth,
                      int nodes,
                      int movetime);

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

    void reset();
    void show_time();
    void debug(Color color);

    void start();
    void setup(Color color);
    bool finishOnThisMove();
    bool finishOnThisDepth(int elapsed, int depth, const PVariation pvs[], U64 total_nodes);

    int  getSearchDepth() const { return(searchDepth); }
    I64  elapsedTime() const;
    void setMoveOverhead(I64 n) { MoveOverhead = n;     }
    I64  getMoveOverhead() const { return MoveOverhead; }

    void updateMoveNodes(MOVE move, U64 nodes);
    void update(int depth, const PVariation pvs[]);

private:
    constexpr static U32 MAX_COUNTER = 1024;
    U32 counter {MAX_COUNTER};

    // gives the exact moment this search was started.
    TimePoint::time_point startTime;

    I64  MoveOverhead;           // temps de réserve pour l'interface
    I64  timeForThisDepth;       // temps pour "iterative deepening"
    I64  timeForThisMove;        // temps pour une recherche "alpha-beta" ou "quiescence"
    int  searchDepth;

    std::array<U64, 4096>   MoveNodeCounts;
    int                     pv_stability;

};

#endif // TIMER_H
