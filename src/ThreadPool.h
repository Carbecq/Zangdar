#ifndef THREADPOOL_H
#define THREADPOOL_H

class ThreadPool;

#include <memory>
#include <vector>
#include "HugePages.h"
#include "defines.h"
#include "Board.h"
#include "Timer.h"
#include "Search.h"

class ThreadPool
{
public:
    explicit ThreadPool(U32 _nbr, bool _tb, bool _log);

    // Le pool possède sa mémoire (voir search ci-dessous) : pas de copie.
    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void set_threads(U32 nbr);
    void reset();
    void reinit_reductions();

    void start_thinking(const Board &board, const Timer &timer);
    void main_thread_stopped();
    void stop();
    void wait(size_t start);
    void quit();

    U64  get_all_nodes() const;
    int  get_all_depths() const;
    int  get_best_thread() const;
    //! \brief  Retourne le meilleur coup, joué par la thread retenue par get_best_thread()
    MOVE get_best_move() const {
        int bt = get_best_thread();
        return search[bt].pv_moves[search[bt].best_depth];
    }
    U64  get_all_tbhits() const;

    //! \brief  Active/désactive l'affichage des informations UCI pendant la recherche
    void set_logUci(bool f)          { logUci = f;       }
    //! \brief  Active/désactive l'utilisation des tables Syzygy
    void set_useSyzygy(bool f)       { useSyzygy = f;    }
    //! \brief  Fixe le nombre maximum de pièces pour le probe Syzygy WDL/DTZ
    void set_syzygyProbeLimit(int n) { syzygyProbeLimit = n; }

    //! \brief  Vide la restriction "searchmoves" (à faire à chaque "go")
    void clear_searchMoves()         { searchMoves.clear(); }
    //! \brief  Ajoute un coup à la restriction "searchmoves"
    void add_searchMove(MOVE move)   { searchMoves.push_back(move); }
    //! \brief  Indique si une restriction "searchmoves" est active
    bool has_searchMoves()     const { return !searchMoves.empty(); }

    //! \brief  Indique si le coup peut être recherché à la racine.
    //!         Sans restriction active, tous les coups sont autorisés.
    bool is_searchMove(MOVE move) const
    {
        if (searchMoves.empty())
            return true;
        for (const MOVE m : searchMoves)
            if (m == move)
                return true;
        return false;
    }

    //! \brief  Indique si l'affichage des informations UCI est actif
    bool get_logUci()           const { return logUci; }
    //! \brief  Retourne le nombre de threads de recherche
    U32  get_nbrThreads()       const { return nbrThreads; }
    //! \brief  Indique si les tables Syzygy sont utilisées
    bool get_useSyzygy()        const { return useSyzygy; }
    //! \brief  Retourne le nombre maximum de pièces pour le probe Syzygy WDL/DTZ
    int  get_syzygyProbeLimit() const { return syzygyProbeLimit; }

    // HugeArray et non make_unique<Search[]> : chaque Search porte son History,
    // soit ~34 Mo en accès dispersé. Voir HugePages.h.
    HugeArray<Search> search;
    std::atomic<bool> searchStopped{false};

private:
    U32     nbrThreads;
    bool    useSyzygy;
    int     syzygyProbeLimit;  // max pieces for WDL/DTZ probing (0 = no limit)
    bool    logUci;
    std::vector<MOVE> searchMoves;  // restriction "go searchmoves" (vide = aucune)

};

extern ThreadPool threadPool;

#endif // THREADPOOL_H
