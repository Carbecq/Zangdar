#ifndef THREADPOOL_H
#define THREADPOOL_H

class ThreadPool;

#include <memory>
#include "defines.h"
#include "Board.h"
#include "Timer.h"
#include "Search.h"

class ThreadPool
{
public:
    explicit ThreadPool(U32 _nbr, bool _tb, bool _log);
    void set_threads(U32 nbr);
    void reset();

    void start_thinking(const Board &board, const Timer &timer);
    void main_thread_stopped();
    void stop();
    void wait(size_t start);
    void quit();

    U64  get_all_nodes() const;
    int  get_all_depths() const;
    int  get_best_thread() const;
    MOVE get_best_move() const { return search[get_best_thread()].iter_best_move; }
    U64  get_all_tbhits() const;

    void set_logUci(bool f)          { logUci = f;       }
    void set_useSyzygy(bool f)       { useSyzygy = f;    }
    void set_syzygyProbeLimit(int n) { syzygyProbeLimit = n; }

    bool get_logUci()           const { return logUci; }
    U32  get_nbrThreads()       const { return nbrThreads; }
    bool get_useSyzygy()        const { return useSyzygy; }
    int  get_syzygyProbeLimit() const { return syzygyProbeLimit; }

    std::unique_ptr<Search[]> search;
    std::atomic<bool> searchStopped{false};

private:
    U32     nbrThreads;
    bool    useSyzygy;
    int     syzygyProbeLimit;  // max pieces for WDL/DTZ probing (0 = no limit)
    bool    logUci;

};

extern ThreadPool threadPool;

#endif // THREADPOOL_H
