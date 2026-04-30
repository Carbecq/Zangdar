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

        I64 time      = limits.time[color];
        int increment = limits.incr[color];
        int mtg       = limits.movestogo;

        // Attention, on peut avoir time<0 (low time / lag)
        I64 time_remaining = std::max(static_cast<I64>(1), time - MoveOverhead);

        double tr  = static_cast<double>(time_remaining);
        double inc = static_cast<double>(increment);

        // CCRL 40/15
        // note : la cadence 40 coups en 15 minutes n'ets plus utilisée de nos jours.
        //        c'est une relique obsolète du passé.
        //        Maintenant les tournois utilisent une horloge avec incrément.
        if (mtg > 0)
        {
            // X / Y + Z time control
            // Cadence à nombre de coups imposé (ex: 40/15)
            timeForThisDepth =  1.80 * tr / (mtg +  5) + inc;   // SoftBound : temps moyen par coup, pondéré par l'incrément
            timeForThisMove  = 10.00 * tr / (mtg + 10) + inc;   // HardBound
        }
        else
        {
            // Sudden death (X + Y)
            timeForThisDepth =  2.50 * (tr + 25.0 * inc) / 50.0;
            timeForThisMove  = 10.00 * (tr + 25.0 * inc) / 50.0;
        }

        // Cap par le temps disponible (overhead déjà retiré)
        timeForThisDepth = std::min(timeForThisDepth, time_remaining);
        timeForThisMove  = std::min(timeForThisMove,  time_remaining);
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
//! \param[in]  depth       profondeur du meilleur coup
//! \param[in]  total_nodes nombre total de noeuds calculés pour cette profondeur
//! \param[in]  pv_scores   historique des meilleurs scores
//! \param[in]  pv_moves    historique des meilleurs coups
//!
//-----------------------------------------------------------
bool Timer::finishOnThisDepth(int elapsed, int depth, U64 total_nodes, const int* pv_scores, const MOVE* pv_moves)
{
    // Formules provenant d'Ethereal

    if (mode == TimerMode::TIME && depth >= 4)
    {
        // Scale time between 80% and 120%, based on stable best moves
        const double pv_factor = 1.20 - 0.04 * pv_stability;

        // score_change : positif si on s'est dégradé
        assert(pv_scores != nullptr);
        const int score_change = pv_scores[depth - 3] - pv_scores[depth];

        // Scale time between 75% and 125%, based on score fluctuations
        const double score_factor = std::max(0.75, std::min(1.25, 0.05 * score_change));

        // Scale time between 50% and 240%, based on where nodes have been spent
        assert(pv_moves != nullptr);
        const double bmNodes = (total_nodes > 0)
                ? static_cast<double>(MoveNodeCounts[Move::fromdest(pv_moves[depth])]) / static_cast<double>(total_nodes)
                : 0.0;
        const double non_best_pct = 1.0 - bmNodes;
        const double nodes_factor = std::max(0.50, 2.0 * non_best_pct + 0.4);

        return (elapsed > timeForThisDepth * pv_factor * score_factor * nodes_factor);
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

void Timer::update(int depth, MOVE last_move, MOVE this_move)
{
    // Don't update our Time Managment plans at very low depths
    if (mode != TimerMode::TIME || depth < 4)
        return;

    // Track how long we've kept the same best move between iterations
    pv_stability = (this_move == last_move) ? std::min(10U, pv_stability + 1) : 0;
}
