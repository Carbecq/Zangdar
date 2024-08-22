#include <algorithm>
#include <chrono>
#include <iostream>
#include "Timer.h"
#include "Move.h"

Timer::Timer()
{
    reset();
}

Timer::Timer(bool infinite,
             int wtime,
             int btime,
             int winc,
             int binc,
             int movestogo,
             int depth,
             int nodes,
             int movetime)
{
    limits.time[WHITE] = wtime;
    limits.time[BLACK] = btime;
    limits.incr[WHITE] = winc;
    limits.incr[BLACK] = binc;
    limits.movestogo   = movestogo;
    limits.depth       = depth;
    limits.nodes       = nodes;
    limits.movetime    = movetime;
    limits.infinite    = infinite;

    // timeForThisDepth    = 0;
    // timeForThisMove     = 0;
    searchDepth         = 0;
}

void Timer::init(bool infinite,
                 int wtime,
                 int btime,
                 int winc,
                 int binc,
                 int movestogo,
                 int depth,
                 int nodes,
                 int movetime)
{
    limits.time[WHITE] = wtime;
    limits.time[BLACK] = btime;
    limits.incr[WHITE] = winc;
    limits.incr[BLACK] = binc;
    limits.movestogo   = movestogo;
    limits.depth       = depth;
    limits.nodes       = nodes;
    limits.movetime    = movetime;
    limits.infinite    = infinite;

    // timeForThisDepth    = 0;
    // timeForThisMove     = 0;
    searchDepth         = 0;
}

void Timer::reset()
{
    limits.time[WHITE] = 0;
    limits.time[BLACK] = 0;
    limits.incr[WHITE] = 0;
    limits.incr[BLACK] = 0;
    limits.movestogo   = 0;
    limits.depth       = 0;
    limits.nodes       = 0;
    limits.movetime    = 0;
    limits.infinite    = false;

    // timeForThisDepth    = 0;
    // timeForThisMove     = 0;
    searchDepth         = 0;
}

//===========================================================
//! \brief Start the timer
//-----------------------------------------------------------
void Timer::start()
{
    std::fill(MoveNodeCounts.begin(), MoveNodeCounts.end(), 0);
    startTime = std::chrono::high_resolution_clock::now();
    pv_stability = 0;
    // PrevBestMove = Move::MOVE_NONE;
}

//===========================================================
//! \brief Initialisation des limites en temps pour la recherche
//-----------------------------------------------------------
void Timer::setup(Color color)
{
    //        std::cout << "time_left   " << limits.time[WHITE] << "  " << limits.time[BLACK] << std::endl;
    //        std::cout << "increment   " << limits.incr[WHITE] << "  " << limits.incr[BLACK] << std::endl;
    //        std::cout << "moves_to_go " << limits.movestogo << std::endl;
    //        std::cout << "depth       " << limits.depth << std::endl;
    //        std::cout << "nodes       " << limits.nodes << std::endl;
    //        std::cout << "move_time   " << limits.movetime << std::endl;
    //        std::cout << "infinite    " << limits.infinite << std::endl;

    searchDepth         = MAX_PLY;
    // timeForThisMove     = MAX_TIME;
    // timeForThisDepth    = MAX_TIME;

    if (limits.infinite) // recherche infinie (temps et profondeur)
    {
        searchDepth         = MAX_PLY;
        // timeForThisMove     = MAX_TIME;
        // timeForThisDepth    = MAX_TIME;
    }
    else if (limits.depth != 0) // profondeur de recherche imposée = depth
    {
        searchDepth         = limits.depth;
        // timeForThisMove     = MAX_TIME;
        // timeForThisDepth    = MAX_TIME;
    }
    else if (limits.movetime != 0) // temps de recherche imposé = move_time
    {
        searchDepth         = MAX_PLY;
        // timeForThisMove     = limits.movetime - MoveOverhead;
        // timeForThisDepth    = limits.movetime - MoveOverhead;
    }
    else if (limits.time[color] != 0)
    {
        int time      = limits.time[color];
        int increment = limits.incr[color];
        int movestogo = limits.movestogo;

        pv_stability = 0; // Clear our stability time usage heuristic
        // start_time = limits->start; // Save off the start time of the search
        // memset( nodes, 0, sizeof(uint16_t) * 0x10000); // Clear Node counters

        // Allocate time if Ethereal is handling the clock

        // Playing using X / Y + Z time control
        if (movestogo > 0)
        {
            ideal_usage =  1.80 * (time - MoveOverhead) / (movestogo +  5) + increment;     // ou movestogo + 4
            max_usage   = 10.00 * (time - MoveOverhead) / (movestogo + 10) + increment;
        }

        // Playing using X + Y time controls
        else
        {
            ideal_usage =  2.50 * ((time - MoveOverhead) + 25 * increment) / 50;
            max_usage   = 10.00 * ((time - MoveOverhead) + 25 * increment) / 50;
        }

        // Cap time allocations using the move overhead
        ideal_usage = std::min(ideal_usage, time - MoveOverhead);
        max_usage   = std::min(max_usage,   time - MoveOverhead);









        // formules provenant de Sirius (provenant elle-mêmes de Stormphrax)
        // m_SoftBound
        // timeForThisDepth = softTimeScale / 100.0 * (time / baseTimeScale + increment * incrementScale / 100.0);

        // m_HardBound
        // timeForThisMove = time * (hardTimeScale / 100.0);
    }

    debug();

#if defined DEBUG_TIME
    debug();
#endif
}

void Timer::update(int depth, PVariation pvs[MAX_PLY])
{
    // Don't update our Time Managment plans at very low depths
    if (depth < 4)
        return;

    MOVE this_move = pvs[depth-0].line[0];
    MOVE last_move = pvs[depth-1].line[0];

    pv_stability = (this_move == last_move) ? std::min(10, pv_stability + 1) : 0;
}

//===========================================================
//! \brief  Détermine si on a assez de temps pour effectuer
//!         une nouvelle itération
//! \param  elapsedTime     temps en millisecondes
//!
//-----------------------------------------------------------
bool Timer::finishOnThisDepth(U64 elapsed, int depth, PVariation pvs[MAX_PLY], U64 total_nodes)
{
    // stopSoft

    // Don't terminate early at very low depths
    if (depth < 4)
        return false;

    // Scale time between 80% and 120%, based on stable best moves
    const double pv_factor = 1.20 - 0.04 * pv_stability;

    // Scale time between 75% and 125%, based on score fluctuations
    const double score_change =   pvs[depth-3].score
                                - pvs[depth-0].score;
    const double score_factor = std::max(0.75, std::min(1.25, 0.05 * score_change));

    // Scale time between 50% and 240%, based on where nodes have been spent
    const U64 best_nodes = MoveNodeCounts[Move::fromdest(pvs[depth-0].line[0])];

    const double non_best_pct = 1.0 - (static_cast<double>(best_nodes) / static_cast<double>(total_nodes));

    const double nodes_factor = std::max(0.50, 2.0 * non_best_pct + 0.4);

    std::cout << "finish : el= " << elapsed << "  ? " << ideal_usage * pv_factor * score_factor * nodes_factor << std::endl;

    return (elapsed > ideal_usage * pv_factor * score_factor * nodes_factor);


    // if (best_move == PrevBestMove)
    //     pv_stability++;
    // else
    //     pv_stability = 0;
    // PrevBestMove = best_move;

    // double bmNodes = static_cast<double>(MoveNodeCounts[Move::fromdest(best_move)]) / static_cast<double>(total_nodes);
    // double scale = ((nodeTMBase / 100.0) - bmNodes) * (nodeTMScale / 100.0);
    // //                  (1.45 - bnm ) * 1.67
    // //   std::cout << "elapsed=" <<elapsed <<  " total=" << total_nodes << " scale=" << scale << " time=" << timeForThisDepth << "  " << timeForThisDepth*scale << std::endl;
    // scale *= stabilityValues[std::min(pv_stability, 6u)];
    // return (elapsed > timeForThisDepth * scale);

}

//==============================================================================
//! \brief Détermine si le temps alloué pour la recherche est dépassé.
//------------------------------------------------------------------------------
bool Timer::finishOnThisMove() const
{
    // stopHard

    auto fin = std::chrono::high_resolution_clock::now();
    U64  elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(fin - startTime).count();

   if (elapsed >= max_usage)
       std::cout << "finish move : el=" << elapsed << " ; " << max_usage << std::endl;
    return (elapsed >= max_usage);
}

//==============================================================================
//! \brief  Retourne le temps écoulé depuis le début de la recherche.
//------------------------------------------------------------------------------
int Timer::elapsedTime()
{
    return (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime).count());
}

//==================================================================
//! \brief Affiche le temps passé en millisecondes
//------------------------------------------------------------------
void Timer::show_time()
{
    // Elapsed time in milliseconds
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - startTime).count();
    std::cout << "Time           " << ms / 1000.0 << " s" << std::endl;
}

//==================================================================
//! \brief Affiche des informations de debug
//------------------------------------------------------------------
void Timer::debug()
{
    std::cout << "ideal_usage: " << ideal_usage
              << " max_usage: " << max_usage
              << " searchDepth: " << searchDepth
              << " moveOverhead: " << MoveOverhead
              << std::endl;
}

//==================================================================
//! \brief Met à jour m_NodeCounts
//! \param[in] move     coup cherché
//! \param[in] nodes    nombre de noeuds cherchés pour trouver ce coup
//------------------------------------------------------------------
void Timer::updateMoveNodes(MOVE move, U64 nodes)
{
    MoveNodeCounts[Move::fromdest(move)] += nodes;
}
