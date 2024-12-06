#include <algorithm>
#include <chrono>
#include <iostream>
#include "Timer.h"
#include "Move.h"

Timer::Timer()
{
    MoveOverhead    = MOVE_OVERHEAD;
    reset();
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

    timeForThisDepth   = 0;
    timeForThisMove    = 0;
    searchDepth        = 0;
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

    timeForThisDepth   = 0;
    timeForThisMove    = 0;
    searchDepth        = 0;
}

//===========================================================
//! \brief Start the timer
//-----------------------------------------------------------
void Timer::start()
{
    startTime = TimePoint::now();
    std::fill(MoveNodeCounts.begin(), MoveNodeCounts.end(), 0);
    pv_stability = 0;
    counter      = MAX_COUNTER;
    PrevBestMove = Move::MOVE_NONE;
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
    timeForThisMove     = MAX_TIME;
    timeForThisDepth    = MAX_TIME;

    if (limits.infinite) // recherche infinie (temps et profondeur)
    {
        searchDepth         = MAX_PLY;
        timeForThisMove     = MAX_TIME;
        timeForThisDepth    = MAX_TIME;
    }
    else if (limits.depth != 0) // profondeur de recherche imposée = depth
    {
        searchDepth         = limits.depth;
        timeForThisMove     = MAX_TIME;
        timeForThisDepth    = MAX_TIME;
    }
    else if (limits.movetime != 0) // temps de recherche imposé = move_time
    {
        searchDepth         = MAX_PLY;
        timeForThisMove     = limits.movetime - MoveOverhead;
        timeForThisDepth    = limits.movetime - MoveOverhead;
    }
    else if (limits.time[color] != 0)
    {
        I64 time      = limits.time[color];
        int increment = limits.incr[color];
        // int movestogo = limits.movestogo;

        // Attention, on peut avoir time<0
        I64 time_remaining = std::max(static_cast<I64>(1), time - MoveOverhead);

        // CCRL blitz : game in 2 minutes plus 1 second increment
        // CCRL 40/15 : 40 moves in 15 minutes
        // Amateur    : 12 minutes with 8 second increments.

        // partie : 40 coups en 15 minutes              : moves_to_go = 40 ; wtime=btime = 15*60000 ; winc=binc = 0
        // partie en 5 minutes, incrément de 6 secondes : moves_to_go = 0  ; wtime=btime =  5*60000 ; winc=binc = 6 >> sudden death

        // formules provenant de Sirius (provenant elle-mêmes de Stormphrax)
        // m_SoftBound
        timeForThisDepth = softTimeScale / 100.0 * (time_remaining / baseTimeScale + increment * incrementScale / 100.0);
        // m_HardBound
        timeForThisMove = time_remaining * (hardTimeScale / 100.0);

   }

#if defined DEBUG_TIME
    debug(color);
#endif
}


//===========================================================
//! \brief  Détermine si on a assez de temps pour effectuer
//!         une nouvelle itération
//! \param[in]  elapsed     temps en millisecondes
//! \param[in]  best_move   meilleur coup trouvé
//! \param[in]  total_nodes nombre total de noeuds calculés pour cette profondeur
//!
//-----------------------------------------------------------
bool Timer::finishOnThisDepth(int elapsed, MOVE best_move, U64 total_nodes)
{
    if (best_move == PrevBestMove)
        pv_stability++;
    else
        pv_stability = 0;
    PrevBestMove = best_move;

    double bmNodes = static_cast<double>(MoveNodeCounts[Move::fromdest(best_move)]) / static_cast<double>(total_nodes);
    double scale = ((nodeTMBase / 100.0) - bmNodes) * (nodeTMScale / 100.0);
    //                  (1.45 - bnm ) * 1.67
    //   std::cout << "elapsed=" <<elapsed <<  " total=" << total_nodes << " scale=" << scale << " time=" << timeForThisDepth << "  " << timeForThisDepth*scale << std::endl;
    scale *= stabilityValues[std::min(pv_stability, 6u)];
    return (elapsed > timeForThisDepth * scale);
}

//==============================================================================
//! \brief Détermine si le temps alloué pour la recherche est dépassé.
//------------------------------------------------------------------------------
bool Timer::finishOnThisMove()
{
    if (--counter > 0)
        return false;
    counter = MAX_COUNTER;
    return (elapsedTime() >= timeForThisMove);
}

//==============================================================================
//! \brief  Retourne le temps écoulé depuis le début de la recherche.
//------------------------------------------------------------------------------
I64 Timer::elapsedTime() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime).count();
}

//==================================================================
//! \brief Affiche le temps passé en millisecondes
//------------------------------------------------------------------
void Timer::show_time()
{
    // Elapsed time in seconds
    std::cout << "Time           " << elapsedTime() / 1000.0 << " s" << std::endl;
}

//==================================================================
//! \brief Affiche des informations de debug
//------------------------------------------------------------------
void Timer::debug(Color color)
{
    std::cout << "color: " << side_name[color]
              << " timeForThisDepth: " << timeForThisDepth
              << " timeForThisMove: "  << timeForThisMove
              << " searchDepth: "      << searchDepth
              << " moveOverhead: "     << MoveOverhead
              << std::endl;
}

//==================================================================
//! \brief Met à jour MoveNodeCounts
//! \param[in] move     coup cherché
//! \param[in] nodes    nombre de noeuds cherchés pour trouver ce coup
//------------------------------------------------------------------
void Timer::updateMoveNodes(MOVE move, U64 nodes)
{
    MoveNodeCounts[Move::fromdest(move)] += nodes;
}
