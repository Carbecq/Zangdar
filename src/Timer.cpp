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
    startTime = std::chrono::high_resolution_clock::now();

    std::fill(MoveNodeCounts.begin(), MoveNodeCounts.end(), 0);
    pv_stability = 0;
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

    searchDepth         = MAX_PLY;
    timeForThisMove     = MAX_TIME;
    timeForThisDepth    = MAX_TIME;
    nodesForThisMove    = 0;
    nodesForThisDepth   = 0;
    mode                = TimerMode::TIME;

    if (limits.infinite) // recherche infinie (temps et profondeur)
    {
        searchDepth         = MAX_PLY;
        timeForThisMove     = MAX_TIME;
        timeForThisDepth    = MAX_TIME;
    }
    else if (limits.nodes != 0) // temps de recherche imposé = nombre de noeuds
    {
        mode                = TimerMode::NODE;
        searchDepth         = MAX_PLY;
        nodesForThisMove    = limits.nodes;
        nodesForThisDepth   = limits.nodes;
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
        int time      = limits.time[color];
        int increment = limits.incr[color];
        int movestogo = limits.movestogo;

        // CCRL blitz : game in 2 minutes plus 1 second increment
        // CCRL 40/15 : 40 moves in 15 minutes
        // Amateur    : 12 minutes with 8 second increments.

        // Formules provenant d'Ethereal

        // partie : 40 coups en 15 minutes              : moves_to_go = 40 ; wtime=btime = 15*60000 ; winc=binc = 0
        if (movestogo > 0)
        {
            timeForThisDepth =  1.80 * (time - MoveOverhead) / (movestogo +  5) + increment;     // ou movestogo + 4
            timeForThisMove  = 10.00 * (time - MoveOverhead) / (movestogo + 10) + increment;
        }

        // partie en 5 minutes, incrément de 6 secondes : moves_to_go = 0  ; wtime=btime =  5*60000 ; winc=binc = 6 >> sudden death
        else
        {
            timeForThisDepth =  2.50 * ((time - MoveOverhead) + 25 * increment) / 50;
            timeForThisMove  = 10.00 * ((time - MoveOverhead) + 25 * increment) / 50;
        }

        // Cap time allocations using the move overhead
        timeForThisDepth = std::min(timeForThisDepth, time - MoveOverhead);
        timeForThisMove  = std::min(timeForThisMove,  time - MoveOverhead);
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
    mode = TimerMode::NODE;
    searchDepth       = MAX_PLY;
    nodesForThisDepth = soft_limit;
    nodesForThisMove  = hard_limit;
}

//============================================================
//! \brief  Mise à jour de la stabilité du meilleur coup
//! \param[in]  depth   profondeur terminée proprement (pas de time-out)
//! \param[in]  pvs     Variation Principale de chaque profondeur
//------------------------------------------------------------
void Timer::update(int depth, const PVariation pvs[])
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
//! \param[in]  elapsed     temps en millisecondes
//! \param[in]  depth       profondeur terminée proprement (pas de time-out)
//! \param[in]  pvs         Variation Principale de chaque profondeur
//! \param[in]  total_nodes nombre total de noeuds calculés pour cette profondeur
//!
//-----------------------------------------------------------
bool Timer::finishOnThisDepth(int elapsed, int depth, const PVariation pvs[], U64 total_nodes)
{
    // Don't terminate early at very low depths
    if (depth < 4)
        return false;

    if (mode==TimerMode::TIME)
    {
        // Scale time between 80% and 120%, based on stable best moves
        // Plus le meilleur coup est stable, plus pv_factor diminue
        const double pv_factor = 1.20 - 0.04 * pv_stability;

        // Scale time between 75% and 125%, based on score fluctuations
        const double score_change = pvs[depth-3].score - pvs[depth-0].score;
        const double score_factor = std::max(0.75, std::min(1.25, 0.05 * score_change));

        // Scale time between 50% and 240%, based on where nodes have been spent
        //   best_nodes                           = nombre de neuds calculés pour trouver le meilleur coup
        //   pvs[depth-0].line[0]                 = meilleur coup trouvé
        //   Move::fromdest(pvs[depth-0].line[0]) = fabrique un indice à partir des cases de départ et d'arrivée du coup
        const U64    best_nodes   = MoveNodeCounts[Move::fromdest(pvs[depth-0].line[0])];
        const double non_best_pct = 1.0 - (static_cast<double>(best_nodes) / static_cast<double>(total_nodes));
        const double nodes_factor = std::max(0.50, 2.0 * non_best_pct + 0.4);

        // printf("el=%d t=%d %f %f %f \n", elapsed, timeForThisDepth , pv_factor , score_factor , nodes_factor);
        return (elapsed > timeForThisDepth * pv_factor * score_factor * nodes_factor);
    }
    else
    {
        return (total_nodes >= nodesForThisDepth);
    }
}

//==============================================================================
//! \brief Détermine si le temps alloué pour la recherche est dépassé.
//------------------------------------------------------------------------------
bool Timer::finishOnThisMove(U64 nodes) const
{
    if (mode==TimerMode::TIME)
        return (elapsedTime() >= timeForThisMove);
    else
        return (nodes >= nodesForThisMove);
}

//==============================================================================
//! \brief  Retourne le temps écoulé depuis le début de la recherche.
//------------------------------------------------------------------------------
int Timer::elapsedTime() const
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
