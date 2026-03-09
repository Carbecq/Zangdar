#include <cstring>
#include "ThreadPool.h"
#include "Board.h"
#include "Search.h"
#include "Move.h"


//=================================================
//! \brief  Constructeur avec arguments
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
//-------------------------------------------------
void ThreadPool::set_threads(U32 nbr)
{
#if defined DEBUG_LOG
    char message[200];
    sprintf(message, "ThreadPool::set_threads : nbrThreads=%d ", nbr);
    printlog(message);
#endif

    U32 processorCount = static_cast<U32>(std::thread::hardware_concurrency());
    // Check if the number of processors can be determined
    if (processorCount == 0)
        processorCount = MAX_THREADS;

    // Clamp the number of threads to the number of processors
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
    // Libère toute la mémoire
    for (size_t i = 0; i < nbrThreads; i++)
    {
        search[i].history.reset();
    }
}

//=================================================
//! \brief  Lance la recherche
//! Fonction lancée par Uci::parse_go
//-------------------------------------------------
void ThreadPool::start_thinking(const Board& board, const Timer& timer)
{
#if defined DEBUG_LOG
    char message[100];
    sprintf(message, "ThreadPool::start_thinking");
    printlog(message);
#endif

    MOVE best = Move::MOVE_NONE;

    //  an opening is selected by the gui, from the book, and the engines play from there
    //  that's how engine testing works, be it by devs, rating lists, or tournaments

    // Probe Syzygy TableBases
    // NUL/PERTE : probe_root retourne true et on joue le coup DTZ-optimal immédiatement.
    // GAIN : probe_root retourne false et root_moves est rempli ; la recherche tourne
    //        limitée à ces coups pour que l'alpha-bêta trouve le gain le plus rapide (DTM-optimal).
    MoveList root_moves;

    if (useSyzygy && board.probe_root(best, root_moves) == true)
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

        for (size_t i = 0; i < nbrThreads; i++)
        {
            search[i].seldepth      = 0;
            search[i].stopped       = false;
            search[i].iter_depth    = timer.getSearchDepth();
            search[i].iter_score    = -INFINITE;
            search[i].nodes         = 0;
            search[i].tbhits        = 0;

            search[i].iter_best_depth = 0;
            search[i].iter_best_move  = Move::MOVE_NONE;
            search[i].iter_best_score = -INFINITE;

            search[i].table           = &transpositionTable;
            search[i].root_moves      = root_moves;
            search[i].tb_root         = root_moves.size() > 0;
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
    // envoie à toutes les autres threads
    // le signal d'arrêter
    for (size_t i = 1; i < nbrThreads; i++)
        search[i].stopped = true;
}

//=================================================
//! \brief  Blocage du programme en attendant
//! les threads.
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
    // Signal d'arrêt à toutes les threads
    for (size_t i = 0; i < nbrThreads; i++)
        search[i].stopped = true;

    // Attente de toutes les threads
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
//! Sélection par profondeur la plus grande,
//! puis par score en cas d'égalité.
//-------------------------------------------------
int ThreadPool::get_best_thread() const
{
    int best = 0;

    for (size_t i = 1; i < nbrThreads; i++)
    {
        if (   search[i].iter_best_depth > search[best].iter_best_depth
            || (search[i].iter_best_depth == search[best].iter_best_depth
                && search[i].iter_best_score > search[best].iter_best_score))
        {
            best = i;
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
        total += search[i].iter_best_depth;
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

