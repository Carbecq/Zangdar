#include <algorithm>
#include <chrono>
#include <iostream>
#include "Timer.h"
#include "Move.h"

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
    limits.infinite    = infinite;
    limits.time[WHITE] = wtime;
    limits.time[BLACK] = btime;
    limits.incr[WHITE] = winc;
    limits.incr[BLACK] = binc;
    limits.movestogo   = movestogo;
    limits.depth       = depth;
    limits.nodes       = nodes;
    limits.movetime    = movetime;

    mode               = TimerMode::TIME;
    MoveOverhead       = MOVE_OVERHEAD;
    timeForThisDepth   = 0;
    timeForThisMove    = 0;
    searchDepth        = 0;
    nodesForThisMove   = 0;
    nodesForThisDepth  = 0;

    // std::cout << "------------------------------------------Timer::Timer " << std::endl;

    // std::cout << "time_left   " << limits.time[WHITE] << "  " << limits.time[BLACK] << std::endl;
    // std::cout << "increment   " << limits.incr[WHITE] << "  " << limits.incr[BLACK] << std::endl;
    // std::cout << "moves_to_go " << limits.movestogo << std::endl;
    // std::cout << "depth       " << limits.depth << std::endl;
    // std::cout << "nodes       " << limits.nodes << std::endl;
    // std::cout << "move_time   " << limits.movetime << std::endl;
    // std::cout << "infinite    " << limits.infinite << std::endl;
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
    // std::cout << "------------------------------------------Timer::setup " << std::endl;

    // std::cout << "time_left   " << limits.time[WHITE] << "  " << limits.time[BLACK] << std::endl;
    // std::cout << "increment   " << limits.incr[WHITE] << "  " << limits.incr[BLACK] << std::endl;
    // std::cout << "moves_to_go " << limits.movestogo << std::endl;
    // std::cout << "depth       " << limits.depth << std::endl;
    // std::cout << "nodes       " << limits.nodes << std::endl;
    // std::cout << "move_time   " << limits.movetime << std::endl;
    // std::cout << "infinite    " << limits.infinite << std::endl;

    mode                = TimerMode::TIME;
    searchDepth         = MAX_PLY;
    timeForThisMove     = MAX_TIME;
    timeForThisDepth    = MAX_TIME;
    nodesForThisMove    = 0;
    nodesForThisDepth   = 0;

    if (limits.infinite) // recherche infinie (temps et profondeur)
    {
    }
    else if (limits.nodes != 0) // temps de recherche imposé = nombre de noeuds
    {
        mode                = TimerMode::NODE;
        nodesForThisMove    = limits.nodes;
        nodesForThisDepth   = limits.nodes;
    }
    else if (limits.depth != 0) // profondeur de recherche imposée = depth
    {
        searchDepth         = limits.depth;
    }
    else if (limits.movetime != 0) // temps de recherche imposé = move_time
    {
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


//============================================================
//! \brief  Impose des limites en mode "NODE"
//------------------------------------------------------------
void Timer::setup(const int soft_limit, const int hard_limit)
{
    mode              = TimerMode::NODE;
    searchDepth       = MAX_PLY;
    nodesForThisDepth = soft_limit;
    nodesForThisMove  = hard_limit;
}

//=========================================================
//! \brief  Controle du time-out
//! \return Retourne "true" si la recherche a dépassé sa limite de temps
//!
//! De façon à éviter un nombre important de mesure de temps , on ne fera
//! ce calcul que tous les MAX_COUNTER .
//---------------------------------------------------------
bool Timer::check_limits(const int depth, const int index, const U64 total_nodes)
{
    if (index == 0 && depth > 4)
    {
        if (mode == TimerMode::NODE)
        {
            return (total_nodes >= nodesForThisDepth);
        }
        else
        {
            // Every MAX_COUNTER , check if our time has expired.
            if (--counter > 0)
                return false;
            counter = MAX_COUNTER;

            return(elapsedTime() >= timeForThisMove);
        }
    }

    return false;
}

//===========================================================
//! \brief  Détermine si on a assez de temps pour effectuer
//!         une nouvelle itération
//! \param[in]  elapsed     temps en millisecondes
//! \param[in]  best_move   meilleur coup trouvé
//! \param[in]  total_nodes nombre total de noeuds calculés pour cette profondeur
//!
//-----------------------------------------------------------
bool Timer::finishOnThisDepth(int elapsed, int depth, MOVE best_move, U64 total_nodes)
{
    // Don't terminate early at very low depths
    // if (depth < 4)
    //     return false;    //TODO avec Datagen, limite trop forte

    if (mode == TimerMode::TIME)
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
    else
    {
        return (total_nodes >= nodesForThisDepth);
    }
}

//==============================================================================
//! \brief  Retourne le temps écoulé depuis le début de la recherche.
//------------------------------------------------------------------------------
I64 Timer::elapsedTime() const
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(TimePoint::now() - startTime).count();
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
void Timer::debug(Color color) const
{
    std::cout << "color: " << side_name[color] << std::endl
              << " mode              : " << mode << std::endl
              << " timeForThisDepth  : " << timeForThisDepth << std::endl
              << " timeForThisMove   : " << timeForThisMove << std::endl
              << " searchDepth       : " << searchDepth << std::endl
              << " moveOverhead      : " << MoveOverhead << std::endl
              << " nodesForThisDepth : " << nodesForThisDepth << std::endl
              << " nodesForThisMove  : " << nodesForThisMove << std::endl
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
