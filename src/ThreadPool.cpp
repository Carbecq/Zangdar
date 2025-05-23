#include <cstring>
#include "ThreadPool.h"
#include "Board.h"
#include "Search.h"
#include "Move.h"


//=================================================
//! \brief  Constructeur avec arguments
//-------------------------------------------------
ThreadPool::ThreadPool(int _nbr, bool _tb, bool _log) :
    nbrThreads(_nbr),
    useSyzygy(_tb),
    logUci(_log)
{
#if defined DEBUG_LOG
    char message[200];
    sprintf(message, "ThreadPool::constructeur : nbrThreads=%d ; useSyzygy=%d ; logUci=%d ", _nbr, _tb, _log);
    printlog(message);
#endif

    for (int i = 0; i < MAX_THREADS; i++)
    {
        threadData[i].search = nullptr;
        threadData[i].index  = i;
    }

    set_threads(_nbr);
}

//=================================================
//! \brief  Initialisation du nombre de threads
//-------------------------------------------------
void ThreadPool::set_threads(int nbr)
{
#if defined DEBUG_LOG
    char message[200];
    sprintf(message, "ThreadPool::set_threads : nbrThreads=%d ", nbr);
    printlog(message);
#endif

    int processorCount = static_cast<int>(std::thread::hardware_concurrency());
    // Check if the number of processors can be determined
    if (processorCount == 0)
        processorCount = MAX_THREADS;

    // Clamp the number of threads to the number of processors
    nbrThreads     = std::min(nbr, processorCount);
    nbrThreads     = std::max(nbrThreads, 1);
    nbrThreads     = std::min(nbrThreads, MAX_THREADS);

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
    for (int i = 0; i < nbrThreads; i++)
    {
        threadData[i].history.reset();
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

        for (int i = 0; i < nbrThreads; i++)
        {
            threadData[i].depth    = 0;
            threadData[i].seldepth = 0;
            threadData[i].score    = -INFINITE;
            threadData[i].nodes    = 0;
            threadData[i].stopped  = false;
            threadData[i].tbhits   = 0;

            threadData[i].best_depth = 0;
            threadData[i].best_move  = Move::MOVE_NONE;
            threadData[i].best_score = -INFINITE;

            delete threadData[i].search;
            threadData[i].search = new Search();
        }

        // Il faut mettre le lancement des threads dans une boucle séparée
        // car il faut être sur que la Search soit bien créée
        // board et timer sont passés par valeur.
        for (int i = 0; i < nbrThreads; i++)
        {
            if (board.side_to_move == WHITE)
                threadData[i].thread = std::thread(&Search::think<WHITE>, threadData[i].search, board, timer, i);
            else
                threadData[i].thread = std::thread(&Search::think<BLACK>, threadData[i].search, board, timer, i);
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
    for (int i = 1; i < nbrThreads; i++)
        threadData[i].stopped = true;
}

//=================================================
//! \brief  Blocage du programme en attendant
//! les threads.
//-------------------------------------------------
void ThreadPool::wait(int start)
{
    for (int i = start; i < nbrThreads; i++)
    {
        if (threadData[i].thread.joinable())
            threadData[i].thread.join();
    }
}

//=================================================
//! \brief  On impose l'arrêt de la recherche.
//! Commande uci-stop.
//-------------------------------------------------
void ThreadPool::stop()
{
    // Message d'arrêt à la thread principale
    threadData[0].stopped = true;

    // On bloque ici en attendant la thread principale
    if (threadData[0].thread.joinable())
        threadData[0].thread.join();
}

//=================================================
//! \brief  Sortie du programme
//-------------------------------------------------
void ThreadPool::quit()
{
    stop();

    for (int i = 0; i < nbrThreads; i++)
    {
        delete threadData[i].search;
        threadData[i].search = nullptr;
    }
}

//=================================================
//! \brief  Retourne le nombre total des nodes recherchés
//-------------------------------------------------
U64 ThreadPool::get_all_nodes() const
{
    U64 total = 0;
    for (int i=0; i<nbrThreads; i++)
    {
        total += threadData[i].nodes;
    }
    return(total);
}

//=================================================
//! \brief  Retourne le nombre total des nodes recherchés
//-------------------------------------------------
int ThreadPool::get_all_depths() const
{
    int total = 0;
    for (int i=0; i<nbrThreads; i++)
    {
        total += threadData[i].best_depth;
    }
    return(total);
}

//=================================================
//! \brief  Retourne le nombre total de tbhits
//-------------------------------------------------
U64 ThreadPool::get_all_tbhits() const
{
    U64 total = 0;
    for (int i=0; i<nbrThreads; i++)
    {
        total += threadData[i].tbhits;
    }
    return(total);
}

