#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <map>
#include "ThreadPool.h"
#include "Board.h"
#include "Search.h"
#include "Move.h"


//=================================================
//! \brief  Constructeur avec arguments
//!
//! \param[in]  _nbr    nombre de threads de recherche à créer
//! \param[in]  _tb     utilisation des tables Syzygy
//! \param[in]  _log    affichage des informations UCI pendant la recherche
//-------------------------------------------------
ThreadPool::ThreadPool(U32 _nbr, bool _tb, bool _log) :
    nbrThreads(0),
    useSyzygy(_tb),
    syzygyProbeLimit(0),
    logUci(_log)
{
#if defined DEBUG_LOG
    char message[200];
    sprintf(message, "ThreadPool::constructeur : nbrThreads=%d ; useSyzygy=%d ; logUci=%d ", _nbr, _tb, _log);
    printlog(message);
#endif

    set_threads(_nbr);
}

//=================================================
//! \brief  Initialisation du nombre de threads
//!
//! \param[in]  nbr   nombre de threads demandé (sera borné entre 1 et
//!                   le nombre de processeurs disponibles / MAX_THREADS)
//-------------------------------------------------
void ThreadPool::set_threads(U32 nbr)
{
#if defined DEBUG_LOG
    char message[200];
    sprintf(message, "ThreadPool::set_threads : nbrThreads=%d ", nbr);
    printlog(message);
#endif

    U32 processorCount = static_cast<U32>(std::thread::hardware_concurrency());
    if (processorCount == 0)
        processorCount = MAX_THREADS;

    // Limite le nombre de threads au nombre de processeurs
    U32 newNbr = std::min(nbr, processorCount);
    newNbr     = std::max(newNbr, 1U);
    newNbr     = std::min(newNbr, MAX_THREADS);

    // Réallouer uniquement si le nombre change
    if (newNbr != nbrThreads)
    {
        // Arrêter toute recherche en cours avant de réallouer
        if (search)
            stop();

        nbrThreads = newNbr;
        search = std::make_unique<Search[]>(nbrThreads);
        for (size_t i = 0; i < nbrThreads; i++)
        {
            search[i].table = nullptr;
            search[i].index = i;
        }
    }

#if defined DEBUG_LOG
    sprintf(message, "ThreadPool::set_threads : nbrThreads=%d ", nbrThreads);
    printlog(message);
#endif
}

//=================================================
//! \brief  Initialisation des valeurs des threads
//! Utilisé lors de "newgame"
//-------------------------------------------------
void ThreadPool::reset()
{
    for (size_t i = 0; i < nbrThreads; i++)
        search[i].history.reset();
}

//=================================================
//! \brief  Réinitialise la table de réductions LMR de toutes les threads
//! Utilisé après un changement des TunableParam (setoption)
//-------------------------------------------------
void ThreadPool::reinit_reductions()
{
    for (size_t i = 0; i < nbrThreads; i++)
        search[i].init_reductions();
}

//=================================================
//! \brief  Lance la recherche
//! Fonction lancée par Uci::parse_go
//!
//! \param[in]  board   position de départ de la recherche
//! \param[in]  timer   gestion du temps alloué à la recherche
//-------------------------------------------------
void ThreadPool::start_thinking(const Board& board, const Timer& timer)
{
#if defined DEBUG_LOG
    char message[100];
    sprintf(message, "ThreadPool::start_thinking");
    printlog(message);
#endif

    // Garantit qu'aucune recherche précédente n'est encore active (double "go")
    stop();

    MOVE best = Move::MOVE_NONE;

    //  une ouverture est choisie par la GUI, depuis le book, et les moteurs jouent à partir de là
    //  c'est ainsi que fonctionnent les tests de moteurs, ou les tournois

    // Probe Syzygy TableBases : joue directement le coup DTZ-optimal si la position est dans les TB.
    if (useSyzygy && board.probe_root(best) == true)
    {
        transpositionTable.update_age();
        std::cout << "bestmove " << Move::name(best) << std::endl;
    }

    else
    {
#if defined DEBUG_LOG
        sprintf(message, "ThreadPool::start_thinking ; lancement de %d threads", nbrThreads);
        printlog(message);
#endif

        searchStopped.store(false, std::memory_order_relaxed);

        for (size_t i = 0; i < nbrThreads; i++)
        {
            search[i].stopFlagPtr     = &searchStopped;
            search[i].seldepth        = 0;
            search[i].nodes           = 0;
            search[i].tbhits          = 0;
            search[i].best_depth      = 0;
            search[i].last_pv.length  = 0;

            // Init de l'historique par profondeur
            for (int d = 0; d <= MAX_PLY; d++)
            {
                search[i].pv_scores[d] = -INFINITE;
                search[i].pv_moves [d] = Move::MOVE_NONE;
            }

            search[i].table           = &transpositionTable;
        }

        // Il faut mettre le lancement des threads dans une boucle séparée
        // car il faut être sur que la Search soit bien créée
        // board et timer sont passés par valeur.
        for (size_t i = 0; i < nbrThreads; i++)
        {
            if (board.side_to_move == WHITE)
                search[i].thread = std::thread(&Search::think<WHITE>, &search[i], board, timer, i);
            else
                search[i].thread = std::thread(&Search::think<BLACK>, &search[i], board, timer, i);
        }
    }
}

//=================================================
//! \brief  Arrêt de la recherche.
//! L'arrêt a été commandé par la thread 0
//! lorsqu'elle a fini sa recherche.
//-------------------------------------------------
void ThreadPool::main_thread_stopped()
{
    searchStopped.store(true, std::memory_order_relaxed);
}

//=================================================
//! \brief  Blocage du programme en attendant
//! les threads.
//!
//! \param[in]  start   indice de la première thread à attendre
//-------------------------------------------------
void ThreadPool::wait(size_t start)
{
    for (size_t i = start; i < nbrThreads; i++)
    {
        if (search[i].thread.joinable())
            search[i].thread.join();
    }
}

//=================================================
//! \brief  On impose l'arrêt de la recherche.
//! Commande uci-stop.
//-------------------------------------------------
void ThreadPool::stop()
{
    searchStopped.store(true, std::memory_order_relaxed);
    wait(0);
}

//=================================================
//! \brief  Sortie du programme
//-------------------------------------------------
void ThreadPool::quit()
{
    stop();
}

//=================================================
//! \brief  Retourne l'index de la meilleure thread
//! Sélection par vote (schéma Stockfish / Berserk)
//!
//! \return Indice de la thread retenue
//-------------------------------------------------
int ThreadPool::get_best_thread() const
{
    // À 1 thread, rien à départager : la thread principale (0) est retenue.
    if (nbrThreads == 1)
        return 0;

    // Accesseurs sur les données finales d'une thread
    auto final_score = [&](size_t i) { return search[i].pv_scores[search[i].best_depth]; };
    auto final_move  = [&](size_t i) { return search[i].pv_moves [search[i].best_depth]; };
    auto final_depth = [&](size_t i) { return search[i].best_depth; };

    // Score minimal parmi les threads
    int minScore = INFINITE;
    for (size_t i = 0; i < nbrThreads; i++)
        minScore = std::min(minScore, final_score(i));

    // Poids du vote d'une thread : d'autant plus fort qu'elle a un bon score
    // (relatif au plancher) et qu'elle a cherché profond. Le +14 garantit qu'une
    // thread au score minimal conserve un poids non nul proportionnel à sa profondeur.
    auto thread_value = [&](size_t i) -> I64
    {
        return static_cast<I64>(final_score(i) - minScore + 14) * final_depth(i);
    };

    // Poids retenu pour le départage à égalité de voix : une PV d'au moins 3 coups
    // est jugée fiable ; une PV trop courte (≤ 2) voit son poids annulé.
    auto tiebreak_value = [&](size_t i) -> I64
    {
        return search[i].last_pv.length > 2 ? thread_value(i) : 0;
    };

    // Chaque thread vote pour SON meilleur coup, avec son poids. Les threads ayant
    // convergé vers le même coup cumulent leurs voix.
    std::map<MOVE, I64> votes;
    for (size_t i = 0; i < nbrThreads; i++)
        votes[final_move(i)] += thread_value(i);

    // On parcourt les threads pour élire la meilleure.
    int best = 0;
    for (size_t i = 1; i < nbrThreads; i++)
    {
        if (std::abs(final_score(best)) >= TBWIN_IN_X)
        {
            // La meilleure actuelle annonce un résultat prouvé (mat/TB) : on ne
            // bascule que vers un score strictement plus élevé (mat gagnant le
            // plus court, ou défaite repoussée le plus loin possible).
            if (final_score(i) > final_score(best))
                best = i;
        }
        else if (final_score(i) >= TBWIN_IN_X)
        {
            // i a trouvé un gain prouvé alors que best n'en avait pas : i l'emporte.
            best = i;
        }
        else if (final_score(i) > -TBWIN_IN_X)   // et i n'est pas en train de se faire mater
        {
            // Aucune des deux n'a de résultat prouvé : on tranche aux voix,
            // puis, à égalité de voix, au poids (PV fiable uniquement).
            if (   votes[final_move(i)] > votes[final_move(best)]
                || (   votes[final_move(i)] == votes[final_move(best)]
                    && tiebreak_value(i) > tiebreak_value(best)))
            {
                best = i;
            }
        }
    }

    return best;
}

//=================================================
//! \brief  Retourne le nombre total des nodes recherchés
//-------------------------------------------------
U64 ThreadPool::get_all_nodes() const
{
    U64 total = 0;
    for (size_t i=0; i<nbrThreads; i++)
    {
        total += search[i].nodes;
    }
    return(total);
}

//=================================================
//! \brief  Retourne la somme des profondeurs atteintes
//-------------------------------------------------
int ThreadPool::get_all_depths() const
{
    int total = 0;
    for (size_t i=0; i<nbrThreads; i++)
    {
        total += search[i].best_depth;
    }
    return(total);
}

//=================================================
//! \brief  Retourne le nombre total de tbhits
//-------------------------------------------------
U64 ThreadPool::get_all_tbhits() const
{
    U64 total = 0;
    for (size_t i=0; i<nbrThreads; i++)
    {
        total += search[i].tbhits;
    }
    return(total);
}

