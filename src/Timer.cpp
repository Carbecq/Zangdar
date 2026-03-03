#include <algorithm>
#include <chrono>
#include <iostream>
#include "Timer.h"
#include "Move.h"
#include "Tunable.h"

//===========================================================
//! \brief  Constructeur
//! Initialise les limites de temps fournies par le protocole UCI
//-----------------------------------------------------------
Timer::Timer(bool infinite,
             int wtime,
             int btime,
             int winc,
             int binc,
             int movestogo,
             int depth,
             U64 nodes,
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
        mode                = TimerMode::DEPTH;
        searchDepth         = limits.depth;
    }
    else if (limits.movetime != 0) // temps de recherche imposé = move_time
    {
        // Dans ce cas, on n'utilise pas MoveOverhead
        timeForThisMove     = limits.movetime;
        timeForThisDepth    = limits.movetime;
    }
    else if (limits.time[color] != 0)
    {
        I64 time      = limits.time[color];
        int increment = limits.incr[color];
        int mtg       = limits.movestogo;

        // Attention, on peut avoir time<0
        I64 time_remaining = std::max(static_cast<I64>(1), time - MoveOverhead);

        // CCRL blitz : game in 2 minutes plus 1 second increment
        // CCRL 40/15 : 40 moves in 15 minutes
        // Amateur    : 12 minutes with 8 second increments.

        //  CCRL 40/15 : 40 coups en 15 minutes, sans incrément
        //  TC=40/15:00
        //  go wtime 900000 btime 900000 movestogo 40

        //  CCRL blitz : game in 2 minutes plus 1 second increment
        //  tc=3:00+1
        //  go wtime 181000 btime 181000 winc 1000 binc 1000

        //  STC : partie en 10 secondes, incrément de 0.1 secondes
        //  tc=10+0.1
        //  go wtime 10100 btime 10100 winc 100 binc 100

        //  LTC : partie en 1 minute, incrément de 0.6 secondes
        //  tc=60+0.6
        //  TC=1:00+0.6
        //  go wtime 60600 btime 60600 winc 600 binc 600

        //  VLTC : partie en 5 minutes, incrément de 3 secondes :
        //  tc=5:00+3
        //  go wtime 303000 btime 303000 winc 3000 binc 3000

        // partie en 12 minutes, incrément de 8 secondes :
        //  tc = 12:00+8
        //  go wtime 728000 btime 728000 winc 8000 binc 8000

        // CCRL 40/15
        if (mtg > 0)
        {
            // Cadence à nombre de coups imposé (ex: 40/15)
            // m_SoftBound : temps moyen par coup, pondéré par l'incrément
            timeForThisDepth = Tunable::softTimeScale / 100.0 * (static_cast<double>(time_remaining) / mtg
                             + static_cast<double>(increment) * Tunable::incrementScale / 100.0);
            // m_HardBound : au plus 4x le temps moyen par coup
            timeForThisMove  = std::min(time_remaining * (Tunable::hardTimeScale / 100.0), static_cast<double>(time_remaining) / mtg * 4.0);

            // Cap de sécurité : tenir compte de l'incrément
            I64 safetyCap = static_cast<I64>(time_remaining * 0.80 + increment * 0.50);
            timeForThisMove = std::min(timeForThisMove, std::min(safetyCap, time_remaining));
        }
        else
        {
            // Sudden death (formules Stormphrax)
            // m_SoftBound
            timeForThisDepth = Tunable::softTimeScale / 100.0 * (static_cast<double>(time_remaining) / Tunable::baseTimeScale
                             + static_cast<double>(increment) * Tunable::incrementScale / 100.0);
            // m_HardBound
            timeForThisMove = time_remaining * (Tunable::hardTimeScale / 100.0);
        }

        timeForThisDepth = std::min(timeForThisDepth, timeForThisMove);
    }

#if defined DEBUG_TIME
    debug(color);
#endif
}


//============================================================
//! \brief  Impose des limites en mode "NODE"
//------------------------------------------------------------
void Timer::setup(U64 soft_limit, U64 hard_limit)
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
    if (index == 0)
    {
        if (mode == TimerMode::TIME && depth >= 4)
        {
            // ce mode est utilisé :
            //  > pour le jeu normal
            // Every MAX_COUNTER , check if our time has expired.
            if (--counter > 0)
                return false;
            counter = MAX_COUNTER;

            return(elapsedTime() > timeForThisMove);
        }
        else if (mode == TimerMode::NODE)
        {
            // ce mode est utilisé :
            //  > pour datagen,
            //  > pour des tests perso (run ...)
            return (total_nodes > nodesForThisDepth);
        }
        else if (mode == TimerMode::DEPTH)
        {
            // ce mode est utilisé :
            //  > pour le bench
            //  > pour datagen, dans la partie random_game
            //  > pour des tests perso (run ...)
            return (depth > searchDepth);
        }
        else
        {
            // Problème
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
    if (mode == TimerMode::TIME && depth >= 4)
    {
        if (best_move == PrevBestMove)
            pv_stability++;
        else
            pv_stability = 0;
        PrevBestMove = best_move;

        double bmNodes = (total_nodes > 0) ?
                    static_cast<double>(MoveNodeCounts[Move::fromdest(best_move)]) / static_cast<double>(total_nodes) : 0.0;
        double scale = ((Tunable::nodeTMBase / 100.0) - bmNodes) * (Tunable::nodeTMScale / 100.0);
        scale *= stabilityValues[std::min(pv_stability, 6u)];
        return (elapsed > timeForThisDepth * scale);
    }
    else if (mode == TimerMode::NODE)
    {
        return (total_nodes > nodesForThisDepth);
    }
    else
    {
        return (depth > searchDepth);   // on va de 1 à max_depth inclus
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
