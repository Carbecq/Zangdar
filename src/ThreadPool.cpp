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
        nbrThreads = newNbr;
        search = std::make_unique<Search[]>(nbrThreads);
        for (size_t i = 0; i < nbrThreads; i++)
        {
            search[i].table    = nullptr;
            search[i].td_index = i;
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
    if (useSyzygy && board.probe_root(best) == true)
    {
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
            search[i].td_depth    = timer.getSearchDepth();
            search[i].td_seldepth = 0;
            search[i].td_score    = -INFINITE;
            search[i].td_nodes    = 0;
            search[i].td_stopped  = false;
            search[i].td_tbhits   = 0;

            search[i].td_best_depth = 0;
            search[i].td_best_move  = Move::MOVE_NONE;
            search[i].td_best_score = -INFINITE;

            search[i].table         = &transpositionTable;
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
        search[i].td_stopped = true;
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
    // Message d'arrêt à la thread principale
    search[0].td_stopped = true;

    // On bloque ici en attendant la thread principale
    if (search[0].thread.joinable())
        search[0].thread.join();
}

//=================================================
//! \brief  Sortie du programme
//-------------------------------------------------
void ThreadPool::quit()
{
    stop();
}

//=================================================
//! \brief  Retourne le nombre total des nodes recherchés
//-------------------------------------------------
U64 ThreadPool::get_all_nodes() const
{
    U64 total = 0;
    for (size_t i=0; i<nbrThreads; i++)
    {
        total += search[i].td_nodes;
    }
    return(total);
}

//=================================================
//! \brief  Retourne le nombre total des nodes recherchés
//-------------------------------------------------
int ThreadPool::get_all_depths() const
{
    int total = 0;
    for (size_t i=0; i<nbrThreads; i++)
    {
        total += search[i].td_best_depth;
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
        total += search[i].td_tbhits;
    }
    return(total);
}

