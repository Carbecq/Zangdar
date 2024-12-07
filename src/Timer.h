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
    bool check_limits(const int depth, const int index);
    bool finishOnThisDepth(int elapsed, MOVE best_move, U64 total_nodes);

    int  getSearchDepth() const { return(searchDepth); }
    I64  elapsedTime() const;
    void setMoveOverhead(I64 n) { MoveOverhead = n;     }
    I64  getMoveOverhead() const { return MoveOverhead; }

    void updateMoveNodes(MOVE move, U64 nodes);
    void update(int depth, const PVariation pvs[]);

private:
    static constexpr int softTimeScale = 57;
    static constexpr int hardTimeScale = 62;
    static constexpr int baseTimeScale = 20;
    static constexpr int incrementScale = 83;
    static constexpr int nodeTMBase = 145;
    static constexpr int nodeTMScale = 167;

    static constexpr std::array<double, 7> stabilityValues = {
        2.2, 1.6, 1.4, 1.1, 1, 0.95, 0.9
    };

    constexpr static U32 MAX_COUNTER = 2048;
    U32 counter {MAX_COUNTER};

    // gives the exact moment this search was started.
    TimePoint::time_point startTime;

    I64  MoveOverhead;           // temps de réserve pour l'interface
    I64  timeForThisDepth;       // temps pour "iterative deepening"
    I64  timeForThisMove;        // temps pour une recherche "alpha-beta" ou "quiescence"
    int  searchDepth;

    std::array<U64, 4096>   MoveNodeCounts;
    MOVE                    PrevBestMove;
    U32                     pv_stability;

};

#endif // TIMER_H
